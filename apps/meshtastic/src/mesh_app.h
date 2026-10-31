/*
 * mesh_app.h — M1 Meshtastic app state.
 *
 * Ported from ZeroMesh's zeromesh_serial.h. Flipper-only members (Gui,
 * ViewPort, Bt, FuriThread, FuriStreamBuffer, the offline map) are gone; the
 * transport is the M1 header UART only, and the app runs as one cooperative
 * task so the RX-thread scaffolding is dropped. The Meshtastic decode state,
 * roster, message history and node-config mirror the original verbatim.
 */
#pragma once

#include "compat.h"
#include "meshtastic_framing.h"

#include "lib/nanopb/pb.h"
#include "lib/nanopb/pb_encode.h"
#include "lib/nanopb/pb_decode.h"

#include "lib/meshtastic_api/meshtastic/mesh.pb.h"
#include "lib/meshtastic_api/meshtastic/config.pb.h"
#include "lib/meshtastic_api/meshtastic/portnums.pb.h"

#define LOG_LINES 18
#define LOG_COLS 64

#define PAYLOAD_CAPTURE_MAX 256

/* Pages (the offline vector Map is deferred from v1). */
#define PAGE_MESSAGES 0
#define PAGE_ROSTER 1
#define PAGE_STATS 2
#define PAGE_SIGNAL 3
#define PAGE_LOGS 4
#define PAGE_SETTINGS 5
#define PAGE_NODECFG 6
#define PAGE_COUNT 7

#define MSG_HISTORY 8
#define ROSTER_MAX_NODES 16
#define MAX_CHANNELS 8

/* Settings persistence lives on the SD card. */
#define SETTINGS_DIR "0:/apps_data/zeromesh"
#define SETTINGS_PATH "0:/apps_data/zeromesh/settings.cfg"

#define RINGTONE_CUSTOM_MAX 8
#define RINGTONE_NAME_MAX 32

typedef enum {
    PendingNone = 0,
    PendingPosReq,
    PendingInfoReq,
    PendingSetLora,
    PendingSetRole,
    PendingSetGps,
    PendingGetChannel,
    PendingSetChannel,
    PendingSetFixed,
    PendingClearFixed,
    PendingReboot,
    PendingInfoAll,
    PendingSendText,
    PendingPlayTone,
} PendingAction;

typedef enum {
    RosterStateList = 0,
    RosterStateChat,
    RosterStateDetails,
} RosterState;

typedef struct {
    uint32_t node_id;
    uint32_t last_seen;
    float last_snr;
    int16_t last_rssi;
    uint8_t battery_level;
    float voltage;
    bool has_telemetry;

    /* Position as Meshtastic sends it: degrees * 1e7, signed. */
    int32_t latitude_i;
    int32_t longitude_i;
    int32_t altitude;
    uint32_t pos_time;
    bool has_position;
    uint8_t sats;
    bool sats_seen;
    bool has_fix;

    char short_name[8];
    char long_name[24];
    bool has_name;
    bool has_new_dm;
} NodeEntry;

typedef struct {
    NodeEntry nodes[ROSTER_MAX_NODES];
    uint8_t count;
    uint8_t selected_idx;
    RosterState state;
    uint8_t chat_scroll;
} NodeRoster;

typedef struct {
    char text[128];
    uint32_t from;
    uint32_t to;
    bool is_tx;
    uint32_t timestamp;
} Message;

typedef struct {
    Message msgs[MSG_HISTORY];
    uint8_t head;
    uint8_t count;
} MessageHistory;

typedef enum {
    RingtoneNone = 0,
    RingtoneShort,
    RingtoneDouble,
    RingtoneTriple,
    RingtoneLong,
    RingtoneSOS,
    RingtoneChirp,
    RingtoneNokia,
    RingtoneDescend,
    RingtoneBounce,
    RingtoneAlert,
    RingtonePulse,
    RingtoneSiren,
    RingtoneBeep3,
    RingtoneTrill,
    RingtoneMario,
    RingtoneLevelUp,
    RingtoneMetric,
    RingtoneMinimalist,
    RINGTONE_COUNT,
} RingtoneType;

typedef enum {
    LMH_Scroll = 0,
    LMH_Wrap,
    LMH_COUNT,
} LongMessageHandling;

/* Settings rows. The Flipper build also had Transport (UART/BLE) and UART port
 * (USART/LPUART) rows; the M1 has one header UART and no BLE transport in v1,
 * so those are gone. */
typedef enum {
    SettingBaud = 0,
    SettingVibro,
    SettingLed,
    SettingRingtone,
    SettingScrollSpeed,
    SettingScrollFramerate,
    SettingLMH,
    SETTING_COUNT,
} SettingItem;

typedef struct {
    FuriMutex lock; /* unused (single task); kept so shim macros compile */

    uint32_t baud;
    bool uart_up;

    /* Framing state, fed byte-by-byte from the UART in the main loop. */
    meshtastic_framing_t framing;

    uint32_t rx_bytes;
    uint32_t rx_frames_ok;
    uint32_t rx_bad_magic;
    uint32_t rx_bad_len;
    uint32_t rx_decode_fail;
    uint32_t tx_frames;
    uint32_t tx_encode_fail;

    char lines[LOG_LINES][LOG_COLS];
    uint8_t line_head;
    char status[LOG_COLS];

    MessageHistory history;

    uint8_t ui_mode;
    uint8_t msg_scroll_offset;

    bool log_paused;
    uint8_t log_scroll_offset;

    char last_rx_text[128];
    uint32_t last_rx_from;
    uint32_t last_rx_to;
    uint32_t last_rx_id;
    float last_rx_snr;
    int16_t last_rx_rssi;
    bool has_rx_signal_data;

    uint8_t my_sats;
    bool my_sats_seen;
    bool my_has_fix;

    uint32_t my_node_num;

    bool has_node_config;
    uint8_t cfg_region;
    uint8_t cfg_preset;
    uint8_t cfg_role;
    uint8_t cfg_gps;
    uint8_t cfg_fixed;
    uint8_t cfg_ch_private;
    uint8_t cfg_ch_pos;
    uint8_t cfg_ch_psk_len;
    uint8_t cfg_ch_psk[32];
    char cfg_ch_name[16];
    bool cfg_ch_known;
    bool cfg_pos_known;
    meshtastic_Config_PositionConfig cfg_pos;
    char my_long_name[24];
    char my_short_name[8];
    uint8_t nodecfg_cursor;
    bool nodecfg_editing;

    volatile bool need_render;
    volatile bool pending_notify;
    volatile PendingAction pending_action;
    uint32_t pending_node;
    uint8_t pending_a;
    uint8_t pending_b;
    char pending_text[64];

    uint32_t sent_msg_ids[8];
    uint8_t sent_msg_head;

    uint8_t settings_cursor;
    bool settings_editing;

    bool notify_vibro;
    bool notify_led;
    uint16_t notify_ringtone;
    char custom_files[RINGTONE_CUSTOM_MAX][RINGTONE_NAME_MAX];
    uint8_t custom_count;

    uint8_t scroll_speed;
    uint8_t scroll_framerate;
    LongMessageHandling lmh_mode;

    uint8_t current_channel;
    uint8_t num_channels;

    bool show_keyboard;
    char text_buffer[64];

    bool stop_thread; /* set to request app exit */

    NodeRoster roster;
} ZeroMeshApp;
