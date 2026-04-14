/**
 * app.js
 *
 * Main application logic for the M1 Web Updater.
 * Orchestrates UI, serial connection, GitHub releases, and firmware flashing.
 *
 * @license See COPYING.txt for license details.
 */

import { M1Serial } from './m1-serial.js';
import {
    RpcParser,
    buildFrame,
    buildFwUpdateStartPayload,
    buildFwUpdateDataPayload,
    parseDeviceInfo,
    parseFwInfo,
    RPC_CMD_PING,
    RPC_CMD_PONG,
    RPC_CMD_GET_DEVICE_INFO,
    RPC_CMD_DEVICE_INFO_RESP,
    RPC_CMD_ACK,
    RPC_CMD_NACK,
    RPC_CMD_FW_INFO,
    RPC_CMD_FW_INFO_RESP,
    RPC_CMD_FW_UPDATE_START,
    RPC_CMD_FW_UPDATE_DATA,
    RPC_CMD_FW_UPDATE_FINISH,
    RPC_CMD_FW_BANK_SWAP,
    RPC_ERRORS,
} from './m1-rpc.js';
import { fetchReleases, downloadFirmware } from './github-releases.js';

/* ── Configuration ── */

const REPO_OWNER = 'hapaxx11';
const REPO_NAME  = 'M1';
const FW_CHUNK_SIZE = 1024;  // Must match RPC_FW_CHUNK_SIZE on device
const RESPONSE_TIMEOUT_MS = 10000;  // 10s timeout for RPC responses
const ERASE_TIMEOUT_MS = 30000;     // 30s timeout for FW_UPDATE_START (flash erase)

/* ── Application State ── */

const serial = new M1Serial();
const parser = new RpcParser();

let seq = 0;
let pendingResolve = null;
let pendingReject = null;
let pendingTimeout = null;
let pendingExpectedCmd = null;
let pendingSeq = null;
let deviceInfo = null;
let fwInfo = null;
let releases = [];
let selectedFirmware = null;  // { data: Uint8Array, name: string }
let isFlashing = false;

/* ── DOM Elements ── */

const $ = (id) => document.getElementById(id);

const elements = {};

/**
 * Detect Android separately from generic mobile devices using
 * User-Agent Client Hints (preferred) with a UA string fallback.
 *
 * Use `isAndroid` for Android-specific troubleshooting or permission UI,
 * and `isMobile` only for platform-agnostic mobile behavior.
 */
const isAndroid = (navigator.userAgentData && navigator.userAgentData.platform === 'Android')
    || /Android/i.test(navigator.userAgent);
const isMobile = isAndroid
    || (navigator.userAgentData && navigator.userAgentData.mobile)
    || /iPhone|iPad|iPod/i.test(navigator.userAgent);

function cacheElements() {
    const ids = [
        'btn-connect', 'btn-disconnect', 'connection-status',
        'device-info-panel', 'device-fw-version', 'device-active-bank',
        'device-battery', 'device-hapax-rev',
        'bank1-info', 'bank2-info',
        'release-section', 'release-list', 'btn-refresh-releases',
        'local-file-input', 'local-file-name',
        'btn-flash', 'flash-section',
        'progress-section', 'progress-bar', 'progress-text', 'progress-detail',
        'log-output',
        'browser-warning',
        'btn-local-file',
        'mobile-tips',
        'btn-usb-permission',
        'usb-permission-status',
    ];
    for (const id of ids) {
        elements[id] = $(id);
    }
}

/* ── Logging ── */

function log(msg, level = 'info') {
    const time = new Date().toLocaleTimeString();
    const line = document.createElement('div');
    line.className = `log-${level}`;
    line.textContent = `[${time}] ${msg}`;
    elements['log-output'].appendChild(line);
    elements['log-output'].scrollTop = elements['log-output'].scrollHeight;
    console.log(`[${level}] ${msg}`);
}

/* ── RPC Communication ── */

function nextSeq() {
    seq = (seq + 1) & 0xFF;
    return seq;
}

/**
 * Send an RPC command and wait for ACK/NACK or a specific response.
 * Only one command can be in-flight at a time — concurrent calls are rejected.
 *
 * @param {number} cmd
 * @param {Uint8Array|null} payload
 * @param {number|null} [expectedCmd] - If set, wait for this response cmd instead of ACK
 * @param {number} [timeout] - Response timeout in ms
 * @returns {Promise<{cmd: number, seq: number, payload: Uint8Array}>}
 */
