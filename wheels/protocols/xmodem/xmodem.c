// SPDX-License-Identifier: MIT
/*
 * xmodem — XMODEM/XMODEM-CRC receiver (passive, byte-fed)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      crc_any
 * Instances: multi
 */

#include "xmodem.h"
#include "crc_any.h"
#include "contract.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static uint8_t xmodem_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

static const crc_any_config_t xmodem_crc_cfg = {
    .width = 16, .poly = 0x1021, .init = 0x0000,
    .refin = false, .refout = false, .xorout = 0x0000,
};

static uint16_t xmodem_crc16(const uint8_t *data, uint16_t len)
{
    uint32_t crc = 0;
    crc_any_compute(&xmodem_crc_cfg, data, len, &crc);
    return (uint16_t)crc;
}

static xmodem_err_t send_byte(const xmodem_t *xm, uint8_t byte)
{
    if (!xm->io || !xm->io->send_byte) return XMODEM_ERR_PARAM;
    return xm->io->send_byte(xm->io->ctx, byte);
}

/* expected frame size: SOH(1) + blk(1) + ~blk(1) + data(128) + check(1 or 2) */
static uint8_t frame_size(const xmodem_t *xm)
{
    return xm->use_crc ? (uint8_t)133 : (uint8_t)132;
}

/* The framing can no longer be trusted: what follows may be the tail of a
 * resent block. Only a block that passes its checks, or a quiet line, ends
 * this. */
static void lose_sync(xmodem_t *xm)
{
    xm->buf_pos = 0;
    xm->purging = true;
}

/* recoverable — sender will retry */
static xmodem_err_t reject_block(xmodem_t *xm)
{
    lose_sync(xm);
    if (!xm->line_idle_on) {
        (void)send_byte(xm, XMODEM_NAK);
    }
    return XMODEM_OK;
}

/* A sender that has not started reads NAK as "use checksums", so CRC mode
 * keeps asking with 'C' until a block has been taken. A sender that has
 * started treats 'C' as a NAK of its first block. */
static uint8_t retry_byte(const xmodem_t *xm)
{
    return (xm->use_crc && xm->total_bytes == 0) ? (uint8_t)XMODEM_C
                                                 : (uint8_t)XMODEM_NAK;
}

static xmodem_err_t process_block(xmodem_t *xm)
{
    uint8_t blk     = xm->buf[1];
    uint8_t blk_inv = xm->buf[2];
    const uint8_t *data = &xm->buf[3];

    /* block number complement check */
    if ((uint8_t)(blk + blk_inv) != 0xFF) {
        return reject_block(xm);
    }

    /* verify checksum or CRC */
    if (xm->use_crc) {
        uint16_t expected = (uint16_t)(((uint16_t)xm->buf[131] << 8) | xm->buf[132]);
        uint16_t actual   = xmodem_crc16(data, XMODEM_BLOCK_SIZE);
        if (actual != expected) {
            return reject_block(xm);
        }
    } else {
        uint8_t expected = xm->buf[131];
        uint8_t actual   = xmodem_checksum(data, XMODEM_BLOCK_SIZE);
        if (actual != expected) {
            return reject_block(xm);
        }
    }

    /* a block that passes its checks means the framing is back */
    xm->purging = false;

    /* sequence check */
    if (blk != xm->block_num) {
        /* duplicate of previous block — re-ACK silently */
        if (blk == (uint8_t)(xm->block_num - 1)) {
            (void)send_byte(xm, XMODEM_ACK);
            return XMODEM_OK;
        }
        xm->state = XMODEM_STATE_ERROR;
        return XMODEM_ERR_SEQUENCE;
    }

    /* write data via callback */
    if (xm->io && xm->io->write_block) {
        xmodem_err_t err = xm->io->write_block(xm->io->ctx, xm->offset,
                                                 data, XMODEM_BLOCK_SIZE);
        if (err != XMODEM_OK) {
            xm->state = XMODEM_STATE_ERROR;
            return XMODEM_ERR_WRITE;
        }
    }

    xm->offset      += XMODEM_BLOCK_SIZE;
    xm->total_bytes  += XMODEM_BLOCK_SIZE;
    xm->block_num    = (uint8_t)(xm->block_num + 1);  /* wraps at 256 */

    (void)send_byte(xm, XMODEM_ACK);
    return XMODEM_OK;
}

/* ---------- public API ---------- */

xmodem_err_t xmodem_init(xmodem_t *xm, const xmodem_io_t *io, bool use_crc)
{
    if (!xm || !io) return XMODEM_ERR_PARAM;

    memset(xm, 0, sizeof(*xm));
    xm->io       = io;
    xm->use_crc  = use_crc;
    xm->state    = XMODEM_STATE_IDLE;
    xm->block_num = 1;  /* XMODEM blocks start at 1 */
    ENSURE(xm->buf_pos == 0 && xm->total_bytes == 0);

    return XMODEM_OK;
}

