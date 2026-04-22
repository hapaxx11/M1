**Sub-GHz: guard `sub_ghz_ring_buffers_init()` against double-allocation** — calling
  the function while buffers are already live no longer leaks the prior allocation;
  a preceding `sub_ghz_ring_buffers_deinit()` call ensures a clean slate and prevents
  heap fragmentation from triggering spurious "Memory error" (error 4) failures.
