#include "meshtastic_framing.h"

void meshtastic_framing_reset(meshtastic_framing_t* framing) {
    framing->hdr_pos = 0;
    framing->frame_len = 0;
    framing->frame_pos = 0;
}

meshtastic_frame_result_t meshtastic_framing_feed(meshtastic_framing_t* framing, uint8_t byte) {
    if(framing->hdr_pos < 4) {
        framing->hdr[framing->hdr_pos++] = byte;
        if(framing->hdr_pos == 1 && framing->hdr[0] != ZEROMESH_MAGIC0) {
            framing->hdr_pos = 0;
            return MESHTASTIC_FRAME_BAD_MAGIC;
        }
        if(framing->hdr_pos == 2 && framing->hdr[1] != ZEROMESH_MAGIC1) {
            framing->hdr_pos = (byte == ZEROMESH_MAGIC0) ? 1 : 0;
            framing->hdr[0] = byte;
            return MESHTASTIC_FRAME_BAD_MAGIC;
        }
        if(framing->hdr_pos == 4) {
            framing->frame_len = ((uint16_t)framing->hdr[2] << 8) | framing->hdr[3];
            framing->frame_pos = 0;
            if(framing->frame_len == 0 || framing->frame_len > MAX_FRAME_SIZE) {
                meshtastic_framing_reset(framing);
                return MESHTASTIC_FRAME_BAD_LENGTH;
            }
        }
        return MESHTASTIC_FRAME_PENDING;
    }

    framing->frame_buf[framing->frame_pos++] = byte;
    return framing->frame_pos == framing->frame_len ? MESHTASTIC_FRAME_COMPLETE :
                                                      MESHTASTIC_FRAME_PENDING;
}
