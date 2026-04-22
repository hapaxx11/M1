/* See COPYING.txt for license details. */

/*
*
*  m1_ir_universal.c
*
*  Universal Remote with Flipper IRDB support
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_ir_universal.h"
#include "flipper_file.h"
#include "m1_system.h"
#include "m1_display.h"
#include "m1_infrared.h"
#include "m1_tasks.h"
#include "m1_watchdog.h"
#include "m1_buzzer.h"
#include "m1_file_browser.h"
#include "m1_lcd.h"
#include "m1_led_indicator.h"
#include "irsnd.h"
#include "flipper_ir.h"
#include "ff.h"
#include "m1_virtual_kb.h"
#include "m1_file_util.h"
#include "m1_scene.h"
#include "m1_ir_quick_remote.h"
#include "m1_settings.h"

/*************************** D E F I N E S ************************************/

#define BROWSE_NAMES_MAX     16
#define BROWSE_NAME_MAX_LEN  64

#define DASHBOARD_ITEM_COUNT  13

#define IR_SEARCH_RESULTS_MAX BROWSE_NAMES_MAX

#define LIST_HEADER_HEIGHT    12
#define LIST_START_Y          (LIST_HEADER_HEIGHT + 2)
#define LIST_VISIBLE_ITEMS    ((uint8_t)(38 / m1_menu_item_h()))

#define IR_LEARNED_DIR        IR_UNIVERSAL_IRDB_ROOT "/Learned"
#define IR_CUSTOM_DIR         IR_UNIVERSAL_IRDB_ROOT "/Custom"

#define FAV_FILE_PATH         "0:/System/ir_favorites.txt"
#define RECENT_FILE_PATH      "0:/System/ir_recent.txt"

#define IR_FILE_EXTENSION     ".ir"

/* Custom remote builder */
#define IR_BUILDER_MAX_SLOTS      10
#define IR_BUILDER_SLOT_NAME_MAX  10
#define IR_BUILDER_LABEL_MAX      8   /* 7 visible chars + NUL */

//************************** S T R U C T U R E S *******************************

/* Signal source for a builder slot */
typedef enum {
	SLOT_SRC_NONE = 0,   /* Not yet assigned */
	SLOT_SRC_LEARNED,    /* Captured live via IR learn */
	SLOT_SRC_IRDB,       /* Picked from an existing .ir file */
} ir_builder_src_t;

/* One button slot in the custom remote builder */
typedef struct {
	char             slot_name[IR_BUILDER_SLOT_NAME_MAX]; /* e.g. "Power", "Vol+" */
	ir_builder_src_t src;
	char             label[IR_BUILDER_LABEL_MAX];          /* 7-char display label */
	/* Learned source: */
	IRMP_DATA        irmp_data;
	/* IRDB source: */
	char             src_filepath[IR_UNIVERSAL_PATH_MAX_LEN];
	char             src_signal_name[IR_UNIVERSAL_NAME_MAX_LEN];
	bool             src_is_raw;
} ir_builder_slot_t;

//****************************** V A R I A B L E S *****************************/

static ir_universal_cmd_t s_commands[IR_UNIVERSAL_MAX_CMDS];
static uint16_t s_cmd_count;
static char s_browse_names[BROWSE_NAMES_MAX][BROWSE_NAME_MAX_LEN];
static uint16_t s_browse_count;
static uint16_t s_browse_page;
static uint16_t s_browse_selection;
static ir_universal_mode_t s_mode;
static char s_current_path[IR_UNIVERSAL_PATH_MAX_LEN];

/* Favorites and recent tracking */
static char s_favorites[IR_UNIVERSAL_MAX_FAVORITES][IR_UNIVERSAL_PATH_MAX_LEN];
static uint8_t s_favorite_count;
static char s_recent[IR_UNIVERSAL_MAX_RECENT][IR_UNIVERSAL_PATH_MAX_LEN];
static uint8_t s_recent_count;

/* Search results */
static char s_search_results[IR_SEARCH_RESULTS_MAX][IR_UNIVERSAL_PATH_MAX_LEN];
static uint16_t s_search_count;

/* Transmit state */
static IRMP_DATA s_tx_irmp_data;

/* Raw IR TX support */
#define IR_RAW_OTA_BUFFER_MAX  FLIPPER_IR_RAW_MAX_SAMPLES
static char s_raw_tx_filepath[IR_UNIVERSAL_PATH_MAX_LEN];
static uint16_t s_raw_ota_buffer[IR_RAW_OTA_BUFFER_MAX];
static flipper_ir_signal_t s_raw_tx_signal;

/* Dashboard menu text (item 12 is dynamic: Remote/Normal Mode) */
static const char *s_dashboard_items[DASHBOARD_ITEM_COUNT] = {
	"TV Remote",
	"AC Remote",
	"Audio Remote",
	"Projector",
	"Fan Remote",
	"LED Remote",
	"Browse IRDB",
	"Search",
	"Learned",
	"Create Remote",
	"Favorites",
	"Recent",
	"Remote Mode"
};

/* Custom remote builder state */
static ir_builder_slot_t s_builder_slots[IR_BUILDER_MAX_SLOTS];
static uint8_t s_builder_n_slots;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static void dashboard_screen(void);
static void browse_directory(const char *path);
static void ir_browse_with_fb(const char *start_dir, uint8_t start_level);
static void show_commands(const char *ir_file_path);
static bool ir_file_action_menu(const char *ir_file_path);
static void transmit_command(const ir_universal_cmd_t *cmd);
static void transmit_raw_command(const ir_universal_cmd_t *cmd);
static void load_favorites(void);
static void save_favorites(void);
static void add_to_recent(const char *path);
static void load_recent(void);
static void save_recent(void);
static uint16_t scan_directory_page(const char *path, uint16_t page, uint16_t page_size);
static void draw_list_screen(const char *title, uint16_t count, uint16_t selection,
                             const char *ok_label);
static void draw_dashboard(uint8_t selection);
static void show_favorites_screen(void);
static void show_recent_screen(void);
static void show_search_screen(void);
static bool is_ir_file(const char *fname);
static void path_append(char *base, const char *item);
static void path_go_up(char *path);
static uint16_t parse_ir_file(const char *filepath);
static bool parse_ir_signal_block(flipper_file_t *ff, ir_universal_cmd_t *cmd);
static uint8_t map_flipper_protocol(const char *name);
static void ir_custom_builder_run(void);
static bool builder_browse_irdb_pick(ir_builder_slot_t *slot);
static bool builder_pick_signal_from_file(const char *filepath, ir_builder_slot_t *slot);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/



/*============================================================================*/
/*
 * Initialize the universal remote module
 */
/*============================================================================*/
void ir_universal_init(void)
{
	s_cmd_count = 0;
	s_browse_count = 0;
	s_browse_page = 0;
	s_browse_selection = 0;
	s_mode = IR_UNIVERSAL_MODE_DASHBOARD;
	memset(s_current_path, 0, sizeof(s_current_path));
	s_favorite_count = 0;
	s_recent_count = 0;

	/* Ensure SD card folders exist */
	f_mkdir(IR_UNIVERSAL_IRDB_ROOT);
	f_mkdir(IR_LEARNED_DIR);
	f_mkdir(IR_CUSTOM_DIR);

	load_favorites();
	load_recent();
} // void ir_universal_init(void)



/*============================================================================*/
/*
 * Deinitialize the universal remote module
 */
/*============================================================================*/
void ir_universal_deinit(void)
{
	save_favorites();
	save_recent();
} // void ir_universal_deinit(void)



/*============================================================================*/
/*
 * Main entry point for the universal remote feature.
 * Called from infrared_universal_remotes() in m1_infrared.c
 */
/*============================================================================*/
void ir_universal_run(void)
{
	dashboard_screen();
} // void ir_universal_run(void)



/*============================================================================*/
/*
 * Browse directly to the Learned files directory using m1_file_browser.
 * Called from infrared_saved_remotes() in m1_infrared.c so that "Replay"
 * goes straight to saved/learned .ir files with full ".." navigation support.
 */
/*============================================================================*/
void ir_universal_run_learned(void)
{
	uint8_t previous_orientation = m1_screen_orientation;

	settings_apply_orientation(M1_ORIENT_NORMAL);
	/* start_level = 2: user can navigate ".." up to "0:/IR", then to "0:/" */
	ir_browse_with_fb(IR_LEARNED_DIR, 2);
	settings_apply_orientation(previous_orientation);
} // void ir_universal_run_learned(void)



/*============================================================================*/
/*
 * Browse .ir files using the stock m1_file_browser (matching Monstatek pattern).
 *
 * start_dir   — directory to open initially (created if absent).
 * start_level — how many ".." hops the user can make before reaching the SD
 *               drive root.  Use 1 for "0:/IR" and 2 for "0:/IR/Learned".
 *               Setting this correctly means the user is never stuck in an
 *               empty folder; they can always navigate up to find files.
 *
 * When a .ir file is selected, its commands are shown via show_commands().
 * Pressing BACK at any depth exits the browser entirely.
 */
/*============================================================================*/
static void ir_browse_with_fb(const char *start_dir, uint8_t start_level)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	S_M1_file_browser_hdl *fb;
	S_M1_file_info *f_info;
	BaseType_t ret;
	uint8_t i;
	char filepath[IR_UNIVERSAL_PATH_MAX_LEN];

	/* Initialise file browser and point it at start_dir.
	 * m1_fb_set_dir() creates the directory if it does not exist. */
	fb = m1_fb_init(&m1_u8g2);
	if (!fb)
		return;

	m1_fb_set_dir(start_dir);

	/* Set dir_level so ".." navigates upward from start_dir.
	 * The buffers must hold one entry per level (0 … start_level). */
	fb->dir_level = start_level;
	{
		uint16_t *tmp_listing = (uint16_t *)realloc(
			fb->listing_index_buffer, ((uint16_t)start_level + 1) * sizeof(uint16_t));
		uint16_t *tmp_row = (uint16_t *)realloc(
			fb->row_index_buffer, ((uint16_t)start_level + 1) * sizeof(uint16_t));
		if (!tmp_listing || !tmp_row)
		{
			/* Heap exhausted — bail out cleanly */
			if (tmp_listing) fb->listing_index_buffer = tmp_listing;
			if (tmp_row)     fb->row_index_buffer     = tmp_row;
			m1_fb_deinit();
			return;
		}
		fb->listing_index_buffer = tmp_listing;
		fb->row_index_buffer     = tmp_row;
	}
	for (i = 0; i <= start_level; i++)
	{
		fb->listing_index_buffer[i] = 0;
		fb->row_index_buffer[i]     = 0;
	}

	/* Initial render */
	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
	m1_u8g2_nextpage();
	f_info = m1_fb_display(NULL);
	if (f_info->status != FB_OK)
	{
		m1_fb_deinit();
		m1_message_box(&m1_u8g2, "Infrared", "SD card not", "available", " OK ");
		return;
	}

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

		/* BACK exits the file browser at any depth */
		if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			m1_fb_deinit();
			return;
		}

		/* Forward all other button events to the file browser */
		f_info = m1_fb_display(&this_button_status);

		if (f_info->status == FB_OK && f_info->file_is_selected)
		{
			if (is_ir_file(f_info->file_name))
			{
				/* Build full path and open command viewer */
				snprintf(filepath, sizeof(filepath), "%s/%s",
				         f_info->dir_name, f_info->file_name);
				add_to_recent(filepath);
				show_commands(filepath);
			}
			/* Redisplay the browser at the same position after returning.
			 * If the SD card is no longer available, bail out cleanly. */
			f_info = m1_fb_display(NULL);
			if (f_info->status != FB_OK)
			{
				m1_fb_deinit();
				m1_message_box(&m1_u8g2, "Infrared", "SD card not", "available", " OK ");
				return;
			}
		}
	}

	/* Unreachable in normal operation */
	m1_fb_deinit();
} // static void ir_browse_with_fb(...)




