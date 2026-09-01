/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "stream_buffer.h"

//#include "esp_system.h"
#include "esp_err.h"
#include "m1_log_debug.h"
#include "spi_master.h"
#include "m1_tasks.h"
#include "m1_compile_cfg.h"
#include "m1_esp32_hal.h"
#include "m1_io_defs.h"
#include "esp_at_list.h"
#include "esp_queue.h"
#include "m1_at_response_parser.h"
#include "esp32_spi_bin.h"
#include "m1_esp32_rpc.h"      /* M1 Link full-duplex framing helper + MTU */

#define STREAM_BUFFER_SIZE    	SPI_TRANS_MAX_LEN

#define TAG						"SPI_AT_Master"
#define ESP_SPI_ID				0x00

#define CR_LF					"\r\n"

#define MILLISEC_TO_SEC			1000
#define TICKS_PER_SEC (1000 / portTICK_PERIOD_MS);
#define SEC_TO_MILLISEC(x) (1000*(x))

/* If request is already being served and
 * another request is pending, time period for
 * which new request will wait in seconds
 * */
#define WAIT_TIME_B2B_CTRL_REQ               5
#define DEFAULT_CTRL_RESP_TIMEOUT            30
#define DEFAULT_CTRL_RESP_AP_SCAN_TIMEOUT    (60*3)
#define DEFAULT_CTRL_RESP_CONNECT_AP_TIMEOUT (15*3)

QueueHandle_t esp_spi_msg_queue; // message queue used for communicating read/write start
QueueHandle_t esp_resp_read_sem = NULL;
QueueHandle_t esp_ctrl_req_sem = NULL;
esp_queue_t* ctrl_msg_Q = NULL;

static spi_device_handle_t spi_dev_handle = NULL;
static StreamBufferHandle_t spi_master_tx_ring_buf = NULL;
static SemaphoreHandle_t pxMutex;
static uint8_t initiative_send_flag = 0; // it means master has data to send to slave
static uint32_t plan_send_len = 0; // master plan to send data len

static uint8_t current_send_seq = 0;
static uint8_t current_recv_seq = 0;

static bool esp32_main_init_done = false;

/* Graceful task-exit support for esp32_main_deinit() */
static volatile bool s_esp32_stop_task = false;
static TaskHandle_t s_spi_task_handle = NULL;
/* Binary semaphore used as a task-join barrier.  Created in esp32_main_init()
 * before the task is created, given by the task on exit, taken by deinit with
 * a timeout.  Using a dedicated semaphore instead of xTaskNotify avoids the
 * race where an unrelated pending notification causes xTaskNotifyWait to return
 * immediately, making deinit free shared objects while the task is still alive. */
static SemaphoreHandle_t s_deinit_sync_sem = NULL;

/* Forward declaration — defined below ble_advertise(). */
static uint8_t esp_at_send_wait_ok(ctrl_cmd_t *app_req, const char *at_cmd_str);

/* uid to link between requests and responses
 * uids are incrementing values from 1 onwards. */
static int32_t uid = 0;
/* uid of request / response */
static int32_t current_uid = 0;

static void spi_mutex_lock(void)
{
    while (xSemaphoreTake(pxMutex, portMAX_DELAY) != pdPASS);
}

static void spi_mutex_unlock(void)
{
    xSemaphoreGive(pxMutex);
}

static void at_spi_master_send_data(uint8_t* data, uint32_t len)
{
	HAL_StatusTypeDef ret;

	spi_transaction_t trans = {
        .cmd = CMD_HD_WRDMA_REG,    // master -> slave command, donnot change
        .length = len * 8,
        .tx_buffer = (void*)data
    };
	ret = spi_device_polling_transmit(spi_dev_handle, &trans);
	current_uid = uid; // Update current request command id after it has been transmitted
}

static void at_spi_master_recv_data(uint8_t* data, uint32_t len)
{
	spi_transaction_t trans = {
        .cmd = CMD_HD_RDDMA_REG,    // master -> slave command, donnot change
        .rxlength = len * 8,
        .rx_buffer = (void*)data
    };
	spi_device_polling_transmit(spi_dev_handle, &trans);
}

// send a single to slave to tell slave that master has read DMA done
static void at_spi_rddma_done(void)
{
    spi_transaction_t end_t = {
        .cmd = CMD_HD_INT0_REG,
    };
    spi_device_polling_transmit(spi_dev_handle, &end_t);
}

// send a single to slave to tell slave that master has write DMA done
static void at_spi_wrdma_done(void)
{
    spi_transaction_t end_t = {
        .cmd = CMD_HD_WR_END_REG,
    };
    spi_device_polling_transmit(spi_dev_handle, &end_t);
}

// when spi slave ready to send/recv data from the spi master, the spi slave will a trigger GPIO interrupt,
// then spi master should query whether the slave will perform read or write operation.
static spi_recv_opt_t query_slave_data_trans_info()
{
    spi_recv_opt_t recv_opt = {};
    spi_transaction_t trans = {
        .cmd = CMD_HD_RDBUF_REG,
        .addr = RDBUF_START_ADDR,
        .rxlength = 4 * 8,
        .rx_buffer = &recv_opt,
    };
    spi_device_polling_transmit(spi_dev_handle, (spi_transaction_t*)&trans);

    return recv_opt;
}

// before spi master write to slave, the master should write WRBUF_REG register to notify slave,
// and then wait for handshake line trigger gpio interrupt to start the data transmission.
static void spi_master_request_to_write(uint8_t send_seq, uint16_t send_len)
{
    spi_send_opt_t send_opt;
    send_opt.magic = 0xFE;
    send_opt.send_seq = send_seq;
    send_opt.send_len = send_len;

    spi_transaction_t trans = {
        .cmd = CMD_HD_WRBUF_REG,
        .addr = WRBUF_START_ADDR,
        .length = 4 * 8,
        .tx_buffer = &send_opt,
    };
    spi_device_polling_transmit(spi_dev_handle, (spi_transaction_t*)&trans);
    // increment
    current_send_seq  = send_seq;
}

// spi master write data to slave
static int8_t spi_write_data(uint8_t* buf, int32_t len)
{
    if (len > SPI_TRANS_MAX_LEN) {
        M1_LOG_E(TAG, "Send length error, len:%ld\r\n", len);
        return -1;
    }
    at_spi_master_send_data(buf, len);
    at_spi_wrdma_done();
    return 0;
}

// write data to spi tx_ring_buf, this is just for test
static int32_t write_data_to_spi_task_tx_ring_buf(const void* data, size_t size)
{
    int32_t length = size;

    if (data == NULL  || length > STREAM_BUFFER_SIZE) {
        M1_LOG_E(TAG, "Write data error, len:%ld\r\n", length);
        return -1;
    }

    length = xStreamBufferSend(spi_master_tx_ring_buf, data, size, portMAX_DELAY);
    return length;
}


// notify slave to recv data
static void notify_slave_to_recv(void)
{
    if (initiative_send_flag == 0)
    {
        spi_mutex_lock();
        uint32_t tmp_send_len = xStreamBufferBytesAvailable(spi_master_tx_ring_buf);
        if (tmp_send_len > 0)
        {
            plan_send_len = tmp_send_len > SPI_TRANS_MAX_LEN ? SPI_TRANS_MAX_LEN : tmp_send_len;
            spi_master_request_to_write(current_send_seq + 1, plan_send_len); // to tell slave that the master want to write data
            initiative_send_flag = 1;
        }
        spi_mutex_unlock();
    }
}


static void spi_trans_control_task(void* arg)
{
    esp_err_t ret;
    spi_master_msg_t trans_msg = {0};
    uint32_t send_len = 0;
    esp_queue_elem_t *elem = NULL;
    char *app_resp = NULL;

    uint8_t *trans_data = (uint8_t*)malloc(SPI_TRANS_MAX_LEN * sizeof(uint8_t));
    if (trans_data == NULL)
    {
        M1_LOG_E(TAG, "malloc fail\r\n");
        return;
    }

    while (1)
    {
        xQueueReceive(esp_spi_msg_queue, (void*)&trans_msg, (TickType_t)portMAX_DELAY);

        /* Graceful-exit sentinel: deinit caller set the stop flag and sent a
         * dummy message to wake us.  Free private heap and exit before doing
         * any SPI work — hardware may already be deInit'd at this point. */
        if (s_esp32_stop_task)
            break;

        spi_mutex_lock();
        spi_recv_opt_t recv_opt = query_slave_data_trans_info();

        if (recv_opt.direct == SPI_WRITE)
        {
            if (plan_send_len == 0) {
                M1_LOG_E(TAG, "master want send data but length is 0\r\n");
                spi_mutex_unlock();
                continue;
            }

            if (recv_opt.seq_num != current_send_seq) {
                M1_LOG_E(TAG, "SPI send seq error, %x, %x\r\n", recv_opt.seq_num, current_send_seq);
                if (recv_opt.seq_num == 1) {
                    M1_LOG_E(TAG, "Maybe SLAVE restart, ignore\r\n");
                }
                current_send_seq = recv_opt.seq_num;
            }

            //initiative_send_flag = 0;
            send_len = xStreamBufferReceive(spi_master_tx_ring_buf, (void*) trans_data, plan_send_len, 0);

            if (send_len != plan_send_len) {
                M1_LOG_E(TAG, "Read len expect %lu, but actual read %lu\r\n", plan_send_len, send_len);
                initiative_send_flag = 0;
                spi_mutex_unlock();
                continue;
            }

            ret = spi_write_data(trans_data, plan_send_len);
            if (ret < 0) {
                M1_LOG_E(TAG, "Load data error\r\n");
                initiative_send_flag = 0;
                spi_mutex_unlock();
                continue;
            }

            // maybe streambuffer filled some data when SPI transmit, just consider it after send done, because send flag has already in SLAVE queue
            uint32_t tmp_send_len = xStreamBufferBytesAvailable(spi_master_tx_ring_buf);
            if (tmp_send_len > 0) {
                plan_send_len = tmp_send_len > SPI_TRANS_MAX_LEN ? SPI_TRANS_MAX_LEN : tmp_send_len;
                spi_master_request_to_write(current_send_seq + 1, plan_send_len);
            } else {
                initiative_send_flag = 0;
            }

        } // if (recv_opt.direct == SPI_WRITE)
        else if (recv_opt.direct == SPI_READ)
        {
            if (recv_opt.seq_num != ((current_recv_seq + 1) & 0xFF)) {
                M1_LOG_E(TAG, "SPI recv seq error, %x, %x\r\n", recv_opt.seq_num, (current_recv_seq + 1));
                if (recv_opt.seq_num == 1) {
                    M1_LOG_E(TAG, "Maybe SLAVE restart, ignore\r\n");
                }
                current_recv_seq = recv_opt.seq_num;
            }

            if (recv_opt.transmit_len > STREAM_BUFFER_SIZE || recv_opt.transmit_len == 0) {
                M1_LOG_E(TAG, "SPI read len error, %x\r\n", recv_opt.transmit_len);
                at_spi_rddma_done();
                spi_mutex_unlock();
                continue;
            }

            current_recv_seq = recv_opt.seq_num;
            memset(trans_data, 0x0, recv_opt.transmit_len);
            at_spi_master_recv_data(trans_data, recv_opt.transmit_len);
            at_spi_rddma_done();
            trans_data[recv_opt.transmit_len] = '\0';
#ifdef M1_APP_ESP_RESPONSE_PRINT_ENABLE
            printf("%s", trans_data);
            fflush(stdout);    //Force to print even if have not '\n'
#endif // #ifdef M1_APP_ESP_RESPONSE_PRINT_ENABLE
    		/* Allocate app struct for response */
    		app_resp = (uint8_t *)malloc(recv_opt.transmit_len + 1);
    		if (!app_resp)
    		{
    			M1_LOG_E(TAG, "Failed to allocate app_resp %d\r\n", recv_opt.transmit_len + 1);
    			spi_mutex_unlock();
    			continue;
    		}
    		/* Copy the response payload verbatim.  A length-based copy (not
    		 * strcpy) is required so binary M1_RPC frames — whose header/CRC
    		 * bytes routinely contain 0x00 — are delivered intact rather than
    		 * truncated at the first NUL.  buf_len below carries the true byte
    		 * count for binary consumers; the trailing NUL keeps text (AT)
    		 * consumers, which treat the buffer as a C string, working. */
    		esp32_spi_bin_copy((uint8_t *)app_resp, recv_opt.transmit_len + 1,
    		                   (const uint8_t *)trans_data, recv_opt.transmit_len);
    		app_resp[recv_opt.transmit_len] = '\0';

    		xSemaphoreGive(esp_ctrl_req_sem);

    		elem = (esp_queue_elem_t*)malloc(sizeof(esp_queue_elem_t));
			if (!elem)
			{
				M1_LOG_E(TAG, "%s %u: Malloc failed\n",__func__,__LINE__);
				free(app_resp);
				spi_mutex_unlock();
				continue;
			}
			elem->buf = app_resp;
			elem->buf_len = recv_opt.transmit_len;
			elem->uid = current_uid;
			if ( esp_queue_put(ctrl_msg_Q, (void*)elem) )
			{
				M1_LOG_E(TAG, "%s %u: ctrl Q put fail\r\n",__func__,__LINE__);
				free(app_resp);
				free(elem);
				spi_mutex_unlock();
				continue;
			} // if ( esp_queue_put(ctrl_msg_Q, (void*)elem) )

			xSemaphoreGive(esp_resp_read_sem);
        } // else if (recv_opt.direct == SPI_READ)
        else
        {
            M1_LOG_D(TAG, "Unknown direct: %d", recv_opt.direct);
            spi_mutex_unlock();
            continue;
        }

        spi_mutex_unlock();
    } // while (1)

    free(trans_data);
    /* Signal the deinit caller that trans_data is freed and we are about to
     * self-delete.  xSemaphoreGive is unconditional — the deinit side uses a
     * dedicated binary semaphore rather than xTaskNotify so that an unrelated
     * pending task notification on the deinit caller's task cannot be mistaken
     * for our exit signal. */
    if (s_deinit_sync_sem)
        xSemaphoreGive(s_deinit_sync_sem);
    vTaskDelete(NULL);
}


