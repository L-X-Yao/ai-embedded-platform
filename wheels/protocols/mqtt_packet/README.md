# mqtt_packet

**Status: candidate**
**Layer: L2 (protocols)**

MQTT 3.1.1 packet serializer/deserializer. Stateless building and parsing
of CONNECT, PUBLISH, SUBSCRIBE, PINGREQ, DISCONNECT and their responses.
No transport — caller handles TCP/TLS. Pure C99, no heap.
## Origin

Proactive build. MQTT is the standard IoT messaging protocol. Every connected
embedded device that talks to a cloud broker (AWS IoT, Azure IoT, Mosquitto)
needs MQTT packet encoding/decoding.


## Why self-written?

Searched GitHub (2026-05-02). Referenced eclipse-paho/paho.mqtt.embedded-c (EPL-2.0). Canonical MQTT library but EPL-2.0 license is restrictive for embedded redistribution. Self-written for platform convention consistency; implementation verified against MQTT 3.1.1 OASIS spec (remaining length encoding, packet type framing, QoS flags).

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`.

## API

```c
/* Build packets (returns length, or negative error) */
int mqtt_packet_build_connect(uint8_t *buf, size_t buf_size,
                              const mqtt_connect_opts_t *opts);
int mqtt_packet_build_publish(uint8_t *buf, size_t buf_size,
                              const mqtt_publish_t *pub);
int mqtt_packet_build_subscribe(uint8_t *buf, size_t buf_size,
                                uint16_t packet_id, const char *topic, uint8_t qos);
int mqtt_packet_build_pingreq(uint8_t *buf, size_t buf_size);
int mqtt_packet_build_disconnect(uint8_t *buf, size_t buf_size);

/* Parse packets */
mqtt_packet_type_t mqtt_packet_get_type(const uint8_t *buf, size_t len);
mqtt_packet_err_t  mqtt_packet_parse_connack(const uint8_t *buf, size_t len,
                                             mqtt_connack_t *out);
mqtt_packet_err_t  mqtt_packet_parse_publish(const uint8_t *buf, size_t len,
                                             mqtt_publish_t *out);
mqtt_packet_err_t  mqtt_packet_parse_puback(const uint8_t *buf, size_t len,
                                            uint16_t *packet_id);

/* Remaining length codec */
int      mqtt_packet_encode_remaining_length(uint8_t *buf, uint32_t length);
uint32_t mqtt_packet_decode_remaining_length(const uint8_t *buf, size_t len,
                                             int *bytes_used);
```

## Configuration (compile-time)

None. All parameters are runtime.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

- Single-topic SUBSCRIBE only (no multi-topic in one packet).
- No QoS 2 (PUBREC/PUBREL/PUBCOMP) support.
- No will message support in CONNECT.

## How to test

```bash
cd wheels/protocols/mqtt_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mqtt_packet
```

57 assertions across 16 test cases: remaining length encode/decode (multi-byte),
CONNECT build (basic + auth + buffer overflow), PUBLISH build (QoS 0/1, retain),
SUBSCRIBE build, PINGREQ/DISCONNECT build, packet type detection, CONNACK parse
(accepted + rejected), PUBACK parse, full PUBLISH roundtrip (build → parse →
verify topic + payload + QoS), null parameter safety.

## Test status

- Unit tests (sim): 25 pass, 0 fail
- libFuzzer: parses raw input, builds frames in exact-size buffers and reads them back
- HIL: N/A

## API convention deviations

Build functions return `int` (packet length or negative error) instead of
`mqtt_packet_err_t`, because callers need the length to know how many bytes
to send over TCP.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Stateless (no connection state machine — that's a separate concern); caller-provided buffers (no heap); PUBLISH parse returns pointers into the input buffer (zero-copy); remaining length codec exposed as public API for custom packet building; MQTT 3.1.1 only (not 5.0 — complexity not justified for most embedded use cases).

## Responsible human
@user
