// SPDX-License-Identifier: MIT
/*
 * xmodem — XMODEM/XMODEM-CRC receiver (passive, byte-fed)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef XMODEM_H
#define XMODEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- protocol constants ---------- */

#define XMODEM_SOH  0x01
#define XMODEM_EOT  0x04
#define XMODEM_ACK  0x06
#define XMODEM_NAK  0x15
#define XMODEM_CAN  0x18
#define XMODEM_C    0x43  /* 'C' — CRC mode handshake */

#define XMODEM_BLOCK_SIZE  128

/* ---------- error codes ---------- */

typedef enum {
    XMODEM_OK           =  0,
    XMODEM_ERR_PARAM   = -1,
    XMODEM_ERR_TIMEOUT  = -2,
    XMODEM_ERR_CRC      = -3,
    XMODEM_ERR_SEQUENCE = -4,
    XMODEM_ERR_CANCEL   = -5,
    XMODEM_ERR_WRITE    = -6,
    XMODEM_DONE         =  1,
} xmodem_err_t;

/* ---------- state ---------- */

typedef enum {
    XMODEM_STATE_IDLE = 0,
    XMODEM_STATE_WAIT_HEADER,
    XMODEM_STATE_RECEIVING,
    XMODEM_STATE_COMPLETE,
    XMODEM_STATE_ERROR,
} xmodem_state_t;

/* ---------- IO callbacks (caller provides) ---------- */

typedef struct {
    /** Write received block to storage. Return XMODEM_OK on success. */
    xmodem_err_t (*write_block)(void *ctx, uint32_t offset,
                                const uint8_t *data, uint16_t len);
    /** Send one byte to serial (ACK/NAK/CAN/'C'). Return XMODEM_OK on success. */
    xmodem_err_t (*send_byte)(void *ctx, uint8_t byte);
    void *ctx;
} xmodem_io_t;

/* ---------- context ---------- */

typedef struct {
    const xmodem_io_t *io;
    xmodem_state_t     state;
    uint8_t            block_num;       /* expected block number (1-based, wraps) */
    uint32_t           offset;          /* write offset in bytes */
    uint8_t            buf[133];        /* SOH(1) + blk(1) + ~blk(1) + data(128) + crc(2) max */
    uint8_t            buf_pos;
    bool               use_crc;         /* true = XMODEM-CRC (CRC-16), false = checksum */
    uint32_t           total_bytes;
    uint8_t            can_count;       /* consecutive CAN bytes received */
    bool               line_idle_on;    /* caller will call xmodem_line_idle() */
    bool               purging;         /* frame sync lost, see xmodem_feed() */
    bool               eot_pending;     /* EOT seen, its proof still to come */
} xmodem_t;

/* ---------- API (all @thread_safe: caller_must_lock) ---------- */

/** Initialize receiver state. */
xmodem_err_t   xmodem_init(xmodem_t *xm, const xmodem_io_t *io, bool use_crc);

/** Promise to call xmodem_line_idle() whenever the line goes quiet. Call it
 *  between xmodem_init() and xmodem_start(); XMODEM_ERR_PARAM after that.
 *
 *  From then on a bad block or header gets no NAK at once. Every byte is
 *  dropped until xmodem_line_idle() reports a quiet line, and the NAK goes
 *  out then, as ymodem.txt 7.3.2 and 7.4 ask. EOT needs a quiet line too:
 *  xmodem_line_idle() returns XMODEM_DONE, and xmodem_feed() never does. */
xmodem_err_t   xmodem_enable_line_idle(xmodem_t *xm);

/** Send initial handshake: 'C' (CRC mode) or NAK (checksum mode). */
xmodem_err_t   xmodem_start(xmodem_t *xm);

/** Feed one received byte. Returns XMODEM_OK while receiving, XMODEM_DONE on
 *  successful completion, or negative error code.
 *
 *  After a bad block or a byte that cannot start one, the framing is lost:
 *  the rest of a resent block may be arriving. Without
 *  xmodem_enable_line_idle() the NAK still goes out at once and blocks are
 *  still taken, but EOT is refused until a block passes its checks, so 0x04
 *  in file data cannot end the transfer. When the next block number is 4, EOT
 *  also looks like block 4 with its SOH lost, so it is NAKed and the sender's
 *  repeat is taken. A stray 0x04 between clean blocks still reads as EOT:
 *  only xmodem_enable_line_idle() closes that. */
xmodem_err_t   xmodem_feed(xmodem_t *xm, uint8_t byte);

/** Report that nothing has arrived for the quiet period, one second in
 *  ymodem.txt 7.4. Call it every time that period passes, from the context
 *  that calls xmodem_feed(), which also has to count how many quiet periods
 *  in a row mean the sender has gone.
 *
 *  Drops a part frame and, if a block was lost, asks for it again with NAK,
 *  or with the handshake byte until a block has been taken. Returns XMODEM_OK,
 *  XMODEM_DONE if the quiet line proves an EOT, or the send_byte error.
 *  XMODEM_ERR_PARAM without xmodem_enable_line_idle() or outside a transfer. */
xmodem_err_t   xmodem_line_idle(xmodem_t *xm);

/** Get current state. */
xmodem_state_t xmodem_get_state(const xmodem_t *xm);

/** Get total bytes received, whole blocks only: XMODEM has no length field,
 *  so this includes any padding in the last block. */
uint32_t       xmodem_get_total_bytes(const xmodem_t *xm);

/** Send CAN (cancel) and move to error state. */
void           xmodem_cancel(xmodem_t *xm);

#ifdef __cplusplus
}
#endif

#endif /* XMODEM_H */
