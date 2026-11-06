# air780eg -- non-blocking AT modem driver for Air780EG (4G/GNSS)

**Status: candidate**
**Layer: L1 (drivers)**

Non-blocking AT command driver for the Air780EG 4G LTE Cat.1 + GNSS module.
Feed UART bytes one at a time, poll for timeouts, and issue AT commands
with expected-response matching. URC dispatching for MQTT RX, PDP
deactivation, module restart, NMEA sentences, and signal quality.

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
air780eg_cmd_status_t air780eg_cmd_status(const air780eg_t *dev);
const char    *air780eg_get_matched_line(const air780eg_t *dev);
```

- `feed()` -- ISR-safe, call from UART RX interrupt or polling loop
- `poll()` -- call periodically to check command timeouts
- `send_cmd()` -- send AT command with expected response token
- Callback registration: `air780eg_on_mqtt_rx`, `air780eg_on_disconnect`,
  `air780eg_on_pdp_deact`, `air780eg_on_restart`, `air780eg_on_nmea`,
  `air780eg_on_csq`

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `AIR780EG_LINE_MAX` | 512 | Maximum AT response line length |
| `AIR780EG_TOPIC_MAX` | 128 | Maximum MQTT topic length |

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected UART HAL)

## Datasheet check

Checked on 2026-11-06 against the Luat Cat.1 module AT command manual V1.5.5, 2024/3/26, written for the 780E/600E/780EL series (EC618/EC716/718 platform). It is not an Air780EG manual: it names that part for one command only. This check was not run on hardware, and flybox has its own AT driver, so no board run backs this wheel. The driver was not changed: the owner decided it is checked but not fixed, because no board is available to retest it.

- **Matched:** CR LF framing, the final results OK, ERROR, +CME ERROR and +CMS ERROR, +CSQ values, the CLOSED URC, the direct-mode +MSUB sample.
- **Fixed:** The tests now read the manual's own direct-mode +MSUB sample.
- **Still open:** `pdp_deact_cb` never fires on the manual's `+PDP DEACT` or `+PDP:DEACT`, nor on `+CGEV: NW PDN DEACT` or `+CGEV: ME PDN DEACT`, because the driver looks for `PDP: DEACT` and `+CGEV: NW DEACT`, so a lost data connection goes unnoticed. The tests feed those same wrong strings, which is why they pass. A plain OK always ends a command, so a wait for `CONNACK OK`, SUBACK or `CONNECT OK` ends early, the caller moves on before the connection exists, and a later ERROR is lost. `CONNECT FAIL` and `SEND FAIL` do not count as failure, so the command only times out. A +MSUB with an unquoted topic, which is what AT+MQTTMSGGET prints, is dropped, so messages read from the cache never reach the callback. In cache mode the `+MSUB: <store_addr>` URC is dropped too, so the host sees no sign of a waiting message unless it sent AT+MQTTMSGSET=0. Topics over 127 bytes are dropped, though the module allows 256. Payloads are cut at 511 bytes with no error, though the module allows 4100, and a payload with a newline is split. A payload that starts with a double quote is altered. The reply to AT+CSQ=? is reported as rssi 0, and values 32 to 98, which the manual does not define, are accepted. There is no way to send a payload after the > prompt, and init does not turn echo off, so an expect token can match the echoed command unless the caller sends ATE0.
- **Suspected:** Lines end only on LF, so a wait for the > prompt may never match. Commands end in CR LF where the manual says CR, so the LF may become the first payload byte in data mode. The `+IPCLOSE:` URC is not in this manual.

## Known issues

- A line over 511 bytes is cut at 511 and still processed.

## How to test

```bash
cd wheels/drivers/air780eg/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_air780eg
```

## Test status

- Unit tests (sim): 54 pass, 0 fail
- libFuzzer: plays the modem with raw bytes, URC lines and command replies
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
