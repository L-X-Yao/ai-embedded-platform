// SPDX-License-Identifier: MIT
// LibFuzzer harness for heatshrink -- coverage-guided fuzzing
//
// Decompresses the input as a raw stream, then compresses and decompresses it with
// the window, lookahead and output chunk read from its first bytes. What comes back
// has to match, and neither side may stop making progress.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_heatshrink.c ../heatshrink.c -o fuzz_heatshrink
// Run:     ./fuzz_heatshrink -max_total_time=60 corpus_heatshrink/

#include "../heatshrink.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_MAX 3072u
#define PACKED_MAX (INPUT_MAX * 2u + 16u)
#define IDLE_LIMIT 4

typedef struct {
    uint16_t window;
    uint8_t  lookahead;
    uint16_t chunk;
} params_t;

static uint8_t g_packed[PACKED_MAX];
static uint8_t g_unpacked[INPUT_MAX];

static void drain_encoder(heatshrink_encoder_t *enc, uint8_t *chunk, uint16_t chunk_size,
                          size_t *packed_len) {
    heatshrink_poll_res_t res;
    int idle = 0;
    do {
        uint16_t produced = 0;
        res = heatshrink_encoder_poll(enc, chunk, chunk_size, &produced);
        if (produced > chunk_size || *packed_len + produced > PACKED_MAX) __builtin_trap();
        memcpy(&g_packed[*packed_len], chunk, produced);
        *packed_len += produced;
        idle = produced ? 0 : idle + 1;
        if (idle > IDLE_LIMIT) __builtin_trap();
    } while (res == HEATSHRINK_POLL_MORE);
}

static size_t compress(const uint8_t *src, size_t len, const params_t *p, uint8_t *chunk) {
    uint8_t *window = malloc(p->window);
    if (!window) return 0;

    heatshrink_encoder_t enc;
    if (heatshrink_encoder_init(&enc, window, p->window, p->lookahead) != HEATSHRINK_OK) {
        __builtin_trap();
    }

    size_t in_off = 0;
    size_t packed_len = 0;
    while (in_off < len) {
        uint16_t consumed = 0;
        size_t before = packed_len;
        heatshrink_encoder_sink(&enc, &src[in_off], (uint16_t)(len - in_off), &consumed);
        in_off += consumed;
        drain_encoder(&enc, chunk, p->chunk, &packed_len);

        // Input refused and nothing to read back: the caller can never finish.
        if (consumed == 0 && packed_len == before) __builtin_trap();
    }

    heatshrink_encoder_finish(&enc);
    drain_encoder(&enc, chunk, p->chunk, &packed_len);
    free(window);
    return packed_len;
}

static void drain_decoder(heatshrink_decoder_t *dec, uint8_t *chunk, uint16_t chunk_size,
                          uint8_t *out, size_t out_cap, size_t *out_len) {
    heatshrink_poll_res_t res;
    int idle = 0;
    do {
        uint16_t produced = 0;
        res = heatshrink_decoder_poll(dec, chunk, chunk_size, &produced);
        if (produced > chunk_size) __builtin_trap();
        if (out && *out_len + produced <= out_cap) memcpy(&out[*out_len], chunk, produced);
        *out_len += produced;
        idle = produced ? 0 : idle + 1;
        if (idle > IDLE_LIMIT) __builtin_trap();
    } while (res == HEATSHRINK_POLL_MORE);
}

static size_t decompress(const uint8_t *src, size_t len, const params_t *p, uint8_t *chunk,
                         uint8_t *out, size_t out_cap) {
    // Exact-size window plus staging half, so ASan sees any access past either.
    size_t buf_size = (size_t)p->window + p->window / 2u;
    uint8_t *buf = malloc(buf_size);
    if (!buf) return 0;

    heatshrink_decoder_t dec;
    if (heatshrink_decoder_init(&dec, buf, p->window, p->lookahead) != HEATSHRINK_OK) {
        __builtin_trap();
    }

    size_t in_off = 0;
    size_t out_len = 0;
    while (in_off < len) {
        uint16_t consumed = 0;
        size_t before = out_len;
        heatshrink_decoder_sink(&dec, &src[in_off], (uint16_t)(len - in_off), &consumed);
        in_off += consumed;
        drain_decoder(&dec, chunk, p->chunk, out, out_cap, &out_len);
        if (consumed == 0 && out_len == before) __builtin_trap();
    }

    heatshrink_decoder_finish(&dec);
    drain_decoder(&dec, chunk, p->chunk, out, out_cap, &out_len);
    if (heatshrink_decoder_finish(&dec) != HEATSHRINK_FINISH_DONE) __builtin_trap();
    free(buf);
    return out_len;
}

static void read_params(const uint8_t *data, params_t *p) {
    p->window = (uint16_t)(256u << (data[0] & 3));
    p->lookahead = (uint8_t)(4 + (data[0] >> 2) % 4);
    p->chunk = (uint16_t)(1u + data[1] % 64u);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2 || size - 2 > INPUT_MAX) return 0;

    params_t p;
    read_params(data, &p);
    const uint8_t *body = data + 2;
    size_t body_len = size - 2;

    // Exact-size output chunk, so ASan sees any write past out_size.
    uint8_t *chunk = malloc(p.chunk);
    if (!chunk) return 0;

    decompress(body, body_len, &p, chunk, NULL, 0);

    size_t packed_len = compress(body, body_len, &p, chunk);
    size_t got = decompress(g_packed, packed_len, &p, chunk, g_unpacked, sizeof(g_unpacked));
    if (got != body_len || memcmp(g_unpacked, body, body_len) != 0) __builtin_trap();

    free(chunk);
    return 0;
}
