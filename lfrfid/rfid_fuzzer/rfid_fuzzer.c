#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <lfrfid/lfrfid_worker.h>
#include <toolbox/name_generator.h>

#define TAG "RFIDFuzzer"

// App states
typedef enum {
    AppStateMainMenu,
    AppStateSettings,
    AppStateFuzzing
} AppState;

// App structure to hold all our data
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    VariableItemList* settings_list;
    LFRFIDWorker* rfid_worker;
    AppState current_state;
    uint32_t current_uid;
    bool fuzzing_active;
} RFIDFuzzerApp;

// This is where the magic starts!
int32_t rfid_fuzzer_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "RFID Fuzzer started!");
    
    // Create app structure
    RFIDFuzzerApp* app = malloc(sizeof(RFIDFuzzerApp));
    
    // Initialize basic stuff
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->current_state = AppStateMainMenu;
    app->fuzzing_active = false;
    app->current_uid = 0;
    
    // Create main menu
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Start Fuzzing", 0, NULL, app);
    submenu_add_item(app->submenu, "Settings", 1, NULL, app);
    submenu_add_item(app->submenu, "Exit", 2, NULL, app);
    
    // Show the main menu
    view_dispatcher_add_view(app->view_dispatcher, 0, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    view_dispatcher_run(app->view_dispatcher);
    
    FURI_LOG_I(TAG, "RFID Fuzzer exiting!");
    return 0;
}