uint8_t spi_AT_app_send_command(ctrl_cmd_t *app_req)
{
	int ret = SUCCESS;

	if (!app_req)
	{
		return CTRL_ERR_INCORRECT_ARG;
	}

	/* 1. Check if any ongoing request present
	 * Send failure in that case */
	ret = xSemaphoreTake(esp_ctrl_req_sem, SEC_TO_MILLISEC(WAIT_TIME_B2B_CTRL_REQ));
	if (ret!=pdPASS)
	{
		return CTRL_ERR_REQ_IN_PROG;
	}
	app_req->msg_type = CTRL_REQ;
	// handle rollover in uid value (range: 1 to INT32_MAX)
	if (uid < INT32_MAX)
		uid++;
	else
		uid = 1;
	app_req->uid = uid;

    write_data_to_spi_task_tx_ring_buf(app_req->at_cmd, app_req->cmd_len);
    notify_slave_to_recv();

    return SUCCESS;
} // uint8_t spi_AT_app_send_command(ctrl_cmd_t *app_req)


static uint8_t *spi_AT_app_get_response(int *read_len, uint32_t *uid, int timeout_sec)
{
	void *data = NULL;
	uint8_t *buf = NULL;
	esp_queue_elem_t *elem = NULL;
	int ret = 0;

	/* 1. Any problems in response, return NULL */
	if (!read_len)
	{
		M1_LOG_E(TAG, "Invalid input parameter\r\n");
		return NULL;
	}

	/* 2. If timeout not specified, use default */
	if (!timeout_sec)
		timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;

	/* 3. Wait for response */
	ret = xSemaphoreTake(esp_resp_read_sem, SEC_TO_MILLISEC(timeout_sec));
	if (ret!=pdPASS)
	{
		M1_LOG_E(TAG, "ESP response timed out after %u sec\r\n", timeout_sec);
		xSemaphoreGive(esp_ctrl_req_sem);
		return NULL;
	}

	/* 4. Fetch response from `esp_queue` */
	data = esp_queue_get(ctrl_msg_Q);
	if (data)
	{
		elem = (esp_queue_elem_t *)data;
		if (!elem)
			return NULL;

		*read_len = elem->buf_len;
		*uid = elem->uid;
		buf = elem->buf;
		free(elem);
		if ( esp_queue_check(ctrl_msg_Q) ) // There's still data in the queue?
			xSemaphoreGive(esp_resp_read_sem); // Give the app the chance to read again
		return buf;
	}
	else
	{
		M1_LOG_E(TAG, "Ctrl Q empty or uninitialized\r\n");
		return NULL;
	}

	return NULL;
} // static uint8_t *spi_AT_app_get_response(int *read_len, uint32_t *uid, int timeout_sec)


uint8_t spi_AT_send_recv(const char *at_cmd, char *out_buf, int out_buf_size, int timeout_sec)
{
	ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
	uint8_t ret;
	int rx_len = 0;
	uint32_t rx_uid = 0;
	uint8_t *rx_buf = NULL;
	int total_len = 0;

	if (!at_cmd || !out_buf || out_buf_size < 2)
		return CTRL_ERR_INCORRECT_ARG;

	out_buf[0] = '\0';

	req.at_cmd = (char *)at_cmd;
	req.cmd_len = strlen(at_cmd);
	if (!timeout_sec)
		timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;
	req.cmd_timeout_sec = timeout_sec;

	ret = spi_AT_app_send_command(&req);
	if (ret != SUCCESS)
	{
		snprintf(out_buf, out_buf_size, "SEND_ERR=%d", ret);
		return ret;
	}

	/* Collect responses until OK/ERROR/timeout (up to buffer) */
	while (total_len < out_buf_size - 1)
	{
		rx_buf = spi_AT_app_get_response(&rx_len, &rx_uid, timeout_sec);
		if (!rx_buf)
		{
			if (total_len == 0)
				snprintf(out_buf, out_buf_size, "TIMEOUT(%ds)", timeout_sec);
			break;
		}

		int copy_len = rx_len;
		if (total_len + copy_len >= out_buf_size - 1)
			copy_len = out_buf_size - 1 - total_len;

		memcpy(out_buf + total_len, rx_buf, copy_len);
		total_len += copy_len;
		out_buf[total_len] = '\0';
		free(rx_buf);

		/* Stop if we got a final response */
		if (strstr(out_buf, "\r\nOK\r\n") || strstr(out_buf, "\r\nERROR\r\n")
				|| strstr(out_buf, "OK\r\n") || strstr(out_buf, "ERROR\r\n"))
			break;
	}

	return SUCCESS;
} // uint8_t spi_AT_send_recv(...)


/******************************************************************************/
/**
  * @brief  Binary-safe send/receive over the SPI-HD transport.
  *
  * Sends an arbitrary binary frame (no NUL-termination assumption) to the
  * ESP32 and returns exactly one binary response frame.  Unlike
  * spi_AT_send_recv() this does NOT scan the reply for a textual "OK"/"ERROR"
  * terminator and does NOT copy via strcpy — it is used for the CD3 native
  * M1_RPC protocol (magic 0x4D31), whose frames contain embedded 0x00 bytes.
  *
  * The send path is length-based (via ctrl_cmd_t.cmd_len), and the receive
  * path copies exactly the number of bytes the slave reported, so binary
  * payloads survive intact in both directions.
  *
  * @param  tx_buf      Binary request frame to send
  * @param  tx_len      Length of @p tx_buf in bytes (> 0)
  * @param  rx_buf      Caller buffer for the binary response
  * @param  rx_buf_size Capacity of @p rx_buf in bytes (>= 1)
  * @param  out_len     [out] bytes written to @p rx_buf (0 on error/timeout)
  * @param  timeout_sec Response timeout in seconds (0 → DEFAULT_CTRL_RESP_TIMEOUT)
  * @return SUCCESS on success, CTRL_ERR_* otherwise
  */
/******************************************************************************/
uint8_t spi_AT_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                             uint8_t *rx_buf, int rx_buf_size,
                             int *out_len, int timeout_sec)
{
	ctrl_cmd_t req = CTRL_CMD_DEFAULT_REQ();
	uint8_t ret;
	int rx_len = 0;
	uint32_t rx_uid = 0;
	uint8_t *resp = NULL;

	if (out_len)
		*out_len = 0;

	if (!tx_buf || tx_len <= 0 || !rx_buf || rx_buf_size < 1)
		return CTRL_ERR_INCORRECT_ARG;

	req.at_cmd  = (char *)tx_buf;   /* send path is length-based (cmd_len) */
	req.cmd_len = (uint16_t)tx_len;
	if (!timeout_sec)
		timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;
	req.cmd_timeout_sec = timeout_sec;

	ret = spi_AT_app_send_command(&req);
	if (ret != SUCCESS)
		return ret;

	/* Binary frames carry no textual OK/ERROR terminator, so read exactly
	 * one response element and copy it verbatim (embedded NULs and all). */
	resp = spi_AT_app_get_response(&rx_len, &rx_uid, timeout_sec);
	if (!resp || rx_len <= 0)
	{
		if (resp)
			free(resp);
		return CTRL_ERR_REQUEST_TIMEOUT;
	}

	{
		size_t copied = esp32_spi_bin_copy(rx_buf, (size_t)rx_buf_size,
		                                   resp, (size_t)rx_len);
		if (out_len)
			*out_len = (int)copied;
	}
	free(resp);

	return SUCCESS;
} // uint8_t spi_AT_send_recv_bin(...)


