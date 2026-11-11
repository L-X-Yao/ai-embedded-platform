# isotp

**Status: candidate**
**Layer: L2 (protocols)**

ISO-TP (ISO 15765-2) CAN multi-frame transport protocol codec. Handles
Single Frame (SF), First Frame (FF), Consecutive Frame (CF), and Flow
Control (FC). Provides stateful reassembly for multi-frame messages up to
4095 bytes over classic CAN (8-byte DLC). Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). ISO-TP (ISO 15765-2) CAN multi-frame transport protocol codec. Handles


## Why self-written?

existing open-source has no ISO-TP implementation. This wheel is a prerequisite for UDS
(Unified Diagnostic Services) over CAN, common in automotive and
industrial embedded systems.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "isotp.h"

/* Receiver (stateful reassembly) */
isotp_err_t        isotp_rx_init(isotp_rx_t *rx, uint8_t *buf, size_t cap);
isotp_err_t        isotp_rx_feed(isotp_rx_t *rx, const uint8_t *can_data, uint8_t dlc);
size_t             isotp_rx_length(const isotp_rx_t *rx);
const uint8_t     *isotp_rx_data(const isotp_rx_t *rx);
isotp_err_t        isotp_rx_reset(isotp_rx_t *rx);

/* Transmitter helpers */
isotp_frame_type_t isotp_frame_type(uint8_t pci);
isotp_err_t        isotp_encode_sf(const uint8_t *payload, size_t len, uint8_t *can_data);
isotp_err_t        isotp_encode_fc(isotp_fc_flag_t flag, uint8_t block_size,
                                    uint8_t st_min, uint8_t *can_data);
```

## Usage

```c
/* Receive multi-frame message */
isotp_rx_t rx;
uint8_t reassembly[256];
isotp_rx_init(&rx, reassembly, sizeof(reassembly));

void on_can_rx(const uint8_t *data, uint8_t dlc) {
    isotp_err_t err = isotp_rx_feed(&rx, data, dlc);
    if (err == ISOTP_OK) {
        process_uds(isotp_rx_data(&rx), isotp_rx_length(&rx));
        isotp_rx_reset(&rx);
    }
}

/* Send single-frame message */
uint8_t can_buf[8];
isotp_encode_sf(payload, 5, can_buf);
can_send(can_buf, 8);
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/protocols\isotp/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_isotp
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

@L-X-Yao
