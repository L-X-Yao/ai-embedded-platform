# crc8

**STATUS: candidate**
**Layer: L0 (primitives)**

CRC-8 calculator with multiple polynomial support. Bit-by-bit computation
(no lookup table, saves 256 bytes Flash on MCU). Supports reflected I/O for
Dallas/Maxim 1-Wire and ROHC. Pre-defined configs for CRC-8, CRC-8/MAXIM,
CRC-8/SENSIRION, CRC-8/SMBUS, and CRC-8/ROHC.

## Why not VSF native?

Checked VSF source (2026-05-01):

- `vsf_crc8_ccitt` in `vsf/source/component/crypto/hash/crc/` provides only
  the CCITT variant (poly 0x07, init 0x00, no reflect support).
- No support for reflected I/O (needed by 1-Wire/MAXIM, ROHC).
- No support for alternate polynomials (0x31 for Sensirion/MAXIM).
- No configurable init/xor-out values.

This wheel provides a generic CRC-8 engine with full parametrization.

## Origin

Proactive build to complete the CRC family (alongside crc16 and crc32).
CRC-8 is used by many I2C sensors (SI7021, SHT30) and protocols (1-Wire, SMBus PEC).

## Dependencies

None.

## API

```c
#include "crc8.h"

typedef struct {
    uint8_t poly;       /* Generator polynomial */
    uint8_t init;       /* Initial CRC value */
    bool    ref_in;     /* Reflect each input byte */
    bool    ref_out;    /* Reflect the final CRC */
    uint8_t xor_out;    /* Final XOR value */
} crc8_config_t;

/* Pre-defined configurations */
extern const crc8_config_t CRC8_DEFAULT;    /* poly=0x07, init=0x00 */
extern const crc8_config_t CRC8_MAXIM;      /* poly=0x31, init=0x00, refin/refout */
extern const crc8_config_t CRC8_SENSIRION;  /* poly=0x31, init=0xFF */
extern const crc8_config_t CRC8_SMBUS;      /* poly=0x07, init=0x00 (same as DEFAULT) */
extern const crc8_config_t CRC8_ROHC;       /* poly=0x07, init=0xFF, refin/refout */

/* One-shot computation */
uint8_t crc8_compute(const crc8_config_t *cfg, const uint8_t *data, size_t len);

/* Streaming (pass cfg->init as crc on first call) */
uint8_t crc8_update(const crc8_config_t *cfg, uint8_t crc,
                    const uint8_t *data, size_t len);

/* Verify against expected value */
bool crc8_verify(const crc8_config_t *cfg, const uint8_t *data,
                 size_t len, uint8_t expected);
```

## Features

- Bit-by-bit computation (no 256-byte table, minimal Flash)
- Configurable polynomial, init, reflect, XOR-out
- 5 pre-defined configs for common embedded CRC-8 variants
- Streaming API via update() for chunked data
- Pure C99, zero dependencies (no VSF / no RTOS / no malloc)
- Fully reentrant, no global state

## Configuration (compile-time)

No compile-time configuration needed. All parameters are runtime via `crc8_config_t`.

## Supported platforms

- [x] Linux host (sim) -- passed 2026-05-01
- [x] WSL Ubuntu 24.04 -- passed 2026-05-01

## Known issues

None. Pure stateless computation.

## How to test

```bash
cd wheels/primitives/crc8/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_crc8
python3 -m pytest test_crc8.py -v
```

Expected: **17 C test cases + 18 pytest cases all green.**

## Test status

- Unit tests: **PASS** (17 C cases + 18 pytest cases)
- Test vectors:
  - CRC8_DEFAULT "123456789" standard vector -> 0xF4
  - CRC8_MAXIM "123456789" -> 0xA1 (1-Wire)
  - CRC8_MAXIM 1-Wire ROM (7 bytes) -> 0x91
  - CRC8_SENSIRION "123456789" -> 0xF7
  - CRC8_SENSIRION SHT30 {0xBE, 0xEF} -> 0x92
  - CRC8_ROHC "123456789" -> 0xD0
  - CRC8_SMBUS == CRC8_DEFAULT
  - Verify pass/fail
  - Streaming update (split 3+4+2) == bulk
  - Streaming byte-by-byte == bulk
  - Empty data, single byte, NULL config
  - Custom config (CRC-8/WCDMA, poly=0x9B, reflected)
  - Reflected vs non-reflected difference
  - All-zeros vs all-ones difference
- HIL: N/A (pure compute)

## Performance reference

- Time complexity: O(n), 8 bit-shifts per byte (no table lookup)
- ROM footprint: ~200 bytes code + 5 x 5 bytes configs (~225 bytes total)
- Stack usage: < 16 bytes
- Trade-off: ~8x slower than table-based CRC-8, but saves 256 bytes Flash

## Usage example

**SHT30 I2C sensor CRC check:**

```c
uint8_t raw[3];  /* 2 data bytes + 1 CRC from sensor */
i2c_read(SHT30_ADDR, raw, 3);
if (!crc8_verify(&CRC8_SENSIRION, raw, 2, raw[2])) {
    /* CRC mismatch — corrupted I2C transfer */
}
```

**1-Wire ROM CRC verification:**

```c
uint8_t rom[8];  /* 1 family + 6 serial + 1 CRC */
onewire_read_rom(rom);
if (!crc8_verify(&CRC8_MAXIM, rom, 7, rom[7])) {
    /* Invalid ROM code */
}
```

**Streaming over DMA chunks:**

```c
uint8_t crc = CRC8_DEFAULT.init;
crc = crc8_update(&CRC8_DEFAULT, crc, chunk1, len1);
crc = crc8_update(&CRC8_DEFAULT, crc, chunk2, len2);
/* For non-reflected configs, crc is the final value. */
```

## API review

- Date: 2026-05-01
- Key decisions:
  - Bit-by-bit (no table) to minimize Flash on small MCUs
  - Config struct instead of separate functions per variant
  - Pre-defined configs for 5 most common embedded CRC-8 variants
  - update() exposes raw accumulator for non-reflected streaming

## API convention deviations

None. Function naming `crc8_<verb>` follows api-conventions.md.

## Responsible human

@user
