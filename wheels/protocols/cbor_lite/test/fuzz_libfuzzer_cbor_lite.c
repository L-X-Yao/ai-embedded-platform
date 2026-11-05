// SPDX-License-Identifier: MIT
// LibFuzzer harness for cbor_lite — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_cbor_lite.c ../cbor_lite.c -o fuzz_cbor_lite
// Run:     ./fuzz_cbor_lite -max_total_time=60 corpus_cbor_lite/

#include "../cbor_lite.h"
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
    uint64_t number;
    size_t   len;
} sent_item_t;

static uint8_t pool[POOL_SIZE];

// A random shift reaches every head size: inline, 1, 2, 4 and 8 bytes.
static uint64_t fuzz_number(fuzz_input_t *in) {
    uint64_t value = 0;
    for (int i = 0; i < 4; i++) value = (value << 16) | fuzz_u16(in);
    return value >> (fuzz_u8(in) % 64);
}

static cbor_lite_err_t encode_one(cbor_lite_encoder_t *enc, sent_item_t *s, fuzz_input_t *in) {
    s->number = fuzz_number(in);
    s->len = (size_t)(s->number % POOL_SIZE);
    const uint8_t *bytes = s->len > 0 ? pool : NULL;

    switch (s->kind) {
    case 0: return cbor_lite_enc_uint(enc, s->number);
    case 1: return cbor_lite_enc_nint(enc, s->number);
    case 2: return cbor_lite_enc_bstr(enc, bytes, s->len);
    case 3: return cbor_lite_enc_tstr(enc, (const char *)bytes, s->len);
    case 4: return cbor_lite_enc_array(enc, (size_t)s->number);
    case 5: return cbor_lite_enc_map(enc, (size_t)s->number);
    case 6: return cbor_lite_enc_bool(enc, (s->number & 1) != 0);
    case 7: return cbor_lite_enc_null(enc);
    default: {
        float value;
        uint32_t bits = (uint32_t)s->number;
        memcpy(&value, &bits, sizeof(value));
        return cbor_lite_enc_float(enc, value);
    }
    }
}

static void check_item(const sent_item_t *s, const cbor_lite_item_t *got) {
    uint32_t bits;
    switch (s->kind) {
    case 0:
        if (got->major != CBOR_MAJOR_UINT || got->uval != s->number) __builtin_trap();
        break;
    case 1:
        if (got->major != CBOR_MAJOR_NINT || got->uval != s->number) __builtin_trap();
        break;
    case 2:
    case 3:
        if (got->major != (s->kind == 2 ? CBOR_MAJOR_BSTR : CBOR_MAJOR_TSTR) ||
            got->str_len != s->len || (s->len > 0 && memcmp(got->str, pool, s->len) != 0)) {
            __builtin_trap();
        }
        break;
    case 4:
        if (got->major != CBOR_MAJOR_ARRAY || got->uval != s->number) __builtin_trap();
        break;
    case 5:
        if (got->major != CBOR_MAJOR_MAP || got->uval != s->number) __builtin_trap();
        break;
    case 6:
        if (got->major != CBOR_MAJOR_SIMPLE || got->bool_val != ((s->number & 1) != 0) ||
            got->simple != ((s->number & 1) ? 21 : 20)) {
            __builtin_trap();
        }
        break;
    case 7:
        if (got->major != CBOR_MAJOR_SIMPLE || got->simple != 22) __builtin_trap();
        break;
    default:
        memcpy(&bits, &got->float_val, sizeof(bits));
        if (got->major != CBOR_MAJOR_SIMPLE || got->simple != 26 ||
            bits != (uint32_t)s->number) {
            __builtin_trap();
        }
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

    cbor_lite_encoder_t enc;
    cbor_lite_enc_init(&enc, buf, capacity);

    int count = 0;
    size_t good = 0;
    while (count < MAX_ITEMS && in->left > 0) {
        sent_item_t *s = &sent[count];
        s->kind = fuzz_u8(in) % 9;

        cbor_lite_err_t err = encode_one(&enc, s, in);
        if (cbor_lite_enc_len(&enc) > capacity) __builtin_trap();
        if (err != CBOR_LITE_OK) {
            // Overflow is sticky: nothing more may be written afterwards.
            if (cbor_lite_enc_null(&enc) == CBOR_LITE_OK) __builtin_trap();
            break;
        }
        good = cbor_lite_enc_len(&enc);
        count++;
    }

    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, buf, good);
    for (int i = 0; i < count; i++) {
        cbor_lite_item_t got;
        if (cbor_lite_dec_next(&dec, &got) != CBOR_LITE_OK) __builtin_trap();
        check_item(&sent[i], &got);
    }
    if (cbor_lite_dec_offset(&dec) != good) __builtin_trap();
    free(buf);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    cbor_lite_decoder_t dec;
    cbor_lite_dec_init(&dec, data, size);

    // Drain all items until error or exhaustion
    cbor_lite_item_t item;
    for (int i = 0; i < 4096; i++) {
        cbor_lite_err_t err = cbor_lite_dec_next(&dec, &item);
        if (err != CBOR_LITE_OK) break;
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
