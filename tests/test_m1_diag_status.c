#include "unity.h"
#include "m1_diag_status.h"
#include <stdbool.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void assert_status(M1DiagBootStage stage, M1DiagFaultKind fault,
                          const char *expected)
{
    char text[32] = {0};
    m1_diag_boot_status_text(stage, fault, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    TEST_ASSERT_LESS_OR_EQUAL(21u, strlen(text));
}

void test_boot_stage_status_is_short_and_specific(void)
{
    assert_status(M1_DIAG_BOOT_MAIN, M1_DIAG_FAULT_NONE, "LAST: MAIN");
    assert_status(M1_DIAG_BOOT_HAL, M1_DIAG_FAULT_NONE, "LAST: HAL");
    assert_status(M1_DIAG_BOOT_CLOCK, M1_DIAG_FAULT_NONE, "LAST: CLOCK");
    assert_status(M1_DIAG_BOOT_MPU, M1_DIAG_FAULT_NONE, "LAST: MPU");
    assert_status(M1_DIAG_BOOT_GPIO, M1_DIAG_FAULT_NONE, "LAST: GPIO");
    assert_status(M1_DIAG_BOOT_IWDG, M1_DIAG_FAULT_NONE, "LAST: IWDG");
    assert_status(M1_DIAG_BOOT_SPI1, M1_DIAG_FAULT_NONE, "LAST: SPI1");
    assert_status(M1_DIAG_BOOT_ICACHE, M1_DIAG_FAULT_NONE, "LAST: ICACHE");
    assert_status(M1_DIAG_BOOT_SPI2, M1_DIAG_FAULT_NONE, "LAST: SPI2");
    assert_status(M1_DIAG_BOOT_I2C, M1_DIAG_FAULT_NONE, "LAST: I2C");
    assert_status(M1_DIAG_BOOT_RTC, M1_DIAG_FAULT_NONE, "LAST: RTC");
    assert_status(M1_DIAG_BOOT_CRC, M1_DIAG_FAULT_NONE, "LAST: CRC");
    assert_status(M1_DIAG_BOOT_KERNEL, M1_DIAG_FAULT_NONE, "LAST: KERNEL");
    assert_status(M1_DIAG_BOOT_SYSINIT, M1_DIAG_FAULT_NONE, "LAST: SYSINIT");
    assert_status(M1_DIAG_BOOT_IOHAL, M1_DIAG_FAULT_NONE, "LAST: IOHAL");
    assert_status(M1_DIAG_BOOT_TASKS, M1_DIAG_FAULT_NONE, "LAST: TASKS");
    assert_status(M1_DIAG_BOOT_SETTINGS, M1_DIAG_FAULT_NONE, "LAST: SETTINGS");
    assert_status(M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_NONE, "LAST: MENU");
    assert_status(M1_DIAG_BOOT_SDMMC, M1_DIAG_FAULT_NONE, "LAST: SDMMC");
    assert_status(M1_DIAG_BOOT_LOGDB, M1_DIAG_FAULT_NONE, "LAST: LOGDB");
    assert_status(M1_DIAG_BOOT_DMA_RX, M1_DIAG_FAULT_NONE, "LAST: DMARX");
    assert_status(M1_DIAG_BOOT_LOG_RTOS, M1_DIAG_FAULT_NONE, "LAST: LOGRTOS");
    assert_status(M1_DIAG_BOOT_LOG_MUTEX, M1_DIAG_FAULT_NONE, "LAST: LOGMUTEX");
    assert_status(M1_DIAG_BOOT_LOG_QUEUE, M1_DIAG_FAULT_NONE, "LAST: LOGQSTATIC");
    assert_status(M1_DIAG_BOOT_LOG_DONE, M1_DIAG_FAULT_NONE, "LAST: LOGQDONE");
    assert_status(M1_DIAG_BOOT_DIAG, M1_DIAG_FAULT_NONE, "LAST: DIAG");
    assert_status(M1_DIAG_BOOT_DIAG_RCC, M1_DIAG_FAULT_NONE, "LAST: DRCC");
    assert_status(M1_DIAG_BOOT_DIAG_SCB, M1_DIAG_FAULT_NONE, "LAST: DSCB");
    assert_status(M1_DIAG_BOOT_DIAG_MAGIC, M1_DIAG_FAULT_NONE, "LAST: DMAGIC");
    assert_status(M1_DIAG_BOOT_DIAG_STAGE, M1_DIAG_FAULT_NONE, "LAST: DSTAGE");
    assert_status(M1_DIAG_BOOT_DIAG_FAULT, M1_DIAG_FAULT_NONE, "LAST: DFAULT");
    assert_status(M1_DIAG_BOOT_DIAG_ORIGIN, M1_DIAG_FAULT_NONE, "LAST: DORIGIN");
    assert_status(M1_DIAG_BOOT_DIAG_SNAPSHOT, M1_DIAG_FAULT_NONE, "LAST: DSNAP");
    assert_status(M1_DIAG_BOOT_DIAG_REGS, M1_DIAG_FAULT_NONE, "LAST: DREGS");
    assert_status(M1_DIAG_BOOT_DIAG_CLEAR, M1_DIAG_FAULT_NONE, "LAST: DCLEAR");
    assert_status(M1_DIAG_BOOT_DIAG_TEXT, M1_DIAG_FAULT_NONE, "LAST: DTEXT");
    assert_status(M1_DIAG_BOOT_DIAG_LCD, M1_DIAG_FAULT_NONE, "LAST: DLCD");
    assert_status(M1_DIAG_BOOT_DIAG_DONE, M1_DIAG_FAULT_NONE, "LAST: DIAGDONE");
    assert_status(M1_DIAG_BOOT_HOME_START, M1_DIAG_FAULT_NONE, "LAST: HSTART");
    assert_status(M1_DIAG_BOOT_HOME_RTC, M1_DIAG_FAULT_NONE, "LAST: HRTC");
    assert_status(M1_DIAG_BOOT_HOME_PAGE, M1_DIAG_FAULT_NONE, "LAST: HPAGE");
    assert_status(M1_DIAG_BOOT_HOME_TIME, M1_DIAG_FAULT_NONE, "LAST: HTIME");
    assert_status(M1_DIAG_BOOT_HOME_LOGO, M1_DIAG_FAULT_NONE, "LAST: HLOGO");
    assert_status(M1_DIAG_BOOT_HOME_BATT, M1_DIAG_FAULT_NONE, "LAST: HBATT");
    assert_status(M1_DIAG_BOOT_HOME_ICONS, M1_DIAG_FAULT_NONE, "LAST: HICONS");
    assert_status(M1_DIAG_BOOT_HOME_TEXT, M1_DIAG_FAULT_NONE, "LAST: HTEXT");
    assert_status(M1_DIAG_BOOT_HOME_SEND, M1_DIAG_FAULT_NONE, "LAST: HSEND");
    assert_status(M1_DIAG_BOOT_HOME_BACKLIGHT, M1_DIAG_FAULT_NONE, "LAST: HBL");
    assert_status(M1_DIAG_BOOT_NEXT_SEND, M1_DIAG_FAULT_NONE, "LAST: NSEND");
    assert_status(M1_DIAG_BOOT_NEXT_REFRESH, M1_DIAG_FAULT_NONE, "LAST: NREFRESH");
    assert_status(M1_DIAG_BOOT_NEXT_RPC, M1_DIAG_FAULT_NONE, "LAST: NRPC");
    assert_status(M1_DIAG_BOOT_NEXT_DONE, M1_DIAG_FAULT_NONE, "LAST: NDONE");
    assert_status(M1_DIAG_BOOT_LCD_SPI_ENTER, M1_DIAG_FAULT_NONE, "LAST: SPIENTER");
    assert_status(M1_DIAG_BOOT_LCD_SPI_EXIT, M1_DIAG_FAULT_NONE, "LAST: SPIEXIT");
    assert_status(M1_DIAG_BOOT_BL_START, M1_DIAG_FAULT_NONE, "LAST: BLSTART");
    assert_status(M1_DIAG_BOOT_BL_PWM, M1_DIAG_FAULT_NONE, "LAST: BLPWM");
    assert_status(M1_DIAG_BOOT_BL_READ, M1_DIAG_FAULT_NONE, "LAST: BLREAD");
    assert_status(M1_DIAG_BOOT_BL_CONFIG, M1_DIAG_FAULT_NONE, "LAST: BLCFG");
    assert_status(M1_DIAG_BOOT_BL_UPDATE, M1_DIAG_FAULT_NONE, "LAST: BLUPDATE");
    assert_status(M1_DIAG_BOOT_I2C_WAIT, M1_DIAG_FAULT_NONE, "LAST: I2CWAIT");
    assert_status(M1_DIAG_BOOT_I2C_XFER, M1_DIAG_FAULT_NONE, "LAST: I2CXFER");
    assert_status(M1_DIAG_BOOT_I2C_DONE, M1_DIAG_FAULT_NONE, "LAST: I2CDONE");
    assert_status(M1_DIAG_BOOT_I2C_RELEASED, M1_DIAG_FAULT_NONE, "LAST: I2CREL");
    assert_status(M1_DIAG_BOOT_LP_WRITE_DONE, M1_DIAG_FAULT_NONE, "LAST: LPWDONE");
    assert_status(M1_DIAG_BOOT_WDT_ENTRY, M1_DIAG_FAULT_NONE, "LAST: WDTENTER");
    assert_status(M1_DIAG_BOOT_WDT_BKUP, M1_DIAG_FAULT_NONE, "LAST: WDTBKUP");
    assert_status(M1_DIAG_BOOT_WDT_WRITE, M1_DIAG_FAULT_NONE, "LAST: WDTWRITE");
    assert_status(M1_DIAG_BOOT_WDT_ARM, M1_DIAG_FAULT_NONE, "LAST: WDTARM");
    assert_status(M1_DIAG_BOOT_WDT_TASK, M1_DIAG_FAULT_NONE, "LAST: WDTTASK");
    assert_status(M1_DIAG_BOOT_SDMMC_IRQ, M1_DIAG_FAULT_NONE, "LAST: SDIRQ");
    assert_status(M1_DIAG_BOOT_LOG_IRQ, M1_DIAG_FAULT_NONE, "LAST: LOGIRQ");
}

void test_fault_status_takes_precedence_over_stage(void)
{
    assert_status(M1_DIAG_BOOT_TASKS, M1_DIAG_FAULT_HARD, "FAULT: HARD");
    assert_status(M1_DIAG_BOOT_CONFIG, M1_DIAG_FAULT_ERROR, "FAULT: ERROR");
    assert_status(M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_MEMMANAGE, "FAULT: MEM");
    assert_status(M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_BUS, "FAULT: BUS");
    assert_status(M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_USAGE, "FAULT: USAGE");
    assert_status(M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_NMI, "FAULT: NMI");
    assert_status(M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_ASSERT, "FAULT: ASSERT");
    assert_status(M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_WDT_TASK, "FAULT: WDT TASK");
}

void test_invalid_values_are_visible_and_bounded(void)
{
    assert_status((M1DiagBootStage)99, M1_DIAG_FAULT_NONE, "LAST: UNKNOWN");
    assert_status(M1_DIAG_BOOT_MENU, (M1DiagFaultKind)99, "FAULT: UNKNOWN");
}

void test_persistent_diag_block_prevents_marker_coalescing(void)
{
    TEST_ASSERT_TRUE(_Generic(&g_m1_diag,
                              volatile M1DiagBlock *: 1,
                              default: 0));
}

void test_boot_report_is_visible_only_for_abnormal_boots(void)
{
    TEST_ASSERT_FALSE(m1_diag_boot_report_visible(
        true, M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_NONE, false));
    TEST_ASSERT_FALSE(m1_diag_boot_report_visible(
        false, M1_DIAG_BOOT_EARLY, M1_DIAG_FAULT_NONE, false));
    TEST_ASSERT_TRUE(m1_diag_boot_report_visible(
        true, M1_DIAG_BOOT_NEXT_RPC, M1_DIAG_FAULT_NONE, false));
    TEST_ASSERT_TRUE(m1_diag_boot_report_visible(
        true, M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_NONE, true));
    TEST_ASSERT_TRUE(m1_diag_boot_report_visible(
        true, M1_DIAG_BOOT_MENU, M1_DIAG_FAULT_HARD, false));
}

void test_exception_frame_yields_fault_pc(void)
{
    uint32_t frame[8] = {1, 2, 3, 4, 5, 6, 0x08012345, 7};
    TEST_ASSERT_EQUAL_HEX32(0x08012345, m1_diag_stacked_pc(frame));
    TEST_ASSERT_EQUAL_HEX32(0, m1_diag_stacked_pc(NULL));
}

void test_reset_origin_names_are_specific(void)
{
    TEST_ASSERT_EQUAL_STRING("RPC", m1_diag_reset_origin_text(M1_DIAG_RESET_RPC));
    TEST_ASSERT_EQUAL_STRING("POWER", m1_diag_reset_origin_text(M1_DIAG_RESET_POWER));
    TEST_ASSERT_EQUAL_STRING("BANK", m1_diag_reset_origin_text(M1_DIAG_RESET_BANK));
    TEST_ASSERT_EQUAL_STRING("DFU", m1_diag_reset_origin_text(M1_DIAG_RESET_DFU));
    TEST_ASSERT_EQUAL_STRING("FLASHOB", m1_diag_reset_origin_text(M1_DIAG_RESET_FLASH_OB));
    TEST_ASSERT_EQUAL_STRING("NOTIFY", m1_diag_reset_origin_text(M1_DIAG_DETAIL_RPC_NOTIFY));
    TEST_ASSERT_EQUAL_STRING("STATUS", m1_diag_reset_origin_text(M1_DIAG_DETAIL_RPC_STATUS));
    TEST_ASSERT_EQUAL_STRING("QMON", m1_diag_reset_origin_text(M1_DIAG_DETAIL_RPC_QMON));
    TEST_ASSERT_EQUAL_STRING("STREAM", m1_diag_reset_origin_text(M1_DIAG_DETAIL_RPC_STREAM));
    TEST_ASSERT_EQUAL_STRING("EPOCH", m1_diag_reset_origin_text(M1_DIAG_DETAIL_RPC_EPOCH));
}

void test_wdt_status_includes_task_state_and_wake_delta(void)
{
    char text[32] = {0};
    m1_diag_wdt_status_text(1u, 0u, 2u, -19900, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("W1 R0 BLK D-19900", text);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_stage_status_is_short_and_specific);
    RUN_TEST(test_fault_status_takes_precedence_over_stage);
    RUN_TEST(test_invalid_values_are_visible_and_bounded);
    RUN_TEST(test_persistent_diag_block_prevents_marker_coalescing);
    RUN_TEST(test_boot_report_is_visible_only_for_abnormal_boots);
    RUN_TEST(test_exception_frame_yields_fault_pc);
    RUN_TEST(test_reset_origin_names_are_specific);
    RUN_TEST(test_wdt_status_includes_task_state_and_wake_delta);
    return UNITY_END();
}
