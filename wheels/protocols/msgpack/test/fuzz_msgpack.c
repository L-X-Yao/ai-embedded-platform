// SPDX-License-Identifier: MIT
// Fuzz harness for msgpack — decode random byte sequences
#include "msgpack.h"
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

        msgpack_dec_t dec;
        msgpack_dec_init(&dec, buf, (size_t)len);

        msgpack_token_t tok;
        // Drain all tokens
        for (int k = 0; k < 1000; k++) {
            msgpack_err_t err = msgpack_dec_next(&dec, &tok);
            if (err != MSGPACK_OK) break;
        }
    }

    // Boundary: empty
    {
        msgpack_dec_t dec;
        msgpack_dec_init(&dec, NULL, 0);
        msgpack_token_t tok;
        msgpack_dec_next(&dec, &tok);
    }

    // Boundary: every possible single byte
    for (int b = 0; b < 256; b++) {
        uint8_t one = (uint8_t)b;
        msgpack_dec_t dec;
        msgpack_dec_init(&dec, &one, 1);
        msgpack_token_t tok;
        msgpack_dec_next(&dec, &tok);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        msgpack_dec_t dec;
        msgpack_dec_init(&dec, buf, sizeof(buf));
        msgpack_token_t tok;
        for (int k = 0; k < 1000; k++) {
            if (msgpack_dec_next(&dec, &tok) != MSGPACK_OK) break;
        }
    }

    // Boundary: all 0xFF (negative fixint)
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        msgpack_dec_t dec;
        msgpack_dec_init(&dec, buf, sizeof(buf));
        msgpack_token_t tok;
        for (int k = 0; k < 1000; k++) {
            if (msgpack_dec_next(&dec, &tok) != MSGPACK_OK) break;
        }
    }

    printf("OK: msgpack fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
