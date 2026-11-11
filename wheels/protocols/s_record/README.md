# s_record

**Status: candidate**
**Layer: L2 (protocols)**

Motorola S-Record (SREC) firmware format parser and encoder. Handles all
record types: S0 (header), S1/S2/S3 (data with 16/24/32-bit address),
S5 (record count), and S7/S8/S9 (end-of-file). Checksum validation included.
Complement to the intel_hex wheel. Useful for bootloaders and firmware update
protocols. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). Motorola S-Record (SREC) firmware format parser and encoder. Handles all


## Why self-written?

existing open-source has no S-Record parser. This wheel is needed by bootloaders that accept
.srec/.s19 files over UART, USB, or OTA update channels.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "s_record.h"

s_record_err_t s_record_decode(const char *line, size_t len, s_record_line_t *out);
s_record_err_t s_record_encode(const s_record_line_t *rec,
                                char *buf, size_t cap, size_t *out_len);
```

## Usage

```c
/* Parse one line from a .srec file */
const char *line = "S1130100214601360121470136007EFE09D21901A8";
s_record_line_t rec;
if (s_record_decode(line, strlen(line), &rec) == S_RECORD_OK) {
    if (rec.type == S_RECORD_S1) {
        flash_write(rec.address, rec.data, rec.data_len);
    }
}
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\s_record/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_s_record
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
