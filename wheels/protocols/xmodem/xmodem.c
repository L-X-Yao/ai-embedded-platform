/*
 * xmodem — XMODEM/XMODEM-CRC receiver (passive, byte-fed)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "xmodem.h"
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

static uint16_t xmodem_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc = (uint16_t)(crc ^ ((uint16_t)data[i] << 8));
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static xmodem_err_t send_byte(const xmodem_t *xm, uint8_t byte)
{
    if (!xm->io || !xm->io->send_byte) return XMODEM_ERR_PARAMS;
    return xm->io->send_byte(xm->io->ctx, byte);
}

/* expected frame size: SOH(1) + blk(1) + ~blk(1) + data(128) + check(1 or 2) */
static uint8_t frame_size(const xmodem_t *xm)
{
    return xm->use_crc ? (uint8_t)133 : (uint8_t)132;
}

static xmodem_err_t process_block(xmodem_t *xm)
{
    uint8_t blk     = xm->buf[1];
    uint8_t blk_inv = xm->buf[2];
    const uint8_t *data = &xm->buf[3];

    /* block number complement check */
    if ((uint8_t)(blk + blk_inv) != 0xFF) {
        (void)send_byte(xm, XMODEM_NAK);
        return XMODEM_OK;  /* recoverable — sender will retry */
    }

    /* verify checksum or CRC */
    if (xm->use_crc) {
        uint16_t expected = (uint16_t)((uint16_t)xm->buf[131] << 8) | xm->buf[132];
        uint16_t actual   = xmodem_crc16(data, XMODEM_BLOCK_SIZE);
        if (actual != expected) {
            (void)send_byte(xm, XMODEM_NAK);
            return XMODEM_OK;
        }
    } else {
        uint8_t expected = xm->buf[131];
        uint8_t actual   = xmodem_checksum(data, XMODEM_BLOCK_SIZE);
        if (actual != expected) {
            (void)send_byte(xm, XMODEM_NAK);
            return XMODEM_OK;
        }
    }

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
    if (!xm || !io) return XMODEM_ERR_PARAMS;

    memset(xm, 0, sizeof(*xm));
    xm->io       = io;
    xm->use_crc  = use_crc;
    xm->state    = XMODEM_STATE_IDLE;
    xm->block_num = 1;  /* XMODEM blocks start at 1 */

    return XMODEM_OK;
}

xmodem_err_t xmodem_start(xmodem_t *xm)
{
    if (!xm) return XMODEM_ERR_PARAMS;
    if (xm->state != XMODEM_STATE_IDLE) return XMODEM_ERR_PARAMS;

    xm->state = XMODEM_STATE_WAIT_HEADER;

    /* send handshake: 'C' for CRC mode, NAK for checksum mode */
    return send_byte(xm, xm->use_crc ? XMODEM_C : XMODEM_NAK);
}

xmodem_err_t xmodem_feed(xmodem_t *xm, uint8_t byte)
{
    if (!xm) return XMODEM_ERR_PARAMS;

    if (xm->state != XMODEM_STATE_WAIT_HEADER &&
        xm->state != XMODEM_STATE_RECEIVING) {
        return XMODEM_ERR_PARAMS;
    }

    /* CAN detection */
    if (byte == XMODEM_CAN && xm->buf_pos == 0) {
        xm->state = XMODEM_STATE_ERROR;
        return XMODEM_ERR_CANCEL;
    }

    /* EOT — transfer complete */
    if (byte == XMODEM_EOT && xm->buf_pos == 0) {
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