/******************************************************************************/
/**
  * @brief  Full-duplex "M1 Link" master transport for the native brain CD3.
  *
  * The brain CD3 firmware (hapaxx11/m1-esp32-brain) is an ESP-IDF full-duplex
  * `spi_slave` device — NOT the ESP-AT half-duplex `spi_slave_hd` slave that
  * spi_AT_send_recv_bin() drives.  Every transaction clocks EXACTLY
  * M1_ESP32_M1LINK_MTU (512) bytes in both directions at once, with one m1_rpc
  * frame at the head of the buffer and zero padding after it.  The slave
  * pipelines its reply onto a LATER transaction, so this transport issues the
  * request then follow-up IDLE transactions, scanning for the matching reply
  * (all handled by the pure helper m1_esp32_m1link_send_recv()).
  *
  * CS (PB10) and the HANDSHAKE line (PD7) are driven manually per transaction,
  * mirroring the SiN360 direct-HAL pattern in m1_esp32_cmd.c.  This path does
  * NOT depend on the ESP-AT RTOS task (spi_trans_control_task) being present
  * -- the brain firmware itself never runs it -- but DOES take the same
  * shared `pxMutex` that task uses whenever it has been created, so the two
  * can never clock SPI3 at the same time (issue #719 C1: SPI3 bus
  * contention).  Safe to call before the AT task has ever been started
  * (pxMutex is still NULL and no locking is attempted).
  *
  * @param  tx_buf      Binary request frame to send (built by m1_esp32_rpc_*)
  * @param  tx_len      Length of @p tx_buf in bytes (> 0, <= 512)
  * @param  rx_buf      Caller buffer for the binary response frame
  * @param  rx_buf_size Capacity of @p rx_buf in bytes (>= 1)
  * @param  out_len     [out] bytes written to @p rx_buf (0 on error/timeout)
  * @param  timeout_sec Real wall-clock budget (seconds) the transport waits
  *                     for a reply before giving up (issue #719 Phase 7);
  *                     <= 0 falls back to a 2 s default.
  * @return SUCCESS on success, CTRL_ERR_* otherwise
  *
  * The wall-clock milliseconds actually spent in the poll loop (regardless of
  * outcome) are recorded and readable via m1_esp32_m1link_last_elapsed_ms() --
  * see the "Poll-budget wall-clock diagnostic" comment in m1_esp32_rpc.h
  * (issue #719 Phase 6).
  */
/******************************************************************************/
#define M1LINK_SPI_TIMEOUT_MS   100u
#define M1LINK_HS_TIMEOUT_MS    100u
#define M1LINK_BUSY_RETRY_MS    150u

static void m1link_cs_delay(void)
{
	/* Short delay (~1us) for the ESP32 SPI slave to recognize the CS edge. */
	for (volatile int i = 0; i < 50; i++) {}
}

/* Yield for ~1 ms while waiting. vTaskDelay is only valid once the scheduler
 * is running; before that (or if it is suspended) fall back to HAL_Delay so we
 * still pace the loop without an illegal RTOS call. */
static void m1link_delay_1ms(void)
{
	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
		vTaskDelay(pdMS_TO_TICKS(1));
	else
		HAL_Delay(1);
}

/* Wait for the brain's HANDSHAKE line (PD7) to signal armed/ready, bounded by
 * @p timeout_ms.  Returns true if it asserted within the window.
 *
 * Yields between polls instead of busy-spinning: a brain that is mid-scan holds
 * the link for a second or more, and a tight spin here starves the FreeRTOS
 * idle hook / watchdog feeder and can trigger an IWDG reset (this mirrors the
 * proven C3 m1_link master, which yields for exactly this reason). */
static bool m1link_wait_handshake(uint32_t timeout_ms)
{
	uint32_t start = HAL_GetTick();
	while (HAL_GPIO_ReadPin(ESP32_HANDSHAKE_GPIO_Port, ESP32_HANDSHAKE_Pin)
	       != GPIO_PIN_SET) {
		if ((HAL_GetTick() - start) >= timeout_ms)
			return false;
		m1link_delay_1ms();
	}
	return true;
}

/* Take the shared SPI3 mutex before a full-duplex M1 Link transfer, but only
 * if the host AT task has actually been created (pxMutex is a file-scope
 * static, zero-initialised to NULL until init_master_hd() creates it inside
 * esp32_main_init()).  This lets the M1 Link probe run standalone — before
 * the AT task has ever been started — while still serialising with
 * spi_trans_control_task on SPI3 whenever that task exists, regardless of
 * which caller started it (see issue #719 C1: SPI3 bus contention between
 * the host AT task and the M1 Link probe). */
static void m1link_lock_spi_if_at_task_present(void)
{
	if (pxMutex)
		spi_mutex_lock();
}

static void m1link_unlock_spi_if_at_task_present(void)
{
	if (pxMutex)
		spi_mutex_unlock();
}

/* Single fixed-size full-duplex exchange; matches m1_esp32_m1link_xfer_fn.
 * The caller (spi_m1link_send_recv_bin) holds the shared SPI3 mutex for the
 * entire abort + timed-exchange sequence, so no per-transfer locking is needed
 * here — doing it per-transfer would serialise individual transactions but still
 * allow spi_trans_control_task to interpose between request and reply polls. */
static int m1link_hal_xfer(const uint8_t *tx, uint8_t *rx, uint16_t mtu,
                           void *ctx)
{
	HAL_StatusTypeDef ret;
	uint32_t start;
	(void)ctx;

	/* Honour the brain's HANDSHAKE: never clock while the slave is not armed.
	 * If it never asserts within the window we still attempt the clock once
	 * (best effort), but a persistent low usually means the slave firmware is
	 * not running. */
	(void)m1link_wait_handshake(M1LINK_HS_TIMEOUT_MS);

	start = HAL_GetTick();
	do {
		HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin,
		                  GPIO_PIN_RESET);
		m1link_cs_delay();

		ret = HAL_SPI_TransmitReceive(&hspi_esp, (uint8_t *)tx, rx, mtu,
		                              M1LINK_SPI_TIMEOUT_MS);

		m1link_cs_delay();
		HAL_GPIO_WritePin(ESP32_SPI3_NSS_GPIO_Port, ESP32_SPI3_NSS_Pin,
		                  GPIO_PIN_SET);

		if (ret != HAL_BUSY)
			break;
		m1link_delay_1ms();
	} while ((HAL_GetTick() - start) < M1LINK_BUSY_RETRY_MS);

	if (ret != HAL_OK)
	{
		/* SELF-HEAL: a transient SPI fault (OVR / mode fault / HAL timeout)
		 * leaves SPI3's FIFO/packing byte-shifted, so every later fixed-size
		 * exchange would fail CRC and the brain would read as permanently
		 * unavailable until a full re-init.  Abort resets the peripheral state
		 * machine and flushes the FIFOs so the NEXT transaction recovers on its
		 * own (same recovery the C3 m1_link master relies on). */
		HAL_SPI_Abort(&hspi_esp);
		return -1;
	}
	return 0;
}

/* Wall-clock ms spent in the most recent spi_m1link_send_recv_bin() poll
 * loop, regardless of outcome -- see m1_esp32_m1link_last_elapsed_ms() below
 * and the "Poll-budget wall-clock diagnostic" comment in m1_esp32_rpc.h
 * (issue #719 Phase 6). */
static uint32_t s_m1link_last_elapsed_ms;

/* Read back the elapsed time recorded by the last spi_m1link_send_recv_bin()
 * call.  Consulted by m1_esp32_rpc_call() to annotate its "no-reply"
 * diagnostic line with how long the transport actually waited, so a repeated
 * "no-reply" report after a poll-budget fix can be told apart from a
 * transport that is giving up early. */
uint32_t m1_esp32_m1link_last_elapsed_ms(void)
{
	return s_m1link_last_elapsed_ms;
}

/* 1 ms inter-poll yield so the RTOS idle task / watchdog gets CPU time
 * during the long brain CD3 scan timeout.  Injected as a pacing callback
 * into m1_esp32_m1link_send_recv_timed() between each unmatched poll. */
static void m1link_pace_1ms(void *ctx)
{
	(void)ctx;
	vTaskDelay(pdMS_TO_TICKS(1));
}

uint8_t spi_m1link_send_recv_bin(const uint8_t *tx_buf, int tx_len,
                                 uint8_t *rx_buf, int rx_buf_size,
                                 int *out_len, int timeout_sec)
{
	/* 512-byte working buffers for fixed-size M1 Link transactions.
	 * Keep these on the caller stack so we don't permanently reserve
	 * additional .bss in the firmware image. */
	uint8_t s_m1link_tx[M1_ESP32_M1LINK_MTU];
	uint8_t s_m1link_rx[M1_ESP32_M1LINK_MTU];
	uint8_t  rc;
	uint32_t timeout_ms;
	uint32_t t0 = HAL_GetTick();

	if (out_len)
		*out_len = 0;

	if (!tx_buf || tx_len <= 0 || !rx_buf || rx_buf_size < 1)
		return CTRL_ERR_INCORRECT_ARG;

	/* The poll budget is the response wait, paced on real wall-clock time
	 * (issue #719 Phase 7) rather than a poll count: earlier revisions
	 * derived a fixed transaction COUNT from timeout_sec assuming each poll
	 * costs ~M1LINK_HS_TIMEOUT_MS, but a poll (HANDSHAKE wait + one SPI
	 * transfer) can complete in well under a millisecond whenever the
	 * brain's HANDSHAKE line is already asserted, so that count-based budget
	 * could exhaust in a fraction of a second regardless of timeout_sec --
	 * see the "Poll-budget wall-clock FIX" comment in m1_esp32_rpc.h. Pacing
	 * on HAL_GetTick() instead means we always wait the caller's requested
	 * timeout_sec, however fast or slow individual transactions turn out to
	 * be. max_iterations remains only as a generous safety backstop against
	 * a runaway loop if the clock source itself ever misbehaves. */
	/* Floor for a poll budget when the caller passes no timeout (seconds). */
	const int m1link_default_timeout_s = 2;
	if (timeout_sec <= 0)
		timeout_sec = m1link_default_timeout_s;
	timeout_ms = (uint32_t)timeout_sec * 1000u;

	/* Hold the mutex across the abort AND the full timed request/poll sequence.
	 * Taking it per-transfer (inside m1link_hal_xfer) serialises individual
	 * transactions but still allows spi_trans_control_task to acquire the mutex
	 * and issue an incompatible half-duplex transaction between the request
	 * unlock and the next IDLE poll.  Holding it here for the entire sequence
	 * prevents that interleaving (issue #719 Phase 1 C1). */
	m1link_lock_spi_if_at_task_present();

	/* Flush any residual FIFO / packing state before full-duplex M1 Link
	 * transactions.  The brain (CD3) detection probe runs AFTER the half-duplex
	 * AT / SiN360 probes; a transfer that timed out against a non-responding
	 * slave can leave bytes stranded in SPI3's RX/TX FIFO.  Left in place, that
	 * residue byte-shifts every subsequent full-duplex frame so the reply never
	 * validates and the brain is misdetected as "Unknown (fallback)".  Abort
	 * resets the peripheral state machine and flushes the FIFOs so the next
	 * M1 Link transaction starts byte-aligned (harmless when already idle). */
	HAL_SPI_Abort(&hspi_esp);

	rc = m1_esp32_m1link_send_recv_timed(m1link_hal_xfer, NULL,
	                                     s_m1link_tx, s_m1link_rx,
	                                     M1_ESP32_M1LINK_MTU,
	                                     HAL_GetTick,
	                                     timeout_ms, 20000,
	                                     m1link_pace_1ms, NULL,
	                                     tx_buf, tx_len,
	                                     rx_buf, rx_buf_size, out_len);

	m1link_unlock_spi_if_at_task_present();

	s_m1link_last_elapsed_ms = HAL_GetTick() - t0;

	switch (rc) {
	case 0u:  return SUCCESS;
	case 1u:  return CTRL_ERR_INCORRECT_ARG;
	case 2u:  return CTRL_ERR_TRANSPORT_SEND;
	default:  return CTRL_ERR_REQUEST_TIMEOUT; /* 3 overflow / 4 no-match */
	}
} // uint8_t spi_m1link_send_recv_bin(...)


