/* See COPYING.txt for license details. */

/*
*
* m1_watchdog.c
*
* Watchdog functions
*
* M1 Project
*
* Reference: https://mcuoneclipse.com/2023/03/26/using-a-watchdog-timer-with-an-rtos/
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_fw_update_bl.h"
#include "m1_watchdog.h"
#include "m1_watchdog_boot_loop.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG	"Watchdog"

/* M1_WDT_TASK_PERIOD_MS is the FreeRTOS IWDG handler task period in
 * milliseconds, defined separately from IWDG_RELOAD to avoid a unit
 * confusion: IWDG_RELOAD is a hardware counter value (ticks, not ms).
 * With prescaler 128 and LSI ~32 kHz, 1 IWDG tick ≈ 4 ms, so the full
 * IWDG timeout is IWDG_RELOAD (4000) × 4 ms = 16,000 ms (16 s).
 * The WDT task fires every 2,000 ms — 1/8 of the IWDG timeout —
 * giving 8× steady-state margin. */
#define M1_WDT_TASK_PERIOD_MS			2000U  /* WDT handler task reload period (ms) */
#define M1_WDT_SYSTEM_CHECK_TIMEOUT		(4U * M1_WDT_TASK_PERIOD_MS) /* 8 s system-check interval */

/* Boot-loop escape hatch — RTC backup register used to count consecutive
 * IWDG resets.  DR2 is the first slot after S_M1_BK_REGS_t (which uses
 * DR0..DR1: magic_number[31:0] at DR0, device_op_status[7:0]+reserved
 * at DR1).  The counter survives chip reset (backup domain) but is
 * cleared by a full power-off. */
#define M1_WDT_BKP_LOOP_CTR_REG	RTC_BKP_DR2
#define M1_WDT_BKP_LOOP_SIG_REG	RTC_BKP_DR3
#define M1_WDT_BOOT_LOOP_MAX		3U   /* suppress IWDG after this many consecutive WDT resets */

//************************** C O N S T A N T **********************************/

//************************** S T R U C T U R E S *******************************

/***************************** V A R I A B L E S ******************************/

static S_M1_WDT_Report wdt_report[M1_REPORT_ID_END_OF_LIST];

static TaskHandle_t m1_wdt_task_hdl;

