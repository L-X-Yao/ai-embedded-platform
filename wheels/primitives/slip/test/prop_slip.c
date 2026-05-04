/*
 * prop_slip.c — property-based tests for slip
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_slip.c ../slip.c -o prop_slip
 */

#include "test_harness.h"
#include "property_test.h"
#include "../slip.h"
#include <string.h>

/* Property: encode then decode returns original data */
PROPERTY(encode_decode_roundtrip, 10000) {
    uint8_t input[128];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, input, input_len);

    uint16_t max_enc = slip_max_encoded_size(input_len);
    uint8_t encoded[512];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    slip_err_t err = slip_encode(input, input_len, encoded, &enc_len);
    PROP_ASSERT_EQ(err, SLIP_OK);
    PROP_ASSERT(enc_len <= max_enc);

    uint8_t decoded[128];
    uint16_t dec_len = (uint16_t)sizeof(decoded);
    err = slip_decode(encoded, enc_len, decoded, &dec_len);
    PROP_ASSERT_EQ(err, SLIP_OK);
    PROP_ASSERT_EQ(dec_len, input_len);
    PROP_ASSERT_MEM_EQ(decoded, input, input_len);
}

/* Property: streaming decoder produces same result as block decode */
PROPERTY(streaming_matches_block, 5000) {
    uint8_t input[64];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 1, 64);
    ARBI_FILL(seed, input, input_len);

    uint8_t encoded[256];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    slip_err_t err = slip_encode(input, input_len, encoded, &enc_len);
    PROP_ASSERT_EQ(err, SLIP_OK);

    /* feed encoded bytes one-at-a-time to streaming decoder */
    uint8_t stream_buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, stream_buf, sizeof(stream_buf));

    for (uint16_t i = 0; i < enc_len; i++) {
        slip_decoder_feed(&dec, encoded[i]);
    }

    PROP_ASSERT(slip_decoder_ready(&dec));

    const uint8_t *frame_data;
    uint16_t frame_len = slip_decoder_frame(&dec, &frame_data);
    PROP_ASSERT_EQ(frame_len, input_len);
    PROP_ASSERT_MEM_EQ(frame_data, input, input_len);
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    uint8_t garbage[256];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 256);
    ARBI_FILL(seed, garbage, len);

    uint8_t out[256];
    uint16_t out_len = (uint16_t)sizeof(out);
    slip_decode(garbage, len, out, &out_len);
    /* no crash is the assertion */
}

/* Property: streaming feed of arbitrary bytes never crashes */
PROPERTY(streaming_feed_no_crash, 50000) {
    uint8_t garbage[128];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, garbage, len);

    uint8_t buf[128];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, sizeof(buf));

    for (uint16_t i = 0; i < len; i++) {
        slip_decoder_feed(&dec, garbage[i]);
    }
    /* no crash is the assertion */
}

/* Property: encoded size is bounded by max_encoded_size */
PROPERTY(encoded_size_bounded, 10000) {
    uint8_t input[128];
    uint16_t input_len = (uint16_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, input, input_len);

    uint16_t max_enc = slip_max_encoded_size(input_len);

    uint8_t encoded[512];
    uint16_t enc_len = (uint16_t)sizeof(encoded);
    slip_err_t err = slip_encode(input, input_len, encoded, &enc_len);
    PROP_ASSERT_EQ(err, SLIP_OK);
    PROP_ASSERT(enc_len <= max_enc);
}

TEST_MAIN("slip properties") {
    RUN(encode_decode_roundtrip);
    RUN(streaming_matches_block);
    RUN(decode_no_crash);
    RUN(streaming_feed_no_crash);
    RUN(encoded_size_bounded);
}
TEST_MAIN_END()
