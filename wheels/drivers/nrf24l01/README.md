# nrf24l01 -- SPI driver for nRF24L01+ 2.4GHz radio transceiver

**Status: candidate**
**Layer: L1 (drivers)**

HAL-injected SPI driver for Nordic nRF24L01+ 2.4GHz ISM band wireless transceiver.
Supports TX/RX modes, configurable channel (0-125), data rate (250kbps/1Mbps/2Mbps),
output power (-18dBm to 0dBm), multi-pipe addressing, and auto-acknowledgment.

## Origin

Proactive build for wireless communication use cases (remote control, telemetry, sensor networks).


## Why self-written?

Searched GitHub (2026-05-02). Found ebrezadev/nRF24L01-C-Driver (permissive). Minimal; our impl covers needs. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

None (HAL callbacks injected via `nrf24_spi_t`).

## API

```c
/* Error codes */
typedef enum {
    NRF24_OK, NRF24_ERR_SPI, NRF24_ERR_PARAMS,
    NRF24_ERR_TIMEOUT, NRF24_ERR_MAX_RT, NRF24_ERR_FULL,
} nrf24_err_t;

/* SPI HAL -- caller provides */
typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*ce_low)(void *ctx);
    void    (*ce_high)(void *ctx);
    void    (*delay_us)(void *ctx, uint32_t us);
    void    *ctx;
} nrf24_spi_t;

/* Core API */
nrf24_err_t nrf24_init(nrf24_t *dev, const nrf24_spi_t *spi,
                       uint8_t channel, uint8_t payload_size);
nrf24_err_t nrf24_set_tx_addr(nrf24_t *dev, const uint8_t *addr, uint8_t addr_len);
nrf24_err_t nrf24_set_rx_addr(nrf24_t *dev, uint8_t pipe,
                              const uint8_t *addr, uint8_t addr_len);
nrf24_err_t nrf24_set_mode(nrf24_t *dev, nrf24_mode_t mode);
nrf24_err_t nrf24_set_data_rate(nrf24_t *dev, nrf24_data_rate_t rate);
nrf24_err_t nrf24_set_power(nrf24_t *dev, nrf24_power_t power);
nrf24_err_t nrf24_send(nrf24_t *dev, const uint8_t *data, uint8_t len);
nrf24_err_t nrf24_receive(nrf24_t *dev, uint8_t *data, uint8_t *len);
bool        nrf24_data_ready(nrf24_t *dev);
nrf24_err_t nrf24_flush_tx(nrf24_t *dev);
nrf24_err_t nrf24_flush_rx(nrf24_t *dev);
uint8_t     nrf24_read_status(nrf24_t *dev);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `NRF24_TX_TIMEOUT_US` | `10000` | Max wait for TX_DS/MAX_RT after send |

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/nrf24l01/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_nrf24l01
```

## Test status

- Unit tests (sim): 30 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: CE pin control via HAL callbacks; fixed payload mode;
  auto-ack on pipe 0 by default; blocking send with timeout.

## Responsible human

@user
