# w5500 -- SPI Ethernet controller driver

**Status: candidate**
**Layer: L1 (drivers)**

Partial driver for the Wiznet W5500 hardwired TCP/IP embedded Ethernet controller.
It resets and identifies the chip, sets the MAC and IP addresses, opens and closes
8 sockets, and moves data through their 2 KB buffers. HAL-injected SPI for
mock-testable host builds.

**TCP and UDP cannot work yet.** `w5500_socket_open` accepts both, but no API sets
the peer address and port or issues CONNECT or LISTEN, so a TCP socket never
connects and a UDP send has no peer to go to. Only MACRAW, on socket 0, needs no
peer.

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
| `W5500_CMD_TIMEOUT` | 50000 | Limit on each busy-wait, in register reads: Sn_CR, Sn_SR, Sn_IR and the size registers |
| `W5500_SOCK_BUF_SIZE` | 2048 | Largest send accepted (bytes). Does not resize the chip's buffers: init always sets 2 KB |

## Supported platforms

- [x] Host (sim) -- 2026-05-07
- [ ] Real hardware target -- not yet verified

## Datasheet check

Checked on 2026-11-06 against W5500 Datasheet Version 1.1.0, (c) 2013 WIZnet Co., Ltd. Not run on hardware.

- **Matched:** SPI frame and block select bits, common and socket register offsets, mode, command and status values, byte order, the TX and RX pointer sequences, buffer size values.
- **Fixed:** `w5500_socket_send` never read Sn_IR, so an ARP or TCP timeout came back as `W5500_OK`. `w5500_socket_open` read Sn_SR once, so a good open could fail, and it could send OPEN to a socket still closing. `w5500_socket_close` could return before the socket had closed. The free and received sizes were read once, so a torn value could overwrite unsent data or over-read the RX buffer. MACRAW was accepted on sockets 1 to 7, and a zero-length receive buffer produced an illegal SPI frame.
- **Still open:** TCP and UDP cannot work: no API sets Sn_DIPR or Sn_DPORT or issues CONNECT or LISTEN. `w5500_socket_send` gives up after `W5500_CMD_TIMEOUT` reads, which can come before the chip's own 1.8 s ARP timeout, and the chip then keeps trying, so a resend at once can overlap it. Overriding `W5500_SOCK_BUF_SIZE` does not resize the buffers. There is no link status, RSTn or PHY mode handling, no interrupts, and no DISCON, keep-alive, TTL, TOS or multicast. Packet-info headers in the RX buffer, which this manual does not describe, are not parsed. No test uses `hw_model_w5500.h`: it drops buffer access past 2 KB, does nothing on SEND and RECV, changes Sn_SR at once, loses the reset values of RTR, RCR, Sn_IMR and Sn_TTL, and lets writes change read-only registers.
- **Suspected:** `w5500_init` never polls MR.RST and waits not at all when `delay_ms` is NULL, so it may read VERSIONR during the reset.

## Known issues

- TCP and UDP cannot work yet: see the top of this file. The other open items are under Datasheet check.

## How to test

```bash
cd wheels/drivers/w5500/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_w5500
```

## Test status

- Unit tests (sim): 68 pass, 0 fail
- Null tests (sim): 18 pass, 0 fail
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
