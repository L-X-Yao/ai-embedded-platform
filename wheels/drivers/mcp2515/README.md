# MCP2515 — SPI CAN Bus Controller Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Microchip MCP2515 stand-alone CAN controller. Assumes 8 MHz crystal (standard module configuration).

## Origin

Proactive build for CAN bus communication in embedded systems.

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Pure C99 with injected SPI HAL callbacks.

## API

```c
mcp2515_err_t mcp2515_init(mcp2515_t *dev, const mcp2515_spi_t *spi, mcp2515_speed_t speed);
mcp2515_err_t mcp2515_set_mode(mcp2515_t *dev, mcp2515_mode_t mode);
mcp2515_err_t mcp2515_send(mcp2515_t *dev, const mcp2515_msg_t *msg);
mcp2515_err_t mcp2515_receive(mcp2515_t *dev, mcp2515_msg_t *msg);
bool          mcp2515_has_message(mcp2515_t *dev);
mcp2515_err_t mcp2515_set_filter(mcp2515_t *dev, uint8_t filter_num, uint32_t id, bool extended);
mcp2515_err_t mcp2515_set_mask(mcp2515_t *dev, uint8_t mask_num, uint32_t mask);
uint8_t       mcp2515_read_status(mcp2515_t *dev);
uint8_t       mcp2515_read_errors(mcp2515_t *dev);
```

Caller provides SPI HAL via `mcp2515_spi_t` callbacks (CS control + byte transfer).

## Configuration

| Macro | Default | Description |
|-------|---------|-------------|
| `MCP2515_MODE_TIMEOUT` | `100000U` | Iteration limit for mode-switch polling; tune for SPI clock speed |

## Supported platforms

- [x] Host C99 (mock SPI) — 17 tests, 2026-05-01
- [ ] Real MCP2515 hardware — not yet verified

## Known issues

- Only TX buffer 0 is used; TX buffers 1 and 2 are not utilized (no priority queueing)
- Speed settings assume 8 MHz crystal; other oscillator frequencies require manual CNF register configuration
- Filter/mask operations require config mode; caller must switch modes explicitly

## How to test

```bash
cd wheels/drivers/mcp2515/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Host mock tests: PASS (17 tests — init, NULL safety, set mode normal/loopback, send standard/extended frame, receive standard/extended, has_message, no message, set filter, set mask, read status, read errors, speed config (all 4 rates), TX busy, DLC validation)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: device handle struct (mcp2515_t) holds SPI pointer and current mode; 8 MHz crystal assumed; single TX buffer for simplicity

## Responsible human

@user

## Features

- Hardware reset + automatic config mode entry
- Four CAN bus speeds: 125 kbps, 250 kbps, 500 kbps, 1 Mbps (8 MHz crystal)
- Standard (11-bit) and extended (29-bit) frame support
- RTR frame support
- TX buffer busy detection
- RX from both receive buffers (RXB0/RXB1) with automatic flag clearing
- 6 acceptance filters + 2 acceptance masks (config mode only)
- Status register quick poll (SPI READ_STATUS instruction)
- Error flag register read
- Bounded mode-switch polling (configurable iteration limit)
- SPI HAL injection via callback struct — zero RTOS / platform dependency

## Usage

```c
static void my_cs_low(void *ctx)  { gpio_bit_reset(GPIOB, GPIO_PIN_12); }
static void my_cs_high(void *ctx) { gpio_bit_set(GPIOB, GPIO_PIN_12); }
static uint8_t my_xfer(void *ctx, uint8_t tx) { /* SPI full-duplex byte */ }

mcp2515_spi_t spi = { .cs_low = my_cs_low, .cs_high = my_cs_high, .xfer = my_xfer };
mcp2515_t can;
if (mcp2515_init(&can, &spi, MCP2515_SPEED_500KBPS) == MCP2515_OK) {
    mcp2515_msg_t msg = { .id = 0x123, .dlc = 3, .data = {0xAA, 0xBB, 0xCC} };
    mcp2515_send(&can, &msg);

    if (mcp2515_has_message(&can)) {
        mcp2515_msg_t rx;
        mcp2515_receive(&can, &rx);
    }
}
```
