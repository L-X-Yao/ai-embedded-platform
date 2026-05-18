# varint

**Status: candidate**
**Layer: L0 (primitives)**

Variable-length integer encoding using LEB128 (protobuf-compatible).
Unsigned uses standard LEB128 (7 data bits per byte, bit 7 = continuation).
Signed uses ZigZag encoding: (n << 1) ^ (n >> 63). Supports uint64 and
int64. Pure C99, no heap, no OS.

## Why not VSF native?

VSF has no varint codec. This wheel is a building block for protobuf_lite,
msgpack, and other compact serialization protocols used in telemetry.

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

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
