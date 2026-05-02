# crc_any — configurable CRC calculator (1-32 bit)

**Status: candidate**
**Layer: L2 (protocols)**

Bit-by-bit CRC calculator supporting any width from 1 to 32 bits.
Configured via the Rocksoft model (width/poly/init/refin/refout/xorout),
so any algorithm from the CRC catalogue can be used without code changes.
No lookup tables — trades speed for flexibility and minimal ROM/RAM.

## Origin

Proactive build to generalize CRC computation across protocols. Complements
the existing `crc16` and `crc32` L0 wheels — those use table-based fast
paths for their specific algorithms; `crc_any` covers everything else.
## Dependencies

None.

## API

```c
/* Configuration (Rocksoft model) */
typedef struct {
    uint8_t  width;     /* CRC width in bits (1..32)                  */
    uint32_t poly;      /* Generator polynomial (high bit implicit)   */
    uint32_t init;      /* Initial CRC register value                 */
    bool     refin;     /* Reflect each input byte                    */
    bool     refout;    /* Reflect final CRC                          */
    uint32_t xorout;    /* Final XOR value                            */
} crc_any_config_t;

/* Pre-defined algorithms */
#define CRC_ANY_CRC16_MODBUS  ...  /* check=0x4B37 */
#define CRC_ANY_CRC16_CCITT   ...  /* check=0x29B1 */
#define CRC_ANY_CRC32_ISO     ...  /* check=0xCBF43926 */
#define CRC_ANY_CRC8_MAXIM    ...  /* check=0xA1 */

/* One-shot computation */
crc_any_err_t crc_any_compute(const crc_any_config_t *cfg,
                              const uint8_t *data, size_t len,
                              uint32_t *out);

/* Streaming / incremental (pass cfg->init as initial crc) */
uint32_t crc_any_update(const crc_any_config_t *cfg, uint32_t crc,
                        const uint8_t *data, size_t len);

/* Verify data against expected CRC */
bool crc_any_verify(const crc_any_config_t *cfg,
                    const uint8_t *data, size_t len,
                    uint32_t expected);
```

## Configuration (compile-time)

None required. Algorithm selection is entirely runtime via `crc_any_config_t`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified (pure software, expected to work)

## Known issues

None.

## How to test

```bash
cd wheels/protocols/crc_any/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_crc_any
```

## Test status

- Unit tests (sim): 15 tests, all passing
- HIL: N/A (pure software wheel)

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Rocksoft model config struct; bit-by-bit (no tables)
  for maximum flexibility at the cost of speed; streaming via update +
  finalize-through-compute pattern.

## Responsible human

@user
