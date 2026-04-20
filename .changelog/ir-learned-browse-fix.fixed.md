**Infrared: fix "Learned" and Universal Remote showing "file not found" with no browse option**
  — `Infrared > Replay` now goes directly to the Learned files browser instead of the
  full Universal Remote dashboard, so saved .ir files are immediately accessible.
  — Quick-remote categories (TV, AC, Audio, etc.) now fall back to browsing the full
  `0:/IR/` tree when the category subdirectory is empty, instead of showing a dead-end
  "No files found" message with no way to navigate.
  — Empty-directory messages are now context-sensitive: the Learned folder tells the user
  to use the Learn screen; other empty folders tell the user where to copy .ir files.
