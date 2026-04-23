/**
 * m1-rpc.js
 *
 * JavaScript implementation of the M1 RPC binary protocol.
 *
 * Frame format:
 *   [0xAA] [CMD:1] [SEQ:1] [LEN:2 LE] [PAYLOAD:0-8192] [CRC16:2]
 *
 * CRC-16/CCITT: polynomial 0x1021, initial value 0xFFFF.
 * CRC is computed over CMD + SEQ + LEN + PAYLOAD (all bytes after sync).
 *
 * @license See COPYING.txt for license details.
 */

/* ── RPC Constants ── */

export const RPC_SYNC_BYTE           = 0xAA;
export const RPC_HEADER_SIZE         = 5;   // SYNC + CMD + SEQ + LEN(2)
export const RPC_CRC_SIZE            = 2;
export const RPC_MAX_PAYLOAD         = 8192;

/* System Commands */
export const RPC_CMD_PING            = 0x01;
export const RPC_CMD_PONG            = 0x02;
export const RPC_CMD_GET_DEVICE_INFO = 0x03;
export const RPC_CMD_DEVICE_INFO_RESP = 0x04;
export const RPC_CMD_REBOOT          = 0x05;
export const RPC_CMD_ACK             = 0x06;
export const RPC_CMD_NACK            = 0x07;

/* File / SD Commands */
export const RPC_CMD_FILE_WRITE_START  = 0x34;
export const RPC_CMD_FILE_WRITE_DATA   = 0x35;
export const RPC_CMD_FILE_WRITE_FINISH = 0x36;
export const RPC_CMD_FILE_MKDIR        = 0x38;
export const RPC_CMD_SD_UNMOUNT        = 0x3B;
export const RPC_CMD_SD_MOUNT          = 0x3C;

/* Firmware Commands */
export const RPC_CMD_FW_INFO         = 0x40;
export const RPC_CMD_FW_INFO_RESP    = 0x41;
export const RPC_CMD_FW_UPDATE_START = 0x42;
export const RPC_CMD_FW_UPDATE_DATA  = 0x43;
export const RPC_CMD_FW_UPDATE_FINISH = 0x44;
export const RPC_CMD_FW_BANK_SWAP    = 0x45;

/* NACK Error Codes */
export const RPC_ERRORS = {
    0x01: 'Unknown command',
    0x02: 'Invalid payload',
    0x03: 'Device busy',
    0x04: 'SD card not ready',
    0x05: 'File not found',
    0x07: 'Flash error',
    0x08: 'CRC mismatch',
    0x09: 'Size too large',
    0x0A: 'Bank empty',
};

/* ── CRC-16/CCITT Lookup Table ── */

const CRC16_TABLE = new Uint16Array([
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x4864, 0x5845, 0x6826, 0x7807, 0x08E0, 0x18C1, 0x28A2, 0x38C3,
    0xC92C, 0xD90D, 0xE96E, 0xF94F, 0x89A8, 0x9989, 0xA9EA, 0xB9CB,
    0x5A15, 0x4A34, 0x7A57, 0x6A76, 0x1A91, 0x0AB0, 0x3AD3, 0x2AF2,
    0xDB1D, 0xCB3C, 0xFB5F, 0xEB7E, 0x9B99, 0x8BB8, 0xBBDB, 0xABFA,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBBBA,
    0x4A55, 0x5A74, 0x6A17, 0x7A36, 0x0AD1, 0x1AF0, 0x2A93, 0x3AB2,
    0xFD0E, 0xED2F, 0xDD4C, 0xCD6D, 0xBD8A, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
]);

/**
 * Compute CRC-16/CCITT over a Uint8Array.
 *
 * Table-driven implementation: for each byte, the upper 8 bits of the
 * running CRC are XOR'd with the input byte to form a table index.
 * The CRC is then shifted left by 8 and XOR'd with the table entry.
 * This is identical to the firmware's rpc_crc16() in m1_rpc.c.
 *
 * @param {Uint8Array} data
 * @param {number} [init=0xFFFF] - Initial CRC value (for continuation)
 * @returns {number} 16-bit CRC
 */
