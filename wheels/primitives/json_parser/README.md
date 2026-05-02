# json_parser — minimal SAX-style JSON parser for embedded systems

**Status: candidate**
**Layer: L0 (primitives)**

Zero-allocation streaming JSON parser with callback-per-token (SAX-style) design.
Supports nested objects/arrays, escape sequences, chunked input via `feed()`.
No malloc, no OS dependencies — pure C99.

## Origin

Proactive build for embedded telemetry/config parsing (MQTT payloads, AT command responses, REST API bodies).

## Why self-written?

Searched GitHub (2026-05-02). Referenced zserge/jsmn (3.8k stars, MIT) for algorithm verification. jsmn is the gold standard embedded JSON parser; our SAX callback API is a deliberate design choice for streaming over jsmn's token-array approach. Self-written for platform convention consistency; algorithm cross-validated against reference.

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    JSON_PARSER_OK             =  0,
    JSON_PARSER_ERR_PARAM      = -1,
    JSON_PARSER_ERR_SYNTAX     = -2,
    JSON_PARSER_ERR_DEPTH      = -3,
    JSON_PARSER_ERR_INCOMPLETE = -4,
} json_parser_err_t;

/* Event types delivered to callback */
typedef enum {
    JSON_EVENT_OBJECT_START,  JSON_EVENT_OBJECT_END,
    JSON_EVENT_ARRAY_START,   JSON_EVENT_ARRAY_END,
    JSON_EVENT_KEY,           JSON_EVENT_STRING,
    JSON_EVENT_NUMBER,        JSON_EVENT_TRUE,
    JSON_EVENT_FALSE,         JSON_EVENT_NULL,
} json_event_t;

/* Callback: value points into input buffer, valid only during call */
typedef void (*json_parser_cb_t)(void *ctx, json_event_t event,
                                  const char *value, uint16_t value_len);

/* Initialize parser with callback and max nesting depth */
json_parser_err_t json_parser_init(json_parser_t *p, json_parser_cb_t cb,
                                    void *ctx, uint8_t max_depth);

/* Feed a chunk of JSON data (streaming) */
json_parser_err_t json_parser_feed(json_parser_t *p, const char *data,
                                    uint16_t len);

/* Finalize — returns ERR_INCOMPLETE if JSON is unfinished */
json_parser_err_t json_parser_finish(json_parser_t *p);

/* Reset to initial state (keeps callback/ctx/max_depth) */
void json_parser_reset(json_parser_t *p);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `JSON_PARSER_VALUE_BUF_SIZE` | 64 | Max bytes for a single string/number/literal token |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

- `\uXXXX` escape sequences store hex digits literally (no UTF-8 conversion). Adequate for ASCII-range escapes; callers needing full Unicode must decode themselves.
- Value buffer overflow (tokens > 64 bytes) silently truncates. Increase `JSON_PARSER_VALUE_BUF_SIZE` if longer tokens are expected.

## How to test

```bash
cd wheels/primitives/json_parser/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_json_parser
```

## Test status

- Unit tests (sim): 24 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: SAX-style over DOM to avoid malloc; streaming feed() for chunked UART/network data; value pointer into input buffer for zero-copy during callback

## Responsible human

@user
