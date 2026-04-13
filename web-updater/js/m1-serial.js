/**
 * m1-serial.js
 *
 * Web Serial API wrapper for M1 communication.
 * Handles connection, reading, writing, and disconnect.
 *
 * @license See COPYING.txt for license details.
 */

/**
 * M1 Serial connection manager.
 *
 * Usage:
 *   const serial = new M1Serial();
 *   await serial.connect();
 *   serial.onData = (data) => parser.feed(data);
 *   await serial.write(frameBytes);
 *   await serial.disconnect();
 */
export class M1Serial {
    constructor() {
        /** @type {SerialPort|null} */
        this.port = null;
        /** @type {ReadableStreamDefaultReader|null} */
        this.reader = null;
        /** @type {WritableStreamDefaultWriter|null} */
        this.writer = null;
        /** @type {boolean} */
        this.connected = false;
        /** @type {boolean} */
        this._reading = false;

        /**
         * Called when raw bytes arrive from the device.
         * @type {function(Uint8Array)|null}
         */
        this.onData = null;

        /**
         * Called when the connection is lost unexpectedly.
         * @type {function()|null}
         */
        this.onDisconnect = null;
    }

    /**
     * Check if Web Serial API is available in this browser.
     * @returns {boolean}
     */
    static isSupported() {
        return 'serial' in navigator;
    }

    /**
     * Open a connection to the M1 device.
     * The browser will show a port picker dialog.
     *
     * @param {object} [options] - Serial port options
     * @param {number} [options.baudRate=115200] - Baud rate
     * @returns {Promise<void>}
     */
    async connect(options = {}) {
        if (this.connected) {
            throw new Error('Already connected');
        }

        const baudRate = options.baudRate || 115200;

        // On Android USB-OTG the VID is often not forwarded to the
        // browser, so a filtered picker shows "No compatible devices
        // found".  Skip the filter on mobile so all ports are visible.
        // On desktop, keep the STMicro VID filter for a cleaner picker.
        const isMobile = /Android|iPhone|iPad|iPod/i.test(navigator.userAgent);
        const portOptions = isMobile
            ? {}
            : { filters: [{ usbVendorId: 0x0483 }] };
        this.port = await navigator.serial.requestPort(portOptions);

        await this.port.open({ baudRate });

        this.writer = this.port.writable.getWriter();
        this.connected = true;
        this._reading = true;

        // Start reading in background
        this._readLoop();

        // Listen for unexpected disconnect
        if (this.port.addEventListener) {
            this.port.addEventListener('disconnect', () => {
                this._handleDisconnect();
            });
        }
    }

    /**
     * Internal read loop — runs until disconnected.
     */
    async _readLoop() {
        let streamClosed = false;
        try {
            while (this._reading && this.port && this.port.readable && !streamClosed) {
                const reader = this.port.readable.getReader();
                this.reader = reader;
                try {
                    while (this._reading) {
                        const { value, done } = await reader.read();
                        if (done) {
                            streamClosed = true;
                            break;
                        }
                        if (value && this.onData) {
                            this.onData(value);
                        }
                    }
                } finally {
                    try { reader.releaseLock(); } catch (_) { /* already released */ }
                    this.reader = null;
                }
            }

            if (streamClosed && this._reading) {
                this._handleDisconnect();
            }
        } catch (err) {
            if (this._reading) {
                console.error('M1Serial read error:', err);
                this._handleDisconnect();
            }
        }
    }

    /**
     * Write raw bytes to the device.
     * @param {Uint8Array} data
     * @returns {Promise<void>}
     */
    async write(data) {
        if (!this.connected || !this.writer) {
            throw new Error('Not connected');
        }
        await this.writer.write(data);
    }

    /**
     * Gracefully disconnect from the device.
     * @returns {Promise<void>}
     */
    async disconnect() {
        this._reading = false;
        this.connected = false;

        try {
            if (this.reader) {
                await this.reader.cancel();
                this.reader.releaseLock();
                this.reader = null;
            }
        } catch (_) { /* ignore */ }

        try {
            if (this.writer) {
                await this.writer.close();
                this.writer.releaseLock();
                this.writer = null;
            }
        } catch (_) { /* ignore */ }

        try {
            if (this.port) {
                await this.port.close();
                this.port = null;
            }
        } catch (_) { /* ignore */ }
    }

    /**
     * Handle an unexpected disconnect.
     */
    _handleDisconnect() {
        this._reading = false;
        this.connected = false;
        this.reader = null;
        this.writer = null;
        this.port = null;
        if (this.onDisconnect) {
            this.onDisconnect();
        }
    }
}