function sendCommand(cmd, payload = null, expectedCmd = null, timeout = RESPONSE_TIMEOUT_MS) {
    if (pendingResolve !== null) {
        return Promise.reject(new Error('Another RPC command is already in flight'));
    }

    return new Promise((resolve, reject) => {
        const s = nextSeq();
        const frame = buildFrame(cmd, s, payload);

        pendingResolve = resolve;
        pendingReject = reject;
        pendingExpectedCmd = expectedCmd;
        pendingSeq = s;

        pendingTimeout = setTimeout(() => {
            pendingResolve = null;
            pendingReject = null;
            pendingExpectedCmd = null;
            pendingSeq = null;
            reject(new Error('Response timeout'));
        }, timeout);

        serial.write(frame).catch(err => {
            clearTimeout(pendingTimeout);
            pendingResolve = null;
            pendingReject = null;
            pendingExpectedCmd = null;
            pendingSeq = null;
            reject(err);
        });
    });
}

/**
 * Handle a parsed RPC frame from the device.
 */
function handleFrame(cmd, frameSeq, payload) {
    // Resolve pending command if this is a response with matching seq
    if (pendingResolve && frameSeq === pendingSeq) {
        // NACK always resolves (as rejection) regardless of expectedCmd
        if (cmd === RPC_CMD_NACK) {
            clearTimeout(pendingTimeout);
            const reject = pendingReject;
            pendingResolve = null;
            pendingReject = null;
            pendingExpectedCmd = null;
            pendingSeq = null;
            const errCode = payload.length > 0 ? payload[0] : 0;
            const errMsg = RPC_ERRORS[errCode] || `Unknown error (0x${errCode.toString(16)})`;
            reject(new Error(`NACK: ${errMsg}`));
            return;
        }

        // If an expectedCmd is set, only resolve on that specific command.
        // If no expectedCmd is set, resolve on ACK or any response.
        if (pendingExpectedCmd === null || cmd === pendingExpectedCmd || cmd === RPC_CMD_ACK) {
            clearTimeout(pendingTimeout);
            const resolve = pendingResolve;
            pendingResolve = null;
            pendingReject = null;
            pendingExpectedCmd = null;
            pendingSeq = null;
            resolve({ cmd, seq: frameSeq, payload });
            return;
        }

        // Frame doesn't match expected cmd — ignore it (could be unsolicited)
    }

    // Handle unsolicited frames
    if (cmd === RPC_CMD_PING) {
        // Auto-respond with PONG
        const pong = buildFrame(RPC_CMD_PONG, frameSeq, null);
        serial.write(pong).catch(() => {});
    }
}

/* ── UI Updates ── */

function updateConnectionUI(connected) {
    elements['btn-connect'].disabled = connected;
    elements['btn-disconnect'].disabled = !connected;
    elements['connection-status'].textContent = connected ? 'Connected' : 'Disconnected';
    elements['connection-status'].className = `status-badge ${connected ? 'status-connected' : 'status-disconnected'}`;

    if (connected) {
        elements['flash-section'].classList.remove('hidden');
    } else {
        elements['device-info-panel'].classList.add('hidden');
        elements['flash-section'].classList.add('hidden');
        elements['progress-section'].classList.add('hidden');
        deviceInfo = null;
        fwInfo = null;
    }

    updateFlashButton();
}

function updateDeviceInfoUI() {
    if (!deviceInfo) return;
    elements['device-fw-version'].textContent = `v${deviceInfo.fwVersion}`;
    elements['device-active-bank'].textContent = `Bank ${deviceInfo.activeBank}`;
    elements['device-battery'].textContent = `${deviceInfo.batteryLevel}%${deviceInfo.batteryCharging ? ' ⚡' : ''}`;
    elements['device-hapax-rev'].textContent = deviceInfo.hapaxRevision !== undefined
        ? (deviceInfo.hapaxRevision === 0 ? 'Stock' : `r${deviceInfo.hapaxRevision}`)
        : 'N/A';
    elements['device-info-panel'].classList.remove('hidden');
}

function updateBankInfoUI() {
    if (!fwInfo) return;

    function formatBank(bank, label) {
        if (!bank || (bank.fwMajor === 0 && bank.fwMinor === 0 && bank.fwBuild === 0 && bank.fwRc === 0)) {
            return `${label}: Empty`;
        }
        let str = `${label}: v${bank.fwVersion}`;
        if (bank.hapaxRevision !== undefined) str += ` (r${bank.hapaxRevision})`;
        if (bank.crcValid) str += ' ✓';
        if (bank.buildDate) str += ` — ${bank.buildDate}`;
        return str;
    }

    elements['bank1-info'].textContent = formatBank(fwInfo.bank1, 'Bank 1');
    elements['bank2-info'].textContent = formatBank(fwInfo.bank2, 'Bank 2');
}

