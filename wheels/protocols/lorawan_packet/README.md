# lorawan_packet — LoRaWAN MAC layer packet encoder/decoder

**Status: candidate**
**Layer: L2 (protocols)**

Stateless frame building and parsing for LoRaWAN Class A uplink/downlink
data frames and join requests. Codec only — no AES encryption, no MIC
computation (caller handles crypto separately).
## Origin

Proactive build for LoRa/LoRaWAN gateway and node projects.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    LORAWAN_OK, LORAWAN_ERR_PARAMS, LORAWAN_ERR_SIZE,
    LORAWAN_ERR_MIC, LORAWAN_ERR_OVERFLOW,
} lorawan_err_t;

/* Frame types */
typedef enum {
    LORAWAN_JOIN_REQUEST, LORAWAN_JOIN_ACCEPT,
    LORAWAN_UNCONFIRMED_UP, LORAWAN_UNCONFIRMED_DOWN,
    LORAWAN_CONFIRMED_UP, LORAWAN_CONFIRMED_DOWN,
} lorawan_mtype_t;

/* Parsed/buildable frame */
typedef struct {
    lorawan_mtype_t mtype;
    uint32_t dev_addr;
    uint32_t fcnt;
    uint8_t  fport;
    bool     ack, adr;
    const uint8_t *payload;
    uint8_t  payload_len;
    uint8_t  mic[4];
} lorawan_frame_t;

/* Data frame codec */
int lorawan_build_data(uint8_t *buf, uint16_t buf_size,
                       const lorawan_frame_t *frame);
lorawan_err_t lorawan_parse_data(const uint8_t *buf, uint16_t len,
                                 lorawan_frame_t *frame);

/* Join request codec */
int lorawan_build_join_request(uint8_t *buf, uint16_t buf_size,
                               const uint8_t *app_eui,
                               const uint8_t *dev_eui,
                               const uint8_t *dev_nonce);
lorawan_err_t lorawan_parse_join_request(const uint8_t *buf, uint16_t len,
                                         uint8_t *app_eui, uint8_t *dev_eui,
                                         uint8_t *dev_nonce);

/* Helpers */
uint8_t lorawan_mhdr(lorawan_mtype_t mtype, uint8_t major);
lorawan_mtype_t lorawan_get_mtype(uint8_t mhdr);
```

## Configuration (compile-time)

None. Pure codec with no configurable parameters.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/protocols/lorawan_packet/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lorawan_packet
```

## Test status

- Unit tests (sim): 18 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Crypto-agnostic design (no AES/MIC computation); MIC field carried in frame struct but not verified by parser; little-endian DevAddr/FCnt per LoRaWAN spec; FPort omitted when payload is empty.

## Responsible human

@user
