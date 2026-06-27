**Sub-GHz Read Raw: re-apply #526 FreeRTOS-heap behaviour to the record path** —
  the SD write buffer (`m1_sdm_memory_init`) and the Read Raw capture reserve probe
  now allocate directly from the FreeRTOS heap-4 pool via `pvPortMalloc`/`vPortFree`
  instead of routing through `malloc_critical`, which wrapped a scheduler-suspending
  allocator inside a critical section — an anti-pattern the FreeRTOS V11 upgrade
  (#589) made riskier.  The "Record failed" screen now distinguishes an SD/FatFs
  fault ("SD card error") from a write-buffer heap shortage ("SD mem error") so the
  cause is visible on-device.  Added a mandatory post-RTOS-update Heap-Redirect
  Component Checklist to CLAUDE.md so future updates cannot silently override the
  #526 heap redirect (closes #610).
