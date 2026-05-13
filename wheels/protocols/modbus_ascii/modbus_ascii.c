// SPDX-License-Identifier: MIT
/*
 * modbus_ascii — Modbus ASCII frame encode/decode
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "modbus_ascii.h"
#include <string.h>

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int hex_byte(const char *s)
{
    int hi = hex_nibble(s[0]);
    int lo = hex_nibble(s[1]);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
}

static void byte_to_hex(uint8_t b, char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[b >> 4];
    out[1] = hex[b & 0x0F];
}

uint8_t modbus_ascii_lrc(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    if (!data)
        return 0;
    for (size_t i = 0; i < len; i++)
        sum += data[i];
    return (uint8_t)(-(int8_t)sum);
}

modbus_ascii_err_t modbus_ascii_decode(const char *ascii, size_t len,
                                        modbus_ascii_frame_t *out)
{
    if (!ascii || !out)
        return MODBUS_ASCII_ERR_PARAM;
    /* Minimum: ':' + addr(2) + func(2) + lrc(2) + CR + LF = 9 */
    if (len < 9)
        return MODBUS_ASCII_ERR_SHORT;
    if (ascii[0] != ':')
        return MODBUS_ASCII_ERR_FORMAT;
    if (ascii[len - 2] != '\r' || ascii[len - 1] != '\n')
        return MODBUS_ASCII_ERR_FORMAT;

    size_t hex_len = len - 3;
    if (hex_len % 2 != 0)
        return MODBUS_ASCII_ERR_FORMAT;

    size_t nbytes = hex_len / 2;
    if (nbytes < 3)
        return MODBUS_ASCII_ERR_SHORT;

    uint8_t binary[256];
    if (nbytes > sizeof(binary))
        return MODBUS_ASCII_ERR_OVERFLOW;

    for (size_t i = 0; i < nbytes; i++) {
        int b = hex_byte(&ascii[1 + i * 2]);
        if (b < 0)
            return MODBUS_ASCII_ERR_FORMAT;
        binary[i] = (uint8_t)b;
    }

    uint8_t lrc = modbus_ascii_lrc(binary, nbytes - 1);
    if (lrc != binary[nbytes - 1])
        return MODBUS_ASCII_ERR_LRC;

    out->addr = binary[0];
    out->func = binary[1];
    out->data_len = (uint8_t)(nbytes - 3);
    if (out->data_len > MODBUS_ASCII_MAX_DATA)
        return MODBUS_ASCII_ERR_OVERFLOW;
    if (out->data_len > 0)
        memcpy(out->data, &binary[2], out->data_len);

    return MODBUS_ASCII_OK;
}

modbus_ascii_err_t modbus_ascii_encode(const modbus_ascii_frame_t *frame,
                                        char *buf, size_t cap, size_t *out_len)
{
    if (!frame || !buf || !out_len)
        return MODBUS_ASCII_ERR_PARAM;
    if (frame->data_len > MODBUS_ASCII_MAX_DATA)
        return MODBUS_ASCII_ERR_OVERFLOW;

    size_t total = 1 + 2 + 2 + (size_t)frame->data_len * 2 + 2 + 2;
    if (total > cap)
        return MODBUS_ASCII_ERR_OVERFLOW;

    size_t pos = 0;
    buf[pos++] = ':';
    byte_to_hex(frame->addr, &buf[pos]); pos += 2;
    byte_to_hex(frame->func, &buf[pos]); pos += 2;
    for (uint8_t i = 0; i < frame->data_len; i++) {
        byte_to_hex(frame->data[i], &buf[pos]);
        pos += 2;
    }

    uint8_t binary_for_lrc[256];
    binary_for_lrc[0] = frame->addr;
    binary_for_lrc[1] = frame->func;
    if (frame->data_len > 0)
        memcpy(&binary_for_lrc[2], frame->data, frame->data_len);
    uint8_t lrc = modbus_ascii_lrc(binary_for_lrc, (size_t)frame->data_len + 2);
    byte_to_hex(lrc, &buf[pos]); pos += 2;

    buf[pos++] = '\r';
    buf[pos++] = '\n';

    *out_len = pos;
    return MODBUS_ASCII_OK;
}
