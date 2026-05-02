// SPDX-License-Identifier: MIT
// Fuzz harness for protobuf_lite — decode random byte sequences
#include "protobuf_lite.h"
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

        pb_decoder_t dec;
        pb_decoder_init(&dec, buf, (uint16_t)(len > 65535 ? 65535 : len));

        pb_field_t field;
        // Drain all fields
        for (int k = 0; k < 1000; k++) {
            pb_err_t err = pb_decode_field(&dec, &field);
            if (err != PB_OK) break;
        }
    }

    // Boundary: empty
    {
        pb_decoder_t dec;
        pb_decoder_init(&dec, NULL, 0);
        pb_field_t field;
        pb_decode_field(&dec, &field);
    }

    // Boundary: single byte (all values)
    for (int b = 0; b < 256; b++) {
        uint8_t one = (uint8_t)b;
        pb_decoder_t dec;
        pb_decoder_init(&dec, &one, 1);
        pb_field_t field;
        pb_decode_field(&dec, &field);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        pb_decoder_t dec;
        pb_decoder_init(&dec, buf, sizeof(buf));
        pb_field_t field;
        for (int k = 0; k < 1000; k++) {
            if (pb_decode_field(&dec, &field) != PB_OK) break;
        }
    }

    // Boundary: all 0xFF (long varint sequences)
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        pb_decoder_t dec;
        pb_decoder_init(&dec, buf, sizeof(buf));
        pb_field_t field;
        for (int k = 0; k < 1000; k++) {
            if (pb_decode_field(&dec, &field) != PB_OK) break;
        }
    }

    // Boundary: wire type 2 (length-delimited) with huge length
    {
        uint8_t buf[] = {0x0A, 0xFF, 0xFF, 0xFF, 0x0F}; // field 1, wire type 2, length = max varint
        pb_decoder_t dec;
        pb_decoder_init(&dec, buf, sizeof(buf));
        pb_field_t field;
        pb_decode_field(&dec, &field);
    }

    printf("OK: protobuf_lite fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
