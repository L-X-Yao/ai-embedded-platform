// SPDX-License-Identifier: MIT
// Fuzz harness for bencode — decode random byte sequences
#include "bencode.h"
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

        bencode_decoder_t dec;
        bencode_decoder_init(&dec, buf, (size_t)len);

        bencode_token_t tok;
        // Drain all tokens
        for (int k = 0; k < 1000; k++) {
            bencode_err_t err = bencode_decode_next(&dec, &tok);
            if (err == BENCODE_ERR_END || err == BENCODE_ERR_MALFORMED) break;
        }
    }

    // Boundary: empty
    {
        bencode_decoder_t dec;
        bencode_decoder_init(&dec, NULL, 0);
        bencode_token_t tok;
        bencode_decode_next(&dec, &tok);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        bencode_decoder_t dec;
        bencode_decoder_init(&dec, &one, 1);
        bencode_token_t tok;
        bencode_decode_next(&dec, &tok);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        bencode_decoder_t dec;
        bencode_decoder_init(&dec, buf, sizeof(buf));
        bencode_token_t tok;
        for (int k = 0; k < 1000; k++) {
            bencode_err_t err = bencode_decode_next(&dec, &tok);
            if (err == BENCODE_ERR_END || err == BENCODE_ERR_MALFORMED) break;
        }
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        bencode_decoder_t dec;
        bencode_decoder_init(&dec, buf, sizeof(buf));
        bencode_token_t tok;
        for (int k = 0; k < 1000; k++) {
            bencode_err_t err = bencode_decode_next(&dec, &tok);
            if (err == BENCODE_ERR_END || err == BENCODE_ERR_MALFORMED) break;
        }
    }

    printf("OK: bencode fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
