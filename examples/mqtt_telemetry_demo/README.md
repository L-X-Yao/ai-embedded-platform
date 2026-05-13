# mqtt_telemetry_demo -- MQTT Client State Machine

Integration recipe driving the `mqtt_client` wheel through a full
**connect / publish / disconnect** lifecycle against an in-process mock
TCP transport. The mock answers CONNECT with CONNACK and PINGREQ with
PINGRESP so the state machine is exercised end-to-end without a real
broker.

This demo is intentionally distinct from `examples/remote_telemetry/`,
which uses the L0/L2 layer to build raw MQTT packets and ships them
over a simulated cellular modem. Here the *L3 client state machine* is
the focus: `mqtt_client` owns the RX buffer, parses CONNACK / PUBACK,
runs the keepalive timer, and refuses publishes when not in
`CONNECTED`.

## Wheels used

| Layer | Wheel | Role |
|-------|-------|------|
| L1 | `bmp280` | Mocked temperature + pressure sensor; reads from RAM-backed registers |
| L2 | `mqtt_packet` | Used by `mqtt_client` for serialization; also used directly here to peek packet types in the TCP send hook |
| L3 | `sensor_hub` | Aggregates the two bmp280 channels (temp / pressure) so publish logic doesn't care about the driver |
| L3 | `mqtt_client` | State machine under test |

## What it demonstrates

1. **CONNECT -> CONNACK** -- The mock TCP send hook detects a CONNECT
   packet and synchronously feeds a hand-built CONNACK back into
   `mqtt_client_feed`, transitioning the wheel into `CONNECTED` before
   `mqtt_client_connect()` returns.
2. **10 publish cycles** -- Each cycle advances virtual time 1000 ms,
   polls sensor_hub, builds a small JSON-ish payload, and calls
   `mqtt_client_publish` for the temperature and pressure topics
   (QoS 0, so no inflight tracking).
3. **Keepalive heartbeat** -- `mqtt_client_poll` is called every
   cycle. PINGREQ only fires when `now - last_tx_ms` crosses 3/4 of
   the keepalive window, so under continuous publishing it stays
   silent (which is the correct behaviour: the publish stream itself
   is enough to prove the link is alive). If the publish loop is
   paused for long enough, the mock broker is ready to reply with
   PINGRESP via the same `mock_tcp_pump_replies` path.
4. **State callbacks** -- `mqtt_client_on_event` fires on every state
   transition (CONNECTING / CONNECTED / DISCONNECTED). The demo
   asserts at least two transitions and that CONNECTED + DISCONNECTED
   were both observed.
5. **Sensor aggregation** -- `sensor_hub_poll` issues one driver read
   per channel; the demo reads back via `sensor_hub_get` so the
   publish logic is loosely coupled from the sensor driver.

## Build (host, WSL or Linux)

```bash
cd examples/mqtt_telemetry_demo
mkdir -p build && cd build
cmake -S .. -B .
cmake --build .
./mqtt_telemetry_demo
```

Exits status `0` on success. Prints `=== mqtt_telemetry_demo complete ===` at the end.

## Expected output (abridged)

```
=== mqtt_telemetry_demo -- MQTT client state machine ===
Wheels: bmp280 + sensor_hub + mqtt_packet + mqtt_client
==========================================================
[init] bmp280 ready (mocked I2C)
[init] sensor_hub channels=2
[init] mqtt_client initial state = DISCONNECTED

--- Connect handshake ---
    [tcp tx] CONNECT  (31 bytes)
    [mqtt evt] -> CONNECTING (reason=0x00)
    [mqtt evt] -> CONNECTED (reason=0x00)

--- Publishing 10 cycles at 1000 ms intervals ---
    [tcp tx] PUBLISH  (46 bytes)
    [tcp tx] PUBLISH  (48 bytes)
  cycle  0  t= 25.33C  p=   100687Pa
  ...
  cycle  9  t= 25.08C  p=   100656Pa

--- Clean disconnect ---
    [tcp tx] DISCONNECT (2 bytes)
    [mqtt evt] -> DISCONNECTED (reason=0x00)

--- Stats ---
  state transitions seen : 3
  connected event seen   : YES
  disconnected event seen: YES
  bytes sent over TCP    : 973
  PUBLISH packets        : 20 (expected 20)
  PINGREQ packets        : 0

=== mqtt_telemetry_demo complete ===
```

## Architecture notes

- The mock TCP `send` hook is what makes the demo possible. By
  inspecting outgoing packet types via `mqtt_packet_get_type` and
  re-injecting the appropriate broker reply with `mqtt_client_feed`,
  the test exercises real packet codec paths and the entire state
  machine inside `mqtt_client`. On a real device, `send` would push
  bytes through TCP/TLS and a separate RX path (interrupt or polling
  socket) would feed received bytes into `mqtt_client_feed`.
- `mqtt_client` is fully passive: no threads, no internal timers,
  no heap. The host application is responsible for calling
  `mqtt_client_poll` periodically -- on an MCU this is usually
  driven by a 1 Hz tick.
- `sensor_hub` adapters in this demo just forward `bmp280_read` into
  a `float`. The same pattern is used for every sensor in the
  platform, so the publishing code never depends on a specific
  driver's API.
- Real telemetry firmware would also build a payload via `json_parser`
  or `cbor_lite` and pipe it through `retry` for QoS-aware resends.
  Those are intentionally out of scope here -- this demo's job is to
  show that **mqtt_client alone is enough to talk to a broker**.
