#include "test_harness.h"
#include "uds.h"
#include <string.h>

/* ---------- decode request ---------- */

TEST(test_decode_request)
{
    uint8_t buf[] = {0x10, 0x01};  /* DiagSessionControl, default session */
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, 2, &msg), UDS_OK);
    ASSERT_EQ(msg.sid, 0x10);
    ASSERT_FALSE(msg.is_response);
    ASSERT_FALSE(msg.is_negative);
    ASSERT_EQ(msg.data_len, 1);
    ASSERT_EQ(msg.data[0], 0x01);
}

TEST(test_decode_read_data_by_id)
{
    uint8_t buf[] = {0x22, 0xF1, 0x90};  /* ReadDataByIdentifier, DID F190 */
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, 3, &msg), UDS_OK);
    ASSERT_EQ(msg.sid, 0x22);
    ASSERT_EQ(msg.data_len, 2);
}

/* ---------- decode positive response ---------- */

TEST(test_decode_positive_response)
{
    uint8_t buf[] = {0x50, 0x01};  /* DiagSessionControl response */
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, 2, &msg), UDS_OK);
    ASSERT_EQ(msg.sid, 0x10);
    ASSERT_TRUE(msg.is_response);
    ASSERT_FALSE(msg.is_negative);
}

TEST(test_decode_tester_present_response)
{
    uint8_t buf[] = {0x7E, 0x00};
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, 2, &msg), UDS_OK);
    ASSERT_EQ(msg.sid, 0x3E);
    ASSERT_TRUE(msg.is_response);
}

/* ---------- decode negative response ---------- */

TEST(test_decode_negative_response)
{
    uint8_t buf[] = {0x7F, 0x22, 0x31};  /* NR for ReadDataByID, requestOutOfRange */
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, 3, &msg), UDS_OK);
    ASSERT_TRUE(msg.is_negative);
    ASSERT_TRUE(msg.is_response);
    ASSERT_EQ(msg.rejected_sid, 0x22);
    ASSERT_EQ(msg.nrc, 0x31);
}

TEST(test_decode_negative_short)
{
    uint8_t buf[] = {0x7F, 0x22};
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, 2, &msg), UDS_ERR_SHORT);
}

/* ---------- decode errors ---------- */

TEST(test_decode_null)
{
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(NULL, 2, &msg), UDS_ERR_PARAM);
    uint8_t buf[] = {0x10};
    ASSERT_EQ(uds_decode(buf, 1, NULL), UDS_ERR_PARAM);
}

TEST(test_decode_empty)
{
    uint8_t buf[] = {0};
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, 0, &msg), UDS_ERR_SHORT);
}

/* ---------- encode request ---------- */

TEST(test_encode_request)
{
    uint8_t data[] = {0x01};
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(uds_encode_request(0x10, data, 1, buf, sizeof(buf), &len), UDS_OK);
    ASSERT_EQ(len, 2u);
    ASSERT_EQ(buf[0], 0x10);
    ASSERT_EQ(buf[1], 0x01);
}

TEST(test_encode_request_no_data)
{
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(uds_encode_request(0x3E, NULL, 0, buf, sizeof(buf), &len), UDS_OK);
    ASSERT_EQ(len, 1u);
    ASSERT_EQ(buf[0], 0x3E);
}

TEST(test_encode_request_null)
{
    size_t len;
    ASSERT_EQ(uds_encode_request(0x10, NULL, 0, NULL, 16, &len), UDS_ERR_PARAM);
    uint8_t buf[16];
    ASSERT_EQ(uds_encode_request(0x10, NULL, 0, buf, 16, NULL), UDS_ERR_PARAM);
}

TEST(test_encode_request_overflow)
{
    uint8_t data[] = {0x01};
    uint8_t buf[1];
    size_t len;
    ASSERT_EQ(uds_encode_request(0x10, data, 1, buf, 1, &len), UDS_ERR_OVERFLOW);
}

/* ---------- encode response ---------- */

