// SPDX-License-Identifier: MIT
/*
 * ymodem — YMODEM batch file transfer receiver (passive, byte-fed)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef YMODEM_H
#define YMODEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- protocol constants ---------- */

#define YMODEM_SOH  0x01   /* 128-byte data block */
#define YMODEM_STX  0x02   /* 1024-byte data block */
#define YMODEM_EOT  0x04
#define YMODEM_ACK  0x06
#define YMODEM_NAK  0x15
#define YMODEM_CAN  0x18
#define YMODEM_C    0x43   /* 'C' — CRC mode handshake */

#define YMODEM_BLOCK_SIZE_SOH   128
#define YMODEM_BLOCK_SIZE_STX   1024

/* ---------- error codes ---------- */

typedef enum {
    YMODEM_OK           =  0,
    YMODEM_ERR_PARAM   = -1,
    YMODEM_ERR_CRC      = -2,
    YMODEM_ERR_SEQ      = -3,
    YMODEM_ERR_CANCEL   = -4,
    YMODEM_ERR_WRITE    = -5,
    YMODEM_ERR_OVERFLOW = -6,   /* block 0 name does not fit filename[] */
    YMODEM_DONE         =  1,
} ymodem_err_t;

/* ---------- state ---------- */

typedef enum {
    YMODEM_STATE_IDLE = 0,
    YMODEM_STATE_HEADER,
    YMODEM_STATE_DATA,
    YMODEM_STATE_COMPLETE,
    YMODEM_STATE_ERROR,
} ymodem_state_t;

/* ---------- IO callbacks (caller provides) ---------- */

typedef struct {
    /** Write received data block to storage. Return YMODEM_OK on success. */
    ymodem_err_t (*write_block)(void *ctx, uint32_t offset,
                                const uint8_t *data, uint16_t len);
    /** Send one byte to serial (ACK/NAK/CAN/'C'). Return YMODEM_OK on success. */
    ymodem_err_t (*send_byte)(void *ctx, uint8_t byte);
    /** Called when block 0 header is parsed. May be NULL. */
    void (*on_file_start)(void *ctx, const char *filename, uint32_t file_size);
    void *ctx;
} ymodem_io_t;

/* ---------- context ---------- */

typedef struct {
    const ymodem_io_t *io;
    ymodem_state_t     state;
    uint8_t            block_num;        /* expected block number */
    uint32_t           offset;           /* write offset in bytes */
    uint8_t            buf[1029];        /* STX(1) + blk(1) + ~blk(1) + data(1024) + crc(2) */
    uint16_t           buf_pos;
    uint16_t           block_size;       /* 128 or 1024 for current frame */
    char               filename[64];
    uint32_t           file_size;
    uint32_t           total_received;
    bool               eot_seen;         /* first EOT received, awaiting second */
    uint8_t            can_count;        /* consecutive CAN bytes received */
    bool               line_idle_on;     /* caller will call ymodem_line_idle() */
    bool               purging;          /* frame sync lost, see ymodem_feed() */
} ymodem_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialize receiver state. */
ymodem_err_t   ymodem_init(ymodem_t *ym, const ymodem_io_t *io);

/** Promise to call ymodem_line_idle() whenever the line goes quiet. Call it
 *  between ymodem_init() and ymodem_start(); YMODEM_ERR_PARAM after that.
 *
 *  From then on a bad block or header gets no NAK at once. Every byte is
 *  dropped until ymodem_line_idle() reports a quiet line, and the NAK goes
 *  out then, as ymodem.txt 7.3.2 and 7.4 ask. */
ymodem_err_t   ymodem_enable_line_idle(ymodem_t *ym);

/** Send initial 'C' handshake to request CRC-mode transfer. */
ymodem_err_t   ymodem_start(ymodem_t *ym);

/** Feed one received byte. Returns YMODEM_OK while receiving, YMODEM_DONE on
 *  successful completion, or negative error code.
 *
 *  After a bad block or a byte that cannot start one, the framing is lost:
 *  the rest of a resent block may be arriving. Without
 *  ymodem_enable_line_idle() the NAK still goes out at once and blocks are
 *  still taken, but EOT is refused until a block passes its checks. A resent
 *  block can then lock onto a 0x01 or 0x02 in its own data on every try, so
 *  one lost byte usually costs the transfer: the sender gives up. */
ymodem_err_t   ymodem_feed(ymodem_t *ym, uint8_t byte);

/** Report that nothing has arrived for the quiet period, one second in
 *  ymodem.txt 7.4. Call it every time that period passes, from the context
 *  that calls ymodem_feed(), which also has to count how many quiet periods
 *  in a row mean the sender has gone.
 *
 *  Drops a part frame and, if a block was lost, asks for it again: 'C' for
 *  a block 0, NAK for a data block. Returns YMODEM_OK or the send_byte error.
 *  YMODEM_ERR_PARAM without ymodem_enable_line_idle() or outside a transfer. */
ymodem_err_t   ymodem_line_idle(ymodem_t *ym);

/** Get current state. */
ymodem_state_t ymodem_get_state(const ymodem_t *ym);

/** Get filename from block 0 header (empty string if not yet received). */
const char    *ymodem_get_filename(const ymodem_t *ym);

/** Get file size from block 0 header (0 if not yet received). */
uint32_t       ymodem_get_file_size(const ymodem_t *ym);

/** Send CAN (cancel) and move to error state. */
void           ymodem_cancel(ymodem_t *ym);

#ifdef __cplusplus
}
#endif

#endif /* YMODEM_H */
