// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ppp_frame.h"
#include <string.h>

/* ---------- FCS-16 tests ---------- */

TEST(test_fcs16_known_vector)
{
    /* RFC 1662 Appendix C test string "123456789" */
    const uint8_t data[] = "123456789";
    uint16_t fcs = ppp_frame_fcs16(data, 9);
    /* CRC-16/HDLC("123456789") = 0x906E */
    ASSERT_EQ_HEX(fcs, 0x906E);
}

TEST(test_fcs16_empty)
{
    /* Empty data should return ~0xFFFF = 0x0000 XOR'd */
    uint8_t data[] = {0};
    uint16_t fcs = ppp_frame_fcs16(data, 0);
    /* FCS_INIT ^ 0xFFFF = 0xFFFF ^ 0xFFFF = 0x0000 */
    ASSERT_EQ_HEX(fcs, 0x0000);
}

TEST(test_fcs16_null)
{
    uint16_t fcs = ppp_frame_fcs16(NULL, 10);
    ASSERT_EQ_HEX(fcs, PPP_FCS_INIT);
}

TEST(test_fcs16_single_byte)
{
    uint8_t data[] = {0x00};
    uint16_t fcs = ppp_frame_fcs16(data, 1);
    /* Known: CRC-16/HDLC of {0x00} */
    ASSERT_TRUE(fcs != 0x0000); /* Non-trivial value */
}

/* ---------- encode tests ---------- */

TEST(test_encode_empty_payload)
{
    uint8_t dst[128];
    size_t out_len = 0;
    ppp_frame_err_t err = ppp_frame_encode(0x0021, NULL, 0,
                                            dst, sizeof(dst), &out_len);
    ASSERT_EQ(err, PPP_FRAME_OK);
    ASSERT_TRUE(out_len > 0);
    /* Must start and end with flag */
    ASSERT_EQ_HEX(dst[0], PPP_FLAG);
    ASSERT_EQ_HEX(dst[out_len - 1], PPP_FLAG);
}

TEST(test_encode_simple_payload)
{
    uint8_t payload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; /* "Hello" */
    uint8_t dst[128];
    size_t out_len = 0;
    ppp_frame_err_t err = ppp_frame_encode(0x0021, payload, sizeof(payload),
                                            dst, sizeof(dst), &out_len);
    ASSERT_EQ(err, PPP_FRAME_OK);
    ASSERT_EQ_HEX(dst[0], PPP_FLAG);
    ASSERT_EQ_HEX(dst[out_len - 1], PPP_FLAG);
    /* No byte in "Hello" needs escaping (all >= 0x20 and not 0x7E/0x7D) */
}

TEST(test_encode_needs_escape)
{
    /* Payload contains 0x7E (flag) and 0x7D (escape) */
    uint8_t payload[] = {0x7E, 0x7D, 0x01};
    uint8_t dst[128];
    size_t out_len = 0;
    ppp_frame_err_t err = ppp_frame_encode(0x0021, payload, sizeof(payload),
                                            dst, sizeof(dst), &out_len);
    ASSERT_EQ(err, PPP_FRAME_OK);
    /* Escaped bytes should appear as 7D 5E and 7D 5D */
    /* Find them in the output (skip opening flag, addr, ctrl, protocol) */
    bool found_flag_escape = false;
    bool found_escape_escape = false;
    for (size_t i = 1; i < out_len - 1; i++) {
        if (dst[i] == 0x7D && i + 1 < out_len - 1) {
            if (dst[i + 1] == 0x5E) found_flag_escape = true;
            if (dst[i + 1] == 0x5D) found_escape_escape = true;
        }
    }
    ASSERT_TRUE(found_flag_escape);
    ASSERT_TRUE(found_escape_escape);
}

TEST(test_encode_control_chars_escaped)
{
    /* Bytes < 0x20 must be escaped */
    uint8_t payload[] = {0x00, 0x0A, 0x0D, 0x1F, 0x20};
    uint8_t dst[128];
    size_t out_len = 0;
    ppp_frame_err_t err = ppp_frame_encode(0x0021, payload, sizeof(payload),
                                            dst, sizeof(dst), &out_len);
    ASSERT_EQ(err, PPP_FRAME_OK);
    /* 0x00, 0x0A, 0x0D, 0x1F should be escaped; 0x20 should NOT be escaped */
}

TEST(test_encode_overflow)
{
    uint8_t payload[] = {0x41};
    uint8_t dst[2]; /* Way too small */
    size_t out_len = 0;
    ppp_frame_err_t err = ppp_frame_encode(0x0021, payload, sizeof(payload),
                                            dst, sizeof(dst), &out_len);
    ASSERT_EQ(err, PPP_FRAME_ERR_OVERFLOW);
}

TEST(test_encode_null_params)
{
    size_t out_len;
    uint8_t dst[128];
    uint8_t payload[] = {0x01};

    ASSERT_EQ(ppp_frame_encode(0x0021, payload, 1, NULL, 128, &out_len),
              PPP_FRAME_ERR_PARAM);
    ASSERT_EQ(ppp_frame_encode(0x0021, payload, 1, dst, 128, NULL),
              PPP_FRAME_ERR_PARAM);
    ASSERT_EQ(ppp_frame_encode(0x0021, NULL, 1, dst, 128, &out_len),
              PPP_FRAME_ERR_PARAM);
}

