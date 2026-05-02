// SPDX-License-Identifier: MIT
// Fuzz harness for ymodem — feed random bytes into receiver state machine
#include "ymodem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static ymodem_err_t dummy_write(void *ctx, uint32_t offset,
                                const uint8_t *data, uint16_t len) {
    (void)ctx; (void)offset; (void)data; (void)len;
    return YMODEM_OK;
}

static ymodem_err_t dummy_send(void *ctx, uint8_t byte) {
    (void)ctx; (void)byte;
    return YMODEM_OK;
}

static void dummy_on_file(void *ctx, const char *filename, uint32_t file_size) {
    (void)ctx; (void)filename; (void)file_size;
}

int main(void) {
    srand(42);

    static const ymodem_io_t io = {
        .write_block   = dummy_write,
        .send_byte     = dummy_send,
        .on_file_start = dummy_on_file,
        .ctx           = NULL,
    };

    ymodem_t ym;

    // Random byte streams
    for (int i = 0; i < 10000; i++) {
        ymodem_init(&ym, &io);
        ymodem_start(&ym);
        int len = rand() % 512; // keep shorter since it's byte-at-a-time
        for (int j = 0; j < len; j++) {
            uint8_t byte = (uint8_t)(rand() & 0xFF);
            ymodem_err_t err = ymodem_feed(&ym, byte);
            if (err < 0 && err != YMODEM_ERR_CRC && err != YMODEM_ERR_SEQ) break;
        }
    }

    // Boundary: all zeros
    {
        ymodem_init(&ym, &io);
        ymodem_start(&ym);
        for (int j = 0; j < 256; j++) {
            ymodem_feed(&ym, 0x00);
        }
    }

    // Boundary: all 0xFF
    {
        ymodem_init(&ym, &io);
        ymodem_start(&ym);
        for (int j = 0; j < 256; j++) {
            ymodem_feed(&ym, 0xFF);
        }
    }

    // Boundary: SOH header block followed by garbage
    {
        ymodem_init(&ym, &io);
        ymodem_start(&ym);
        ymodem_feed(&ym, YMODEM_SOH);
        for (int j = 0; j < 132; j++) {
            ymodem_feed(&ym, (uint8_t)(j & 0xFF));
        }
    }

    // Boundary: STX (1024-byte block) followed by garbage
    {
        ymodem_init(&ym, &io);
        ymodem_start(&ym);
        ymodem_feed(&ym, YMODEM_STX);
        for (int j = 0; j < 1029; j++) {
            ymodem_feed(&ym, (uint8_t)(j & 0xFF));
        }
    }

    // Boundary: CAN sequence
    {
        ymodem_init(&ym, &io);
        ymodem_start(&ym);
        for (int j = 0; j < 10; j++) {
            ymodem_feed(&ym, YMODEM_CAN);
        }
    }

    printf("OK: ymodem fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
