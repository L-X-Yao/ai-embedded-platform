// SPDX-License-Identifier: MIT
// LibFuzzer harness for ymodem -- coverage-guided fuzzing
//
// Feeds the input as a raw byte stream, then as a block stream whose CRC the
// harness computes, so headers and data blocks get past the check.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../crc_any \
//          -I../../../../tooling/harness fuzz_libfuzzer_ymodem.c ../ymodem.c \
//          ../../crc_any/crc_any.c -o fuzz_ymodem
// Run:     ./fuzz_ymodem -max_total_time=60 corpus_ymodem/

#include "../ymodem.h"
#include "crc_any.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FRAME_MAX (3 + YMODEM_BLOCK_SIZE_STX + 2)

static const crc_any_config_t crc_cfg = {
    .width = 16, .poly = 0x1021, .init = 0x0000,
    .refin = false, .refout = false, .xorout = 0x0000,
};

static uint8_t g_write_buf[65536];
static uint32_t g_next_offset;
static bool g_track_offsets;
static bool g_write_fail;
static uint8_t g_sent_bytes[4096];
static uint32_t g_sent_pos;
static char g_last_filename[64];
static uint32_t g_last_file_size;

static void reset_io(bool track_offsets) {
    g_next_offset = 0;
    g_track_offsets = track_offsets;
    g_write_fail = false;
    g_sent_pos = 0;
    memset(g_last_filename, 0, sizeof(g_last_filename));
    g_last_file_size = 0;
}

static ymodem_err_t dummy_write_block(void *ctx, uint32_t offset,
                                       const uint8_t *data, uint16_t len) {
    (void)ctx;
    if (g_write_fail) return YMODEM_ERR_WRITE;

    // Accepted blocks of a file have to land back to back.
    if (g_track_offsets && offset != g_next_offset) __builtin_trap();
    g_next_offset = offset + len;

    if (offset + len <= sizeof(g_write_buf)) {
        memcpy(g_write_buf + offset, data, len);
    }
    return YMODEM_OK;
}

static ymodem_err_t dummy_send_byte(void *ctx, uint8_t byte) {
    (void)ctx;
    if (g_sent_pos < sizeof(g_sent_bytes)) {
        g_sent_bytes[g_sent_pos++] = byte;
    }
    return YMODEM_OK;
}

static void dummy_on_file_start(void *ctx, const char *filename,
                                 uint32_t file_size) {
    (void)ctx;
    if (filename) {
        size_t len = strlen(filename);
        if (len >= sizeof(g_last_filename)) len = sizeof(g_last_filename) - 1;
        memcpy(g_last_filename, filename, len);
        g_last_filename[len] = '\0';
    }
    g_last_file_size = file_size;
    g_next_offset = 0;
}

static uint8_t pick_block(const ymodem_t *ym, uint8_t op, fuzz_input_t *in) {
    switch ((op >> 2) & 3) {
    case 1:  return (uint8_t)(ym->block_num - 1);
    case 2:  return (uint8_t)(ym->block_num + 1);
    case 3:  return fuzz_u8(in);
    default: return ym->block_num;
    }
}

static size_t build_block(uint8_t *frame, uint8_t blk, const uint8_t *payload,
                          uint16_t block_size) {
    uint32_t crc = 0;

    frame[0] = block_size == YMODEM_BLOCK_SIZE_STX ? YMODEM_STX : YMODEM_SOH;
    frame[1] = blk;
    frame[2] = (uint8_t)~blk;
    memcpy(&frame[3], payload, block_size);

    crc_any_compute(&crc_cfg, payload, block_size, &crc);
    frame[3 + block_size] = (uint8_t)(crc >> 8);
    frame[3 + block_size + 1] = (uint8_t)crc;
    return (size_t)block_size + 5;
}

static void check_state(const ymodem_t *ym) {
    // The file name stays terminated however long the header claims it is.
    if (!memchr(ym->filename, 0, sizeof(ym->filename))) __builtin_trap();

    // Padding of the last block never counts as received data.
    if (ym->state == YMODEM_STATE_DATA && ym->file_size > 0 &&
        ym->total_received > ym->file_size) {
        __builtin_trap();
    }
}

