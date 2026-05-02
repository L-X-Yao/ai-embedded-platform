# mqtt5_packet — MQTT 5.0 packet encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

Stateless packet building and parsing for MQTT 5.0. Extends mqtt_packet
(v3.1.1) with MQTT 5.0 features: variable byte integers for all lengths,
properties (session expiry, message expiry, receive max, max QoS), and
reason codes. Handles CONNECT, PUBLISH, SUBSCRIBE, DISCONNECT encoding
and CONNACK, PUBLISH decoding. No transport -- caller handles TCP/TLS.
Pure C99, no heap, all buffers caller-provided.
## Origin

Proactive build for IoT projects requiring MQTT 5.0 features (session
expiry, message expiry, enhanced reason codes).

## Dependencies

None.

## API

```c
typedef enum {
    MQTT5_OK, MQTT5_ERR_PARAMS, MQTT5_ERR_SIZE,
    MQTT5_ERR_PROTOCOL, MQTT5_ERR_OVERFLOW
} mqtt5_err_t;

/* Builders — return bytes written (>0) or negative mqtt5_err_t */
int mqtt5_build_connect(buf, size, &cfg);
int mqtt5_build_publish(buf, size, &pub);
int mqtt5_build_subscribe(buf, size, packet_id, topic, qos);
int mqtt5_build_disconnect(buf, size, reason_code);

/* Parsers */
mqtt5_err_t mqtt5_parse_connack(buf, len, &ack);
mqtt5_err_t mqtt5_parse_publish(buf, len, &pub);

/* Variable byte integer helpers */
int         mqtt5_encode_vbi(buf, value);
mqtt5_err_t mqtt5_decode_vbi(buf, len, &value, &bytes_used);
```

## Configuration (compile-time)

No compile-time defines. All behavior controlled via function parameters.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

- Property parsing is strict: unknown property IDs cause parse failure
  (no skip). This is intentional for safety on constrained devices.
- Only a subset of MQTT 5.0 properties is supported (session expiry,
  message expiry, receive max, max QoS). Add more as needed.

## How to test

```bash
cd wheels/protocols/mqtt5_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mqtt5_packet
```

## Test status

- Unit tests (sim): 35 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Follow mqtt_packet style. Use uint16_t for buffer
  sizes (matching embedded constraints). Properties encoded inline
  (no separate property struct/builder). Strict unknown-property rejection.

## Responsible human

@user
