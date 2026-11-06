# lin_bus

**Status: candidate**
**Layer: L2 (protocols)**

LIN bus (Local Interconnect Network) frame encoding and decoding. Handles
PID calculation with parity bits, classic (LIN 1.x) and enhanced (LIN 2.0+)
checksums, and full frame encode/decode with sync byte and checksum validation.
Break field is at UART level and not included. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). LIN bus (Local Interconnect Network) frame encoding and decoding. Handles


## Why self-written?

existing open-source has no LIN bus protocol support. LIN is widely used for low-cost
automotive sub-networks (seat control, mirrors, climate, lighting).

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "lin_bus.h"

uint8_t       lin_bus_calc_pid(uint8_t id);
bool          lin_bus_verify_pid(uint8_t pid);
uint8_t       lin_bus_calc_checksum(const lin_bus_frame_t *frame);
lin_bus_err_t lin_bus_encode(const lin_bus_frame_t *frame,
                             uint8_t *buf, size_t cap, size_t *len);
lin_bus_err_t lin_bus_decode(const uint8_t *buf, size_t len,
                             lin_bus_frame_t *frame);
```

## Usage

```c
/* Encode a LIN frame */
lin_bus_frame_t frame = {
    .id = 0x10, .data = {0x01, 0x02}, .data_len = 2,
    .checksum_type = LIN_BUS_CHECKSUM_ENHANCED,
};
uint8_t buf[LIN_BUS_MAX_FRAME_LEN];
size_t len;
lin_bus_encode(&frame, buf, sizeof(buf), &len);
uart_send_break();
uart_send(buf, len);

/* Decode */
lin_bus_frame_t rx;
lin_bus_decode(uart_buf, uart_len, &rx);
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\lin_bus/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lin_bus
```


## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