function updateProgress(percent, text, detail = '') {
    elements['progress-bar'].style.width = `${percent}%`;
    elements['progress-bar'].textContent = `${Math.round(percent)}%`;
    elements['progress-text'].textContent = text;
    elements['progress-detail'].textContent = detail;
}

function updateFlashButton() {
    const hasFirmware = selectedFirmware !== null ||
        document.querySelector('input[name="release"]:checked') !== null;
    elements['btn-flash'].disabled = !serial.connected || !hasFirmware || isFlashing;
}

/* ── Release List ── */

async function loadReleases() {
    log('Fetching releases from GitHub...');
    elements['release-list'].innerHTML = '<div class="loading">Loading releases...</div>';

    try {
        releases = await fetchReleases(REPO_OWNER, REPO_NAME);
        renderReleases();
        log(`Found ${releases.length} firmware release(s)`);
    } catch (err) {
        log(`Failed to fetch releases: ${err.message}`, 'error');
        elements['release-list'].innerHTML =
            '<div class="error">Failed to load releases. Check your internet connection.</div>';
    }
}

function renderReleases() {
    if (releases.length === 0) {
        elements['release-list'].innerHTML = '<div class="empty">No firmware releases found.</div>';
        return;
    }

    elements['release-list'].innerHTML = releases.map((r, i) => `
        <label class="release-item${i === 0 ? ' latest' : ''}">
            <input type="radio" name="release" value="${i}"
                   ${i === 0 ? 'checked' : ''}>
            <div class="release-info">
                <span class="release-name">${escapeHtml(r.name)}</span>
                <span class="release-meta">
                    ${r.prerelease ? '<span class="badge badge-pre">pre-release</span>' : ''}
                    <span class="release-date">${new Date(r.publishedAt).toLocaleDateString()}</span>
                    <span class="release-size">${formatSize(r.fwAsset.size)}</span>
                </span>
            </div>
        </label>
    `).join('');

    // Add change listeners
    for (const radio of document.querySelectorAll('input[name="release"]')) {
        radio.addEventListener('change', () => {
            selectedFirmware = null;  // Clear local file selection
            elements['local-file-name'].textContent = '';
            updateFlashButton();
        });
    }

    updateFlashButton();
}

/* ── Connection Handlers ── */

async function handleConnect() {
    if (!M1Serial.isSupported()) {
        log('Web Serial API not supported in this browser. Use Chrome or Edge.', 'error');
        return;
    }

    log('Requesting serial port...');
    try {
        await serial.connect();
        log('Serial port opened');
        updateConnectionUI(true);

        // Query device info
        log('Querying device info...');
        try {
            const resp = await sendCommand(RPC_CMD_GET_DEVICE_INFO);
            if (resp.cmd === RPC_CMD_DEVICE_INFO_RESP) {
                deviceInfo = parseDeviceInfo(resp.payload);
                if (deviceInfo) {
                    log(`Device: M1 v${deviceInfo.fwVersion} (Hapax r${deviceInfo.hapaxRevision || 0})`);
                    updateDeviceInfoUI();
                }
            }
        } catch (err) {
            log(`Device info query failed: ${err.message}`, 'warn');
        }

        // Query firmware bank info
        try {
            const resp = await sendCommand(RPC_CMD_FW_INFO);
            if (resp.cmd === RPC_CMD_FW_INFO_RESP) {
                fwInfo = parseFwInfo(resp.payload);
                if (fwInfo) {
                    updateBankInfoUI();
                    log(`Active bank: ${fwInfo.activeBank}`);
                }
            }
        } catch (err) {
            log(`FW info query failed: ${err.message}`, 'warn');
        }
    } catch (err) {
        if (isMobile && err.name === 'NotFoundError') {
            log('No serial device found. Make sure the M1 is connected via USB OTG and tap "Grant USB Access" first.', 'warn');
        } else {
            log(`Connection failed: ${err.message}`, 'error');
        }
        updateConnectionUI(false);
    }
}

async function handleDisconnect() {
    log('Disconnecting...');
    await serial.disconnect();
    updateConnectionUI(false);
    log('Disconnected');
}

/* ── Local File Selection ── */

