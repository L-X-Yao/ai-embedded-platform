// SPDX-License-Identifier: MIT
// LibFuzzer harness for bencode — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_bencode.c ../bencode.c -o fuzz_bencode
// Run:     ./fuzz_bencode -max_total_time=60 corpus_bencode/

#include "../bencode.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 64
#define STRING_MAX 300

typedef struct {
    bencode_token_type_t type;
    int64_t              integer;
    uint8_t              bytes[STRING_MAX];
    size_t               len;
} sent_token_t;

static int64_t fuzz_integer(fuzz_input_t *in) {
    uint64_t value = 0;
    uint8_t op = fuzz_u8(in);
    for (int i = 0; i < 4; i++) value = (value << 16) | fuzz_u16(in);
    value >>= op % 64;
    return (int64_t)((op & 0x80) ? 0 - value : value);
}

// Open and close containers in order, never past the decoder's nesting limit.
static bencode_err_t encode_one(bencode_encoder_t *enc, sent_token_t *s,
                                uint8_t *stack, int *depth, fuzz_input_t *in) {
    uint8_t kind = fuzz_u8(in) % 5;

    if (kind == 2 && *depth == BENCODE_MAX_DEPTH) kind = 0;
    if (kind == 3 && *depth == BENCODE_MAX_DEPTH) kind = 1;
    if (kind == 4 && *depth == 0) kind = 0;

    switch (kind) {
    case 0:
        s->type = BENCODE_TOKEN_INT;
        s->integer = fuzz_integer(in);
        return bencode_encode_int(enc, s->integer);
    case 1:
        s->type = BENCODE_TOKEN_STRING;
        s->len = fuzz_u16(in) % (STRING_MAX + 1);
        memset(s->bytes, 'x', s->len);
        fuzz_take(in, s->bytes, s->len);
        return bencode_encode_string(enc, s->len > 0 ? s->bytes : NULL, s->len);
    case 2:
        s->type = BENCODE_TOKEN_LIST_BEGIN;
        stack[(*depth)++] = 'l';
        return bencode_encode_list_begin(enc);
    case 3:
        s->type = BENCODE_TOKEN_DICT_BEGIN;
        stack[(*depth)++] = 'd';
        return bencode_encode_dict_begin(enc);
    default:
        if (stack[--(*depth)] == 'd') {
            s->type = BENCODE_TOKEN_DICT_END;
            return bencode_encode_dict_end(enc);
        }
        s->type = BENCODE_TOKEN_LIST_END;
        return bencode_encode_list_end(enc);
    }
}

static void check_token(const sent_token_t *s, const bencode_token_t *got) {
    if (got->type != s->type) __builtin_trap();
    if (s->type == BENCODE_TOKEN_INT && got->value.integer != s->integer) __builtin_trap();
    if (s->type == BENCODE_TOKEN_STRING &&
        (got->value.string.len != s->len ||
         (s->len > 0 && memcmp(got->value.string.data, s->bytes, s->len) != 0))) {
        __builtin_trap();
    }
}

// Encode tokens into an exact-size buffer until it fills, then decode them back.
static void round_trip(fuzz_input_t *in) {
    static sent_token_t sent[MAX_TOKENS];
    uint8_t stack[BENCODE_MAX_DEPTH];
    size_t capacity = fuzz_u16(in) % 1024;
    uint8_t *buf = malloc(capacity ? capacity : 1);
    if (!buf) return;

    bencode_encoder_t enc;
    if (bencode_encoder_init(&enc, buf, capacity) != BENCODE_OK) __builtin_trap();

    int count = 0;
    int depth = 0;
    int good_depth = 0;
    size_t good = 0;
    while (count < MAX_TOKENS && in->left > 0) {
        bencode_err_t err = encode_one(&enc, &sent[count], stack, &depth, in);
        if (bencode_encoder_len(&enc) > capacity) __builtin_trap();
        if (err != BENCODE_OK) break;
        good = bencode_encoder_len(&enc);
        good_depth = depth;
        count++;
    }

    bencode_decoder_t dec;
    if (bencode_decoder_init(&dec, buf, good) != BENCODE_OK) __builtin_trap();
    for (int i = 0; i < count; i++) {
        bencode_token_t got;
        if (bencode_decode_next(&dec, &got) != BENCODE_OK) __builtin_trap();
        check_token(&sent[i], &got);
    }
    // Encoding can stop with containers open: that input is cut short.
    bencode_token_t last;
    bencode_err_t want = good_depth == 0 ? BENCODE_ERR_END : BENCODE_ERR_MALFORMED;
    if (bencode_decode_next(&dec, &last) != want) __builtin_trap();
    free(buf);
}

// Drain tokens until error or exhaustion. Each end token must close the container
// opened last, and only nesting past the limit may overflow.
static void drain(bencode_decoder_t *dec) {
    bencode_token_type_t open[BENCODE_MAX_DEPTH];
    int depth = 0;
    bencode_token_t token;

    for (int i = 0; i < 4096; i++) {
        bencode_err_t err = bencode_decode_next(dec, &token);
        if (err != BENCODE_OK) {
            if (err == BENCODE_ERR_OVERFLOW && depth != BENCODE_MAX_DEPTH) __builtin_trap();
            return;
        }
        if (token.type == BENCODE_TOKEN_LIST_BEGIN || token.type == BENCODE_TOKEN_DICT_BEGIN) {
            if (depth == BENCODE_MAX_DEPTH) __builtin_trap();
            open[depth++] = token.type;
        } else if (token.type == BENCODE_TOKEN_LIST_END || token.type == BENCODE_TOKEN_DICT_END) {
            if (depth == 0) __builtin_trap();
            depth--;
            if ((open[depth] == BENCODE_TOKEN_DICT_BEGIN) !=
                (token.type == BENCODE_TOKEN_DICT_END)) {
                __builtin_trap();
            }
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    bencode_decoder_t dec;
    bencode_decoder_init(&dec, data, size);
    drain(&dec);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        round_trip(&in);
    }

    return 0;
}