/*============================================================================*/
static void draw_dashboard(uint8_t selection)
{
	uint8_t i;
	const uint8_t row_h   = m1_menu_item_h();
	const uint8_t max_vis = M1_MENU_VIS(DASHBOARD_ITEM_COUNT);
	uint8_t scroll = 0;

	if ((max_vis < DASHBOARD_ITEM_COUNT) && (selection >= max_vis))
	{
		scroll = selection - max_vis + 1;
		if (scroll > (DASHBOARD_ITEM_COUNT - max_vis))
		{
			scroll = DASHBOARD_ITEM_COUNT - max_vis;
		}
	}

	/* Update Remote Mode label to reflect current state */
	s_dashboard_items[12] = (m1_screen_orientation == M1_ORIENT_REMOTE) ? "Normal Mode" : "Remote Mode";

	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

	/* Title bar */
	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 2, 10, "Universal Remote");
	u8g2_DrawHLine(&m1_u8g2, 0, 12, 128);

	/* Menu items */
	u8g2_SetFont(&m1_u8g2, m1_menu_font());

	for (i = 0; (i < max_vis) && ((scroll + i) < DASHBOARD_ITEM_COUNT); i++)
	{
		uint8_t y = M1_MENU_AREA_TOP + (i * row_h);
		uint8_t item_idx = scroll + i;

		if (item_idx == selection)
		{
			/* Draw selection highlight */
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			u8g2_DrawBox(&m1_u8g2, 0, y, M1_MENU_TEXT_W, row_h);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
			u8g2_DrawStr(&m1_u8g2, 4, y + row_h - 1, s_dashboard_items[item_idx]);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		}
		else
		{
			u8g2_DrawStr(&m1_u8g2, 4, y + row_h - 1, s_dashboard_items[item_idx]);
		}
	}

	/* Scrollbar */
	{
		int track_y = M1_MENU_AREA_TOP - 1;
		int track_h = max_vis * row_h;
		u8g2_DrawFrame(&m1_u8g2, M1_MENU_SCROLLBAR_X, track_y, M1_MENU_SCROLLBAR_W, track_h);
		int handle_h = track_h / DASHBOARD_ITEM_COUNT;
		if (handle_h < 3) handle_h = 3;
		int handle_y = track_y + (selection * (track_h - handle_h)) / (DASHBOARD_ITEM_COUNT - 1);
		u8g2_DrawBox(&m1_u8g2, M1_MENU_SCROLLBAR_X, handle_y, M1_MENU_SCROLLBAR_W, handle_h);
	}

	m1_u8g2_nextpage();
}



/*============================================================================*/
/*
 * Dashboard screen - top-level menu for the universal remote
 */
/*============================================================================*/
static void dashboard_screen(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint8_t selection = 0;

	/* Remember the caller's orientation so we can restore it on exit.
	 * The "Toggle Remote Mode" item changes m1_screen_orientation to
	 * M1_ORIENT_REMOTE while inside the IR dashboard; leaving without
	 * restoring would leave the entire device in portrait mode. */
	const uint8_t saved_orient = m1_screen_orientation;

	draw_dashboard(selection);

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					/* Restore the orientation the caller had when
					 * entering the IR dashboard. */
					if (m1_screen_orientation != saved_orient)
						settings_apply_orientation(saved_orient);
					xQueueReset(main_q_hdl);
					break; /* Exit to caller */
				}
				else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection > 0)
						selection--;
					else
						selection = DASHBOARD_ITEM_COUNT - 1;
					draw_dashboard(selection);
				}
				else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < DASHBOARD_ITEM_COUNT - 1)
						selection++;
					else
						selection = 0;
					draw_dashboard(selection);
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					switch (selection)
					{
						case 0: /* TV Remote */
						case 1: /* AC Remote */
						case 2: /* Audio Remote */
						case 3: /* Projector */
						case 4: /* Fan Remote */
						case 5: /* LED Remote */
						{
							ir_quick_remote((ir_category_t)selection);
							break;
						}
						case 6: /* Browse IRDB */
						case 8: /* Learned — browse user-saved remotes */
						{
							/* Use m1_file_browser (stock Monstatek pattern) so the user
							 * can always navigate up via ".." even when the target folder
							 * is empty.  start_level encodes how many levels deep the
							 * starting directory sits relative to the SD drive root. */
							uint8_t browse_saved_orient = m1_screen_orientation;
							if (browse_saved_orient != M1_ORIENT_NORMAL)
								settings_apply_orientation(M1_ORIENT_NORMAL);
							if (selection == 6)
								ir_browse_with_fb(IR_UNIVERSAL_IRDB_ROOT, 1);
							else
								ir_browse_with_fb(IR_LEARNED_DIR, 2);
							/* Restore orientation */
							if (browse_saved_orient != M1_ORIENT_NORMAL)
								settings_apply_orientation(browse_saved_orient);
							break;
						}
						case 7: /* Search IRDB */
							show_search_screen();
							break;
						case 9: /* Create Remote — custom remote builder */
						{
							uint8_t build_saved_orient = m1_screen_orientation;
							if (build_saved_orient != M1_ORIENT_NORMAL)
								settings_apply_orientation(M1_ORIENT_NORMAL);
							ir_custom_builder_run();
							if (build_saved_orient != M1_ORIENT_NORMAL)
								settings_apply_orientation(build_saved_orient);
							break;
						}
						case 10: /* Favorites */
							show_favorites_screen();
							break;
						case 11: /* Recent */
							show_recent_screen();
							break;
						case 12: /* Toggle Remote Mode */
							if (m1_screen_orientation == M1_ORIENT_REMOTE)
								settings_apply_orientation(M1_ORIENT_NORMAL);
							else
								settings_apply_orientation(M1_ORIENT_REMOTE);
							break;
						default:
							break;
					}
					/* Redraw dashboard after returning from sub-screen */
					draw_dashboard(selection);
				}
			} /* if (q_item.q_evt_type == Q_EVENT_KEYPAD) */
		} /* if (ret == pdTRUE) */
	} /* while (1) */
} // static void dashboard_screen(void)



/*============================================================================*/
/*
 * Draw a generic list screen with title, items, and selection highlight
 */
/*============================================================================*/
static void draw_list_screen(const char *title, uint16_t count, uint16_t selection,
                             const char *ok_label)
{
	uint16_t i;
	uint16_t start_idx;
	uint16_t visible;
	uint8_t y;
	const uint8_t item_h = m1_menu_item_h();
	const uint8_t text_ofs = item_h - 1;

	/* Calculate which items are visible in the scroll window */
	if (selection < LIST_VISIBLE_ITEMS)
		start_idx = 0;
	else
		start_idx = selection - LIST_VISIBLE_ITEMS + 1;

	visible = count - start_idx;
	if (visible > LIST_VISIBLE_ITEMS)
		visible = LIST_VISIBLE_ITEMS;

	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

	/* Title bar */
	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 2, 10, title);
	u8g2_DrawHLine(&m1_u8g2, 0, LIST_HEADER_HEIGHT, 128);

	/* List items */
	u8g2_SetFont(&m1_u8g2, m1_menu_font());
	for (i = 0; i < visible; i++)
	{
		uint16_t idx = start_idx + i;
		y = LIST_START_Y + (i * item_h);

		if (idx == selection)
		{
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			u8g2_DrawBox(&m1_u8g2, 0, y, M1_MENU_TEXT_W, item_h);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
			u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, s_browse_names[idx]);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		}
		else
		{
			u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, s_browse_names[idx]);
		}
	}

	/* Scroll indicator */
	if (count > LIST_VISIBLE_ITEMS)
	{
		uint8_t bar_height = (LIST_VISIBLE_ITEMS * item_h * LIST_VISIBLE_ITEMS) / count;
		uint8_t bar_y;

		if (bar_height < 4)
			bar_height = 4;
		bar_y = LIST_START_Y + (start_idx * (LIST_VISIBLE_ITEMS * item_h - bar_height)) / (count - LIST_VISIBLE_ITEMS);
		u8g2_DrawBox(&m1_u8g2, 126, bar_y, 2, bar_height);
	}

	/* Bottom bar with position counter */
	{
		char page_info[16];
		const char *right_label = ok_label ? ok_label : "Open";
		const char *left_label;

		if (ok_label && strcmp(ok_label, "Send") == 0)
		{
			/* Command screen: LEFT = file actions, position counter as left text */
			snprintf(page_info, sizeof(page_info), "%u/%u", (unsigned)(selection + 1), (unsigned)count);
			left_label = "More";
		}
		else
		{
			snprintf(page_info, sizeof(page_info), "%u/%u", (unsigned)(selection + 1), (unsigned)count);
			left_label = page_info;
		}

		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, left_label, right_label, arrowright_8x8);
	}

	m1_u8g2_nextpage();
} // static void draw_list_screen(...)



/*============================================================================*/
/*
 * Scan a directory page and populate s_browse_names[]
 * Returns the number of entries found on this page
 */
/*============================================================================*/
static uint16_t scan_directory_page(const char *path, uint16_t page, uint16_t page_size)
{
	DIR dir;
	FILINFO fno;
	FRESULT res;
	uint16_t skip_count;
	uint16_t found = 0;

	res = f_opendir(&dir, path);
	if (res != FR_OK)
		return 0;

	/* Skip entries from previous pages */
	skip_count = page * page_size;
	while (skip_count > 0)
	{
		res = f_readdir(&dir, &fno);
		if (res != FR_OK || fno.fname[0] == '\0')
		{
			f_closedir(&dir);
			return 0;
		}
		/* Skip hidden files and . entries */
		if (fno.fname[0] == '.')
			continue;
		skip_count--;
	}

	/* Read entries for this page */
	while (found < page_size && found < BROWSE_NAMES_MAX)
	{
		res = f_readdir(&dir, &fno);
		if (res != FR_OK || fno.fname[0] == '\0')
			break;

		/* Skip hidden files */
		if (fno.fname[0] == '.')
			continue;

		strncpy(s_browse_names[found], fno.fname, BROWSE_NAME_MAX_LEN - 1);
		s_browse_names[found][BROWSE_NAME_MAX_LEN - 1] = '\0';
		found++;
	}

	f_closedir(&dir);
	return found;
} // static uint16_t scan_directory_page(...)



/*============================================================================*/
/*
 * Check if a filename has the .ir extension
 */
/*============================================================================*/
static bool is_ir_file(const char *fname)
{
	size_t len;

	if (fname == NULL)
		return false;

	len = strlen(fname);
	if (len < 4)
		return false;

	return (strcmp(&fname[len - 3], IR_FILE_EXTENSION) == 0);
} // static bool is_ir_file(...)



/*============================================================================*/
/*
 * Append a path component to a base path
 */
