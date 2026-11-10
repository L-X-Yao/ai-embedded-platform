# crsf

**Status: candidate**
**Layer: L2 (protocols)**

TBS Crossfire / ExpressLRS (CRSF) RC protocol codec. Encodes and decodes
CRSF frames with CRC-8 DVB-S2 (poly 0xD5) validation. Packs and unpacks
16 channels of 11-bit RC data (range 172-1811). Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). TBS Crossfire / ExpressLRS (CRSF) RC protocol codec. Encodes and decodes


## Why self-written?

existing open-source has no RC protocol support. CRSF is the dominant protocol for
ExpressLRS receivers used in drone/UAV projects.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

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

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## Migration

- `CRSF_MAX_PAYLOAD` (60) and `CRSF_ERR_FORMAT` are new. `CRSF_MAX_FRAME_SIZE` is
  still 64, and now limits what it always said: a whole frame.
- Encoding a payload of 61 to 64 bytes now returns `CRSF_ERR_OVERFLOW`: split it.
  Such a frame was over the spec's limit, and receivers must discard it.
- Decoding a length byte outside 2 to 62 now returns `CRSF_ERR_FORMAT` as soon as
  the byte is there. It used to be `CRSF_ERR_SHORT` or `CRSF_ERR_OVERFLOW`, or OK
  for 63 to 66. A stream reader that waits on `CRSF_ERR_SHORT` should treat
  `CRSF_ERR_FORMAT` as a bad frame and look for the next sync byte. One that goes
  on waiting for any non-OK result needs that change, or it stalls on a bad byte.
- `crsf_frame_t` keeps its size.

## How to test

```bash
cd wheels/protocols\crsf/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_crsf
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
