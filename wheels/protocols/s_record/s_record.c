// SPDX-License-Identifier: MIT
/*
 * s_record — Motorola S-Record (SREC) firmware format parser
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "s_record.h"
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
    static const char h[] = "0123456789ABCDEF";
    out[0] = h[b >> 4];
    out[1] = h[b & 0x0F];
}

static int addr_bytes_for_type(int type)
{
    switch (type) {
    case 0: case 1: case 5: case 9: return 2;
    case 2: case 8: return 3;
    case 3: case 7: return 4;
    default: return -1;
    }
}

s_record_err_t s_record_decode(const char *line, size_t len,
                                s_record_line_t *out)
{
    if (!line || !out)
        return S_RECORD_ERR_PARAM;

    /* Strip trailing CR/LF */
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
        len--;

    if (len < 4)
        return S_RECORD_ERR_SHORT;
    if (line[0] != 'S')
        return S_RECORD_ERR_FORMAT;

    int type_digit = hex_nibble(line[1]);
    if (type_digit < 0 || type_digit > 9)
        return S_RECORD_ERR_FORMAT;

    int ab = addr_bytes_for_type(type_digit);
    if (ab < 0)
        return S_RECORD_ERR_FORMAT;

    /* Parse byte count */
    int byte_count = hex_byte(&line[2]);
    if (byte_count < 0)
        return S_RECORD_ERR_FORMAT;

    /* Verify line has enough hex chars: 'S' + type + (byte_count+1)*2 */
    size_t expected_chars = 4 + (size_t)byte_count * 2;
    if (len < expected_chars)
        return S_RECORD_ERR_SHORT;

    /* Parse all binary bytes (including address, data, checksum) */
    uint8_t binary[256];
    if (byte_count > 255)
        return S_RECORD_ERR_OVERFLOW;

    uint8_t checksum = (uint8_t)byte_count;
    for (int i = 0; i < byte_count; i++) {
        int b = hex_byte(&line[4 + i * 2]);
        if (b < 0)
            return S_RECORD_ERR_FORMAT;
        binary[i] = (uint8_t)b;
        if (i < byte_count - 1)
            checksum += (uint8_t)b;
    }

    /* Verify checksum (one's complement) */
    checksum = (uint8_t)~checksum;
    if (checksum != binary[byte_count - 1])
        return S_RECORD_ERR_CHECKSUM;

    /* Extract address */
    uint32_t addr = 0;
    for (int i = 0; i < ab; i++)
        addr = (addr << 8) | binary[i];

    /* Extract data */
    int data_len = byte_count - ab - 1;
    if (data_len < 0)
        data_len = 0;
    if (data_len > S_RECORD_MAX_DATA)
        return S_RECORD_ERR_OVERFLOW;

    out->type = (s_record_type_t)type_digit;
    out->address = addr;
    out->data_len = (uint8_t)data_len;
    if (data_len > 0)
        memcpy(out->data, &binary[ab], (size_t)data_len);

    return S_RECORD_OK;
}

s_record_err_t s_record_encode(const s_record_line_t *rec,
                                char *buf, size_t cap, size_t *out_len)
{
    if (!rec || !buf || !out_len)
        return S_RECORD_ERR_PARAM;
    if (rec->data_len > S_RECORD_MAX_DATA)
        return S_RECORD_ERR_OVERFLOW;

    int ab = addr_bytes_for_type((int)rec->type);
    if (ab < 0)
        return S_RECORD_ERR_FORMAT;

    int byte_count = ab + (int)rec->data_len + 1;
    size_t total = 4 + (size_t)byte_count * 2 + 2;
    if (total > cap)
        return S_RECORD_ERR_OVERFLOW;

    size_t pos = 0;
    buf[pos++] = 'S';
    buf[pos++] = (char)('0' + (int)rec->type);
    byte_to_hex((uint8_t)byte_count, &buf[pos]); pos += 2;

    uint8_t checksum = (uint8_t)byte_count;

    /* Address bytes (MSB first) */
    for (int i = ab - 1; i >= 0; i--) {
        uint8_t b = (uint8_t)(rec->address >> (i * 8));
        byte_to_hex(b, &buf[pos]); pos += 2;
        checksum += b;
    }

    /* Data bytes */
    for (uint8_t i = 0; i < rec->data_len; i++) {
        byte_to_hex(rec->data[i], &buf[pos]); pos += 2;
        checksum += rec->data[i];
    }

    /* Checksum (one's complement) */
    checksum = (uint8_t)~checksum;
    byte_to_hex(checksum, &buf[pos]); pos += 2;

    buf[pos++] = '\r';
    buf[pos++] = '\n';
    *out_len = pos;
    return S_RECORD_OK;
}
