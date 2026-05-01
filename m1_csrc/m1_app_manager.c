/* See COPYING.txt for license details. */

/*
*
*  m1_app_manager.c
*
*  App manager — browses, loads, and runs external .m1app (ELF32) files
*  from the SD card. Each app runs as a FreeRTOS task and communicates
*  with the display and buttons via the firmware API.
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_app_manager.h"
#include "m1_elf_loader.h"
#include "m1_app_api.h"
#include "m1_system.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_tasks.h"
#include "m1_buzzer.h"
#include "m1_watchdog.h"
#include "m1_games.h"
#include "m1_log_debug.h"
#include "ff.h"
#include "m1_scene.h"

/*************************** D E F I N E S ************************************/

#define TAG "APPMGR"

#define APPS_DIR              "0:/apps"
#define APPS_EXTENSION        ".m1app"
#define APPS_MAX_FILES        16
#define APPS_NAME_MAX_LEN     40
#define APPS_PATH_MAX_LEN     128

#define LIST_VISIBLE_ITEMS    ((uint8_t)m1_menu_max_visible())

/* App task priority — same level as subfunc handler */
#define APP_TASK_PRIORITY     (tskIDLE_PRIORITY + 5)

/* Default stack size if manifest doesn't specify (in 32-bit words) */
#define APP_DEFAULT_STACK     2048

//************************** S T R U C T U R E S *******************************

typedef struct {
    char filename[APPS_NAME_MAX_LEN];
    char display_name[APPS_NAME_MAX_LEN];
    char full_path[APPS_PATH_MAX_LEN];
} app_list_entry_t;

typedef struct {
    elf_app_t elf;
    TaskHandle_t task_handle;
    TaskHandle_t caller_handle;
    volatile bool running;
} app_run_ctx_t;

/***************************** V A R I A B L E S ******************************/

static app_list_entry_t s_app_list[APPS_MAX_FILES];
static uint16_t s_app_count = 0;
static app_run_ctx_t s_run_ctx;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static uint16_t apps_scan_directory(void);
static bool apps_read_manifest_name(const char *path, char *name_out, uint16_t name_len);
static void apps_draw_list(const char *title, uint16_t count, uint16_t selection);
static void apps_draw_message(const char *line1, const char *line2);
static elf_load_status_t apps_run(const char *path);
static void app_task_wrapper(void *param);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/*============================================================================*/
/*
 * @brief  Initialize the app manager (call once at startup)
 */
/*============================================================================*/
void m1_app_manager_init(void)
{
    m1_app_api_init();
    memset(&s_run_ctx, 0, sizeof(s_run_ctx));
    M1_LOG_I(TAG, "App manager initialized");
}


/*============================================================================*/
/*
 * @brief  Quick check: returns true if at least one .m1app file exists in
 *         the apps directory.  Stops at the first match so it is cheap
 *         enough to call from menu_main_init() on every menu open.
 */
/*============================================================================*/
bool m1_apps_any_available(void)
{
    DIR     dir;
    FILINFO fno;
    FRESULT res;
    uint16_t ext_len = (uint16_t)strlen(APPS_EXTENSION);

    res = f_opendir(&dir, APPS_DIR);
    if (res != FR_OK)
        return false;

    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == '\0')
            break;

        if (fno.fattrib & AM_DIR)
            continue;

        uint16_t name_len = (uint16_t)strlen(fno.fname);

        if (name_len > ext_len &&
            strcmp(&fno.fname[name_len - ext_len], APPS_EXTENSION) == 0)
        {
            f_closedir(&dir);
            return true;
        }
    }

    f_closedir(&dir);
    return false;
}


/*============================================================================*/
/*
 * @brief  Scan the apps directory for .m1app files
 * @retval uint16_t  Number of apps found
 */
