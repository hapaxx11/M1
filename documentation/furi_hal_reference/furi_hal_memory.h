#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriHalMemoryHeapTrackModeNone,
    FuriHalMemoryHeapTrackModeCount,
    FuriHalMemoryHeapTrackModeTree,
    FuriHalMemoryHeapTrackModeAll,
} FuriHalMemoryHeapTrackMode;

typedef struct {
    void* start;
    size_t size_bytes;
} FuriHalMemoryRegion;

void furi_hal_memory_init(void);
void* furi_hal_memory_alloc(size_t size);
size_t furi_hal_memory_get_free(void);
size_t furi_hal_memory_max_pool_block(void);
uint32_t furi_hal_memory_get_region_count(void);
const FuriHalMemoryRegion* furi_hal_memory_get_region(uint32_t index);
void furi_hal_memory_set_heap_track_mode(FuriHalMemoryHeapTrackMode mode);
FuriHalMemoryHeapTrackMode furi_hal_memory_get_heap_track_mode(void);

#ifdef __cplusplus
}
#endif