TEST(test_encode_response)
{
    uint8_t data[] = {0x01};
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(uds_encode_response(0x10, data, 1, buf, sizeof(buf), &len), UDS_OK);
    ASSERT_EQ(buf[0], 0x50);
}

/* ---------- encode negative ---------- */

TEST(test_encode_negative)
{
    uint8_t buf[16];
    size_t len = 0;
    ASSERT_EQ(uds_encode_negative(0x22, 0x31, buf, sizeof(buf), &len), UDS_OK);
    ASSERT_EQ(len, 3u);
    ASSERT_EQ(buf[0], 0x7F);
    ASSERT_EQ(buf[1], 0x22);
    ASSERT_EQ(buf[2], 0x31);
}

TEST(test_encode_negative_null)
{
    size_t len;
    ASSERT_EQ(uds_encode_negative(0x22, 0x31, NULL, 16, &len), UDS_ERR_PARAM);
}

TEST(test_encode_negative_overflow)
{
    uint8_t buf[2];
    size_t len;
    ASSERT_EQ(uds_encode_negative(0x22, 0x31, buf, 2, &len), UDS_ERR_OVERFLOW);
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip_request)
{
    uint8_t data[] = {0xF1, 0x90};
    uint8_t buf[16];
    size_t len = 0;
    uds_encode_request(0x22, data, 2, buf, sizeof(buf), &len);
    uds_msg_t msg;
    ASSERT_EQ(uds_decode(buf, len, &msg), UDS_OK);
    ASSERT_EQ(msg.sid, 0x22);
    ASSERT_FALSE(msg.is_response);
    ASSERT_EQ(msg.data[0], 0xF1);
}

TEST(test_roundtrip_response)
{
    uint8_t data[] = {0xF1, 0x90, 0x41, 0x42};
    uint8_t buf[16];
    size_t len = 0;
    uds_encode_response(0x22, data, 4, buf, sizeof(buf), &len);
    uds_msg_t msg;
    uds_decode(buf, len, &msg);
    ASSERT_EQ(msg.sid, 0x22);
    ASSERT_TRUE(msg.is_response);
    ASSERT_EQ(msg.data_len, 4);
}

/* ---------- is_positive_response ---------- */

TEST(test_is_positive_response)
{
    ASSERT_TRUE(uds_is_positive_response(0x50));
    ASSERT_TRUE(uds_is_positive_response(0x62));
    ASSERT_FALSE(uds_is_positive_response(0x10));
    ASSERT_FALSE(uds_is_positive_response(0x7F));
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)UDS_OK, 0);
    ASSERT_EQ((int)UDS_ERR_PARAM, -1);
    ASSERT_EQ((int)UDS_ERR_FORMAT, -2);
    ASSERT_EQ((int)UDS_ERR_SHORT, -3);
    ASSERT_EQ((int)UDS_ERR_OVERFLOW, -4);
}

/* ---------- fuzz ---------- */

TEST(test_fuzz)
{
    uds_msg_t msg;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[64];
        int len = (int)(seed % 33);
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        uds_decode(buf, (size_t)len, &msg);
    }
}

TEST_MAIN("uds")
    RUN(test_decode_request);
    RUN(test_decode_read_data_by_id);
    RUN(test_decode_positive_response);
    RUN(test_decode_tester_present_response);
    RUN(test_decode_negative_response);
    RUN(test_decode_negative_short);
    RUN(test_decode_null);
    RUN(test_decode_empty);
    RUN(test_encode_request);
    RUN(test_encode_request_no_data);
    RUN(test_encode_request_null);
    RUN(test_encode_request_overflow);
    RUN(test_encode_response);
    RUN(test_encode_negative);
    RUN(test_encode_negative_null);
    RUN(test_encode_negative_overflow);
    RUN(test_roundtrip_request);
    RUN(test_roundtrip_response);
    RUN(test_is_positive_response);
    RUN(test_error_codes);
    RUN(test_fuzz);
TEST_MAIN_END()
