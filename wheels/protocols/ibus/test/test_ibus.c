#include "test_harness.h"
#include "ibus.h"
#include <string.h>

static void build_valid_frame(uint8_t *buf, uint16_t ch0_val)
{
    ibus_frame_t f;
    memset(&f, 0, sizeof(f));
    f.channels[0] = ch0_val;
    for (int i = 1; i < IBUS_NUM_CHANNELS; i++)
        f.channels[i] = 1500;
    ibus_encode(&f, buf, IBUS_FRAME_LEN);
}

/* ---------- decode ---------- */

TEST(test_decode_ok)
{
    uint8_t buf[IBUS_FRAME_LEN];
    build_valid_frame(buf, 1500);
    ibus_frame_t f;
    ASSERT_EQ(ibus_decode(buf, IBUS_FRAME_LEN, &f), IBUS_OK);
    ASSERT_EQ(f.channels[0], 1500);
}

TEST(test_decode_all_channels)
{
    ibus_frame_t src;
    for (int i = 0; i < IBUS_NUM_CHANNELS; i++)
        src.channels[i] = (uint16_t)(1000 + i * 50);
    uint8_t buf[IBUS_FRAME_LEN];
    ibus_encode(&src, buf, IBUS_FRAME_LEN);

    ibus_frame_t dst;
    ASSERT_EQ(ibus_decode(buf, IBUS_FRAME_LEN, &dst), IBUS_OK);
    for (int i = 0; i < IBUS_NUM_CHANNELS; i++)
        ASSERT_EQ(dst.channels[i], src.channels[i]);
}

TEST(test_decode_null)
{
    ibus_frame_t f;
    ASSERT_EQ(ibus_decode(NULL, 32, &f), IBUS_ERR_PARAM);
    uint8_t buf[32];
    ASSERT_EQ(ibus_decode(buf, 32, NULL), IBUS_ERR_PARAM);
}

TEST(test_decode_short)
{
    uint8_t buf[16] = {0};
    ibus_frame_t f;
    ASSERT_EQ(ibus_decode(buf, 16, &f), IBUS_ERR_SHORT);
}

TEST(test_decode_bad_header_len)
{
    uint8_t buf[IBUS_FRAME_LEN];
    build_valid_frame(buf, 1500);
    buf[0] = 0x00;
    ibus_frame_t f;
    ASSERT_EQ(ibus_decode(buf, IBUS_FRAME_LEN, &f), IBUS_ERR_HEADER);
}

TEST(test_decode_bad_header_type)
{
    uint8_t buf[IBUS_FRAME_LEN];
    build_valid_frame(buf, 1500);
    buf[1] = 0x00;
    ibus_frame_t f;
    ASSERT_EQ(ibus_decode(buf, IBUS_FRAME_LEN, &f), IBUS_ERR_HEADER);
}

TEST(test_decode_bad_checksum)
{
    uint8_t buf[IBUS_FRAME_LEN];
    build_valid_frame(buf, 1500);
    buf[30] ^= 0xFF;
    ibus_frame_t f;
    ASSERT_EQ(ibus_decode(buf, IBUS_FRAME_LEN, &f), IBUS_ERR_CHECKSUM);
}

/* ---------- encode ---------- */

TEST(test_encode_ok)
{
    ibus_frame_t f;
    memset(&f, 0, sizeof(f));
    f.channels[0] = 1000;
    uint8_t buf[IBUS_FRAME_LEN];
    ASSERT_EQ(ibus_encode(&f, buf, IBUS_FRAME_LEN), IBUS_OK);
    ASSERT_EQ(buf[0], 0x20);
    ASSERT_EQ(buf[1], 0x40);
}

TEST(test_encode_null)
{
    uint8_t buf[32];
    ASSERT_EQ(ibus_encode(NULL, buf, 32), IBUS_ERR_PARAM);
    ibus_frame_t f = {0};
    ASSERT_EQ(ibus_encode(&f, NULL, 32), IBUS_ERR_PARAM);
}

TEST(test_encode_short_buf)
{
    ibus_frame_t f = {0};
    uint8_t buf[16];
    ASSERT_EQ(ibus_encode(&f, buf, 16), IBUS_ERR_SHORT);
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip)
{
    ibus_frame_t src;
    for (int i = 0; i < IBUS_NUM_CHANNELS; i++)
        src.channels[i] = (uint16_t)(1000 + i * 71);
    uint8_t buf[IBUS_FRAME_LEN];
    ASSERT_EQ(ibus_encode(&src, buf, IBUS_FRAME_LEN), IBUS_OK);
    ibus_frame_t dst;
    ASSERT_EQ(ibus_decode(buf, IBUS_FRAME_LEN, &dst), IBUS_OK);
    for (int i = 0; i < IBUS_NUM_CHANNELS; i++)
        ASSERT_EQ(dst.channels[i], src.channels[i]);
}

/* ---------- checksum ---------- */

TEST(test_checksum_empty)
{
    ASSERT_EQ(ibus_checksum(NULL, 0), 0xFFFF);
}

TEST(test_checksum_known)
{
    uint8_t data[] = {0x20, 0x40};
    uint16_t chk = ibus_checksum(data, 2);
    ASSERT_EQ(chk, (uint16_t)(0xFFFF - 0x20 - 0x40));
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)IBUS_OK, 0);
    ASSERT_EQ((int)IBUS_ERR_PARAM, -1);
    ASSERT_EQ((int)IBUS_ERR_SHORT, -2);
    ASSERT_EQ((int)IBUS_ERR_HEADER, -3);
    ASSERT_EQ((int)IBUS_ERR_CHECKSUM, -4);
}

/* ---------- fuzz ---------- */

TEST(test_fuzz)
{
    ibus_frame_t f;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[64];
        int len = (int)(seed % 65);
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        ibus_decode(buf, (size_t)len, &f);
    }
}

TEST_MAIN("ibus")
    RUN(test_decode_ok);
    RUN(test_decode_all_channels);
    RUN(test_decode_null);
    RUN(test_decode_short);
    RUN(test_decode_bad_header_len);
    RUN(test_decode_bad_header_type);
    RUN(test_decode_bad_checksum);
    RUN(test_encode_ok);
    RUN(test_encode_null);
    RUN(test_encode_short_buf);
    RUN(test_roundtrip);
    RUN(test_checksum_empty);
    RUN(test_checksum_known);
    RUN(test_error_codes);
    RUN(test_fuzz);
TEST_MAIN_END()
