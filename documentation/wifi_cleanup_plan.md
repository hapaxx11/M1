# WiFi / ESP32 UX Cleanup — Phased Implementation Plan

> **Status:** design proposal (this PR contains *only* the plan).
> **Tracking issue:** hapaxx11/M1 #680 — "WiFi Cleanup".
> **Scope:** re-organize the WiFi (and related ESP32) menu tree into a cohesive,
> C3-inspired UX while preserving maintainability across the several ESP32
> firmwares we support (stock AT, dag T‑800 AT, SiN360 binary‑SPI).

This document is intentionally the whole deliverable for the first PR. Each
phase below becomes its own follow‑up PR so that every change stays small,
reviewable, and independently testable. No feature code is modified here.

---

## 1. Motivation

Today the WiFi module presents a wide, flat set of tools whose grouping does not
map cleanly onto *what the user is trying to do* or *what state the radio must be
in*. The two biggest problems:

1. **Ambiguous groupings.** "Recon" vs "Sniffers" overlap heavily, Deauth is
   reachable from two paths, and channel/AP‑specific actions are scattered.
2. **State is implicit.** Actions that require an authenticated connection, a
   selected SSID, or a specific AP/channel are presented next to actions that
   require none of those, with no visual cue about the precondition.

The [C3 firmware](https://github.com/bedge117/M1) demonstrates a flatter,
task‑first top menu that users find more intuitive. We adopt its *spirit*
(scan‑and‑connect first, one obvious path per action, context‑scoped attacks)
without copying it verbatim, and we keep the M1‑specific features we already
ship (Peer Link, richer Recon, Wardrive suite, Evil Portal, Net Scan).

### Guiding UX principle — "the precondition determines the location"

The single organizing rule for the whole redesign:

| If an action requires…                     | …then it lives…                                            |
|--------------------------------------------|------------------------------------------------------------|
| Nothing (works anytime)                    | Top‑level WiFi menu                                         |
| An **authenticated connection** to an SSID | Behind the **Connected** button‑menu of that network       |
| A **specific SSID** (no auth)              | In the selected‑network **Target** submenu                 |
| A **specific AP/channel** (BSSID)          | In the selected‑network Target submenu, cycling known APs  |
| A **set of networks** to cycle through     | In a multi‑target **Attack** screen seeded from selections |

This gives every feature exactly one home and makes the precondition obvious
from where the feature appears.

---

## 2. Current vs. reference state

### 2.1 Current M1 top menu (`m1_wifi_scene_menu.c`)

```
WiFi
├── Networks          → scan + connect (already replaces into Connected menu on join)
├── Recon             → Station Scan · 2.4G Survey · MAC Track · Wardrive · Station Wardrive · Signal Monitor
├── Sniffers          → All Packets · Beacons · Probe Req · Deauth · EAPOL · Pwnagotchi · SAE/WPA3
├── Attacks           → Deauth · Beacon Spam · AP Clone · Rickroll · Evil Portal · Probe Flood · Karma · Karma+Portal · PMKID Grab
├── 802.15.4          → Zigbee Scan · Thread Scan
├── Peer Link         → ESP‑NOW (own scene manager)
└── General           → View AP Info · Select APs · Select STAs · Save/Load/Clear APs · Load/Clear SSIDs · Join · Set MACs · Set Chan · Shutdown · EP SSID · EP HTML
```

Relevant existing plumbing we can build on (no need to invent from scratch):

- **`WifiSceneConnectedMenu`** already exists (compile‑gated by
  `M1_APP_WIFI_CONNECT_ENABLE`) and `Networks`/`Saved` already
  `m1_scene_replace()` into it on successful join — the "connected context"
  pattern is proven.
- **`wifi_prompt_disconnect()`** and **`wifi_require_connected()`** already gate
  sub‑menus by connection state (see `test_wifi_ux_restructure.c`).
- **`wifi_selection.[ch]`** provides pure `wifi_selected_ap_count()` /
  `wifi_selected_sta_count()` — the basis for multi‑target attack seeding.
- **`esp32_feature_map.[ch]`** + `DELEGATE_FEATURE()` already gate features by
  ESP32 capability, so per‑firmware availability is a solved problem.
- Scenes are split per‑file (`m1_wifi_scene_{menu,sniff,attack,net,general,connect}.c`)
  and driven by a shared `subghz_submenu_model_t` — new menus are cheap to add.

### 2.2 C3 reference top menu

```
1 WiFi Scan+Connect   2 Deauth        3 Handshake      4 Beacon
5 Probe Flood         6 Karma         7 Packet Monitor 8 802.15.4 Scan
9 802.15.4 Flood     10 Saved Networks 11 WiFi Hotspot 12 Status  13 Disconnect
```

C3 is flat but *state‑blind* — Deauth/Handshake/Beacon sit at the top level even
though they target a specific network. Our redesign keeps C3's "scan first,
obvious verbs" feel but routes state‑dependent verbs to the context where their
precondition is already satisfied.

---

## 3. Target design

### 3.1 Proposed top‑level WiFi menu

```
WiFi
├── Scan & Connect     ← primary entry (rename of "Networks"); scan list → select SSID
├── Recon              ← passive discovery/monitoring (see §3.4)
├── Attacks            ← broadcast / multi‑target attacks that need no single SSID
├── Wardrive           ← promoted to its own item (see §3.5)
├── 802.15.4           ← Zigbee/Thread scan + flood (see §3.6)
├── Peer Link          ← ESP‑NOW (unchanged, kept per issue)
└── General            ← Saved Networks · Status · Disconnect · AP/SSID list mgmt
```

"Sniffers" disappears as a top‑level noun; its members are redistributed by
precondition (passive captures → Recon; targeted captures → the selected‑network
menu). "Attacks" is retained but slimmed: only attacks that are genuinely
network‑agnostic (broadcast beacon spam, karma, probe flood) or that operate on
a *selection set* remain here; single‑target attacks move into the selected‑
network menu.

### 3.2 Selected‑network context (the heart of the cleanup)

Pressing OK on an SSID in **Scan & Connect** opens a per‑network screen showing
BSSID/channel/RSSI/security, plus a **button bar** offering two clearly separated
groups:

```
[SSID:  MyNetwork]  ch 6  -52dBm  WPA2
── Target (no auth needed) ──        ── Connect (auth needed) ──
  • Deauth (this AP)                    • Connect  → Connected menu
  • Handshake / EAPOL capture           (only shown if credentials/known)
  • Beacon (clone this SSID)
  • Probe capture (this SSID)
  • PMKID grab
  • Cycle APs ▸ (if SSID has >1 BSSID)
```

- **Target group** = actions that need a *specific SSID/AP but not auth*
  (Handshake, targeted Beacon, per‑AP Deauth, PMKID). These are the C3
  "Handshake/Beacon" verbs, now correctly scoped.
- **Connect group** = the single "Connect" action; on success we
  `m1_scene_replace()` into the existing **Connected menu**.
- **Cycle APs** handles the "specific AP needed" case: when one SSID is served by
  multiple BSSIDs, the Target actions iterate the known AP records for that SSID.

### 3.3 Connected context (`WifiSceneConnectedMenu`, already exists)

Everything that needs an *authenticated* connection lives here. We extend the
current 3‑item menu (Status · Net Scan · Disconnect) as capability allows:

```
Connected: MyNetwork
├── Status
├── Net Scan     → Ping · ARP · SSH · Telnet · Ports (existing WifiSceneNetMenu)
├── (future) Captive/Evil‑Portal helpers that assume an uplink
└── Disconnect
```

### 3.4 Resolving "Recon vs Sniffers"

They collapse into **one** passive‑observation menu, **Recon**, because both are
"listen, don't transmit, no target required":

```
Recon
├── Station Scan
├── 2.4G Survey
├── Signal Monitor
├── MAC Track
├── Packet Monitor      (was "All Packets" — C3 naming)
├── Beacon Sniff
├── Probe Sniff
├── EAPOL Sniff
├── Deauth Sniff
├── Pwnagotchi
└── SAE / WPA3
```

Rationale: a "sniffer" *is* recon. Keeping two menus forced users to guess
whether a capture was filed under Recon or Sniffers. Channel‑specific passive
tools (Packet Monitor, Signal Monitor) prompt for a channel on entry rather than
being siloed into a separate "channel" group — the prompt makes the precondition
explicit at the moment it matters.

### 3.5 Wardrive promoted to its own menu

Per the issue, Wardrive is its own workflow (GPS‑logged surveying, largely from
the Dag/SiN lineage). It becomes a top‑level WiFi item collecting all
war‑driving flows:

```
Wardrive
├── AP Wardrive        (was "Wardrive")
├── Station Wardrive
└── (GPS/log settings, save/export)     ← relocate the log‑oriented "General" items here
```

This answers the issue's "what are the rest of General used for — maybe moved to
Wardrive?": AP/SSID list *management* (Save/Load/Clear APs, Load/Clear SSIDs)
that exists to persist wardrive/scan captures moves under Wardrive's export flow;
only Saved Networks / Status / Disconnect stay in General.

### 3.6 802.15.4 (kept, expanded)

Per the issue, all 802.15.4 features stay under this one item. Add a **Flood**
action beside the existing Zigbee/Thread scans to match C3, capability‑gated by
`ESP32_FEATURE_802154`:

```
802.15.4
├── Zigbee Scan
├── Thread Scan
└── 802.15.4 Flood     (capability-gated)
```

### 3.7 Consolidating Karma / Evil Portal

The issue asks whether Evil Portal / Karma / Karma+Portal are overdone — yes.
Collapse to **two** entries driven by an option toggle instead of three menu
items:

- **Karma** with an inline "Serve captive portal" yes/no option (replaces
  `Karma` + `Karma+Portal`).
- **Evil Portal** as a standalone captive‑portal attack.

This removes the confusing `Karma+Portal` third item while preserving every
capability behind an obvious toggle.

### 3.8 Deauth — one primary path, plus a multi‑target flow

Deauth is currently reachable from both Recon(→Sniffers) and Attacks; the
"Deauth Sniff" (passive detection) and "Deauth attack" (active tx) are different
features that share a name. The redesign disambiguates:

- **Deauth Sniff** (passive detector) → stays in **Recon**.
- **Per‑AP Deauth** (single target) → **Target** group of the selected network.
- **Multi‑target Deauth** (cycle a selection set) → **Attacks → Deauth** using
  `wifi_selected_ap_count()` selections. This is the generalized "attack screen
  for multiple networks cycled through" the issue asks for, and the same
  multi‑target harness is reused by Beacon Spam / Probe Flood.
- **Post‑deauth chaining** (issue's "deauth then evil portal") is offered as a
  follow‑up prompt on the single‑target Deauth completion screen:
  `Deauth done → [Capture Handshake] [Evil Portal] [Done]`.

### 3.9 WiFi Hotspot

Add a **WiFi Hotspot** entry (C3 item 11) under General or as its own top item.
Implementation depends on ESP32 firmware support (SoftAP); **deferred** to the
last phase and capability‑gated so builds without SoftAP simply hide it. If the
gate shows it is non‑trivial, it can slip without blocking the rest of the plan.

---

## 4. Mapping every issue bullet to a decision

| Issue point | Decision | Phase |
|-------------|----------|-------|
| C3 "Scan+Connect" should be first | Rename `Networks` → `Scan & Connect`, keep first | P1 |
| Auth‑required features behind connected network button menu | Route to `WifiSceneConnectedMenu` | P3 |
| SSID‑required (no auth) features behind a selected‑SSID submenu, *different* from the connected one | New **Target** button group on selected network | P3 |
| Specific‑AP features cycle known APs | "Cycle APs" in Target group | P3 |
| Deauth double path / what's required / follow‑up actions / multi‑target | §3.8: split sniff vs attack, per‑AP + multi‑target + chaining | P2/P4 |
| Recon vs Sniffers difference | Merge into single **Recon** menu | P2 |
| Probe Flood / Karma / Packet Monitor need channels — group? | Channel prompt on entry; Packet Monitor→Recon, Probe Flood/Karma→Attacks | P2 |
| Evil Portal / Karma / Karma+Portal overdone | Collapse to Karma(+toggle) & Evil Portal | P4 |
| Keep 802.15.4, put related features there | Keep + add Flood | P2 |
| Wardrive own menu | Promote to top‑level **Wardrive** | P2 |
| Add WiFi Hotspot (may defer) | Deferred, capability‑gated | P5 |
| Keep Peer Link here | Unchanged | — |
| General = Saved/Status/Disconnect; move the rest | Slim General; list mgmt → Wardrive export | P2 |

---

## 5. Phased rollout

Each phase is a self‑contained PR. Per repo policy every behavioral change ships
with a **host‑side regression test** (under `tests/`) following the existing
source‑assertion style of `test_wifi_ux_restructure.c` (verifying menu labels,
targets, scene ordering, and gating declarations) plus pure‑logic unit tests for
any new helper extracted along the way. A firmware build is required for every
phase that touches `.c/.h` sources.

### Phase 0 — Plan — **DONE**
- [x] This document.
- [x] Changelog fragment noting the design doc.
- No source changes, no build required (docs‑only).

### Phase 1 — Rename + top‑menu reshape (low risk, no new features) — **DONE**
- [x] Rename top item `Networks` → `Scan & Connect` (label + user‑facing hint
      strings in `m1_wifi.c`).
- [x] Introduce a top‑level **Wardrive** sub‑menu. Rather than shipping an empty
      placeholder, the two existing wardrive tools (AP Wardrive / Station
      Wardrive) were relocated out of Recon into it immediately — this pulls the
      §3.5 relocation forward from Phase 2 and avoids a dead menu.
- [x] **Sniffers is intentionally kept on the top menu for now** so its tools
      stay reachable; it is removed only in Phase 2 when merged into Recon, per
      the "keep old submenus reachable so nothing regresses mid‑migration" rule.
      Phase 1 top menu: `Scan & Connect · Recon · Sniffers · Attacks · Wardrive ·
      802.15.4 · Peer Link · General`.
- [x] Tests: extended `test_wifi_ux_restructure.c` (new label assertion +
      `test_wardrive_is_top_level_menu`); firmware build verified.

### Phase 2 — Recon/Sniffers merge, General split, 802.15.4 Flood — **DONE**
- [x] Merge Sniffers into Recon (§3.4): deleted the standalone `Sniffers` top
      item and its sub-menu scene; the seven passive captures now live in
      **Recon** with C3-style naming (`Packet Monitor`, `Beacon Sniff`, …) and
      inherit Recon's disconnect prompt. Top menu is now `Scan & Connect ·
      Recon · Attacks · Wardrive · 802.15.4 · Peer Link · General`.
- [x] Moved the list-management items (Save/Load/Clear APs, Load/Clear SSIDs)
      into the **Wardrive** sub-menu (§3.5). The delegates still live in
      `m1_wifi_scene_general.c`; only their menu placement moved.
- [x] Slimmed **General**: list-management removed; **Status** and
      **Disconnect** surfaced alongside Saved Networks. The remaining
      config/selection items (View AP Info, Select APs/STAs, Join, Set
      MACs/Channel, Shutdown, EP SSID/HTML) are **kept in General for now** so
      nothing becomes unreachable — they relocate to the selected-network
      **Target** group (P3) and the **Attacks**/Evil-Portal config (P4), per the
      Phase 1 "keep old submenus reachable so nothing regresses mid-migration"
      rule. General reaches its final Saved/Status/Disconnect shape once P3/P4
      land.
- [x] Added capability-gated **802.15.4 Flood** (`ieee802154_flood()`, gated on
      `ESP32_FEATURE_802154`, driving `AT+ZIGFLOOD`).
- [x] Tests: extended `test_wifi_ux_restructure.c` with menu-content assertions
      for the Recon merge, Wardrive list-mgmt absorption, the slimmed General,
      and the 802.15.4 Flood entry (23/23 pass).

### Phase 3 — Selected‑network context (Target + Connect groups) — **DONE**
- [x] New selected‑SSID scene (`m1_wifi_scene_target.c`, `WifiSceneTargetMenu`)
      with the two precondition groups (§3.2): a **Connect** item (auth group)
      and the SSID‑scoped **Target** actions. Pressing OK on a network in
      **Scan & Connect** flags a pending target (`wifi_scan_ap_target_selected()`)
      and the Scan & Connect delegate pushes the Target menu for the highlighted
      AP; **Connect** `m1_scene_replace()`s into the existing Connected menu on
      success, wiring it as the Connect target.
- [x] Moved per‑AP **Deauth**, **Handshake/EAPOL** capture, targeted **Beacon**
      (clone this SSID) and **PMKID** into the Target group as `wifi_target_*`
      wrappers that act on the highlighted AP; implemented **Cycle AP** on top of
      the new pure `wifi_ap_cycle_next()` helper so the Target actions iterate
      the known BSSIDs of one SSID. The Target actions are unreachable except via
      a selected network.
- [x] Tests: pure‑logic `test_wifi_ap_cycle.c` (AP‑cycling over `wifi_ap_t[]`,
      8/8) plus source assertions in `test_wifi_ux_restructure.c` that the Target
      scene exposes both groups, that Scan & Connect opens it on selection, and
      that the actions require a selected network (26/26).
- The per‑AP Deauth also stays reachable from **Attacks → Deauth** for now (its
  multi‑target selection harness lands in Phase 4); nothing is stranded.

### Phase 4 — Attack consolidation & multi‑target flow — **DONE**
- [x] Collapsed Karma/Karma+Portal → **Karma(+toggle)**: new
      `wifi_attack_karma_with_portal()` prompts "Serve portal? [Yes] [No]" and
      routes to `wifi_attack_karma_portal()` or `wifi_attack_karma()` accordingly.
      "Karma+Portal" removed from the Attacks menu (8 items); the scene handler
      is retained in the registry (append-only rule) but unreachable via the menu.
- [x] Extracted pure-logic **`wifi_multi_target_build()`** (`wifi_multi_target.h/.c`):
      builds a flat target list from the selected entries of an AP list, with no
      HAL/RTOS deps. Seeds multi-target attack flows from the current `ap_list[]`
      (populated from live scan or loaded saved list — "both" per §7 answer).
- [x] Added **post-deauth chaining prompt** to `wifi_target_deauth()` (§3.8):
      after the deauth loop completes the user is offered
      `[Handshake] [Evil Portal] [Done]` to immediately chain into EAPOL capture
      or Evil Portal without returning to the menu.
- [x] Tests: `test_wifi_multi_target.c` (11/11, pure-logic) +
      source assertions 27–31 in `test_wifi_ux_restructure.c` (31/31 pass).

### Phase 5 — WiFi Hotspot (deferred / optional)
- Capability‑gated SoftAP hotspot entry (§3.9). Ships only if ESP32 firmware
  support is confirmed; otherwise remains hidden behind the capability gate.
- Tests: capability‑gate assertion (feature hidden when cap absent).

---

## 6. Cross‑firmware & compatibility guardrails

- Every state‑dependent or firmware‑dependent action stays behind
  `DELEGATE_FEATURE()` / `esp32_feature_required_caps()` so stock‑AT, dag‑AT and
  SiN360 builds each see only what they support — no new per‑firmware `#ifdef`
  sprawl.
- Connection‑gated menus continue to use `wifi_prompt_disconnect()` /
  `wifi_require_connected()`; the connect flow keeps compile‑gating on
  `M1_APP_WIFI_CONNECT_ENABLE`.
- No change to the 20‑byte `S_M1_FW_CONFIG_t` or any on‑SD file formats
  (`wifi_creds.bin`, saved AP/SSID lists) — the cleanup is UI/navigation only.
- Scene‑ID enum values are append‑only within each migration step to avoid
  churn in the `scene_registry[]` table.

---

## 7. Open questions for the owner

1. Should **WiFi Hotspot** and **Peer Link** live under a future top‑level
   **Tools** menu (as C3 does for Peer Link), or stay under WiFi as today? Hapax: do not add **Tools** menu, leave here for now
2. For multi‑target attacks, is the selection set seeded from the persisted
   "Select APs" list, from live scan results, or both? Hapax: both
3. Preferred label casing/order for the final top menu (this plan proposes
   `Scan & Connect · Recon · Attacks · Wardrive · 802.15.4 · Peer Link ·
   General`).

These are non‑blocking for Phase 1 and can be resolved as later phases land.