static uint16_t m1_wdt_check_count = 0;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void m1_wdt_init(void);
void m1_wdt_report_init(void);
void m1_wdt_send_report(S_M1_WDT_Report_ID rpt_id, uint32_t time);
void m1_wdt_send_report_ex(S_M1_WDT_Report_ID rpt_id, TickType_t start_time);
void m1_wdt_send_delayed_report(S_M1_WDT_Report_ID rpt_id, uint32_t delay_ms, uint8_t repeat);
void m1_wdt_reset(void);
static void m1_wdt_add_task_to_report(S_M1_WDT_Report_ID rpt_id, uint32_t rpt_period, uint32_t min_rpt_percent, uint32_t max_rpt_percent);
static void m1_wdt_handler_task(void *param);
static void m1_wdt_checkin(void);
static void m1_wdt_checkin_check(void);
static void m1_wdt_checkout(void);
static void m1_wdt_checkout_check(void);
static void m1_wdt_system_check(void);
static void m1_wdt_failure_handler(void);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/******************************************************************************/
/*
*	This function initializes the device watchdog system
*
*/
/******************************************************************************/
void m1_wdt_init(void)
{
	BaseType_t ret;
	size_t free_heap;
	uint32_t loop_ctr;
	uint32_t loop_sig;
	uint32_t expected_sig;
	bool was_wdt_reset;
	m1_wdt_boot_loop_eval_t loop_eval;

	/*
	 * Freeze the IWDG counter when the CPU is halted by the debugger.
	 * Without this, the IWDG fires a reset during breakpoints/stepping,
	 * making debugging impossible with ST-Link/J-Link.
	 * This only has effect when the debug port is connected; in production
	 * (no debugger attached), this bit is ignored by the hardware.
	 * m1_system_GPIO_init() already sets this bit; repeating here is
	 * harmless (idempotent) and ensures it is set before we arm the IWDG.
	 */
#if defined(__HAL_DBGMCU_FREEZE_IWDG)
	__HAL_DBGMCU_FREEZE_IWDG();
#elif defined(DBGMCU_APB1FZR1_DBG_IWDG_STOP)
	SET_BIT(DBGMCU->APB1FZR1, DBGMCU_APB1FZR1_DBG_IWDG_STOP);
#endif

	was_wdt_reset = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0);
	if(was_wdt_reset)
	{
		m1_device_stat.dev_reset_by_wdt = true;
		M1_LOG_W(M1_LOGDB_TAG, "Device reset by Watchdog!\r\n");
	}
	else
	{
		m1_device_stat.dev_reset_by_wdt = false;
		M1_LOG_I(M1_LOGDB_TAG, "Device reset by normal cause\r\n");
	}
	__HAL_RCC_CLEAR_RESET_FLAGS(); // clears reset flags

	/*
	 * Boot-loop escape hatch.
	 *
	 * TAMP backup register DR2 holds a counter of consecutive IWDG resets.
	 * It survives chip reset (backup domain is reset-persistent) but is
	 * cleared by a full power-off (Vbat removed).  startup_device_init()
	 * already called HAL_PWR_EnableBkUpAccess(), so direct read/write is safe.
	 *
	 * On each IWDG reset, the counter is incremented.  On any non-IWDG reset
	 * (user power-cycle, clean reset), the counter is cleared.  When the
	 * counter reaches M1_WDT_BOOT_LOOP_MAX, we skip arming the IWDG and skip
	 * creating the WDT handler task for this one boot, then clear the counter.
	 * This lets the device always reach the main menu so the user can update
	 * firmware, reformat the SD card, or remove a problematic file — without
	 * needing a debugger.
	 *
	 * DR2 is the first backup register after S_M1_BK_REGS_t, which uses
	 * DR0..DR1 (sizeof = 8 bytes = 2 x 32-bit words).
	 */
	HAL_PWR_EnableBkUpAccess(); /* safe to call again; no-op if already enabled */
	loop_ctr = HAL_RTCEx_BKUPRead(&hrtc, M1_WDT_BKP_LOOP_CTR_REG);
	loop_sig = HAL_RTCEx_BKUPRead(&hrtc, M1_WDT_BKP_LOOP_SIG_REG);
	expected_sig = m1_wdt_boot_loop_build_sig_components(
		FW_VERSION_MAJOR,
		FW_VERSION_MINOR,
		FW_VERSION_BUILD,
		FW_VERSION_RC,
		M1_HAPAX_REVISION);
	/* DR3 stores a firmware-version signature for DR2 validity.
	 * Any signature mismatch (e.g. flashed new firmware while VBAT
	 * retained) invalidates DR2 so stale in-range counters cannot
	 * trigger a false boot-loop suppressor hit. */
	loop_eval = m1_wdt_boot_loop_evaluate(
		loop_ctr,
		loop_sig,
		was_wdt_reset,
		expected_sig,
		M1_WDT_BOOT_LOOP_MAX);
	loop_ctr = loop_eval.runtime_ctr;
	HAL_RTCEx_BKUPWrite(&hrtc, M1_WDT_BKP_LOOP_SIG_REG, expected_sig);
	HAL_RTCEx_BKUPWrite(&hrtc, M1_WDT_BKP_LOOP_CTR_REG, loop_eval.stored_ctr);

	if (loop_eval.disable_iwdg)
	{
		M1_LOG_W(M1_LOGDB_TAG,
		         "Boot loop detected (%lu consecutive WDT resets). "
		         "IWDG disabled for this boot.\r\n",
		         (unsigned long)loop_ctr);
		/* Display a recovery banner on the home screen. */
		startup_info_screen_display("WDT DISABLED - BOOT LOOP");
		/* Return without arming IWDG or creating the WDT task.
		 * The device boots normally to the main menu without watchdog
		 * protection for this session, giving the user a chance to
		 * update firmware, reformat the SD card, or remove a bad file. */
		return;
	}

	/*
	 * Arm the IWDG.  Deliberately deferred from MX_IWDG_Init() in main.c
	 * so the timeout countdown starts after the early init block
	 * (LCD, SD card, logdb) instead of in main().  The remaining early-boot
	 * work (startup_config_handler() and scheduler handoff to the WDT task)
	 * still runs inside this watchdog window.  The hiwdg struct was
	 * pre-configured by MX_IWDG_Init(); HAL_IWDG_Init() starts the counter.
	 *
	 * With IWDG_PRESCALER_128 and IWDG_RELOAD=4000 (LSI ~32 kHz):
	 *   timeout = 128 / 32000 * 4000 = 16 s.
	 * The WDT handler task reloads every M1_WDT_TASK_PERIOD_MS = 2000 ms,
	 * giving 8x steady-state margin inside the 16 s window.
	 */
	if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
	{
		Error_Handler();
	}

	m1_wdt_report_init();

	ret = xTaskCreate(m1_wdt_handler_task, "m1_wdt_handler_task_n", M1_TASK_STACK_SIZE_DEFAULT, NULL, TASK_PRIORITY_WDT_HANDLER, &m1_wdt_task_hdl);
	assert(ret==pdPASS);
	assert(m1_wdt_task_hdl!=NULL);
	free_heap = xPortGetFreeHeapSize();
	assert(free_heap >= M1_LOW_FREE_HEAP_WARNING_SIZE);
} // void m1_wdt_init(void)



