# mqtt_client -- stateful MQTT 3.1.1 client (non-blocking, no OS)

**Status: candidate**
**Layer: L3 (modules)**

Non-blocking MQTT 3.1.1 client state machine. Caller feeds raw TCP bytes
via `feed()`, calls `poll()` periodically for keepalive and timeout
management, and uses `publish()`/`subscribe()` when connected. Transport
injected via IO function pointers. Supports QoS 0 and QoS 1 with one
inflight message.

## Origin

Proactive build for IoT telemetry projects (flybox, sensor gateways)
that need MQTT connectivity over AT modem or raw TCP.

## Why self-written?

existing open-source does not provide a standalone MQTT client that works without the
VSF network stack. Self-written for transport-agnostic operation (works
over AT modem, raw TCP, or any byte-stream transport).

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

`mqtt_packet` (L2) -- for MQTT packet encoding/decoding. Transport
injected via `mqtt_client_io_t` callbacks.

## API

```c
mqtt_client_err_t mqtt_client_init(mqtt_client_t *c,
                                   const mqtt_client_io_t *io,
                                   const mqtt_client_cfg_t *cfg);
mqtt_client_err_t mqtt_client_connect(mqtt_client_t *c);
mqtt_client_err_t mqtt_client_disconnect(mqtt_client_t *c);
mqtt_client_err_t mqtt_client_publish(mqtt_client_t *c, const char *topic,
                                      const uint8_t *payload, uint16_t len,
                                      uint8_t qos);
mqtt_client_err_t mqtt_client_subscribe(mqtt_client_t *c,
                                        const char *topic, uint8_t qos);
mqtt_client_err_t mqtt_client_feed(mqtt_client_t *c,
                                   const uint8_t *data, uint16_t len);
mqtt_client_err_t mqtt_client_poll(mqtt_client_t *c);
mqtt_client_state_t mqtt_client_get_state(const mqtt_client_t *c);
void mqtt_client_on_message(mqtt_client_t *c, mqtt_client_msg_cb_t cb, void *ctx);
void mqtt_client_on_event(mqtt_client_t *c, mqtt_client_evt_cb_t cb, void *ctx);
```

## Usage

```c
mqtt_client_t client;
mqtt_client_cfg_t cfg = { .client_id = "device01",
                          .keepalive_s = 60,
                          .clean_session = true,
                          .connect_timeout_ms = 5000,
                          .ack_timeout_ms = 3000 };
mqtt_client_init(&client, &tcp_io, &cfg);
mqtt_client_on_message(&client, on_msg, NULL);
mqtt_client_connect(&client);

/* main loop */
mqtt_client_feed(&client, rx_data, rx_len);
if (mqtt_client_poll(&client) == MQTT_CLIENT_ERR_TIMEOUT) {
    /* no PUBACK for the last QoS 1 publish: publish it again if it matters */
}

if (mqtt_client_get_state(&client) == MQTT_CLIENT_CONNECTED) {
    mqtt_client_publish(&client, "sensor/temp", payload, len, 0);
}
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `MQTT_CLIENT_RX_BUF_SIZE` | 1024 | Receive buffer size (bytes) |
| `MQTT_CLIENT_TX_BUF_SIZE` | 512 | Transmit buffer size (bytes) |

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected transport)

## Known issues

- QoS 2 not supported.
- Only one inflight QoS 1 publish and one inflight subscribe at a time.
- A refused subscription is not reported: a SUBACK failure code clears the inflight id like a grant.
- A SUBACK timeout is not reported either: poll clears the inflight id and returns OK.
- A QoS 1 message is not resent: the client keeps no copy. Poll reports the timeout and the caller decides.

## Migration

- `mqtt_client_poll()` now returns the new `MQTT_CLIENT_ERR_TIMEOUT` from the one
  call that gives up on a QoS 1 publish. The connection stays up, no event fires,
  and the slot is free again, as before.
- To tell a lost message from a delivered one, test that return, and publish
  again if the message matters. It may then arrive twice, as QoS 1 allows.
- Do nothing and all is as before: a timed-out publish looks delivered. A caller
  that treats any poll error as fatal should let this one through.
- If the link drops with a publish pending, `mqtt_client_pub_inflight()` stays true
  until the next connect, so read it in the DISCONNECTED event.

## How to test

```bash
cd wheels/modules/mqtt_client/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mqtt_client
```

## Test status

- Unit tests (sim): 59 pass, 0 fail
- libFuzzer: runs sessions in input order, compares a stream fed whole and byte by byte, and checks that an oversized packet is dropped whole
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@L-X-Yao