export function crc16(data, init = 0xFFFF) {
    let crc = init;
    for (let i = 0; i < data.length; i++) {
        const idx = ((crc >>> 8) ^ data[i]) & 0xFF;
        crc = ((crc << 8) ^ CRC16_TABLE[idx]) & 0xFFFF;
    }
    return crc;
}

/* ── Frame Builder ── */

/**
 * Build an RPC frame as a Uint8Array.
 *
 * @param {number} cmd     - Command byte
 * @param {number} seq     - Sequence number (0-255)
 * @param {Uint8Array|null} payload - Payload data (may be null/empty)
 * @returns {Uint8Array} Complete frame ready to send
 */
export function buildFrame(cmd, seq, payload = null) {
    const payloadLen = payload ? payload.length : 0;
    const frameSize = RPC_HEADER_SIZE + payloadLen + RPC_CRC_SIZE;
    const frame = new Uint8Array(frameSize);

    let idx = 0;
    frame[idx++] = RPC_SYNC_BYTE;
    frame[idx++] = cmd;
    frame[idx++] = seq;
    frame[idx++] = payloadLen & 0xFF;         // LEN low
    frame[idx++] = (payloadLen >>> 8) & 0xFF;  // LEN high

    if (payload && payloadLen > 0) {
        frame.set(payload, idx);
        idx += payloadLen;
    }

    // CRC over CMD + SEQ + LEN + PAYLOAD (bytes 1..idx-1)
    const crcData = frame.slice(1, idx);
    const crcVal = crc16(crcData);
    frame[idx++] = crcVal & 0xFF;
    frame[idx++] = (crcVal >>> 8) & 0xFF;

    return frame;
}

/* ── Frame Parser ── */

/**
 * RPC frame parser with a state machine matching the firmware parser.
 *
 * Usage:
 *   const parser = new RpcParser();
 *   parser.onFrame = (cmd, seq, payload) => { ... };
 *   // Feed incoming bytes:
 *   parser.feed(new Uint8Array([0xAA, ...]));
 */
export class RpcParser {
    constructor() {
        this.state = 'idle';
        this.headerBuf = new Uint8Array(4);
        this.headerIdx = 0;
        this.payload = null;
        this.payloadIdx = 0;
        this.payloadLen = 0;
        this.crcBuf = new Uint8Array(2);
        this.crcIdx = 0;
        this.cmd = 0;
        this.seq = 0;

        /** @type {function(number, number, Uint8Array)|null} */
        this.onFrame = null;
    }

    /**
     * Feed raw bytes from the serial port into the parser.
     * @param {Uint8Array} data
     */
    feed(data) {
        for (let i = 0; i < data.length; i++) {
            this._parseByte(data[i]);
        }
    }

    _parseByte(byte) {
        switch (this.state) {
        case 'idle':
            if (byte === RPC_SYNC_BYTE) {
                this.headerIdx = 0;
                this.payloadIdx = 0;
                this.crcIdx = 0;
                this.state = 'header';
            }
            break;

        case 'header':
            this.headerBuf[this.headerIdx++] = byte;
            if (this.headerIdx >= 4) {
                this.cmd = this.headerBuf[0];
                this.seq = this.headerBuf[1];
                this.payloadLen = this.headerBuf[2] | (this.headerBuf[3] << 8);

                if (this.payloadLen > RPC_MAX_PAYLOAD) {
                    this.state = 'idle';
                } else if (this.payloadLen === 0) {
                    this.payload = new Uint8Array(0);
                    this.state = 'crc';
                } else {
                    this.payload = new Uint8Array(this.payloadLen);
                    this.payloadIdx = 0;
                    this.state = 'payload';
                }
            }
            break;

        case 'payload':
            this.payload[this.payloadIdx++] = byte;
            if (this.payloadIdx >= this.payloadLen) {
                this.state = 'crc';
            }
            break;

        case 'crc':
            this.crcBuf[this.crcIdx++] = byte;
            if (this.crcIdx >= 2) {
                const rxCrc = this.crcBuf[0] | (this.crcBuf[1] << 8);

                // Compute CRC over header(4) + payload
                let computedCrc = crc16(this.headerBuf);
                if (this.payloadLen > 0) {
                    computedCrc = crc16(this.payload, computedCrc);
                }

                if (rxCrc === computedCrc && this.onFrame) {
                    this.onFrame(this.cmd, this.seq, this.payload);
                }

                this.state = 'idle';
            }
            break;

        default:
            this.state = 'idle';
            break;
        }
    }
}