/******************************************************************************/
/*
*	This function handles the device watchdog system
*
*/
/******************************************************************************/
static void m1_wdt_handler_task(void *param)
{
	static uint32_t run_time = 0;

	/* Reload the IWDG counter immediately on first scheduling of this task,
	 * BEFORE the initial 2s vTaskDelay below.  The IWDG is now armed inside
	 * m1_wdt_init() (deferred from MX_IWDG_Init in main.c), after pre-RTOS
	 * early init but before startup_config_handler().  This initial reload is
	 * belt-and-suspenders: the only time remaining after m1_wdt_init() returns
	 * is m1_tasks_init() + startup_config_handler() + a final m1_wdt_reset()
	 * kick, all of which complete well within the 16 s IWDG window.  Still,
	 * this reload must come before any logging: m1_logdb_printf() can
	 * allocate and block on the log mutex, adding unpredictable latency. */
	__HAL_IWDG_RELOAD_COUNTER(&hiwdg);

	M1_LOG_I(M1_LOGDB_TAG, "WDT task started\r\n");

	while (1)
	{
		m1_wdt_checkin();
		vTaskDelay(pdMS_TO_TICKS(M1_WDT_TASK_PERIOD_MS));
		run_time += M1_WDT_TASK_PERIOD_MS;
		m1_wdt_checkout();
		if ( run_time >= M1_WDT_SYSTEM_CHECK_TIMEOUT )
		{
			m1_wdt_system_check();
			run_time = 0;
		} // if ( run_time >= M1_WDT_SYSTEM_CHECK_TIMEOUT )
		xTaskNotify(idle_task_hdl, 0, eSetValueWithoutOverwrite);
	} // while (1)
} // static void m1_wdt_handler_task(void *param)



/******************************************************************************/
/*
*	This function initializes the report table
*
*/
/******************************************************************************/
void m1_wdt_report_init(void)
{
	uint8_t i;

	for(i=0; i<M1_REPORT_ID_END_OF_LIST; i++)
	{
		wdt_report[i].run_time = 0;
	}
	m1_wdt_add_task_to_report(M1_REPORT_ID_BUTTONS_HANDLER_TASK, M1_WDT_SYSTEM_CHECK_TIMEOUT, 70, 120);
	// Add other tasks here if needed
} // void m1_wdt_report_init(void)



