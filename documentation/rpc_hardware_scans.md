# RPC Hardware Scans

These commands use the existing M1 USB RPC frame format. Responses preserve the
request sequence number.

| Command | ID | Request payload | Response |
|---|---:|---|---|
| GPIO scan | `0x70` | Empty | `0x71`: pin count, then little-endian level bitset |
| I2C scan | `0x72` | First and last 7-bit address, inclusive | `0x73`: found count, then ascending 7-bit addresses |

The GPIO response reports a read-only snapshot of the 15 `m1_ext_gpio` entries.
Bit `N` is the sampled level for entry `N`, in the order defined by
`m1_gpio.c` (`0`–`2` are power-control signals; `3`–`14` are external pins).
The high bit of the second bitmap byte is unused.

I2C requests must use an inclusive range from `0x08` through `0x77`; reserved
addresses outside that range and reversed ranges are rejected. The scan probes
each address once while holding the shared I2C transaction mutex. A scan error
returns NACK error `0x0C`; malformed requests return `0x02`, and an occupied
deferred-operation slot returns `0x03`. I2C requests run in the RPC worker task
so probing does not stall USB receive processing.
