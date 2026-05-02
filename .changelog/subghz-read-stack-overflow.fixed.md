**Sub-GHz: fix hard crash when entering any Sub-GHz scene** — `SubGhzApp` (5 KB) was
  allocated on the menu task's 4 KB stack, causing a guaranteed stack overflow on every
  entry. Moved to static storage (BSS) so it no longer touches the stack.
