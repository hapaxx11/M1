# WiFi Test Workflows

Use these workflows in a controlled lab with networks and clients you own or have permission to test.

## Firmware Pairing

1. Flash the STM32 `M1_SiN360_v0.9.0.8_SD.bin`.
2. Flash the matching ESP32-C6 `v0.9.0.8` firmware.
3. Reboot the M1 after both updates.

STM32 and ESP32 firmware must match because WiFi and BLE commands use a shared binary SPI protocol.

## Basic WiFi Health Check

1. Open `WiFi > Scan AP`.
2. Confirm nearby AP names, BSSIDs, RSSI, and channels appear.
3. Open `WiFi > Sniffers > Channel Analyzer`.
4. Confirm channel counts and RSSI summaries appear after an AP scan.
5. Open `WiFi > Config > Status`.
6. Confirm AP cache, station cache, SSID pool, portal SSID, HTML state, and radio state look sane.

You should see: `Scan AP` fills the AP cache, and Channel Analyzer uses that cache.

## Capture Settings

1. Open `WiFi > Config > Capture > Status`.
2. Review the current toggles: `SavePCAP`, `ForcePMKID`, `ForceProbe`, and `EPDeauth`.
3. Open each toggle item to switch it on or off.

You should see: settings are saved to `wifi/capture.cfg`.

These controls do the following:

- `SavePCAP`: WiFi sniffers write PCAP files under `pcap/`.
- `ForcePMKID`: EAPOL/PMKID sniffing starts selected-target deauth when selected AP/station targets exist.
- `ForceProbe`: Probe Request sniffing starts selected-target deauth when selected AP/station targets exist.
- `EPDeauth`: Evil Portal starts selected-target deauth while the portal is active.

Use AP/station selection first when testing the force options.

## AP And Station Target Selection

1. Run `WiFi > Scan AP`.
2. Open `WiFi > Config > APs > Select APs`.
3. Use `OK` to mark one or more lab APs. Selected rows show `*`.
4. Run `WiFi > Scan Stations`.
5. Open `WiFi > Config > Stations > Select Stations`.
6. Use `OK` to mark lab stations.

You should see: selected APs/stations are reused by attacks that support selected targets.

## Beacon Spam

1. Put a text file on SD card, for example `wifi/ssids.txt`.
2. Add one SSID per line. Keep each SSID 32 bytes or less.
3. Open `WiFi > Attacks > Beacon Spam`.
4. Pick the `.txt` or `.lst` file from the file browser.
5. Choose `All in Order`, `Shuffle All`, or `Random 16`.
6. Watch with a phone or laptop WiFi scanner.
7. Press `BACK` to stop.

You should see: multiple SSIDs from the file appear over time. Phone WiFi UIs cache and throttle scan results, so a dedicated analyzer may show changes faster.

## Rickroll

1. Open `WiFi > Attacks > Rickroll`.
2. Watch the on-device lyric view and a WiFi scanner.
3. Press `BACK` to stop.

You should see: the lyric SSIDs rotate while the beacon run is active.

## AP Clone

1. Run `WiFi > Scan AP`.
2. Optionally mark lab APs in `WiFi > Config > APs > Select APs`.
3. Open `WiFi > Attacks > AP Clone`.
4. Press `BACK` to stop.

You should see: if APs are selected, clone spam uses selected AP names first. Otherwise it uses scanned AP names.

## Deauth Lab Test

1. Use only a lab AP and client you own.
2. Run `WiFi > Scan AP`.
3. Mark the lab AP in `WiFi > Config > APs > Select APs`.
4. Optionally run `WiFi > Scan Stations` and mark the lab client.
5. Open `WiFi > Attacks > Deauth`.
6. Press `BACK` to stop.

You should see: selected AP/station targets are used first. If no targets are selected, the UI falls back to AP picking.

## Probe Flood

1. Open `WiFi > Attacks > Probe Flood`.
2. Watch with a monitor-mode adapter or analyzer.
3. Press `BACK` to stop.

You should see: probe request count increments while running.

## PCAP Capture

1. Open `WiFi > Config > Capture > SavePCAP`.
2. Make sure it is `On`.
3. Run a WiFi sniffer, such as `WiFi > Sniffers > Beacons`, `Probe Requests`, `Deauth`, `EAPOL/Handshake`, `SAE/WPA3`, or `Raw Sniff`.
4. Let it capture packets for a short test.
5. Press `BACK` to stop the sniffer.
6. Check SD card files under `pcap/`.

You should see: PCAP files are appended by sniffer type:

```text
pcap/beacon.pcap
pcap/probe.pcap
pcap/deauth.pcap
pcap/eapol.pcap
pcap/pwnagotchi.pcap
pcap/sae.pcap
pcap/raw.pcap
```

