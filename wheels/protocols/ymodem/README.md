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

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
/* Error codes */
typedef enum {
    YMODEM_OK, YMODEM_ERR_PARAMS, YMODEM_ERR_CRC, YMODEM_ERR_SEQ,
    YMODEM_ERR_CANCEL, YMODEM_ERR_WRITE, YMODEM_DONE,
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
ymodem_err_t   ymodem_start(ymodem_t *ym);          /* sends 'C' */
ymodem_err_t   ymodem_feed(ymodem_t *ym, uint8_t byte);
ymodem_state_t ymodem_get_state(const ymodem_t *ym);
const char    *ymodem_get_filename(const ymodem_t *ym);
uint32_t       ymodem_get_file_size(const ymodem_t *ym);
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

## Configuration (compile-time)

No compile-time defines. SOH blocks are 128 bytes, STX blocks are 1024 bytes
per YMODEM spec.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

- Receiver-only. Sender mode is not implemented.
- Timeout detection is the caller's responsibility (the wheel is passive).
- Filename field is capped at 63 characters.

## How to test

```bash
cd wheels/protocols/ymodem/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ymodem
```

## Test status

- Unit tests (sim): 20 tests, all passing
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

@user
