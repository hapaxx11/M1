**Sub-GHz: pure-logic `subghz_submenu_model` widget foundation** —
  Adds a 4-byte hardware-independent model for scrollable list scenes
  (item_count / selected / scroll_offset / visible_count) with wrap-around
  navigation, scroll-window anchoring, and empty/zero-visible safety.
  First step toward consolidating the hand-rolled list code in every
  Sub-GHz menu scene onto a shared widget.  25 host tests under ASan+UBSan
  pin every invariant.
