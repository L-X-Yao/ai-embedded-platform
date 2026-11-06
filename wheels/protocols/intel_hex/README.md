# intel_hex

**Status: candidate**
**Layer: L2 (protocols)**

Intel HEX firmware format parser and encoder. Handles all record types:
Data (00), EOF (01), Extended Segment (02), Start Segment (03), Extended
Linear Address (04), and Start Linear Address (05). Checksum and record
shape validation included. Complement to the s_record wheel. Useful for bootloaders and
firmware update tools. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). i


## Why self-written?

existing open-source has no Intel HEX parser. This wheel is needed by bootloaders that
accept .hex files over UART, USB, or OTA.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "intel_hex.h"

intel_hex_err_t intel_hex_decode(const char *line, size_t len,
                                 intel_hex_record_t *out);
intel_hex_err_t intel_hex_encode(const intel_hex_record_t *rec,
                                 char *buf, size_t cap, size_t *out_len);
```

## Usage

```c
/* Parse one line from a .hex file */
const char *line = ":10010000214601360121470136007EFE09D21901E0";
intel_hex_record_t rec;
if (intel_hex_decode(line, strlen(line), &rec) == INTEL_HEX_OK) {
    if (rec.type == INTEL_HEX_DATA) {
        flash_write(base_addr + rec.address, rec.data, rec.data_len);
    }
}
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\intel_hex/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_intel_hex
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

@user
