/*
 * m1app.h -- M1 External App SDK (v2)
 *
 * Include this header in your .m1app project. It declares all firmware
 * functions that apps can call at runtime, resolved via the ELF loader's
 * symbol hash table.
 *
 * API Version: 2
 * Target: STM32H573 (Cortex-M33, Thumb-2)
 * Display: 128x64 monochrome (u8g2)
 *
 * Build your app with the provided Makefile and m1app.ld linker script.
 * Output is a relocatable ELF (.m1app) placed on SD card under 0:/apps/
 */

#ifndef M1APP_H_
#define M1APP_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- API version ---- */
#define M1_APP_API_VERSION  2

/* ---- Display constants ---- */
#define M1APP_SCREEN_W   128
#define M1APP_SCREEN_H   64

/* ==================================================================
 *  App Manifest
 * ================================================================== */

#define M1_APP_MANIFEST_MAGIC  0x4D314150  /* "M1AP" */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t api_version;
    uint16_t stack_size;     /* FreeRTOS task stack in 32-bit words */
    char     name[32];
} m1_app_manifest_t;

#define M1_APP_MANIFEST(_name, _stack_words) \
    __attribute__((section(".m1meta"), used)) \
    static const m1_app_manifest_t _m1_manifest = { \
        .magic       = M1_APP_MANIFEST_MAGIC, \
        .api_version = M1_APP_API_VERSION, \
        .stack_size  = (_stack_words), \
        .name        = _name \
    }


/* ==================================================================
 *  Button Input
 * ================================================================== */

typedef enum {
    M1APP_BTN_NONE = 0,
    M1APP_BTN_UP,
    M1APP_BTN_DOWN,
    M1APP_BTN_LEFT,
    M1APP_BTN_RIGHT,
    M1APP_BTN_OK,
    M1APP_BTN_BACK
} m1app_button_t;

/* Poll for a button press. Blocks up to timeout_ms. */
extern m1app_button_t game_poll_button(uint32_t timeout_ms);

/* Seed the RNG from hardware tick. Call once at app startup. */
extern void game_rand_seed(void);

/* Random number in [min, max] inclusive. */
extern int game_rand_range(int min, int max);


/* ==================================================================
 *  Display — u8g2 core
 * ================================================================== */

typedef struct u8g2_struct u8g2_t;
typedef uint16_t u8g2_uint_t;

extern u8g2_t   *m1app_get_u8g2(void);
extern void      m1app_display_begin(void);
extern uint8_t   m1app_display_flush(void);
extern void      m1_lcd_cleardisplay(void);

