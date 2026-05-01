/*
 * ymodem — YMODEM batch file transfer receiver (passive, byte-fed)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "ymodem.h"
#include <string.h>

/* ---------- internal helpers ---------- */

static uint16_t ymodem_crc16(const uint8_t *data, uint16_t len)
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

static ymodem_err_t send_byte(const ymodem_t *ym, uint8_t byte)
{
    if (!ym->io || !ym->io->send_byte) return YMODEM_ERR_PARAMS;
    return ym->io->send_byte(ym->io->ctx, byte);
}

/* expected frame size: header(1) + blk(1) + ~blk(1) + data(block_size) + crc(2) */
static uint16_t frame_size(const ymodem_t *ym)
{
    return (uint16_t)(3 + ym->block_size + 2);
}

static void parse_block0(ymodem_t *ym, const uint8_t *data, uint16_t len)
{
    /* block 0 payload: "filename\0size\0..." or all zeros for end of batch */
    uint16_t i = 0;
    uint16_t fname_max = (uint16_t)(sizeof(ym->filename) - 1);

    /* copy filename */
    while (i < len && i < fname_max && data[i] != '\0') {
        ym->filename[i] = (char)data[i];
        i++;
    }
    ym->filename[i] = '\0';

    /* parse file size (decimal ASCII after the null terminator) */
    ym->file_size = 0;
    if (i < len) {
        i++; /* skip null terminator after filename */
        while (i < len && data[i] >= '0' && data[i] <= '9') {
            ym->file_size = ym->file_size * 10 + (uint32_t)(data[i] - '0');
            i++;
        }
    }
}

static ymodem_err_t process_block(ymodem_t *ym)
{
    uint8_t blk     = ym->buf[1];
    uint8_t blk_inv = ym->buf[2];
    const uint8_t *data = &ym->buf[3];

    /* block number complement check */
    if ((uint8_t)(blk + blk_inv) != 0xFF) {
        (void)send_byte(ym, YMODEM_NAK);
        return YMODEM_OK;  /* recoverable */
    }

    /* verify CRC-16 (YMODEM always uses CRC mode) */
    uint16_t crc_offset = (uint16_t)(3 + ym->block_size);
    uint16_t expected = (uint16_t)((uint16_t)ym->buf[crc_offset] << 8) |
                        ym->buf[crc_offset + 1];
    uint16_t actual = ymodem_crc16(data, ym->block_size);
    if (actual != expected) {
        (void)send_byte(ym, YMODEM_NAK);
        return YMODEM_OK;  /* recoverable */
    }

    /* sequence check */
    if (blk != ym->block_num) {
        /* duplicate of previous block — re-ACK silently */
        if (blk == (uint8_t)(ym->block_num - 1)) {
            (void)send_byte(ym, YMODEM_ACK);
            return YMODEM_OK;
        }
        ym->state = YMODEM_STATE_ERROR;
        return YMODEM_ERR_SEQ;
    }

    /* block 0 = file header */
    if (ym->state == YMODEM_STATE_HEADER && ym->block_num == 0) {
        /* empty filename = end of batch */
        if (data[0] == '\0') {
            (void)send_byte(ym, YMODEM_ACK);
            ym->state = YMODEM_STATE_COMPLETE;
            return YMODEM_DONE;
        }

        parse_block0(ym, data, ym->block_size);

        if (ym->io && ym->io->on_file_start) {
            ym->io->on_file_start(ym->io->ctx, ym->filename, ym->file_size);
        }

        ym->block_num = 1;
        ym->offset = 0;
        ym->total_received = 0;
        ym->state = YMODEM_STATE_DATA;

        /* ACK block 0 + send 'C' to start data transfer */
        (void)send_byte(ym, YMODEM_ACK);
        (void)send_byte(ym, YMODEM_C);
        return YMODEM_OK;
    }

    /* data block — write via callback */
    uint16_t write_len = ym->block_size;

    /* trim padding if we know file size and this is the last data */
    if (ym->file_size > 0) {
        uint32_t remaining = ym->file_size - ym->total_received;
        if (remaining < write_len) {
            write_len = (uint16_t)remaining;
        }
    }

    if (ym->io && ym->io->write_block) {
        ymodem_err_t err = ym->io->write_block(ym->io->ctx, ym->offset,
                                                data, write_len);
        if (err != YMODEM_OK) {
            ym->state = YMODEM_STATE_ERROR;
            return YMODEM_ERR_WRITE;
        }
    }

    ym->offset         += write_len;
    ym->total_received += write_len;
    ym->block_num       = (uint8_t)(ym->block_num + 1);  /* wraps at 256 */

    (void)send_byte(ym, YMODEM_ACK);
    return YMODEM_OK;
}