void esp32_queue_reset(void)
{
	q_node_t *list_head = NULL;
	q_node_t *temp = NULL;
	esp_queue_elem_t *elem = NULL;

	if (!ctrl_msg_Q)
		return;

	/* Detach the entire list atomically so the SPI transport task
	 * cannot keep the loop running indefinitely. */
	vTaskSuspendAll();
	list_head = ctrl_msg_Q->front;
	ctrl_msg_Q->front = NULL;
	ctrl_msg_Q->rear = NULL;
	(void)xTaskResumeAll();

	/* Free detached nodes outside the critical section */
	while (list_head != NULL)
	{
		temp = list_head;
		list_head = list_head->next;
		elem = (esp_queue_elem_t *)temp->data;
		if (elem)
		{
			free(elem->buf);
			free(elem);
		}
		free(temp);
	}
}

bool esp32_queue_reset_locked(void)
{
	BaseType_t sem_taken;

	if (!esp_ctrl_req_sem)
		return false;

	sem_taken = xSemaphoreTake(esp_ctrl_req_sem, SEC_TO_MILLISEC(WAIT_TIME_B2B_CTRL_REQ));
	if (sem_taken != pdPASS)
		return false;

	esp32_queue_reset();
	xSemaphoreGive(esp_ctrl_req_sem);
	return true;
}


static void init_master_hd(spi_device_handle_t* spi)
{
	spi_device_handle_t spi_dev;

	/* queue init */
	ctrl_msg_Q = create_esp_queue();
	if (!ctrl_msg_Q) {
		M1_LOG_E(TAG, "Failed to create app ctrl msg Q\r\n");
		return;
	}
    // Create the message queue.
    esp_spi_msg_queue = xQueueCreate(5, sizeof(spi_master_msg_t));
    // Create the tx_buf.
    spi_master_tx_ring_buf = xStreamBufferCreate(STREAM_BUFFER_SIZE, 1);
    // Create the semaphore.
    pxMutex = xSemaphoreCreateMutex();

    /* semaphore init */
	esp_ctrl_req_sem = xSemaphoreCreateBinary();
	assert(esp_ctrl_req_sem);
	esp_resp_read_sem = xSemaphoreCreateBinary();
	assert(esp_resp_read_sem );
	/*
	Note that binary semaphores created using
	 * the vSemaphoreCreateBinary() macro are created in a state such that the
	 * first call to 'take' the semaphore would pass, whereas binary semaphores
	 * created using xSemaphoreCreateBinary() are created in a state such that the
	 * the semaphore must first be 'given' before it can be 'taken'
	 *
	*/
	/* Get read semaphore for first time */
	//xSemaphoreTake(esp_resp_read_sem, portMAX_DELAY);
	/* Give req semaphore for first time */
	xSemaphoreGive(esp_ctrl_req_sem);

    spi_dev = pvPortMalloc(sizeof(struct spi_device_t));
    assert(spi_dev!=NULL);
    memset(spi_dev, 0, sizeof(struct spi_device_t));
    spi_dev->id = ESP_SPI_ID;
    spi_dev->cfg.flags = 0;
    spi_dev->host = pvPortMalloc(sizeof(spi_host_t));
    assert(spi_dev->host!=NULL);
    memset(spi_dev->host, 0, sizeof(spi_host_t));
    *spi = spi_dev;

    spi_mutex_lock();

    /* Poll slave status with retries — ESP32 may still be booting */
    spi_recv_opt_t recv_opt = {};
    int retry;
    for (retry = 0; retry < 20; retry++) {
        recv_opt = query_slave_data_trans_info();
        M1_LOG_I(TAG, "init query[%d]: direct=%u seq=%u len=%u\r\n",
                 retry, recv_opt.direct, recv_opt.seq_num, recv_opt.transmit_len);
        if (recv_opt.direct == SPI_READ || recv_opt.direct == SPI_WRITE) {
            break;  /* Got a valid response from slave */
        }
        spi_mutex_unlock();
        HAL_Delay(500);  /* Wait 500ms between retries */
        spi_mutex_lock();
    }
    if (retry >= 20) {
        M1_LOG_E(TAG, "ESP32 slave not responding after %d retries\r\n", retry);
    }

    if (recv_opt.direct == SPI_READ) {
        if (recv_opt.seq_num != ((current_recv_seq + 1) & 0xFF)) {
            M1_LOG_E(TAG, "SPI recv seq error, %x, %x\r\n", recv_opt.seq_num, (current_recv_seq + 1));
            if (recv_opt.seq_num == 1) {
                M1_LOG_E(TAG, "Maybe SLAVE restart, ignore\r\n");
            }
        }
        current_recv_seq = recv_opt.seq_num;
        /* Must actually read the pending data via RDDMA before sending INT0.
         * Without this, the slave's DMA TX transaction never completes and
         * its SPI task hangs forever waiting on spi_slave_hd_get_trans_res(). */
        if (recv_opt.transmit_len > 0 && recv_opt.transmit_len <= SPI_TRANS_MAX_LEN) {
            uint8_t *drain_buf = pvPortMalloc(recv_opt.transmit_len);
            if (drain_buf) {
                at_spi_master_recv_data(drain_buf, recv_opt.transmit_len);
                M1_LOG_I(TAG, "Drained %u bytes of boot data from slave\r\n",
                         recv_opt.transmit_len);
                vPortFree(drain_buf);
            }
        }
        at_spi_rddma_done();
    }

    spi_mutex_unlock();
} // static void init_master_hd(spi_device_handle_t* spi)


bool get_esp32_main_init_status(void)
{
	return esp32_main_init_done;
} // bool get_esp32_main_init_status(void)


/******************************************************************************/
/**
  * @brief Tear down the legacy AT-over-SPI task and release all heap it holds.
  *
  * Signals spi_trans_control_task to exit, waits for it to free its private
  * trans_data buffer, then releases the RTOS objects and device handle that
  * were allocated by init_master_hd().  Safe to call from any task context.
  * No-op when esp32_main_init() was never called.
  *
  * Must be called while the FreeRTOS scheduler is running and from a task
  * context (not from an ISR).
  */
/******************************************************************************/
void esp32_main_deinit(void)
{
	BaseType_t joined;

	if (!esp32_main_init_done)
		return;

	/* Signal the SPI control task to exit.  Place the sentinel at the FRONT
	 * of the queue so it is received before any stale notification messages,
	 * and the task never attempts SPI work after the hardware is deInit'd. */
	if (s_spi_task_handle != NULL)
	{
		spi_master_msg_t sentinel = { .slave_notify_flag = false };
		s_esp32_stop_task = true;
		/* Best-effort: wait up to 100 ms for a queue slot.  Even if this
		 * times out, the task will see s_esp32_stop_task on its next dequeue
		 * iteration and still exit — just without the explicit wake-up.
		 * Guard against NULL: init_master_hd() may have failed to create
		 * the queue (e.g. under low-memory conditions), in which case the
		 * task relies solely on the stop flag. */
		if (esp_spi_msg_queue != NULL)
			(void)xQueueSendToFront(esp_spi_msg_queue, &sentinel, pdMS_TO_TICKS(100));

		/* Wait up to 500 ms for the task to give s_deinit_sync_sem, confirming
		 * it has freed trans_data and is about to self-delete.  A dedicated
		 * binary semaphore is used instead of xTaskNotifyWait() to avoid the
		 * race where an unrelated pending notification on the calling task's
		 * notification value causes the wait to return immediately. */
		joined = (s_deinit_sync_sem != NULL)
		       ? xSemaphoreTake(s_deinit_sync_sem, pdMS_TO_TICKS(500))
		       : pdFALSE;

		if (joined != pdTRUE)
		{
			/* xSemaphoreTake timed out — the task has not acknowledged
			 * shutdown.  We cannot safely delete shared RTOS objects
			 * (queue, semaphores) while the task may still be blocked on
			 * them; doing so is undefined behaviour.  Leave all shared
			 * objects allocated and return.  The stop flag remains set so
			 * the task will self-delete when it next runs; the next
			 * esp32_main_deinit() call will find esp32_main_init_done still
			 * true and retry. */
			return;
		}

		/* Task confirmed exit — safe to free shared objects. */
		s_spi_task_handle = NULL;
	}

	/* Release the join semaphore itself now that the task is gone. */
	if (s_deinit_sync_sem)
	{
		vSemaphoreDelete(s_deinit_sync_sem);
		s_deinit_sync_sem = NULL;
	}

	/* Release all RTOS objects and device-handle memory that were allocated
	 * by init_master_hd().  The task has already freed its own trans_data.
	 *
	 * IMPORTANT: for the queue, NULL the global handle BEFORE vQueueDelete so
	 * that any concurrent ISR that calls xQueueSendFromISR() after seeing a
	 * non-NULL pointer (but before the delete completes) hits the NULL guard in
	 * ESP32_GPIO_EXTI_Callback() and skips the send rather than touching a
	 * freed handle.  Use a local copy for the actual deletion. */
	if (esp_spi_msg_queue)
	{
		QueueHandle_t q = esp_spi_msg_queue;
		esp_spi_msg_queue = NULL;  /* ISR NULL-guard: visible before vQueueDelete */
		vQueueDelete(q);
	}
	if (spi_master_tx_ring_buf)
	{
		vStreamBufferDelete(spi_master_tx_ring_buf);
		spi_master_tx_ring_buf = NULL;
	}
	if (pxMutex)
	{
		vSemaphoreDelete(pxMutex);
		pxMutex = NULL;
	}
	if (esp_ctrl_req_sem)
	{
		vSemaphoreDelete(esp_ctrl_req_sem);
		esp_ctrl_req_sem = NULL;
	}
	if (esp_resp_read_sem)
	{
		vSemaphoreDelete(esp_resp_read_sem);
		esp_resp_read_sem = NULL;
	}
	if (spi_dev_handle)
	{
		if (spi_dev_handle->host)
		{
			vPortFree(spi_dev_handle->host);
			spi_dev_handle->host = NULL;
		}
		vPortFree(spi_dev_handle);
		spi_dev_handle = NULL;
	}
	/* Drain and free the ctrl_msg_Q linked list */
	esp_queue_destroy(&ctrl_msg_Q);

	/* Reset sequencing state so the next init cycle starts clean */
	initiative_send_flag = 0;
	plan_send_len = 0;
	current_send_seq = 0;
	current_recv_seq = 0;

	/* Clear the stop flag only after all shared objects have been freed, so
	 * a late-waking task cannot re-enter normal SPI operation on freed memory. */
	s_esp32_stop_task = false;
	esp32_main_init_done = false;
} // void esp32_main_deinit(void)