/*============================================================================*/
static uint16_t apps_scan_directory(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    uint16_t count = 0;

    s_app_count = 0;

    /* Ensure the apps directory exists */
    f_mkdir(APPS_DIR);

    res = f_opendir(&dir, APPS_DIR);
    if (res != FR_OK)
    {
        M1_LOG_W(TAG, "Cannot open %s (err=%u)", APPS_DIR, res);
        return 0;
    }

    while (count < APPS_MAX_FILES)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == '\0')
            break;

        /* Skip directories */
        if (fno.fattrib & AM_DIR)
            continue;

        /* Check for .m1app extension */
        uint16_t name_len = (uint16_t)strlen(fno.fname);
        uint16_t ext_len = (uint16_t)strlen(APPS_EXTENSION);

        if (name_len <= ext_len)
            continue;

        if (strcmp(&fno.fname[name_len - ext_len], APPS_EXTENSION) != 0)
            continue;

        /* Store the entry */
        strncpy(s_app_list[count].filename, fno.fname, APPS_NAME_MAX_LEN - 1);
        s_app_list[count].filename[APPS_NAME_MAX_LEN - 1] = '\0';

        snprintf(s_app_list[count].full_path, APPS_PATH_MAX_LEN,
                 "%s/%s", APPS_DIR, fno.fname);

        /* Try to read the display name from the manifest */
        if (!apps_read_manifest_name(s_app_list[count].full_path,
                                      s_app_list[count].display_name,
                                      APPS_NAME_MAX_LEN))
        {
            /* Fall back to filename without extension */
            strncpy(s_app_list[count].display_name, fno.fname, name_len - ext_len);
            s_app_list[count].display_name[name_len - ext_len] = '\0';
        }

        count++;
    }

    f_closedir(&dir);
    s_app_count = count;

    M1_LOG_I(TAG, "Found %u apps in %s", count, APPS_DIR);
    return count;
}


/*============================================================================*/
/*
 * @brief  Try to read the app name from the .m1meta section of an ELF
 * @param  path      FatFS path to the .m1app file
 * @param  name_out  Buffer to receive the app name
 * @param  name_len  Size of name_out buffer
 * @retval bool      true if name was read successfully
 */
/*============================================================================*/
static bool apps_read_manifest_name(const char *path, char *name_out, uint16_t name_len)
{
    FIL fp;
    FRESULT res;
    UINT br;
    Elf32_Ehdr ehdr;
    Elf32_Shdr shdr;
    uint8_t shstrtab[256];
    uint32_t shstrtab_size;

    res = f_open(&fp, path, FA_READ);
    if (res != FR_OK)
        return false;

    /* Read ELF header */
    res = f_read(&fp, &ehdr, sizeof(Elf32_Ehdr), &br);
    if (res != FR_OK || br != sizeof(Elf32_Ehdr))
    {
        f_close(&fp);
        return false;
    }

    /* Quick validate */
    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L'  || ehdr.e_ident[3] != 'F')
    {
        f_close(&fp);
        return false;
    }

    if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0 || ehdr.e_shstrndx >= ehdr.e_shnum)
    {
        f_close(&fp);
        return false;
    }

    /* Read shstrtab header */
    uint32_t shstr_off = ehdr.e_shoff + (uint32_t)ehdr.e_shstrndx * ehdr.e_shentsize;
    res = f_lseek(&fp, shstr_off);
    if (res != FR_OK)
    {
        f_close(&fp);
        return false;
    }

    res = f_read(&fp, &shdr, sizeof(Elf32_Shdr), &br);
    if (res != FR_OK || br != sizeof(Elf32_Shdr))
    {
        f_close(&fp);
        return false;
    }

    shstrtab_size = shdr.sh_size;
    if (shstrtab_size > sizeof(shstrtab))
        shstrtab_size = sizeof(shstrtab);

    res = f_lseek(&fp, shdr.sh_offset);
    if (res != FR_OK)
    {
        f_close(&fp);
        return false;
    }

    res = f_read(&fp, shstrtab, shstrtab_size, &br);
    if (res != FR_OK || br != shstrtab_size)
    {
        f_close(&fp);
        return false;
    }

    /* Scan section headers looking for .m1meta */
    uint16_t num_sh = ehdr.e_shnum;
    if (num_sh > ELF_MAX_SECTIONS)
        num_sh = ELF_MAX_SECTIONS;

    uint16_t i;
    for (i = 0; i < num_sh; i++)
    {
        uint32_t hdr_off = ehdr.e_shoff + (uint32_t)i * ehdr.e_shentsize;
        res = f_lseek(&fp, hdr_off);
        if (res != FR_OK)
            break;

        res = f_read(&fp, &shdr, sizeof(Elf32_Shdr), &br);
        if (res != FR_OK || br != sizeof(Elf32_Shdr))
            break;

        if (shdr.sh_name < shstrtab_size)
        {
            const char *sec_name = (const char *)&shstrtab[shdr.sh_name];
            if (strcmp(sec_name, ".m1meta") == 0 &&
                shdr.sh_size >= sizeof(m1_app_manifest_t))
            {
                m1_app_manifest_t manifest;

                res = f_lseek(&fp, shdr.sh_offset);
                if (res != FR_OK)
                    break;

                res = f_read(&fp, &manifest, sizeof(m1_app_manifest_t), &br);
                if (res != FR_OK || br != sizeof(m1_app_manifest_t))
                    break;

                if (manifest.magic == M1_APP_MANIFEST_MAGIC &&
                    manifest.name[0] != '\0')
                {
                    manifest.name[sizeof(manifest.name) - 1] = '\0';
                    strncpy(name_out, manifest.name, name_len - 1);
                    name_out[name_len - 1] = '\0';
                    f_close(&fp);
                    return true;
                }
                break;
            }
        }
    }

    f_close(&fp);
    return false;
}