/******************************************************************************/
/*
*	This function adds an entry to the report table
*
*/
/******************************************************************************/
void m1_wdt_add_task_to_report(S_M1_WDT_Report_ID rpt_id, uint32_t rpt_period, uint32_t min_rpt_percent, uint32_t max_rpt_percent)
{
	if ( rpt_id < M1_REPORT_ID_END_OF_LIST )
	{
		wdt_report[rpt_id].report_id = rpt_id;
		wdt_report[rpt_id].inactive = false;
		wdt_report[rpt_id].report_period = rpt_period;
		wdt_report[rpt_id].min_rpt_percent = min_rpt_percent;
		wdt_report[rpt_id].max_rpt_percent = max_rpt_percent;
	} // if ( rpt_id < M1_REPORT_ID_END_OF_LIST )
	else
	{
		M1_LOG_E(M1_LOGDB_TAG, "Wrong task ID %d\r\n", rpt_id);
	}
} // void m1_wdt_add_task_to_report(S_M1_WDT_Report_ID rpt_id, uint32_t rpt_period, uint32_t min_rpt_percent, uint32_t max_rpt_percent)



/******************************************************************************/
/*
*	This function checks the wdt state at check-in
*
*/
/******************************************************************************/
static void m1_wdt_checkin_check(void)
{
	if ( m1_wdt_check_count!=0x5555 )
	{
		M1_LOG_E(M1_LOGDB_TAG, "WDT went wrong!\r\n");
		m1_wdt_failure_handler();
	}
	m1_wdt_check_count += 0x1111;
} // static void m1_wdt_checkin_check(void)



/******************************************************************************/
/*
*	This function checks the wdt state at check-out and update WDT
*
*/
/******************************************************************************/
static void m1_wdt_checkout_check(void)
{
	if ( m1_wdt_check_count != 0x8888 )
	{
		M1_LOG_E(M1_LOGDB_TAG, "WDT went wrong!\r\n");
		m1_wdt_failure_handler();
    }

	// Reset WDT counter
	__HAL_IWDG_RELOAD_COUNTER(&hiwdg);

	if ( m1_wdt_check_count!=0x8888 )
	{
		M1_LOG_E(M1_LOGDB_TAG, "WDT went wrong!\r\n");
		m1_wdt_failure_handler();
    }

	m1_wdt_check_count = 0; // Reset
} // static void m1_wdt_checkout_check(void)



/******************************************************************************/
/*
*	This function initializes the wdt state at check-in
*
*/
/******************************************************************************/
static void m1_wdt_checkin(void)
{
	m1_wdt_check_count = 0x5555;
	m1_wdt_checkin_check();
} // static void m1_wdt_checkin(void)



/******************************************************************************/
/*
*	This function checks the wdt state at check-out
*
*/
/******************************************************************************/
static void m1_wdt_checkout(void)
{
	m1_wdt_check_count += 0x2222;
	m1_wdt_checkout_check();
} // static void m1_wdt_checkout(void)



/******************************************************************************/
/*
*	This function checks the status of all tasks registered to the report table
*
*/
/******************************************************************************/
static void m1_wdt_system_check(void)
{
	uint32_t min, max;
	uint8_t i;

	for(i=0; i<M1_REPORT_ID_END_OF_LIST; i++)
	{
		min = (wdt_report[i].report_period/100)*wdt_report[i].min_rpt_percent;
		max = (wdt_report[i].report_period/100)*wdt_report[i].max_rpt_percent;
		taskENTER_CRITICAL();
		if ( (wdt_report[i].run_time >= min) && (wdt_report[i].run_time <= max) )
		{
			//M1_LOG_I(M1_LOGDB_TAG, "SysOK\r\n");
			wdt_report[i].run_time = 0;
		}
		else if (wdt_report[i].inactive)
		{
			M1_LOG_W(M1_LOGDB_TAG, "Task ID %d suspended\r\n", wdt_report[i].report_id);
			wdt_report[i].run_time = 0;
		}
		else
		{
			M1_LOG_W(M1_LOGDB_TAG, "WDT failure. Task ID %d, run time: %ld\r\n", wdt_report[i].report_id, wdt_report[i].run_time);
		    m1_wdt_failure_handler();
		} // else
		taskEXIT_CRITICAL();
	} // for(i=0; i<M1_REPORT_ID_END_OF_LIST; i++)
} // static void m1_wdt_system_check(void)




