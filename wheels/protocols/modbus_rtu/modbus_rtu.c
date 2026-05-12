// SPDX-License-Identifier: MIT
/*
 * modbus_rtu — Modbus RTU frame codec with master/slave helpers
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      crc16
 * Instances: multi
 */

#include "modbus_rtu.h"
#include "contract.h"
#include "crc16.h"
#include <string.h>

/* ---------- byte-order helpers ---------- */

static void put_u16_be(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

static uint16_t get_u16_be(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

/* ---------- frame layer ---------- */

bool modbus_rtu_check_crc(const uint8_t *frame, uint16_t len)
{
    if (!frame || len < MODBUS_RTU_MIN_FRAME)
        return false;
    uint16_t crc = crc16_modbus(frame, len - 2);
    return frame[len - 2] == (uint8_t)(crc & 0xFF) &&
           frame[len - 1] == (uint8_t)(crc >> 8);
}

int modbus_rtu_parse(const uint8_t *buf, uint16_t len,
                     modbus_rtu_frame_t *frame)
{
    if (!buf || !frame || len < MODBUS_RTU_MIN_FRAME)
        return -1;
    if (!modbus_rtu_check_crc(buf, len))
        return -1;
    frame->addr     = buf[0];
    frame->func     = buf[1];
    frame->data     = (len > MODBUS_RTU_MIN_FRAME) ? &buf[2] : NULL;
    frame->data_len = len - MODBUS_RTU_MIN_FRAME;
    ENSURE(frame->data_len <= len);
    return 0;
}

int modbus_rtu_build(uint8_t *buf, uint16_t buf_size,
                     uint8_t addr, uint8_t func,
                     const uint8_t *data, uint16_t data_len)
{
    if (!buf || data_len > MODBUS_RTU_MAX_FRAME - 4 || (data_len > 0 && !data))
        return -1;
    uint16_t frame_len = 2 + data_len + 2;
    if (buf_size < frame_len)
        return -1;
    buf[0] = addr;
    buf[1] = func;
    if (data_len > 0 && data)
        memcpy(&buf[2], data, data_len);
    uint16_t crc = crc16_modbus(buf, 2 + data_len);
    buf[2 + data_len]     = (uint8_t)(crc & 0xFF);
    buf[2 + data_len + 1] = (uint8_t)(crc >> 8);
    ENSURE(frame_len <= buf_size);
    return (int)frame_len;
}

/* ---------- internal ---------- */

static int build_exception(uint8_t *buf, uint16_t buf_size,
                           uint8_t addr, uint8_t func, uint8_t ex_code)
{
    uint8_t data = ex_code;
    return modbus_rtu_build(buf, buf_size, addr, func | 0x80, &data, 1);
}

/* ---------- master ---------- */

#if MODBUS_RTU_MASTER

int modbus_rtu_master_read_regs(uint8_t *buf, uint16_t buf_size,
                                uint8_t slave, uint8_t func,
                                uint16_t start, uint16_t count)
{
    if (!buf) return -1;
    if (count == 0 || count > MODBUS_MAX_READ_REGS)
        return -1;
    if (func != MODBUS_FC_READ_HOLDING_REGS && func != MODBUS_FC_READ_INPUT_REGS)
        return -1;
    uint8_t data[4];
    put_u16_be(&data[0], start);
    put_u16_be(&data[2], count);
    return modbus_rtu_build(buf, buf_size, slave, func, data, 4);
}

int modbus_rtu_master_write_reg(uint8_t *buf, uint16_t buf_size,
                                uint8_t slave,
                                uint16_t reg, uint16_t value)
{
    if (!buf) return -1;
    uint8_t data[4];
    put_u16_be(&data[0], reg);
    put_u16_be(&data[2], value);
    return modbus_rtu_build(buf, buf_size, slave, MODBUS_FC_WRITE_SINGLE_REG,
                            data, 4);
}

int modbus_rtu_master_write_regs(uint8_t *buf, uint16_t buf_size,
                                 uint8_t slave,
                                 uint16_t start, uint16_t count,
                                 const uint16_t *values)
{
    if (!values || count == 0 || count > MODBUS_MAX_WRITE_REGS)
        return -1;
    uint8_t data[5 + MODBUS_MAX_WRITE_REGS * 2];
    put_u16_be(&data[0], start);
    put_u16_be(&data[2], count);
    data[4] = (uint8_t)(count * 2);
    for (uint16_t i = 0; i < count; i++)
        put_u16_be(&data[5 + i * 2], values[i]);
    return modbus_rtu_build(buf, buf_size, slave, MODBUS_FC_WRITE_MULTIPLE_REGS,
                            data, (uint16_t)(5 + count * 2));
}

int modbus_rtu_master_get_regs(const modbus_rtu_frame_t *frame,
                               uint16_t *regs, uint16_t max_count)
{
    if (!frame || !regs || !frame->data || frame->data_len < 1)
        return -1;
    uint8_t byte_count = frame->data[0];
    if (byte_count == 0 || byte_count > 250 || byte_count % 2 != 0
        || (uint16_t)(byte_count + 1) > frame->data_len)
        return -1;
    uint16_t reg_count = byte_count / 2;
    if (reg_count > max_count)
        reg_count = max_count;
    for (uint16_t i = 0; i < reg_count; i++)
        regs[i] = get_u16_be(&frame->data[1 + i * 2]);
    return (int)reg_count;
}

#endif /* MODBUS_RTU_MASTER */

/* ---------- slave ---------- */

#if MODBUS_RTU_SLAVE

void modbus_rtu_slave_init(modbus_rtu_slave_t *s, uint8_t addr, void *ctx)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->addr = addr;
    s->ctx  = ctx;
}

static int slave_read_regs(modbus_rtu_slave_t *s,
                           const modbus_rtu_frame_t *req,
                           uint8_t *resp, uint16_t resp_size)
{
    if (req->data_len < 4)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_DATA_VALUE);

    uint16_t start = get_u16_be(&req->data[0]);
    uint16_t count = get_u16_be(&req->data[2]);

    if (count == 0 || count > MODBUS_MAX_READ_REGS
        || (uint32_t)start + count > 0x10000u)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_DATA_VALUE);

    modbus_rtu_reg_read_t cb = (req->func == MODBUS_FC_READ_HOLDING_REGS)
                                ? s->read_holding : s->read_input;
    if (!cb)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_FUNCTION);

    uint8_t data[1 + MODBUS_MAX_READ_REGS * 2];
    data[0] = (uint8_t)(count * 2);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t val;
        if (cb(s->ctx, start + i, &val) != 0)
            return build_exception(resp, resp_size, s->addr, req->func,
                                   MODBUS_EX_SLAVE_FAILURE);
        put_u16_be(&data[1 + i * 2], val);
    }

    return modbus_rtu_build(resp, resp_size, s->addr, req->func,
                            data, (uint16_t)(1 + count * 2));
}