/**
  * @brief Delay without context switch
  * @param  x in ms approximately
  * @retval None
  */
void hard_delay(uint32_t x)
{
    volatile uint32_t idx;

    for (idx=0; idx<6000*x; idx++) // 100
    {
    	;
    }
}


/**
  * @brief  Reset slave to initialize
  * @param  None
  * @retval None
  */
static void reset_slave(void)
{
	esp32_disable();
	hard_delay(1); // 50
	esp32_enable();
	/* Brief delay for ESP32-C6 SPI slave to initialize after reset.
	 * Stock firmware used hard_delay(200) here (~200ms busy-loop).
	 * The handshake pin check in esp32_main_init() catches missed events. */
	HAL_Delay(200);
}



void esp32_main_init(void)
{
	BaseType_t ret;
	size_t free_heap;

	if ( esp32_main_init_done )
		return;

	/* Reset graceful-exit state from any prior deinit cycle */
	s_esp32_stop_task = false;
	s_spi_task_handle = NULL;
	s_deinit_sync_sem = NULL;

	reset_slave();

	init_master_hd(&spi_dev_handle);

	/* Create the join semaphore BEFORE the task so it is ready the moment
	 * the task gives it on exit.  A binary semaphore is created in the taken
	 * (empty) state, so xSemaphoreTake in deinit will block until the task
	 * calls xSemaphoreGive — no spurious wake from unrelated notifications. */
	s_deinit_sync_sem = xSemaphoreCreateBinary();
	assert(s_deinit_sync_sem != NULL);

    ret = xTaskCreate(spi_trans_control_task, "spi_trans_control_task", M1_TASK_STACK_SIZE_2048, NULL, TASK_PRIORITY_ESP32_TASKS, &s_spi_task_handle);
	assert(ret==pdPASS);
	free_heap = xPortGetFreeHeapSize(); // xPortGetMinimumEverFreeHeapSize()
	assert(free_heap >= M1_LOW_FREE_HEAP_WARNING_SIZE);

	/* If handshake pin is already HIGH, we missed the rising edge interrupt.
	 * Manually enqueue a message so spi_trans_control_task processes it. */
	if (HAL_GPIO_ReadPin(ESP32_HANDSHAKE_GPIO_Port, ESP32_HANDSHAKE_Pin) == GPIO_PIN_SET) {
		M1_LOG_I(TAG, "Handshake already HIGH — injecting missed event\r\n");
		spi_master_msg_t spi_msg = { .slave_notify_flag = true };
		xQueueSend(esp_spi_msg_queue, (void*)&spi_msg, 0);
	}

	esp32_main_init_done = true;
} // void esp32_main_init(void)


static void esp_free_mem( char **buf_ptr)
{
	if ( *buf_ptr != NULL )
	{
		free (*buf_ptr);
		*buf_ptr = NULL;
	}
} // static void esp_free_mem( char **buf_ptr)


uint8_t wifi_ap_scan_list(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *ok_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;
	uint32_t saved_scan_timeout = app_req->cmd_timeout_sec;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_WIFI_MODE, ESP32C6_WIFI_MODE_STA));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass ) // at least one second has passed?
			{
				tick_t0 += MILLISEC_TO_SEC; // Update tick_t0
				if ( app_req->cmd_timeout_sec > tick_pass )
				{
					app_req->cmd_timeout_sec -= tick_pass;
				}
				else
					break; // Timeout
			} // if ( tick_pass )
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf )
				continue;
			if ( rx_uid != current_uid ) // Not the expected response?
				continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) ) // Not the expected response?
				continue;
			ret = SUCCESS;
			break;
		} // while ( true )
		if ( ret==SUCCESS )
		{
			esp_free_mem(&app_req->at_cmd);
			esp_free_mem(&app_req->cmd_resp);
			/* Restore full scan timeout for the actual AP list command */
			app_req->cmd_timeout_sec = saved_scan_timeout;
			tick_t0 = HAL_GetTick();
			app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_LIST_AP, ""));
			app_req->cmd_len = strlen(app_req->at_cmd);
			app_req->cmd_resp = NULL;
			ret = spi_AT_app_send_command(app_req);
			while ( ret==SUCCESS )
			{
				esp_free_mem(&resp_buf);
				rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
				resp_buf = rx_buf;
				if ( rx_buf && rx_buf_len)
				{
					if ( rx_uid != current_uid ) // Not the expected response?
						continue;
					m1_parse_spi_at_resp(rx_buf, ESP32C6_AT_RES_LIST_AP_KEY, app_req);
					ok_buf = strstr(rx_buf, "OK");
					if ( ok_buf!=NULL ) // If "OK" found in the response, it's the last response to receive from the slave
						break; // Complete and exit
					tick_pass = HAL_GetTick() - tick_t0;
					tick_pass /= MILLISEC_TO_SEC;
					if ( tick_pass ) // at least one second has passed?
					{
						tick_t0 += MILLISEC_TO_SEC; // Update tick_t0
						if ( app_req->cmd_timeout_sec > tick_pass )
						{
							app_req->cmd_timeout_sec -= tick_pass;
						}
						else
							break; // Timeout
					} // if ( tick_pass )
				} // if ( rx_buf && rx_buf_len)
				else
					ret = ERROR;
			} // while ( ret==SUCCESS )
		} // if ( ret==SUCCESS )
	} // if ( ret==SUCCESS )
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	} // if ( ret==SUCCESS )
	else
	{
		M1_LOG_E(TAG, "Response not received\r\n");
	}

	return ret;
} // uint8_t wifi_ap_scan_list(ctrl_cmd_t *app_req)



uint8_t ble_scan_list(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *ok_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_CLI));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass ) // at least one second has passed?
			{
				tick_t0 += MILLISEC_TO_SEC; // Update tick_t0
				if ( app_req->cmd_timeout_sec > tick_pass )
				{
					app_req->cmd_timeout_sec -= tick_pass;
				}
				else
					break; // Timeout
			} // if ( tick_pass )
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf )
				continue;
			if ( rx_uid != current_uid ) // Not the expected response?
				continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) ) // Not the expected response?
				continue;
			ret = SUCCESS;
			break;
		} // while ( true )
		if ( ret==SUCCESS )
		{
			esp_free_mem(&app_req->at_cmd);
			esp_free_mem(&app_req->cmd_resp);
			app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_SCAN, "1")); // Scan for 3 seconds, hard coded
			app_req->cmd_len = strlen(app_req->at_cmd);
			app_req->cmd_resp = NULL;
			ret = spi_AT_app_send_command(app_req);
			while ( ret==SUCCESS )
			{
				esp_free_mem(&resp_buf);
				rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
				resp_buf = rx_buf;
				if ( rx_buf && rx_buf_len)
				{
					if ( rx_uid != current_uid ) // Not the expected response?
						continue;
					m1_parse_spi_at_resp(rx_buf, ESP32C6_AT_RES_BLE_SCAN_KEY, app_req);
					ok_buf = strstr(rx_buf, "+BLESCANDONE");
					if ( ok_buf!=NULL ) // If "+BLESCANDONE" found in the response, it's the last response to receive from the slave
					{
						break; // Complete and exit
					}
					tick_pass = HAL_GetTick() - tick_t0;
					tick_pass /= MILLISEC_TO_SEC;
					if ( tick_pass ) // at least one second has passed?
					{
						tick_t0 += MILLISEC_TO_SEC; // Update tick_t0
						if ( app_req->cmd_timeout_sec > tick_pass )
						{
							app_req->cmd_timeout_sec -= tick_pass;
						}
						else
							break; // Timeout
					} // if ( tick_pass )
				} // if ( rx_buf && rx_buf_len)
				else
				{
					ret = ERROR;
					break;
				} // else
			} // while ( ret==SUCCESS )
		} // if ( ret==SUCCESS )
	} // if ( ret==SUCCESS )
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	} // if ( ret==SUCCESS )
	else
	{
		M1_LOG_E(TAG, "Response not received\r\n");
	}

	return ret;
} // uint8_t ble_scan_list(ctrl_cmd_t *app_req)



#ifdef M1_APP_BT_MANAGE_ENABLE

uint8_t ble_scan_list_ex(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *ok_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;
	uint32_t saved_scan_timeout = app_req->cmd_timeout_sec;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();

	/* Deinitialize BLE if it was left active from a previous scan or BLE operation.
	 * AT+BLEINIT=0 is idempotent — returns OK even when BLE is already in mode 0.
	 * Without this, AT+BLEINIT=1 returns ERROR on any repeat scan, causing a
	 * 10-second timeout before showing "Scan failed!". */
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_NULL));

	/* Initialize ble_scan union member */
	app_req->u.ble_scan.count = 0;
	app_req->u.ble_scan.out_list = NULL;

	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_CLI));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass )
			{
				tick_t0 += MILLISEC_TO_SEC;
				if ( app_req->cmd_timeout_sec > tick_pass )
					app_req->cmd_timeout_sec -= tick_pass;
				else
					break;
			}
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf )
				continue;
			if ( rx_uid != current_uid )
				continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) )
				continue;
			ret = SUCCESS;
			break;
		}
		if ( ret==SUCCESS )
		{
			esp_free_mem(&app_req->at_cmd);
			esp_free_mem(&app_req->cmd_resp);
			/* Restore full scan timeout for the actual BLE scan command */
			app_req->cmd_timeout_sec = saved_scan_timeout;
			tick_t0 = HAL_GetTick();
			app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_SCAN, "3"));
			app_req->cmd_len = strlen(app_req->at_cmd);
			app_req->cmd_resp = NULL;
			app_req->msg_id = CTRL_RESP_GET_BLE_SCAN_LIST;
			ret = spi_AT_app_send_command(app_req);
			while ( ret==SUCCESS )
			{
				esp_free_mem(&resp_buf);
				rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
				resp_buf = rx_buf;
				if ( rx_buf && rx_buf_len)
				{
					if ( rx_uid != current_uid )
						continue;
					m1_parse_ble_scan_resp(rx_buf, ESP32C6_AT_RES_BLE_SCAN_KEY, app_req);
					ok_buf = strstr(rx_buf, "+BLESCANDONE");
					if ( ok_buf!=NULL )
					{
						break;
					}
					tick_pass = HAL_GetTick() - tick_t0;
					tick_pass /= MILLISEC_TO_SEC;
					if ( tick_pass )
					{
						tick_t0 += MILLISEC_TO_SEC;
						if ( app_req->cmd_timeout_sec > tick_pass )
							app_req->cmd_timeout_sec -= tick_pass;
						else
							break;
					}
				}
				else
				{
					ret = ERROR;
					break;
				}
			}
		}
	}
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	}
	else
	{
		M1_LOG_E(TAG, "BLE scan response not received\r\n");
	}

	return ret;
} // uint8_t ble_scan_list_ex(ctrl_cmd_t *app_req)



