/* Minimal t5577.h stub for host-side unit tests. */
#ifndef T5577_H_
#define T5577_H_

#include <stdint.h>
#include <stdbool.h>

#define T5577_BLOCK_COUNT 8
#define T5577_BLOCKS_IN_PAGE_0 8
#define T5577_BLOCKS_IN_PAGE_1 4

typedef struct {
    uint32_t bsrr;
    uint16_t time;
} LFRFIDT5577WriteCtrl;

typedef struct {
    uint32_t block_data[T5577_BLOCK_COUNT];
    uint32_t max_blocks;
    LFRFIDT5577WriteCtrl write_frame[85];
} LFRFIDT5577;

typedef enum {
    LFRFIDProgramTypeT5577,
} LFRFIDProgramType;

typedef struct {
    LFRFIDProgramType type;
    union {
        LFRFIDT5577 t5577;
    };
} LFRFIDProgram;

#endif /* T5577_H_ */