static int slave_write_single(modbus_rtu_slave_t *s,
                              const modbus_rtu_frame_t *req,
                              uint8_t *resp, uint16_t resp_size)
{
    if (req->data_len < 4)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_DATA_VALUE);

    if (!s->write_holding)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_FUNCTION);

    uint16_t reg   = get_u16_be(&req->data[0]);
    uint16_t value = get_u16_be(&req->data[2]);

    if (s->write_holding(s->ctx, reg, value) != 0)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_SLAVE_FAILURE);

    return modbus_rtu_build(resp, resp_size, s->addr, req->func,
                            req->data, 4);
}

static int slave_write_multiple(modbus_rtu_slave_t *s,
                                const modbus_rtu_frame_t *req,
                                uint8_t *resp, uint16_t resp_size)
{
    if (req->data_len < 5)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_DATA_VALUE);

    if (!s->write_holding)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_FUNCTION);

    uint16_t start      = get_u16_be(&req->data[0]);
    uint16_t count      = get_u16_be(&req->data[2]);
    uint8_t  byte_count = req->data[4];

    if (count == 0 || count > MODBUS_MAX_WRITE_REGS ||
        (uint32_t)start + count > 0x10000u ||
        byte_count != count * 2 ||
        req->data_len < 5 + byte_count)
        return build_exception(resp, resp_size, s->addr, req->func,
                               MODBUS_EX_ILLEGAL_DATA_VALUE);

    for (uint16_t i = 0; i < count; i++) {
        uint16_t val = get_u16_be(&req->data[5 + i * 2]);
        if (s->write_holding(s->ctx, start + i, val) != 0)
            return build_exception(resp, resp_size, s->addr, req->func,
                                   MODBUS_EX_SLAVE_FAILURE);
    }

    uint8_t data[4];
    put_u16_be(&data[0], start);
    put_u16_be(&data[2], count);
    return modbus_rtu_build(resp, resp_size, s->addr, req->func, data, 4);
}

int modbus_rtu_slave_process(modbus_rtu_slave_t *s,
                             const uint8_t *req, uint16_t req_len,
                             uint8_t *resp, uint16_t resp_size)
{
    if (!s || !req || !resp)
        return -1;

    modbus_rtu_frame_t frame;
    if (modbus_rtu_parse(req, req_len, &frame) != 0)
        return -1;

    if (frame.addr != s->addr && frame.addr != MODBUS_RTU_ADDR_BROADCAST)
        return 0;

    bool broadcast = (frame.addr == MODBUS_RTU_ADDR_BROADCAST);

    int result;
    switch (frame.func) {
    case MODBUS_FC_READ_HOLDING_REGS:
    case MODBUS_FC_READ_INPUT_REGS:
        if (broadcast)
            return 0;
        result = slave_read_regs(s, &frame, resp, resp_size);
        break;
    case MODBUS_FC_WRITE_SINGLE_REG:
        result = slave_write_single(s, &frame, resp, resp_size);
        break;
    case MODBUS_FC_WRITE_MULTIPLE_REGS:
        result = slave_write_multiple(s, &frame, resp, resp_size);
        break;
    default:
        if (broadcast)
            return 0;
        result = build_exception(resp, resp_size, s->addr, frame.func,
                                 MODBUS_EX_ILLEGAL_FUNCTION);
        break;
    }

    if (broadcast)
        return 0;

    return result;
}

#endif /* MODBUS_RTU_SLAVE */
