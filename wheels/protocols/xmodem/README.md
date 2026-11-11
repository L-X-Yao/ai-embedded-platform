# xmodem — XMODEM/XMODEM-CRC receiver (passive, byte-fed)

**Status: candidate**
**Layer: L2 (protocols)**

Passive XMODEM/XMODEM-CRC receiver for firmware update over serial. Application
feeds bytes one at a time; the wheel handles framing, checksum/CRC verification,
ACK/NAK responses, and data delivery via write callback. No OS, no malloc, no
serial access — purely event-driven.

## Origin

Proactive build for firmware update over serial (common in embedded bootloaders).

## Why self-written?

Searched GitHub (2026-05-02). Referenced AndreRenaud/async_xmodem (Unlicense). Well-designed non-blocking implementation. Self-written for platform convention consistency; implementation verified against XMODEM spec (SOH/EOT/ACK/NAK framing, CRC-16 CCITT polynomial 0x1021, block sequence numbering).

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
xmodem_err_t   xmodem_enable_line_idle(xmodem_t *xm);  /* before start */
xmodem_err_t   xmodem_start(xmodem_t *xm);       /* sends NAK or 'C' */
xmodem_err_t   xmodem_feed(xmodem_t *xm, uint8_t byte);
xmodem_err_t   xmodem_line_idle(xmodem_t *xm);   /* the line went quiet */
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

### With a timer

A caller that can tell when the line has been quiet for a second should say
so. This is what recovers from a lost byte: ymodem.txt 7.3.2 and 7.4 have a
receiver wait for the line to clear before it NAKs. The wheel owns no timer,
so the caller reports the quiet line and counts how long is too long.

```c
xmodem_init(&xm, &io, true);
xmodem_enable_line_idle(&xm);
xmodem_start(&xm);

for (;;) {
    int c = uart_getc_timeout_ms(1000);   /* -1 after a quiet second */
    xmodem_err_t err = (c < 0) ? xmodem_line_idle(&xm)
                               : xmodem_feed(&xm, (uint8_t)c);
    if (err == XMODEM_DONE) break;        /* comes from xmodem_line_idle() */
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
- Without `xmodem_enable_line_idle()`, a stray 0x04 between two clean blocks
  still reads as EOT, because plain XMODEM has one EOT and nothing else to
  check it against. A lost byte may also fail the transfer: a 0x01 in the data
  can lock a false frame at the same place on every resend until the sender
  gives up. It cannot end in `XMODEM_DONE` with the file cut short.
- A noise byte just before the last EOT also makes the receiver refuse that
  EOT without a timer, because only a good block or a quiet line clears it.

## Migration

### Quiet line and EOT checks (2026-11)

**What changed.** New calls `xmodem_enable_line_idle()` and
`xmodem_line_idle()`. `xmodem_feed()` now treats a bad block, or a byte that
cannot start one, as lost framing, and refuses EOT until a block passes its
checks. When the next block number is 4, the first EOT gets a NAK and the
sender's repeat gets the ACK, because block 4 with its SOH lost starts with the
same byte. `xmodem_t` is opaque storage and grew by three `bool` fields.

**What a caller has to do.** Nothing, to keep building and running. To recover
from a lost byte, follow "With a timer" above. `XMODEM_DONE` then comes from
`xmodem_line_idle()`, never from `xmodem_feed()`, so check that return value.

**If you do nothing.** Transfers run as before and a lost byte can no longer
report a short file as complete. A file of exactly 3 blocks needs the sender to
repeat its EOT, as ymodem.txt 7.3.3 requires it to. The limits under "Known
issues" stay.

## How to test

```bash
cd wheels/protocols/xmodem/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_xmodem
```

## Test status

- Unit tests (sim): 37 tests, all passing, including the loss of each byte of a
  transfer in turn
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Passive byte-fed design; receiver-only; callbacks for IO;
  supports both checksum and CRC-16 modes.

## Responsible human

@L-X-Yao
