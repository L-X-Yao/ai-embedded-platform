# modbus_rtu — Modbus RTU frame codec with master/slave

**Status: candidate**

Stateless frame encoder/decoder for Modbus RTU (register operations).
Master builds requests and parses responses; slave dispatches to register
callbacks. Zero OS dependency — consumer handles UART framing (3.5 char gap)
and timeout externally.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`modbus`, `modbus_rtu`, `modbus_master`, `modbus_slave` — zero results.

VSF has no Modbus implementation.

## Origin

Written for the platform's industrial project pipeline (Modbus RTU is the
dominant protocol in Chinese industrial automation). API shape derived from
the Modbus Application Protocol Specification V1.1b3.

## Dependencies

- `crc16` (L0, concrete) — CRC16-MODBUS frame check per ADR-009

## Supported platforms

- [x] Host (sim) — passed 2026-04-28
- [ ] Real hardware — not yet verified (no project consumer)

## Features

- **Frame layer**: build/parse/validate any Modbus RTU ADU with CRC16
- **Master helpers**: FC 03 (Read Holding), FC 04 (Read Input), FC 06 (Write Single), FC 10 (Write Multiple)
- **Slave dispatcher**: request parsing, register callback dispatch, exception response generation
- **Broadcast**: slave executes writes silently on addr 0, skips reads
- **Exception handling**: ILLEGAL_FUNCTION, ILLEGAL_DATA_ADDR, ILLEGAL_DATA_VALUE, SLAVE_FAILURE
- **Compile-time selection**: `MODBUS_RTU_MASTER` / `MODBUS_RTU_SLAVE` (both on by default)
- **Zero-copy parse**: `modbus_rtu_frame_t.data` points into original buffer

## API

```c
/* --- Frame layer (shared) --- */
bool modbus_rtu_check_crc(const uint8_t *frame, uint16_t len);
int  modbus_rtu_parse(const uint8_t *buf, uint16_t len, modbus_rtu_frame_t *frame);
int  modbus_rtu_build(uint8_t *buf, uint16_t buf_size,
                      uint8_t addr, uint8_t func,
                      const uint8_t *data, uint16_t data_len);

/* --- Master --- */
int modbus_rtu_master_read_regs(buf, buf_size, slave, func, start, count);
int modbus_rtu_master_write_reg(buf, buf_size, slave, reg, value);
int modbus_rtu_master_write_regs(buf, buf_size, slave, start, count, values);
int modbus_rtu_master_get_regs(frame, regs, max_count);

/* --- Slave --- */
modbus_rtu_slave_t slave;
modbus_rtu_slave_init(&slave, 0x01, &my_ctx);
slave.read_holding  = my_read_holding_cb;
slave.read_input    = my_read_input_cb;
slave.write_holding = my_write_holding_cb;

int resp_len = modbus_rtu_slave_process(&slave, req, req_len, resp, resp_size);
if (resp_len > 0) uart_send(resp, resp_len);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `MODBUS_RTU_MASTER` | 1 | Include master request builders |
| `MODBUS_RTU_SLAVE` | 1 | Include slave dispatcher |
| `MODBUS_MAX_READ_REGS` | 125 | Max registers per read (Modbus spec) |
| `MODBUS_MAX_WRITE_REGS` | 123 | Max registers per write (Modbus spec) |

## Known issues

- No coil/discrete operations (FC 01/02/05/0F) — register operations only. Add when a project needs them.
- No built-in timeout — consumer must implement inter-frame gap detection and response timeout.
- No command queue — single request/response at a time.
- Slave stack usage: up to ~251 bytes for read response data array (reduce `MODBUS_MAX_READ_REGS` if stack-constrained).

## Tests

```bash
cd wheels/protocols/modbus_rtu/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_modbus_rtu
```

109 assertions across 34 test cases: frame build/parse/CRC, master request builders (FC 03/04/06/10), master response parsing, slave dispatch (read holding/input, write single/multiple), address mismatch, broadcast (write executes + no response, read skipped), exception responses (unknown FC, missing callback, callback failure), write_regs count boundary validation, null safety, master-slave round-trip integration.

## Test status

- Unit tests (sim): PASS (34 C tests, 109 assertions)
- HIL: N/A

## API review

- **Date**: 2026-04-28
- **Key decisions**: Stateless frame layer (pure functions on buffers); master/slave in one wheel with compile-time selection; concrete dependency on crc16 per ADR-009; register-only (no coils in v1); slave uses per-register callbacks (not block read/write); broadcast per Modbus spec (writes execute, no response).
