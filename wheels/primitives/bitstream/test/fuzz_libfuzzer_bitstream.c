// SPDX-License-Identifier: MIT
// LibFuzzer harness for bitstream -- coverage-guided fuzzing
//
// Treats the input as a sequence of operations on an exact-size buffer and checks
// every result against a plain bit-array model.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_bitstream.c ../bitstream.c -o fuzz_bitstream
// Run:     ./fuzz_bitstream -max_total_time=60 corpus_bitstream/

#include "../bitstream.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define CAP_MAX 64

typedef struct {
    uint8_t bits[CAP_MAX * 8];
    size_t  cap_bits;
    size_t  pos;
} model_t;

// Lengths past 32 are part of the input, the component has to refuse them.
static void do_write(bitstream_t *bs, model_t *m, uint32_t value, uint8_t nbits) {
    bitstream_err_t err = bitstream_write(bs, value, nbits);

    if (nbits == 0 || nbits > 32) {
        if (err != BITSTREAM_ERR_PARAM) __builtin_trap();
    } else if (m->pos + nbits > m->cap_bits) {
        if (err != BITSTREAM_ERR_OVERFLOW) __builtin_trap();
    } else {
        if (err != BITSTREAM_OK) __builtin_trap();
        for (uint8_t i = 0; i < nbits; i++) {
            m->bits[m->pos++] = (uint8_t)((value >> i) & 1u);
        }
    }
}

static void do_read(bitstream_t *bs, model_t *m, uint8_t nbits) {
    uint32_t value = 0;
    bitstream_err_t err = bitstream_read(bs, nbits, &value);

    if (nbits == 0 || nbits > 32) {
        if (err != BITSTREAM_ERR_PARAM) __builtin_trap();
    } else if (m->pos + nbits > m->cap_bits) {
        if (err != BITSTREAM_ERR_OVERFLOW) __builtin_trap();
    } else {
        uint32_t want = 0;
        for (uint8_t i = 0; i < nbits; i++) {
            want |= (uint32_t)m->bits[m->pos++] << i;
        }
        if (err != BITSTREAM_OK || value != want) __builtin_trap();
    }
}

static void do_seek(bitstream_t *bs, model_t *m, size_t pos) {
    bitstream_err_t err = bitstream_seek(bs, pos);

    if (pos > m->cap_bits) {
        if (err != BITSTREAM_ERR_OVERFLOW) __builtin_trap();
    } else {
        if (err != BITSTREAM_OK) __builtin_trap();
        m->pos = pos;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static model_t m;
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);

    size_t cap = fuzz_u8(&in) % (CAP_MAX + 1);
    uint8_t *buf = calloc(1, cap ? cap : 1);
    if (!buf) return 0;

    memset(&m, 0, sizeof(m));
    m.cap_bits = cap * 8;

    bitstream_t bs;
    if (bitstream_init(&bs, buf, cap) != BITSTREAM_OK) __builtin_trap();

    while (in.left > 0) {
        uint8_t op = fuzz_u8(&in);
        uint8_t nbits = (uint8_t)((op >> 2) % 40);

        switch (op & 3) {
        case 0: {
            uint32_t hi = fuzz_u16(&in);
            do_write(&bs, &m, (hi << 16) | fuzz_u16(&in), nbits);
            break;
        }
        case 1:
            do_read(&bs, &m, nbits);
            break;
        case 2:
            do_seek(&bs, &m, fuzz_u16(&in) % (CAP_MAX * 8 + 16));
            break;
        default:
            if (bitstream_reset(&bs) != BITSTREAM_OK) __builtin_trap();
            m.pos = 0;
            break;
        }
        if (bitstream_tell(&bs) != m.pos) __builtin_trap();
    }
    free(buf);

    return 0;
}
