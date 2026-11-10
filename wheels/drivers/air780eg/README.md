# air780eg -- non-blocking AT modem driver for Air780EG (4G/GNSS)

**Status: candidate**
**Layer: L1 (drivers)**

Non-blocking AT command driver for the Air780EG 4G LTE Cat.1 + GNSS module.
Feed UART bytes one at a time, poll for timeouts, and issue AT commands
with expected-response matching. Commands with a data phase work through
the `>` prompt. URC dispatching for MQTT RX, PDP deactivation, module
restart, NMEA sentences, signal quality and connect or send failures.

## Origin

Reimplemented for the needs of an internal project, for reuse across cellular IoT projects.

## Why self-written?

Air780EG is a niche module with custom AT extensions (MQTT built-in,
GNSS control). No existing open-source component covers this modem. Self-written for
zero-dependency non-blocking architecture matching our wheel contract.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None (UART transport injected via `air780eg_io_t` callbacks).

## API

```c
air780eg_err_t air780eg_init(air780eg_t *dev, const air780eg_io_t *io);
void           air780eg_feed(air780eg_t *dev, uint8_t byte);
void           air780eg_poll(air780eg_t *dev);
air780eg_err_t air780eg_send_cmd(air780eg_t *dev, const char *cmd,
                                 const char *expect, uint32_t timeout_ms);
air780eg_err_t air780eg_send_data(air780eg_t *dev, const uint8_t *data,
                                  uint16_t len, const char *expect,
                                  uint32_t timeout_ms);
air780eg_cmd_status_t air780eg_cmd_status(const air780eg_t *dev);
const char    *air780eg_get_matched_line(const air780eg_t *dev);
uint32_t       air780eg_get_msg_drop_count(const air780eg_t *dev);
```

- `feed()` -- from the UART RX interrupt or the polling loop, not both. The URC
  callbacks run inside it, so from an ISR they must be short and must not call
  this driver. The top of the header says which side may call what.
- `poll()` -- call periodically: it checks command timeouts and gives up on a
  message whose bytes stopped
- `send_cmd()` -- send AT command with expected response token. A plain OK ends
  it only when `expect` is NULL or `"OK"`, so wait for `CONNACK OK` or `SUBACK`
  directly. Use `"CONNECT"` after AT+MIPSTART if `ALREADY CONNECT` is fine too.
  `">"` waits for the prompt of a data phase.
- `send_data()` -- the bytes after the prompt, with no line end
- Callback registration: `air780eg_on_mqtt_rx`, `air780eg_on_mqtt_cached`,
  `air780eg_on_disconnect`, `air780eg_on_pdp_deact`, `air780eg_on_restart`,
  `air780eg_on_nmea`, `air780eg_on_csq`, `air780eg_on_fail`

Publishing with a data phase, which needs no escaping of the payload:

```c
air780eg_send_cmd(&dev, "AT+MPUBEX=\"t\",0,0,5", ">", 2000);
/* ... air780eg_cmd_status() says AIR780EG_CMD_OK ... */
air780eg_send_data(&dev, data, 5, NULL, 9000);
/* ... AIR780EG_CMD_OK again once the module answers OK ... */
```

An incoming message is read by its `<len>` field, in direct mode and from
AT+MQTTMSGGET. Send AT+MQTTMSGGET with `expect` NULL: a message never ends a
command, the callback gets the messages and the final OK ends it.

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `AIR780EG_LINE_MAX` | 512 | Maximum AT response line length |
| `AIR780EG_TOPIC_MAX` | 257 | Maximum MQTT topic length with its NUL: the module allows 256 bytes |
| `AIR780EG_PAYLOAD_MAX` | 4100 | Bytes kept for one incoming message, the module's limit. Define it lower to save RAM: a longer message is skipped and counted, never cut |
| `AIR780EG_MSG_STALL_MS` | 2000 | Silence inside a message after which poll gives it up |
| `AIR780EG_BARRIER()` | GCC and Clang: compiler barrier | Define it for another compiler |

`air780eg_t` is 5872 bytes on Cortex-M, 2028 with `AIR780EG_PAYLOAD_MAX` at 256.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected UART HAL)

## Datasheet check

Checked on 2026-11-06 against the Luat Cat.1 module AT command manual V1.5.5, 2024/3/26, written for the 780E/600E/780EL series (EC618/EC716/718 platform). It is not an Air780EG manual: it names that part for one command only. The owner first held the driver back because no board is available to retest it, then approved fixing it from the manual. So neither the check nor the fixes ran on hardware, and flybox has its own AT driver, so no board run backs this wheel.

