# w5500 -- SPI Ethernet controller driver

**Status: candidate**
**Layer: L1 (drivers)**

Full driver for the Wiznet W5500 hardwired TCP/IP embedded Ethernet controller.
Supports 8 independent sockets with configurable TX/RX buffers, TCP/UDP/MACRAW
protocols, and 32KB on-chip buffer memory. HAL-injected SPI for mock-testable
host builds.

## Origin

Proactive build for embedded wired Ethernet connectivity (IoT / industrial / telemetry).

## Why self-written?

Searched GitHub (2026-05-07). Existing W5500 libraries (ioLibrary_Driver, etc.) are
tightly coupled to specific platforms or network stacks. Self-written with HAL
injection per platform conventions for zero-dependency portability.

## Dependencies

None (SPI HAL injected by caller).

## API

```c
w5500_err_t w5500_init(w5500_t *dev, const w5500_spi_t *spi);
w5500_err_t w5500_set_mac(w5500_t *dev, const uint8_t mac[6]);
w5500_err_t w5500_set_ip(w5500_t *dev, const uint8_t ip[4],
                          const uint8_t subnet[4],
                          const uint8_t gateway[4]);
w5500_err_t w5500_socket_open(w5500_t *dev, uint8_t sock,
                               uint8_t protocol, uint16_t port);
w5500_err_t w5500_socket_close(w5500_t *dev, uint8_t sock);
w5500_err_t w5500_socket_send(w5500_t *dev, uint8_t sock,
                               const uint8_t *data, uint16_t len);
w5500_err_t w5500_socket_recv(w5500_t *dev, uint8_t sock,
                               uint8_t *buf, uint16_t buf_len,
                               uint16_t *out_len);
w5500_err_t w5500_socket_status(w5500_t *dev, uint8_t sock,
                                 uint8_t *status);
uint8_t     w5500_read_version(w5500_t *dev);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `W5500_RESET_DELAY_MS` | 2 | Delay after software reset (ms) |
| `W5500_CMD_TIMEOUT` | 50000 | Command completion busy-wait iterations |
| `W5500_SOCK_BUF_SIZE` | 2048 | TX/RX buffer size per socket (bytes) |

## Supported platforms

- [x] Host (sim) -- 2026-05-07
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/w5500/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_w5500
```

## Test status

- Unit tests (sim): 52 pass, 0 fail
- Null tests (sim): 14 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-07
- **Key decisions**: Stateful device handle (no banked registers -- W5500 uses BSB
  in SPI frame); SPI HAL with delay_ms callback; socket-centric API with open/close
  /send/recv/status; version check on init (VERSIONR=0x04); error codes include
  W5500_ERR_SOCK for invalid socket number

## Responsible human

@user
