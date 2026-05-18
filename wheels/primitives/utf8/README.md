# utf8

**Status: candidate**
**Layer: L0 (primitives)**

UTF-8 validation, decoding, encoding, and codepoint counting for embedded
systems. Stateless utilities that work on raw byte buffers. Handles 1-4 byte
sequences (full Unicode range). Pure C99, no heap, no OS.

## Why not VSF native?

VSF has no standalone UTF-8 utility. This wheel provides the minimal
primitives needed by display drivers, CLI shells, and protocol parsers
that handle multi-byte text.

## API

```c
#include "utf8.h"

uint8_t    utf8_byte_length(uint8_t lead);
utf8_err_t utf8_validate(const uint8_t *data, size_t len);
int32_t    utf8_codepoint_count(const uint8_t *data, size_t len);
utf8_err_t utf8_decode(const uint8_t *data, size_t len,
                       uint32_t *codepoint, size_t *out_len);
utf8_err_t utf8_encode(uint32_t codepoint, uint8_t *buf, size_t cap,
                       size_t *out_len);
```

## Usage

```c
const uint8_t text[] = "Hello \xe4\xb8\x96\xe7\x95\x8c";  /* "Hello world" in Chinese */
if (utf8_validate(text, sizeof(text) - 1) == UTF8_OK) {
    int32_t count = utf8_codepoint_count(text, sizeof(text) - 1);
    /* count == 8 (6 ASCII + 2 CJK characters) */
}

uint32_t cp;
size_t consumed;
utf8_decode(text + 6, 3, &cp, &consumed);  /* cp = 0x4E16 */
```

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
