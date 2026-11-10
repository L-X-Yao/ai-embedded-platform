# enc28j60 — SPI Ethernet controller driver

**Status: candidate**
**Layer: L1 (drivers)**

Full send/receive driver for the Microchip ENC28J60 SPI Ethernet controller.
Supports banked register access, configurable MAC address, and standard
Ethernet frames up to 1518 bytes on the wire: `enc28j60_send` takes 1514 and the
chip appends the CRC. HAL-injected SPI for mock-testable host builds.

## Origin

Proactive build for embedded Ethernet connectivity (IoT / telemetry / industrial).

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. All impls tied to network stacks. Self-written with HAL injection per platform conventions.

## Dependencies

None (SPI HAL injected by caller, `delay_ms` included).

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

`enc28j60_receive` gives the frame without its 4-byte CRC, so a `buf_size` of 1514 always does. `ENC28J60_ERR_SIZE` means the frame was dropped: `*frame_len` then says how long it was, or 0 if the chip's count was too short to be a frame.

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `ENC28J60_RESET_DELAY_MS` | 2 | Delay after soft reset (ms), at least 1 |
| `ENC28J60_TX_TIMEOUT` | 50000 | TX busy-wait loop iterations |
| `ENC28J60_READY_POLLS` | 10 | Reads of CLKRDY, MISTAT.BUSY or RXBUSY, 1 ms apart, before `ENC28J60_ERR_TIMEOUT` |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against Microchip DS39662C and its errata DS80349C. Not run on hardware.

- **Matched:** SPI opcodes, register addresses and bit masks, no dummy byte on ETH registers, the half duplex MAC values, MAC byte order, buffer layout, the TX sequence, RX header parsing, the odd ERXRDPT when freeing a packet.
- **Fixed:** `enc28j60_init` never polled CLKRDY and took a HAL with no `delay_ms`, which cannot give the 1 ms the errata asks for after a reset. It left PHCON1.PDPXMD to the LEDB wiring although FULDPX must match it, left half duplex loopback on, left MACON4.DEFER clear, and started ERXRDPT on an even value, which the errata says may corrupt the receive buffer. `enc28j60_send` reported aborted frames as sent, left TXRTS set after a timeout so the next send wrote under a pending transmission, and took frames of 1515 to 1518 bytes, which the chip aborts once the CRC is added. `enc28j60_send` also never reset the transmit logic, so after a half duplex abort the next send could end in `ENC28J60_ERR_TIMEOUT` (errata issue 12). `enc28j60_set_mac` wrote the address with reception running. The model and mock now send the MAC and MII dummy byte, set Received Ok in injected packets, and keep BFS and BFC off MAC and MII registers. The model starts with CLKRDY clear and the Table 3-2 reset values, BFC clears the R/C bits of ESTAT and EIR, PKTDEC decrements EPKTCNT, and the read pointer wraps at ERXND. `enc28j60_receive` returned the 4 CRC bytes with the frame, and refused the longest frame with `ENC28J60_ERR_SIZE` from a 1514-byte buffer. It now leaves them out, and a count below 4 is dropped instead of wrapping. The model counts and stores those 4 bytes as Table 7-3 says.
- **Still open:** Receive overflow is not reported, so dropped frames go unnoticed, and there is no link status, so a lost cable shows only as silence. `enc28j60_receive` ignores the receive status vector.
- **Suspected:** CS rises straight after the last byte, but MAC and MII registers need 210 ns of hold, which a fast MCU may break.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/enc28j60/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_enc28j60
```

## Test status

- Unit tests (sim): 98 pass, 0 fail (30 main + 53 hw_model + 13 null + 2 prop)
- HIL: N/A

## Migration

From the fixes of 2026-11.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| `*frame_len` is the frame without the CRC, 4 less than before, and the CRC is no longer copied | Stop subtracting 4 if you did | A caller that still subtracts 4 loses the last 4 bytes of every frame |
| `buf_size` only has to hold the frame | Nothing. 1514 bytes always do, where 1518 were needed | A larger buffer still works |
| With `ENC28J60_ERR_SIZE`, `*frame_len` is also without the CRC, and 0 for a chip count below 4 | Nothing, unless you size a retry from it | The number is 4 less than before |

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Stateful device handle (banked register caching); SPI HAL
  with delay_ms callback; separate init/send/receive; ENC28J60_MAX_FRAME = 1518

## Responsible human

@user
