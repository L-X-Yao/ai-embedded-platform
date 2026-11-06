# ibus

**Status: candidate**
**Layer: L2 (protocols)**

FlySky iBUS serial protocol codec for RC receivers. Decodes and encodes
32-byte frames carrying 14 channels of servo data (range 1000-2000,
center 1500), each in the low 12 bits of a 16-bit word. Frame format: `[0x20] [0x40] [ch1_lo ch1_hi ... ch14_lo ch14_hi] [cksum_lo cksum_hi]`.
Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). i


## Why self-written?

existing open-source has no RC receiver protocol support. iBUS is the standard protocol
for FlySky receivers commonly used in hobby drones and RC vehicles.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "ibus.h"

ibus_err_t ibus_decode(const uint8_t *data, size_t len, ibus_frame_t *out);
ibus_err_t ibus_encode(const ibus_frame_t *frame, uint8_t *buf, size_t cap);
uint16_t   ibus_checksum(const uint8_t *data, size_t len);
```

## Usage

```c
/* Decode a received 32-byte UART frame */
ibus_frame_t frame;
if (ibus_decode(uart_buf, 32, &frame) == IBUS_OK) {
    uint16_t throttle = frame.channels[2];  /* 1000-2000 */
    uint16_t yaw      = frame.channels[3];
}

/* Encode for simulation / forwarding */
ibus_frame_t tx = { .channels = {1500, 1500, 1000, 1500} };
uint8_t buf[IBUS_FRAME_LEN];
ibus_encode(&tx, buf, sizeof(buf));
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

- Channels 15 to 18, which newer receivers carry in the top nibbles, are not
  decoded: `ibus_decode` drops those bits and `ibus_frame_t` has no field for them.


## How to test

```bash
cd wheels/protocols\ibus/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ibus
```


## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
