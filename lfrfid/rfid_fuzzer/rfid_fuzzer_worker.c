#include "rfid_fuzzer_worker.h"
#include <furi.h>
#include <lfrfid/lfrfid_worker.h>
#include <toolbox/protocols/protocol_dict.h>

typedef struct {
    FuriThread* thread;
    RFIDFuzzerApp* app;
    bool running;
} RFIDFuzzerWorker;

// This runs in the background
static int32_t rfid_fuzzer_worker_thread(void* context) {
    RFIDFuzzerWorker* worker = context;
    
    // List of common RFID protocols
    const char* protocols[] = {
        "EM4100", "H10301", "Indala", "IOProx", "T55xx", "EM4305"
    };
    
    uint32_t test_uid = 0;
    
    FURI_LOG_I("RFIDWorker", "Fuzzing started!");
    
    while(worker->running) {
        // Try a new UID
        test_uid++;
        
        // Format as EM4100 (10-bit facility + 16-bit card)
        uint32_t facility = (test_uid >> 16) & 0x3FF;
        uint16_t card = test_uid & 0xFFFF;
        
        FURI_LOG_D("RFIDWorker", "Trying Facility: %lu, Card: %u", facility, card);
        
        // Send the UID (simplified - real implementation would use lfrfid_worker)
        furi_delay_ms(100); // Wait between attempts
    }
    
    return 0;
}

RFIDFuzzerWorker* rfid_fuzzer_worker_alloc(RFIDFuzzerApp* app) {
    RFIDFuzzerWorker* worker = malloc(sizeof(RFIDFuzzerWorker));
    worker->app = app;
    worker->running = false;
    worker->thread = furi_thread_alloc();
    
    furi_thread_set_name(worker->thread, "RFIDFuzzerWorker");
    furi_thread_set_stack_size(worker->thread, 2048);
    furi_thread_set_context(worker->thread, worker);
    furi_thread_set_callback(worker->thread, rfid_fuzzer_worker_thread);
    
    return worker;
}

void rfid_fuzzer_worker_start(RFIDFuzzerWorker* worker) {
    furi_assert(worker);
    worker->running = true;
    furi_thread_start(worker->thread);
}

void rfid_fuzzer_worker_stop(RFIDFuzzerWorker* worker) {
    furi_assert(worker);
    worker->running = false;
    furi_thread_join(worker->thread);
}

void rfid_fuzzer_worker_free(RFIDFuzzerWorker* worker) {
    furi_assert(worker);
    furi_thread_free(worker->thread);
    free(worker);
}