/*============================================================================*/
static void path_append(char *base, const char *item)
{
	size_t len = strlen(base);

	if (len > 0 && base[len - 1] != '/')
	{
		if (len + 1 < IR_UNIVERSAL_PATH_MAX_LEN)
		{
			base[len] = '/';
			base[len + 1] = '\0';
			len++;
		}
	}

	strncat(base, item, IR_UNIVERSAL_PATH_MAX_LEN - len - 1);
	base[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
} // static void path_append(...)



/*============================================================================*/
/*
 * Navigate one level up in the path
 */
/*============================================================================*/
static void path_go_up(char *path)
{
	size_t len = strlen(path);
	int i;

	if (len == 0)
		return;

	/* Remove trailing slash */
	if (path[len - 1] == '/')
	{
		path[len - 1] = '\0';
		len--;
	}

	/* Find the last slash and truncate */
	for (i = (int)len - 1; i >= 0; i--)
	{
		if (path[i] == '/')
		{
			path[i] = '\0';
			return;
		}
	}

	/* No slash found - clear the path */
	path[0] = '\0';
} // static void path_go_up(...)



/*============================================================================*/
/*
 * Browse a directory in the IRDB hierarchy.
 * Handles pagination with LEFT/RIGHT, selection with UP/DOWN/OK,
 * and BACK to navigate up.
 */
/*============================================================================*/
static void browse_directory(const char *path)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	char saved_path[IR_UNIVERSAL_PATH_MAX_LEN];
	char child_path[IR_UNIVERSAL_PATH_MAX_LEN];
	FILINFO fno;
	uint8_t is_directory;

	strncpy(s_current_path, path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
	s_current_path[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';

	s_browse_page = 0;
	s_browse_selection = 0;
	s_browse_count = scan_directory_page(s_current_path, s_browse_page, BROWSE_NAMES_MAX);

	if (s_browse_count == 0)
	{
		/* Empty directory — show a context-sensitive message.
		 * Match IR_LEARNED_DIR exactly or one of its subdirectories only,
		 * so similarly-prefixed sibling folders (for example "Learned2")
		 * are not treated as learned content. */
		size_t learned_dir_len = strlen(IR_LEARNED_DIR);
		bool is_learned =
			(strncmp(path, IR_LEARNED_DIR, learned_dir_len) == 0) &&
			(path[learned_dir_len] == '\0' || path[learned_dir_len] == '/');
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 25, "No files found");
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		if (is_learned)
		{
			u8g2_DrawStr(&m1_u8g2, 4, 38, "Use 'Learn' to capture");
			u8g2_DrawStr(&m1_u8g2, 4, 50, "and save IR signals.");
		}
		else
		{
			u8g2_DrawStr(&m1_u8g2, 4, 38, "Copy .ir files to");
			u8g2_DrawStr(&m1_u8g2, 4, 50, "0:/IR/ on SD card.");
		}
		u8g2_DrawStr(&m1_u8g2, 4, 62, "Back to return");
		m1_u8g2_nextpage();

		/* Wait for BACK press */
		while (1)
		{
			ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
			if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
			}
		}
	}

	/* Extract a short title from the path */
	{
		const char *title = s_current_path;
		const char *p = s_current_path;
		while (*p)
		{
			if (*p == '/')
				title = p + 1;
			p++;
		}
		if (*title == '\0')
			title = "IRDB";
		draw_list_screen(title, s_browse_count, s_browse_selection, NULL);
	}

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return; /* Exit to caller */
				}
				else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (s_browse_selection > 0)
						s_browse_selection--;
					else
						s_browse_selection = s_browse_count - 1;
				}
				else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (s_browse_selection < s_browse_count - 1)
						s_browse_selection++;
					else
						s_browse_selection = 0;
				}
				else if (this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
				{
					/* Next page */
					uint16_t next_count;
					next_count = scan_directory_page(s_current_path, s_browse_page + 1, BROWSE_NAMES_MAX);
					if (next_count > 0)
					{
						s_browse_page++;
						s_browse_count = next_count;
						s_browse_selection = 0;
					}
				}
				else if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
				{
					/* Previous page */
					if (s_browse_page > 0)
					{
						s_browse_page--;
						s_browse_count = scan_directory_page(s_current_path, s_browse_page, BROWSE_NAMES_MAX);
						s_browse_selection = 0;
					}
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (s_browse_count > 0 && s_browse_selection < s_browse_count)
					{
						/* Build child path */
						strncpy(child_path, s_current_path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
						child_path[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
						path_append(child_path, s_browse_names[s_browse_selection]);

						/* Check if it's a directory or file */
						if (f_stat(child_path, &fno) == FR_OK)
						{
							is_directory = (fno.fattrib & AM_DIR) ? 1 : 0;
						}
						else
						{
							is_directory = 0;
						}

						if (is_directory)
						{
							/* Save current state and recurse into subdirectory */
							strncpy(saved_path, s_current_path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
							saved_path[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';

							browse_directory(child_path);

							/* Restore state after returning */
							strncpy(s_current_path, saved_path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
							s_current_path[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
							s_browse_count = scan_directory_page(s_current_path, s_browse_page, BROWSE_NAMES_MAX);
							if (s_browse_selection >= s_browse_count && s_browse_count > 0)
								s_browse_selection = s_browse_count - 1;
						}
						else if (is_ir_file(s_browse_names[s_browse_selection]))
						{
							/* Open .ir file and show commands */
							add_to_recent(child_path);
							show_commands(child_path);

							/* Restore browse state after returning */
							s_browse_count = scan_directory_page(s_current_path, s_browse_page, BROWSE_NAMES_MAX);
							if (s_browse_selection >= s_browse_count && s_browse_count > 0)
								s_browse_selection = s_browse_count - 1;
						}
					}
				}

				/* Redraw list */
				{
					const char *title = s_current_path;
					const char *p = s_current_path;
					while (*p)
					{
						if (*p == '/')
							title = p + 1;
						p++;
					}
					if (*title == '\0')
						title = "IRDB";
					draw_list_screen(title, s_browse_count, s_browse_selection, NULL);
				}
			} /* if (q_item.q_evt_type == Q_EVENT_KEYPAD) */
		} /* if (ret == pdTRUE) */
	} /* while (1) */
} // static void browse_directory(...)



/*============================================================================*/
/*
 * Map a Flipper-format protocol name to an IRMP protocol ID.
 * Returns 0 if not found.
 */
/*============================================================================*/
static uint8_t map_flipper_protocol(const char *name)
{
	/* Common Flipper protocol names mapped to IRMP IDs.
	 * Must stay in sync with flipper_ir.c ir_proto_table[]. */
	if (strcmp(name, "NEC") == 0 || strcmp(name, "NECext") == 0)
		return IRMP_NEC_PROTOCOL;
	if (strcmp(name, "Samsung48") == 0)
		return IRMP_SAMSUNG48_PROTOCOL;
	if (strcmp(name, "Samsung32") == 0 || strcmp(name, "Samsung") == 0)
		return IRMP_SAMSUNG32_PROTOCOL;
	if (strcmp(name, "RC5") == 0 || strcmp(name, "RC5X") == 0)
		return IRMP_RC5_PROTOCOL;
	if (strcmp(name, "RC6") == 0)
		return IRMP_RC6_PROTOCOL;
	if (strcmp(name, "Sony12") == 0 || strcmp(name, "Sony15") == 0 || strcmp(name, "Sony20") == 0 ||
	    strcmp(name, "SIRC") == 0 || strcmp(name, "SIRC15") == 0 || strcmp(name, "SIRC20") == 0)
		return IRMP_SIRCS_PROTOCOL;
	if (strcmp(name, "Kaseikyo") == 0 || strcmp(name, "Panasonic") == 0)
		return IRMP_KASEIKYO_PROTOCOL;
	if (strcmp(name, "NEC42") == 0 || strcmp(name, "NEC42ext") == 0)
		return IRMP_NEC42_PROTOCOL;
	if (strcmp(name, "Denon") == 0 || strcmp(name, "Sharp") == 0)
		return IRMP_DENON_PROTOCOL;
	if (strcmp(name, "JVC") == 0)
		return IRMP_JVC_PROTOCOL;
	if (strcmp(name, "LG") == 0)
		return IRMP_LGAIR_PROTOCOL;
	if (strcmp(name, "Pioneer") == 0)
		return IRMP_NEC_PROTOCOL;
	if (strcmp(name, "Apple") == 0)
		return IRMP_APPLE_PROTOCOL;
	if (strcmp(name, "Bose") == 0)
		return IRMP_BOSE_PROTOCOL;
	if (strcmp(name, "Nokia") == 0)
		return IRMP_NOKIA_PROTOCOL;
	if (strcmp(name, "RCA") == 0)
		return IRMP_RCCAR_PROTOCOL;
	if (strcmp(name, "RCMM") == 0)
		return IRMP_RCMM32_PROTOCOL;

	return 0; /* Unknown protocol */
} // static uint8_t map_flipper_protocol(...)



/*============================================================================*/
/*
 * Parse a single IR signal block from a Flipper .ir file.
 * The flipper_file cursor should be positioned at the start of a signal block.
 * Returns true if a valid signal was parsed.
 */
/*============================================================================*/
static bool parse_ir_signal_block(flipper_file_t *ff, ir_universal_cmd_t *cmd)
{
	bool got_name = false;
	bool got_type = false;
	bool is_parsed = false;
	bool is_raw_type = false;

	memset(cmd, 0, sizeof(ir_universal_cmd_t));

	/* Read key-value pairs until we hit a separator or EOF */
	while (ff_read_line(ff))
	{
		if (ff_is_separator(ff))
			break; /* End of this signal block */

		if (!ff_parse_kv(ff))
			continue;

		if (strcmp(ff_get_key(ff), "name") == 0)
		{
			strncpy(cmd->name, ff_get_value(ff), IR_UNIVERSAL_NAME_MAX_LEN - 1);
			cmd->name[IR_UNIVERSAL_NAME_MAX_LEN - 1] = '\0';
			got_name = true;
		}
		else if (strcmp(ff_get_key(ff), "type") == 0)
		{
			got_type = true;
			if (strcmp(ff_get_value(ff), "parsed") == 0)
			{
				is_parsed = true;
				is_raw_type = false;
			}
			else if (strcmp(ff_get_value(ff), "raw") == 0)
			{
				is_parsed = false;
				is_raw_type = true;
			}
		}
		else if (strcmp(ff_get_key(ff), "protocol") == 0 && is_parsed)
		{
			cmd->protocol = map_flipper_protocol(ff_get_value(ff));
		}
		else if (strcmp(ff_get_key(ff), "address") == 0 && is_parsed)
		{
			uint8_t hex_buf[4];
			uint8_t n = ff_parse_hex_bytes(ff_get_value(ff), hex_buf, 4);
			cmd->address = (n >= 2) ? (uint16_t)(hex_buf[0] | ((uint16_t)hex_buf[1] << 8))
			                        : (uint16_t)hex_buf[0];
		}
		else if (strcmp(ff_get_key(ff), "command") == 0 && is_parsed)
		{
			uint8_t hex_buf[4];
			uint8_t n = ff_parse_hex_bytes(ff_get_value(ff), hex_buf, 4);
			cmd->command = (n >= 2) ? (uint16_t)(hex_buf[0] | ((uint16_t)hex_buf[1] << 8))
			                        : (uint16_t)hex_buf[0];
		}
		else if (strcmp(ff_get_key(ff), "frequency") == 0 && is_raw_type)
		{
			cmd->raw_freq = (uint32_t)strtoul(ff_get_value(ff), NULL, 10);
		}
		else if (strcmp(ff_get_key(ff), "data") == 0 && is_raw_type)
		{
			/* Count the raw timing values (space-separated integers) */
			const char *p = ff_get_value(ff);
			uint16_t count = 0;
			while (*p)
			{
				/* Skip whitespace */
				while (*p == ' ')
					p++;
				if (*p == '\0')
					break;
				count++;
				/* Skip non-whitespace */
				while (*p && *p != ' ')
					p++;
			}
			cmd->raw_count = count;
		}
	}

	if (got_name && got_type)
	{
		if (is_parsed && cmd->protocol != 0)
		{
			cmd->is_raw = false;
			cmd->flags = 0; /* Full frame (not repeat) */
			cmd->valid = true;
			return true;
		}
		else if (is_raw_type && cmd->raw_freq > 0)
		{
			cmd->is_raw = true;
			cmd->valid = true;
			return true;
		}
	}

	return false;
} // static bool parse_ir_signal_block(...)



/*============================================================================*/
/*
 * Parse a Flipper .ir file and load signals into s_commands[].
 * Accepts both "IR signals file" (per-device saved remotes) and
 * "IR library file" (universal remote databases) — Flipper uses both.
 * Returns the number of commands loaded.
 */
/*============================================================================*/
static uint16_t parse_ir_file(const char *filepath)
{
	flipper_file_t ff;
	uint16_t count = 0;

	if (!ff_open(&ff, filepath))
		return 0;

	/*
	 * Flipper uses two IR filetypes:
	 *   "IR signals file"  — individual device remote (e.g. user-saved)
	 *   "IR library file"  — universal remote database (tv.ir, ac.ir, etc.)
	 * Accept either so files from Flipper SD card work without conversion.
	 */
	if (!ff_validate_header(&ff, "IR signals file", 1))
	{
		/* Not a signals file — try library format */
		ff_close(&ff);
		if (!ff_open(&ff, filepath))
			return 0;
		if (!ff_validate_header(&ff, "IR library file", 1))
		{
			ff_close(&ff);
			return 0;
		}
	}

	/* Parse signal blocks */
	while (count < IR_UNIVERSAL_MAX_CMDS)
	{
		if (!parse_ir_signal_block(&ff, &s_commands[count]))
		{
			/* Check if we've reached EOF */
			if (ff.eof)
				break;
			continue; /* Skip invalid blocks */
		}
		count++;
	}

	ff_close(&ff);
	return count;
} // static uint16_t parse_ir_file(...)



/*============================================================================*/
/*
 * IR Saved File Action Menu — Flipper "saved_menu" pattern.
 *
 * Presents file-level actions (Send All, Info, Rename, Delete) after a
 * .ir file has been loaded and its commands are in s_commands[].
 *
 * Returns:
 *   true  — file still exists (caller should continue / redraw)
 *   false — file was deleted or renamed (caller should return to browse)
 */
/*============================================================================*/

#define IR_ACTION_COUNT   4
#define IR_ACTION_SEND_ALL 0
#define IR_ACTION_INFO     1
#define IR_ACTION_RENAME   2
#define IR_ACTION_DELETE   3

static const char *ir_action_labels[IR_ACTION_COUNT] = {
    "Send All", "Info", "Rename", "Delete"
};

static void draw_ir_action_menu(const char *filename, uint8_t sel)
{
    char dname[22];
    strncpy(dname, filename, 21);
    dname[21] = '\0';

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Truncated filename at top */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, dname);
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    /* 4 items in 52px (y=13..64) → 13px per item */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    for (uint8_t i = 0; i < IR_ACTION_COUNT; i++)
    {
        uint8_t y = 13 + i * 13;
        if (i == sel)
        {
            u8g2_DrawBox(&m1_u8g2, 0, y, M1_LCD_DISPLAY_WIDTH, 13);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }
        u8g2_DrawStr(&m1_u8g2, 8, y + 10, ir_action_labels[i]);
        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    m1_u8g2_nextpage();
}

static void draw_ir_info_screen(const char *filepath)
{
    char line[48];
    char fname[32];
    uint16_t raw_count = 0, parsed_count = 0;

    fu_get_filename_without_ext(filepath, fname, sizeof(fname));

    for (uint16_t i = 0; i < s_cmd_count; i++)
    {
        if (s_commands[i].is_raw)
            raw_count++;
        else
            parsed_count++;
    }

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "IR File Info");
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    snprintf(line, sizeof(line), "Name: %s", fname);
    u8g2_DrawStr(&m1_u8g2, 2, 22, line);

    snprintf(line, sizeof(line), "Commands: %u", (unsigned)s_cmd_count);
    u8g2_DrawStr(&m1_u8g2, 2, 32, line);

    snprintf(line, sizeof(line), "Parsed: %u  Raw: %u",
             (unsigned)parsed_count, (unsigned)raw_count);
    u8g2_DrawStr(&m1_u8g2, 2, 42, line);

    /* Show first command's protocol as representative */
    if (s_cmd_count > 0 && !s_commands[0].is_raw)
    {
        extern const char * const irmp_protocol_names[];
        snprintf(line, sizeof(line), "Proto: %s",
                 irmp_protocol_names[s_commands[0].protocol]);
        u8g2_DrawStr(&m1_u8g2, 2, 52, line);
    }
    else if (s_cmd_count > 0 && s_commands[0].is_raw)
    {
        snprintf(line, sizeof(line), "Freq: %lu Hz",
                 (unsigned long)s_commands[0].raw_freq);
        u8g2_DrawStr(&m1_u8g2, 2, 52, line);
    }

    m1_u8g2_nextpage();
}

static bool ir_file_action_menu(const char *ir_file_path)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint8_t action_sel = 0;

    const char *short_name = fu_get_filename(ir_file_path);

    draw_ir_action_menu(short_name, action_sel);

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD)
            continue;

        xQueueReceive(button_events_q_hdl, &this_button_status, 0);

        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            return true;  /* Cancel — file unchanged */
        }
        else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            action_sel = (action_sel > 0) ? action_sel - 1 : IR_ACTION_COUNT - 1;
        }
        else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            action_sel = (action_sel + 1) % IR_ACTION_COUNT;
        }
        else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            switch (action_sel)
            {
                case IR_ACTION_SEND_ALL:
                {
                    /* Transmit every command in the file sequentially.
                     * Each transmit_command() calls infrared_encode_sys_init(),
                     * so we must deinit after each TX — even on timeout —
                     * before the next command can re-init the hardware. */
                    bool cancelled = false;
                    for (uint16_t i = 0; i < s_cmd_count && !cancelled; i++)
                    {
                        if (s_commands[i].valid)
                        {
                            transmit_command(&s_commands[i]);

                            /* Wait specifically for Q_EVENT_IRRED_TX, ignoring
                             * unrelated events.  BACK cancels the loop.
                             * Drain button_events_q_hdl when keypad events
                             * arrive to keep the two queues in sync. */
                            S_M1_Main_Q_t tx_q;
                            uint32_t deadline = HAL_GetTick() + 3000;
                            bool tx_done = false;
                            while (!tx_done && !cancelled)
                            {
                                uint32_t now = HAL_GetTick();
                                uint32_t remaining = (now < deadline) ? (deadline - now) : 0;
                                if (remaining == 0)
                                    break; /* timeout */
                                BaseType_t rx = xQueueReceive(main_q_hdl, &tx_q, pdMS_TO_TICKS(remaining));
                                if (rx != pdTRUE)
                                    break; /* timeout */
                                if (tx_q.q_evt_type == Q_EVENT_IRRED_TX)
                                {
                                    tx_done = true;
                                }
                                else if (tx_q.q_evt_type == Q_EVENT_KEYPAD)
                                {
                                    S_M1_Buttons_Status bs;
                                    xQueueReceive(button_events_q_hdl, &bs, 0);
                                    if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                                        cancelled = true;
                                }
                                /* else: ignore other event types, loop again */
                            }

                            /* Always deinit — transmit_command() called
                             * infrared_encode_sys_init(), so we must tear down
                             * regardless of whether TX complete arrived. */
                            infrared_encode_sys_deinit();
                            if (!cancelled)
                                vTaskDelay(pdMS_TO_TICKS(200));
                        }
                    }
                    m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
                    return true;  /* Done — stay in commands */
                }

                case IR_ACTION_INFO:
                {
                    draw_ir_info_screen(ir_file_path);
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
                    break;  /* Redraw action menu */
                }

                case IR_ACTION_RENAME:
                {
                    char new_name[32];
                    char base[32];
                    fu_get_filename_without_ext(ir_file_path, base, sizeof(base));

                    if (m1_vkb_get_filename("Rename to:", base, new_name))
                    {
                        char dir_path[IR_UNIVERSAL_PATH_MAX_LEN];
                        char new_path[IR_UNIVERSAL_PATH_MAX_LEN];
                        fu_get_directory_path(ir_file_path, dir_path, sizeof(dir_path));
                        snprintf(new_path, sizeof(new_path), "%s/%s%s",
                                 dir_path, new_name, IR_FILE_EXTENSION);
                        FRESULT res = f_rename(ir_file_path, new_path);
                        if (res == FR_OK)
                            return false;  /* File moved — return to browse */
                    }
                    break;  /* Rename cancelled or failed — stay in menu */
                }

                case IR_ACTION_DELETE:
                {
                    uint8_t confirm = m1_message_box_choice(&m1_u8g2,
                        "Delete file?", short_name, "", "OK  /  Cancel");
                    if (confirm == 1)
                    {
                        m1_fb_delete_file(ir_file_path);
                        return false;  /* File gone — return to browse */
                    }
                    break;  /* Cancelled — redraw action menu */
                }
            }
        }

        draw_ir_action_menu(short_name, action_sel);
    }
}