/* ── High-Level Command Builders ── */

/**
 * Build FW_UPDATE_START payload.
 * Payload: [size:4 LE] [crc32:4 LE]
 *
 * @param {number} totalSize   - Firmware image size in bytes
 * @param {number} expectedCrc - Expected CRC32 (from _wCRC.bin metadata, or 0)
 * @returns {Uint8Array} 8-byte payload
 */
export function buildFwUpdateStartPayload(totalSize, expectedCrc) {
    const buf = new Uint8Array(8);
    buf[0] = totalSize & 0xFF;
    buf[1] = (totalSize >>> 8) & 0xFF;
    buf[2] = (totalSize >>> 16) & 0xFF;
    buf[3] = (totalSize >>> 24) & 0xFF;
    buf[4] = expectedCrc & 0xFF;
    buf[5] = (expectedCrc >>> 8) & 0xFF;
    buf[6] = (expectedCrc >>> 16) & 0xFF;
    buf[7] = (expectedCrc >>> 24) & 0xFF;
    return buf;
}

/**
 * Build FW_UPDATE_DATA payload.
 * Payload: [offset:4 LE] [data:N]
 *
 * @param {number}     offset - Write offset within the image
 * @param {Uint8Array} chunk  - Firmware data chunk (up to 1024 bytes)
 * @returns {Uint8Array}
 */
export function buildFwUpdateDataPayload(offset, chunk) {
    const buf = new Uint8Array(4 + chunk.length);
    buf[0] = offset & 0xFF;
    buf[1] = (offset >>> 8) & 0xFF;
    buf[2] = (offset >>> 16) & 0xFF;
    buf[3] = (offset >>> 24) & 0xFF;
    buf.set(chunk, 4);
    return buf;
}

/**
 * Build FILE_WRITE_START payload.
 * Payload: [total_size:4 LE] [path string (UTF-8, no null terminator)]
 *
 * The firmware prepends "0:/" automatically when the path does not start with
 * a drive prefix, so paths like "IR/foo.ir" or "SubGHz/bar.sub" are correct.
 *
 * @param {number} totalSize - File size in bytes
 * @param {string} path      - Destination path on SD card (e.g. "IR/TVs/foo.ir")
 * @returns {Uint8Array}
 */
export function buildFileWriteStartPayload(totalSize, path) {
    const pathBytes = new TextEncoder().encode(path);
    const buf = new Uint8Array(4 + pathBytes.length);
    buf[0] = totalSize & 0xFF;
    buf[1] = (totalSize >>> 8) & 0xFF;
    buf[2] = (totalSize >>> 16) & 0xFF;
    buf[3] = (totalSize >>> 24) & 0xFF;
    buf.set(pathBytes, 4);
    return buf;
}

/**
 * Build FILE_WRITE_DATA payload.
 * Payload: [offset:4 LE] [data:N]
 *
 * @param {number}     offset - Write offset within the file
 * @param {Uint8Array} chunk  - Data chunk (≤ RPC_MAX_PAYLOAD − 4 bytes)
 * @returns {Uint8Array}
 */
