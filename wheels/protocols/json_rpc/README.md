# json_rpc -- JSON-RPC 2.0 frame builder and validator

**Status: candidate**
**Layer: L2 (protocols)**

Builds and validates JSON-RPC 2.0 request/response/error strings. Does NOT parse JSON
(use the json_parser wheel for that). Pure string formatting and simple pattern matching.
Suitable for embedded devices speaking JSON-RPC over serial, HTTP, or WebSocket.

## Origin

Proactive build for embedded device management interfaces. JSON-RPC 2.0 is widely used
for device configuration and monitoring APIs.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Existing JSON-RPC implementations couple parsing with full JSON parsers (cJSON, etc.).
- This wheel intentionally avoids full JSON parsing -- it only builds well-formed strings
  and does simple key-value extraction for routing.
- No suitable zero-dependency C99 builder-only implementation found.

Reference: JSON-RPC 2.0 specification (https://www.jsonrpc.org/specification).

## Dependencies

None. Pure C99, uses snprintf for formatting.

## API

```c
char buf[256];
size_t len;

/* Build a request */
json_rpc_build_request(buf, sizeof(buf), "add", "[1,2]", 1, &len);
/* buf = {"jsonrpc":"2.0","method":"add","params":[1,2],"id":1} */

/* Build a success response */
json_rpc_build_response(buf, sizeof(buf), "3", 1, &len);

/* Build an error response */
json_rpc_build_error(buf, sizeof(buf),
    JSON_RPC_ERR_METHOD_NOT_FOUND, "Method not found", 1, &len);

/* Validate and classify */
json_rpc_type_t type = json_rpc_validate(buf, len);
/* Returns REQUEST, RESPONSE, ERROR, or INVALID */

/* Extract method name for routing */
size_t mstart, mlen;
json_rpc_extract_method(buf, len, &mstart, &mlen);

/* Extract id for response matching */
int32_t id;
json_rpc_extract_id(buf, len, &id);
```

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] Real hardware target -- not yet verified

## Known issues

- Validation uses simple string matching, not a full JSON parser. Malformed JSON that
  happens to contain the right key patterns may be misclassified.
- Method names containing escaped quotes are not handled in extract_method.
- Integer id only (no string or null ids per JSON-RPC 2.0 spec extension).
- snprintf dependency means the C library must provide it (standard on all targets).

## How to test

```bash
cd wheels/protocols/json_rpc/test
cmake -B build -S . && cmake --build build
./build/test_json_rpc
./build/test_null_json_rpc
```

29 tests + 16 null tests: request building (with/without params, overflow, null), response building (scalar, object, null), error building (standard codes, null), validation (request/response/error/invalid/wrong version/null/empty), method extraction (found/not found/null), id extraction (positive/negative/zero/not found/null), round-trip request and error, standard error code constants.

## Test status

- Unit tests (sim): PASS (29 C tests + 16 null tests)
- HIL: N/A

## API convention deviations

None.

## Protocol

JSON-RPC 2.0 message formats:

| Type | Required fields |
|------|----------------|
| Request | `jsonrpc`, `method`, `id` (optional: `params`) |
| Response | `jsonrpc`, `result`, `id` |
| Error | `jsonrpc`, `error` (with `code` and `message`), `id` |

Standard error codes: -32700 (parse error), -32600 (invalid request), -32601 (method not found), -32602 (invalid params), -32603 (internal error).

## API review

- **Date**: 2026-05-11
- **Key decisions**: Builder-only approach (no JSON parser dependency); simple string search for validation and extraction (intentionally not full JSON parse); standard error code constants as macros; params_json passed as pre-formatted JSON string.

## Responsible human

@user