- **Matched:** CR LF framing, the final results OK, ERROR, +CME ERROR and +CMS ERROR, +CSQ values, the CLOSED URC, the direct-mode +MSUB sample.
- **Fixed:** `pdp_deact_cb` fires on `+PDP DEACT`, `+PDP:DEACT`, `+CGEV: NW PDN DEACT` and `+CGEV: ME PDN DEACT`, and the tests feed the manual's strings. A plain OK ends a command only when OK is what it waits for, and `CONNECT FAIL` and `SEND FAIL` count as failure. A +MSUB message is read by its `<len>`, up to 4100 bytes, with an unquoted topic or a quoted one of up to 256 bytes, unchanged and whole, newlines included, or it is counted as dropped. `+MSUB: <store_addr>` reaches a callback. The reply to AT+CSQ=? and values 32 to 98 are ignored. `air780eg_send_data` sends the payload after the > prompt, the prompt ends a wait with no line end, and the echo of ATE1 is dropped.
- **Still open:** A line that ends a command is not dispatched as a URC, so `expect "+CSQ:"` leaves `last_csq` as it was: use expect NULL if you want the reading there. A URC that arrives during a command overwrites `prev_line`. +CGNSINF, GSV and other NMEA sentences are not dispatched. There are no default timeouts from the section 1.4 table, no power-key or reset control and no SMS or HTTP URCs.
- **Suspected:** Nothing here ran on a module. Commands now end in CR, as the manual asks, where flybox's board-tested driver sends CR LF. The > prompt without a line end comes from a flybox comment, not from the manual. The manual does not say whether the module echoes a data phase, so turn echo off with ATE0 before `send_data` if the data could look like a reply. `+IPCLOSE:` was removed because neither the manual nor flybox has it.

## Known issues

- A line over 511 bytes is cut at 511 and still processed. +MSUB messages are not lines and are not cut.

## How to test

```bash
cd wheels/drivers/air780eg/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_air780eg
```

## Test status

- Unit tests (sim): 95 pass, 0 fail, plus 2 in a build with `AIR780EG_PAYLOAD_MAX` at 16
- libFuzzer: plays the modem with raw bytes, messages, URC lines, command replies and data phases
- HIL: N/A

## Migration

From the fixes of 2026-11. Nothing in this repo uses the wheel: flybox has its own driver.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| With an `expect` other than NULL or `"OK"`, a plain OK no longer ends the command | Pass NULL or `"OK"` if the OK is all you wait for | The command runs until its timeout |
| `expect "OK"` needs a line that is exactly OK, where any line containing OK matched before | Pass the full token, for example `"CONNECT OK"` | A wait that ended on such a line now ends on the final OK, or times out |
| A line ending in FAIL ends the command with `AIR780EG_CMD_ERROR`, whatever `expect` is, and with nothing pending goes to `air780eg_on_fail` | Handle the error instead of the timeout. Register the callback to see late failures | An error comes sooner than the timeout did. With no callback the line is dropped as before |
| `air780eg_t` grew from 1672 to 5872 bytes on Cortex-M | Check RAM or stack if it is a local, or define `AIR780EG_PAYLOAD_MAX` lower | The struct takes 4200 bytes more |
| `AIR780EG_TOPIC_MAX` went from 128 to 257 | Size topic buffers from it, and check copies with a fixed 128 | A callback that copies into a 128-byte buffer can now see longer topics |
| `<len>` is trusted: a quoted payload no longer loses its quotes, the `payload` pointer points at a message buffer valid only during the call, and a `+MSUB` with no number there is not delivered | Copy in the callback. Check `air780eg_get_msg_drop_count` for messages not delivered | Code that kept the pointer past the call reads the next message |
| Commands end in CR, not CR LF | Nothing, unless your port or a test looks at the bytes | The module gets what the manual asks for |
| `PDP: DEACT` without a plus, `+CGEV: NW DEACT` and `+IPCLOSE:` no longer fire a callback | Nothing for this module: the manual has none of them | Those strings are ignored |
| +CSQ with no number, or between 32 and 98, is ignored | Nothing | `last_csq` keeps the last valid reading |
| Spaces at the start of a line are dropped | Nothing, unless you compare a reply that has them | `prev_line` and `matched_line` lack them |
| `air780eg_poll` also recovers from a stalled message | Keep calling it, also with no command pending | After lost bytes the driver swallows up to `<len>` bytes of later traffic |

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
