# ymodem — YMODEM batch file transfer receiver (passive, byte-fed)

**Status: candidate**
**Layer: L2 (protocols)**

Passive YMODEM batch receiver for firmware update over serial. Extends XMODEM
with filename/size metadata in block 0, 1024-byte STX blocks, and multi-file
batch transfers. Application feeds bytes one at a time; the wheel handles
framing, CRC-16 verification, ACK/NAK responses, and data delivery via write
callback. No OS, no malloc, no serial access — purely event-driven.

## Origin

Proactive build for firmware update over serial (YMODEM batch mode, common in
embedded bootloaders). Extends the existing `xmodem` wheel with metadata and
1024-byte block support.

## Why self-written?

Searched GitHub (2026-05-02). Referenced Dinamiet/YModem (varies license). Decent but less proven alternative. Self-written for platform convention consistency; implementation verified against YMODEM spec (block 0 filename/size metadata, SOH/STX dual block sizes, CRC-16 mandatory).

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    YMODEM_OK, YMODEM_ERR_PARAMS, YMODEM_ERR_CRC, YMODEM_ERR_SEQ,
    YMODEM_ERR_CANCEL, YMODEM_ERR_WRITE, YMODEM_ERR_OVERFLOW, YMODEM_ERR_SHORT,
    YMODEM_DONE,
} ymodem_err_t;

/* IO callbacks (caller provides) */
typedef struct {
    ymodem_err_t (*write_block)(void *ctx, uint32_t offset,
                                const uint8_t *data, uint16_t len);
    ymodem_err_t (*send_byte)(void *ctx, uint8_t byte);
    void (*on_file_start)(void *ctx, const char *filename, uint32_t file_size);
    void *ctx;
} ymodem_io_t;

/* Lifecycle */
ymodem_err_t   ymodem_init(ymodem_t *ym, const ymodem_io_t *io);
ymodem_err_t   ymodem_enable_line_idle(ymodem_t *ym);  /* before start */
ymodem_err_t   ymodem_start(ymodem_t *ym);          /* sends 'C' */
ymodem_err_t   ymodem_feed(ymodem_t *ym, uint8_t byte);
ymodem_err_t   ymodem_line_idle(ymodem_t *ym);      /* the line went quiet */
ymodem_state_t ymodem_get_state(const ymodem_t *ym);
const char    *ymodem_get_filename(const ymodem_t *ym);
uint32_t       ymodem_get_file_size(const ymodem_t *ym);
uint32_t       ymodem_get_total_received(const ymodem_t *ym);
void           ymodem_cancel(ymodem_t *ym);          /* sends CAN x2 */
```

### Usage pattern

```c
ymodem_io_t io = {
    .write_block   = flash_write,
    .send_byte     = uart_tx,
    .on_file_start = on_new_file,  /* optional — may be NULL */
    .ctx           = NULL,
};
ymodem_t ym;
ymodem_init(&ym, &io);
ymodem_start(&ym);              /* sends 'C' to sender */

/* in UART RX interrupt or polling loop: */
while (ymodem_get_state(&ym) == YMODEM_STATE_HEADER ||
       ymodem_get_state(&ym) == YMODEM_STATE_DATA) {
    uint8_t byte = uart_getc();
    ymodem_err_t err = ymodem_feed(&ym, byte);
    if (err == YMODEM_DONE) break;
    if (err < 0) { /* handle error */ }
}
```

### With a timer

A caller that can tell when the line has been quiet for a second should say
so. Without it one lost byte usually costs the transfer: ymodem.txt 7.3.2 and
7.4 have a receiver wait for the line to clear before it NAKs. The wheel owns
no timer, so the caller reports the quiet line and counts how long is too long.

```c
ymodem_init(&ym, &io);
ymodem_enable_line_idle(&ym);
ymodem_start(&ym);

for (;;) {
    int c = uart_getc_timeout_ms(1000);   /* -1 after a quiet second */
    ymodem_err_t err = (c < 0) ? ymodem_line_idle(&ym)
                               : ymodem_feed(&ym, (uint8_t)c);
    if (err == YMODEM_DONE) break;
    if (err < 0) { /* handle error */ }
}
```

## Configuration (compile-time)

No compile-time defines. SOH blocks are 128 bytes, STX blocks are 1024 bytes
per YMODEM spec.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

- Receiver-only. Sender mode is not implemented.
- A file name of 64 bytes or more ends the transfer with CAN CAN and `YMODEM_ERR_OVERFLOW`.
- Timeout detection is the caller's responsibility (the wheel is passive).
- Filename field is capped at 63 characters.
- Without `ymodem_enable_line_idle()`, one lost byte usually fails the transfer:
  a 0x01 or 0x02 in the data locks a false frame at the same place on every
  resend until the sender gives up. The failure is reported, never silent.
- Block 0 need not carry a size, and then a short file cannot be detected from
  the protocol alone: check `ymodem_get_total_received()`, which includes the
  padding of the last block.

## Migration

### Two CANs, quiet line, short files (2026-11)

**What changed.**

- Cancel needs two consecutive CANs at a frame boundary (ymodem.txt 4.1). One
  CAN used to return `YMODEM_ERR_CANCEL` at once.
- `ymodem_feed()` treats a bad block, or a byte that cannot start one, as lost
  framing, and refuses EOT until a block passes its checks.
- The sender's two EOTs must be back to back. Any other byte restarts the pair.
- New `YMODEM_ERR_SHORT`: the second EOT of a file that stopped short of the
  size in its block 0 now gets CAN CAN and this error instead of an ACK. That
  includes a sender that announces more than it sends on purpose.
- New calls `ymodem_enable_line_idle()`, `ymodem_line_idle()` and
  `ymodem_get_total_received()`.
- `ymodem_t` is opaque storage and grew by three small fields.

**What a caller has to do.** Treat `YMODEM_ERR_SHORT` as a failed transfer,
which a generic `err < 0` branch already does. To recover from a lost byte,
follow "With a timer" above.

**If you do nothing.** A sender that cancels with one CAN is no longer heard:
the spec says two, and common senders send more. A file cut short now ends in an
error rather than `YMODEM_DONE`. The limits under "Known issues" stay.

## How to test

```bash
cd wheels/protocols/ymodem/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ymodem
```

## Test status

- Unit tests (sim): 47 tests, all passing, including the loss of each byte of a
  batch in turn
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Passive byte-fed design; receiver-only; CRC-16 only (no
  checksum mode — YMODEM spec mandates CRC); on_file_start callback for
  filename notification; supports both SOH (128-byte) and STX (1024-byte)
  blocks; file size trimming on last block.

## Responsible human

@L-X-Yao
