// SPDX-License-Identifier: MIT
// LibFuzzer harness for xmodem -- coverage-guided fuzzing
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_xmodem.c ../xmodem.c -o fuzz_xmodem
// Run:     ./fuzz_xmodem -max_total_time=60 corpus_xmodem/

#include "../xmodem.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint8_t g_write_buf[65536];
static uint32_t g_write_pos;
static uint8_t g_sent_bytes[4096];
static uint32_t g_sent_pos;

static xmodem_err_t dummy_write_block(void *ctx, uint32_t offset,
                                       const uint8_t *data, uint16_t len) {
    (void)ctx;
    if (offset + len <= sizeof(g_write_buf)) {
        memcpy(g_write_buf + offset, data, len);
        g_write_pos = offset + len;
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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    xmodem_io_t io;
    memset(&io, 0, sizeof(io));
    io.write_block = dummy_write_block;
    io.send_byte = dummy_send_byte;
    io.ctx = NULL;

    // Test with CRC mode
    {
        g_write_pos = 0;
        g_sent_pos = 0;

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
        g_write_pos = 0;
        g_sent_pos = 0;

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
        g_write_pos = 0;
        g_sent_pos = 0;

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

    return 0;
}
