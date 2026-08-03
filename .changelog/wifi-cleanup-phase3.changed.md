**WiFi: menu cleanup Phase 3** — Added the selected-network **Target** context:
  pressing OK on a network in **Scan & Connect** now opens a per-network menu
  that separates the **Connect** action (joins the network → Connected menu)
  from the SSID-scoped **Target** actions (per-AP Deauth, Handshake/EAPOL
  capture, Beacon clone, PMKID) plus **Cycle AP** to iterate the known BSSIDs of
  a single SSID. The Target actions are only reachable once a network is
  selected, so each action's precondition is satisfied by construction. Third
  phase of the WiFi UX restructuring plan (`documentation/wifi_cleanup_plan.md`).
