// SPDX-License-Identifier: MIT
/*
 * diff_crc16.c -- differential test driver for crc16 wheel
 *
 * Reads raw binary from stdin, computes CRC16-MODBUS with our wheel,
 * prints the result as a lowercase 4-digit hex string to stdout
 * (no trailing newline).
 *
 * Build (from repo root):
 *   gcc -O2 -I wheels/primitives/crc16 -I tooling/harness \
 *       tooling/harness/diff_crc16.c wheels/primitives/crc16/crc16.c \
 *       -o diff_crc16
 */

#include "crc16.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    size_t cap = 4096, len = 0;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) { fprintf(stderr, "malloc failed\n"); return 2; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, stdin)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            buf = (uint8_t *)realloc(buf, cap);
            if (!buf) { fprintf(stderr, "realloc failed\n"); return 2; }
        }
    }

    uint16_t crc = crc16_modbus(buf, len);
    printf("%04x", (unsigned)crc);

    free(buf);
    return 0;
}