uint8_t esp_get_version(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *resp_buf = NULL;
	char *index;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_GET_VERSION, ""));
	app_req->cmd_resp = NULL;
	app_req->cmd_len = strlen(app_req->at_cmd);

	/* Clear version output */
	memset(app_req->u.wifi_ap_config.status, 0, STATUS_LENGTH);

	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass )
			{
				tick_t0 += MILLISEC_TO_SEC;
				if ( app_req->cmd_timeout_sec > tick_pass )
					app_req->cmd_timeout_sec -= tick_pass;
				else
					break;
			}
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			if ( !rx_buf || !rx_buf_len )
				continue;
			if ( rx_uid != current_uid )
				continue;

			/* Parse "AT version:x.x.x.x..." */
			index = strstr(rx_buf, ESP32C6_AT_RES_VERSION_KEY);
			if ( index )
			{
				size_t i;
				index += strlen(ESP32C6_AT_RES_VERSION_KEY);
				for (i = 0; i < STATUS_LENGTH - 1 && index[i] != '\0' &&
					index[i] != '\r' && index[i] != '\n' && index[i] != '('; i++)
				{
					app_req->u.wifi_ap_config.status[i] = index[i];
				}
				app_req->u.wifi_ap_config.status[i] = '\0';
			}

			/* Check for final "OK" */
			if ( strstr(rx_buf, ESP32C6_AT_RES_OK) )
			{
				ret = SUCCESS;
				break;
			}
		}
	}
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	}
	else
	{
		M1_LOG_E(TAG, "Version query failed\r\n");
	}

	return ret;
} // uint8_t esp_get_version(ctrl_cmd_t *app_req)



uint8_t ble_connect(ctrl_cmd_t *app_req, const char *addr, uint8_t addr_type)
{
	char *rx_buf = NULL;
	char *resp_buf = NULL;
	char at_cmd_buf[64];
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();

	/* Step 1: Init BLE in client mode */
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_CLI));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass )
			{
				tick_t0 += MILLISEC_TO_SEC;
				if ( app_req->cmd_timeout_sec > tick_pass )
					app_req->cmd_timeout_sec -= tick_pass;
				else
					break;
			}
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf ) continue;
			if ( rx_uid != current_uid ) continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) ) continue;
			ret = SUCCESS;
			break;
		}

		/* Step 2: Connect to device AT+BLECONN=0,"addr",addr_type */
		if ( ret==SUCCESS )
		{
			esp_free_mem(&app_req->at_cmd);
			esp_free_mem(&app_req->cmd_resp);

			snprintf(at_cmd_buf, sizeof(at_cmd_buf), "%s0,\"%s\",%u%s",
					ESP32C6_AT_REQ_BLE_CONNECT, addr, addr_type, ESP32C6_AT_REQ_CRLF);
			app_req->at_cmd = strdup(at_cmd_buf);
			app_req->cmd_len = strlen(app_req->at_cmd);
			app_req->cmd_resp = NULL;

			if ( app_req->cmd_timeout_sec < 15 )
				app_req->cmd_timeout_sec = 15;

			ret = spi_AT_app_send_command(app_req);
			app_req->resp_event_status = FAILURE;

			while ( ret==SUCCESS )
			{
				esp_free_mem(&resp_buf);
				rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
				resp_buf = rx_buf;
				if ( rx_buf && rx_buf_len )
				{
					if ( rx_uid != current_uid ) continue;

					/* Check for +BLECONN: success */
					if ( strstr(rx_buf, ESP32C6_AT_RES_BLE_CONNECT_KEY) )
						app_req->resp_event_status = SUCCESS;

					/* Check for final OK or ERROR */
					if ( strstr(rx_buf, ESP32C6_AT_RES_OK) )
					{
						if ( app_req->resp_event_status == SUCCESS )
							break;
					}
					if ( strstr(rx_buf, "ERROR") )
					{
						ret = ERROR;
						break;
					}

					tick_pass = HAL_GetTick() - tick_t0;
					tick_pass /= MILLISEC_TO_SEC;
					if ( tick_pass )
					{
						tick_t0 += MILLISEC_TO_SEC;
						if ( app_req->cmd_timeout_sec > tick_pass )
							app_req->cmd_timeout_sec -= tick_pass;
						else
							break;
					}
				}
				else
				{
					ret = ERROR;
				}
			}
		}
	}

	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS && app_req->resp_event_status==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
	}
	else
	{
		ret = ERROR;
		M1_LOG_E(TAG, "BLE connect failed\r\n");
	}

	return ret;
} // uint8_t ble_connect(ctrl_cmd_t *app_req, const char *addr, uint8_t addr_type)



uint8_t ble_disconnect(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_DISCONNECT, "0"));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass )
			{
				tick_t0 += MILLISEC_TO_SEC;
				if ( app_req->cmd_timeout_sec > tick_pass )
					app_req->cmd_timeout_sec -= tick_pass;
				else
					break;
			}
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf ) continue;
			if ( rx_uid != current_uid ) continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) ) continue;
			ret = SUCCESS;
			break;
		}
	}
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	}
	else
	{
		M1_LOG_E(TAG, "BLE disconnect failed\r\n");
	}

	return ret;
} // uint8_t ble_disconnect(ctrl_cmd_t *app_req)

#endif /* M1_APP_BT_MANAGE_ENABLE */


// Helper: send an AT command and wait for "OK" response (5-sec timeout)
// Reuses app_req's SPI transport. Best-effort — caller decides how to handle ERROR.
static uint8_t esp_at_send_wait_ok(ctrl_cmd_t *app_req, const char *at_cmd_str)
{
	char *rx_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0;
	uint8_t timeout_sec = 5;

	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	app_req->at_cmd = strdup(at_cmd_str);
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);

	ret = spi_AT_app_send_command(app_req);
	if ( ret!=SUCCESS )
	{
		esp_free_mem(&app_req->at_cmd);
		esp_free_mem(&app_req->cmd_resp);
		return ERROR;
	}

	ret = ERROR;
	tick_t0 = HAL_GetTick();
	while (true)
	{
		uint32_t tick_pass = (HAL_GetTick() - tick_t0) / MILLISEC_TO_SEC;
		if ( tick_pass >= timeout_sec )
			break;
		esp_free_mem(&resp_buf);
		vTaskDelay(100);
		rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, timeout_sec);
		resp_buf = rx_buf;
		rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
		if ( !rx_buf ) continue;
		if ( rx_uid != current_uid ) continue;
		if ( strcmp(rx_buf, app_req->cmd_resp) ) continue; // Not "OK"
		ret = SUCCESS;
		break;
	}

	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	return ret;
}


uint8_t ble_advertise(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *ok_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();

	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_SER));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass ) // at least one second has passed?
			{
				tick_t0 += MILLISEC_TO_SEC; // Update tick_t0
				if ( app_req->cmd_timeout_sec > tick_pass )
				{
					app_req->cmd_timeout_sec -= tick_pass;
				}
				else
					break; // Timeout
			} // if ( tick_pass )
			esp_free_mem(&resp_buf);
			vTaskDelay(100); // Give the system some time to avoid possible crash for unknown reason
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf )
				continue;
			if ( rx_uid != current_uid ) // Not the expected response?
				continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) ) // Not the expected response?
				continue;
			ret = SUCCESS;
			break;
		} // while ( true )
		if ( ret==SUCCESS )
		{
			// Set up GATT service and security before advertising (best-effort)
			esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_GATTS_CRE, ""));
			esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_GATTS_START, ""));
			esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_SEC_PARAM, ""));

			esp_free_mem(&app_req->at_cmd);
			esp_free_mem(&app_req->cmd_resp);
			app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_ADVERTISE, ESP32C6_AT_REQ_ADV_DATA));
			app_req->cmd_len = strlen(app_req->at_cmd);
			app_req->cmd_resp = NULL;
			ret = spi_AT_app_send_command(app_req);
			while ( true )
			{
				esp_free_mem(&resp_buf);
				vTaskDelay(100); // Give the system some time to avoid possible crash for unknown reason
				rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
				resp_buf = rx_buf;
				if ( rx_buf && rx_buf_len)
				{
					if ( rx_uid != current_uid ) // Not the expected response?
						continue;
					ok_buf = strstr(rx_buf, "OK");
					if ( ok_buf!=NULL ) // If "OK" found in the response, it's the last response to receive from the slave
					{
						break; // Complete and exit
					}
					tick_pass = HAL_GetTick() - tick_t0;
					tick_pass /= MILLISEC_TO_SEC;
					if ( tick_pass ) // at least one second has passed?
					{
						tick_t0 += MILLISEC_TO_SEC; // Update tick_t0
						if ( app_req->cmd_timeout_sec > tick_pass )
						{
							app_req->cmd_timeout_sec -= tick_pass;
						}
						else
							break; // Timeout
					} // if ( tick_pass )
				} // if ( rx_buf && rx_buf_len)
				else
				{
					ret = ERROR;
					break;
				}
			} // while ( true )
		} // if ( ret==SUCCESS )
	} // if ( ret==SUCCESS )
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	} // if ( ret==SUCCESS )
	else
	{
		M1_LOG_E(TAG, "Response not received\r\n");
	}

	return ret;
} // uint8_t ble_advertise(ctrl_cmd_t *app_req)




#ifdef M1_APP_BADBT_ENABLE

