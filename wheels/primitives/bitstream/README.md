# bitstream -- bit-level read/write over a byte buffer

**Status: candidate**
**Layer: L0 (primitives)**

Bit-level reader/writer that packs and unpacks N-bit unsigned values
from/to a byte buffer at arbitrary bit offsets. LSB-first packing.
Useful for packed protocols (CRSF, SBUS, CAN signals) and binary
serialization formats.

## Origin

Proactive build. Bit-level packing is needed by SBUS, CRSF, CAN signal
extraction, and other packed binary protocols in the wheel ecosystem.

## Why not VSF native?

No VSF equivalent. Textbook utility self-written for platform convention
consistency and test harness integration.

## Dependencies

None.

## API

```c
bitstream_err_t bitstream_init(bitstream_t *ctx, uint8_t *buf, size_t cap_bytes);
bitstream_err_t bitstream_write(bitstream_t *ctx, uint32_t value, uint8_t nbits);
bitstream_err_t bitstream_read(bitstream_t *ctx, uint8_t nbits, uint32_t *out);
size_t          bitstream_tell(const bitstream_t *ctx);
bitstream_err_t bitstream_seek(bitstream_t *ctx, size_t bit_pos);
bitstream_err_t bitstream_reset(bitstream_t *ctx);
```

- `write()` -- write up to 32 bits (LSB first)
- `read()` -- read up to 32 bits (LSB first)
- `seek()` -- jump to arbitrary bit position
- Returns `BITSTREAM_ERR_OVERFLOW` if read/write exceeds buffer capacity
- Caller provides backing buffer; no heap allocation

## Usage

```c
uint8_t buf[4] = {0};
bitstream_t bs;
bitstream_init(&bs, buf, sizeof(buf));

bitstream_write(&bs, 0x1FF, 9);   /* write 9-bit value */
bitstream_write(&bs, 3, 2);       /* write 2-bit value */

bitstream_seek(&bs, 0);
uint32_t val;
bitstream_read(&bs, 9, &val);     /* val == 0x1FF */
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/bitstream/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bitstream
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
