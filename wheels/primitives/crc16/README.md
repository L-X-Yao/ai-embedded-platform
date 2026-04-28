# crc16

**STATUS: candidate**
**Layer: L0 (primitives)**

CRC16-MODBUS computation (table-based, ~1 KB ROM). Drop-in for Modbus RTU
frame check, custom telemetry frames using the same polynomial, and any
other CRC16 with poly 0xA001 / init 0xFFFF / no final XOR.

## Why not VSF native?

已检查 VSF 模块（2026-04-26）：

- **`vsf_crc()`**（`vsf/source/component/crypto/hash/crc/vsf_crc.{h,c}`）：✓ 提供 generic
  CRC engine，支持任意 poly + bitlen。**但 ships 只有 CCITT variants**：
  `vsf_crc8_ccitt` (poly 0x07), `vsf_crc16_ccitt` (poly 0x1021)。
- **算法不是 drop-in for MODBUS**：vsf_crc 引擎是 **left-shift non-reflected**
  （byte XOR 进高字节，向左移）；MODBUS 是 **right-shift reflected**（byte XOR 进
  低字节，向右移）。直接喂 `{bitlen=16, poly=0xA001}` 会得到错的结果——poly
  reverse 不是。

**两种修法：**

1. 给 VSF 加 `reflect` 标志位 → 上游 PR 工作（ADR-005 patches workflow）
2. 独立 wheel，专门做 MODBUS variant

**选 #2** 因为：
- MODBUS variant 单独存在的需求很高（Modbus RTU 是工业总线主流，flybox 自定义
  遥测帧也用 0xA001）
- Table-based 比 vsf_crc 的 bit-by-bit 快 ~8x（每字节 1 次查表 vs 8 次循环）
  ——对 flash ring frame check 这种高频 CRC 用例 ROI 明显
- ~1 KB ROM 在嵌入式可接受

**不替换 vsf_crc**——本 wheel 不提供 CCITT、CCITT-FALSE 等其他 variant；那些
继续用 vsf_crc 即可。本 wheel 是 MODBUS 专家。

## How to test

```bash
cd wheels/primitives/crc16
cmake -GNinja -S . -B build
cmake --build build
python3 -m pytest test_crc16.py -v
```

预期：**1 个 C 测试 harness（7 个内嵌 case） + 9 个 pytest 用例全绿。**

## API

```c
#include "crc16.h"

#define CRC16_MODBUS_INIT  0xFFFFu

/* One-shot bulk computation */
uint16_t crc16_modbus(const uint8_t *data, size_t len);

/* Streaming single-byte step (DMA / 网络收包用) */
uint16_t crc16_modbus_step(uint16_t crc, uint8_t byte);
```

## Features

- CRC16-MODBUS (poly 0xA001 reflected, init 0xFFFF, no final XOR)
- Bulk + incremental step API
- 256-entry lookup table (~512 bytes ROM)
- Pure C99, zero dependencies (no VSF / no RTOS / no syscall)
- Fully reentrant, no global state

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-26
- [x] WSL Ubuntu 24.04 — passed 2026-04-26
- [x] ARM Cortex-M4 (GD32F405 cross-compile) — integrated into flybox 2026-04-26, HIL verified

## Dependencies

None.

## Known issues

None. Pure stateless computation, alignment/endianness irrelevant (byte-level).

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

## Test status

- Unit tests: **PASS** (7 C cases + 9 pytest cases)
- Test vectors:
  - Empty buffer (NULL + len=0) → 0xFFFF
  - "123456789" standard MODBUS test vector → 0x4B37
  - Modbus RTU frame (read 10 registers) → 0xCDC5
  - Single byte 0x00 / 0xFF
  - Step API equivalent to bulk API
  - Segmented step (3+4+3) equivalent to one-shot
  - flybox 34-byte deterministic frame (regression guard)
- HIL: N/A (pure compute); flybox integration verified

## Performance reference

- Time complexity: O(n), 1 table lookup + 1 XOR per byte
- ROM footprint: ~512 bytes (256-entry × 2-byte table) + ~50 bytes code
- Stack usage: < 16 bytes (GCC -fstack-usage verified)
- ~8x faster than vsf_crc bit-by-bit implementation

## API review

- 提案日期：2026-04-26
- 审批人：@user
- 关键决策：
  - 第一版只装 MODBUS variant（不做 CCITT-FALSE / KERMIT 等其他多项式）
  - 接受 "crc16" wheel 名（不叫 crc16_modbus），明确 README 写"目前只 MODBUS"
  - 提供 step API（streaming）+ bulk API 两种
  - 不依赖 VSF（`vsf_crc()` 算法不兼容 MODBUS reflected）

## Convention Deviations

无。函数命名 `crc16_<verb>` 符合 api-conventions.md §1.1。

## Responsible human

@user