uint8_t ble_hid_init(ctrl_cmd_t *app_req, const char *device_name)
{
	uint8_t ret;
	uint8_t fail_step = 0;

	esp32_queue_reset();

	// Step 1: Init BLE in server mode
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_SER));
	vTaskDelay(200);

	// Step 2: Set GAP device name (shown after BLE connection)
	{
		char name_cmd[64];
		const char *gap_name = (device_name && device_name[0] != '\0') ? device_name : "M1-BadBT";
		snprintf(name_cmd, sizeof(name_cmd), "%s\"%s\"\r\n", ESP32C6_AT_REQ_BLE_NAME, gap_name);
		esp_at_send_wait_ok(app_req, name_cmd);
	}

	// Step 3: Register HID GATT service/appearance on ESP32
	ret = esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_HID_INIT, "1"));
	if (ret != SUCCESS) { fail_step = 3; goto cleanup; }

	// Step 4: Set security parameters for HID pairing
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_SEC_PARAM, ""));

	// Step 5: Set advertising parameters & raw data with keyboard appearance
	{
		char adv_cmd[196];
		const char *name = (device_name && device_name[0] != '\0') ? device_name : "M1-BadBT";
		uint8_t name_len = strlen(name);
		if (name_len > 20) name_len = 20; /* cap to fit 31-byte adv limit */

		/* AT+BLEADVPARAM: min=32(20ms),max=64(40ms),type=0(connectable),addr=0,ch=7,filter=0 */
		esp_at_send_wait_ok(app_req, "AT+BLEADVPARAM=32,64,0,0,7,0\r\n");
		vTaskDelay(50);

		/* Build raw advertising data hex string:
		 *   02 01 06               - Flags: LE General Discoverable + BR/EDR Not Supported
		 *   03 03 12 18            - Complete 16-bit UUID: 0x1812 (HID)
		 *   03 19 C1 03            - Appearance: 0x03C1 (Keyboard)
		 *   XX 09 <name bytes>     - Complete Local Name
		 */
		char hex[128];
		int pos = 0;
		/* Flags */
		pos += snprintf(hex + pos, sizeof(hex) - pos, "020106");
		/* UUID 0x1812 (little-endian) */
		pos += snprintf(hex + pos, sizeof(hex) - pos, "03031218");
		/* Appearance 0x03C1 = Keyboard (little-endian: C1 03) */
		pos += snprintf(hex + pos, sizeof(hex) - pos, "0319C103");
		/* Complete Local Name */
		pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X09", name_len + 1);
		for (uint8_t i = 0; i < name_len; i++)
			pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X", (uint8_t)name[i]);

		snprintf(adv_cmd, sizeof(adv_cmd),
		         "AT+BLEADVDATA=\"%s\"\r\n", hex);
		ret = esp_at_send_wait_ok(app_req, adv_cmd);
	}
	if (ret != SUCCESS) { fail_step = 5; goto cleanup; }

	// Step 6: Start advertising
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_ADV_START, ""));

	app_req->msg_type = CTRL_RESP;
	app_req->resp_event_status = SUCCESS;
	return SUCCESS;

cleanup:
	// Deinit BLE so stock BT/WiFi works after failure
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_ADV_STOP, ""));
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_NULL));
	return fail_step;
}


uint8_t ble_hid_deinit(ctrl_cmd_t *app_req)
{
	esp32_queue_reset();

	// Stop advertising
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_ADV_STOP, ""));

	// Reset HID registration state so next init re-registers GATT services
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_HID_INIT, "0"));

	// Deinit BLE entirely
	esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_MODE, ESP32C6_BLE_MODE_NULL));

	return SUCCESS;
}


uint8_t ble_hid_send_kb(ctrl_cmd_t *app_req, uint8_t modifier, uint8_t key1)
{
	char cmd[48];
	uint8_t ret;

	// Format: AT+BLEHIDKB=<mod>,<k1>,<k2>,<k3>,<k4>,<k5>,<k6>\r\n
	snprintf(cmd, sizeof(cmd), "%s%d,%d,0,0,0,0,0\r\n",
			ESP32C6_AT_REQ_BLE_HID_KB, modifier, key1);

	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	app_req->at_cmd = strdup(cmd);
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);

	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		// Brief wait for OK — don't block long for keystroke throughput
		char *rx_buf = NULL;
		char *resp_buf = NULL;
		int rx_buf_len = 0;
		uint32_t rx_uid;
		uint32_t tick_t0 = HAL_GetTick();

		ret = ERROR;
		while (true)
		{
			uint32_t tick_pass = (HAL_GetTick() - tick_t0) / MILLISEC_TO_SEC;
			if ( tick_pass >= 2 ) // 2-sec timeout
				break;
			esp_free_mem(&resp_buf);
			vTaskDelay(10); // Short delay for keystroke speed
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, 2);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf ) continue;
			if ( rx_uid != current_uid ) continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) ) continue;
			ret = SUCCESS;
			break;
		}
		esp_free_mem(&resp_buf);
	}

	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	return ret;
}


// Wait for BLE HID connection + security handshake to complete.
// Handles: +BLECONN: → +BLESECREQ: → AT+BLEENC → +BLEAUTHCMPL:
// Also handles +BLESECNTFYNUM: (numeric comparison) → AT+BLECONFREPLY
// Returns SUCCESS when connection + pairing are both done.
uint8_t ble_hid_wait_connect(ctrl_cmd_t *app_req, uint8_t timeout_sec)
{
	char *rx_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret = ERROR;
	uint8_t got_conn = 0;
	uint8_t got_auth = 0;
	uint32_t tick_t0 = HAL_GetTick();

	while (true)
	{
		uint32_t tick_pass = (HAL_GetTick() - tick_t0) / MILLISEC_TO_SEC;
		if ( tick_pass >= timeout_sec )
			break;

		esp_free_mem(&resp_buf);
		vTaskDelay(100);
		rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, timeout_sec);
		resp_buf = rx_buf;
		if ( !rx_buf || !rx_buf_len )
			continue;

		M1_LOG_I(TAG, "BLE evt: %s\r\n", rx_buf);

		// Check for connection event
		if ( strstr(rx_buf, "+BLECONN:") || strstr(rx_buf, "+BLEHIDCONN:") )
		{
			got_conn = 1;
			M1_LOG_I(TAG, "BLE HID connected\r\n");
			// Reset timeout — give security handshake time to complete
			tick_t0 = HAL_GetTick();
			if ( timeout_sec < 15 )
				timeout_sec = 15;
			continue;
		}

		// Security request from remote device — initiate encryption
		if ( strstr(rx_buf, "+BLESECREQ:") )
		{
			M1_LOG_I(TAG, "Security request — starting encryption\r\n");
			// AT+BLEENC=0,3  (conn_index=0, sec_act=3)
			esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_ENC, "0,3"));
			continue;
		}

		// Numeric comparison — auto-confirm (Just Works with NoInputNoOutput)
		if ( strstr(rx_buf, "+BLESECNTFYNUM:") )
		{
			M1_LOG_I(TAG, "Numeric comparison — auto-confirming\r\n");
			// AT+BLECONFREPLY=0,1  (conn_index=0, confirm=1)
			esp_at_send_wait_ok(app_req, CONCAT_CMD_PARAM(ESP32C6_AT_REQ_BLE_CONF_REPLY, "0,1"));
			continue;
		}

		// Authentication complete — pairing done
		if ( strstr(rx_buf, "+BLEAUTHCMPL:") )
		{
			got_auth = 1;
			M1_LOG_I(TAG, "BLE auth complete\r\n");
			break; // Connection + auth done
		}

		// Disconnection during handshake
		if ( strstr(rx_buf, "+BLEDISCONN:") )
		{
			M1_LOG_W(TAG, "Disconnected during pairing\r\n");
			got_conn = 0;
			break;
		}
	}

	esp_free_mem(&resp_buf);

	if ( got_conn && got_auth )
	{
		ret = SUCCESS;
	}
	else if ( got_conn && !got_auth )
	{
		// Connected but auth didn't complete — some devices don't trigger BLEAUTHCMPL
		// Give it a shot anyway, the connection may still be usable
		M1_LOG_W(TAG, "Connected but no auth event — proceeding anyway\r\n");
		ret = SUCCESS;
	}

	return ret;
}


/*============================================================================*/
/*
 * ble_hid_send_mouse() — Send a BLE HID mouse report.
 *
 * buttons: bitmask  bit0=Left, bit1=Right, bit2=Middle (0=release all)
 * dx, dy : relative X/Y movement (-127..127)
 * wheel  : scroll wheel (-127..127, positive=up)
 *
 * Sends AT+HIDMSSEND=<buttons>,<dx>,<dy>,<wheel>
 * NOTE: Requires ESP32-C6 AT firmware with AT+HIDMSSEND handler.
 */
/*============================================================================*/
uint8_t ble_hid_send_mouse(ctrl_cmd_t *app_req, uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel)
{
	char cmd[48];
	uint8_t ret;

	snprintf(cmd, sizeof(cmd), "%s%d,%d,%d,%d\r\n",
	         ESP32C6_AT_REQ_BLE_HID_MOUSE,
	         (int)buttons, (int)dx, (int)dy, (int)wheel);

	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	app_req->at_cmd  = strdup(cmd);
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);

	ret = spi_AT_app_send_command(app_req);
	if (ret == SUCCESS)
	{
		char *rx_buf  = NULL;
		char *resp_buf = NULL;
		int rx_buf_len = 0;
		uint32_t rx_uid;
		uint32_t tick_t0 = HAL_GetTick();

		ret = ERROR;
		while (true)
		{
			uint32_t elapsed = (HAL_GetTick() - tick_t0) / MILLISEC_TO_SEC;
			if (elapsed >= 2)
				break;
			esp_free_mem(&resp_buf);
			vTaskDelay(10);
			rx_buf    = spi_AT_app_get_response(&rx_buf_len, &rx_uid, 2);
			resp_buf  = rx_buf;
			rx_buf    = m1_resp_string_strip(rx_buf, CR_LF);
			if (!rx_buf) continue;
			if (rx_uid != current_uid) continue;
			if (strcmp(rx_buf, app_req->cmd_resp)) continue;
			ret = SUCCESS;
			break;
		}
		esp_free_mem(&resp_buf);
	}

	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	return ret;
}


/*============================================================================*/
/*
 * ble_hid_send_media() — Send a BLE HID Consumer Control (media key) report.
 *
 * usage: 16-bit USB HID Consumer Usage ID, e.g.:
 *   0x00B5 = Scan Next Track
 *   0x00B6 = Scan Previous Track
 *   0x00B7 = Stop
 *   0x00CD = Play/Pause
 *   0x00E2 = Mute
 *   0x00E9 = Volume Increment
 *   0x00EA = Volume Decrement
 *   0x0000 = Release (all keys up)
 *
 * Sends AT+HIDCSSEND=<usage>
 * NOTE: Requires ESP32-C6 AT firmware with AT+HIDCSSEND handler.
 */
