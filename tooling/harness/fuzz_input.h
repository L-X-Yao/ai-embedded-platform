// SPDX-License-Identifier: MIT
// Byte reader for structured libFuzzer harnesses.
//
// A harness that builds frames from the fuzz input reads its control fields
// and payloads through this reader. Reads past the end return zero, so a
// truncated input still produces a complete, well-formed record.

#ifndef FUZZ_INPUT_H
#define FUZZ_INPUT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t         left;
} fuzz_input_t;

static inline void fuzz_input_init(fuzz_input_t *in, const uint8_t *data,
                                   size_t size)
{
    in->data = data;
    in->left = size;
}

static inline uint8_t fuzz_u8(fuzz_input_t *in)
{
    if (in->left == 0) return 0;
    in->left--;
    return *in->data++;
}

static inline uint16_t fuzz_u16(fuzz_input_t *in)
{
    uint16_t hi = fuzz_u8(in);
    return (uint16_t)((hi << 8) | fuzz_u8(in));
}

// Copy up to n bytes into out and return how many were available.
static inline size_t fuzz_take(fuzz_input_t *in, uint8_t *out, size_t n)
{
    if (n > in->left) n = in->left;
    if (n > 0) memcpy(out, in->data, n);
    in->data += n;
    in->left -= n;
    return n;
}

// Copy up to n bytes into out as a C string, without embedded NULs. out holds
// n + 1 bytes. Returns the string length.
static inline size_t fuzz_take_str(fuzz_input_t *in, char *out, size_t n)
{
    size_t len = fuzz_take(in, (uint8_t *)out, n);
    for (size_t i = 0; i < len; i++) {
        if (out[i] == 0) out[i] = 'a';
    }
    out[len] = 0;
    return len;
}

#endif // FUZZ_INPUT_H
