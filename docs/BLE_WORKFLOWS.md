# Bluetooth / BLE Test Workflows

Use these workflows in a controlled lab with devices you own or have permission to test.

## Firmware Pairing

1. Flash the STM32 `M1_SiN360_v0.9.0.9_SD.bin`.
2. Flash a compatible ESP32-C6 firmware. ESP32 `v0.9.0.9` is compatible with this STM32 release.
3. Reboot the M1 after firmware updates.

BLE features live on the ESP32-C6, so compatible ESP32 firmware is required.

## Basic BLE Scan

1. Open `Bluetooth > Scan`.
2. Wait for nearby BLE devices to appear.
3. Use `UP` and `DOWN` to scroll.
4. Press `BACK` to exit.

You should see: addresses, RSSI, and names appear when devices advertise. Some phones randomize addresses and may not show a friendly name.

## Advertising Name

1. Open `Bluetooth > Config`.
2. Select `Name`.
3. Enter a test name with the full keyboard.
4. Open `Bluetooth > Advertise`.
5. Confirm the name from a phone BLE scanner app.
6. Press `BACK` to stop advertising.

You should see: the configured advertising name is saved to `bt.cfg` and reused by `Advertise`.

## BLE Spam Config

1. Open `Bluetooth > Config`.
2. Select `Rand MAC` and turn it on for spam tests that should rotate the ESP32 advertiser address.
3. Select `Spam Speed`.
4. Pick `Normal` first, then test `Fast` or `Slow` as needed.

You should see: spam screens use the configured interval and random-MAC behavior through the extended raw advertising command.

## BLE Spam Payloads

1. Open `Bluetooth > Attacks`.
2. Test one group at a time:
   - `Sour Apple`
   - `Swiftpair Spam`
   - `Samsung Spam`
   - `Google FP Spam`
   - `Flipper Spam`
   - `BT Spam All`
   - `Spoof Airtag`
3. Watch the on-device packet and error counters.
4. Press `BACK` to stop each run.

You should see: packet count increments on accepted advertisements. The error counter increments if the ESP32 rejects a payload or restart. Modern fully updated phones may ignore or silently suppress some spam payloads.

## BLE Analyzer And Sniffers

1. Open `Bluetooth > Sniffers > Bluetooth Analyzer`.
2. Let it collect raw advertisements.
3. Test specific detectors:
   - `Bluetooth Sniffer`
   - `Flipper Sniff`
   - `Airtag Sniff`
   - `Airtag Monitor`
   - `Detect Skimmers`
   - `Flock Sniff`
   - `Meta Detect`
4. Use `UP` and `DOWN` to scroll matches when matches are found.
5. Press `OK` to toggle the detail view for a match.
6. Press `BACK` to exit a view.
7. Check `bt/detect.csv` on the SD card.

You should see: analyzer views show raw BLE observations, best-effort signature matches, address/RSSI/name details, and a short raw advertisement hex preview. Skimmer, Flock, and Meta matching still depends on better field captures.

## BLE Wardrive

1. Open `Bluetooth > Wardrive > Bluetooth Wardrive`.
2. Let it collect nearby advertisements.
3. Press `BACK` to stop.
4. Check `bt/wardrive.csv` on the SD card.

You should see: raw BLE observations are appended to the CSV log.

BLE wardrive logs do not include GPS coordinates yet. They are signal and advertisement logs until GPS support is added.

## BLE Wardrive Continuous

1. Open `Bluetooth > Wardrive > Bluetooth Wardrive Continuous`.
2. Let it run while moving through a lab area.
3. Press `BACK` to stop.
4. Check `bt/wardrive.csv`.

You should see: continuous scan cycles append repeated observations over time.

Continuous wardrive appends repeated BLE observations without location fields.

## Flock Wardrive

1. Open `Bluetooth > Wardrive > Flock Wardrive`.
2. Let it run near known lab test advertisements.
3. Press `BACK` to stop.
4. Check `bt/flock_wardrive.csv`.

You should see: best-effort Flock-like matches are appended when signatures match.

Flock Wardrive keeps its name for the future GPS-backed workflow. For now, it logs address, type, RSSI, name, signature, and raw advertisement hex only.

## AirTag Monitor

1. Open `Bluetooth > Sniffers > Airtag Monitor`.
2. Let it run near an AirTag or AirTag-like test source.
3. Watch the count and latest match.
4. Press `BACK` to stop.

You should see: AirTag-like Apple manufacturer advertisements are counted. This is not Find My decoding.

## GATT Discovery

1. Use a connectable BLE peripheral, such as a BLE dev board, heart-rate sensor, or another known GATT test device.
2. Open `Bluetooth > GATT Discovery`.
3. Wait for the BLE scan list.
4. Select a target with `OK`.
5. Wait for connection and discovery.
6. Use `UP` and `DOWN` to scroll services, characteristics, and descriptors.
7. Press `OK` on a characteristic to open GATT tools.
8. Press `BACK` to exit.
9. Check `bt/gatt.csv` on the SD card.

You should see: GATT Discovery exports services, characteristics, descriptors, properties, UUIDs, and short readable characteristic values. Phones and privacy-focused devices often advertise but refuse normal GATT connections, so use a known connectable peripheral for the first test.

## GATT Write And Notify Tools

1. Use a known lab peripheral with writable or notifiable characteristics.
2. Open `Bluetooth > GATT Discovery`.
3. Select the target.
4. Scroll to a characteristic with `W`, `w`, `N`, or `I` in the props line.
5. Press `OK`.
6. Use `Write Text` for printable test values.
7. Use `Write Hex` for byte values such as `010203` or `01 02 03`.
8. Use `Notify View` to subscribe and watch incoming notification or indication values.
9. Press `BACK` from Notify View to unsubscribe.
10. Check `bt/gatt_notify.csv` on the SD card.

You should see: writes return a success or failure message, and Notify View shows a count, last handle, value length, and short hex preview as values arrive.

## GATT CSV Fields

`bt/gatt.csv` uses these columns:

```text
target_addr,target_name,type,handle1,handle2,props,uuid_hex,value_hex
```

- `type`: `svc`, `chr`, or `dsc`
- `handle1`: service start handle, characteristic definition handle, or descriptor handle
- `handle2`: service end handle, characteristic value handle, or owning characteristic value handle
- `props`: characteristic properties, or `00` for services/descriptors

`bt/gatt_notify.csv` uses these columns:

```text
target_addr,target_name,handle,type,value_hex
```

`bt/detect.csv` uses these columns:

```text
source,addr,type,rssi,name,signatures,adv_len,adv_hex
```
- `uuid_hex`: UUID bytes in hex
- `value_hex`: up to 16 bytes read from readable characteristics

## Cleanup Between BLE Tests

1. Stop the active BLE screen with `BACK`.
2. If a spam mode or advertiser seems stuck, start `Bluetooth > Advertise` and press `BACK` to force an advertising stop.
3. Reboot the M1 if the ESP32 radio gets into a stale coexistence state after heavy WiFi and BLE testing.
