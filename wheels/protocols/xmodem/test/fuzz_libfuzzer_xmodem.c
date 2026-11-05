// SPDX-License-Identifier: MIT
// LibFuzzer harness for xmodem -- coverage-guided fuzzing
//
// Feeds the input as a raw byte stream, then as a block stream whose check
// fields the harness computes, so blocks get past the CRC.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../crc_any \
//          -I../../../../tooling/harness fuzz_libfuzzer_xmodem.c ../xmodem.c \
//          ../../crc_any/crc_any.c -o fuzz_xmodem
// Run:     ./fuzz_xmodem -max_total_time=60 corpus_xmodem/

#include "../xmodem.h"
#include "crc_any.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FRAME_MAX 133

static const crc_any_config_t crc_cfg = {
    .width = 16, .poly = 0x1021, .init = 0x0000,
    .refin = false, .refout = false, .xorout = 0x0000,
};

static uint8_t g_write_buf[65536];
static uint32_t g_next_offset;
static bool g_write_fail;
static uint8_t g_sent_bytes[4096];
static uint32_t g_sent_pos;

static void reset_io(void) {
    g_next_offset = 0;
    g_write_fail = false;
    g_sent_pos = 0;
}

static xmodem_err_t dummy_write_block(void *ctx, uint32_t offset,
                                       const uint8_t *data, uint16_t len) {
    (void)ctx;
    if (g_write_fail) return XMODEM_ERR_WRITE;

    // Accepted blocks have to land back to back.
    if (offset != g_next_offset) __builtin_trap();
    g_next_offset = offset + len;

    if (offset + len <= sizeof(g_write_buf)) {
        memcpy(g_write_buf + offset, data, len);
    }
    return XMODEM_OK;
}

static xmodem_err_t dummy_send_byte(void *ctx, uint8_t byte) {
    (void)ctx;
    if (g_sent_pos < sizeof(g_sent_bytes)) {
        g_sent_bytes[g_sent_pos++] = byte;
    }
    return XMODEM_OK;
}

static uint8_t pick_block(const xmodem_t *xm, uint8_t op, fuzz_input_t *in) {
    switch ((op >> 2) & 3) {
    case 1:  return (uint8_t)(xm->block_num - 1);
    case 2:  return (uint8_t)(xm->block_num + 1);
    case 3:  return fuzz_u8(in);
    default: return xm->block_num;
    }
}

static size_t build_block(uint8_t *frame, uint8_t blk, const uint8_t *payload,
                          bool use_crc) {
    const size_t data_end = 3 + XMODEM_BLOCK_SIZE;

    frame[0] = XMODEM_SOH;
    frame[1] = blk;
    frame[2] = (uint8_t)~blk;
    memcpy(&frame[3], payload, XMODEM_BLOCK_SIZE);

    if (use_crc) {
        uint32_t crc = 0;
        crc_any_compute(&crc_cfg, payload, XMODEM_BLOCK_SIZE, &crc);
        frame[data_end] = (uint8_t)(crc >> 8);
        frame[data_end + 1] = (uint8_t)crc;
        return data_end + 2;
    }

    uint8_t sum = 0;
    for (size_t i = 0; i < XMODEM_BLOCK_SIZE; i++) sum = (uint8_t)(sum + payload[i]);
    frame[data_end] = sum;
    return data_end + 1;
}

// Block stream built from the input. Each record is [op][arg][payload]:
//   op bits 0-1: 0 valid block, 1 bad check field, 2 bad complement, 3 raw byte
//   op bits 2-3: 0 next block, 1 duplicate, 2 skip ahead, 3 arbitrary number
//   op bit 7:    fail the write callback from this block on
// For a raw byte, arg is the byte. Otherwise arg is the payload length and the
// block is zero padded.
static void fuzz_blocks(const xmodem_io_t *io, const uint8_t *data, size_t size,
                        bool use_crc) {
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    reset_io();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_init(&xm, io, use_crc);
    xmodem_start(&xm);

    bool running = true;
    while (running && in.left > 0) {
        uint8_t op = fuzz_u8(&in);
        uint8_t arg = fuzz_u8(&in);

        if ((op & 3) == 3) {
            running = xmodem_feed(&xm, arg) == XMODEM_OK;
            continue;
        }

        uint8_t payload[XMODEM_BLOCK_SIZE];
        uint8_t frame[FRAME_MAX];
        memset(payload, 0, sizeof(payload));
        fuzz_take(&in, payload, arg < sizeof(payload) ? arg : sizeof(payload));

        uint8_t blk = pick_block(&xm, op, &in);
        size_t frame_len = build_block(frame, blk, payload, use_crc);
        if ((op & 3) == 1) frame[frame_len - 1] ^= 0x01;
        if ((op & 3) == 2) frame[2] ^= 0x01;
        g_write_fail = (op & 0x80) != 0;

        for (size_t i = 0; running && i < frame_len; i++) {
            running = xmodem_feed(&xm, frame[i]) == XMODEM_OK;
        }
    }

    // The byte count has to match what reached the write callback.
    if (xmodem_get_total_bytes(&xm) != g_next_offset) __builtin_trap();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    xmodem_io_t io;
    memset(&io, 0, sizeof(io));
    io.write_block = dummy_write_block;
    io.send_byte = dummy_send_byte;
    io.ctx = NULL;

    // Test with CRC mode
    {
        reset_io();

        xmodem_t xm;
        memset(&xm, 0, sizeof(xm));
        xmodem_init(&xm, &io, true);
        xmodem_start(&xm);

        for (size_t i = 0; i < size; i++) {
            xmodem_err_t err = xmodem_feed(&xm, data[i]);
            if (err < 0 || err == XMODEM_DONE) break;
        }

        (void)xmodem_get_state(&xm);
        (void)xmodem_get_total_bytes(&xm);
    }

    // Test with checksum mode
    {
        reset_io();

        xmodem_t xm;
        memset(&xm, 0, sizeof(xm));
        xmodem_init(&xm, &io, false);
        xmodem_start(&xm);

        for (size_t i = 0; i < size; i++) {
            xmodem_err_t err = xmodem_feed(&xm, data[i]);
            if (err < 0 || err == XMODEM_DONE) break;
        }
    }

    // Test cancel mid-stream
    if (size > 10) {
        reset_io();

        xmodem_t xm;
        memset(&xm, 0, sizeof(xm));
        xmodem_init(&xm, &io, true);
        xmodem_start(&xm);

        for (size_t i = 0; i < size / 2; i++) {
            xmodem_err_t err = xmodem_feed(&xm, data[i]);
            if (err < 0 || err == XMODEM_DONE) break;
        }
        xmodem_cancel(&xm);
        (void)xmodem_get_state(&xm);
    }

    fuzz_blocks(&io, data, size, true);
    fuzz_blocks(&io, data, size, false);

    return 0;
}
