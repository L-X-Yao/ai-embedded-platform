# crsf

**Status: candidate**
**Layer: L2 (protocols)**

TBS Crossfire / ExpressLRS (CRSF) RC protocol codec. Encodes and decodes
CRSF frames with CRC-8 DVB-S2 (poly 0xD5) validation. Packs and unpacks
16 channels of 11-bit RC data (range 172-1811). Pure C99, no heap, no OS.

## Why not VSF native?

VSF has no RC protocol support. CRSF is the dominant protocol for
ExpressLRS receivers used in drone/UAV projects.

## API

```c
#include "crsf.h"

uint8_t    crsf_crc8(const uint8_t *data, size_t len);
crsf_err_t crsf_decode_frame(const uint8_t *data, size_t len, crsf_frame_t *out);
crsf_err_t crsf_encode_frame(const crsf_frame_t *frame,
                              uint8_t *buf, size_t cap, size_t *out_len);
crsf_err_t crsf_unpack_channels(const uint8_t *payload, size_t len,
                                 crsf_channels_t *out);
crsf_err_t crsf_pack_channels(const crsf_channels_t *channels,
                               uint8_t *payload, size_t cap);
```

## Usage

```c
crsf_frame_t frame;
if (crsf_decode_frame(uart_buf, uart_len, &frame) == CRSF_OK) {
    if (frame.type == CRSF_TYPE_RC_CHANNELS) {
        crsf_channels_t ch;
        crsf_unpack_channels(frame.payload, frame.payload_len, &ch);
        int16_t throttle = ch.channels[2];  /* channel 3 */
    }
}
```

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