/*============================================================================*/
/*
 * Display the command list from a .ir file and allow the user
 * to select and transmit commands.
 */
/*============================================================================*/
static void show_commands(const char *ir_file_path)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t selection = 0;
	uint16_t i;
	const char *short_name;
	const char *p;
	char title[BROWSE_NAME_MAX_LEN];

	/* Store file path for raw TX re-read */
	strncpy(s_raw_tx_filepath, ir_file_path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
	s_raw_tx_filepath[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';

	/* Parse the .ir file */
	s_cmd_count = parse_ir_file(ir_file_path);

	if (s_cmd_count == 0)
	{
		/* No commands found */
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 32, "No IR signals");
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 50, "Back to return");
		m1_u8g2_nextpage();

		while (1)
		{
			ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
			if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
			}
		}
	}

	/* Copy command names into s_browse_names for the list renderer */
	for (i = 0; i < s_cmd_count && i < BROWSE_NAMES_MAX; i++)
	{
		snprintf(s_browse_names[i], BROWSE_NAME_MAX_LEN, "%s", s_commands[i].name);
	}
	s_browse_count = (s_cmd_count < BROWSE_NAMES_MAX) ? s_cmd_count : BROWSE_NAMES_MAX;

	/* Extract filename for title */
	short_name = ir_file_path;
	p = ir_file_path;
	while (*p)
	{
		if (*p == '/')
			short_name = p + 1;
		p++;
	}
	strncpy(title, short_name, BROWSE_NAME_MAX_LEN - 1);
	title[BROWSE_NAME_MAX_LEN - 1] = '\0';

	draw_list_screen(title, s_browse_count, selection, "Send");

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return; /* Exit to caller */
				}
				else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection > 0)
						selection--;
					else
						selection = s_browse_count - 1;
				}
				else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_browse_count - 1)
						selection++;
					else
						selection = 0;
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_cmd_count && s_commands[selection].valid)
					{
						transmit_command(&s_commands[selection]);
					}
				}
				else if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
				{
					/* File action menu (Flipper saved_menu pattern) */
					if (!ir_file_action_menu(ir_file_path))
					{
						/* File was renamed or deleted — exit to caller */
						xQueueReset(main_q_hdl);
						return;
					}
				}

				draw_list_screen(title, s_browse_count, selection, "Send");
			} /* if (q_item.q_evt_type == Q_EVENT_KEYPAD) */
			else if (q_item.q_evt_type == Q_EVENT_IRRED_TX)
			{
				/* IR TX completed — tear down hardware cleanly */
				infrared_encode_sys_deinit();
				m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_OFF, LED_FASTBLINK_ONTIME_OFF);
				draw_list_screen(title, s_browse_count, selection, "Send");
			}
		} /* if (ret == pdTRUE) */
	} /* while (1) */
} // static void show_commands(...)



/*============================================================================*/
/*
 * Transmit an IR command.
 * Parsed signals: uses IRSND to encode and transmit via TIM16.
 * Raw signals: re-reads .ir file, converts timing data to OTA format, transmits.
 */
/*============================================================================*/
static void transmit_command(const ir_universal_cmd_t *cmd)
{
	char tx_info[32];

	if (cmd == NULL || !cmd->valid)
		return;

	if (cmd->is_raw)
	{
		transmit_raw_command(cmd);
		return;
	}

	/* Check for unknown/unsupported protocol */
	if (cmd->protocol == IRMP_UNKNOWN_PROTOCOL)
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
		u8g2_DrawStr(&m1_u8g2, 10, 15, "Unsupported");
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 30, "Protocol not recognized");
		u8g2_DrawStr(&m1_u8g2, 10, 45, cmd->name);
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1500));
		return;
	}

	/* Parsed signal: use IRSND to transmit */
	m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);

	/* Show transmitting screen */
	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 10, 15, "Transmitting...");
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 10, 30, cmd->name);
	snprintf(tx_info, sizeof(tx_info), "Addr:0x%04X Cmd:0x%04X", cmd->address, cmd->command);
	u8g2_DrawStr(&m1_u8g2, 4, 45, tx_info);
	m1_u8g2_nextpage();

	/* Set up IRMP_DATA for transmission */
	s_tx_irmp_data.protocol = cmd->protocol;
	s_tx_irmp_data.address = cmd->address;
	s_tx_irmp_data.command = cmd->command;
	s_tx_irmp_data.flags = cmd->flags;

	/* Initialize the IR encoder system */
	infrared_encode_sys_init();

	/* Generate OTA data from the IRMP_DATA structure */
	irsnd_generate_tx_data(s_tx_irmp_data);

	/* Start the transmit process */
	infrared_transmit(1); /* 1 = initialize state machine */
	infrared_transmit(0); /* 0 = run state machine (kicks off TX) */

	m1_buzzer_notification();
} // static void transmit_command(...)



/*============================================================================*/
/*
 * Transmit a raw IR signal.
 * Re-reads the .ir file to get timing data, converts to OTA buffer format,
 * sets the carrier frequency, and drives the TX via TIM16 interrupt.
 *
 * Flipper raw format: space-separated positive integers alternating mark/space.
 * OTA buffer format: uint16_t with LSB=1 for mark, LSB=0 for space.
 */
