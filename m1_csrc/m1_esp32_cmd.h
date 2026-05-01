/* See COPYING.txt for license details. */

/*
 * m1_esp32_cmd.h
 *
 * Binary SPI command protocol for STM32 <-> ESP32-C6 communication.
 * Replaces the old AT-command-over-SPI interface.
 *
 * M1 Project
 */

#ifndef M1_ESP32_CMD_H_
#define M1_ESP32_CMD_H_

#include <stdint.h>

/* ---- Packet framing ---- */
#define M1_CMD_MAGIC   0xAB
#define M1_RESP_MAGIC  0xCD

#define M1_MAX_PAYLOAD 60

/* ---- Command IDs ---- */

/* System */
#define CMD_PING              0x01
#define CMD_GET_STATUS        0x02

/* WiFi scan */
#define CMD_WIFI_SCAN_START   0x10
#define CMD_WIFI_SCAN_NEXT    0x11
#define CMD_WIFI_SCAN_STOP    0x12

/* Station scan (promiscuous client discovery) */
#define CMD_STA_SCAN_START    0x13
#define CMD_STA_SCAN_NEXT     0x14
#define CMD_STA_SCAN_STOP     0x15

/* BLE */
#define CMD_BLE_SCAN_START    0x20
#define CMD_BLE_SCAN_NEXT     0x21
#define CMD_BLE_SCAN_STOP     0x22
#define CMD_BLE_ADV_START     0x23
#define CMD_BLE_ADV_STOP      0x24
#define CMD_BLE_ADV_RAW       0x25
#define CMD_BLE_SCAN_NEXT_RAW 0x26

/* WiFi attacks */
#define CMD_DEAUTH_START      0x30
#define CMD_DEAUTH_STOP       0x31
#define CMD_BEACON_START      0x32
#define CMD_BEACON_STOP       0x33
#define CMD_PROBE_FLOOD_START 0x34
#define CMD_PROBE_FLOOD_STOP  0x35
#define CMD_BEACON_SET_FLAGS  0x36
#define CMD_KARMA_START       0x37
#define CMD_KARMA_STOP        0x38
#define CMD_DEAUTH_MULTI      0x39
#define CMD_KARMA_STATUS      0x3A
#define CMD_KARMA_PORTAL_START 0x3B

/* Packet monitor / sniffer */
#define CMD_PKTMON_START      0x40
#define CMD_PKTMON_NEXT       0x41
#define CMD_PKTMON_STOP       0x42
#define CMD_PKTMON_SET_CHAN   0x43

/* Sniffer type constants (pktmon_start payload[0]) */
#define SNIFF_ALL             0x00
#define SNIFF_BEACON          0x01
#define SNIFF_PROBE_REQ       0x02
#define SNIFF_DEAUTH          0x03
#define SNIFF_EAPOL           0x04
#define SNIFF_SIGNAL          0x05
#define SNIFF_PWNAGOTCHI      0x06
#define SNIFF_SAE             0x07

/* Evil portal */
#define CMD_PORTAL_START      0x50
#define CMD_PORTAL_STOP       0x51
#define CMD_PORTAL_CREDS      0x52
#define CMD_PORTAL_HTML_CLEAR 0x56
#define CMD_PORTAL_HTML_ADD   0x57

/* SSID management */
#define CMD_SSID_ADD          0x53
#define CMD_SSID_CLEAR        0x54
#define CMD_SSID_COUNT        0x55

/* WiFi general */
#define CMD_WIFI_JOIN         0x58
#define CMD_WIFI_DISCONNECT   0x59
#define CMD_WIFI_SET_MAC      0x5A
#define CMD_WIFI_SET_CHANNEL  0x5B
#define CMD_NETSCAN_START     0x5C
#define CMD_NETSCAN_NEXT      0x5D
#define CMD_NETSCAN_STOP      0x5E

/* ---- Response status ---- */
#define RESP_OK    0x00
#define RESP_ERR   0x01
#define RESP_BUSY  0x02

/* ---- Packet structures (64 bytes each) ---- */

/* Command: STM32 -> ESP32 */
typedef struct {
    uint8_t  magic;           /* M1_CMD_MAGIC (0xAB) */
    uint8_t  cmd_id;
    uint8_t  payload_len;
    uint8_t  payload[61];
} __attribute__((packed)) m1_cmd_t;

/* Response: ESP32 -> STM32 */
typedef struct {
    uint8_t  magic;           /* M1_RESP_MAGIC (0xCD) */
    uint8_t  cmd_id;
    uint8_t  status;
    uint8_t  payload_len;
    uint8_t  payload[M1_MAX_PAYLOAD];
} __attribute__((packed)) m1_resp_t;

/* ---- API ---- */

/**
 * Send a command to the ESP32 and receive the response.
 * @param cmd  Pointer to the command to send (64 bytes)
 * @param resp Pointer to buffer for the response (64 bytes)
 * @param timeout_ms Maximum time to wait for response (ms)
 * @return 0 on success, -1 on SPI error, -2 on timeout, -3 on bad response magic
 */
int m1_esp32_send_cmd(const m1_cmd_t *cmd, m1_resp_t *resp, uint32_t timeout_ms);

/**
 * Build and send a simple command with no payload.
 * @param cmd_id  The command ID
 * @param resp    Pointer to buffer for the response
 * @param timeout_ms Maximum time to wait for response (ms)
 * @return 0 on success, negative on error
 */
int m1_esp32_simple_cmd(uint8_t cmd_id, m1_resp_t *resp, uint32_t timeout_ms);

#endif /* M1_ESP32_CMD_H_ */
