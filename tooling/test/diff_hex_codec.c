/*
 * diff_hex_codec.c -- differential test driver for hex_codec wheel
 *
 * Default (encode): reads raw binary from stdin, encodes with our
 * hex_codec wheel, prints lowercase hex string to stdout (no newline).
 *
 * With -d flag (decode): reads hex string from stdin, decodes with
 * our wheel, writes raw binary to stdout.
 *
 * Build (from repo root):
 *   gcc -O2 -I wheels/primitives/hex_codec -I tooling/test \
 *       tooling/test/diff_hex_codec.c wheels/primitives/hex_codec/hex_codec.c \
 *       -o diff_hex_codec
 */

#include "hex_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t read_stdin(uint8_t **out)
{
    size_t cap = 4096, len = 0;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) { fprintf(stderr, "malloc failed\n"); exit(2); }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, stdin)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            buf = (uint8_t *)realloc(buf, cap);
            if (!buf) { fprintf(stderr, "realloc failed\n"); exit(2); }
        }
    }
    *out = buf;
    return len;
}

static int do_encode(void)
{
    uint8_t *data = NULL;
    size_t data_len = read_stdin(&data);

    /* hex_codec uses uint16_t lengths */
    if (data_len > 32767u) {
        fprintf(stderr, "input too large for uint16_t hex_codec API\n");
        free(data);
        return 2;
    }

    uint16_t out_size = (uint16_t)(data_len * 2u + 1u);
    char *out = (char *)malloc((size_t)out_size);
    if (!out) { free(data); fprintf(stderr, "malloc failed\n"); return 2; }

    hex_err_t err = hex_encode(data, (uint16_t)data_len, out, out_size);
    if (err != HEX_OK) {
        fprintf(stderr, "hex_encode failed: %d\n", (int)err);
        free(data); free(out);
        return 1;
    }

    /* Write hex string without trailing newline */
    size_t hex_len = data_len * 2u;
    fwrite(out, 1, hex_len, stdout);

    free(data);
    free(out);
    return 0;
}

static int do_decode(void)
{
    uint8_t *raw = NULL;
    size_t raw_len = read_stdin(&raw);

    /* Strip trailing whitespace/newline */
    while (raw_len > 0 && (raw[raw_len - 1] == '\n' || raw[raw_len - 1] == '\r'
                        || raw[raw_len - 1] == ' ')) {
        raw_len--;
    }

    if (raw_len > 65535u) {
        fprintf(stderr, "input too large for uint16_t hex_codec API\n");
        free(raw);
        return 2;
    }

    uint16_t out_max = (uint16_t)(raw_len / 2u);
    uint8_t *out = (uint8_t *)malloc((size_t)out_max + 1u);
    if (!out) { free(raw); fprintf(stderr, "malloc failed\n"); return 2; }

    uint16_t out_len = (uint16_t)(out_max + 1u);
    hex_err_t err = hex_decode((const char *)raw, (uint16_t)raw_len,
                               out, &out_len);
    if (err != HEX_OK) {
        fprintf(stderr, "hex_decode failed: %d\n", (int)err);
        free(raw); free(out);
        return 1;
    }

    fwrite(out, 1, (size_t)out_len, stdout);

    free(raw);
    free(out);
    return 0;
}

int main(int argc, char *argv[])
{
    int decode_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) { decode_mode = 1; }
    }

    return decode_mode ? do_decode() : do_encode();
}
