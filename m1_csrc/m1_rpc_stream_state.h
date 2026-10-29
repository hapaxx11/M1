/* See COPYING.txt for license details. */

#ifndef M1_RPC_STREAM_STATE_H_
#define M1_RPC_STREAM_STATE_H_

#include <stdbool.h>

static inline bool m1_rpc_screen_stream_lock_ready(const void *mutex)
{
    return mutex != NULL;
}

#endif /* M1_RPC_STREAM_STATE_H_ */
