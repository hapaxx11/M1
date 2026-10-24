**WiFi: bounded AP-scan wait can no longer free resources under a live
  worker** — the brain-CD3 scan job (async context, results buffer, semaphore)
  moved to the heap; a worker that outlives the cancel+drain window has its job
  orphaned and reaped on the next scan only after it signals completion,
  eliminating the use-after-free in the drain-timeout path.