function handleLocalFile(event) {
    const file = event.target.files[0];
    if (!file) return;

    if (!file.name.endsWith('.bin')) {
        log('Please select a .bin firmware file', 'error');
        return;
    }

    const reader = new FileReader();
    reader.onload = (e) => {
        selectedFirmware = {
            data: new Uint8Array(e.target.result),
            name: file.name,
        };
        elements['local-file-name'].textContent = `${file.name} (${formatSize(file.size)})`;
        log(`Local file loaded: ${file.name} (${formatSize(file.size)})`);

        // Deselect any release radio
        for (const radio of document.querySelectorAll('input[name="release"]')) {
            radio.checked = false;
        }
        updateFlashButton();
    };
    reader.readAsArrayBuffer(file);
}

/* ── Firmware Flashing ── */

async function handleFlash() {
    if (isFlashing) return;
    if (!serial.connected) {
        log('Not connected to device', 'error');
        return;
    }

    let firmware;

    if (selectedFirmware) {
        firmware = selectedFirmware;
    } else {
        // Get selected release
        const radio = document.querySelector('input[name="release"]:checked');
        if (!radio) {
            log('No firmware selected', 'error');
            return;
        }
        const release = releases[parseInt(radio.value, 10)];

        // Download firmware
        log(`Downloading ${release.fwAsset.name}...`);
        elements['progress-section'].classList.remove('hidden');
        updateProgress(0, 'Downloading firmware...');

        try {
            const data = await downloadFirmware(release.fwAsset.downloadUrl, (loaded, total) => {
                const pct = total > 0 ? (loaded / total) * 100 : 0;
                updateProgress(pct, 'Downloading firmware...', `${formatSize(loaded)} / ${formatSize(total)}`);
            });
            firmware = { data, name: release.fwAsset.name };
            log(`Downloaded ${firmware.name} (${formatSize(firmware.data.length)})`);
        } catch (err) {
            log(`Download failed: ${err.message}`, 'error');
            elements['progress-section'].classList.add('hidden');
            return;
        }
    }

    // Start flashing
    isFlashing = true;
    updateFlashButton();
    elements['progress-section'].classList.remove('hidden');

    try {
        await flashFirmware(firmware.data, firmware.name);
    } catch (err) {
        log(`Flash failed: ${err.message}`, 'error');
        updateProgress(0, 'Flash failed!', err.message);
    } finally {
        isFlashing = false;
        updateFlashButton();
    }
}

/**
 * Flash firmware to the M1 device using the RPC protocol.
 *
 * Steps:
 *   1. FW_UPDATE_START — sends size + CRC, device erases inactive bank
 *   2. FW_UPDATE_DATA  — sends firmware in 1024-byte chunks
 *   3. FW_UPDATE_FINISH — device verifies CRC
 *   4. FW_BANK_SWAP    — device swaps banks and reboots
 *
 * @param {Uint8Array} data - Firmware binary data
 * @param {string}     name - Firmware file name (for logging)
 */
async function flashFirmware(data, name) {
    const totalSize = data.length;
    log(`Starting firmware flash: ${name} (${formatSize(totalSize)})`);

    // Step 1: FW_UPDATE_START
    log('Erasing inactive flash bank...');
    updateProgress(0, 'Erasing flash...', 'This may take a few seconds');

    const startPayload = buildFwUpdateStartPayload(totalSize, 0);
    try {
        await sendCommand(RPC_CMD_FW_UPDATE_START, startPayload, null, ERASE_TIMEOUT_MS);
        log('Flash erased, starting write...');
    } catch (err) {
        throw new Error(`FW_UPDATE_START failed: ${err.message}`);
    }

    // Step 2: FW_UPDATE_DATA — send chunks
    const totalChunks = Math.ceil(totalSize / FW_CHUNK_SIZE);
    let offset = 0;

    for (let i = 0; i < totalChunks; i++) {
        const chunkEnd = Math.min(offset + FW_CHUNK_SIZE, totalSize);
        const chunk = data.slice(offset, chunkEnd);
        const payload = buildFwUpdateDataPayload(offset, chunk);

        try {
            await sendCommand(RPC_CMD_FW_UPDATE_DATA, payload);
        } catch (err) {
            throw new Error(`FW_UPDATE_DATA failed at offset ${offset}: ${err.message}`);
        }

        offset = chunkEnd;
        const pct = ((i + 1) / totalChunks) * 100;
        updateProgress(pct, 'Writing firmware...', `${formatSize(offset)} / ${formatSize(totalSize)}`);
    }

    log(`Wrote ${formatSize(totalSize)} (${totalChunks} chunks)`);

    // Step 3: FW_UPDATE_FINISH
    log('Verifying firmware...');
    updateProgress(100, 'Verifying firmware...');

    try {
        await sendCommand(RPC_CMD_FW_UPDATE_FINISH);
        log('Firmware verified OK');
    } catch (err) {
        throw new Error(`FW_UPDATE_FINISH failed: ${err.message}`);
    }

    // Step 4: FW_BANK_SWAP
    log('Swapping flash banks and rebooting...');
    updateProgress(100, 'Swapping banks...', 'Device will reboot');

    try {
        await sendCommand(RPC_CMD_FW_BANK_SWAP);
    } catch (err) {
        // The device reboots immediately after bank swap, so the serial
        // connection drops. A timeout or read error here is expected and OK.
        const msg = err.message || '';
        const isExpectedDisconnect =
            err.code === 'DEVICE_DISCONNECTED' ||
            msg.includes('timeout') ||
            msg.includes('disconnect') ||
            msg.includes('NetworkError') ||
            msg.includes('break') ||
            msg.includes('The device has been lost');
        if (isExpectedDisconnect) {
            log('Device is rebooting with new firmware...', 'success');
        } else {
            throw new Error(`FW_BANK_SWAP failed: ${err.message}`);
        }
    }

    updateProgress(100, 'Update complete!', 'Device is rebooting with new firmware');
    log('✓ Firmware update complete!', 'success');

    // The device will disconnect during reboot
    setTimeout(() => {
        updateConnectionUI(false);
    }, 2000);
}

