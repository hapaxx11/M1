**UI/UX: Button bar consistency audit** — Fixed numerous button bar hints
  across scene and legacy modules that pointed at the wrong physical button
  (e.g. "Back"/"Stop" labels shown in the LEFT slot when only the hardware
  BACK button dismissed the screen, or "OK"-triggered actions like
  Send/Save/Write/Clone shown in the RIGHT slot instead of the CENTER slot).
  Extracted the generic 3-column button bar renderer into a shared
  `m1_button_bar.c`/`.h` module so non-Sub-GHz modules no longer depend on
  the Sub-GHz-specific wrapper.