/* ---------- public API ---------- */

ymodem_err_t ymodem_init(ymodem_t *ym, const ymodem_io_t *io)
{
    if (!ym || !io) return YMODEM_ERR_PARAMS;

    memset(ym, 0, sizeof(*ym));
    ym->io    = io;
    ym->state = YMODEM_STATE_IDLE;
    /* YMODEM block 0 is the header — block numbering starts at 0 */

    return YMODEM_OK;
}

ymodem_err_t ymodem_start(ymodem_t *ym)
{
    if (!ym) return YMODEM_ERR_PARAMS;
    if (ym->state != YMODEM_STATE_IDLE) return YMODEM_ERR_PARAMS;

    ym->state = YMODEM_STATE_HEADER;

    /* YMODEM always uses CRC mode — send 'C' */
    return send_byte(ym, YMODEM_C);
}

ymodem_err_t ymodem_feed(ymodem_t *ym, uint8_t byte)
{
    if (!ym) return YMODEM_ERR_PARAMS;

    if (ym->state != YMODEM_STATE_HEADER &&
        ym->state != YMODEM_STATE_DATA) {
        return YMODEM_ERR_PARAMS;
    }

    /* CAN detection — only at frame boundary */
    if (byte == YMODEM_CAN && ym->buf_pos == 0) {
        ym->state = YMODEM_STATE_ERROR;
        return YMODEM_ERR_CANCEL;
    }

    /* EOT handling: first EOT -> NAK, second EOT -> ACK + 'C' for next file */
    if (byte == YMODEM_EOT && ym->buf_pos == 0) {
        if (!ym->eot_seen) {
            /* first EOT — NAK per YMODEM spec */
            ym->eot_seen = true;
            (void)send_byte(ym, YMODEM_NAK);
            return YMODEM_OK;
        }
        /* second EOT — ACK + send 'C' to request next file header */
        ym->eot_seen = false;
        (void)send_byte(ym, YMODEM_ACK);
        (void)send_byte(ym, YMODEM_C);
        ym->state = YMODEM_STATE_HEADER;
        ym->block_num = 0;
        return YMODEM_OK;
    }

    /* SOH or STX — start of new block */
    if (ym->buf_pos == 0) {
        if (byte == YMODEM_SOH) {
            ym->block_size = YMODEM_BLOCK_SIZE_SOH;
            ym->buf[0] = byte;
            ym->buf_pos = 1;
            return YMODEM_OK;
        }
        if (byte == YMODEM_STX) {
            ym->block_size = YMODEM_BLOCK_SIZE_STX;
            ym->buf[0] = byte;
            ym->buf_pos = 1;
            return YMODEM_OK;
        }
        /* ignore other bytes at frame boundary */
        return YMODEM_OK;
    }

    /* accumulate bytes into frame buffer */
    if (ym->buf_pos < frame_size(ym)) {
        ym->buf[ym->buf_pos] = byte;
        ym->buf_pos++;
    }

    /* complete frame? */
    if (ym->buf_pos >= frame_size(ym)) {
        ym->buf_pos = 0;
        return process_block(ym);
    }

    return YMODEM_OK;
}

ymodem_state_t ymodem_get_state(const ymodem_t *ym)
{
    if (!ym) return YMODEM_STATE_IDLE;
    return ym->state;
}

const char *ymodem_get_filename(const ymodem_t *ym)
{
    if (!ym) return "";
    return ym->filename;
}

uint32_t ymodem_get_file_size(const ymodem_t *ym)
{
    if (!ym) return 0;
    return ym->file_size;
}

void ymodem_cancel(ymodem_t *ym)
{
    if (!ym) return;
    (void)send_byte(ym, YMODEM_CAN);
    (void)send_byte(ym, YMODEM_CAN);
    ym->state = YMODEM_STATE_ERROR;
}
