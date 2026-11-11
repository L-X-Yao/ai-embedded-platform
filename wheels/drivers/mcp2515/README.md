# MCP2515 — SPI CAN Bus Controller Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Microchip MCP2515 stand-alone CAN controller. Assumes 8 MHz crystal (standard module configuration).

## Origin

Proactive build for CAN bus communication in embedded systems.

## Why self-written?

Searched GitHub (2026-05-02). Found parmAshu/mcp2515_driver (MIT). Limited adoption; ours fine. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

None. Pure C99 with injected SPI HAL callbacks.

## API

```c
mcp2515_err_t mcp2515_init(mcp2515_t *dev, const mcp2515_spi_t *spi, mcp2515_speed_t speed);
mcp2515_err_t mcp2515_set_mode(mcp2515_t *dev, mcp2515_mode_t mode);
mcp2515_err_t mcp2515_send(mcp2515_t *dev, const mcp2515_msg_t *msg);
mcp2515_err_t mcp2515_abort_tx(mcp2515_t *dev);
mcp2515_err_t mcp2515_receive(mcp2515_t *dev, mcp2515_msg_t *msg);
bool          mcp2515_has_message(mcp2515_t *dev);
mcp2515_err_t mcp2515_set_filter(mcp2515_t *dev, uint8_t filter_num, uint32_t id, bool extended);
mcp2515_err_t mcp2515_set_mask(mcp2515_t *dev, uint8_t mask_num, uint32_t mask, bool extended);
uint8_t       mcp2515_read_status(mcp2515_t *dev);
uint8_t       mcp2515_read_errors(mcp2515_t *dev);
```

Caller provides SPI HAL via `mcp2515_spi_t` callbacks (CS control + byte transfer + `delay_us`). `delay_us` is required: `mcp2515_init` returns `MCP2515_ERR_PARAM` and sends nothing without it.

## Usage

```c
static void my_cs_low(void *ctx)  { (void)ctx; }
static void my_cs_high(void *ctx) { (void)ctx; }
static uint8_t my_xfer(void *ctx, uint8_t tx) { (void)ctx; return tx; }
static void my_delay_us(void *ctx, uint32_t us) { (void)ctx; (void)us; }

static void example(void) {
    mcp2515_spi_t spi = { .cs_low = my_cs_low, .cs_high = my_cs_high, .xfer = my_xfer,
                          .delay_us = my_delay_us };
    mcp2515_t can;
    if (mcp2515_init(&can, &spi, MCP2515_SPEED_500KBPS) == MCP2515_OK) {
        mcp2515_msg_t msg = { .id = 0x123, .dlc = 3, .data = {0xAA, 0xBB, 0xCC} };
        mcp2515_send(&can, &msg);

        if (mcp2515_has_message(&can)) {
            mcp2515_msg_t rx;
            mcp2515_receive(&can, &rx);
        }
    }
}
```

## Configuration (compile-time)

| Macro | Default | Description |
|-------|---------|-------------|
| `MCP2515_MODE_TIMEOUT` | `100000U` | Iteration limit for mode-switch polling; tune for SPI clock speed |

## Supported platforms

- [x] Host C99 (mock SPI) — 17 tests, 2026-05-01
- [ ] Real MCP2515 hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against Microchip DS20001801J. Not run on hardware.

- **Matched:** SPI opcodes, register addresses, mode encodings and polling, BIT MODIFY targets, ID packing, the 125 to 500 kbps timing values.
- **Fixed:** The 1 Mbps setting broke the 2 TQ minimum for PS2 and no legal one exists at 8 MHz, so it is now refused. Standard remote frames read as data frames. Read-backs of TXREQ and RXnIF turned a fast send or a busy bus into `MCP2515_ERR_SPI`. Two comments were wrong. The models use the CANCTRL reset value and clear TXREQ. `mcp2515_init` read CANSTAT straight after RESET, inside the Oscillator Start-up Timer. It now waits through the new `delay_us`, for 128 us, which covers 128 OSC1 cycles at the slowest clock the chip allows. `mcp2515_set_mode` could not leave Sleep, because nothing set WAKIF, so any mode after Sleep timed out. It now sets WAKIE and WAKIF, waits the same time, requests the mode and puts both flags back; INT goes low meanwhile. The model and mock changed mode out of Sleep at once, which hid this, and the model now counts SPI operations inside the timer. These two fixes have not run on hardware. `mcp2515_send` returned OK in Listen-Only, Sleep and Configuration mode, where the frame can never go out and then holds up every mode change; it now returns `MCP2515_ERR_PARAM`. The model holds a mode change while a frame is pending, takes CNF, TXRTSCTRL, filter and mask writes only in Configuration mode and reads filters and masks as 0 elsewhere, and it has read-only bits, a running address pointer, BIT MODIFY as a write outside Table 11-1 and wake-up by bus activity. `mcp2515_init` promised to accept all messages but cleared only the masks, while EXIDE is unknown after power-on and gives each filter one frame format (p.35). It now writes six zero filters, even ones standard and odd ones extended, as arduino-mcp2515 and MCP_CAN_lib do. Both formats still need a check on a real bus. There was no way to take back a frame that could not go out. `mcp2515_abort_tx` clears TXREQ of the buffer `mcp2515_send` uses, which also frees a mode change the frame holds up. A frame already on the bus goes on, so `MCP2515_OK` only says the buffer is free. The manual does not say what TXREQ reads during such a frame: `MCP2515_ERR_TX_BUSY` covers it staying set, which the model assumes.
- **Still open:** init cannot know when power came up, so the caller must not call it during the power-on start-up. The manual defines the wake-up wait as 128 TOSC and gives no crystal start-up time, so only hardware can show whether 128 us is enough before the mode request. After `MCP2515_ERR_TIMEOUT` on a pending frame the request stays in REQOP, so the chip changes mode once the frame is out, unless `mcp2515_abort_tx` takes the frame back first.

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

- Host mock tests: PASS (41 tests — init, NULL safety, set mode normal/loopback, send standard/extended frame, receive standard/extended, has_message, no message, set filter, set mask, read status, read errors, speed config (all 4 rates), TX busy, DLC validation)
- Model tests (sim): 53 pass
- HIL: N/A

## Migration

From the fixes of 2026-11.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| New `mcp2515_abort_tx` | Nothing. Call it to take back a pending frame | Nothing changes for the caller |

## API convention deviations

None.

## Features

- Hardware reset + automatic config mode entry
- Three CAN bus speeds: 125 kbps, 250 kbps, 500 kbps (8 MHz crystal)
- Standard (11-bit) and extended (29-bit) frame support
- RTR frame support
- TX buffer busy detection
- RX from both receive buffers (RXB0/RXB1) with automatic flag clearing
- 6 acceptance filters + 2 acceptance masks (config mode only)
- Status register quick poll (SPI READ_STATUS instruction)
- Error flag register read
- Bounded mode-switch polling (configurable iteration limit)
- SPI HAL injection via callback struct — zero RTOS / platform dependency

## API review

- **Date**: 2026-05-01
- **Key decisions**: device handle struct (mcp2515_t) holds SPI pointer and current mode; 8 MHz crystal assumed; single TX buffer for simplicity

## Responsible human

@L-X-Yao