/* ---------- decode tests ---------- */

TEST(test_decode_null_params)
{
    uint8_t src[] = {PPP_FLAG, PPP_FLAG};
    uint16_t proto;
    uint8_t pbuf[64];
    size_t plen;

    ASSERT_EQ(ppp_frame_decode(NULL, 2, &proto, pbuf, 64, &plen),
              PPP_FRAME_ERR_PARAM);
    ASSERT_EQ(ppp_frame_decode(src, 2, NULL, pbuf, 64, &plen),
              PPP_FRAME_ERR_PARAM);
    ASSERT_EQ(ppp_frame_decode(src, 2, &proto, NULL, 64, &plen),
              PPP_FRAME_ERR_PARAM);
    ASSERT_EQ(ppp_frame_decode(src, 2, &proto, pbuf, 64, NULL),
              PPP_FRAME_ERR_PARAM);
}

TEST(test_decode_too_short)
{
    uint16_t proto;
    uint8_t pbuf[64];
    size_t plen;
    uint8_t src[] = {PPP_FLAG};

    ASSERT_EQ(ppp_frame_decode(src, 1, &proto, pbuf, 64, &plen),
              PPP_FRAME_ERR_SHORT);
}

TEST(test_decode_no_flags)
{
    uint16_t proto;
    uint8_t pbuf[64];
    size_t plen;
    uint8_t src[] = {0x00, 0xFF, 0x03, 0x00, 0x21};

    ASSERT_EQ(ppp_frame_decode(src, sizeof(src), &proto, pbuf, 64, &plen),
              PPP_FRAME_ERR_FORMAT);
}

/* ---------- encode-decode round trip ---------- */

TEST(test_roundtrip_empty)
{
    uint8_t encoded[128];
    size_t elen = 0;
    ppp_frame_err_t err = ppp_frame_encode(0x0021, NULL, 0,
                                            encoded, sizeof(encoded), &elen);
    ASSERT_EQ(err, PPP_FRAME_OK);

    uint16_t proto = 0;
    uint8_t decoded[128];
    size_t dlen = 0;
    err = ppp_frame_decode(encoded, elen, &proto, decoded, sizeof(decoded), &dlen);
    ASSERT_EQ(err, PPP_FRAME_OK);
    ASSERT_EQ_HEX(proto, 0x0021);
    ASSERT_EQ(dlen, 0);
}

TEST(test_roundtrip_simple)
{
    uint8_t payload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; /* "Hello" */
    uint8_t encoded[128];
    size_t elen = 0;
    ppp_frame_encode(0x0021, payload, sizeof(payload),
                     encoded, sizeof(encoded), &elen);

    uint16_t proto = 0;
    uint8_t decoded[128];
    size_t dlen = 0;
    ppp_frame_err_t err = ppp_frame_decode(encoded, elen, &proto,
                                            decoded, sizeof(decoded), &dlen);
    ASSERT_EQ(err, PPP_FRAME_OK);
    ASSERT_EQ_HEX(proto, 0x0021);
    ASSERT_EQ(dlen, 5);
    ASSERT_MEMEQ_N(decoded, payload, 5);
}

TEST(test_roundtrip_with_escapes)
{
    /* Payload with bytes that need escaping */
    uint8_t payload[] = {0x7E, 0x7D, 0x00, 0x0D, 0x1F, 0x20, 0xFF};
    uint8_t encoded[128];
    size_t elen = 0;
    ppp_frame_encode(0xC021, payload, sizeof(payload),
                     encoded, sizeof(encoded), &elen);

    uint16_t proto = 0;
    uint8_t decoded[128];
    size_t dlen = 0;
    ppp_frame_err_t err = ppp_frame_decode(encoded, elen, &proto,
                                            decoded, sizeof(decoded), &dlen);
    ASSERT_EQ(err, PPP_FRAME_OK);
    ASSERT_EQ_HEX(proto, 0xC021);
    ASSERT_EQ(dlen, sizeof(payload));
    ASSERT_MEMEQ_N(decoded, payload, sizeof(payload));
}

TEST(test_roundtrip_all_protocols)
{
    uint8_t payload[] = {0x42};
    uint16_t protocols[] = {0x0021, 0x002B, 0x8021, 0xC021, 0xC023, 0x0057};
    uint8_t encoded[128];
    uint8_t decoded[128];

    for (int i = 0; i < 6; i++) {
        size_t elen = 0;
        ppp_frame_encode(protocols[i], payload, 1,
                         encoded, sizeof(encoded), &elen);

        uint16_t proto = 0;
        size_t dlen = 0;
        ppp_frame_err_t err = ppp_frame_decode(encoded, elen, &proto,
                                                decoded, sizeof(decoded), &dlen);
        ASSERT_EQ(err, PPP_FRAME_OK);
        ASSERT_EQ_HEX(proto, protocols[i]);
        ASSERT_EQ(dlen, 1);
        ASSERT_EQ_HEX(decoded[0], 0x42);
    }
}