// Block stream built from the input. Each record is [op][arg][payload]:
//   op bits 0-1: 0 valid block, 1 bad CRC, 2 bad complement, 3 raw byte
//   op bits 2-3: 0 next block, 1 duplicate, 2 skip ahead, 3 arbitrary number
//   op bit 4:    1024-byte block instead of 128
//   op bit 6:    with the timer on, report a quiet line instead
//   op bit 7:    fail the write callback from this block on
// For a raw byte, arg is the byte. Otherwise arg is the payload length and the
// block is zero padded.
static void fuzz_blocks(const ymodem_io_t *io, const uint8_t *data, size_t size,
                        bool use_idle) {
    static uint8_t frame[FRAME_MAX];
    static uint8_t payload[YMODEM_BLOCK_SIZE_STX];

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    reset_io(true);

    ymodem_t ym;
    memset(&ym, 0, sizeof(ym));
    ymodem_init(&ym, io);
    if (use_idle) ymodem_enable_line_idle(&ym);
    ymodem_start(&ym);

    bool running = true;
    while (running && in.left > 0) {
        uint8_t op = fuzz_u8(&in);
        uint8_t arg = fuzz_u8(&in);

        if (use_idle && (op & 0x40)) {
            running = ymodem_line_idle(&ym) == YMODEM_OK;
            check_state(&ym);
            continue;
        }

        if ((op & 3) == 3) {
            running = ymodem_feed(&ym, arg) == YMODEM_OK;
            check_state(&ym);
            continue;
        }

        uint16_t block_size = (op & 0x10) ? YMODEM_BLOCK_SIZE_STX
                                          : YMODEM_BLOCK_SIZE_SOH;
        memset(payload, 0, sizeof(payload));
        fuzz_take(&in, payload, arg < block_size ? arg : block_size);

        uint8_t blk = pick_block(&ym, op, &in);
        size_t frame_len = build_block(frame, blk, payload, block_size);
        if ((op & 3) == 1) frame[frame_len - 1] ^= 0x01;
        if ((op & 3) == 2) frame[2] ^= 0x01;
        g_write_fail = (op & 0x80) != 0;

        for (size_t i = 0; running && i < frame_len; i++) {
            running = ymodem_feed(&ym, frame[i]) == YMODEM_OK;
        }
        check_state(&ym);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ymodem_io_t io;
    memset(&io, 0, sizeof(io));
    io.write_block = dummy_write_block;
    io.send_byte = dummy_send_byte;
    io.on_file_start = dummy_on_file_start;
    io.ctx = NULL;

    // Normal flow: init, start, feed all bytes
    {
        reset_io(true);

        ymodem_t ym;
        memset(&ym, 0, sizeof(ym));
        ymodem_init(&ym, &io);
        ymodem_start(&ym);

        for (size_t i = 0; i < size; i++) {
            ymodem_err_t err = ymodem_feed(&ym, data[i]);
            if (err < 0 || err == YMODEM_DONE) break;
        }

        (void)ymodem_get_state(&ym);
        (void)ymodem_get_filename(&ym);
        (void)ymodem_get_file_size(&ym);
    }

    // Cancel mid-stream
    if (size > 10) {
        reset_io(true);

        ymodem_t ym;
        memset(&ym, 0, sizeof(ym));
        ymodem_init(&ym, &io);
        ymodem_start(&ym);

        for (size_t i = 0; i < size / 2; i++) {
            ymodem_err_t err = ymodem_feed(&ym, data[i]);
            if (err < 0 || err == YMODEM_DONE) break;
        }
        ymodem_cancel(&ym);
        (void)ymodem_get_state(&ym);
    }

    // Test without on_file_start callback
    {
        ymodem_io_t io_no_cb;
        memset(&io_no_cb, 0, sizeof(io_no_cb));
        io_no_cb.write_block = dummy_write_block;
        io_no_cb.send_byte = dummy_send_byte;
        io_no_cb.on_file_start = NULL;
        io_no_cb.ctx = NULL;

        // No on_file_start, so the harness cannot see where a new file begins.
        reset_io(false);

        ymodem_t ym;
        memset(&ym, 0, sizeof(ym));
        ymodem_init(&ym, &io_no_cb);
        ymodem_start(&ym);

        for (size_t i = 0; i < size; i++) {
            ymodem_err_t err = ymodem_feed(&ym, data[i]);
            if (err < 0 || err == YMODEM_DONE) break;
        }
    }

    fuzz_blocks(&io, data, size, false);
    fuzz_blocks(&io, data, size, true);

    return 0;
}
