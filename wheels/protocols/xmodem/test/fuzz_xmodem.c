// SPDX-License-Identifier: MIT
// Fuzz harness for xmodem — feed random bytes into receiver state machine
#include "xmodem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static xmodem_err_t dummy_write(void *ctx, uint32_t offset,
                                const uint8_t *data, uint16_t len) {
    (void)ctx; (void)offset; (void)data; (void)len;
    return XMODEM_OK;
}

static xmodem_err_t dummy_send(void *ctx, uint8_t byte) {
    (void)ctx; (void)byte;
    return XMODEM_OK;
}

int main(void) {
    srand(42);

    static const xmodem_io_t io = {
        .write_block = dummy_write,
        .send_byte   = dummy_send,
        .ctx         = NULL,
    };

    xmodem_t xm;

    // Random byte streams
    for (int i = 0; i < 10000; i++) {
        xmodem_init(&xm, &io, (bool)(rand() & 1));
        xmodem_start(&xm);
        int len = rand() % 512; // keep shorter since it's byte-at-a-time
        for (int j = 0; j < len; j++) {
            uint8_t byte = (uint8_t)(rand() & 0xFF);
            xmodem_err_t err = xmodem_feed(&xm, byte);
            if (err < 0 && err != XMODEM_ERR_CRC && err != XMODEM_ERR_SEQUENCE) break;
        }
    }

    // Boundary: all zeros
    {
        xmodem_init(&xm, &io, true);
        xmodem_start(&xm);
        for (int j = 0; j < 256; j++) {
            xmodem_feed(&xm, 0x00);
        }
    }

    // Boundary: all 0xFF
    {
        xmodem_init(&xm, &io, true);
        xmodem_start(&xm);
        for (int j = 0; j < 256; j++) {
            xmodem_feed(&xm, 0xFF);
        }
    }

    // Boundary: SOH followed by garbage
    {
        xmodem_init(&xm, &io, false);
        xmodem_start(&xm);
        xmodem_feed(&xm, XMODEM_SOH);
        for (int j = 0; j < 132; j++) {
            xmodem_feed(&xm, (uint8_t)(j & 0xFF));
        }
    }

    // Boundary: CAN sequence
    {
        xmodem_init(&xm, &io, true);
        xmodem_start(&xm);
        for (int j = 0; j < 10; j++) {
            xmodem_feed(&xm, XMODEM_CAN);
        }
    }

    // Boundary: EOT without data
    {
        xmodem_init(&xm, &io, true);
        xmodem_start(&xm);
        xmodem_feed(&xm, XMODEM_EOT);
    }

    printf("OK: xmodem fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
