#include "test_harness.h"
#include "dhcp_packet.h"
#include <string.h>

TEST(test_parse_ok)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    dhcp_packet_msg_t msg;
    ASSERT_EQ(dhcp_packet_parse(data, sizeof(data), &msg), DHCP_PACKET_OK);
}

TEST(test_parse_null)
{
    dhcp_packet_msg_t msg;
    ASSERT_EQ(dhcp_packet_parse(NULL, 0, &msg), DHCP_PACKET_ERR_PARAM);
    uint8_t data[] = {0x01};
    ASSERT_EQ(dhcp_packet_parse(data, 1, NULL), DHCP_PACKET_ERR_PARAM);
}

TEST(test_parse_short)
{
    uint8_t data[] = {0x01};
    dhcp_packet_msg_t msg;
    ASSERT_EQ(dhcp_packet_parse(data, 1, &msg), DHCP_PACKET_ERR_SHORT);
}

TEST(test_build_ok)
{
    dhcp_packet_msg_t msg = { .type = 0x01, .length = 2 };
    uint8_t buf[32];
    int n = dhcp_packet_build(buf, sizeof(buf), &msg);
    ASSERT_TRUE(n > 0);
}

TEST(test_build_null)
{
    dhcp_packet_msg_t msg = { .type = 0x01 };
    ASSERT_EQ(dhcp_packet_build(NULL, 32, &msg), -1);
    uint8_t buf[32];
    ASSERT_EQ(dhcp_packet_build(buf, 32, NULL), -1);
}

TEST(test_build_overflow)
{
    dhcp_packet_msg_t msg = { .type = 0x01 };
    uint8_t buf[1];
    ASSERT_EQ(dhcp_packet_build(buf, 1, &msg), -1);
}

TEST(test_parse_adversarial)
{
    dhcp_packet_msg_t msg;
    /* All zeros */
    uint8_t zeros[64];
    memset(zeros, 0, sizeof(zeros));
    dhcp_packet_parse(zeros, sizeof(zeros), &msg);

    /* All 0xFF */
    uint8_t ones[64];
    memset(ones, 0xFF, sizeof(ones));
    dhcp_packet_parse(ones, sizeof(ones), &msg);

    /* Boundary lengths: 0, 1, 255, 256 */
    uint8_t big[256];
    memset(big, 0xAA, sizeof(big));
    dhcp_packet_parse(big, 0, &msg);
    dhcp_packet_parse(big, 1, &msg);
    dhcp_packet_parse(big, 255, &msg);
    dhcp_packet_parse(big, 256, &msg);

    /* Must not crash on any input — return value irrelevant */
}

TEST(test_parse_mini_fuzz)
{
    dhcp_packet_msg_t msg;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[128];
        int len = (int)(seed % 129);
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        dhcp_packet_parse(buf, (size_t)len, &msg);
    }
    /* 1000 random inputs — must not crash */
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)DHCP_PACKET_OK);
    ASSERT_EQ(-1, (int)DHCP_PACKET_ERR_PARAM);
    ASSERT_EQ(-2, (int)DHCP_PACKET_ERR_FORMAT);
    ASSERT_EQ(-3, (int)DHCP_PACKET_ERR_SHORT);
    ASSERT_EQ(-4, (int)DHCP_PACKET_ERR_OVERFLOW);
}

TEST_MAIN("dhcp_packet")
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
