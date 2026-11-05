// SPDX-License-Identifier: MIT
// LibFuzzer harness for tlv_codec — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_tlv_codec.c ../tlv_codec.c -o fuzz_tlv_codec
// Run:     ./fuzz_tlv_codec -max_total_time=60 corpus_tlv_codec/

#include "../tlv_codec.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RECORDS 16
#define VALUE_MAX 64

typedef struct {
    uint16_t tag;
    uint16_t length;
    uint8_t  value[VALUE_MAX + 1];
} sent_tlv_t;

static tlv_err_t encode_one(tlv_encoder_t *enc, sent_tlv_t *t, uint8_t kind, fuzz_input_t *in) {
    uint32_t hi = fuzz_u16(in);
    uint32_t number = (hi << 16) | fuzz_u16(in);

    switch (kind) {
    case 0:
        t->length = 1;
        t->value[0] = (uint8_t)number;
        return tlv_encode_u8(enc, t->tag, (uint8_t)number);
    case 1:
        t->length = 2;
        return tlv_encode_u16(enc, t->tag, (uint16_t)number);
    case 2:
        t->length = 4;
        return tlv_encode_u32(enc, t->tag, number);
    case 3: {
        uint8_t want = fuzz_u8(in) % (VALUE_MAX + 1);
        t->length = (uint16_t)fuzz_take_str(in, (char *)t->value, want);
        return tlv_encode_string(enc, t->tag, (const char *)t->value);
    }
    default: {
        uint8_t want = fuzz_u8(in) % (VALUE_MAX + 1);
        t->length = (uint16_t)fuzz_take(in, t->value, want);
        return tlv_encode(enc, t->tag, t->length > 0 ? t->value : NULL, t->length);
    }
    }
}

// Encode records into an exact-size buffer until it fills, then decode them back.
static void round_trip(fuzz_input_t *in) {
    static sent_tlv_t sent[MAX_RECORDS];
    static uint8_t kinds[MAX_RECORDS];
    uint16_t capacity = (uint16_t)(fuzz_u16(in) % 512);
    uint8_t *buf = malloc(capacity ? capacity : 1);
    if (!buf) return;

    tlv_encoder_t enc;
    tlv_encoder_init(&enc, buf, capacity);

    int count = 0;
    while (count < MAX_RECORDS && in->left > 0) {
        sent_tlv_t *t = &sent[count];
        uint16_t before = tlv_encoder_len(&enc);
        kinds[count] = fuzz_u8(in) % 5;
        t->tag = fuzz_u16(in);

        tlv_err_t err = encode_one(&enc, t, kinds[count], in);
        if (tlv_encoder_len(&enc) > capacity) __builtin_trap();
        if (err != TLV_OK) {
            if (tlv_encoder_len(&enc) != before) __builtin_trap();
            break;
        }
        count++;
    }

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, tlv_encoder_len(&enc));
    for (int i = 0; i < count; i++) {
        tlv_t got;
        if (tlv_decode_next(&dec, &got) != TLV_OK || got.tag != sent[i].tag ||
            got.length != sent[i].length) {
            __builtin_trap();
        }
        bool has_bytes = kinds[i] == 0 || kinds[i] >= 3;
        if (has_bytes && got.length > 0 &&
            memcmp(got.value, sent[i].value, got.length) != 0) {
            __builtin_trap();
        }
    }
    if (!tlv_decoder_done(&dec)) __builtin_trap();
    free(buf);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, data, len);

    // Drain all TLV elements until error or exhaustion
    tlv_t tlv;
    for (int i = 0; i < 4096; i++) {
        tlv_err_t err = tlv_decode_next(&dec, &tlv);
        if (err != TLV_OK) break;

        // Exercise value extraction on each decoded element
        tlv_get_u8(&tlv);
        tlv_get_u16(&tlv);
        tlv_get_u32(&tlv);
    }

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        round_trip(&in);
    }

    return 0;
}
