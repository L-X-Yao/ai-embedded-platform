# air780eg -- non-blocking AT modem driver for Air780EG (4G/GNSS)

**Status: candidate**
**Layer: L1 (drivers)**

Non-blocking AT command driver for the Air780EG 4G LTE Cat.1 + GNSS module.
Feed UART bytes one at a time, poll for timeouts, and issue AT commands
with expected-response matching. URC dispatching for MQTT RX, PDP
deactivation, module restart, NMEA sentences, and signal quality.

## Origin

Extracted from `projects/flybox/` for reuse across cellular IoT projects.

## Why not VSF native?

Air780EG is a niche module with custom AT extensions (MQTT built-in,
GNSS control). No VSF component covers this modem. Self-written for
zero-dependency non-blocking architecture matching our wheel contract.

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

## Known issues

None.

## How to test

```bash
cd wheels/drivers/air780eg/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_air780eg
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
