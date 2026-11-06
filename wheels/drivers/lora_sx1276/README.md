# lora_sx1276 — SPI LoRa radio transceiver driver for SX1276/SX1278

**Status: candidate**
**Layer: L1 (drivers)**

PHY-layer driver for Semtech SX1276/SX1278 LoRa radio transceivers. Handles
frequency/bandwidth/spreading-factor/coding-rate/power configuration, packet
send/receive, RSSI/SNR readout, and sleep/standby power modes. All SPI and
reset-pin access is HAL-injected.
## Origin

Proactive build for LoRa-based IoT projects. Pairs with `lorawan_packet` (L2)
for MAC-layer framing.


## Why self-written?

Searched GitHub (2026-05-02). Referenced StuartsProjects/SX12XX-LoRa (MIT) for register sequences and compensation algorithms. Radio config patterns; complex register programming. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

- `spi_hal` (callback injection, no concrete dependency)

## API

```c
typedef enum {
    LORA_OK          =  0,
    LORA_ERR_SPI     = -1,
    LORA_ERR_PARAMS  = -2,
    LORA_ERR_TIMEOUT = -3,
    LORA_ERR_CRC     = -4,
    LORA_ERR_VERSION = -5,
} lora_err_t;

typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*reset)(void *ctx);
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} lora_spi_t;

typedef struct {
    const lora_spi_t *spi;
    uint32_t frequency;        /* Hz */
    uint8_t  bandwidth;        /* 0-9 */
    uint8_t  spreading_factor; /* 7-12 */
    uint8_t  coding_rate;      /* 1-4 */
    int8_t   tx_power;         /* dBm (2-20) */
} lora_t;

lora_err_t lora_init(lora_t *dev, const lora_spi_t *spi, uint32_t freq_hz);
lora_err_t lora_set_frequency(lora_t *dev, uint32_t freq_hz);
lora_err_t lora_set_bandwidth(lora_t *dev, uint8_t bw);
lora_err_t lora_set_spreading_factor(lora_t *dev, uint8_t sf);
lora_err_t lora_set_coding_rate(lora_t *dev, uint8_t cr);
lora_err_t lora_set_tx_power(lora_t *dev, int8_t dbm);
lora_err_t lora_send(lora_t *dev, const uint8_t *data, uint8_t len);
lora_err_t lora_receive(lora_t *dev, uint8_t *data, uint8_t *len, uint8_t max_len);
bool       lora_packet_available(lora_t *dev);
int16_t    lora_get_rssi(lora_t *dev);
int8_t     lora_get_snr(lora_t *dev);
lora_err_t lora_sleep(lora_t *dev);
lora_err_t lora_standby(lora_t *dev);
```

## Configuration (compile-time)

None. All parameters set at runtime via API calls.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware (SX1276/SX1278 module) — not yet verified

## Datasheet check

Checked on 2026-11-06 against Semtech SX1276/77/78/79 Rev. 4. Not run on hardware.

- **Matched:** RegVersion, register addresses, SPI framing, mode codes, IRQ bits, the Frf formula, the FIFO flow.
- **Fixed:** Setup registers went to the FSK page because the LoRa bit was set from standby. There was no calibration at the working frequency, and the payload CRC was off. A packet without a CRC was accepted. LowDataRateOptimize never followed the settings. SF6 was accepted without the implicit header it needs. +20 dBm kept the 100 mA current limit. Long packets hit a fixed 2 s limit, and RxTimeout was ignored. RSSI used the wrong offset at 862-868 MHz and left out a negative SNR. Frequencies outside the three bands are refused, and so are 250 and 500 kHz in the low band. The model now keeps the LoRa and FSK pages apart, returns to Standby after RxDone and RxTimeout, resets the TX FIFO base to 0x80 and transmits from it.
- **Still open:** `lora_set_frequency` after init does not calibrate again. `lora_receive` listens for one RX single window, 100 symbols by default.
- **Suspected:** `set_mode` clears LowFrequencyModeOn on every mode change. RegVersion must be exactly 0x12, though its low bits are a metal mask revision. On a slow bus the read-back after TX can already see standby.

## Known issues

- TX/RX uses polling (delay_ms loop), not interrupt-driven. Suitable for
  simple applications; interrupt-based mode can be added later.
- High-power mode (+20 dBm) requires PA_BOOST pin wired; some modules
  may need external PA enable pin not covered by this driver.

## How to test

```bash
cd wheels/drivers/lora_sx1276/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lora_sx1276
```

## Test status

- Unit tests (sim): 20 pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Short prefix `lora_` (not `lora_sx1276_`) since the API
  is generic enough for the SX127x family. Error codes use `LORA_` prefix.
  SPI HAL includes `reset` and `delay_ms` callbacks beyond the standard
  cs_low/cs_high/xfer triple.

## Responsible human

@user