The PCAP link type is raw IEEE 802.11. Frames are capped at 256 bytes per packet to fit the STM32/ESP32 SPI chunking path.

## EAPOL / PMKID Capture

1. Run `WiFi > Scan AP`.
2. Mark a lab AP in `WiFi > Config > APs > Select APs`.
3. Open `WiFi > Config > Capture > ForcePMKID` and turn it on if you want selected-target deauth nudges.
4. Open `WiFi > Config > Capture > SavePCAP` and turn it on if you want PCAP output.
5. Open `WiFi > Sniffers > EAPOL/Handshake`.
6. Let the sniffer run while a lab client connects or reconnects.
7. Press `BACK` to stop.
8. Check `pmkid/captures.22000` and `pcap/eapol.pcap`.

You should see: PMKID hashes save in hashcat 22000 format when present, while EAPOL PCAP frames save separately.

## Advanced Raw Management Attacks

1. Run `WiFi > Scan AP`.
2. Select a lab AP or let the attack picker choose one.
3. Open `WiFi > Attacks > Advanced`.
4. Test one item at a time: `SAE Commit`, `Channel Switch`, `Quiet Time`, `Sleep Attack`, or `Bad Msg`.
5. Press `BACK` to stop.

You should see: packet counters increase. Channel Switch and Quiet Time include selected AP SSID context in their beacon-style frames. Client/AP behavior varies heavily by chipset and firmware, so not every target will visibly react.

## Join WiFi And Network Scanners

1. Open `WiFi > Config > Radio > Join WiFi`.
2. Select your lab AP.
3. Enter the password with the full keyboard.
4. After joining, open `WiFi > Scanners`.
5. Run `Ping Scan`, `ARP Scan`, `SSH Scan`, `Telnet Scan`, or `Port Scan`.

You should see: network scanners report hosts on the joined network's `/24`. They require a successful WiFi join first.

## Wardrive And Station Wardrive

1. Open `WiFi > Sniffers > Wardrive`.
2. Let it collect AP rows, then press `BACK`.
3. Open `WiFi > Sniffers > Station Wardrive`.
4. Let it collect station rows, then press `BACK`.
5. Check SD card CSV outputs.

You should see: AP/station observations are appended to SD card CSV files.

## Evil Portal Built-In Page

1. Open `WiFi > Config > Portal > Portal SSID`.
2. Set the SSID to a lab-only portal name.
3. Open `WiFi > Attacks > Evil Portal`.
4. Connect a test phone or laptop to the portal AP.
5. Submit test credentials.
6. Stop the portal and check the credential log on SD card.

You should see: the built-in page captures supported username/network and password fields.

## Evil Portal Custom HTML

1. Put a `.html` or `.htm` file on SD card.
2. Keep it at or below `32768` bytes.
3. Prefer inline CSS/JS because captive portal clients may block external assets.
4. Use `{{SSID}}` anywhere the active SSID should appear.
5. Use a normal POST form where possible:

```html
<form method="POST" action="/login">
  <input name="ssid" placeholder="Network name">
  <input name="password" type="password" placeholder="Password">
  <button type="submit">Continue</button>
</form>
```

6. Supported username/network field names: `user`, `username`, `email`, `login`, `identity`, `ssid`, `wifi`, `wifi_name`, `network`, `network_name`.
7. Supported password field names: `pass`, `password`, `pwd`.
8. Simple GET forms such as `action="/get"` also work when they use supported field names.
9. Open `WiFi > Config > Portal > EP HTML File`.
10. Start `WiFi > Attacks > Evil Portal` and submit a test form.

You should see: POST forms, GET query forms, JSON posts, and many JavaScript `preventDefault()` pages are captured.

## Karma

1. Open `WiFi > Attacks > Karma`.
2. Use a lab phone or laptop that sends directed probe requests.
3. Watch directed probe count, total probe count, channel, and latest SSID.
4. Press `BACK` to stop.

You should see: plain Karma channel-hops and changes the open AP SSID to recent directed probe SSIDs.

## Karma Portal

1. Configure Portal SSID and optional custom HTML first.
2. Open `WiFi > Attacks > Karma Portal`.
3. Connect a lab client when the expected AP name appears.
4. Submit test credentials.
5. Press `BACK` and check the SD card credential log.

You should see: Karma Portal uses the same captive portal and credential store as Evil Portal, while syncing `{{SSID}}` to the active probe-followed SSID.

## Radio Controls

1. `Set Channel`: sets ESP32 WiFi channel 1-13.
2. `Set MACs`: sets random or manual STA/AP MAC values.
3. `Shutdown WiFi`: stops active WiFi AP/promiscuous/connection state and restores STA mode.

Use `Shutdown WiFi` between unrelated WiFi tests if the radio state looks stale.