/*============================================================================*/
/*
 * @brief  Draw the app list on display
 * @param  title      Header title string
 * @param  count      Number of items
 * @param  selection  Currently selected index
 */
/*============================================================================*/
static void apps_draw_list(const char *title, uint16_t count, uint16_t selection)
{
    const uint8_t item_h   = m1_menu_item_h();
    const uint8_t text_ofs = item_h - 1;
    const uint8_t visible  = LIST_VISIBLE_ITEMS;
    uint16_t start_idx;
    uint16_t draw_visible;
    uint16_t i;
    uint8_t y;

    /* Calculate which items are visible */
    if (selection < visible)
        start_idx = 0;
    else
        start_idx = selection - visible + 1;

    draw_visible = count - start_idx;
    if (draw_visible > visible)
        draw_visible = visible;

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title — centered, matching m1_scene_draw_menu */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, title, TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* List items */
    u8g2_SetFont(&m1_u8g2, m1_menu_font());
    for (i = 0; i < draw_visible; i++)
    {
        uint16_t idx = start_idx + i;
        y = M1_MENU_AREA_TOP + (i * item_h);

        if (idx == selection)
        {
            u8g2_DrawRBox(&m1_u8g2, 0, y, M1_MENU_TEXT_W, item_h, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
            u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, s_app_list[idx].display_name);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
        else
        {
            u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, s_app_list[idx].display_name);
        }
    }

    /* Scrollbar — proportional handle, matching m1_scene_draw_menu */
    if (count > 0)
    {
        uint8_t sb_area_h   = visible * item_h;
        uint8_t sb_handle_h = (uint8_t)((uint16_t)sb_area_h / count);
        if (sb_handle_h < 6)
            sb_handle_h = 6;
        uint8_t sb_travel_h = (sb_area_h > sb_handle_h) ? (sb_area_h - sb_handle_h) : 0;
        uint8_t sb_handle_y = M1_MENU_AREA_TOP;
        if (count > 1)
            sb_handle_y += (uint8_t)((uint16_t)sb_travel_h * selection / (count - 1));

        u8g2_DrawVLine(&m1_u8g2, M1_MENU_SCROLLBAR_X + M1_MENU_SCROLLBAR_W / 2,
                       M1_MENU_AREA_TOP, sb_area_h);
        u8g2_DrawRBox(&m1_u8g2, M1_MENU_SCROLLBAR_X, sb_handle_y,
                      M1_MENU_SCROLLBAR_W, sb_handle_h, 1);
    }

    m1_u8g2_nextpage();
}


