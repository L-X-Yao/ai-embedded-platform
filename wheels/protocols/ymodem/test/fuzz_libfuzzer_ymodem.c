// SPDX-License-Identifier: MIT
// LibFuzzer harness for ymodem -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_ymodem.c ../ymodem.c -o fuzz_ymodem
// Run:     ./fuzz_ymodem -max_total_time=60 corpus_ymodem/

#include "../ymodem.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint8_t g_write_buf[65536];
static uint32_t g_write_pos;
static uint8_t g_sent_bytes[4096];
static uint32_t g_sent_pos;
static char g_last_filename[64];
static uint32_t g_last_file_size;

static ymodem_err_t dummy_write_block(void *ctx, uint32_t offset,
                                       const uint8_t *data, uint16_t len) {
    (void)ctx;
    if (offset + len <= sizeof(g_write_buf)) {
        memcpy(g_write_buf + offset, data, len);
        g_write_pos = offset + len;
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
        g_write_pos = 0;
        g_sent_pos = 0;
        memset(g_last_filename, 0, sizeof(g_last_filename));
        g_last_file_size = 0;

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
        g_write_pos = 0;
        g_sent_pos = 0;

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

        g_write_pos = 0;
        g_sent_pos = 0;

        ymodem_t ym;
        memset(&ym, 0, sizeof(ym));
        ymodem_init(&ym, &io_no_cb);
        ymodem_start(&ym);

        for (size_t i = 0; i < size; i++) {
            ymodem_err_t err = ymodem_feed(&ym, data[i]);
            if (err < 0 || err == YMODEM_DONE) break;
        }
    }

    return 0;
}
