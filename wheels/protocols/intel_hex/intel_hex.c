// SPDX-License-Identifier: MIT
/*
 * intel_hex — Intel HEX firmware format parser
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "intel_hex.h"
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

intel_hex_err_t intel_hex_decode(const char *line, size_t len,
                                 intel_hex_record_t *out)
{
    if (!line || !out)
        return INTEL_HEX_ERR_PARAM;

    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
        len--;

    if (len < 11)
        return INTEL_HEX_ERR_SHORT;
    if (line[0] != ':')
        return INTEL_HEX_ERR_FORMAT;

    int byte_count = hex_byte(&line[1]);
    if (byte_count < 0)
        return INTEL_HEX_ERR_FORMAT;

    /* ':', then count, address, type, data and checksum as hex pairs. */
    size_t expected = 1 + (size_t)(4 + byte_count + 1) * 2;
    if (len < expected)
        return INTEL_HEX_ERR_SHORT;

    int addr_hi = hex_byte(&line[3]);
    int addr_lo = hex_byte(&line[5]);
    int rec_type = hex_byte(&line[7]);
    if (addr_hi < 0 || addr_lo < 0 || rec_type < 0)
        return INTEL_HEX_ERR_FORMAT;
    if (rec_type > INTEL_HEX_START_LINEAR)
        return INTEL_HEX_ERR_FORMAT;

    if (byte_count > INTEL_HEX_MAX_DATA)
        return INTEL_HEX_ERR_OVERFLOW;
    if (len > expected)
        return INTEL_HEX_ERR_FORMAT;

    uint8_t checksum = (uint8_t)((uint8_t)byte_count + (uint8_t)addr_hi +
                                (uint8_t)addr_lo + (uint8_t)rec_type);

    for (int i = 0; i < byte_count; i++) {
        int b = hex_byte(&line[9 + i * 2]);
        if (b < 0)
            return INTEL_HEX_ERR_FORMAT;
        out->data[i] = (uint8_t)b;
        checksum += (uint8_t)b;
    }

    int chk = hex_byte(&line[9 + byte_count * 2]);
    if (chk < 0)
        return INTEL_HEX_ERR_FORMAT;
    checksum += (uint8_t)chk;
    if (checksum != 0)
        return INTEL_HEX_ERR_CHECKSUM;

    out->type = (intel_hex_type_t)rec_type;
    out->address = (uint16_t)((uint16_t)addr_hi << 8 | (uint16_t)addr_lo);
    out->data_len = (uint8_t)byte_count;

    return INTEL_HEX_OK;
}

intel_hex_err_t intel_hex_encode(const intel_hex_record_t *rec,
                                 char *buf, size_t cap, size_t *out_len)
{
    if (!rec || !buf || !out_len)
        return INTEL_HEX_ERR_PARAM;
    if ((uint32_t)rec->type > (uint32_t)INTEL_HEX_START_LINEAR)
        return INTEL_HEX_ERR_PARAM;
    if (rec->data_len > INTEL_HEX_MAX_DATA)
        return INTEL_HEX_ERR_OVERFLOW;

    size_t total = 1 + (4 + (size_t)rec->data_len + 1) * 2 + 2;
    if (total > cap)
        return INTEL_HEX_ERR_OVERFLOW;

    size_t pos = 0;
    buf[pos++] = ':';

    uint8_t checksum = 0;

    byte_to_hex(rec->data_len, &buf[pos]); pos += 2;
    checksum += rec->data_len;

    uint8_t addr_hi = (uint8_t)(rec->address >> 8);
    uint8_t addr_lo = (uint8_t)(rec->address & 0xFF);
    byte_to_hex(addr_hi, &buf[pos]); pos += 2;
    byte_to_hex(addr_lo, &buf[pos]); pos += 2;
    checksum += addr_hi;
    checksum += addr_lo;

    byte_to_hex((uint8_t)rec->type, &buf[pos]); pos += 2;
    checksum += (uint8_t)rec->type;

    for (uint8_t i = 0; i < rec->data_len; i++) {
        byte_to_hex(rec->data[i], &buf[pos]); pos += 2;
        checksum += rec->data[i];
    }

    checksum = (uint8_t)(-(int8_t)checksum);
    byte_to_hex(checksum, &buf[pos]); pos += 2;

    buf[pos++] = '\r';
    buf[pos++] = '\n';
    *out_len = pos;
    return INTEL_HEX_OK;
}
