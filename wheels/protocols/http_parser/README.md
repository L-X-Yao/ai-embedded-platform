# http_parser

**Status: candidate**
**Layer: L2 (protocols)**

Lightweight HTTP/1.1 request and response parser. Zero-copy: parsed fields
point into the caller's input buffer. Supports Content-Length-based body
extraction and case-insensitive header lookup.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Referenced h2o/picohttpparser (MIT). Industry standard for high-performance HTTP parsing. Self-written for platform convention consistency; implementation verified against HTTP/1.1 spec (RFC 7230/7231).

## Dependencies

None.

## API

```c
#include "http_parser.h"

http_parser_err_t http_parser_parse_request(const char *data, size_t len, http_parser_request_t *req);
http_parser_err_t http_parser_parse_response(const char *data, size_t len, http_parser_response_t *resp);
http_parser_err_t http_parser_parse_response_to_head(const char *data, size_t len, http_parser_response_t *resp);
http_parser_err_t http_parser_find_header(const http_parser_header_t *headers, int count,
                            const char *name, const http_parser_header_t **out);
int        http_parser_get_content_length(const http_parser_header_t *headers, int count);
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `HTTP_PARSER_MAX_HEADERS` | 16 | Maximum number of parsed headers per request/response |

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

- Bodies are framed by Content-Length only. No transfer coding is decoded:
  a message with Transfer-Encoding returns `HTTP_PARSER_ERR_TRANSFER_ENCODING`.
- A reply with neither header, whose body ends when the connection closes
  (RFC 7230 section 3.3.3 item 7), reads as having no body.

## Migration

- A 1xx, 204 or 304 reply now ends at the blank line whatever its headers say.
  Nothing to do: such a reply used to return `HTTP_PARSER_ERR_INCOMPLETE` for
  ever, or `HTTP_PARSER_ERR_MALFORMED` on a bad Content-Length.
- `http_parser_parse_response_to_head()` is new. Call it for the reply to a HEAD
  request. If you keep calling `http_parser_parse_response()`, a HEAD reply with
  a Content-Length still waits for a body that never comes.
- `HTTP_PARSER_ERR_TRANSFER_ENCODING` is new, for requests and replies. The head
  is filled in and the coded body starts at `total_consumed`, so decode it
  yourself or give up. A caller that only tests for `HTTP_PARSER_OK` now sees a
  failure where it used to read an empty message.

## How to test

```bash
cd wheels/protocols/http_parser/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **27 C cases** — GET request, POST with body, 200/404 response,
  incomplete request/body, header lookup, missing header, NULL safety, content-length

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@L-X-Yao
