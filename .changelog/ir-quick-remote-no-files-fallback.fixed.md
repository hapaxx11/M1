**Infrared: fix category Quick Remote dead-ending with "No IR files found."**
  — When a category (TV Remote, AC Remote, Audio, Projector, Fan, LED) had no files
  in its expected IRDB subfolder AND no files at the `0:/IR` root, `browse_for_device()`
  showed a static error and returned — with no way for the user to navigate to files
  stored elsewhere on the SD card.  Replaced the dead-end `return false` with an
  `m1_file_browser` fallback (matching the "Browse IRDB" and "Learned" patterns from
  the previous fix) so the user can always navigate up and find their `.ir` files.