/*============================================================================*/
/*
 * @brief  Draw a simple message screen (two centered lines)
 * @param  line1  First line text (or NULL)
 * @param  line2  Second line text (or NULL)
 */
/*============================================================================*/
static void apps_draw_message(const char *line1, const char *line2)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title — centered, matching m1_scene_draw_menu */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "Apps", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Message lines centered in the 52px menu area */
    u8g2_SetFont(&m1_u8g2, m1_menu_font());
    if (line1 != NULL)
    {
        m1_draw_text(&m1_u8g2, 0, 34, M1_LCD_DISPLAY_WIDTH, line1, TEXT_ALIGN_CENTER);
    }
    if (line2 != NULL)
    {
        m1_draw_text(&m1_u8g2, 0, 46, M1_LCD_DISPLAY_WIDTH, line2, TEXT_ALIGN_CENTER);
    }

    m1_u8g2_nextpage();
}


/*============================================================================*/
/*
 * @brief  FreeRTOS task wrapper that calls the app's entry point
 * @param  param  Pointer to app_run_ctx_t
 */
/*============================================================================*/
static void app_task_wrapper(void *param)
{
    app_run_ctx_t *ctx = (app_run_ctx_t *)param;
    int32_t (*entry)(void *) = (int32_t (*)(void *))ctx->elf.entry_point;

    M1_LOG_I(TAG, "App task started, entry=0x%08lX",
             (unsigned long)ctx->elf.entry_point);

    /* Call the app's main function */
    int32_t result = entry(NULL);

    M1_LOG_I(TAG, "App returned %ld", (long)result);

    /* Signal the caller that we're done */
    ctx->running = false;

    /* Notify the caller task */
    if (ctx->caller_handle != NULL)
    {
        xTaskNotifyGive(ctx->caller_handle);
    }

    /* Delete ourselves */
    vTaskDelete(NULL);
}


/*============================================================================*/
/*
 * @brief  Load and run an app from the given path
 * @param  path  FatFS path to the .m1app file
 * @retval elf_load_status_t
 */
/*============================================================================*/
static elf_load_status_t apps_run(const char *path)
{
    elf_load_status_t status;
    const m1_api_interface_t *api;
    BaseType_t xret;
    uint32_t stack_words;

    /* Show loading screen */
    apps_draw_message("Loading...", NULL);

    api = m1_app_get_api();

    /* Load the ELF */
    memset(&s_run_ctx, 0, sizeof(s_run_ctx));
    status = elf_load_app(path, &s_run_ctx.elf, api);
    if (status != ELF_OK)
    {
        M1_LOG_E(TAG, "Load failed: %d", status);
        return status;
    }

    /* Determine stack size */
    stack_words = s_run_ctx.elf.manifest.stack_size;
    if (stack_words < 512)
        stack_words = 512;

    M1_LOG_I(TAG, "Starting app \"%s\" stack=%lu words",
             s_run_ctx.elf.manifest.name, (unsigned long)stack_words);

    /* Store caller task handle for notification */
    s_run_ctx.caller_handle = xTaskGetCurrentTaskHandle();
    s_run_ctx.running = true;

    /* Create the app task */
    xret = xTaskCreate(app_task_wrapper,
                       s_run_ctx.elf.manifest.name,
                       (uint16_t)stack_words,
                       &s_run_ctx,
                       APP_TASK_PRIORITY,
                       &s_run_ctx.task_handle);

    if (xret != pdPASS)
    {
        M1_LOG_E(TAG, "Task create failed (stack=%lu words, heap=%lu)",
                 (unsigned long)stack_words,
                 (unsigned long)xPortGetFreeHeapSize());
        elf_unload_app(&s_run_ctx.elf);
        return ELF_ERR_NO_MEMORY;
    }

    /* Wait for the app task to complete.
     * We poll with a timeout so we can check for BACK button to force-kill. */
    while (s_run_ctx.running)
    {
        /* Wait for notification from app task, or timeout to check buttons */
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
        (void)notified;

        if (!s_run_ctx.running)
            break;

        /* Check if BACK button was pressed (force kill) */
        if (m1_button_pressed_check(BUTTON_BACK_KP_ID))
        {
            M1_LOG_W(TAG, "Force-killing app task");
            s_run_ctx.running = false;

            if (s_run_ctx.task_handle != NULL)
            {
                vTaskDelete(s_run_ctx.task_handle);
                s_run_ctx.task_handle = NULL;
            }
            break;
        }
    }

    /* Cleanup */
    s_run_ctx.task_handle = NULL;
    elf_unload_app(&s_run_ctx.elf);

    /* Drain any stale button events */
    xQueueReset(main_q_hdl);

    M1_LOG_I(TAG, "App finished, heap=%lu",
             (unsigned long)xPortGetFreeHeapSize());

    return ELF_OK;
}