xmodem_err_t xmodem_enable_line_idle(xmodem_t *xm)
{
    if (!xm) return XMODEM_ERR_PARAM;
    if (xm->state != XMODEM_STATE_IDLE) return XMODEM_ERR_PARAM;

    xm->line_idle_on = true;
    return XMODEM_OK;
}

xmodem_err_t xmodem_start(xmodem_t *xm)
{
    if (!xm) return XMODEM_ERR_PARAM;
    if (xm->state != XMODEM_STATE_IDLE) return XMODEM_ERR_PARAM;

    xm->state = XMODEM_STATE_WAIT_HEADER;

    /* send handshake: 'C' for CRC mode, NAK for checksum mode */
    return send_byte(xm, xm->use_crc ? XMODEM_C : XMODEM_NAK);
}

xmodem_err_t xmodem_feed(xmodem_t *xm, uint8_t byte)
{
    if (!xm) return XMODEM_ERR_PARAM;

    if (xm->state != XMODEM_STATE_WAIT_HEADER &&
        xm->state != XMODEM_STATE_RECEIVING) {
        return XMODEM_ERR_PARAM;
    }

    /* the line is still clearing: nothing on it can be trusted */
    if (xm->purging && xm->line_idle_on) return XMODEM_OK;

    /* an EOT only counts if nothing but EOT follows it */
    if (byte != XMODEM_EOT) xm->eot_pending = false;

    /* CAN detection — spec requires 2 consecutive CAN to cancel */
    if (byte == XMODEM_CAN && xm->buf_pos == 0) {
        xm->can_count++;
        if (xm->can_count >= 2) {
            xm->state = XMODEM_STATE_ERROR;
            return XMODEM_ERR_CANCEL;
        }
        return XMODEM_OK;
    }
    xm->can_count = 0;

    /* EOT — transfer complete, unless the byte can be something else */
    if (byte == XMODEM_EOT && xm->buf_pos == 0) {
        if (xm->purging) return XMODEM_OK;  /* file data, not an EOT */

        if (xm->line_idle_on) {
            xm->eot_pending = true;         /* xmodem_line_idle() decides */
            return XMODEM_OK;
        }

        /* Block 4 with its SOH lost starts with this same byte. The sender
         * resends EOT until it gets an ACK (ymodem.txt 7.3.3). */
        if (xm->block_num == XMODEM_EOT && !xm->eot_pending) {
            xm->eot_pending = true;
            (void)send_byte(xm, XMODEM_NAK);
            return XMODEM_OK;
        }

        xm->eot_pending = false;
        (void)send_byte(xm, XMODEM_ACK);
        xm->state = XMODEM_STATE_COMPLETE;
        return XMODEM_DONE;
    }

    /* SOH — start of new block */
    if (byte == XMODEM_SOH && xm->buf_pos == 0) {
        xm->buf[0] = byte;
        xm->buf_pos = 1;
        xm->state = XMODEM_STATE_RECEIVING;
        return XMODEM_OK;
    }

    /* accumulate bytes into frame buffer */
    if (xm->buf_pos > 0) {
        if (xm->buf_pos < frame_size(xm)) {
            xm->buf[xm->buf_pos] = byte;
            xm->buf_pos++;
        }

        /* complete frame? */
        if (xm->buf_pos >= frame_size(xm)) {
            xm->buf_pos = 0;
            return process_block(xm);
        }
        return XMODEM_OK;
    }

    /* A byte that cannot start a block: its SOH was lost, or this is the
     * tail of one. Either way what follows is file data. */
    lose_sync(xm);
    return XMODEM_OK;
}

xmodem_err_t xmodem_line_idle(xmodem_t *xm)
{
    if (!xm || !xm->line_idle_on) return XMODEM_ERR_PARAM;

    if (xm->state != XMODEM_STATE_WAIT_HEADER &&
        xm->state != XMODEM_STATE_RECEIVING) {
        return XMODEM_ERR_PARAM;
    }

    /* two CANs with a quiet gap between them are not consecutive */
    xm->can_count = 0;

    if (xm->eot_pending) {
        xm->eot_pending = false;
        (void)send_byte(xm, XMODEM_ACK);
        xm->state = XMODEM_STATE_COMPLETE;
        return XMODEM_DONE;
    }

    if (xm->purging || xm->buf_pos > 0) {
        xm->purging = false;
        xm->buf_pos = 0;
        return send_byte(xm, retry_byte(xm));
    }

    return XMODEM_OK;
}

xmodem_state_t xmodem_get_state(const xmodem_t *xm)
{
    if (!xm) return XMODEM_STATE_IDLE;
    return xm->state;
}

uint32_t xmodem_get_total_bytes(const xmodem_t *xm)
{
    if (!xm) return 0;
    return xm->total_bytes;
}

void xmodem_cancel(xmodem_t *xm)
{
    if (!xm) return;
    (void)send_byte(xm, XMODEM_CAN);
    (void)send_byte(xm, XMODEM_CAN);
    xm->state = XMODEM_STATE_ERROR;
}
