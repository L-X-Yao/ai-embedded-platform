# varint

**Status: candidate**
**Layer: L0 (primitives)**

Variable-length integer encoding using LEB128 (protobuf-compatible).
Unsigned uses standard LEB128 (7 data bits per byte, bit 7 = continuation).
Signed uses ZigZag encoding: (n << 1) ^ (n >> 63). Supports uint64 and
int64. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). Variable-length integer encoding using LEB128 (protobuf-compatible).


## Why self-written?

existing open-source has no varint codec. This wheel is a building block for protobuf_lite,
msgpack, and other compact serialization protocols used in telemetry.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "varint.h"

varint_err_t varint_encode_u64(uint64_t value, uint8_t *buf, size_t cap, size_t *out_len);
varint_err_t varint_decode_u64(const uint8_t *buf, size_t len,
                               uint64_t *value, size_t *out_len);
varint_err_t varint_encode_s64(int64_t value, uint8_t *buf, size_t cap, size_t *out_len);
varint_err_t varint_decode_s64(const uint8_t *buf, size_t len,
                               int64_t *value, size_t *out_len);
size_t       varint_encoded_size_u64(uint64_t value);
```

## Usage

```c
uint8_t buf[10];
size_t len;
varint_encode_u64(300, buf, sizeof(buf), &len);  /* len == 2 */

uint64_t val;
size_t consumed;
varint_decode_u64(buf, len, &val, &consumed);     /* val == 300 */
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/primitives\varint/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_varint
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

@L-X-Yao
