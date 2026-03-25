#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriHalNvmStorageOK,
    FuriHalNvmStorageItemNotFound,
    FuriHalNvmStorageError,
} FuriHalNvmStorage;

typedef enum {
    FuriHalRtcBootModeDummy,
} FuriHalNvmBootMode;

FuriHalNvmBootMode furi_hal_nvm_get_boot_mode(void);
void furi_hal_nvm_set_fault_data(uint32_t value);
void furi_hal_nvm_init(void);
void furi_hal_nvm_deinit(void);
FuriHalNvmStorage furi_hal_nvm_delete(const char* key);
FuriHalNvmStorage furi_hal_nvm_set_str(const char* key, FuriString* value);
FuriHalNvmStorage furi_hal_nvm_get_str(const char* key, FuriString* value);
FuriHalNvmStorage furi_hal_nvm_set_uint32(const char* key, uint32_t value);
FuriHalNvmStorage furi_hal_nvm_get_uint32(const char* key, uint32_t* value);
FuriHalNvmStorage furi_hal_nvm_set_int32(const char* key, int32_t value);
FuriHalNvmStorage furi_hal_nvm_get_int32(const char* key, int32_t* value);
FuriHalNvmStorage furi_hal_nvm_set_bool(const char* key, bool value);
FuriHalNvmStorage furi_hal_nvm_get_bool(const char* key, bool* value);

#ifdef __cplusplus
}
#endif