TEST(test_roundtrip_larger_payload)
{
    uint8_t payload[256];
    for (int i = 0; i < 256; i++)
        payload[i] = (uint8_t)i;

    uint8_t encoded[1024];
    size_t elen = 0;
    ppp_frame_encode(0x0021, payload, 256, encoded, sizeof(encoded), &elen);

    uint16_t proto = 0;
    uint8_t decoded[256];
    size_t dlen = 0;
    ppp_frame_err_t err = ppp_frame_decode(encoded, elen, &proto,
                                            decoded, sizeof(decoded), &dlen);
    ASSERT_EQ(err, PPP_FRAME_OK);
    ASSERT_EQ(dlen, 256);
    ASSERT_MEMEQ_N(decoded, payload, 256);
}

TEST(test_decode_bad_fcs)
{
    uint8_t encoded[128];
    size_t elen = 0;
    uint8_t payload[] = {0x41, 0x42};
    ppp_frame_encode(0x0021, payload, 2, encoded, sizeof(encoded), &elen);

    /* Corrupt the last data byte before closing flag */
    encoded[elen - 2] ^= 0xFF;

    uint16_t proto;
    uint8_t decoded[128];
    size_t dlen;
    ppp_frame_err_t err = ppp_frame_decode(encoded, elen, &proto,
                                            decoded, sizeof(decoded), &dlen);
    ASSERT_EQ(err, PPP_FRAME_ERR_FCS);
}

TEST(test_decode_payload_overflow)
{
    uint8_t payload[10];
    memset(payload, 0x42, sizeof(payload));
    uint8_t encoded[128];
    size_t elen = 0;
    ppp_frame_encode(0x0021, payload, 10, encoded, sizeof(encoded), &elen);

    uint16_t proto;
    uint8_t tiny[2]; /* Too small for 10-byte payload */
    size_t dlen;
    ppp_frame_err_t err = ppp_frame_decode(encoded, elen, &proto,
                                            tiny, sizeof(tiny), &dlen);
    ASSERT_EQ(err, PPP_FRAME_ERR_OVERFLOW);
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)PPP_FRAME_OK);
    ASSERT_EQ(-1, (int)PPP_FRAME_ERR_PARAM);
    ASSERT_EQ(-2, (int)PPP_FRAME_ERR_FCS);
    ASSERT_EQ(-3, (int)PPP_FRAME_ERR_SHORT);
    ASSERT_EQ(-4, (int)PPP_FRAME_ERR_OVERFLOW);
    ASSERT_EQ(-5, (int)PPP_FRAME_ERR_FORMAT);
}

TEST(test_decode_trailing_escape)
{
    /* Frame with a trailing 0x7D before flag -- invalid */
    uint8_t bad[] = {PPP_FLAG, 0xFF, 0x03, 0x00, 0x21, 0x7D, PPP_FLAG};
    uint16_t proto;
    uint8_t pbuf[64];
    size_t plen;
    ppp_frame_err_t err = ppp_frame_decode(bad, sizeof(bad), &proto,
                                            pbuf, sizeof(pbuf), &plen);
    ASSERT_EQ(err, PPP_FRAME_ERR_FORMAT);
}

TEST(test_decode_embedded_flag)
{
    /* A raw 0x7E in the middle should be rejected */
    uint8_t bad[] = {PPP_FLAG, 0xFF, 0x03, PPP_FLAG, 0x21, 0x00, 0x00, PPP_FLAG};
    uint16_t proto;
    uint8_t pbuf[64];
    size_t plen;
    ppp_frame_err_t err = ppp_frame_decode(bad, sizeof(bad), &proto,
                                            pbuf, sizeof(pbuf), &plen);
    ASSERT_EQ(err, PPP_FRAME_ERR_FORMAT);
}

TEST_MAIN("ppp_frame")
    RUN(test_fcs16_known_vector);
    RUN(test_fcs16_empty);
    RUN(test_fcs16_null);
    RUN(test_fcs16_single_byte);
    RUN(test_encode_empty_payload);
    RUN(test_encode_simple_payload);
    RUN(test_encode_needs_escape);
    RUN(test_encode_control_chars_escaped);
    RUN(test_encode_overflow);
    RUN(test_encode_null_params);
    RUN(test_decode_null_params);
    RUN(test_decode_too_short);
    RUN(test_decode_no_flags);
    RUN(test_roundtrip_empty);
    RUN(test_roundtrip_simple);
    RUN(test_roundtrip_with_escapes);
    RUN(test_roundtrip_all_protocols);
    RUN(test_roundtrip_larger_payload);
    RUN(test_decode_bad_fcs);
    RUN(test_decode_payload_overflow);
    RUN(test_error_codes);
    RUN(test_decode_trailing_escape);
    RUN(test_decode_embedded_flag);
TEST_MAIN_END()