/*============================================================================*/
static void transmit_raw_command(const ir_universal_cmd_t *cmd)
{
	flipper_file_t ff;
	uint16_t i;
	uint16_t ota_len;
	uint32_t duration;
	char tx_info[32];
	bool found = false;

	if (cmd == NULL || !cmd->valid || !cmd->is_raw)
		return;

	/* Re-open the .ir file to read raw timing data */
	if (!flipper_ir_open(&ff, s_raw_tx_filepath))
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 30, "File read error");
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1000));
		return;
	}

	/* Find the signal by name */
	while (flipper_ir_read_signal(&ff, &s_raw_tx_signal))
	{
		if (strcmp(s_raw_tx_signal.name, cmd->name) == 0 &&
		    s_raw_tx_signal.type == FLIPPER_IR_SIGNAL_RAW)
		{
			found = true;
			break;
		}
	}
	ff_close(&ff);

	if (!found || s_raw_tx_signal.raw.sample_count == 0)
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 30, "Signal not found");
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1000));
		return;
	}

	/* Show transmitting screen */
	m1_led_fast_blink(LED_BLINK_ON_RGB, LED_FASTBLINK_PWM_M, LED_FASTBLINK_ONTIME_M);

	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 10, 15, "Transmitting...");
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 10, 30, cmd->name);
	snprintf(tx_info, sizeof(tx_info), "Raw %luHz %u smp",
	         (unsigned long)s_raw_tx_signal.raw.frequency,
	         (unsigned)s_raw_tx_signal.raw.sample_count);
	u8g2_DrawStr(&m1_u8g2, 4, 45, tx_info);
	m1_u8g2_nextpage();

	/* Convert Flipper raw data to OTA buffer format.
	 * Flipper raw: all positive values, alternating mark/space starting with mark.
	 * OTA: uint16_t, LSB=1 for mark (carrier ON), LSB=0 for space (carrier OFF). */
	ota_len = s_raw_tx_signal.raw.sample_count;
	if (ota_len > IR_RAW_OTA_BUFFER_MAX)
		ota_len = IR_RAW_OTA_BUFFER_MAX;

	for (i = 0; i < ota_len; i++)
	{
		duration = (uint32_t)abs(s_raw_tx_signal.raw.samples[i]);
		if (duration > 65534)
			duration = 65534;
		if (duration < 2)
			duration = 2;

		if (i % 2 == 0) /* Even index = mark (carrier ON) */
			s_raw_ota_buffer[i] = (uint16_t)duration | IR_OTA_PULSE_BIT_MASK;
		else /* Odd index = space (carrier OFF) */
			s_raw_ota_buffer[i] = (uint16_t)duration & IR_OTA_SPACE_BIT_MASK;
	}

	/* Initialize the IR encoder hardware (TIM1 carrier + TIM16 baseband) */
	infrared_encode_sys_init();

	/* Set carrier frequency for this raw signal */
	irsnd_set_carrier_freq(s_raw_tx_signal.raw.frequency);

	/* Set up TX variables directly (bypassing IRSND OTA frame generation) */
	ir_ota_data_tx_active = TRUE;
	ir_ota_data_tx_counter = 0;
	ir_ota_data_tx_len = ota_len;
	pir_ota_data_tx_buffer = s_raw_ota_buffer;

	/* Configure TIM16 with first entry and start transmission */
	__HAL_TIM_URS_ENABLE(&Timerhdl_IrTx);
	Timerhdl_IrTx.Instance->ARR = s_raw_ota_buffer[0];
	HAL_TIM_GenerateEvent(&Timerhdl_IrTx, TIM_EVENTSOURCE_UPDATE);
	__HAL_TIM_URS_DISABLE(&Timerhdl_IrTx);

	if (HAL_IS_BIT_SET(Timerhdl_IrTx.Instance->SR, TIM_FLAG_UPDATE))
		CLEAR_BIT(Timerhdl_IrTx.Instance->SR, TIM_FLAG_UPDATE);

	if (s_raw_ota_buffer[0] & 0x0001) /* First entry is a mark */
		irsnd_on();

	__HAL_TIM_ENABLE(&Timerhdl_IrTx);

	/* Load next entry for the second period */
	if (ota_len > 1)
		Timerhdl_IrTx.Instance->ARR = s_raw_ota_buffer[++ir_ota_data_tx_counter];

	m1_buzzer_notification();

	/* TX is now running asynchronously via TIM16 ISR.
	 * The ISR steps through s_raw_ota_buffer and sends Q_EVENT_IRRED_TX
	 * when complete. The caller's event loop handles cleanup. */
} // static void transmit_raw_command(...)



/*============================================================================*/
/*
 * Load favorite paths from SD card file
 */
/*============================================================================*/
static void load_favorites(void)
{
	FIL file;
	FRESULT res;
	UINT br;
	char line[IR_UNIVERSAL_PATH_MAX_LEN];
	uint8_t count = 0;

	s_favorite_count = 0;

	res = f_open(&file, FAV_FILE_PATH, FA_READ);
	if (res != FR_OK)
		return;

	while (count < IR_UNIVERSAL_MAX_FAVORITES)
	{
		/* Read a line manually */
		uint16_t i = 0;
		char ch;

		while (i < IR_UNIVERSAL_PATH_MAX_LEN - 1)
		{
			res = f_read(&file, &ch, 1, &br);
			if (res != FR_OK || br == 0)
			{
				if (i > 0) /* Partial line without newline at EOF */
				{
					line[i] = '\0';
					strncpy(s_favorites[count], line, IR_UNIVERSAL_PATH_MAX_LEN - 1);
					s_favorites[count][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
					count++;
				}
				goto done_fav;
			}

			if (ch == '\n' || ch == '\r')
			{
				if (i > 0)
				{
					line[i] = '\0';
					strncpy(s_favorites[count], line, IR_UNIVERSAL_PATH_MAX_LEN - 1);
					s_favorites[count][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
					count++;
				}
				break;
			}
			line[i++] = ch;
		}
	}

done_fav:
	s_favorite_count = count;
	f_close(&file);
} // static void load_favorites(void)



/*============================================================================*/
/*
 * Save favorites to SD card file
 */
/*============================================================================*/
static void save_favorites(void)
{
	FIL file;
	FRESULT res;
	UINT bw;
	uint8_t i;

	/* Ensure the System directory exists */
	f_mkdir("0:/System");

	res = f_open(&file, FAV_FILE_PATH, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK)
		return;

	for (i = 0; i < s_favorite_count; i++)
	{
		f_write(&file, s_favorites[i], strlen(s_favorites[i]), &bw);
		f_write(&file, "\n", 1, &bw);
	}

	f_close(&file);
} // static void save_favorites(void)



/*============================================================================*/
/*
 * Add a path to the recent list (most recent first, de-duplicated)
 */
/*============================================================================*/
static void add_to_recent(const char *path)
{
	uint8_t i;
	int existing = -1;

	/* Check if it already exists */
	for (i = 0; i < s_recent_count; i++)
	{
		if (strcmp(s_recent[i], path) == 0)
		{
			existing = i;
			break;
		}
	}

	if (existing >= 0)
	{
		/* Move it to the front */
		char tmp[IR_UNIVERSAL_PATH_MAX_LEN];
		strncpy(tmp, s_recent[existing], IR_UNIVERSAL_PATH_MAX_LEN - 1);
		tmp[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
		for (i = existing; i > 0; i--)
		{
			strncpy(s_recent[i], s_recent[i - 1], IR_UNIVERSAL_PATH_MAX_LEN - 1);
			s_recent[i][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
		}
		strncpy(s_recent[0], tmp, IR_UNIVERSAL_PATH_MAX_LEN - 1);
		s_recent[0][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
	}
	else
	{
		/* Shift everything down and insert at front */
		if (s_recent_count < IR_UNIVERSAL_MAX_RECENT)
			s_recent_count++;
		for (i = s_recent_count - 1; i > 0; i--)
		{
			strncpy(s_recent[i], s_recent[i - 1], IR_UNIVERSAL_PATH_MAX_LEN - 1);
			s_recent[i][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
		}
		strncpy(s_recent[0], path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
		s_recent[0][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
	}
} // static void add_to_recent(...)



/*============================================================================*/
/*
 * Load recent paths from SD card file
 */
/*============================================================================*/
static void load_recent(void)
{
	FIL file;
	FRESULT res;
	UINT br;
	char line[IR_UNIVERSAL_PATH_MAX_LEN];
	uint8_t count = 0;

	s_recent_count = 0;

	res = f_open(&file, RECENT_FILE_PATH, FA_READ);
	if (res != FR_OK)
		return;

	while (count < IR_UNIVERSAL_MAX_RECENT)
	{
		uint16_t i = 0;
		char ch;

		while (i < IR_UNIVERSAL_PATH_MAX_LEN - 1)
		{
			res = f_read(&file, &ch, 1, &br);
			if (res != FR_OK || br == 0)
			{
				if (i > 0)
				{
					line[i] = '\0';
					strncpy(s_recent[count], line, IR_UNIVERSAL_PATH_MAX_LEN - 1);
					s_recent[count][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
					count++;
				}
				goto done_recent;
			}

			if (ch == '\n' || ch == '\r')
			{
				if (i > 0)
				{
					line[i] = '\0';
					strncpy(s_recent[count], line, IR_UNIVERSAL_PATH_MAX_LEN - 1);
					s_recent[count][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
					count++;
				}
				break;
			}
			line[i++] = ch;
		}
	}

done_recent:
	s_recent_count = count;
	f_close(&file);
} // static void load_recent(void)



/*============================================================================*/
/*
 * Save recent list to SD card file
 */
/*============================================================================*/
static void save_recent(void)
{
	FIL file;
	FRESULT res;
	UINT bw;
	uint8_t i;

	/* Ensure the System directory exists */
	f_mkdir("0:/System");

	res = f_open(&file, RECENT_FILE_PATH, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK)
		return;

	for (i = 0; i < s_recent_count; i++)
	{
		f_write(&file, s_recent[i], strlen(s_recent[i]), &bw);
		f_write(&file, "\n", 1, &bw);
	}

	f_close(&file);
} // static void save_recent(void)



/*============================================================================*/
/*
 * Show the favorites screen
 */
/*============================================================================*/
static void show_favorites_screen(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t selection = 0;
	uint8_t i;

	if (s_favorite_count == 0)
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 32, "No favorites");
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 50, "Back to return");
		m1_u8g2_nextpage();

		while (1)
		{
			ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
			if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
			}
		}
	}

	/* Copy favorites to browse names, extracting just the filename */
	for (i = 0; i < s_favorite_count && i < BROWSE_NAMES_MAX; i++)
	{
		const char *fname = s_favorites[i];
		const char *p = s_favorites[i];
		while (*p)
		{
			if (*p == '/')
				fname = p + 1;
			p++;
		}
		strncpy(s_browse_names[i], fname, BROWSE_NAME_MAX_LEN - 1);
		s_browse_names[i][BROWSE_NAME_MAX_LEN - 1] = '\0';
	}
	s_browse_count = (s_favorite_count < BROWSE_NAMES_MAX) ? s_favorite_count : BROWSE_NAMES_MAX;

	draw_list_screen("Favorites", s_browse_count, selection, NULL);

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
				else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection > 0)
						selection--;
					else
						selection = s_browse_count - 1;
				}
				else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_browse_count - 1)
						selection++;
					else
						selection = 0;
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_favorite_count)
					{
						show_commands(s_favorites[selection]);
						/* Refresh display after returning */
					}
				}

				draw_list_screen("Favorites", s_browse_count, selection, NULL);
			} /* if (q_item.q_evt_type == Q_EVENT_KEYPAD) */
		} /* if (ret == pdTRUE) */
	} /* while (1) */
} // static void show_favorites_screen(void)



/*============================================================================*/
/*
 * Show the recent files screen
 */
