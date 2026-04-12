/* Minimal LFRFIDProtocol enum stub for host-side unit tests.
 * Mirrors the protocol enum from lfrfid/lfrfid_protocol.h. */

#ifndef LFRFID_PROTOCOL_STUB_H
#define LFRFID_PROTOCOL_STUB_H

typedef enum {
    LFRFIDProtocolEM4100,
    LFRFIDProtocolEM4100_32,
    LFRFIDProtocolEM4100_16,
    LFRFIDProtocolH10301,
    LFRFIDProtocolHIDGeneric,
    LFRFIDProtocolIndala26,
    /* FSK protocols */
    LFRFIDProtocolAWID,
    LFRFIDProtocolPyramid,
    LFRFIDProtocolParadox,
    LFRFIDProtocolIoProxXSF,
    LFRFIDProtocolFDX_A,
    LFRFIDProtocolHIDExGeneric,
    /* Manchester protocols */
    LFRFIDProtocolViking,
    LFRFIDProtocolFDX_B,
    LFRFIDProtocolElectra,
    LFRFIDProtocolGallagher,
    LFRFIDProtocolJablotron,
    LFRFIDProtocolPACStanley,
    LFRFIDProtocolSecurakey,
    LFRFIDProtocolGProxII,
    LFRFIDProtocolNoralsy,
    LFRFIDProtocolIdteck,
    /* PSK protocols */
    LFRFIDProtocolKeri,
    LFRFIDProtocolNexwatch,
    /* Extended protocols (Flipper-ported) */
    LFRFIDProtocolIndala224,
    LFRFIDProtocolInstaFob,
    LFRFIDProtocolMax,
} LFRFIDProtocol;

#endif /* LFRFID_PROTOCOL_STUB_H */
