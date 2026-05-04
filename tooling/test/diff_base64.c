/*
 * diff_base64.c -- differential test driver for base64 wheel
 *
 * Reads raw binary from stdin, encodes with our base64 wheel, prints
 * the Base64 string to stdout (no trailing newline).
 *
 * With -d flag: reads Base64 text from stdin, decodes with our wheel,
 * writes raw binary to stdout.
 *
 * Build (from repo root):
 *   gcc -O2 -I wheels/primitives/base64 -I tooling/test \
 *       tooling/test/diff_base64.c wheels/primitives/base64/base64.c \
 *       -o diff_base64
 */

#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read all of stdin into a malloc'd buffer. Returns bytes read. */
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

    /* uint16_t range check: base64 wheel uses uint16_t lengths */
    if (data_len > 65535u) {
        fprintf(stderr, "input too large for uint16_t base64 API\n");
        free(data);
        return 2;
    }

    uint16_t enc_size = base64_encoded_size((uint16_t)data_len);
    char *out = (char *)malloc((size_t)enc_size + 1u);
    if (!out) { free(data); fprintf(stderr, "malloc failed\n"); return 2; }

    uint16_t out_len = (uint16_t)(enc_size + 1u);
    base64_err_t err = base64_encode(data, (uint16_t)data_len, out, &out_len);
    if (err != BASE64_OK) {
        fprintf(stderr, "base64_encode failed: %d\n", (int)err);
        free(data); free(out);
        return 1;
    }

    /* Write encoded string without newline */
    fwrite(out, 1, (size_t)out_len, stdout);

    free(data);
    free(out);
    return 0;
}

static int do_decode(void)
{
    uint8_t *raw = NULL;
    size_t raw_len = read_stdin(&raw);

    /* Strip trailing whitespace/newline that shell might append */
    while (raw_len > 0 && (raw[raw_len - 1] == '\n' || raw[raw_len - 1] == '\r'
                        || raw[raw_len - 1] == ' ')) {
        raw_len--;
    }

    if (raw_len > 65535u) {
        fprintf(stderr, "input too large for uint16_t base64 API\n");
        free(raw);
        return 2;
    }

    uint16_t dec_max = base64_decoded_size((uint16_t)raw_len);
    /* Add extra room: decoded_size may over-estimate by up to 2 */
    uint8_t *out = (uint8_t *)malloc((size_t)dec_max + 4u);
    if (!out) { free(raw); fprintf(stderr, "malloc failed\n"); return 2; }

    uint16_t out_len = (uint16_t)(dec_max + 4u);
    base64_err_t err = base64_decode((const char *)raw, (uint16_t)raw_len,
                                     out, &out_len);
    if (err != BASE64_OK) {
        fprintf(stderr, "base64_decode failed: %d\n", (int)err);
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
