/* Minimal portable.h stub for host-side unit tests.
 *
 * The real FreeRTOS portable.h declares pvPortMalloc / vPortFree / etc.
 * On the host these are already #defined to libc in the FreeRTOS.h stub,
 * so this file only needs to exist to satisfy the #include. */
#ifndef PORTABLE_H_STUB
#define PORTABLE_H_STUB

#include "FreeRTOS.h"

#endif /* PORTABLE_H_STUB */
