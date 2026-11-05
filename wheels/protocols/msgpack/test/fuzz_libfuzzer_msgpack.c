// SPDX-License-Identifier: MIT
// LibFuzzer harness for msgpack — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_msgpack.c ../msgpack.c -o fuzz_msgpack
// Run:     ./fuzz_msgpack -max_total_time=60 corpus_msgpack/

#include "../msgpack.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 16
#define POOL_SIZE 70000   // Long enough for a string with a four-byte length

typedef struct {
    uint8_t  kind;
    uint32_t number;
    int32_t  integer;
    size_t   len;
} sent_item_t;

static uint8_t pool[POOL_SIZE];

static msgpack_err_t encode_one(msgpack_enc_t *enc, sent_item_t *s, fuzz_input_t *in) {
    // A random shift reaches every width: fixint, 1, 2 and 4 bytes.
    uint32_t hi = fuzz_u16(in);
    uint8_t op = fuzz_u8(in);
    s->number = ((hi << 16) | fuzz_u16(in)) >> (op % 32);
    s->integer = (op & 0x80) ? (int32_t)(0u - s->number) : (int32_t)s->number;
    s->len = s->number % POOL_SIZE;
    const uint8_t *bytes = s->len > 0 ? pool : NULL;

    switch (s->kind) {
    case 0: return msgpack_enc_nil(enc);
    case 1: return msgpack_enc_bool(enc, (s->number & 1) != 0);
    case 2: return msgpack_enc_uint(enc, s->number);
    case 3: return msgpack_enc_int(enc, s->integer);
    case 4: {
        float value;
        memcpy(&value, &s->number, sizeof(value));
        return msgpack_enc_float(enc, value);
    }
    case 5: return msgpack_enc_str(enc, (const char *)bytes, s->len);
    case 6: return msgpack_enc_bin(enc, bytes, s->len);
    case 7: return msgpack_enc_array(enc, s->number);
    default: return msgpack_enc_map(enc, s->number);
    }
}

static void check_token(const sent_item_t *s, const msgpack_token_t *got) {
    uint32_t bits;
    switch (s->kind) {
    case 0:
        if (got->type != MSGPACK_TYPE_NIL) __builtin_trap();
        break;
    case 1:
        if (got->type != MSGPACK_TYPE_BOOL || got->val.boolean != ((s->number & 1) != 0)) {
            __builtin_trap();
        }
        break;
    case 2:
        if (got->type != MSGPACK_TYPE_UINT || got->val.u32 != s->number) __builtin_trap();
        break;
    case 3:
        // Zero and positive values use the unsigned forms.
        if (s->integer >= 0) {
            if (got->type != MSGPACK_TYPE_UINT || got->val.u32 != (uint32_t)s->integer) {
                __builtin_trap();
            }
        } else if (got->type != MSGPACK_TYPE_INT || got->val.i32 != s->integer) {
            __builtin_trap();
        }
        break;
    case 4:
        memcpy(&bits, &got->val.f32, sizeof(bits));
        if (got->type != MSGPACK_TYPE_FLOAT || bits != s->number) __builtin_trap();
        break;
    case 5:
        if (got->type != MSGPACK_TYPE_STR || got->val.str.len != s->len ||
            (s->len > 0 && memcmp(got->val.str.ptr, pool, s->len) != 0)) {
            __builtin_trap();
        }
        break;
    case 6:
        if (got->type != MSGPACK_TYPE_BIN || got->val.bin.len != s->len ||
            (s->len > 0 && memcmp(got->val.bin.ptr, pool, s->len) != 0)) {
            __builtin_trap();
        }
        break;
    case 7:
        if (got->type != MSGPACK_TYPE_ARRAY || got->val.array_count != s->number) {
            __builtin_trap();
        }
        break;
    default:
        if (got->type != MSGPACK_TYPE_MAP || got->val.map_count != s->number) __builtin_trap();
        break;
    }
}

// Encode items into an exact-size buffer until it fills, then decode them back.
static void round_trip(fuzz_input_t *in) {
    static sent_item_t sent[MAX_ITEMS];
    uint8_t op = fuzz_u8(in);
    size_t capacity = fuzz_u16(in) % 512;
    if ((op & 0x0F) == 0) capacity += POOL_SIZE;

    uint8_t *buf = malloc(capacity ? capacity : 1);
    if (!buf) return;

    msgpack_enc_t enc;
    if (msgpack_enc_init(&enc, buf, capacity) != MSGPACK_OK) __builtin_trap();

    int count = 0;
    while (count < MAX_ITEMS && in->left > 0) {
        sent_item_t *s = &sent[count];
        size_t before = msgpack_enc_len(&enc);
        s->kind = fuzz_u8(in) % 9;

        msgpack_err_t err = encode_one(&enc, s, in);
        if (msgpack_enc_len(&enc) > capacity) __builtin_trap();
        if (err != MSGPACK_OK) {
            if (msgpack_enc_len(&enc) != before) __builtin_trap();
            break;
        }
        count++;
    }

    msgpack_dec_t dec;
    if (msgpack_dec_init(&dec, buf, msgpack_enc_len(&enc)) != MSGPACK_OK) __builtin_trap();
    for (int i = 0; i < count; i++) {
        msgpack_token_t got;
        if (msgpack_dec_next(&dec, &got) != MSGPACK_OK) __builtin_trap();
        check_token(&sent[i], &got);
    }
    free(buf);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    msgpack_dec_t dec;
    msgpack_dec_init(&dec, data, size);

    // Drain all tokens until error or exhaustion
    msgpack_token_t tok;
    for (int i = 0; i < 4096; i++) {
        msgpack_err_t err = msgpack_dec_next(&dec, &tok);
        if (err != MSGPACK_OK) break;
    }

    for (size_t i = 0; pool[1] == 0 && i < POOL_SIZE; i++) {
        pool[i] = (uint8_t)(i * 31 + 7);
    }

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        round_trip(&in);
    }

    return 0;
}