/******************************************************************************/
/*
*	This function returns the start time of a task
*
*/
/******************************************************************************/
TickType_t m1_wdt_get_start_time(void)
{
	return xTaskGetTickCount();
} // TickType_t m1_wdt_get_start_time(void)



/******************************************************************************/
/*
*	This function updates the report table for a task with start time
*
*/
/******************************************************************************/
void m1_wdt_send_report_ex(S_M1_WDT_Report_ID rpt_id, TickType_t start_time)
{
	m1_wdt_send_report(rpt_id, ((xTaskGetTickCount() - start_time)*configTICK_RATE_HZ)/1000);
} // void m1_wdt_send_report_ex(S_M1_WDT_Report_ID rpt_id, TickType_t start_time)



/******************************************************************************/
/*
*	This function updates the report table for a task
*
*/
/******************************************************************************/
void m1_wdt_send_report(S_M1_WDT_Report_ID rpt_id, uint32_t time)
{
	if ( rpt_id < M1_REPORT_ID_END_OF_LIST )
	{
		//taskENTER_CRITICAL();
		if ( rpt_id < M1_REPORT_ID_END_OF_LIST)
		{
			wdt_report[rpt_id].run_time += time;
		} // if ( rpt_id < M1_REPORT_ID_END_OF_LIST)
		else
		{
			M1_LOG_E(M1_LOGDB_TAG, "WDT went wrong!\r\n");
			vTaskDelay(1);
			m1_wdt_failure_handler();
	    } // else
		//taskEXIT_CRITICAL();
	} // if ( rpt_id < M1_REPORT_ID_END_OF_LIST )
} // void m1_wdt_send_report(S_M1_WDT_Report_ID rpt_id, uint32_t time)




/******************************************************************************/
/*
*	This function updates the report table for a task with long run time
*
*/
/******************************************************************************/
void m1_wdt_send_delayed_report(S_M1_WDT_Report_ID rpt_id, uint32_t delay_ms, uint8_t repeat)
{
	uint8_t i;

	for (i=0; i<repeat; i++)
	{
		vTaskDelay(pdMS_TO_TICKS(delay_ms));
		m1_wdt_send_report(rpt_id, delay_ms);
	}
} // void m1_wdt_send_delayed_report(S_M1_WDT_Report_ID rpt_id, uint32_t delay_ms, uint8_t repeat)




/******************************************************************************/
/*
*	This function handles the WDT failure case
*
*/
/******************************************************************************/
static void m1_wdt_failure_handler(void)
{
	/* Disable interrupts to prevent further damage, then spin.
	 * The IWDG will eventually fire and reset the system cleanly.
	 * In debug mode (IWDG frozen), this acts as a breakpoint-friendly
	 * infinite loop for post-mortem inspection. */
	__disable_irq();
	while (1)
	{
		__asm("nop");
	} // while (1)
} // static void m1_wdt_failure_handler(void)



/******************************************************************************/
/*
*	This function resets the WDT
*
*/
/******************************************************************************/
void m1_wdt_reset(void)
{
	// Reset WDT counter
	__HAL_IWDG_RELOAD_COUNTER(&hiwdg);
} // void m1_wdt_reset(void)


void m1_wdt_suspend_task(S_M1_WDT_Report_ID rpt_id)
{
	if (rpt_id < M1_REPORT_ID_END_OF_LIST)
	{
		taskENTER_CRITICAL();
		wdt_report[rpt_id].inactive = true;
		wdt_report[rpt_id].run_time = 0;
		taskEXIT_CRITICAL();
	}
}

void m1_wdt_resume_task(S_M1_WDT_Report_ID rpt_id)
{
	if (rpt_id < M1_REPORT_ID_END_OF_LIST)
	{
		taskENTER_CRITICAL();
		wdt_report[rpt_id].inactive = false;
		wdt_report[rpt_id].run_time = 0;
		taskEXIT_CRITICAL();
	}
}
