// SPDX-License-Identifier: MIT
// Fuzz harness for cbor_lite — decode random byte sequences
#include "cbor_lite.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    // Random byte streams
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 1025;
        uint8_t buf[1025];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        cbor_lite_decoder_t dec;
        cbor_lite_dec_init(&dec, buf, (size_t)len);

        cbor_lite_item_t item;
        // Drain all items
        for (int k = 0; k < 1000; k++) {
            cbor_lite_err_t err = cbor_lite_dec_next(&dec, &item);
            if (err != CBOR_LITE_OK) break;
        }
    }

    // Boundary: empty
    {
        cbor_lite_decoder_t dec;
        cbor_lite_dec_init(&dec, NULL, 0);
        cbor_lite_item_t item;
        cbor_lite_dec_next(&dec, &item);
    }

    // Boundary: single byte (each major type)
    for (int b = 0; b < 256; b++) {
        uint8_t one = (uint8_t)b;
        cbor_lite_decoder_t dec;
        cbor_lite_dec_init(&dec, &one, 1);
        cbor_lite_item_t item;
        cbor_lite_dec_next(&dec, &item);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        cbor_lite_decoder_t dec;
        cbor_lite_dec_init(&dec, buf, sizeof(buf));
        cbor_lite_item_t item;
        for (int k = 0; k < 1000; k++) {
            if (cbor_lite_dec_next(&dec, &item) != CBOR_LITE_OK) break;
        }
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        cbor_lite_decoder_t dec;
        cbor_lite_dec_init(&dec, buf, sizeof(buf));
        cbor_lite_item_t item;
        for (int k = 0; k < 1000; k++) {
            if (cbor_lite_dec_next(&dec, &item) != CBOR_LITE_OK) break;
        }
    }

    printf("OK: cbor_lite fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
