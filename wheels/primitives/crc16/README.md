# crc16

**Status: candidate**
**Layer: L0 (primitives)**

CRC16-MODBUS computation (table-based, ~1 KB ROM). Drop-in for Modbus RTU
frame check, custom telemetry frames using the same polynomial, and any
other CRC16 with poly 0xA001 / init 0xFFFF / no final XOR.

## Origin

Reimplemented for the needs of an internal project (CRC16-MODBUS for telemetry frames). Also consumed by `wheels/protocols/modbus_rtu/` as concrete dependency.

## Why self-written?

Searched GitHub (2026-05-02). Found lammertb/libcrc (MIT). Standard textbook CRC16-MODBUS algorithm. Self-written for platform convention consistency (unified error codes, HAL injection, test harness).

## Dependencies

None.

## API

```c
#include "crc16.h"

#define CRC16_MODBUS_INIT  0xFFFFu

/* One-shot bulk computation */
uint16_t crc16_modbus(const uint8_t *data, size_t len);

/* Streaming single-byte step (DMA / 网络收包用) */
uint16_t crc16_modbus_step(uint16_t crc, uint8_t byte);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-26
- [x] WSL Ubuntu 24.04 — passed 2026-04-26
- [x] ARM Cortex-M4 (GD32F405 cross-compile) — integrated into flybox 2026-04-26, HIL verified (the code as of that date; later changes have not run on a board)

## Known issues

None. Pure stateless computation, alignment/endianness irrelevant (byte-level).

## How to test

```bash
cd wheels/primitives/crc16
cmake -GNinja -S test -B test/build
cmake --build test/build
python3 -m pytest test/test_crc16.py -v
```

预期：**1 个 C 测试 harness（12 个内嵌 case） + 9 个 pytest 用例全绿。**

## Test status

- Unit tests: **PASS** (12 C cases + 9 pytest cases, 17 assertions)
- Test vectors:
  - Empty buffer (NULL + len=0) → 0xFFFF
  - "123456789" standard MODBUS test vector → 0x4B37
  - Modbus RTU frame (read 10 registers) → 0xCDC5
  - Single byte 0x00 / 0xFF
  - Step API equivalent to bulk API
  - Segmented step (3+4+3) equivalent to one-shot
  - 34-byte deterministic pattern (regression guard)
  - 256-byte all-zeros / all-ones buffers (step vs bulk consistency)
  - Single-byte uniqueness (all 256 CRCs distinct — proves table collision-free)
  - CRC residue self-check (append CRC little-endian → residue is 0x0000)
  - 1024-byte buffer step vs bulk equivalence
- HIL: N/A (pure compute); flybox integration verified

## API convention deviations

无。函数命名 `crc16_<verb>` 符合 api-conventions.md §1.1。

## Performance reference

- Time complexity: O(n), 1 table lookup + 1 XOR per byte
- ROM footprint: ~512 bytes (256-entry × 2-byte table) + ~50 bytes code
- Stack usage: < 16 bytes (GCC -fstack-usage verified)
- ~8x faster than vsf_crc bit-by-bit implementation

## Usage example

**Modbus RTU frame check:**

```c
uint8_t frame[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A };
uint16_t crc = crc16_modbus(frame, sizeof(frame));
/* Modbus on-wire is little-endian: send low byte first */
send_byte((uint8_t)(crc & 0xFF));
send_byte((uint8_t)(crc >> 8));
```

**Streaming over DMA chunks:**

```c
uint16_t crc = CRC16_MODBUS_INIT;
while (rx_byte_available()) {
    uint8_t b = rx_byte();
    crc = crc16_modbus_step(crc, b);
}
/* crc now equals crc16_modbus(<all bytes received>, total_len) */
```

## Features

- CRC16-MODBUS (poly 0xA001 reflected, init 0xFFFF, no final XOR)
- Bulk + incremental step API
- 256-entry lookup table (~512 bytes ROM)
- Pure C99, zero dependencies (no VSF / no RTOS / no syscall)
- Fully reentrant, no global state

## API review

- 提案日期：2026-04-26
- 审批人：@user
- 关键决策：
  - 第一版只装 MODBUS variant（不做 CCITT-FALSE / KERMIT 等其他多项式）
  - 接受 "crc16" wheel 名（不叫 crc16_modbus），明确 README 写"目前只 MODBUS"
  - 提供 step API（streaming）+ bulk API 两种
  - 不依赖 VSF（`vsf_crc()` 算法不兼容 MODBUS reflected）

## Responsible human

@L-X-Yao
