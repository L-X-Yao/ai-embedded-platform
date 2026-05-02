# canbus — CAN 2.0A/B frame encoder/decoder with signal extraction

**Status: candidate**
**Layer: L2 (protocols)**

Stateless CAN frame packing, unpacking, ID filtering, and bit-level signal
extract/pack for DBC-style decoding. Handles both standard (11-bit) and
extended (29-bit) CAN identifiers. Pure codec — no hardware interaction.

## Origin

Proactive build for automotive/industrial CAN bus protocol handling.
Common pattern across SPI CAN controllers (MCP2515, SJA1000) and
on-chip CAN peripherals.
## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    CANBUS_OK, CANBUS_ERR_PARAMS, CANBUS_ERR_SIZE,
    CANBUS_ERR_ID, CANBUS_ERR_DLC, CANBUS_ERR_OVERFLOW,
} canbus_err_t;

/* CAN frame */
typedef struct {
    uint32_t id;        // 11-bit (standard) or 29-bit (extended)
    uint8_t  data[8];
    uint8_t  dlc;       // 0-8
    bool     extended;  // true = 29-bit ID (CAN 2.0B)
    bool     rtr;       // remote transmission request
} canbus_frame_t;

/* Validate, pack, unpack */
canbus_err_t canbus_frame_validate(const canbus_frame_t *frame);
int          canbus_frame_pack(const canbus_frame_t *frame, uint8_t *buf, uint16_t buf_size);
canbus_err_t canbus_frame_unpack(const uint8_t *buf, uint16_t len, canbus_frame_t *frame);

/* ID filtering */
typedef struct {
    uint32_t id, mask;  // mask: 1=must match, 0=don't care
    bool     extended;
} canbus_filter_t;
bool canbus_filter_match(const canbus_filter_t *filter, const canbus_frame_t *frame);

/* DBC-style signal extract/pack (bit-level, LE/BE) */
uint64_t canbus_signal_extract(const uint8_t *data, uint8_t start_bit,
                               uint8_t length, bool big_endian);
void     canbus_signal_pack(uint8_t *data, uint8_t start_bit,
                            uint8_t length, bool big_endian, uint64_t value);

/* Helpers */
bool canbus_is_standard_id(uint32_t id);
bool canbus_is_extended_id(uint32_t id);
```

## Configuration (compile-time)

No compile-time configuration required. All functions are pure and stateless.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified (designed for any platform with C99 compiler)

## Known issues

None.

## How to test

```bash
cd wheels/protocols/canbus/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_canbus
```

## Test status

- Unit tests (sim): 32 tests, all passing
- HIL: N/A (pure codec, no hardware dependency)

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: FLAGS_DLC byte placed first in wire format for unambiguous
  standard/extended detection during unpack. RTR frames carry no data bytes.
  Signal extract/pack follows Motorola/Intel bit numbering convention for
  DBC compatibility.

## Responsible human

@user
