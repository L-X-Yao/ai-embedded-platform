# http_request -- HTTP/1.1 request builder (zero-alloc)

**Status: candidate**
**Layer: L2 (protocols)**

Buffer-based HTTP/1.1 request serializer for GET/POST/PUT/HEAD with custom
headers and body. Includes `build_range_get` convenience for OTA byte-range
downloads. Zero heap allocation, caller provides the output buffer.

## Origin

Proactive build for OTA firmware download with resume support (pairs with
`http_parser` for response parsing and `ota_manager` for orchestration).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-07):

- Embedded HTTP libraries (mongoose, esp-idf http_client) are full clients with
  socket management -- too heavy, OS-dependent.
- No standalone zero-dependency HTTP request *builder* found.
- ~100 lines of straightforward serialization; vendoring would add more friction.

Reference: RFC 7230 (HTTP/1.1 Message Syntax).

## Dependencies

None.

## API

```c
http_request_err_t http_request_build(uint8_t *buf, size_t cap,
                                      const http_request_cfg_t *cfg,
                                      size_t *out_len);

http_request_err_t http_request_build_range_get(uint8_t *buf, size_t cap,
                                                const char *host,
                                                const char *path,
                                                size_t offset, size_t end,
                                                size_t *out_len);
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim) -- 2026-05-07
- [ ] ARM Cortex-M -- not yet verified (pure C99, expected to work)

## Known issues

None.

## How to test

```bash
cd wheels/protocols/http_request/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_http_request
```

## Test status

- Unit tests (sim): 26 tests (request line + headers + body + Range + overflow + NULL guards + line breaks)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: Stateless builder (no context struct) since requests are one-shot. Config struct for build, convenience wrapper for Range GET.

## Responsible human

@L-X-Yao
