// SPDX-License-Identifier: MIT
/*
 * modbus_tcp --- Modbus TCP/IP frame encoder/decoder (MBAP + PDU)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "modbus_tcp.h"
#include <string.h>

/* ---------- byte-order helpers (big-endian wire format) ---------- */

static void put_u16_be(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

static uint16_t get_u16_be(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] << 8 | (uint16_t)buf[1]);
}

/* ---------- internal: write MBAP header ---------- */

static void write_mbap(uint8_t *buf, uint16_t transaction_id,
                       uint16_t pdu_len, uint8_t unit_id)
{
    put_u16_be(&buf[0], transaction_id);
    put_u16_be(&buf[2], MODBUS_TCP_PROTOCOL_ID);
    put_u16_be(&buf[4], (uint16_t)(1 + pdu_len));  /* length = unit_id + PDU */
    buf[6] = unit_id;
}

/* ---------- build functions ---------- */

int modbus_tcp_build_read_regs(uint8_t *buf, uint16_t buf_size,
                               uint16_t transaction_id, uint8_t unit_id,
                               uint16_t start_addr, uint16_t count)
{
    if (!buf)
        return MODBUS_TCP_ERR_PARAM;
    if (count == 0 || count > MODBUS_TCP_MAX_READ_REGS)
        return MODBUS_TCP_ERR_PARAM;

    /* MBAP(7) + FC(1) + start(2) + count(2) = 12 */
    uint16_t frame_len = 12;
    if (buf_size < frame_len)
        return MODBUS_TCP_ERR_OVERFLOW;

    write_mbap(buf, transaction_id, 5, unit_id);    /* PDU = FC + 4 data = 5 */
    buf[7]  = MODBUS_TCP_FC_READ_HOLDING;
    put_u16_be(&buf[8],  start_addr);
    put_u16_be(&buf[10], count);

    return (int)frame_len;
}

int modbus_tcp_build_write_reg(uint8_t *buf, uint16_t buf_size,
                               uint16_t transaction_id, uint8_t unit_id,
                               uint16_t addr, uint16_t value)
{
    if (!buf)
        return MODBUS_TCP_ERR_PARAM;

    /* MBAP(7) + FC(1) + addr(2) + value(2) = 12 */
    uint16_t frame_len = 12;
    if (buf_size < frame_len)
        return MODBUS_TCP_ERR_OVERFLOW;

    write_mbap(buf, transaction_id, 5, unit_id);    /* PDU = FC + 4 data = 5 */
    buf[7]  = MODBUS_TCP_FC_WRITE_SINGLE;
    put_u16_be(&buf[8],  addr);
    put_u16_be(&buf[10], value);

    return (int)frame_len;
}

int modbus_tcp_build_write_regs(uint8_t *buf, uint16_t buf_size,
                                uint16_t transaction_id, uint8_t unit_id,
                                uint16_t start_addr, const uint16_t *values,
                                uint16_t count)
{
    if (!buf || !values)
        return MODBUS_TCP_ERR_PARAM;
    if (count == 0 || count > MODBUS_TCP_MAX_WRITE_REGS)
        return MODBUS_TCP_ERR_PARAM;

    /* MBAP(7) + FC(1) + start(2) + count(2) + byte_count(1) + data(count*2) */
    uint16_t pdu_len   = (uint16_t)(6 + count * 2);
    uint16_t frame_len = (uint16_t)(MODBUS_TCP_MBAP_SIZE + pdu_len);
    if (buf_size < frame_len)
        return MODBUS_TCP_ERR_OVERFLOW;

    write_mbap(buf, transaction_id, pdu_len, unit_id);
    buf[7] = MODBUS_TCP_FC_WRITE_MULTIPLE;
    put_u16_be(&buf[8],  start_addr);
    put_u16_be(&buf[10], count);
    buf[12] = (uint8_t)(count * 2);

    for (uint16_t i = 0; i < count; i++)
        put_u16_be(&buf[13 + i * 2], values[i]);

    return (int)frame_len;
}

/* ---------- parse ---------- */

modbus_tcp_err_t modbus_tcp_parse(const uint8_t *buf, uint16_t len,
                                  modbus_tcp_frame_t *frame)
{
    if (!buf || !frame)
        return MODBUS_TCP_ERR_PARAM;

    /* Need at least MBAP(7) + FC(1) = 8 bytes */
    if (len < MODBUS_TCP_MBAP_SIZE + 1)
        return MODBUS_TCP_ERR_SHORT;

    /* Parse MBAP header */
    frame->header.transaction_id = get_u16_be(&buf[0]);
    frame->header.protocol_id   = get_u16_be(&buf[2]);
    frame->header.length         = get_u16_be(&buf[4]);
    frame->header.unit_id        = buf[6];

    /* Validate protocol ID */
    if (frame->header.protocol_id != MODBUS_TCP_PROTOCOL_ID)
        return MODBUS_TCP_ERR_PROTOCOL;

    /* Validate length field: must be >= 2 (unit_id + at least FC) */
    if (frame->header.length < 2)
        return MODBUS_TCP_ERR_SHORT;

    /* Validate that buffer has enough data: MBAP(6) + length */
    uint16_t expected = (uint16_t)(6 + frame->header.length);
    if (len < expected)
        return MODBUS_TCP_ERR_SHORT;

    /* Extract PDU */
    frame->function_code = buf[7];
    uint16_t pdu_data_len = (uint16_t)(frame->header.length - 2);  /* minus unit_id and FC */
    frame->data     = (pdu_data_len > 0) ? &buf[8] : NULL;
    frame->data_len = pdu_data_len;

    return MODBUS_TCP_OK;
}

/* ---------- response helpers ---------- */

modbus_tcp_err_t modbus_tcp_get_regs(const modbus_tcp_frame_t *frame,
                                     uint16_t *values, uint16_t max_count,
                                     uint16_t *actual_count)
{
    if (!frame || !values || max_count == 0)
        return MODBUS_TCP_ERR_PARAM;
    if (!frame->data || frame->data_len < 1)
        return MODBUS_TCP_ERR_SHORT;

    /* Check for exception response */
    if (frame->function_code & 0x80)
        return MODBUS_TCP_ERR_EXCEPTION;

    uint8_t byte_count = frame->data[0];
    if (byte_count == 0 || byte_count % 2 != 0)
        return MODBUS_TCP_ERR_SHORT;
    if ((uint16_t)(byte_count + 1) > frame->data_len)
        return MODBUS_TCP_ERR_SHORT;

    uint16_t reg_count = (uint16_t)(byte_count / 2);
    if (reg_count > max_count)
        reg_count = max_count;

    for (uint16_t i = 0; i < reg_count; i++)
        values[i] = get_u16_be(&frame->data[1 + i * 2]);

    if (actual_count)
        *actual_count = reg_count;

    return MODBUS_TCP_OK;
}