/* ── Utility Functions ── */

function formatSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return `${(bytes / Math.pow(k, i)).toFixed(1)} ${sizes[i]}`;
}

function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

/* ── Initialization ── */

function init() {
    cacheElements();

    // Check browser support
    if (!M1Serial.isSupported()) {
        elements['browser-warning'].classList.remove('hidden');
        elements['btn-connect'].disabled = true;
        log('Web Serial API not supported. Please use Chrome 89+ or Edge 89+.', 'error');
    }

    // Wire up RPC parser
    parser.onFrame = handleFrame;
    serial.onData = (data) => parser.feed(data);
    serial.onDisconnect = () => {
        // Immediately reject any in-flight RPC command on disconnect
        if (pendingReject) {
            clearTimeout(pendingTimeout);
            const reject = pendingReject;
            pendingResolve = null;
            pendingReject = null;
            pendingExpectedCmd = null;
            pendingSeq = null;
            pendingTimeout = null;
            const err = new Error('Device disconnected');
            err.code = 'DEVICE_DISCONNECTED';
            reject(err);
        }
        log('Device disconnected', 'warn');
        updateConnectionUI(false);
    };

    // Wire up UI event handlers
    elements['btn-connect'].addEventListener('click', handleConnect);
    elements['btn-disconnect'].addEventListener('click', handleDisconnect);
    elements['btn-refresh-releases'].addEventListener('click', loadReleases);
    elements['btn-flash'].addEventListener('click', handleFlash);
    elements['local-file-input'].addEventListener('change', handleLocalFile);
    elements['btn-local-file'].addEventListener('click', () => {
        elements['local-file-input'].click();
    });

    // Android / mobile: show troubleshooting tips and USB permission button
    if (isMobile) {
        elements['mobile-tips'].classList.remove('hidden');

        if ('usb' in navigator) {
            elements['btn-usb-permission'].addEventListener('click', async () => {
                try {
                    const device = await navigator.usb.requestDevice({
                        filters: [
                            { vendorId: 0x0483, productId: 0x5750 },
                            { vendorId: 0x0483, productId: 0x5740 },
                            { vendorId: 0x0483, productId: 0x572A },
                        ]
                    });
                    elements['usb-permission-status'].textContent = '✓ Access granted';
                    elements['usb-permission-status'].style.color = 'var(--success)';
                    log(`USB access granted for ${device.productName || 'STM32 device'}. Now tap Connect.`, 'success');
                } catch (e) {
                    if (e.name === 'NotFoundError') {
                        elements['usb-permission-status'].textContent = 'No device selected';
                        elements['usb-permission-status'].style.color = 'var(--text-muted)';
                        log('USB device chooser dismissed or no matching device selected.');
                    } else {
                        log(`USB permission request: ${e.message}`, 'warn');
                    }
                }
            });
        } else {
            elements['btn-usb-permission'].disabled = true;
            elements['usb-permission-status'].textContent = 'WebUSB not available';
            elements['usb-permission-status'].style.color = 'var(--text-muted)';
        }
    }

    // Initial UI state
    updateConnectionUI(false);

    // Load releases
    loadReleases();

    log('M1 Web Updater ready');
}

document.addEventListener('DOMContentLoaded', init);
