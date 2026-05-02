# at_command — AT command engine

**Status: candidate**

Byte-feed AT command parser with response matching, URC dispatch, and data-mode prompt detection.
Zero OS dependency — consumer feeds bytes from any UART source, engine emits events.
## Origin

Extracted from `projects/flybox/src/drivers/air780eg/air780eg_driver.c` — line buffering,
substring response matching, URC dispatch, `>` prompt detection, and prev-line capture.
Stripped all FreeRTOS/RTOS synchronization (consumer handles that), all Air780EG-specific
commands, and all hardware UART/DMA code.

## API

```c
/* HAL: caller sends bytes to modem */
at_command_hal_t hal = { .send = my_uart_write, .ctx = &uart };

at_command_t at;
at_command_init(&at, &hal);

/* Register URC handlers */
at_command_register_urc(&at, "RDY", on_modem_restart, NULL);
at_command_register_urc(&at, "+CGREG:", on_network_status, NULL);
at_command_register_urc(&at, "$GP", on_nmea_sentence, NULL);

/* Send command and set expected response */
at_command_send(&at, "AT+CSQ");
at_command_expect(&at, "+CSQ:");

/* In RX loop: feed bytes one at a time */
at_command_evt_t evt;
at_command_evt_type_t r = at_command_feed(&at, byte, &evt);
switch (r) {
case AT_COMMAND_EVT_OK:     /* response matched */
    parse_csq(at_command_get_matched(&at));
    break;
case AT_COMMAND_EVT_ERROR:  /* ERROR/+CME ERROR */
    handle_error();
    break;
case AT_COMMAND_EVT_URC:    /* unsolicited, dispatched to handler */
    break;
case AT_COMMAND_EVT_PROMPT: /* ">" for data mode (AT+CMGS, AT+MPUBEX) */
    at_command_send_raw(&at, payload, len);
    break;
}

/* For commands where data appears on prev line (e.g., AT+CGSN → IMEI) */
at_command_send(&at, "AT+CGSN");
at_command_expect(&at, "OK");
/* ... after AT_COMMAND_EVT_OK ... */
const char *imei = at_command_get_prev_line(&at);
```

## Features

- **Response matching**: substring search (matches `+CSQ:` in `+CSQ: 18,0`)
- **ERROR detection**: any line containing `ERROR` (covers `ERROR`, `+CME ERROR:`, `+CMS ERROR:`)
- **URC dispatch**: prefix-based callback table (up to `AT_COMMAND_MAX_URCS`, default 16)
- **Prompt detection**: `>` character for data-mode commands
- **Prev-line capture**: for commands where response data precedes `OK`
- **CR stripping**: handles `\r\n` and bare `\n` line endings

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `AT_COMMAND_LINE_SIZE` | 256 | Max line buffer |
| `AT_COMMAND_MATCH_SIZE` | 256 | Matched/prev-line buffer |
| `AT_COMMAND_MAX_URCS` | 16 | Max URC handler slots |

## Dependencies

- None (interface dependency: `at_command_hal_t` send callback provided by caller)

## Supported platforms

- [x] Host (sim) — passed 2026-04-27
- [ ] GD32F405 (flybox) — original project code verified, wheel not separately HIL tested

## Known issues

- No built-in timeout — consumer must implement timeout externally (check elapsed time, call `at_command_cancel()`).
- No command queue — single pending expect at a time. Consumer serializes with mutex if multi-threaded.
- No binary/fixed-length data mode (HTTP chunked, MQTT payload). Consumer handles mode switching externally.
- URC matching is prefix-only (`starts_with`), not substring. Register `"+CME"` not `"CME"`.

## How to test

```bash
cd wheels/protocols/at_command/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_at_command
```

90 assertions across 35 test cases: init, send framing (CRLF append), OK/ERROR/CME/CMS response, bare-LF line ending, prev-line capture, URC dispatch (single/multiple/during-wait/before-expect), prompt detection, cancel, raw send (normal/null/zero-length), CR stripping, empty lines, null safety, table full, line buffer overflow protection, re-init clears URCs, exact buffer boundary, consecutive command cycles, null callback/prefix rejection, null cmd/raw safety, URC not matched as response, ERROR clears matched state.

## Test status

- Unit tests (sim): PASS (35 C tests, 90 assertions)
- HIL: N/A

## API convention deviations

- **`at_command_urc_cb_t` parameter order** (api-conventions §5.1): Signature is `(const char *line, void *ctx)` instead of `(void *ctx, event, data)`. URC callbacks receive a raw line string, not a structured event — the three-parameter pattern adds no value here. Changing parameter order would break consumers for no functional benefit.

## API review

- **Date**: 2026-04-27
- **Key decisions**: Byte-feed API (no UART dependency); events instead of semaphores (portable); URC table with prefix matching (not regex); single `expected` register (matches flybox pattern); `prev_line` for multi-line responses; `send_raw` for data-mode payloads.

## Responsible human
@user
