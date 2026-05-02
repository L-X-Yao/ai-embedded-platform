// SPDX-License-Identifier: MIT
// Fuzz harness for tlv_codec — decode random byte sequences
#include "tlv_codec.h"
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

        tlv_decoder_t dec;
        tlv_decoder_init(&dec, buf, (uint16_t)(len > 65535 ? 65535 : len));

        tlv_t tlv;
        // Drain all TLVs
        for (int k = 0; k < 1000; k++) {
            tlv_err_t err = tlv_decode_next(&dec, &tlv);
            if (err != TLV_OK) break;

            // Exercise value extraction helpers
            tlv_get_u8(&tlv);
            tlv_get_u16(&tlv);
            tlv_get_u32(&tlv);
        }
    }

    // Boundary: empty
    {
        tlv_decoder_t dec;
        tlv_decoder_init(&dec, NULL, 0);
        tlv_t tlv;
        tlv_decode_next(&dec, &tlv);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        tlv_decoder_t dec;
        tlv_decoder_init(&dec, &one, 1);
        tlv_t tlv;
        tlv_decode_next(&dec, &tlv);
    }

    // Boundary: exactly 4 bytes (minimum TLV header)
    {
        uint8_t buf[4] = {0x00, 0x01, 0x00, 0x00}; // tag=1, length=0
        tlv_decoder_t dec;
        tlv_decoder_init(&dec, buf, 4);
        tlv_t tlv;
        tlv_decode_next(&dec, &tlv);
    }

    // Boundary: TLV with length > remaining data
    {
        uint8_t buf[4] = {0x00, 0x01, 0xFF, 0xFF}; // tag=1, length=65535
        tlv_decoder_t dec;
        tlv_decoder_init(&dec, buf, 4);
        tlv_t tlv;
        tlv_decode_next(&dec, &tlv);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        tlv_decoder_t dec;
        tlv_decoder_init(&dec, buf, sizeof(buf));
        tlv_t tlv;
        for (int k = 0; k < 1000; k++) {
            if (tlv_decode_next(&dec, &tlv) != TLV_OK) break;
        }
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        tlv_decoder_t dec;
        tlv_decoder_init(&dec, buf, sizeof(buf));
        tlv_t tlv;
        for (int k = 0; k < 1000; k++) {
            if (tlv_decode_next(&dec, &tlv) != TLV_OK) break;
        }
    }

    printf("OK: tlv_codec fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
