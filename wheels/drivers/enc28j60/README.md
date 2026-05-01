# enc28j60 — SPI Ethernet controller driver

**Status: candidate**
**Layer: L1 (drivers)**

Full send/receive driver for the Microchip ENC28J60 SPI Ethernet controller.
Supports banked register access, configurable MAC address, and standard
Ethernet frames up to 1518 bytes. HAL-injected SPI for mock-testable host builds.

## Origin

Proactive build for embedded Ethernet connectivity (IoT / telemetry / industrial).

## Why not VSF native?

VSF has no dedicated ENC28J60 driver. The `vsf_netdrv_t` interface handles
higher-level networking but does not provide chip-level register access or
SPI framing for this specific controller.

## Dependencies

None (SPI HAL injected by caller).

## API

```c
enc28j60_err_t enc28j60_init(enc28j60_t *dev, const enc28j60_spi_t *spi,
                              const uint8_t *mac_addr);
enc28j60_err_t enc28j60_send(enc28j60_t *dev, const uint8_t *frame,
                              uint16_t len);
enc28j60_err_t enc28j60_receive(enc28j60_t *dev, uint8_t *frame,
                                 uint16_t buf_size, uint16_t *frame_len);
bool           enc28j60_has_packet(enc28j60_t *dev);
uint8_t        enc28j60_get_revision(enc28j60_t *dev);
enc28j60_err_t enc28j60_set_mac(enc28j60_t *dev, const uint8_t *mac_addr);
void           enc28j60_enable_reception(enc28j60_t *dev);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `ENC28J60_RESET_DELAY_MS` | 2 | Delay after soft reset (ms) |
| `ENC28J60_TX_TIMEOUT` | 50000 | TX busy-wait loop iterations |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/enc28j60/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_enc28j60
```

## Test status

- Unit tests (sim): 22 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Stateful device handle (banked register caching); SPI HAL
  with delay_ms callback; separate init/send/receive; ENC28J60_MAX_FRAME = 1518

## Responsible human

@user
