# ibus

**Status: candidate**
**Layer: L2 (protocols)**

FlySky iBUS serial protocol codec for RC receivers. Decodes and encodes
32-byte frames carrying 14 channels of 16-bit servo data (range 1000-2000,
center 1500). Frame format: `[0x20] [0x40] [ch1_lo ch1_hi ... ch14_lo ch14_hi] [cksum_lo cksum_hi]`.
Pure C99, no heap, no OS.

## Why not VSF native?

VSF has no RC receiver protocol support. iBUS is the standard protocol
for FlySky receivers commonly used in hobby drones and RC vehicles.

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

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
