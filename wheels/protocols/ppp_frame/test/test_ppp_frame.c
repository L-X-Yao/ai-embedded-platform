#include "test_harness.h"
#include "ppp_frame.h"
#include <string.h>

TEST(test_parse_ok)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    ppp_frame_msg_t msg;
    ASSERT_EQ(ppp_frame_parse(data, sizeof(data), &msg), PPP_FRAME_OK);
}

TEST(test_parse_null)
{
    ppp_frame_msg_t msg;
    ASSERT_EQ(ppp_frame_parse(NULL, 0, &msg), PPP_FRAME_ERR_PARAM);
    uint8_t data[] = {0x01};
    ASSERT_EQ(ppp_frame_parse(data, 1, NULL), PPP_FRAME_ERR_PARAM);
}

TEST(test_parse_short)
{
    uint8_t data[] = {0x01};
    ppp_frame_msg_t msg;
    ASSERT_EQ(ppp_frame_parse(data, 1, &msg), PPP_FRAME_ERR_SHORT);
}

TEST(test_build_ok)
{
    ppp_frame_msg_t msg = { .type = 0x01, .length = 2 };
    uint8_t buf[32];
    int n = ppp_frame_build(buf, sizeof(buf), &msg);
    ASSERT_TRUE(n > 0);
}

TEST(test_build_null)
{
    ppp_frame_msg_t msg = { .type = 0x01 };
    ASSERT_EQ(ppp_frame_build(NULL, 32, &msg), -1);
    uint8_t buf[32];
    ASSERT_EQ(ppp_frame_build(buf, 32, NULL), -1);
}

TEST(test_build_overflow)
{
    ppp_frame_msg_t msg = { .type = 0x01 };
    uint8_t buf[1];
    ASSERT_EQ(ppp_frame_build(buf, 1, &msg), -1);
}

TEST(test_parse_adversarial)
{
    ppp_frame_msg_t msg;
    /* All zeros */
    uint8_t zeros[64];
    memset(zeros, 0, sizeof(zeros));
    ppp_frame_parse(zeros, sizeof(zeros), &msg);

    /* All 0xFF */
    uint8_t ones[64];
    memset(ones, 0xFF, sizeof(ones));
    ppp_frame_parse(ones, sizeof(ones), &msg);

    /* Boundary lengths: 0, 1, 255, 256 */
    uint8_t big[256];
    memset(big, 0xAA, sizeof(big));
    ppp_frame_parse(big, 0, &msg);
    ppp_frame_parse(big, 1, &msg);
    ppp_frame_parse(big, 255, &msg);
    ppp_frame_parse(big, 256, &msg);

    /* Must not crash on any input — return value irrelevant */
}

TEST(test_parse_mini_fuzz)
{
    ppp_frame_msg_t msg;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[128];
        int len = (int)(seed % 129);
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        ppp_frame_parse(buf, (size_t)len, &msg);
    }
    /* 1000 random inputs — must not crash */
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)PPP_FRAME_OK);
    ASSERT_EQ(-1, (int)PPP_FRAME_ERR_PARAM);
    ASSERT_EQ(-2, (int)PPP_FRAME_ERR_FORMAT);
    ASSERT_EQ(-3, (int)PPP_FRAME_ERR_SHORT);
    ASSERT_EQ(-4, (int)PPP_FRAME_ERR_OVERFLOW);
}

TEST_MAIN("ppp_frame")
    RUN(test_parse_ok);
    RUN(test_parse_null);
    RUN(test_parse_short);
    RUN(test_build_ok);
    RUN(test_build_null);
    RUN(test_build_overflow);
    RUN(test_parse_adversarial);
    RUN(test_parse_mini_fuzz);
    RUN(test_error_codes);
TEST_MAIN_END()
