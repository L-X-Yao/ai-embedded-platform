# http_parser

**STATUS: candidate**
**Layer: L2 (protocols)**

Lightweight HTTP/1.1 request and response parser. Zero-copy: parsed fields
point into the caller's input buffer. Supports Content-Length-based body
extraction and case-insensitive header lookup.

## Why not VSF native?

Checked VSF source (2026-04-29): no HTTP parser in service/utilities/component.

## API

```c
#include "http_parser.h"

http_err_t http_parse_request(const char *data, size_t len, http_request_t *req);
http_err_t http_parse_response(const char *data, size_t len, http_response_t *resp);
http_err_t http_find_header(const http_header_t *headers, int count,
                            const char *name, const http_header_t **out);
int        http_get_content_length(const http_header_t *headers, int count);
```

## How to test

```bash
cd wheels/protocols/http_parser/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **10 C cases** — GET request, POST with body, 200/404 response,
  incomplete request/body, header lookup, missing header, NULL safety, content-length

## Dependencies

None.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Configuration macros

| Macro | Default | Description |
|-------|---------|-------------|
| `HTTP_MAX_HEADERS` | 16 | Maximum number of parsed headers per request/response |

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## API convention deviations

无。

## Responsible human

@user