/* u8g2 drawing */
extern void      u8g2_FirstPage(u8g2_t *u8g2);
extern uint16_t  u8g2_DrawStr(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, const char *str);
extern void      u8g2_DrawBox(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h);
extern void      u8g2_DrawFrame(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h);
extern void      u8g2_DrawRBox(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, u8g2_uint_t r);
extern void      u8g2_DrawRFrame(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, u8g2_uint_t r);
extern void      u8g2_DrawPixel(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y);
extern void      u8g2_DrawLine(u8g2_t *u8g2, u8g2_uint_t x0, u8g2_uint_t y0, u8g2_uint_t x1, u8g2_uint_t y1);
extern void      u8g2_DrawCircle(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t rad, uint8_t opt);
extern void      u8g2_DrawDisc(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t rad, uint8_t opt);
extern void      u8g2_DrawHLine(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w);
extern void      u8g2_DrawVLine(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t h);
extern void      u8g2_DrawTriangle(u8g2_t *u8g2, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
extern void      u8g2_DrawXBM(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, u8g2_uint_t w, u8g2_uint_t h, const uint8_t *bitmap);
extern void      u8g2_SetDrawColor(u8g2_t *u8g2, uint8_t color);
extern void      u8g2_SetFont(u8g2_t *u8g2, const uint8_t *font);
extern void      u8g2_SetFontDirection(u8g2_t *u8g2, uint8_t dir);
extern void      u8g2_SetClipWindow(u8g2_t *u8g2, u8g2_uint_t x0, u8g2_uint_t y0, u8g2_uint_t x1, u8g2_uint_t y1);
extern void      u8g2_SetMaxClipWindow(u8g2_t *u8g2);

/* u8g2 font metrics */
extern u8g2_uint_t u8g2_GetStrWidth(u8g2_t *u8g2, const char *s);
extern int8_t    u8g2_GetAscent(u8g2_t *u8g2);   /* wrapper — macro in firmware */
extern int8_t    u8g2_GetDescent(u8g2_t *u8g2);   /* wrapper — macro in firmware */

/* u8g2 circle draw options */
#define U8G2_DRAW_UPPER_RIGHT  0x01
#define U8G2_DRAW_UPPER_LEFT   0x02
#define U8G2_DRAW_LOWER_LEFT   0x04
#define U8G2_DRAW_LOWER_RIGHT  0x08
#define U8G2_DRAW_ALL          0x0F


/* ==================================================================
 *  Display — M1 UI Helpers
 * ================================================================== */

typedef enum {
    TEXT_ALIGN_LEFT = 0,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
} m1app_text_align_t;

/* Show a modal message box. Returns button index (0=left, 1=right). */
extern uint8_t m1_message_box(u8g2_t *u8g2, const char *title1, const char *title2,
                              const char *title3, const char *buttons);

/* Draw bottom navigation bar with optional bitmaps and labels */
extern void m1_draw_bottom_bar(u8g2_t *u8g2, const uint8_t *lbitmap, const char *ltext,
                               const char *rtext, const uint8_t *rbitmap);

/* Draw a bitmap icon at (x,y) with color */
extern void m1_draw_icon(uint8_t color, u8g2_uint_t x, u8g2_uint_t y,
                         u8g2_uint_t w, u8g2_uint_t h, const uint8_t *bitmap);

/* Draw aligned text within max_width */
extern void m1_draw_text(u8g2_t *u8g2, int x, int y, int max_width,
                         const char *text, m1app_text_align_t align);

/* Draw multi-line text box */
extern void m1_draw_text_box(u8g2_t *u8g2, int x, int y, int max_width,
                             int line_height, const char *text, m1app_text_align_t align);

/* Info box (3-line area at bottom of screen) */
extern void m1_info_box_display_init(uint8_t high_box);
extern void m1_info_box_display_clear(void);
extern void m1_info_box_display_draw(uint8_t box_row, const uint8_t *ptext);


/* ==================================================================
 *  u8g2 Fonts
 * ================================================================== */

extern const uint8_t u8g2_font_4x6_tr[];
extern const uint8_t u8g2_font_5x8_tr[];
extern const uint8_t u8g2_font_6x10_tr[];
extern const uint8_t u8g2_font_10x20_mr[];
extern const uint8_t u8g2_font_helvB08_tr[];
extern const uint8_t u8g2_font_helvB08_tf[];
extern const uint8_t u8g2_font_finderskeepers_tf[];
extern const uint8_t u8g2_font_resoledmedium_tr[];
extern const uint8_t u8g2_font_NokiaSmallPlain_tf[];
extern const uint8_t u8g2_font_squeezed_b7_tr[];
extern const uint8_t u8g2_font_spleen5x8_mf[];
extern const uint8_t u8g2_font_spleen8x16_mf[];
extern const uint8_t u8g2_font_nine_by_five_nbp_tf[];
extern const uint8_t u8g2_font_courB08_tf[];
extern const uint8_t u8g2_font_Terminal_tr[];
extern const uint8_t u8g2_font_lubB08_tf[];
extern const uint8_t u8g2_font_profont17_tr[];
extern const uint8_t u8g2_font_VCR_OSD_tu[];
extern const uint8_t u8g2_font_Pixellari_tu[];
extern const uint8_t u8g2_font_pcsenior_8f[];


/* ==================================================================
 *  Timing
 * ================================================================== */

extern void     m1app_delay(uint32_t ms);      /* yields to RTOS */
extern uint32_t m1app_get_tick(void);           /* ms since boot */
extern void     vTaskDelay(uint32_t ticks);     /* RTOS ticks (1ms) */
extern uint32_t HAL_GetTick(void);
extern void     m1_hard_delay(uint32_t x);      /* busy-wait (use sparingly) */


/* ==================================================================
 *  Memory
 * ================================================================== */

extern void *m1app_malloc(size_t size);
extern void  m1app_free(void *ptr);
extern void *pvPortMalloc(size_t size);
extern void  vPortFree(void *ptr);


/* ==================================================================
 *  Audio / Buzzer
 * ================================================================== */

extern void m1_buzzer_notification(void);      /* standard notification beep */
extern void m1_buzzer_notification2(void);     /* alternate notification beep */
extern void m1_buzzer_set(uint16_t frequency, uint16_t duration_ms);


/* ==================================================================
 *  LED (RGB notification LED — LP5814 driver)
 * ================================================================== */

/* Individual color channels (pwm: 0-255) */
extern void lp5814_led_on_Red(uint8_t pwm);
extern void lp5814_led_on_Green(uint8_t pwm);
extern void lp5814_led_on_Blue(uint8_t pwm);
extern void lp5814_led_on_rgb(uint8_t led_rgb, uint8_t value);
extern void lp5814_all_off_RGB(void);

/* Port-level control */
extern void lp5814_led_on(uint8_t port, uint8_t value);
extern void lp5814_led_off(uint8_t port);

/* Display backlight (brightness: 0=off, 255=max) */
extern void lp5814_backlight_on(uint8_t brightness);

/* Blink pattern (r_g_b: bitmask 0x01=R 0x02=G 0x04=B) */
extern void lp5814_fastblink_on_R_G_B(uint8_t r_g_b, uint8_t pwm_rgb, uint16_t on_off_ms);

/* LED port IDs */
#define M1APP_LED_B  0
#define M1APP_LED_G  1
#define M1APP_LED_R  2
#define M1APP_LED_W  3

/* LED blink color masks */
#define M1APP_LED_BLINK_RED    0x01
#define M1APP_LED_BLINK_GREEN  0x02
#define M1APP_LED_BLINK_BLUE   0x04
#define M1APP_LED_BLINK_RGB    0x07


/* ==================================================================
 *  GPIO
 * ================================================================== */

/* GPIO port base type (opaque — use M1APP_GPIOx defines below) */
typedef struct { uint32_t _opaque[256]; } M1_GPIO_TypeDef;

/* External power rails (set_mode: 0=off, 1=on) */
extern void ext_power_5V_set(uint8_t set_mode);
extern void ext_power_3V_set(uint8_t set_mode);

/* HAL GPIO — read/write/toggle any pin */
extern int  HAL_GPIO_ReadPin(M1_GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
extern void HAL_GPIO_WritePin(M1_GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, int PinState);
extern void HAL_GPIO_TogglePin(M1_GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

#define M1APP_GPIO_PIN_SET    1
#define M1APP_GPIO_PIN_RESET  0

/* App-friendly external-header GPIO (recommended over raw HAL_GPIO_*).
 * Pins are 0-based over the user signal pins only — the power-rail control
 * pins are hidden. app_id 0..count-1; the SWD debug pins are restored on
 * release(). mode: 0 = input, 1 = push-pull output (driven low on entry). */
#define M1APP_GPIO_MODE_INPUT   0
#define M1APP_GPIO_MODE_OUTPUT  1
extern uint8_t     m1_gpio_ext_app_count(void);            /* number of header pins (12) */
extern const char *m1_gpio_ext_app_name(uint8_t app_id);  /* e.g. "Pin PE2" */
extern void        m1_gpio_ext_app_mode(uint8_t app_id, uint8_t mode);
extern void        m1_gpio_ext_app_write(uint8_t app_id, uint8_t on);
extern uint8_t     m1_gpio_ext_app_read(uint8_t app_id);
extern void        m1_gpio_ext_app_release(void);          /* park pins safe, restore SWD */

/* Generic 1-Wire bit-bang primitive (microsecond-timed + IRQ-masked, in firmware).
 * Configure one header pin (0-based id) as an open-drain 1-Wire line; an external
 * 4.7k pull-up to +3.3V is required. Build device logic (DS18B20, iButton) on top. */
extern void        m1_ow_init(uint8_t app_id);
extern int         m1_ow_reset(void);          /* 1 = a device responded */
extern void        m1_ow_write_byte(uint8_t v);
extern uint8_t     m1_ow_read_byte(void);
extern void        m1_ow_deinit(void);


/* ==================================================================
 *  I2C
 * ================================================================== */

typedef enum {
    M1APP_I2C_READ_REG = 0,
    M1APP_I2C_WRITE_REG,
    M1APP_I2C_READ_DATA,
    M1APP_I2C_WRITE_DATA,
    M1APP_I2C_READ_REG_MULTI,
    M1APP_I2C_WRITE_REG_MULTI
} m1app_i2c_trans_type_t;

typedef enum {
    M1APP_I2C_DEV_BQ27421 = 0,
    M1APP_I2C_DEV_FUSB302,
    M1APP_I2C_DEV_BQ25896,
    M1APP_I2C_DEV_LP5814
} m1app_i2c_device_t;

typedef struct {
    m1app_i2c_device_t     dev_id;
    m1app_i2c_trans_type_t trans_type;
    uint16_t reg_address;
    uint8_t  reg_data;
    uint16_t data_len;
    uint8_t  *pdata;
    uint32_t timeout;
} m1app_i2c_trans_t;

extern int      m1_i2c_hal_trans_req(m1app_i2c_trans_t *trans_inf);
extern uint32_t m1_i2c_hal_get_error(void);


/* ==================================================================
 *  SPI
 * ================================================================== */

typedef struct {
    uint8_t  dev_id;
    uint8_t  trans_type;
    uint16_t reg_address;
    uint8_t  reg_data;
    uint16_t data_len;
    uint8_t  *pdata;
    uint32_t timeout;
} m1app_spi_trans_t;

extern int m1_spi_hal_trans_req(m1app_spi_trans_t *trans_inf);


/* ==================================================================
 *  Infrared TX
 * ================================================================== */

typedef enum {
    M1APP_IR_TX_INIT = 0,
    M1APP_IR_TX_ACTIVE,
    M1APP_IR_TX_POST_PROCESS,
    M1APP_IR_TX_DELAY,
    M1APP_IR_TX_COMPLETED
} m1app_ir_tx_state_t;

extern void               infrared_encode_sys_init(void);
extern void               infrared_encode_sys_deinit(void);
extern m1app_ir_tx_state_t infrared_transmit(uint8_t init);


/* ==================================================================
 *  Power / Battery
 * ================================================================== */

/* Returns battery icon level (0-4) from remaining charge percentage */
extern uint8_t m1_check_battery_level(uint8_t remaining_charge);


/* ==================================================================
 *  SD Card Info
 * ================================================================== */

typedef enum {
    M1APP_SD_NOT_READY = 0,
    M1APP_SD_MOUNTED,
    M1APP_SD_UNMOUNTED,
    M1APP_SD_FORMATTING,
    M1APP_SD_ERROR
} m1app_sd_status_t;

extern m1app_sd_status_t m1_sdcard_get_status(void);
extern uint8_t  m1_sd_detected(void);
extern uint32_t m1_sdcard_get_free_capacity(void);
extern uint32_t m1_sdcard_get_total_capacity(void);


/* ==================================================================
 *  FatFS File I/O
 * ================================================================== */

typedef enum {
    FR_OK = 0, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE,
    FR_NO_PATH, FR_INVALID_NAME, FR_DENIED, FR_EXIST,
    FR_INVALID_OBJECT, FR_WRITE_PROTECTED, FR_INVALID_DRIVE,
    FR_NOT_ENABLED, FR_NO_FILESYSTEM, FR_MKFS_ABORTED, FR_TIMEOUT,
    FR_LOCKED, FR_NOT_ENOUGH_CORE, FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER
} FRESULT;

/* File open mode flags */
#define FA_READ           0x01
#define FA_WRITE          0x02
#define FA_OPEN_EXISTING  0x00
#define FA_CREATE_NEW     0x04
#define FA_CREATE_ALWAYS  0x08
#define FA_OPEN_ALWAYS    0x10
#define FA_OPEN_APPEND    0x30

/* Opaque FatFS types — sized to match firmware layout */
typedef struct { uint8_t _opaque[600]; } FIL;
typedef struct { uint8_t _opaque[44]; }  FILINFO;
typedef struct { uint8_t _opaque[48]; }  DIR;

/* File operations */
extern FRESULT f_open(FIL *fp, const char *path, uint8_t mode);
extern FRESULT f_close(FIL *fp);
extern FRESULT f_read(FIL *fp, void *buf, unsigned int btr, unsigned int *br);
extern FRESULT f_write(FIL *fp, const void *buf, unsigned int btw, unsigned int *bw);
extern FRESULT f_lseek(FIL *fp, unsigned long ofs);
extern FRESULT f_sync(FIL *fp);
extern FRESULT f_truncate(FIL *fp);
extern int     f_printf(FIL *fp, const char *str, ...);
extern int     f_puts(const char *str, FIL *fp);

/* Directory operations */
extern FRESULT f_opendir(DIR *dp, const char *path);
extern FRESULT f_closedir(DIR *dp);
extern FRESULT f_readdir(DIR *dp, FILINFO *fno);
extern FRESULT f_mkdir(const char *path);
extern FRESULT f_unlink(const char *path);
extern FRESULT f_rename(const char *path_old, const char *path_new);
extern FRESULT f_stat(const char *path, FILINFO *fno);


/* ==================================================================
 *  File Utilities
 * ================================================================== */

extern const char *fu_get_filename(const char *path);
extern const char *fu_get_file_extension(const char *filename);
extern void fu_get_filename_without_ext(const char *path, char *outName, size_t outSize);
extern void fu_get_directory_path(const char *fullPath, char *outDir, size_t dirSize);
extern void fu_path_combine(char *out, size_t outSize, const char *path, const char *file);
extern int  fs_file_exists(const char *path);
extern int  fs_directory_exists(const char *path);
extern FRESULT fs_directory_ensure(const char *path);


/* ==================================================================
 *  Flipper File Format Parser
 * ================================================================== */

typedef struct {
    uint8_t _opaque[1024];  /* sized to match firmware flipper_file_t */
} flipper_file_t;

extern int  ff_open(flipper_file_t *ctx, const char *path);
extern int  ff_open_write(flipper_file_t *ctx, const char *path);
extern void ff_close(flipper_file_t *ctx);
extern int  ff_read_line(flipper_file_t *ctx);
extern int  ff_parse_kv(flipper_file_t *ctx);
extern const char *ff_get_key(const flipper_file_t *ctx);
extern const char *ff_get_value(const flipper_file_t *ctx);
extern int  ff_is_separator(const flipper_file_t *ctx);
extern int  ff_validate_header(flipper_file_t *ctx, const char *expected_filetype, uint8_t min_ver);
extern int  ff_write_kv_str(flipper_file_t *ctx, const char *key, const char *value);
extern int  ff_write_kv_uint32(flipper_file_t *ctx, const char *key, uint32_t val);
extern int  ff_write_kv_hex(flipper_file_t *ctx, const char *key, const uint8_t *data, uint8_t len);
extern int  ff_write_separator(flipper_file_t *ctx);
extern int  ff_write_comment(flipper_file_t *ctx, const char *comment);
extern uint8_t  ff_parse_hex_bytes(const char *str, uint8_t *out, uint8_t max_len);
extern uint16_t ff_parse_int32_array(const char *str, int32_t *out, uint16_t max_count);


/* ==================================================================
 *  Flipper IR File Format
 * ================================================================== */

#define FLIPPER_IR_RAW_MAX  512

typedef struct {
    char     name[64];
    uint8_t  type;          /* 0=parsed, 1=raw */
    char     protocol[32];
    uint8_t  address[4];
    uint8_t  command[4];
    uint32_t frequency;
    float    duty_cycle;
    int32_t  raw_data[FLIPPER_IR_RAW_MAX];
    uint16_t raw_count;
} flipper_ir_signal_t;

extern int      flipper_ir_open(flipper_file_t *ctx, const char *path);
extern int      flipper_ir_read_signal(flipper_file_t *ctx, flipper_ir_signal_t *out);
extern int      flipper_ir_write_header(flipper_file_t *ctx);
extern int      flipper_ir_write_signal(flipper_file_t *ctx, const flipper_ir_signal_t *sig);
extern uint16_t flipper_ir_count_signals(const char *path);
extern uint8_t  flipper_ir_proto_to_irmp(const char *name);
extern const char *flipper_ir_irmp_to_proto(uint8_t irmp_id);


/* ==================================================================
 *  Flipper NFC File Format
 * ================================================================== */

typedef struct {
    uint8_t _opaque[128];  /* sized to match firmware flipper_nfc_card_t */
} flipper_nfc_card_t;

extern int flipper_nfc_load(const char *path, flipper_nfc_card_t *out);
extern int flipper_nfc_save(const char *path, const flipper_nfc_card_t *card);


/* ==================================================================
 *  Flipper RFID File Format
 * ================================================================== */

typedef struct {
    uint8_t _opaque[64];   /* sized to match firmware flipper_rfid_tag_t */
} flipper_rfid_tag_t;

extern int flipper_rfid_load(const char *path, flipper_rfid_tag_t *out);
extern int flipper_rfid_save(const char *path, const flipper_rfid_tag_t *tag);


/* ==================================================================
 *  Flipper Sub-GHz File Format
 * ================================================================== */

typedef struct {
    uint8_t _opaque[2200]; /* sized to match firmware flipper_subghz_signal_t */
} flipper_subghz_signal_t;

extern int flipper_subghz_load(const char *path, flipper_subghz_signal_t *out);
extern int flipper_subghz_save(const char *path, const flipper_subghz_signal_t *sig);


/* ==================================================================
 *  Field Detection (NFC 13.56 MHz / LF RFID 125 kHz)
 * ================================================================== */

/* Initialize hardware for passive field detection.
 * Powers up NFC chip (ST25R3916) and RFID antenna.
 * Returns 0 on success, -1 if NFC chip init failed. */
extern int m1_field_detect_start(void);

/* Shut down field detection hardware and power down. */
extern void m1_field_detect_stop(void);

/* Check for external 13.56 MHz NFC reader field.
 * Returns 1 (true) if field is present, 0 otherwise.
 * Uses the ST25R3916's built-in External Field Detector. */
extern int m1_field_detect_nfc(void);

/* Check for external ~125 kHz LF RFID reader field.
 * Returns 1 (true) if field is present, 0 otherwise.
 * If frequency is non-NULL, writes estimated carrier frequency in Hz. */
extern int m1_field_detect_rfid(uint32_t *frequency);

/* Debug: returns last raw transition count from RFID sampling.
 * Useful for tuning thresholds. 0 = no signal, -1 = not initialized. */
extern int m1_field_detect_rfid_raw(void);

/* Debug: returns ST25R3916 AUX_DISPLAY register value.
 * Bit 6 (0x40) = efd_o (external field detected).
 * Bit 4 (0x10) = osc_ok (oscillator stable).
 * Returns -1 if not initialized. */
extern int m1_field_detect_nfc_raw(void);

/* Debug: returns ST25R3916 OP_CONTROL register value.
 * Bit 7 (0x80) = oscillator ON. Bits 1:0 = EFD mode (0x02 = manual PDT).
 * Expected value when working: 0x82 (osc ON + manual EFD). */
extern int m1_field_detect_nfc_opctl(void);


/* ==================================================================
 *  Crypto (AES-256-CBC)
 * ================================================================== */

#define M1APP_AES_KEY_SIZE  32
#define M1APP_AES_IV_SIZE   16

extern void     m1_crypto_derive_key(uint8_t key[M1APP_AES_KEY_SIZE]);
extern void     m1_crypto_generate_iv(uint8_t iv[M1APP_AES_IV_SIZE]);
extern uint32_t m1_crypto_encrypt(uint8_t *buf, uint32_t plaintext_len, uint32_t buf_size);
extern uint32_t m1_crypto_decrypt(uint8_t *buf, uint32_t total_len);


/* ==================================================================
 *  Virtual Keyboard
 * ================================================================== */

/* Show filename entry dialog. Returns 1 on OK, 0 on cancel. */
extern uint8_t m1_vkb_get_filename(char *description, char *default_name, char *new_name);

/* Show text entry dialog. Returns 1 on OK, 0 on cancel. */
extern uint8_t m1_vkbs_get_data(char *description, char *new_data);


/* ==================================================================
 *  String Utilities
 * ================================================================== */

extern void m1_strtolower(char *str);
extern void m1_strtoupper(char *str);
extern void m1_byte_to_hextext(const uint8_t *src, int len, char *out);
extern void m1_float_to_string(char *out_str, float in_val, uint8_t in_fraction);


/* ==================================================================
 *  C Library
 * ================================================================== */

extern void  *memset(void *s, int c, size_t n);
extern void  *memcpy(void *dest, const void *src, size_t n);
extern int    memcmp(const void *s1, const void *s2, size_t n);
extern size_t strlen(const char *s);
extern int    strcmp(const char *s1, const char *s2);
extern int    strncmp(const char *s1, const char *s2, size_t n);
extern char  *strncpy(char *dest, const char *src, size_t n);
extern char  *strncat(char *dest, const char *src, size_t n);
extern char  *strstr(const char *haystack, const char *needle);
extern char  *strchr(const char *s, int c);
extern int    snprintf(char *str, size_t size, const char *fmt, ...);
extern int    atoi(const char *nptr);
extern long   strtol(const char *nptr, char **endptr, int base);
extern int    rand(void);
extern void   srand(unsigned int seed);
extern int    abs(int j);


/* ==================================================================
 *  ESP32-C6 wireless card — WiFi / BLE / ESP-NOW
 * ==================================================================
 * Programmatic access to the M1's wireless coprocessor for custom apps.
 * All calls block on the SPI RPC round-trip; most return bool (true=ok).
 */

/* Health / version */
extern bool m1_esp_client_ping(void);
extern bool m1_esp_client_fw_version(char *out, uint16_t cap);

/* --- WiFi station --- */
/* Scan APs: fills buf with [count:2 LE] then per-AP records; returns length (<0 err). */
extern int  m1_esp_client_wifi_scan(uint8_t *buf, uint16_t buf_size);
/* Join an AP; on success writes the 4-byte IP into ip_out. */
extern bool m1_esp_client_wifi_connect(const char *ssid, const char *pwd, uint8_t ip_out[4]);
extern bool m1_esp_client_wifi_disconnect(void);
extern bool m1_esp_client_wifi_status(uint8_t *connected, uint8_t ip_out[4]);

/* --- BLE --- */
/* Scan for duration_sec; fills buf with advertisement records; returns length. */
extern int  m1_esp_client_ble_scan(uint8_t *buf, uint16_t buf_size, uint8_t duration_sec);
extern bool m1_esp_client_ble_advertise(const char *name);
extern bool m1_esp_client_ble_connect(const char *addr_str, uint8_t addr_type);
/* BLE HID keyboard (BadBT): advertise as `name`, send keypresses, tear down. */
extern bool m1_esp_client_ble_hid_init(const char *name);
extern bool m1_esp_client_ble_hid_deinit(void);
extern bool m1_esp_client_ble_hid_key(uint8_t modifier, uint8_t keycode);
extern bool m1_esp_client_ble_hid_status(uint8_t *connected);

/* --- Offensive WiFi (ESP runs these autonomously once started) --- */
extern bool m1_esp_client_deauth_start(const uint8_t bssid[6], uint8_t channel,
                                       const uint8_t station[6], uint16_t count,
                                       uint16_t interval_ms);
extern bool m1_esp_client_deauth_stop(void);
extern int  m1_esp_client_sta_scan(const uint8_t bssid[6], uint8_t channel, uint8_t duration_sec,
                                   uint8_t *buf, uint16_t buf_size);
extern bool m1_esp_client_hs_start(const uint8_t bssid[6], uint8_t channel, uint16_t deauth_count);
extern bool m1_esp_client_hs_status(uint8_t *state, uint32_t *total_len);
extern int  m1_esp_client_hs_read(uint32_t offset, uint8_t *buf, uint16_t want);
extern bool m1_esp_client_hs_stop(void);
extern bool m1_esp_client_beacon_start(const char ssids[][33], uint8_t count);
extern bool m1_esp_client_beacon_stop(void);

/* --- ESP-NOW peer link (M1<->M1, no AP) --- */
typedef struct { uint8_t mac[6]; int8_t rssi; char name[24]; } m1app_now_peer_t;
extern bool m1_esp_client_now_start(uint8_t channel, const char *name, uint8_t mac_out[6]);
extern bool m1_esp_client_now_stop(void);
extern bool m1_esp_client_now_announce(void);
extern int  m1_esp_client_now_get_peers(m1app_now_peer_t *out, int max);
extern bool m1_esp_client_now_send(const uint8_t mac[6], const uint8_t *data, uint16_t len);
extern int  m1_esp_client_now_recv(uint8_t *buf, uint16_t cap);


/* ==================================================================
 *  App Entry Point
 * ================================================================== */

/*
 * Your app must define this function. It is called by the M1 app
 * manager as a FreeRTOS task. Return 0 for clean exit.
 */
extern int32_t app_main(void *context);


#ifdef __cplusplus
}
#endif

#endif /* M1APP_H_ */