/*============================================================================*/
static void show_recent_screen(void)
{
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t selection = 0;
	uint8_t i;

	if (s_recent_count == 0)
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 32, "No recent files");
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 50, "Back to return");
		m1_u8g2_nextpage();

		while (1)
		{
			ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
			if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
			}
		}
	}

	/* Copy recent paths to browse names, extracting just the filename */
	for (i = 0; i < s_recent_count && i < BROWSE_NAMES_MAX; i++)
	{
		const char *fname = s_recent[i];
		const char *p = s_recent[i];
		while (*p)
		{
			if (*p == '/')
				fname = p + 1;
			p++;
		}
		strncpy(s_browse_names[i], fname, BROWSE_NAME_MAX_LEN - 1);
		s_browse_names[i][BROWSE_NAME_MAX_LEN - 1] = '\0';
	}
	s_browse_count = (s_recent_count < BROWSE_NAMES_MAX) ? s_recent_count : BROWSE_NAMES_MAX;

	draw_list_screen("Recent", s_browse_count, selection, NULL);

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
				else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection > 0)
						selection--;
					else
						selection = s_browse_count - 1;
				}
				else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_browse_count - 1)
						selection++;
					else
						selection = 0;
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_recent_count)
					{
						show_commands(s_recent[selection]);
					}
				}

				draw_list_screen("Recent", s_browse_count, selection, NULL);
			} /* if (q_item.q_evt_type == Q_EVENT_KEYPAD) */
		} /* if (ret == pdTRUE) */
	} /* while (1) */
} // static void show_recent_screen(void)



/*============================================================================*/
/*
 * Custom Remote Builder — internal helpers
 * =========================================
 *
 * Template definitions: each template provides a fixed set of named button
 * slots.  The user assigns a signal to each slot via Learn, Browse, or Skip.
 */
/*============================================================================*/

/* Template IDs */
#define TMPL_TV      0
#define TMPL_AC      1
#define TMPL_AUDIO   2
#define TMPL_CUSTOM  3
#define TMPL_COUNT   4

static const char * const s_tmpl_names[TMPL_COUNT] = {
	"TV Remote",
	"AC Remote",
	"Audio Remote",
	"Custom (blank)",
};

/* TV template slots */
static const char * const s_tmpl_tv[] = {
	"Power", "Vol+", "Vol-", "Mute", "Ch+", "Ch-", "Input", "OK"
};
/* AC template slots */
static const char * const s_tmpl_ac[] = {
	"Power", "Temp+", "Temp-", "Mode", "Fan", "Swing"
};
/* Audio template slots */
static const char * const s_tmpl_audio[] = {
	"Power", "Vol+", "Vol-", "Mute", "Play", "Next", "Prev", "Input"
};
/* Custom template — generic button names */
static const char * const s_tmpl_custom[] = {
	"Btn1", "Btn2", "Btn3", "Btn4", "Btn5", "Btn6"
};

static const char * const * const s_tmpls[TMPL_COUNT] = {
	s_tmpl_tv, s_tmpl_ac, s_tmpl_audio, s_tmpl_custom
};
static const uint8_t s_tmpl_slot_counts[TMPL_COUNT] = { 8, 6, 8, 6 };


/*
* Truncate src into dst so that dst holds at most (IR_BUILDER_LABEL_MAX-1)
* visible characters and is NUL-terminated.
*/
static void builder_make_label(char *dst, const char *src)
{
	snprintf(dst, IR_BUILDER_LABEL_MAX, "%s", src);
}


/*
* Draw the builder slot list.
*   slots     — array of slot descriptors
*   n_slots   — number of slots
*   sel       — currently highlighted slot index
*/
static void builder_draw_slots(const ir_builder_slot_t *slots, uint8_t n_slots,
	uint8_t sel)
{
	const uint8_t item_h  = m1_menu_item_h();
	const uint8_t text_ofs = item_h - 1;
	const uint8_t max_vis = M1_MENU_VIS((uint8_t)n_slots);
	uint8_t scroll = 0;

	if ((max_vis < n_slots) && (sel >= max_vis))
	{
		scroll = sel - max_vis + 1;
		if (scroll > n_slots - max_vis)
			scroll = n_slots - max_vis;
	}

	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

	/* Title bar */
	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 2, 10, "Build Remote");
	u8g2_DrawHLine(&m1_u8g2, 0, 12, 128);

	u8g2_SetFont(&m1_u8g2, m1_menu_font());

	for (uint8_t i = 0; (i < max_vis) && ((scroll + i) < n_slots); i++)
	{
		uint8_t idx = scroll + i;
		uint8_t y   = LIST_START_Y + (i * item_h);

		if (idx == sel)
		{
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			u8g2_DrawBox(&m1_u8g2, 0, y, M1_MENU_TEXT_W, item_h);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
		}

		/* Slot name (left-aligned) */
		u8g2_DrawStr(&m1_u8g2, 2, y + text_ofs, slots[idx].slot_name);

		/* Assignment label (right side) */
		const char *lbl = (slots[idx].src == SLOT_SRC_NONE)
			? "(empty)" : slots[idx].label;
		u8g2_uint_t lbl_w = u8g2_GetStrWidth(&m1_u8g2, lbl);
		u8g2_DrawStr(&m1_u8g2, M1_MENU_TEXT_W - lbl_w - 2, y + text_ofs, lbl);

		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	}

	/* Scrollbar */
	if (n_slots > max_vis)
	{
		uint8_t track_y = LIST_START_Y;
		uint8_t track_h = max_vis * item_h;
		uint8_t handle_h = (track_h * max_vis) / n_slots;
		if (handle_h < 4) handle_h = 4;
		uint8_t handle_y = track_y +
			(sel * (track_h - handle_h)) / (n_slots - 1);
		u8g2_DrawFrame(&m1_u8g2, M1_MENU_SCROLLBAR_X, track_y,
			M1_MENU_SCROLLBAR_W, track_h);
		u8g2_DrawBox(&m1_u8g2, M1_MENU_SCROLLBAR_X, handle_y,
			M1_MENU_SCROLLBAR_W, handle_h);
	}

	/* Bottom hint: OK = assign, RIGHT = save */
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	m1_draw_bottom_bar(&m1_u8g2, NULL, "Assign", "Save", NULL);

	m1_u8g2_nextpage();
}


/*
* Draw the template selection screen.
*/
static void builder_draw_template_screen(uint8_t sel)
{
	const uint8_t item_h   = m1_menu_item_h();
	const uint8_t text_ofs = item_h - 1;

	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 2, 10, "Select Template");
	u8g2_DrawHLine(&m1_u8g2, 0, 12, 128);
	u8g2_SetFont(&m1_u8g2, m1_menu_font());

	for (uint8_t i = 0; i < TMPL_COUNT; i++)
	{
		uint8_t y = LIST_START_Y + (i * item_h);
		if (i == sel)
		{
			u8g2_DrawBox(&m1_u8g2, 0, y, M1_MENU_TEXT_W, item_h);
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
		}
		u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, s_tmpl_names[i]);
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	}

	m1_u8g2_nextpage();
}


/*
* Template selection screen.
* Fills slots[] and sets *n_slots.
* Returns true if the user confirmed, false if they cancelled.
*/
static bool builder_pick_template(uint8_t *n_slots, ir_builder_slot_t slots[])
{
	S_M1_Buttons_Status bs;
	S_M1_Main_Q_t q;
	uint8_t sel = 0;

	builder_draw_template_screen(sel);

	while (1)
	{
		if (xQueueReceive(main_q_hdl, &q, portMAX_DELAY) != pdTRUE)
			continue;
		if (q.q_evt_type != Q_EVENT_KEYPAD)
			continue;
		xQueueReceive(button_events_q_hdl, &bs, 0);

		if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			return false;
		}

		if (bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			sel = (sel > 0) ? sel - 1 : TMPL_COUNT - 1;
		else if (bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			sel = (sel + 1) % TMPL_COUNT;
		else if (bs.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			/* Populate slot names for chosen template */
			uint8_t tmpl_slots = s_tmpl_slot_counts[sel];
			*n_slots = (tmpl_slots <= IR_BUILDER_MAX_SLOTS) ? tmpl_slots : IR_BUILDER_MAX_SLOTS;
			memset(slots, 0, sizeof(ir_builder_slot_t) * (*n_slots));
			for (uint8_t i = 0; i < *n_slots; i++)
			{
				snprintf(slots[i].slot_name, IR_BUILDER_SLOT_NAME_MAX, "%s", s_tmpls[sel][i]);
				slots[i].src = SLOT_SRC_NONE;
			}
			xQueueReset(main_q_hdl);
			return true;
		}
		builder_draw_template_screen(sel);
	}
}


/*
* Slot action menu: Learn / Browse IRDB / Skip.
* Returns true if a signal was assigned (or skipped intentionally),
* false if user cancelled back to the slot list without changing anything.
*/
#define SLOT_ACTION_LEARN   0
#define SLOT_ACTION_BROWSE  1
#define SLOT_ACTION_SKIP    2
#define SLOT_ACTION_COUNT   3

static const char * const s_slot_action_labels[SLOT_ACTION_COUNT] = {
	"Learn (point remote)", "Browse IRDB", "Skip (leave empty)"
};

static bool builder_slot_action(ir_builder_slot_t *slot)
{
	S_M1_Buttons_Status bs;
	S_M1_Main_Q_t q;
	uint8_t asel = 0;

#define DRAW_SLOT_MENU() do { \
	const uint8_t item_h   = m1_menu_item_h(); \
	const uint8_t text_ofs = item_h - 1; \
	m1_u8g2_firstpage(); \
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); \
	u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B); \
	u8g2_DrawStr(&m1_u8g2, 2, 10, slot->slot_name); \
	u8g2_DrawHLine(&m1_u8g2, 0, 12, 128); \
	u8g2_SetFont(&m1_u8g2, m1_menu_font()); \
	for (uint8_t _i = 0; _i < SLOT_ACTION_COUNT; _i++) { \
	uint8_t _y = LIST_START_Y + (_i * item_h); \
	if (_i == asel) { \
	u8g2_DrawBox(&m1_u8g2, 0, _y, M1_MENU_TEXT_W, item_h); \
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG); \
	} \
	u8g2_DrawStr(&m1_u8g2, 4, _y + text_ofs, s_slot_action_labels[_i]); \
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT); \
	} \
	m1_u8g2_nextpage(); \
	} while (0)

	DRAW_SLOT_MENU();

	while (1)
	{
		if (xQueueReceive(main_q_hdl, &q, portMAX_DELAY) != pdTRUE)
			continue;
		if (q.q_evt_type != Q_EVENT_KEYPAD)
			continue;
		xQueueReceive(button_events_q_hdl, &bs, 0);

		if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			return false; /* User cancelled — no change */
		}
		else if (bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			asel = (asel > 0) ? asel - 1 : SLOT_ACTION_COUNT - 1;
		else if (bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			asel = (asel + 1) % SLOT_ACTION_COUNT;
		else if (bs.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			if (asel == SLOT_ACTION_LEARN)
			{
				/* Capture one signal from a physical remote */
				IRMP_DATA captured;
				if (infrared_capture_one_signal(&captured))
				{
					slot->src       = SLOT_SRC_LEARNED;
					slot->irmp_data = captured;

					/* Build display label from protocol name */
					extern const char * const irmp_protocol_names[];
					builder_make_label(slot->label,
						irmp_protocol_names[captured.protocol]);
					xQueueReset(main_q_hdl);
					return true;
				}
				/* Capture cancelled — redraw menu and let user retry */
			}
			else if (asel == SLOT_ACTION_BROWSE)
			{
				/* Browse IRDB and pick a signal */
				if (builder_browse_irdb_pick(slot))
				{
					xQueueReset(main_q_hdl);
					return true;
				}
				/* Browse cancelled or nothing chosen */
			}
			else /* SLOT_ACTION_SKIP */
			{
				slot->src = SLOT_SRC_NONE;
				slot->label[0] = '\0';
				xQueueReset(main_q_hdl);
				return true;
			}
		}
		DRAW_SLOT_MENU();
	}
#undef DRAW_SLOT_MENU
}


