# xmodem — XMODEM/XMODEM-CRC receiver (passive, byte-fed)

**Status: candidate**
**Layer: L2 (protocols)**

Passive XMODEM/XMODEM-CRC receiver for firmware update over serial. Application
feeds bytes one at a time; the wheel handles framing, checksum/CRC verification,
ACK/NAK responses, and data delivery via write callback. No OS, no malloc, no
serial access — purely event-driven.

## Origin

Proactive build for firmware update over serial (common in embedded bootloaders).

## Why not VSF native?

VSF has no XMODEM implementation. The protocol is simple enough (< 200 lines)
that a standalone wheel is the right approach.

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    XMODEM_OK, XMODEM_ERR_PARAMS, XMODEM_ERR_TIMEOUT,
    XMODEM_ERR_CRC, XMODEM_ERR_SEQUENCE, XMODEM_ERR_CANCEL,
    XMODEM_ERR_WRITE, XMODEM_DONE,
} xmodem_err_t;

/* IO callbacks (caller provides) */
typedef struct {
    xmodem_err_t (*write_block)(void *ctx, uint32_t offset,
                                const uint8_t *data, uint16_t len);
    xmodem_err_t (*send_byte)(void *ctx, uint8_t byte);
    void *ctx;
} xmodem_io_t;

/* Lifecycle */
xmodem_err_t   xmodem_init(xmodem_t *xm, const xmodem_io_t *io, bool use_crc);
xmodem_err_t   xmodem_start(xmodem_t *xm);       /* sends NAK or 'C' */
xmodem_err_t   xmodem_feed(xmodem_t *xm, uint8_t byte);
xmodem_state_t xmodem_get_state(const xmodem_t *xm);
uint32_t       xmodem_get_total_bytes(const xmodem_t *xm);
void           xmodem_cancel(xmodem_t *xm);       /* sends CAN x2 */
```

### Usage pattern

```c
xmodem_io_t io = { .write_block = flash_write, .send_byte = uart_tx, .ctx = NULL };
xmodem_t xm;
xmodem_init(&xm, &io, true);   /* true = CRC mode */
xmodem_start(&xm);             /* sends 'C' to sender */

/* in UART RX interrupt or polling loop: */
while (xmodem_get_state(&xm) == XMODEM_STATE_RECEIVING ||
       xmodem_get_state(&xm) == XMODEM_STATE_WAIT_HEADER) {
    uint8_t byte = uart_getc();
    xmodem_err_t err = xmodem_feed(&xm, byte);
    if (err == XMODEM_DONE) break;
    if (err < 0) { /* handle error */ }
}
```

## Configuration (compile-time)

No compile-time defines. Block size is fixed at 128 bytes per XMODEM spec.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

- Receiver-only. Sender mode is not implemented.
- No XMODEM-1K (STX / 1024-byte blocks) support.
- Timeout detection is the caller's responsibility (the wheel is passive).

## How to test

```bash
cd wheels/protocols/xmodem/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_xmodem
```

## Test status

- Unit tests (sim): 19 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Passive byte-fed design; receiver-only; callbacks for IO;
  supports both checksum and CRC-16 modes.

## Responsible human

@user
