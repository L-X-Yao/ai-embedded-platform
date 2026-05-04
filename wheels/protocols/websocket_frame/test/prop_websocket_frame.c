/*
 * prop_websocket_frame.c — property-based tests for websocket_frame
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_websocket_frame.c ../websocket_frame.c -o prop_websocket_frame
 */

#include "test_harness.h"
#include "property_test.h"
#include "../websocket_frame.h"
#include <string.h>

/* Property: build then parse roundtrip (unmasked) preserves fields */
PROPERTY(unmasked_roundtrip, 10000) {
    static const websocket_frame_opcode_t opcodes[] = {
        WEBSOCKET_FRAME_OPCODE_TEXT,
        WEBSOCKET_FRAME_OPCODE_BINARY,
        WEBSOCKET_FRAME_OPCODE_PING,
        WEBSOCKET_FRAME_OPCODE_PONG,
    };
    websocket_frame_opcode_t opcode = opcodes[ARBI_UINT8(seed) % 4u];

    uint8_t payload[128];
    /* Control frames (ping/pong) max 125 bytes per spec */
    uint8_t max_pl = (opcode >= WEBSOCKET_FRAME_OPCODE_CLOSE) ? 125 : 128;
    uint8_t payload_len = (uint8_t)ARBI_RANGE(seed, 0, max_pl);
    ARBI_FILL(seed, payload, payload_len);

    bool fin = (ARBI_UINT8(seed) & 1) || (opcode >= WEBSOCKET_FRAME_OPCODE_CLOSE);

    uint8_t buf[512];
    size_t out_len = 0;
    websocket_frame_err_t err = websocket_frame_build(
        buf, sizeof(buf), &out_len,
        fin, opcode, NULL, /* no mask */
        payload, payload_len);
    PROP_ASSERT_EQ(err, WEBSOCKET_FRAME_OK);
    PROP_ASSERT(out_len > 0);

    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    err = websocket_frame_parse(buf, out_len, &frame);
    PROP_ASSERT_EQ(err, WEBSOCKET_FRAME_OK);
    PROP_ASSERT_EQ(frame.fin, fin);
    PROP_ASSERT_EQ(frame.opcode, opcode);
    PROP_ASSERT_EQ(frame.masked, false);
    PROP_ASSERT_EQ(frame.payload_len, payload_len);
    if (payload_len > 0) {
        PROP_ASSERT_MEM_EQ(frame.payload, payload, payload_len);
    }
    PROP_ASSERT_EQ(frame.frame_len, out_len);
}

/* Property: build then parse roundtrip (masked) preserves payload after unmask */
PROPERTY(masked_roundtrip, 10000) {
    uint8_t payload[64];
    uint8_t payload_len = (uint8_t)ARBI_RANGE(seed, 0, 64);
    ARBI_FILL(seed, payload, payload_len);

    uint8_t mask_key[4];
    ARBI_FILL(seed, mask_key, 4);

    uint8_t buf[256];
    size_t out_len = 0;
    websocket_frame_err_t err = websocket_frame_build(
        buf, sizeof(buf), &out_len,
        true, WEBSOCKET_FRAME_OPCODE_BINARY, mask_key,
        payload, payload_len);
    PROP_ASSERT_EQ(err, WEBSOCKET_FRAME_OK);

    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    err = websocket_frame_parse(buf, out_len, &frame);
    PROP_ASSERT_EQ(err, WEBSOCKET_FRAME_OK);
    PROP_ASSERT_EQ(frame.masked, true);
    PROP_ASSERT_EQ(frame.payload_len, payload_len);

    /* Unmask the payload to recover original */
    if (payload_len > 0) {
        uint8_t unmasked[64];
        memcpy(unmasked, frame.payload, payload_len);
        websocket_frame_mask_payload(unmasked, payload_len, frame.mask_key);
        PROP_ASSERT_MEM_EQ(unmasked, payload, payload_len);
    }
}

/* Property: build is deterministic */
PROPERTY(build_determinism, 5000) {
    uint8_t payload[32];
    uint8_t plen = (uint8_t)ARBI_RANGE(seed, 0, 32);
    ARBI_FILL(seed, payload, plen);

    uint8_t buf1[128], buf2[128];
    size_t len1 = 0, len2 = 0;
    websocket_frame_build(buf1, sizeof(buf1), &len1,
                          true, WEBSOCKET_FRAME_OPCODE_TEXT,
                          NULL, payload, plen);
    websocket_frame_build(buf2, sizeof(buf2), &len2,
                          true, WEBSOCKET_FRAME_OPCODE_TEXT,
                          NULL, payload, plen);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, len1);
}

/* Property: mask then unmask is identity */
PROPERTY(mask_unmask_identity, 10000) {
    uint8_t data[128];
    uint8_t len = (uint8_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, data, len);

    uint8_t copy[128];
    memcpy(copy, data, len);

    uint8_t mask_key[4];
    ARBI_FILL(seed, mask_key, 4);

    /* Mask then unmask should return to original */
    websocket_frame_mask_payload(copy, len, mask_key);
    websocket_frame_mask_payload(copy, len, mask_key);
    PROP_ASSERT_MEM_EQ(copy, data, len);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    uint8_t garbage[300];
    size_t len = ARBI_SIZE(seed, 300);
    ARBI_FILL(seed, garbage, len);

    websocket_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    websocket_frame_parse(garbage, len, &frame);
}

TEST_MAIN("websocket_frame properties") {
    RUN(unmasked_roundtrip);
    RUN(masked_roundtrip);
    RUN(build_determinism);
    RUN(mask_unmask_identity);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
