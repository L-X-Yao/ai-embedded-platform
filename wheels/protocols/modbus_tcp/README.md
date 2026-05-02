# modbus_tcp --- Modbus TCP/IP frame encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

Stateless frame encoder/decoder for Modbus TCP/IP (MBAP header + PDU).
Builds request frames and parses responses for register operations.
Zero OS dependency --- consumer handles TCP connections externally.

## Origin

Proactive build for industrial Ethernet projects. Modbus TCP is the TCP/IP
variant of Modbus RTU --- same PDU but with MBAP header instead of CRC.
Companion to the existing `modbus_rtu` wheel.

## Why self-written?

Searched GitHub (2026-05-02). Referenced debevv/nanoMODBUS (MIT). Same library covers TCP variant. Self-written for unified error handling and zero-allocation guarantee; implementation verified against Modbus TCP/IP spec (MBAP header, protocol ID, unit ID framing).

## Dependencies

None.

## API

```c
/* Build request frames (return frame length or negative error) */
int modbus_tcp_build_read_regs(buf, buf_size, transaction_id, unit_id,
                               start_addr, count);
int modbus_tcp_build_write_reg(buf, buf_size, transaction_id, unit_id,
                               addr, value);
int modbus_tcp_build_write_regs(buf, buf_size, transaction_id, unit_id,
                                start_addr, values, count);

/* Parse response from raw bytes (zero-copy) */
modbus_tcp_err_t modbus_tcp_parse(buf, len, &frame);

/* Extract registers from parsed read response */
modbus_tcp_err_t modbus_tcp_get_regs(&frame, values, max_count, &actual_count);

/* Inline helpers */
bool    modbus_tcp_is_exception(&frame);
uint8_t modbus_tcp_exception_code(&frame);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `MODBUS_TCP_MAX_READ_REGS` | 125 | Max registers per read (Modbus spec) |
| `MODBUS_TCP_MAX_WRITE_REGS` | 123 | Max registers per write (Modbus spec) |

## Supported platforms

- [x] Host (sim) --- passed 2026-05-01
- [ ] Real hardware --- not yet verified (no project consumer)

## Known issues

- No coil/discrete operations (FC 01/02/05/0F) --- register operations only. Add when a project needs them.
- No FC 04 (Read Input Registers) --- add when needed.
- No built-in transaction tracking --- consumer manages transaction_id matching.
- Codec only --- no connection management, no retransmit, no keep-alive.

## How to test

```bash
cd wheels/protocols/modbus_tcp/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_modbus_tcp
```

23 test cases: MBAP header encoding (FC 03/06/16), response parsing (read/write/exception), protocol_id validation, short buffer rejection, transaction_id preservation, unit_id round-trip, buffer overflow protection, null parameter safety, multi-register (5+) build and extract, build-parse round-trips, exception-to-get_regs error propagation, actual_count NULL tolerance.

## Test status

- Unit tests (sim): PASS (23 C tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: MBAP header struct exposed for consumer inspection; build functions return frame length (positive) or error code (negative); parse is zero-copy (data pointer into input buffer); no CRC (TCP layer handles reliability); separate from modbus_rtu (different framing, shared PDU concept but independent implementation to avoid coupling).

## Responsible human

@user

## Features

- **MBAP header**: transaction_id (2B) + protocol_id (2B, always 0) + length (2B) + unit_id (1B)
- **Function codes**: FC 03 (Read Holding), FC 06 (Write Single), FC 16 (Write Multiple)
- **Zero-copy parse**: `modbus_tcp_frame_t.data` points into original buffer
- **Exception detection**: `modbus_tcp_is_exception()` / `modbus_tcp_exception_code()`
- **No CRC**: TCP provides reliability (unlike RTU)
- **Codec only**: application handles TCP socket lifecycle