/*
* Show the commands from a .ir file and let the user pick one.
* On success, fills slot with IRDB source info and returns true.
*/
static bool builder_pick_signal_from_file(const char *filepath,
ir_builder_slot_t *slot)
{
	S_M1_Buttons_Status bs;
	S_M1_Main_Q_t q;
	uint16_t cmd_count;
	uint16_t pick_sel = 0;
	char title[BROWSE_NAME_MAX_LEN];

	/* Extract short filename for title */
	const char *fn = filepath;
	const char *p  = filepath;
	while (*p) { if (*p == '/') fn = p + 1; p++; }
	snprintf(title, BROWSE_NAME_MAX_LEN, "%s", fn);

	/* Parse the .ir file into s_commands[] */
	cmd_count = parse_ir_file(filepath);
	if (cmd_count == 0)
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 32, "No signals found");
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1200));
		xQueueReset(main_q_hdl);
		return false;
	}

	/* Copy command names into s_browse_names[] for the list renderer */
	uint16_t disp_count = (cmd_count < BROWSE_NAMES_MAX) ? cmd_count : BROWSE_NAMES_MAX;
	for (uint16_t i = 0; i < disp_count; i++)
	{
		snprintf(s_browse_names[i], BROWSE_NAME_MAX_LEN, "%s", s_commands[i].name);
	}
	s_browse_count = disp_count;

	draw_list_screen(title, disp_count, pick_sel, "Pick");

	while (1)
	{
		if (xQueueReceive(main_q_hdl, &q, portMAX_DELAY) != pdTRUE)
			continue;
		if (q.q_evt_type != Q_EVENT_KEYPAD)
			continue;
		xQueueReceive(button_events_q_hdl, &bs, 0);

		if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			return false;
		}
		else if (bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			pick_sel = (pick_sel > 0) ? pick_sel - 1 : disp_count - 1;
		else if (bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			pick_sel = (pick_sel + 1) % disp_count;
		else if (bs.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			if (pick_sel < cmd_count && s_commands[pick_sel].valid)
			{
				/* Store IRDB reference in the slot */
				slot->src        = SLOT_SRC_IRDB;
				slot->src_is_raw = s_commands[pick_sel].is_raw;
				snprintf(slot->src_filepath, IR_UNIVERSAL_PATH_MAX_LEN, "%s", filepath);
				snprintf(slot->src_signal_name, IR_UNIVERSAL_NAME_MAX_LEN, "%s", s_commands[pick_sel].name);
				builder_make_label(slot->label, s_commands[pick_sel].name);
				xQueueReset(main_q_hdl);
				return true;
			}
		}
		draw_list_screen(title, disp_count, pick_sel, "Pick");
	}
}


/*
* Navigate the IRDB directory tree in "pick" mode.
* When the user selects a .ir file, calls builder_pick_signal_from_file().
* Returns true if a signal was picked, false if user backed out.
*/
static bool builder_browse_irdb_pick(ir_builder_slot_t *slot)
{
	S_M1_Buttons_Status bs;
	S_M1_Main_Q_t q;
	char browse_path[IR_UNIVERSAL_PATH_MAX_LEN];
	char child_path[IR_UNIVERSAL_PATH_MAX_LEN];
	FILINFO fno;

	snprintf(browse_path, IR_UNIVERSAL_PATH_MAX_LEN, "%s", IR_UNIVERSAL_IRDB_ROOT);

	/* Save/restore the main browse globals around this sub-browse */
	char saved_browse_names[BROWSE_NAMES_MAX][BROWSE_NAME_MAX_LEN];
	uint16_t saved_browse_count = s_browse_count;
	uint16_t saved_browse_page  = s_browse_page;
	uint16_t saved_browse_sel   = s_browse_selection;
	memcpy(saved_browse_names, s_browse_names, sizeof(s_browse_names));

	s_browse_page = 0;
	while (1)
	{
		/* Scan current directory */
		s_browse_count = scan_directory_page(browse_path, s_browse_page, BROWSE_NAMES_MAX);

		if (s_browse_count == 0)
		{
			m1_u8g2_firstpage();
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
			u8g2_DrawStr(&m1_u8g2, 10, 32, "No files found");
			m1_u8g2_nextpage();
			vTaskDelay(pdMS_TO_TICKS(1000));
			xQueueReset(main_q_hdl);
			break; /* Back to caller */
		}

		s_browse_selection = 0;

		/* Extract title from path */
		const char *title = browse_path;
		const char *pp    = browse_path;
		while (*pp) { if (*pp == '/') title = pp + 1; pp++; }
		if (*title == '\0') title = "IRDB";
		draw_list_screen(title, s_browse_count, s_browse_selection, NULL);

		bool exit_dir = false;
		while (!exit_dir)
		{
			if (xQueueReceive(main_q_hdl, &q, portMAX_DELAY) != pdTRUE)
				continue;
			if (q.q_evt_type != Q_EVENT_KEYPAD)
				continue;
			xQueueReceive(button_events_q_hdl, &bs, 0);

			if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				/* Go up one level or exit if already at root */
				if (strcmp(browse_path, IR_UNIVERSAL_IRDB_ROOT) == 0)
				{
					/* Restore globals and return */
					memcpy(s_browse_names, saved_browse_names,
						sizeof(s_browse_names));
					s_browse_count     = saved_browse_count;
					s_browse_page      = saved_browse_page;
					s_browse_selection = saved_browse_sel;
					xQueueReset(main_q_hdl);
					return false;
				}
				path_go_up(browse_path);
				exit_dir = true; /* Re-scan parent */
			}
			else if (bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
			{
				s_browse_selection = (s_browse_selection > 0)
					? s_browse_selection - 1 : s_browse_count - 1;
			}
			else if (bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
			{
				s_browse_selection = (s_browse_selection < s_browse_count - 1)
					? s_browse_selection + 1 : 0;
			}
			else if (bs.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
			{
				/* Next page */
				uint16_t next_count = scan_directory_page(browse_path,
					s_browse_page + 1, BROWSE_NAMES_MAX);
				if (next_count > 0)
				{
					s_browse_page++;
					s_browse_count = next_count;
					s_browse_selection = 0;
				}
			}
			else if (bs.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
			{
				/* Previous page */
				if (s_browse_page > 0)
				{
					s_browse_page--;
					s_browse_count = scan_directory_page(browse_path,
						s_browse_page, BROWSE_NAMES_MAX);
					s_browse_selection = 0;
				}
			}
			else if (bs.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
			{
				/* Build child path */
				snprintf(child_path, IR_UNIVERSAL_PATH_MAX_LEN, "%s", browse_path);
				path_append(child_path, s_browse_names[s_browse_selection]);

				if (f_stat(child_path, &fno) == FR_OK &&
					(fno.fattrib & AM_DIR))
				{
					/* Navigate into subdirectory */
					snprintf(browse_path, IR_UNIVERSAL_PATH_MAX_LEN, "%s", child_path);
					s_browse_page = 0;
					exit_dir = true;
				}
				else if (is_ir_file(s_browse_names[s_browse_selection]))
				{
					/* Let user pick a signal from this file */
					bool picked = builder_pick_signal_from_file(child_path,
						slot);
					if (picked)
					{
						/* Restore globals and return success */
						memcpy(s_browse_names, saved_browse_names,
							sizeof(s_browse_names));
						s_browse_count     = saved_browse_count;
						s_browse_page      = saved_browse_page;
						s_browse_selection = saved_browse_sel;
						xQueueReset(main_q_hdl);
						return true;
					}
					/* User backed out of file — redraw dir listing */
					s_browse_count = scan_directory_page(browse_path,
						s_browse_page, BROWSE_NAMES_MAX);
				}
			}

			if (!exit_dir)
			{
				const char *t2 = browse_path;
				const char *pp2 = browse_path;
				while (*pp2) { if (*pp2 == '/') t2 = pp2 + 1; pp2++; }
				if (*t2 == '\0') t2 = "IRDB";
				draw_list_screen(t2, s_browse_count, s_browse_selection, NULL);
			}
		}
	}

	/* Restore globals */
	memcpy(s_browse_names, saved_browse_names, sizeof(s_browse_names));
	s_browse_count     = saved_browse_count;
	s_browse_page      = saved_browse_page;
	s_browse_selection = saved_browse_sel;
	return false;
}


/*
* Save the completed remote as a .ir signals file in IR_CUSTOM_DIR.
* The user is prompted for a filename via the virtual keyboard.
* Returns true on success.
*/
static bool builder_save_remote(ir_builder_slot_t *slots, uint8_t n_slots)
{
	char remote_name[M1_VIRTUAL_KB_FILENAME_MAX + 1];
	char out_path[IR_UNIVERSAL_PATH_MAX_LEN];
	flipper_file_t ff;
	flipper_ir_signal_t sig;
	bool write_ok = true;

	/* Ask the user for a name */
	if (!m1_vkb_get_filename("Remote name:", "MyRemote", remote_name))
		return false; /* Cancelled */

	/* Sanitize: reject characters that could escape IR_CUSTOM_DIR */
	for (const char *p = remote_name; *p; p++)
	{
		if (*p == '/' || *p == '\\' || *p == ':')
		{
			m1_u8g2_firstpage();
			u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
			u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
			u8g2_DrawStr(&m1_u8g2, 4, 30, "Invalid name");
			u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
			u8g2_DrawStr(&m1_u8g2, 4, 46, "No / \\ : allowed");
			m1_u8g2_nextpage();
			vTaskDelay(pdMS_TO_TICKS(1500));
			return false;
		}
	}
	if (strstr(remote_name, "..") != NULL)
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 4, 30, "Invalid name");
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1500));
		return false;
	}

	/* Build output path */
	snprintf(out_path, sizeof(out_path), "%s/%s%s",
	         IR_CUSTOM_DIR, remote_name, IR_FILE_EXTENSION);

	/* Open / create output file */
	if (!ff_open_write(&ff, out_path))
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 4, 30, "File create failed");
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1500));
		return false;
	}

	if (!flipper_ir_write_header(&ff))
	{
		ff_close(&ff);
		f_unlink(out_path);
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 4, 30, "File write failed");
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1500));
		return false;
	}

	/* Write each assigned slot as a signal.
	 * Cache last-parsed filepath so consecutive slots from the same source
	 * file don't redundantly re-read the SD card. */
	char last_parsed_path[IR_UNIVERSAL_PATH_MAX_LEN] = {0};
	uint16_t last_parsed_cnt = 0;

	for (uint8_t i = 0; i < n_slots && write_ok; i++)
	{
		if (slots[i].src == SLOT_SRC_NONE)
			continue; /* Skip unassigned slots */

		memset(&sig, 0, sizeof(sig));
		snprintf(sig.name, FLIPPER_IR_NAME_MAX_LEN, "%s", slots[i].slot_name);
		sig.valid = true;

		if (slots[i].src == SLOT_SRC_LEARNED)
		{
			/* Learned signal: store as parsed */
			sig.type            = FLIPPER_IR_SIGNAL_PARSED;
			sig.parsed.protocol = slots[i].irmp_data.protocol;
			sig.parsed.address  = slots[i].irmp_data.address;
			sig.parsed.command  = slots[i].irmp_data.command;
			sig.parsed.flags    = slots[i].irmp_data.flags;
			if (!flipper_ir_write_signal(&ff, &sig))
				write_ok = false;
		}
		else if (slots[i].src == SLOT_SRC_IRDB)
		{
			if (!slots[i].src_is_raw)
			{
				/* Parsed IRDB signal: re-parse from source to get data
				 * (skip if we already parsed this file for a previous slot) */
				uint16_t cnt;
				if (strcmp(last_parsed_path, slots[i].src_filepath) == 0)
				{
					cnt = last_parsed_cnt;
				}
				else
				{
					cnt = parse_ir_file(slots[i].src_filepath);
					snprintf(last_parsed_path, IR_UNIVERSAL_PATH_MAX_LEN, "%s",
						slots[i].src_filepath);
					last_parsed_cnt = cnt;
				}

				bool found = false;
				for (uint16_t j = 0; j < cnt; j++)
				{
					if (strcmp(s_commands[j].name, slots[i].src_signal_name) == 0
					    && !s_commands[j].is_raw && s_commands[j].valid)
					{
						sig.type            = FLIPPER_IR_SIGNAL_PARSED;
						sig.parsed.protocol = s_commands[j].protocol;
						sig.parsed.address  = s_commands[j].address;
						sig.parsed.command  = s_commands[j].command;
						sig.parsed.flags    = s_commands[j].flags;
						if (!flipper_ir_write_signal(&ff, &sig))
							write_ok = false;
						found = true;
						break;
					}
				}
				if (!found)
					write_ok = false;
			}
			else
			{
				/* Raw IRDB signal: read from source file via flipper_ir API */
				flipper_file_t src_ff;
				if (flipper_ir_open(&src_ff, slots[i].src_filepath))
				{
					bool found = false;
					while (flipper_ir_read_signal(&src_ff, &sig))
					{
						if (strcmp(sig.name, slots[i].src_signal_name) == 0
						    && sig.type == FLIPPER_IR_SIGNAL_RAW
						    && sig.valid)
						{
							/* Write under the slot name */
							snprintf(sig.name, FLIPPER_IR_NAME_MAX_LEN, "%s",
							         slots[i].slot_name);
							if (!flipper_ir_write_signal(&ff, &sig))
								write_ok = false;
							found = true;
							break;
						}
					}
					ff_close(&src_ff);
					if (!found)
						write_ok = false;
				}
				else
				{
					write_ok = false;
				}
			}
		}
	}

	ff_close(&ff);

	if (!write_ok)
	{
		/* Delete the incomplete file and show error */
		f_unlink(out_path);
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 4, 30, "Write failed");
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 4, 46, "Check SD card");
		m1_u8g2_nextpage();
		vTaskDelay(pdMS_TO_TICKS(1500));
		return false;
	}

	/* Show saved confirmation */
	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
	u8g2_DrawStr(&m1_u8g2, 2, 11, "Remote Saved!");
	u8g2_DrawHLine(&m1_u8g2, 0, 13, 128);
	u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 2, 25, remote_name);
	u8g2_DrawStr(&m1_u8g2, 2, 36, "Saved to IR/Custom/");
	u8g2_DrawStr(&m1_u8g2, 2, 47, "Find via Browse IRDB");
	m1_u8g2_nextpage();
	vTaskDelay(pdMS_TO_TICKS(2000));

	return true;
}



