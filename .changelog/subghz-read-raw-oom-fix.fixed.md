**Sub-GHz Read Raw: fixed "Record failed / Low memory" OOM on SiN360 firmware** — ring
  buffer allocations (front-buffer up to 128 KB, ring-read 1 KB, SD-write 3.75 KB)
  switched from the newlib heap (`malloc`) to the FreeRTOS heap (`pvPortMalloc`).
  The SiN360 binary-SPI integration enlarged the firmware's BSS significantly,
  leaving the newlib heap too small to hold the capture buffers; the FreeRTOS heap
  retains >180 KB free and absorbs the allocation without issue.  The on-screen
  "Heap free" counter now correctly reflects the pool actually used for recording
  buffers.
