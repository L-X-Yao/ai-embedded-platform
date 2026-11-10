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


## Why self-written?

Searched GitHub (2026-05-02). Referenced 1byt3/m5 (unclear license). Niche MQTT 5.0 library with unclear licensing. Self-written for platform convention consistency; implementation verified against MQTT 5.0 OASIS spec (variable byte integers, property encoding, reason codes).

## Dependencies

None.

## API

```c
/* Error codes: MQTT5_OK=0, MQTT5_ERR_PARAM=-1, MQTT5_ERR_SIZE=-2,
 *              MQTT5_ERR_PROTOCOL=-3, MQTT5_ERR_OVERFLOW=-4 */

/* Builders — return bytes written (>0) or negative mqtt5_err_t */
int mqtt5_build_connect(uint8_t *buf, uint16_t size,
                        const mqtt5_connect_t *cfg);
int mqtt5_build_publish(uint8_t *buf, uint16_t size,
                        const mqtt5_publish_t *pub);
int mqtt5_build_subscribe(uint8_t *buf, uint16_t size,
                          uint16_t packet_id, const char *topic, uint8_t qos);
int mqtt5_build_disconnect(uint8_t *buf, uint16_t size, uint8_t reason_code);

/* Parsers */
mqtt5_err_t mqtt5_parse_connack(const uint8_t *buf, uint16_t len,
                                mqtt5_connack_t *ack);
mqtt5_err_t mqtt5_parse_publish(const uint8_t *buf, uint16_t len,
                                mqtt5_publish_t *pub);

/* Variable byte integer helpers */
int         mqtt5_encode_vbi(uint8_t *buf, uint32_t value);
mqtt5_err_t mqtt5_decode_vbi(const uint8_t *buf, uint16_t len,
                             uint32_t *value, uint8_t *bytes_used);
```

## Configuration (compile-time)

No compile-time defines. All behavior controlled via function parameters.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

- Only a subset of MQTT 5.0 properties is stored (session expiry,
  message expiry, receive max, max QoS). Add more as needed.
- The parsers step over any other property the spec allows in that
  packet, by the type it gives the ID, and drop the value. An ID it does
  not allow there is refused: section 2.2.2.2 calls that a Malformed Packet.

## Migration

- `mqtt5_publish_t` has a new `topic_len`, at the end, set by
  `mqtt5_parse_publish()`. The parsed `topic` has no terminator, so read
  `topic_len` bytes of it. 0 is valid there, when a Topic Alias stands in.
- `mqtt5_build_publish()` ignores the field and still reads `topic` as a C string,
  so copy a parsed topic and terminate it before building from it.
- A caller that used `strlen()` on a parsed topic gets the wrong length, or reads
  past the buffer, and must change. Nothing else needs to. The struct grows on
  32-bit targets, so rebuild whatever holds one.

## How to test

```bash
cd wheels/protocols/mqtt5_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mqtt5_packet
```

## Test status

- Unit tests (sim): 51 pass, 0 fail
- libFuzzer: parses raw input, builds frames in exact-size buffers and reads them back
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Follow mqtt_packet style. Use uint16_t for buffer
  sizes (matching embedded constraints). Properties encoded inline
  (no separate property struct/builder). Properties with no field are
  stepped over: rejecting them refused the CONNACK of a default broker.

## Responsible human

@user