/*============================================================================*/
/*
* Case-insensitive substring search.
* Returns true if needle is found anywhere within haystack (ASCII only).
* An empty needle always matches.
*/
/*============================================================================*/
static bool str_contains_icase(const char *haystack, const char *needle)
{
	size_t hl, nl, i, j;
	char h, n;

	if (needle == NULL || needle[0] == '\0')
		return true;
	if (haystack == NULL)
		return false;

	hl = strlen(haystack);
	nl = strlen(needle);
	if (nl > hl)
		return false;

	for (i = 0; i <= hl - nl; i++)
	{
		bool match = true;
		for (j = 0; j < nl; j++)
		{
			h = haystack[i + j];
			n = needle[j];
			if (h >= 'A' && h <= 'Z')
				h = (char)(h + 32);
			if (n >= 'A' && n <= 'Z')
				n = (char)(n + 32);
			if (h != n)
			{
				match = false;
				break;
			}
		}
		if (match)
			return true;
	}
	return false;
} // static bool str_contains_icase(...)



/*============================================================================*/
/*
* Walk the IRDB directory tree (3 levels: category / brand / device) and
* collect up to IR_SEARCH_RESULTS_MAX .ir file paths whose filenames contain
* the given query string (case-insensitive).  Uses three nested DIR objects
* instead of recursion to stay stack-friendly on FreeRTOS tasks.
*
* Results are stored in s_search_results[]; s_search_count is updated.
* Caller must set s_search_count = 0 before calling.
*/
/*============================================================================*/
static void search_ir_files(const char *root, const char *query)
{
	DIR cat_dir, brand_dir, dev_dir;
	FILINFO cat_fno, brand_fno, dev_fno;
	FRESULT res;
	char cat_path[IR_UNIVERSAL_PATH_MAX_LEN];
	char brand_path[IR_UNIVERSAL_PATH_MAX_LEN];
	char file_path[IR_UNIVERSAL_PATH_MAX_LEN];

	res = f_opendir(&cat_dir, root);
	if (res != FR_OK)
		return;

	/* Level 1: categories (or files directly in root) */
	while (s_search_count < IR_SEARCH_RESULTS_MAX)
	{
		res = f_readdir(&cat_dir, &cat_fno);
		if (res != FR_OK || cat_fno.fname[0] == '\0')
			break;
		if (cat_fno.fname[0] == '.')
			continue;

		strncpy(cat_path, root, IR_UNIVERSAL_PATH_MAX_LEN - 1);
		cat_path[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
		path_append(cat_path, cat_fno.fname);

		if (!(cat_fno.fattrib & AM_DIR))
		{
			/* .ir file directly in root */
			if (is_ir_file(cat_fno.fname) && str_contains_icase(cat_fno.fname, query))
			{
				strncpy(s_search_results[s_search_count], cat_path,
					IR_UNIVERSAL_PATH_MAX_LEN - 1);
				s_search_results[s_search_count][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
				s_search_count++;
			}
			continue;
		}

		/* Level 2: brands (or files directly in category) */
		res = f_opendir(&brand_dir, cat_path);
		if (res != FR_OK)
			continue;

		while (s_search_count < IR_SEARCH_RESULTS_MAX)
		{
			res = f_readdir(&brand_dir, &brand_fno);
			if (res != FR_OK || brand_fno.fname[0] == '\0')
				break;
			if (brand_fno.fname[0] == '.')
				continue;

			strncpy(brand_path, cat_path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
			brand_path[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
			path_append(brand_path, brand_fno.fname);

			if (!(brand_fno.fattrib & AM_DIR))
			{
				/* .ir file directly in category dir */
				if (is_ir_file(brand_fno.fname) && str_contains_icase(brand_fno.fname, query))
				{
					strncpy(s_search_results[s_search_count], brand_path,
						IR_UNIVERSAL_PATH_MAX_LEN - 1);
					s_search_results[s_search_count][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
					s_search_count++;
				}
				continue;
			}

			/* Level 3: device .ir files */
			res = f_opendir(&dev_dir, brand_path);
			if (res != FR_OK)
				continue;

			while (s_search_count < IR_SEARCH_RESULTS_MAX)
			{
				res = f_readdir(&dev_dir, &dev_fno);
				if (res != FR_OK || dev_fno.fname[0] == '\0')
					break;
				if (dev_fno.fname[0] == '.')
					continue;

				if (is_ir_file(dev_fno.fname) && str_contains_icase(dev_fno.fname, query))
				{
					strncpy(file_path, brand_path, IR_UNIVERSAL_PATH_MAX_LEN - 1);
					file_path[IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
					path_append(file_path, dev_fno.fname);
					strncpy(s_search_results[s_search_count], file_path,
						IR_UNIVERSAL_PATH_MAX_LEN - 1);
					s_search_results[s_search_count][IR_UNIVERSAL_PATH_MAX_LEN - 1] = '\0';
					s_search_count++;
				}
			}
			f_closedir(&dev_dir);
		}
		f_closedir(&brand_dir);
	}
	f_closedir(&cat_dir);
} // static void search_ir_files(...)


/*============================================================================*/
/*
* Main entry point for the custom remote builder.
* Called from dashboard_screen() when "Create Remote" is selected.
*
* Flow:
*   1. Pick a template (TV / AC / Audio / Custom)
*   2. Assign a signal to each slot (Learn / Browse / Skip)
*   3. Press RIGHT to save → VKB name → write .ir file
*/
/*============================================================================*/
static void ir_custom_builder_run(void)
{
	S_M1_Buttons_Status bs;
	S_M1_Main_Q_t q;
	uint8_t slot_sel = 0;

	/* Step 1: choose a template */
	if (!builder_pick_template(&s_builder_n_slots, s_builder_slots))
		return; /* User cancelled */

	/* Step 2: assign signals to slots */
	builder_draw_slots(s_builder_slots, s_builder_n_slots, slot_sel);

	while (1)
	{
		if (xQueueReceive(main_q_hdl, &q, portMAX_DELAY) != pdTRUE)
			continue;
		if (q.q_evt_type != Q_EVENT_KEYPAD)
			continue;
		xQueueReceive(button_events_q_hdl, &bs, 0);

		if (bs.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			xQueueReset(main_q_hdl);
			return; /* Abandon builder */
		}
		else if (bs.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
		{
			slot_sel = (slot_sel > 0) ? slot_sel - 1 : s_builder_n_slots - 1;
		}
		else if (bs.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
		{
			slot_sel = (slot_sel + 1) % s_builder_n_slots;
		}
		else if (bs.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
		{
			/* Assign a signal to the selected slot */
			builder_slot_action(&s_builder_slots[slot_sel]);
		}
		else if (bs.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
		{
			/* Save the remote and only leave the builder on success. */
			if (builder_save_remote(s_builder_slots, s_builder_n_slots))
			{
				xQueueReset(main_q_hdl);
				return;
			}
		}

		builder_draw_slots(s_builder_slots, s_builder_n_slots, slot_sel);
	}
} // static void ir_custom_builder_run(void)



/*============================================================================*/
/*
 * Show the IRDB search screen.
 * Prompts for a query via the virtual keyboard, walks the IRDB tree, and
 * displays matching .ir file names in a scrollable list.  Selecting a result
 * opens its command list via show_commands() and records it in recent history.
 */
/*============================================================================*/
static void show_search_screen(void)
{
	char query[M1_VIRTUAL_KB_FILENAME_MAX + 1];
	S_M1_Buttons_Status this_button_status;
	S_M1_Main_Q_t q_item;
	BaseType_t ret;
	uint16_t selection = 0;
	uint8_t i;
	uint8_t qlen;

	/* Get search query from the virtual keyboard */
	query[0] = '\0';
	qlen = m1_vkb_get_filename("Search IRDB", "", query);

	/* Return to dashboard if user cancelled without typing anything */
	if (qlen == 0)
		return;

	/* Show a "Searching…" screen while the IRDB tree walk runs.
	 * The scan is synchronous and may take a moment on large databases. */
	m1_u8g2_firstpage();
	u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
	u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
	u8g2_DrawStr(&m1_u8g2, 10, 32, "Searching...");
	m1_u8g2_nextpage();

	/* Walk the IRDB tree collecting matching .ir file paths */
	s_search_count = 0;
	search_ir_files(IR_UNIVERSAL_IRDB_ROOT, query);

	/* No results found */
	if (s_search_count == 0)
	{
		m1_u8g2_firstpage();
		u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
		u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 32, "No results");
		u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
		u8g2_DrawStr(&m1_u8g2, 10, 50, "Back to return");
		m1_u8g2_nextpage();

		while (1)
		{
			ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
			if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				xQueueReceive(button_events_q_hdl, &this_button_status, 0);
				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
			}
		}
	}

	/* Populate browse names with the filename portion of each result path */
	for (i = 0; i < s_search_count && i < BROWSE_NAMES_MAX; i++)
	{
		const char *fname = s_search_results[i];
		const char *p = s_search_results[i];
		while (*p)
		{
			if (*p == '/')
				fname = p + 1;
			p++;
		}
		strncpy(s_browse_names[i], fname, BROWSE_NAME_MAX_LEN - 1);
		s_browse_names[i][BROWSE_NAME_MAX_LEN - 1] = '\0';
	}
	s_browse_count = (s_search_count < BROWSE_NAMES_MAX) ? s_search_count : BROWSE_NAMES_MAX;

	draw_list_screen("Results", s_browse_count, selection, NULL);

	while (1)
	{
		ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
		if (ret == pdTRUE)
		{
			if (q_item.q_evt_type == Q_EVENT_KEYPAD)
			{
				ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

				if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					xQueueReset(main_q_hdl);
					return;
				}
				else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection > 0)
						selection--;
					else
						selection = s_browse_count - 1;
				}
				else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_browse_count - 1)
						selection++;
					else
						selection = 0;
				}
				else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
				{
					if (selection < s_search_count)
					{
						add_to_recent(s_search_results[selection]);
						show_commands(s_search_results[selection]);
						/* Rebuild browse names — show_commands() clobbers the shared buffer */
						for (i = 0; i < s_search_count && i < BROWSE_NAMES_MAX; i++)
						{
							const char *fn = s_search_results[i];
							const char *q = s_search_results[i];
							while (*q)
							{
								if (*q == '/')
									fn = q + 1;
								q++;
							}
							strncpy(s_browse_names[i], fn, BROWSE_NAME_MAX_LEN - 1);
							s_browse_names[i][BROWSE_NAME_MAX_LEN - 1] = '\0';
						}
						s_browse_count = (s_search_count < BROWSE_NAMES_MAX)
						                 ? s_search_count : BROWSE_NAMES_MAX;
					}
				}

				draw_list_screen("Results", s_browse_count, selection, NULL);
			} /* if (q_item.q_evt_type == Q_EVENT_KEYPAD) */
		} /* if (ret == pdTRUE) */
	} /* while (1) */
} // static void show_search_screen()
