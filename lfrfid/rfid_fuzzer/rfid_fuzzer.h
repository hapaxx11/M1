#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct RFIDFuzzerApp RFIDFuzzerApp;

// Function declarations
RFIDFuzzerApp* rfid_fuzzer_app_alloc();
void rfid_fuzzer_app_free(RFIDFuzzerApp* app);
int32_t rfid_fuzzer_app(void* p);