/*============================================================================*/
uint8_t ble_hid_send_media(ctrl_cmd_t *app_req, uint16_t usage)
{
	char cmd[32];
	uint8_t ret;

	snprintf(cmd, sizeof(cmd), "%s%u\r\n",
	         ESP32C6_AT_REQ_BLE_HID_CONSUMER, (unsigned)usage);

	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	app_req->at_cmd  = strdup(cmd);
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);

	ret = spi_AT_app_send_command(app_req);
	if (ret == SUCCESS)
	{
		char *rx_buf  = NULL;
		char *resp_buf = NULL;
		int rx_buf_len = 0;
		uint32_t rx_uid;
		uint32_t tick_t0 = HAL_GetTick();

		ret = ERROR;
		while (true)
		{
			uint32_t elapsed = (HAL_GetTick() - tick_t0) / MILLISEC_TO_SEC;
			if (elapsed >= 2)
				break;
			esp_free_mem(&resp_buf);
			vTaskDelay(10);
			rx_buf    = spi_AT_app_get_response(&rx_buf_len, &rx_uid, 2);
			resp_buf  = rx_buf;
			rx_buf    = m1_resp_string_strip(rx_buf, CR_LF);
			if (!rx_buf) continue;
			if (rx_uid != current_uid) continue;
			if (strcmp(rx_buf, app_req->cmd_resp)) continue;
			ret = SUCCESS;
			break;
		}
		esp_free_mem(&resp_buf);
	}

	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	return ret;
}

#endif /* M1_APP_BADBT_ENABLE */


uint8_t esp_dev_reset(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *ok_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret, got_at_reset = 0;
	uint32_t tick_t0, tick_pass;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_RESET, ""));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_READY);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass ) // at least one second has passed?
			{
				tick_t0 += MILLISEC_TO_SEC; // Update tick_t0
				if ( app_req->cmd_timeout_sec > tick_pass )
				{
					app_req->cmd_timeout_sec -= tick_pass;
				}
				else
					break; // Timeout
			} // if ( tick_pass )
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf )
				continue;
			if ( rx_uid != current_uid ) // Not the expected response?
				continue;
			if ( !got_at_reset )
			{
				if ( strcmp(rx_buf, ESP32C6_AT_RESET) ) // Not the expected response?
					continue;
				got_at_reset = 1;
				continue;
			} // if ( !got_at_reset )
			else
			{
				if ( strcmp(rx_buf, app_req->cmd_resp) ) // Not the expected response?
					continue;
			}
			ret = SUCCESS;
			break;
		} // while ( true )
	} // if ( ret==SUCCESS )
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	} // if ( ret==SUCCESS )
	else
	{
		M1_LOG_E(TAG, "Response not received\r\n");
	}

	return ret;
} // uint8_t esp_dev_reset(ctrl_cmd_t *app_req)


#ifdef M1_APP_WIFI_CONNECT_ENABLE

uint8_t wifi_connect_ap(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *ok_buf = NULL;
	char *resp_buf = NULL;
	char at_cmd_buf[128];
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;
	uint8_t got_ip = 0;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();

	/* Step 1: Set station mode */
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_WIFI_MODE, ESP32C6_WIFI_MODE_STA));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass )
			{
				tick_t0 += MILLISEC_TO_SEC;
				if ( app_req->cmd_timeout_sec > tick_pass )
					app_req->cmd_timeout_sec -= tick_pass;
				else
					break;
			}
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf )
				continue;
			if ( rx_uid != current_uid )
				continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) )
				continue;
			ret = SUCCESS;
			break;
		}

		/* Step 2: Send connect command AT+CWJAP="ssid","password" */
		if ( ret==SUCCESS )
		{
			esp_free_mem(&app_req->at_cmd);
			esp_free_mem(&app_req->cmd_resp);

			snprintf(at_cmd_buf, sizeof(at_cmd_buf), "%s\"%s\",\"%s\"%s",
					ESP32C6_AT_REQ_CONNECT_AP,
					(char *)app_req->u.wifi_ap_config.ssid,
					(char *)app_req->u.wifi_ap_config.pwd,
					ESP32C6_AT_REQ_CRLF);
			app_req->at_cmd = strdup(at_cmd_buf);
			app_req->cmd_len = strlen(app_req->at_cmd);
			app_req->cmd_resp = NULL;

			/* Use longer timeout for connect */
			if ( app_req->cmd_timeout_sec < DEFAULT_CTRL_RESP_CONNECT_AP_TIMEOUT )
				app_req->cmd_timeout_sec = DEFAULT_CTRL_RESP_CONNECT_AP_TIMEOUT;

			ret = spi_AT_app_send_command(app_req);
			got_ip = 0;
			app_req->resp_event_status = FAILURE;

			while ( ret==SUCCESS )
			{
				esp_free_mem(&resp_buf);
				rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
				resp_buf = rx_buf;
				if ( rx_buf && rx_buf_len )
				{
					if ( rx_uid != current_uid )
						continue;

					/* Check for "WIFI GOT IP" */
					if ( strstr(rx_buf, ESP32C6_AT_RES_WIFI_GOT_IP) )
						got_ip = 1;

					/* Check for error: +CWJAP:<error_code> */
					ok_buf = strstr(rx_buf, ESP32C6_AT_RES_CONNECT_AP_KEY);
					if ( ok_buf )
					{
						/* Parse error code */
						app_req->resp_event_status = strtol(ok_buf + strlen(ESP32C6_AT_RES_CONNECT_AP_KEY), NULL, 10);
					}

					/* Check for final "OK" or "FAIL" */
					ok_buf = strstr(rx_buf, ESP32C6_AT_RES_OK);
					if ( ok_buf )
					{
						if ( got_ip )
							app_req->resp_event_status = SUCCESS;
						break;
					}
					ok_buf = strstr(rx_buf, ESP32C6_AT_RES_FAIL);
					if ( ok_buf )
					{
						ret = ERROR;
						break;
					}

					tick_pass = HAL_GetTick() - tick_t0;
					tick_pass /= MILLISEC_TO_SEC;
					if ( tick_pass )
					{
						tick_t0 += MILLISEC_TO_SEC;
						if ( app_req->cmd_timeout_sec > tick_pass )
							app_req->cmd_timeout_sec -= tick_pass;
						else
							break;
					}
				}
				else
				{
					ret = ERROR;
				}
			} // while ( ret==SUCCESS )
		} // if ( ret==SUCCESS ) step 2
	} // if ( ret==SUCCESS ) step 1

	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS && app_req->resp_event_status==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
	}
	else
	{
		ret = ERROR;
		M1_LOG_E(TAG, "WiFi connect failed (status %ld)\r\n", app_req->resp_event_status);
	}

	return ret;
} // uint8_t wifi_connect_ap(ctrl_cmd_t *app_req)



uint8_t wifi_disconnect_ap(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *resp_buf = NULL;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();
	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_DISCONNECT_AP, ""));
	app_req->cmd_resp = strdup(ESP32C6_AT_RES_OK);
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass )
			{
				tick_t0 += MILLISEC_TO_SEC;
				if ( app_req->cmd_timeout_sec > tick_pass )
					app_req->cmd_timeout_sec -= tick_pass;
				else
					break;
			}
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			rx_buf = m1_resp_string_strip(rx_buf, CR_LF);
			if ( !rx_buf )
				continue;
			if ( rx_uid != current_uid )
				continue;
			if ( strcmp(rx_buf, app_req->cmd_resp) )
				continue;
			ret = SUCCESS;
			break;
		}
	}
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	}
	else
	{
		M1_LOG_E(TAG, "WiFi disconnect failed\r\n");
	}

	return ret;
} // uint8_t wifi_disconnect_ap(ctrl_cmd_t *app_req)



uint8_t wifi_get_ip(ctrl_cmd_t *app_req)
{
	char *rx_buf = NULL;
	char *resp_buf = NULL;
	char *index, *end_index;
	int rx_buf_len = 0;
	uint32_t rx_uid;
	uint8_t ret;
	uint32_t tick_t0, tick_pass;
	size_t cp_len;

	tick_t0 = HAL_GetTick();
	esp32_queue_reset();

	/* Clear output fields */
	memset(app_req->u.wifi_ap_config.status, 0, STATUS_LENGTH);
	memset(app_req->u.wifi_ap_config.out_mac, 0, MAX_MAC_STR_SIZE);

	app_req->at_cmd = strdup(CONCAT_CMD_PARAM(ESP32C6_AT_REQ_GET_IP, ""));
	app_req->cmd_resp = NULL;
	app_req->cmd_len = strlen(app_req->at_cmd);
	ret = spi_AT_app_send_command(app_req);
	if ( ret==SUCCESS )
	{
		ret = ERROR;
		while (true)
		{
			tick_pass = HAL_GetTick() - tick_t0;
			tick_pass /= MILLISEC_TO_SEC;
			if ( tick_pass )
			{
				tick_t0 += MILLISEC_TO_SEC;
				if ( app_req->cmd_timeout_sec > tick_pass )
					app_req->cmd_timeout_sec -= tick_pass;
				else
					break;
			}
			esp_free_mem(&resp_buf);
			rx_buf = spi_AT_app_get_response(&rx_buf_len, &rx_uid, app_req->cmd_timeout_sec);
			resp_buf = rx_buf;
			if ( !rx_buf || !rx_buf_len )
				continue;
			if ( rx_uid != current_uid )
				continue;

			/* Parse +CIFSR:STAIP,"x.x.x.x" */
			index = strstr(rx_buf, ESP32C6_AT_RES_STAIP_KEY);
			if ( index )
			{
				index += strlen(ESP32C6_AT_RES_STAIP_KEY);
				if ( *index == '\"' ) index++;
				end_index = strstr(index, "\"");
				if ( end_index )
				{
					cp_len = end_index - index;
					if ( cp_len >= STATUS_LENGTH ) cp_len = STATUS_LENGTH - 1;
					strncpy(app_req->u.wifi_ap_config.status, index, cp_len);
					app_req->u.wifi_ap_config.status[cp_len] = '\0';
				}
			}

			/* Parse +CIFSR:STAMAC,"xx:xx:xx:xx:xx:xx" */
			index = strstr(rx_buf, ESP32C6_AT_RES_STAMAC_KEY);
			if ( index )
			{
				index += strlen(ESP32C6_AT_RES_STAMAC_KEY);
				if ( *index == '\"' ) index++;
				end_index = strstr(index, "\"");
				if ( end_index )
				{
					cp_len = end_index - index;
					if ( cp_len >= MAX_MAC_STR_SIZE ) cp_len = MAX_MAC_STR_SIZE - 1;
					strncpy(app_req->u.wifi_ap_config.out_mac, index, cp_len);
					app_req->u.wifi_ap_config.out_mac[cp_len] = '\0';
				}
			}

			/* Check for final "OK" */
			if ( strstr(rx_buf, ESP32C6_AT_RES_OK) )
			{
				ret = SUCCESS;
				break;
			}
		}
	}
	esp_free_mem(&resp_buf);
	esp_free_mem(&app_req->at_cmd);
	esp_free_mem(&app_req->cmd_resp);
	if ( ret==SUCCESS )
	{
		app_req->msg_type = CTRL_RESP;
		app_req->resp_event_status = SUCCESS;
	}
	else
	{
		M1_LOG_E(TAG, "WiFi get IP failed\r\n");
	}

	return ret;
} // uint8_t wifi_get_ip(ctrl_cmd_t *app_req)

#endif /* M1_APP_WIFI_CONNECT_ENABLE */
