# bcd -- binary-coded decimal encode/decode

**Status: candidate**
**Layer: L0 (primitives)**

Binary-coded decimal (BCD) encode and decode utilities for RTC chips
and legacy protocols. Each byte encodes two decimal digits (0-9) in its
upper and lower nibble. Common in DS3231, PCF8563, and other RTC register
formats. Provides both fast unchecked and validated (safe) variants,
plus batch conversion functions.

## Origin

Proactive build. BCD conversion is needed by every RTC driver wheel
(ds3231, pcf8563) and some legacy serial protocols.

## Why not VSF native?

Textbook utility with no VSF equivalent. Self-written for platform
convention consistency (unified error codes, test harness).

## Dependencies

None.

## API

```c
bool      bcd_is_valid(uint8_t bcd);
uint8_t   bcd_to_bin(uint8_t bcd);
uint8_t   bcd_from_bin(uint8_t bin);
bcd_err_t bcd_to_bin_safe(uint8_t bcd, uint8_t *out);
bcd_err_t bcd_from_bin_safe(uint8_t bin, uint8_t *out);
bcd_err_t bcd_buf_to_bin(const uint8_t *bcd, uint8_t *bin, uint16_t len);
bcd_err_t bcd_buf_from_bin(const uint8_t *bin, uint8_t *bcd, uint16_t len);
```

- Fast variants (`bcd_to_bin`, `bcd_from_bin`) -- no validation, undefined
  on invalid input
- Safe variants (`bcd_to_bin_safe`, `bcd_from_bin_safe`) -- return error
  code on invalid input
- Batch variants -- convert arrays in place
- All functions are pure, thread-safe, and ISR-safe

## Usage

```c
uint8_t bcd = bcd_from_bin(59);   /* 0x59 */
uint8_t bin = bcd_to_bin(0x23);   /* 23   */

uint8_t out;
if (bcd_to_bin_safe(0xAB, &out) != BCD_OK) {
    /* invalid BCD: nibble > 9 */
}
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/bcd/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bcd
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
