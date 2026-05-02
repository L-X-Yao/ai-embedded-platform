// SPDX-License-Identifier: MIT
/*
 * uuid — UUID v4 generation and parsing for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "uuid.h"
#include <string.h>

const uuid_t UUID_NIL = { .bytes = {0} };

/* ---- internal helpers -------------------------------------------------- */

static const char hex_lut[] = "0123456789abcdef";

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* ---- public API -------------------------------------------------------- */

uuid_err_t uuid_v4_generate(uuid_t *uuid, uuid_rng_t rng, void *ctx)
{
    if (!uuid || !rng) {
        return UUID_ERR_PARAM;
    }

    rng(ctx, uuid->bytes, 16);

    /* Version 4: byte[6] high nibble = 0100 */
    uuid->bytes[6] = (uint8_t)(0x40u | (uuid->bytes[6] & 0x0Fu));

    /* Variant RFC 4122: byte[8] high 2 bits = 10 */
    uuid->bytes[8] = (uint8_t)(0x80u | (uuid->bytes[8] & 0x3Fu));

    return UUID_OK;
}

uuid_err_t uuid_to_string(const uuid_t *uuid, char *buf, uint16_t buf_size)
{
    if (!uuid || !buf) {
        return UUID_ERR_PARAM;
    }
    if (buf_size < 37) {
        return UUID_ERR_PARAM;
    }

    /*
     * Format: 8-4-4-4-12 hex with hyphens.
     * Byte groups: [0..3]-[4..5]-[6..7]-[8..9]-[10..15]
     */
    static const uint8_t group_sizes[] = { 4, 2, 2, 2, 6 };
    uint16_t pos = 0;
    uint16_t byte_idx = 0;

    for (uint8_t g = 0; g < 5; g++) {
        if (g > 0) {
            buf[pos++] = '-';
        }
        for (uint8_t i = 0; i < group_sizes[g]; i++) {
            buf[pos++] = hex_lut[(uuid->bytes[byte_idx] >> 4) & 0x0F];
            buf[pos++] = hex_lut[uuid->bytes[byte_idx] & 0x0F];
            byte_idx++;
        }
    }
    buf[pos] = '\0';

    return UUID_OK;
}

uuid_err_t uuid_from_string(uuid_t *uuid, const char *str)
{
    if (!uuid || !str) {
        return UUID_ERR_PARAM;
    }

    /* Validate length == 36 */
    size_t len = strlen(str);
    if (len != 36) {
        return UUID_ERR_FORMAT;
    }

    /* Validate hyphens at positions 8, 13, 18, 23 */
    if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
        return UUID_ERR_FORMAT;
    }

    /* Parse hex pairs */
    uint8_t byte_idx = 0;
    for (size_t i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue; /* skip hyphens */
        }

        int hi = hex_val(str[i]);
        int lo = hex_val(str[i + 1]);
        if (hi < 0 || lo < 0) {
            return UUID_ERR_FORMAT;
        }
        uuid->bytes[byte_idx++] = (uint8_t)((hi << 4) | lo);
        i++; /* consumed two chars */
    }

    return UUID_OK;
}

bool uuid_equal(const uuid_t *a, const uuid_t *b)
{
    if (!a || !b) {
        return false;
    }
    return memcmp(a->bytes, b->bytes, 16) == 0;
}

bool uuid_is_nil(const uuid_t *uuid)
{
    if (!uuid) {
        return false;
    }
    return memcmp(uuid->bytes, UUID_NIL.bytes, 16) == 0;
}
