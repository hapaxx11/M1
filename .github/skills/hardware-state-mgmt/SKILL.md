---
name: hardware-state-mgmt
description: Async/non-blocking RTOS best practices and hardware state-management lifecycles for the SI4463 radio, ESP32 coprocessor, NFC worker, IR hardware, Read Raw scene (TIM1), and the home-screen backlight. Load when touching radio/NFC/IR lifecycle, scene TX/RX flows, or diagnosing "works in tool X but not tool Y" bugs.
---

# Async RTOS & Hardware State Management

> Extracted from CLAUDE.md Architecture Rules. Load when writing or fixing
> code that owns a hardware lifecycle (radio, ESP32, NFC, IR, Read Raw, backlight).

### Async / Non-Blocking RTOS Best Practices

> **For every new feature or refactor, prefer event-driven / non-blocking patterns
> over blocking patterns wherever the hardware permits.  Reach for a blocking
> delegate only when the alternative is significantly more complex AND the
> blocking duration is bounded and short.  When in doubt, design for async.**

This is the canonical RTOS hygiene rule for the M1 codebase.  It is independent of —
and takes precedence over — the "Blocking delegate" wrapper pattern documented in the
[Scene-Based Application Architecture](../ui-scene-architecture/SKILL.md#scene-based-application-architecture)
section (in the `ui-scene-architecture` skill).  That section describes how *existing*
legacy event-loops are integrated
into scenes; **new code should not add new blocking delegates** without justification.

#### Why async/non-blocking

The M1 main task is a single FreeRTOS task that owns the UI, button input, the
event queue (`main_q_hdl`), and most module dispatch.  When a scene handler blocks
this task:

- **The scene `draw()` callback stops running** — no animation, no live status,
  no progress indicator.  The display freezes on the last frame drawn before the
  block began.
- **`Q_EVENT_KEYPAD` events queue up but are not processed**, so BACK does not
  cancel mid-operation and the user must wait for the blocking call to return.
- **Other periodic work stalls** — battery indicator refresh, RGB LED state
  machine, LCD inactivity saver — all share the main task.
- **Hardware completion ISRs that post events become irrelevant** — the events
  sit in `main_q_hdl` until the blocking code finally calls `xQueueReceive` (or
  the blocking code has its own private receive loop, in which case the *outer*
  scene loop is starved instead).

Async patterns avoid every one of these symptoms.

#### The canonical async pattern

For any hardware operation that finishes via an interrupt (DMA TC, timer expire,
GPIO edge, SPI completion):

1. **In the ISR**: post a typed event to `main_q_hdl` using `xQueueSendFromISR`
   and yield with `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)`.  Do **not** call
   any non-ISR-safe FreeRTOS API, do **not** touch heap allocations, do **not**
   `printf`.  Existing canonical example: `TIM1_UP_IRQHandler` in
   `m1_csrc/m1_int_hdl.c` posts `Q_EVENT_SUBGHZ_TX` on DMA completion.

2. **In the scene `on_event` handler**: handle the new event type.  Update scene
   state, set `app->need_redraw = true` if visible state changed, and return.
   Never block waiting for a *second* completion — chain by re-arming the
   hardware and returning, so the next completion event flows through the same
   handler.

3. **In the scene `draw()`**: render based on `app->raw_state` (or equivalent).
   The scene manager calls `draw()` on its draw tick (≥5 fps), so animations
   advance naturally as long as the main task is not blocked.

4. **Resource cleanup**: ownership of any heap allocation, temp file, or peripheral
   that outlives the *start* call must be released by the *completion* event
   handler, not by the caller of the start function.  A common bug is freeing a
   DMA source buffer in the caller while the DMA is still mid-transfer.

5. **Cancellation**: a BACK event in the scene handler must call a synchronous
   `_abort()` function that disables the timer / DMA / radio and returns the
   peripheral to a known-idle state.  After the abort, the scene transitions to
   its pre-operation state.  Any subsequent completion event (raced past the
   abort) must be a no-op when scene state is no longer "operation in progress".

#### When blocking is acceptable

A blocking pattern is acceptable when **all** of the following are true:

- The operation completes in **bounded, short** wall time (typically <50 ms) —
  short enough that no animation frame is visibly skipped.
- The operation does not depend on an external event that could fail to arrive
  (e.g. waiting for an ESP32 SPI response with no built-in timeout is **not**
  short and bounded — wrap it).
- Re-architecting as async would require duplicating substantial state across
  multiple `on_event` invocations with no concrete UX benefit (no animation
  needed, no cancel button, no concurrent work to do).

When you choose a blocking pattern despite this rule, document the choice with a
comment near the blocking call site explaining which of the above conditions
holds.

#### Forbidden patterns

- **Never `vTaskDelay()` inside a scene `on_event` handler.**  Use a redraw tick
  or a software timer if you need a delayed action; use a state-machine
  transition if you need to "wait" for a hardware event.
- **Never spin-wait on a flag set by an ISR** (e.g. `while (!subghz_tx_tc_flag);`).
  The ISR should post a queue event; the consumer should receive it.
- **Never call `HAL_Delay()` from a scene handler.**  `HAL_Delay()` blocks the
  whole task and breaks the watchdog discipline.
- **Never receive on the main queue from inside a blocking delegate** unless that
  delegate has fully replaced the scene's event loop — partial replacement
  starves the scene of events when the delegate returns control.
- **Never allocate memory in an ISR** or call any FreeRTOS function whose name
  does not end in `FromISR`.

#### Current async-ready infrastructure

These pieces are already in place and should be used by any new async work:

| Infrastructure | Where | What it does |
|----------------|-------|--------------|
| `main_q_hdl` | `m1_tasks.h`, `m1_main_task.c` | Main task event queue (`S_M1_Main_Q_t`) |
| `Q_EVENT_SUBGHZ_TX` | `m1_tasks.h`, `m1_int_hdl.c` | Posted by TIM1 UP ISR after DMA TC + final pulses |
| `Q_EVENT_SUBGHZ_RX` | `m1_tasks.h`, `m1_int_hdl.c` | Posted by TIM1 CC ISR per captured edge |
| `Q_EVENT_KEYPAD` | `m1_tasks.h`, button driver | Posted by the keypad driver on button transitions |
| `Q_EVENT_NFC_*`   | `m1_tasks.h`, `nfc_driver.c` | NFC worker state-machine event family |
| `Q_EVENT_IRRED_TX` | `m1_tasks.h`, IR driver | Posted on IR transmission completion |
| Scene draw tick | `m1_subghz_scene.c`, `m1_scene.c` | Periodic `draw()` invocation while waiting on `xQueueReceive` |
| `app->need_redraw` | `SubGhzApp` and generic scene context | Coalesced redraw flag — set in handlers, cleared by the scene loop |

The DMA-TC → ISR → queue plumbing for Sub-GHz TX **exists end-to-end**, and
the Read Raw scene consumes `Q_EVENT_SUBGHZ_TX` directly in its `on_event`
handler (implemented in PR #470), enabling live sine-wave animation and
hold-to-repeat TX.

#### Async TX state machine (completed — PR #470)

The Read Raw scene uses a fully async, non-blocking TX architecture with four
explicit Momentum-aligned states defined in `m1_subghz_read_raw_state.h`:

- `SubGhzReadRawStateTX` — one-shot TX from a freshly-recorded capture (Idle → TX → Idle).
- `SubGhzReadRawStateTXRepeat` — hold-to-repeat from Idle (TX → TXRepeat while OK held).
- `SubGhzReadRawStateLoadKeyTX` — one-shot TX from a pre-loaded file (Loaded → LoadKeyTX → Loaded).
- `SubGhzReadRawStateLoadKeyTXRepeat` — hold-to-repeat from Loaded.

The async TX API (declared in `m1_sub_ghz.h`):

- `sub_ghz_replay_prepare_datafile(path, freq, mod)` — populate streaming globals for a
  native `.sgh` file; no conversion, no temp file.
- `sub_ghz_replay_prepare_flipper(sub_path, &out_tmp_path)` — convert a Flipper/PACKET
  source to a temp `.sgh` at `/SUBGHZ/_flipper_tmp.sgh`; returns the path via
  `out_tmp_path` so the scene can track and unlink it.
- `sub_ghz_replay_start_async()` — arm TIM1+DMA and return immediately;
  `Q_EVENT_SUBGHZ_TX` events drive subsequent steps from the scene's `on_event`.
- `sub_ghz_replay_continue_async(repeat_on_idle)` — called from the scene's
  `SubGhzEventTxComplete` handler; returns `SUBGHZ_REPLAY_ASYNC_RUNNING` (more bursts
  to come), `SUBGHZ_REPLAY_ASYNC_DONE`, or `SUBGHZ_REPLAY_ASYNC_ERROR`.
- `sub_ghz_replay_abort()` — synchronous teardown for BACK / scene exit; safe to call
  even after natural completion.

**Blocking wrappers retained for legacy one-shot replay callers:**
`sub_ghz_replay_flipper_file()` and `sub_ghz_replay_datafile()` remain for the
Saved scene's PACKET/key emulate path, Playlist transmissions, Remote button TX,
Bind Wizard TX steps, Add Manually TX, and Flipper integration replay. These
callers do not need cancel-mid-TX or animation; the blocking wrappers run a private
mini event loop internally and are implemented in terms of the async primitives.  The temp file
`/SUBGHZ/_flipper_tmp.sgh` lifetime is managed internally by these wrappers.  For the
Read Raw scene, the temp file (when produced by `sub_ghz_replay_prepare_flipper()`) is
tracked via a scene-local `tx_unlink_path` buffer and unlinked on TX completion, abort,
or scene exit.

**TIM1 sharing discipline:** Read Raw must call `sub_ghz_rx_deinit_ext()` before
`sub_ghz_replay_start_async()` so the TX arming path cannot race the prior RX
input-capture configuration.  `sub_ghz_replay_start_async()` assumes this gate is
already done by the caller.  When Read Raw stays in-scene after TX completes
(`continue_async()` returns DONE/ERROR) or after a BACK abort, it restores listening
mode via `start_passive_rx()`.  On `scene_on_exit()`, it aborts and then performs
full radio teardown instead of restarting passive RX.

**Hold-to-repeat:** On `Q_EVENT_SUBGHZ_TX` completion, the scene peeks the OK button
state via `HAL_GPIO_ReadPin` (no FreeRTOS queue side effects).  If OK is still held, the
state transitions to the repeat variant (TX → TXRepeat, LoadKeyTX → LoadKeyTXRepeat)
and `continue_async(true)` re-arms the next burst.  When OK is released or the
one-shot completes without a held press, `continue_async(false)` drives the final
teardown and the scene returns to Idle or Loaded.

---

### SI4463 Radio State Management — `menu_sub_ghz_init()` / `menu_sub_ghz_exit()`

> **Every caller of a function that powers off the SI4463 MUST restore radio state
> before using the radio again.**  This is the single most common source of
> "radio works in tool X but not in tool Y" bugs.

**Background.** `menu_sub_ghz_exit()` deasserts the SI4463 ENA pin, completely
powering off the radio.  After this, the chip is unresponsive — SPI commands
return garbage, RX captures zero edges, TX produces no output.
`menu_sub_ghz_init()` calls `radio_init_rx_tx()` with a full PowerUp + patch
load + config reset, restoring the radio to a known good state.

**The pattern:**

1. **Blocking delegates** (Spectrum Analyzer, Freq Scanner, RSSI Meter,
   Weather Station, Brute Force, Add Manually, Frequency Reader) each call
   `menu_sub_ghz_init()` at their top and `menu_sub_ghz_exit()` at their
   bottom.  They own their own radio lifecycle — this is correct.

2. **`sub_ghz_replay_flipper_file()`** and **`sub_ghz_replay_datafile()`** both
   call `menu_sub_ghz_exit()` before returning.  **Any caller that performs
   additional direct radio operations after the wrapper returns must call
   `menu_sub_ghz_init()` first.**  Current wrapper users include:
   - `m1_subghz_scene_saved.c` — PACKET/key emulate handler (immediate re-init)
   - `m1_subghz_scene_playlist.c` — `playlist_transmit_next()` (immediate re-init)
   - `m1_subghz_scene_remote.c` — remote button TX helper (immediate re-init)
   - `m1_subghz_scene_bind_wizard.c` — bind-step TX helper (immediate re-init)
   - `m1_sub_ghz.c` — Add Manually replay path
   - `m1_flipper_integration.c` — Flipper `.sub` replay integration

   The Read Raw scene no longer calls these blocking wrappers for TX — it uses
   the async API (`sub_ghz_replay_prepare_*/start_async`) and restores radio
   state via `start_passive_rx()` after TX completion or abort.

3. **Scene-native RX starters** (`start_rx()` in Read, `start_raw_rx()` in
   Read Raw) call `menu_sub_ghz_init()` before configuring RX, so they
   recover from any prior powered-off state regardless of which scene
   the user was in before.

**Rules for new code:**
- If you call `sub_ghz_replay_flipper_file()` or `sub_ghz_replay_datafile()`, call
  `menu_sub_ghz_init()` before any subsequent direct radio operation outside the wrapper.
- If you write a new blocking delegate, call `menu_sub_ghz_init()` at the
  top and `menu_sub_ghz_exit()` at the bottom.
- If you write a new RX scene, call `menu_sub_ghz_init()` before starting
  RX — never assume the radio is already powered on.
- **Never assume radio state is preserved across scene transitions** — a
  blocking delegate may have run and powered off the radio between the
  user's last scene and yours.

### ESP32 Coprocessor State Management — `m1_esp32_init()` / `m1_esp32_deinit()`

> **Every blocking delegate that uses the ESP32 MUST call `m1_esp32_deinit()`
> on ALL exit paths — including early returns, error paths, and normal
> completion.**  Failing to deinit leaves the SPI transport and GPIO
> interrupts active, wasting power and potentially interfering with
> subsequent ESP32 operations from other modules.

**Background.** The ESP32-C6 coprocessor communicates with the STM32 via SPI.
Two initialization layers exist:

1. `m1_esp32_init()` — HAL-level SPI peripheral + GPIO + interrupt setup
   (in `m1_esp32_hal.c`).  Sets `esp32_init_done = TRUE`.
2. `esp32_main_init()` — Creates the `spi_trans_control_task` RTOS task
   and sets `esp32_main_init_done = true` (in `esp_app_main.c`).  The task
   persists across init/deinit cycles — the flag prevents duplicate creation.

`m1_esp32_deinit()` tears down the SPI peripheral, disables interrupts, and
resets `esp32_init_done`.  The SPI control task remains alive but dormant
(blocked on a queue with no interrupts firing).

**The pattern for ESP32-using blocking delegates:**

```c
void my_esp32_operation(void)
{
    /* Init ESP32 if needed */
    if (!m1_esp32_get_init_status())
        m1_esp32_init();
    if (!get_esp32_main_init_status())
        esp32_main_init();

    if (!get_esp32_main_init_status())
    {
        show_error("ESP32 not ready!");
        m1_esp32_deinit();   /* ← REQUIRED even on failure */
        return;
    }

    /* ... do work ... */

    m1_esp32_deinit();       /* ← REQUIRED on every exit path */
}
```

**Modules that follow this pattern:**
- WiFi: `wifi_scan_ap()`, `wifi_show_status()`, `wifi_disconnect()`,
  `wifi_ntp_sync()`
- Bluetooth: `bluetooth_scan()`, `bluetooth_advertise()`
- 802.15.4: `ieee802154_scan()` (Zigbee/Thread)
- BLE Spam: `ble_spam_*()` functions in `m1_bt.c` (SiN360 binary SPI path)
- Bad-BT: `badbt_run()` — calls `badbt_spi_hid_stop()` (SiN360) or `ble_hid_deinit()` (AT) before ESP32 deinit

**Rules for new code:**
- If you call `m1_esp32_init()` or `esp32_main_init()`, you MUST call
  `m1_esp32_deinit()` on every return path — including error early returns.
- `m1_esp32_deinit()` is safe to call even if init was not fully successful
  (it checks `esp32_init_done` internally).
- **Never leave the ESP32 SPI transport initialized after a blocking
  delegate returns** — the caller's scene does not know which ESP32 mode
  was active and cannot clean up for you.
- **`m1_esp32_ensure_init()` is NOT sufficient for capability-dispatching
  code.**  `m1_esp32_ensure_init()` calls only `m1_esp32_init()` (HAL-level
  SPI hardware).  The capability probe (`AT+CMD?` and `CMD_GET_STATUS`) runs
  inside the RTOS SPI-AT task, which is created by `esp32_main_init()`.
  If `esp32_main_init()` has not been called, `m1_esp32_has_cap()` and the
  entire capability bitmap return zero/false for every bit — including
  `M1_ESP32_CAP_WIFI_JOIN`, `M1_ESP32_CAP_BLE_HID`, etc.  Any code that
  dispatches on firmware type (AT vs SiN360 vs unknown future firmware) will
  silently take the binary-SPI fallback path and fail.

  **Rule:** Any function that reads capability bits — via `m1_esp32_has_cap()`,
  `m1_esp32_require_cap()`, `esp32_firmware_is_sin360()`, or any check on the
  cap bitmap — MUST call both init layers before the first capability read:
  ```c
  if (!m1_esp32_get_init_status())
      m1_esp32_init();
  if (!get_esp32_main_init_status())
      esp32_main_init();
  /* Now m1_esp32_has_cap() / m1_esp32_caps_probe() return correct values */
  ```
  This applies to ALL firmware variants — SiN360, dag T-800, stock AT, and any
  firmware type introduced in the future.  The two-probe architecture
  (CMD_GET_STATUS → AT+CMD? fallback) is designed to handle unknown firmware
  gracefully, but it can only run when the RTOS task is alive.

### NFC Worker State Management — `Q_EVENT_NFC_*` Events

> **The NFC worker task's state machine must handle ALL stop events sent
> to it.**  An unhandled event leaves the worker in `NFC_STATE_PROCESS`
> indefinitely, preventing `nfc_deinit_func()` from running and leaving
> `EN_EXT_5V` asserted.

**Background.** The NFC worker runs in a dedicated RTOS task
(`nfc_worker_task` in `nfc_driver.c`) with a 4-state machine:
`WAIT → INITIALIZE → PROCESS → DONE → WAIT`.  Transitions from PROCESS
to DONE are driven by events sent to `nfc_worker_q_hdl`.

**Handled stop events (trigger transition to `NFC_STATE_DONE`):**
- `Q_EVENT_NFC_READ_COMPLETE` — read operation finished
- `Q_EVENT_NFC_EMULATE_STOP` — emulation cancelled by user
- `Q_EVENT_NFC_STOP` — generic stop (cancel any operation in progress)

**The `NFC_STATE_DONE` handler calls `nfc_deinit_func()` and deasserts
`EN_EXT_5V`, so it is critical that every stop path reaches it.**

**Rules for new code:**
- If you add a new NFC stop event type, add a handler for it in the
  `NFC_STATE_PROCESS` case of `nfc_worker_task`.
- Always send a stop event to the worker before exiting an NFC blocking
  delegate — never rely on `menu_nfc_deinit()`'s `vTaskDelete()` to clean
  up, as that skips `nfc_deinit_func()`.

### IR Hardware State Management — `infrared_encode_sys_init()` / `_deinit()`

> **Every call to `infrared_encode_sys_init()` MUST be followed by
> `infrared_encode_sys_deinit()` — even if the TX complete event is not
> received.**  Skipping deinit on timeout leaves timer/DMA resources
> allocated, and the next `init()` call may fail or behave unpredictably.

**Background.** `transmit_command()` and `transmit_raw_command()` call
`infrared_encode_sys_init()` internally but do NOT call deinit — the
caller is responsible for deiniting after the `Q_EVENT_IRRED_TX` event.

**The pattern for IR TX loops (Send All, single-shot, raw):**
```c
transmit_command(&cmd);
/* Wait specifically for Q_EVENT_IRRED_TX, ignoring unrelated events.
 * BACK cancels.  Always deinit regardless of outcome. */
S_M1_Main_Q_t tx_q;
uint32_t deadline = HAL_GetTick() + 3000;
bool tx_done = false;
while (!tx_done) {
    uint32_t remaining = (HAL_GetTick() < deadline) ? (deadline - HAL_GetTick()) : 0;
    if (remaining == 0) break;
    if (xQueueReceive(main_q_hdl, &tx_q, pdMS_TO_TICKS(remaining)) != pdTRUE) break;
    if (tx_q.q_evt_type == Q_EVENT_IRRED_TX)
        tx_done = true;
    else if (tx_q.q_evt_type == Q_EVENT_KEYPAD)
        xQueueReceive(button_events_q_hdl, &bs, 0); /* drain to keep in sync */
}
infrared_encode_sys_deinit();  /* ← ALWAYS, even on timeout */
```

**Rules for new code:**
- Never skip `infrared_encode_sys_deinit()` — it must be called on every
  path after `transmit_command()` / `transmit_raw_command()`, regardless
  of whether `Q_EVENT_IRRED_TX` was received.
- When waiting for `Q_EVENT_IRRED_TX`, loop and ignore unrelated events
  (especially `Q_EVENT_KEYPAD`) — a single `xQueueReceive` may dequeue a
  keypad event instead.  Always drain `button_events_q_hdl` when consuming
  a keypad event to keep the two queues in sync.
- The event-loop single-shot TX handler already follows this pattern
  (deinit in the `Q_EVENT_IRRED_TX` handler).  The Send All loop must
  do the same for each iteration.

### Read Raw Scene State Management — TIM1 ISR / `start_passive_rx()` / `start_raw_rx()`

> **TIM1 input-capture MUST NOT be started during passive listen mode.**
> Starting it while `subghz_record_mode_flag == 0` causes the ISR to enqueue
> a `Q_EVENT_SUBGHZ_RX` for every noise edge (thousands per second at 433 MHz),
> flooding the main queue and preventing the 200 ms timeout that drives RSSI
> refreshes and the sine-wave animation.

**Background.** The Read Raw scene uses two radio states:

- **Passive listen** (Start / Idle) — SI4463 in RX for live RSSI; TIM1 ISR is
  NOT started.  The 200 ms queue timeout fires reliably and drives animations /
  RSSI updates.
- **Recording** — TIM1 ISR armed so captured edges flow into the ring buffer.

**The canonical timer lifecycle:**
```
scene_on_enter
  └─ start_passive_rx()          ← sub_ghz_rx_init_ext() only, NO rx_start
OK pressed (Start → Recording)
  └─ start_raw_rx()              ← sub_ghz_rx_start_ext() THEN record_mode_flag = 1
OK / BACK pressed (Recording → Idle)
  └─ stop_raw_rx()               ← sub_ghz_rx_pause_ext() + flush; NO rx_start at end
scene_on_exit
  └─ (recording) stop_raw_rx()   ← same cleanup including LED blink-off
  └─ (always)   sub_ghz_rx_deinit_ext() + opmode ISOLATED
```

**Rules for new code in or around the Read Raw scene:**

1. **Never call `sub_ghz_rx_start_ext()` inside `start_passive_rx()`.**  The
   timer must only be started when recording actually begins.
2. **In `start_raw_rx()`, set `subghz_decenc_ctl.pulse_det_stat = PULSE_DET_ACTIVE`
   and call `sub_ghz_rx_start_ext()` *before* setting `subghz_record_mode_flag = 1`.**
   This ensures the ISR state machine is initialised before any edge is captured.
3. **`stop_raw_rx()` pauses the timer but must NOT restart it.**  Passive Idle
   mode does not need TIM1 running.
4. **Never call `sub_ghz_rx_pause_ext()` in `scene_on_exit()` for Start/Idle.**
   The timer was never started in those states; calling pause on a stopped timer
   can trigger `Error_Handler()` via `HAL_TIM_IC_Stop_IT()`.  Call
   `sub_ghz_rx_deinit_ext()` directly — it checks `timerhdl_subghz_rx.State`
   before de-initialising and is safe regardless of run state.
5. **Use `stop_raw_rx()` on ALL exit paths when recording is active** (including
   scene exit), not inline teardown code.  `stop_raw_rx()` turns off the LED
   fast-blink — inline code that omits `m1_led_fast_blink(... OFF)` leaves the
   RGB LED blinking after leaving the scene.
6. **When staying in the Read Raw scene after TX completion or BACK abort, restore
   radio via `start_passive_rx(app)`** (not an inline restart block).  The scene calls
   `start_passive_rx(app)` from its `SubGhzEventTxComplete` handler (when
   `sub_ghz_replay_continue_async()` returns DONE/ERROR) and from its BACK handler
   (after `sub_ghz_replay_abort()`).  On `scene_on_exit()`, the scene aborts and then
   performs full teardown (`sub_ghz_rx_deinit_ext()` + isolated opmode), so it must
   **not** restart passive RX there. `sub_ghz_replay_prepare_flipper()` can mutate
   `subghz_custom_freq_hz`, `subghz_replay_band`, and `subghz_scan_config.modulation`;
   `start_passive_rx()` re-applies `app->freq_idx/mod_idx` via `subghz_apply_config_ext()`
   to restore the user-selected config.  An inline restart that reuses mutated replay
   globals can resume on the wrong frequency/modulation context after replay.

### Home Screen Backlight — Do Not Wake on Background Refresh

> **Never call `startup_info_screen_display()` or `m1_gui_welcome_scr()` from a
> background update path (battery, clock, SD card, or any periodic event).
> Both functions turn the backlight on and reset the inactivity timer, which
> wakes the screen without user interaction and prevents the saver timeout from
> ever expiring.**

**Background.** `startup_info_screen_display()` has two hardware side effects at the
end of every call:
1. `lp5814_backlight_on(M1_BACKLIGHT_BRIGHTNESS)` — turns the backlight on regardless
   of the current saver state.
2. `m1_device_stat.active_timestamp = HAL_GetTick()` — resets the inactivity timer,
   effectively restarting the full backlight timeout.

Both side effects are intentional for the call sites that **wake** the device (power-up
sequence, firmware update messages, user presses BACK from the main menu).  They are
**wrong** for background refresh paths.

**The two functions and when to use each:**

| Function | Use for | Backlight | Timer |
|----------|---------|-----------|-------|
| `startup_info_screen_display(msg)` | Power-up, FW update messages, user-initiated navigation to home | ON | Reset |
| `startup_home_screen_refresh()` | Background redraw — battery level/state changed, clock minute advanced | ✗ unchanged | ✗ unchanged |

**Rules for new code that redraws the home screen:**

1. **If the redraw is triggered by user input** (pressing BACK from the menu, completing
   a module and returning to home), use `startup_info_screen_display("")` or the
   `m1_gui_welcome_scr()` wrapper — these legitimately wake the display.
2. **If the redraw is triggered by a background event** (`Q_EVENT_BATTERY_UPDATED`,
   a clock tick, an SD-card state change, or any other periodic event), use
   `startup_home_screen_refresh()` only.  Do NOT call `m1_gui_welcome_scr()` or
   `startup_info_screen_display()`.
3. **Never reset `m1_device_stat.active_timestamp` in a background redraw.**  The
   inactivity timer must only advance from actual user keypresses (set in
   `system_periodic_task()` when `event_change & BUTTON_EVENT_ACTIVE` fires).
4. **The `Q_EVENT_BATTERY_UPDATED` handler in `m1_menu.c`** is the canonical
   example of the correct pattern — it calls `startup_home_screen_refresh()`.
   Any new event type that causes a home screen redraw must follow the same pattern.
5. **`lcd_saver_update()` in `m1_system.c`** controls the backlight based solely on
   `HAL_GetTick() - m1_device_stat.active_timestamp`.  Any code path that sets
   `active_timestamp` outside of a button-press event will extend the backlight
   timeout by the full configured period, defeating the saver.

**Implementation note — `s_home_scr_text` — preserving status text across background redraws:**
`startup_home_screen_refresh()` must redraw with the *same* text that was last passed to
`startup_info_screen_display()`, not an empty string.  Passing `""` would silently erase
user-visible messages such as "UPDATE COMPLETED!" or "ROLLBACK COMPLETED!" whenever the
battery/clock refresh fires in the background.  The implementation uses a 32-byte static
buffer `s_home_scr_text` in `m1_system.c`:
- `startup_info_screen_display(msg)` copies `msg` into `s_home_scr_text` (null-guarded
  `strncpy`) before drawing.
- `startup_home_screen_refresh()` calls `home_screen_draw_content(s_home_scr_text)`,
  replaying whatever text was last set by the wake path.
- On a fresh boot `s_home_scr_text` is zero-initialised (BSS), so the refresh draws an
  empty status line — correct for the normal idle screen.
- The buffer is **only** written by `startup_info_screen_display()`; the refresh path
  is read-only with respect to the buffer.

**Implementation note — `old_stat` ownership in `battery_indicator_update()`:**
The `old_stat` static variable in `battery_indicator_update()` is shared between the
1 s battery-refresh timer block and the 25 ms LED indicator state machine that runs
immediately after in the same function.  **Do not update `old_stat` inside the 1 s
timer block.**  If you do, the LED state machine sees no change on the same tick and
skips the LED transition when charging state changes on a timer boundary.  Only
`old_level` and `old_minute` are updated in the timer block; `old_stat` is exclusively
maintained by the LED section at the bottom of `battery_indicator_update()`.

---