export function buildFileWriteDataPayload(offset, chunk) {
    const buf = new Uint8Array(4 + chunk.length);
    buf[0] = offset & 0xFF;
    buf[1] = (offset >>> 8) & 0xFF;
    buf[2] = (offset >>> 16) & 0xFF;
    buf[3] = (offset >>> 24) & 0xFF;
    buf.set(chunk, 4);
    return buf;
}

/**
 * Build FILE_MKDIR payload.
 * Payload: [path string (UTF-8, no null terminator)]
 *
 * The firmware treats FR_EXIST as success, so calling mkdir on an existing
 * directory is safe and returns ACK.
 *
 * @param {string} path - Directory path on SD card (e.g. "IR/TVs")
 * @returns {Uint8Array}
 */
export function buildFileMkdirPayload(path) {
    return new TextEncoder().encode(path);
}

/**
 * Parse a DEVICE_INFO_RESP payload into a structured object.
 * @param {Uint8Array} payload
 * @returns {object}
 */
export function parseDeviceInfo(payload) {
    if (payload.length < 22) return null;
    const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
    const magic = view.getUint32(0, true);
    if (magic !== 0x4D314649) return null; // "M1FI"

    const info = {
        fwVersion: `${payload[4]}.${payload[5]}.${payload[6]}.${payload[7]}`,
        fwMajor: payload[4],
        fwMinor: payload[5],
        fwBuild: payload[6],
        fwRc: payload[7],
        activeBank: view.getUint16(8, true),
        batteryLevel: payload[10],
        batteryCharging: payload[11] !== 0,
        sdCardPresent: payload[12] !== 0,
        sdTotalKb: view.getUint32(13, true),
        sdFreeKb: view.getUint32(17, true),
        esp32Ready: payload[21] !== 0,
    };

    // ESP32 version string (null-terminated, 32 bytes starting at offset 22)
    if (payload.length >= 54) {
        const esp32Bytes = payload.slice(22, 54);
        const nullIdx = esp32Bytes.indexOf(0);
        info.esp32Version = new TextDecoder().decode(
            esp32Bytes.slice(0, nullIdx >= 0 ? nullIdx : esp32Bytes.length)
        );
    }

    if (payload.length >= 57) {
        info.ismBandRegion = payload[54];
        info.opMode = payload[55];
        info.southpawMode = payload[56];
    }
    if (payload.length >= 58) {
        info.hapaxRevision = payload[57];
    }

    return info;
}

/**
 * Parse a FW_INFO_RESP payload into a structured object.
 * @param {Uint8Array} payload
 * @returns {object}
 */
export function parseFwInfo(payload) {
    if (payload.length < 2) return null;
    const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);

    function parseBank(offset) {
        if (payload.length < offset + 13) return null;
        const bank = {
            fwVersion: `${payload[offset]}.${payload[offset+1]}.${payload[offset+2]}.${payload[offset+3]}`,
            fwMajor: payload[offset],
            fwMinor: payload[offset+1],
            fwBuild: payload[offset+2],
            fwRc: payload[offset+3],
            crcValid: payload[offset+4] !== 0,
            crc32: view.getUint32(offset+5, true),
            imageSize: view.getUint32(offset+9, true),
        };
        if (payload.length >= offset + 14) {
            bank.hapaxRevision = payload[offset+13];
        }
        if (payload.length >= offset + 34) {
            const dateBytes = payload.slice(offset+14, offset+34);
            const nullIdx = dateBytes.indexOf(0);
            bank.buildDate = new TextDecoder().decode(
                dateBytes.slice(0, nullIdx >= 0 ? nullIdx : dateBytes.length)
            );
        }
        return bank;
    }

    return {
        activeBank: view.getUint16(0, true),
        bank1: parseBank(2),
        bank2: parseBank(2 + 34),
    };
}
