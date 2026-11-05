// SPDX-License-Identifier: MIT
// LibFuzzer harness for protobuf_lite — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_protobuf_lite.c ../protobuf_lite.c -o fuzz_protobuf_lite
// Run:     ./fuzz_protobuf_lite -max_total_time=60 corpus_protobuf_lite/

#include "../protobuf_lite.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 16
#define BYTES_MAX 64

typedef struct {
    uint8_t  kind;
    uint8_t  field_num;
    uint64_t number;
    uint8_t  bytes[BYTES_MAX + 1];
    uint16_t len;
} sent_field_t;

static pb_err_t encode_one(pb_encoder_t *enc, sent_field_t *f, fuzz_input_t *in) {
    uint32_t hi = fuzz_u16(in);
    uint32_t lo = ((hi << 16) | fuzz_u16(in));
    hi = fuzz_u16(in);
    f->number = ((uint64_t)((hi << 16) | fuzz_u16(in)) << 32) | lo;

    switch (f->kind) {
    case 0: return pb_encode_varint(enc, f->field_num, f->number);
    case 1: return pb_encode_sint32(enc, f->field_num, (int32_t)(uint32_t)f->number);
    case 2: return pb_encode_sint64(enc, f->field_num, (int64_t)f->number);
    case 3: return pb_encode_fixed32(enc, f->field_num, (uint32_t)f->number);
    case 4: return pb_encode_fixed64(enc, f->field_num, f->number);
    case 5: {
        float value;
        uint32_t bits = (uint32_t)f->number;
        memcpy(&value, &bits, sizeof(value));
        return pb_encode_float(enc, f->field_num, value);
    }
    case 6: {
        uint8_t want = fuzz_u8(in) % (BYTES_MAX + 1);
        f->len = (uint16_t)fuzz_take(in, f->bytes, want);
        return pb_encode_bytes(enc, f->field_num, f->len > 0 ? f->bytes : NULL, f->len);
    }
    default: {
        uint8_t want = fuzz_u8(in) % (BYTES_MAX + 1);
        f->len = (uint16_t)fuzz_take_str(in, (char *)f->bytes, want);
        return pb_encode_string(enc, f->field_num, (const char *)f->bytes);
    }
    }
}

static void check_field(const sent_field_t *f, const pb_field_t *got) {
    if (got->field_num != f->field_num) __builtin_trap();

    switch (f->kind) {
    case 0:
        if (got->wire_type != PB_WIRE_VARINT || got->value.varint != f->number) __builtin_trap();
        break;
    case 1:
        if (pb_zigzag_decode32((uint32_t)got->value.varint) != (int32_t)(uint32_t)f->number) {
            __builtin_trap();
        }
        break;
    case 2:
        if (pb_zigzag_decode64(got->value.varint) != (int64_t)f->number) __builtin_trap();
        break;
    case 3:
    case 5:
        if (got->wire_type != PB_WIRE_32BIT || got->value.fixed32 != (uint32_t)f->number) {
            __builtin_trap();
        }
        break;
    case 4:
        if (got->wire_type != PB_WIRE_64BIT || got->value.fixed64 != f->number) __builtin_trap();
        break;
    default:
        if (got->wire_type != PB_WIRE_LENGTH || got->value.bytes.len != f->len ||
            (f->len > 0 && memcmp(got->value.bytes.data, f->bytes, f->len) != 0)) {
            __builtin_trap();
        }
        break;
    }
}

// Encode fields into an exact-size buffer until it fills, then decode them back.
static void round_trip(fuzz_input_t *in) {
    static sent_field_t sent[MAX_FIELDS];
    uint16_t capacity = (uint16_t)(fuzz_u16(in) % 512);
    uint8_t *buf = malloc(capacity ? capacity : 1);
    if (!buf) return;

    pb_encoder_t enc;
    pb_encoder_init(&enc, buf, capacity);

    int count = 0;
    while (count < MAX_FIELDS && in->left > 0) {
        sent_field_t *f = &sent[count];
        uint16_t before = pb_encoder_len(&enc);
        f->kind = fuzz_u8(in) % 8;
        f->field_num = fuzz_u8(in);
        f->len = 0;

        pb_err_t err = encode_one(&enc, f, in);
        if (pb_encoder_len(&enc) > capacity) __builtin_trap();
        if (err == PB_ERR_PARAM) {
            if (f->field_num != 0) __builtin_trap();
            continue;
        }
        if (err != PB_OK) {
            // A field that did not fit may leave part of itself behind.
            enc.pos = before;
            break;
        }
        count++;
    }

    pb_decoder_t dec;
    pb_decoder_init(&dec, buf, pb_encoder_len(&enc));
    for (int i = 0; i < count; i++) {
        pb_field_t got;
        if (pb_decode_field(&dec, &got) != PB_OK) __builtin_trap();
        check_field(&sent[i], &got);
    }
    if (!pb_decoder_done(&dec)) __builtin_trap();
    free(buf);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    pb_decoder_t dec;
    pb_decoder_init(&dec, data, len);

    // Drain all fields until error or exhaustion
    pb_field_t field;
    for (int i = 0; i < 4096; i++) {
        pb_err_t err = pb_decode_field(&dec, &field);
        if (err != PB_OK) break;
    }

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    while (in.left > 0) {
        round_trip(&in);
    }

    return 0;
}