/*============================================================================*/
/*
 * @brief  Browse and run apps from SD card. Called as a menu sub_func.
 */
/*============================================================================*/
void game_apps_browser_run(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint16_t selection = 0;
    uint16_t count;

    /* Initialize API if not done yet */
    m1_app_api_init();

    /* Scan for apps */
    apps_draw_message("Scanning...", NULL);
    count = apps_scan_directory();

    if (count == 0)
    {
        apps_draw_message("No apps found", "Put .m1app in /apps/");

        /* Wait for BACK button */
        while (1)
        {
            ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
            if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
            {
                ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
                if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    xQueueReset(main_q_hdl);
                    return;
                }
            }
        }
    }

    /* Draw the initial list */
    apps_draw_list("Apps", count, selection);

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE)
            continue;

        if (q_item.q_evt_type != Q_EVENT_KEYPAD)
            continue;

        ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
        if (ret != pdTRUE)
            continue;

        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            break; /* Exit browser */
        }
        else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (selection > 0)
                selection--;
            else
                selection = count - 1;
            apps_draw_list("Apps", count, selection);
        }
        else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (selection < count - 1)
                selection++;
            else
                selection = 0;
            apps_draw_list("Apps", count, selection);
        }
        else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            /* Run the selected app */
            elf_load_status_t result = apps_run(s_app_list[selection].full_path);

            if (result != ELF_OK)
            {
                /* Show error */
                const char *err_msg;
                switch (result)
                {
                    case ELF_ERR_FILE:       err_msg = "File error";       break;
                    case ELF_ERR_FORMAT:     err_msg = "Bad format";       break;
                    case ELF_ERR_MACHINE:    err_msg = "Wrong arch";       break;
                    case ELF_ERR_NO_MEMORY:  err_msg = "Out of memory";    break;
                    case ELF_ERR_RELOCATE:   err_msg = "Relocation fail";  break;
                    case ELF_ERR_SYMBOL:     err_msg = "Missing symbol";   break;
                    case ELF_ERR_MANIFEST:   err_msg = "Bad manifest";     break;
                    case ELF_ERR_API_VERSION: err_msg = "API too new";     break;
                    default:                 err_msg = "Unknown error";     break;
                }

                apps_draw_message("Load Error", err_msg);

                /* Wait for any button to dismiss */
                while (1)
                {
                    ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
                    if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
                    {
                        xQueueReceive(button_events_q_hdl, &this_button_status, 0);
                        break;
                    }
                }
            }

            /* Re-draw list after app returns */
            apps_draw_list("Apps", count, selection);
        }
    }
}
