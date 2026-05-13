#include "test_harness.h"
#include "modbus_ascii.h"
#include <string.h>

/* ---------- LRC ---------- */

TEST(test_lrc_known)
{
    /* Addr=0x01, Func=0x03, Data=0x00,0x00,0x00,0x0A → LRC=0xF2 */
    uint8_t bin[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint8_t lrc = modbus_ascii_lrc(bin, sizeof(bin));
    ASSERT_EQ(lrc, 0xF2);
}

TEST(test_lrc_null)
{
    ASSERT_EQ(modbus_ascii_lrc(NULL, 5), 0);
}

TEST(test_lrc_single)
{
    uint8_t b = 0x01;
    ASSERT_EQ(modbus_ascii_lrc(&b, 1), 0xFF);
}

/* ---------- decode ---------- */

TEST(test_decode_read_holding)
{
    /* :0103000000 0AF2\r\n → addr=1, func=3, data={0,0,0,10} */
    const char *frame = ":0103000000 0AF2\r\n";
    /* Remove space for proper frame */
    const char *proper = ":01030000000AF2\r\n";
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode(proper, strlen(proper), &f), MODBUS_ASCII_OK);
    ASSERT_EQ(f.addr, 0x01);
    ASSERT_EQ(f.func, 0x03);
    ASSERT_EQ(f.data_len, 4);
    ASSERT_EQ(f.data[3], 0x0A);
    (void)frame;
}

TEST(test_decode_no_data)
{
    /* addr=0x01, func=0x06, no data, LRC=0xF9 */
    uint8_t bin[] = {0x01, 0x06};
    uint8_t lrc = modbus_ascii_lrc(bin, 2);
    char frame[32];
    int n = snprintf(frame, sizeof(frame), ":0106%02X\r\n", lrc);
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode(frame, (size_t)n, &f), MODBUS_ASCII_OK);
    ASSERT_EQ(f.addr, 0x01);
    ASSERT_EQ(f.func, 0x06);
    ASSERT_EQ(f.data_len, 0);
}

TEST(test_decode_null)
{
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode(NULL, 10, &f), MODBUS_ASCII_ERR_PARAM);
    ASSERT_EQ(modbus_ascii_decode(":01030AF2\r\n", 11, NULL), MODBUS_ASCII_ERR_PARAM);
}

TEST(test_decode_short)
{
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode(":01\r\n", 5, &f), MODBUS_ASCII_ERR_SHORT);
}

TEST(test_decode_no_colon)
{
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode("01030AF2\r\n", 10, &f), MODBUS_ASCII_ERR_FORMAT);
}

TEST(test_decode_no_crlf)
{
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode(":0106F9XX", 9, &f), MODBUS_ASCII_ERR_FORMAT);
}

TEST(test_decode_bad_lrc)
{
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode(":0106FF\r\n", 9, &f), MODBUS_ASCII_ERR_LRC);
}

TEST(test_decode_bad_hex)
{
    modbus_ascii_frame_t f;
    ASSERT_EQ(modbus_ascii_decode(":01XXFF\r\n", 9, &f), MODBUS_ASCII_ERR_FORMAT);
}

/* ---------- encode ---------- */

TEST(test_encode_ok)
{
    modbus_ascii_frame_t f = { .addr = 0x01, .func = 0x03, .data_len = 0 };
    char buf[64];
    size_t len = 0;
    ASSERT_EQ(modbus_ascii_encode(&f, buf, sizeof(buf), &len), MODBUS_ASCII_OK);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[0], ':');
    ASSERT_EQ(buf[len - 2], '\r');
    ASSERT_EQ(buf[len - 1], '\n');
}

TEST(test_encode_null)
{
    char buf[64];
    size_t len = 0;
    modbus_ascii_frame_t f = {0};
    ASSERT_EQ(modbus_ascii_encode(NULL, buf, 64, &len), MODBUS_ASCII_ERR_PARAM);
    ASSERT_EQ(modbus_ascii_encode(&f, NULL, 64, &len), MODBUS_ASCII_ERR_PARAM);
    ASSERT_EQ(modbus_ascii_encode(&f, buf, 64, NULL), MODBUS_ASCII_ERR_PARAM);
}

TEST(test_encode_overflow)
{
    modbus_ascii_frame_t f = { .addr = 0x01, .func = 0x03, .data_len = 4 };
    char buf[4];
    size_t len = 0;
    ASSERT_EQ(modbus_ascii_encode(&f, buf, sizeof(buf), &len), MODBUS_ASCII_ERR_OVERFLOW);
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip)
{
    modbus_ascii_frame_t src = { .addr = 0x0A, .func = 0x10, .data_len = 3 };
    src.data[0] = 0xDE;
    src.data[1] = 0xAD;
    src.data[2] = 0x42;
    char buf[128];
    size_t len = 0;
    ASSERT_EQ(modbus_ascii_encode(&src, buf, sizeof(buf), &len), MODBUS_ASCII_OK);

    modbus_ascii_frame_t dst;
    ASSERT_EQ(modbus_ascii_decode(buf, len, &dst), MODBUS_ASCII_OK);
    ASSERT_EQ(dst.addr, 0x0A);
    ASSERT_EQ(dst.func, 0x10);
    ASSERT_EQ(dst.data_len, 3);
    ASSERT_EQ(dst.data[0], 0xDE);
    ASSERT_EQ(dst.data[2], 0x42);
}

TEST(test_roundtrip_max_data)
{
    modbus_ascii_frame_t src = { .addr = 0xFF, .func = 0x01, .data_len = 10 };
    for (int i = 0; i < 10; i++)
        src.data[i] = (uint8_t)i;
    char buf[256];
    size_t len = 0;
    modbus_ascii_encode(&src, buf, sizeof(buf), &len);
    modbus_ascii_frame_t dst;
    ASSERT_EQ(modbus_ascii_decode(buf, len, &dst), MODBUS_ASCII_OK);
    ASSERT_EQ(dst.data_len, 10);
    for (int i = 0; i < 10; i++)
        ASSERT_EQ(dst.data[i], (uint8_t)i);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)MODBUS_ASCII_OK, 0);
    ASSERT_EQ((int)MODBUS_ASCII_ERR_PARAM, -1);
    ASSERT_EQ((int)MODBUS_ASCII_ERR_FORMAT, -2);
    ASSERT_EQ((int)MODBUS_ASCII_ERR_SHORT, -3);
    ASSERT_EQ((int)MODBUS_ASCII_ERR_OVERFLOW, -4);
    ASSERT_EQ((int)MODBUS_ASCII_ERR_LRC, -5);
}

/* ---------- fuzz ---------- */

TEST(test_fuzz)
{
    modbus_ascii_frame_t f;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        char buf[128];
        int len = (int)(seed % 65);
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (char)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        modbus_ascii_decode(buf, (size_t)len, &f);
    }
}

TEST_MAIN("modbus_ascii")
    RUN(test_lrc_known);
    RUN(test_lrc_null);
    RUN(test_lrc_single);
    RUN(test_decode_read_holding);
    RUN(test_decode_no_data);
    RUN(test_decode_null);
    RUN(test_decode_short);
    RUN(test_decode_no_colon);
    RUN(test_decode_no_crlf);
    RUN(test_decode_bad_lrc);
    RUN(test_decode_bad_hex);
    RUN(test_encode_ok);
    RUN(test_encode_null);
    RUN(test_encode_overflow);
    RUN(test_roundtrip);
    RUN(test_roundtrip_max_data);
    RUN(test_error_codes);
    RUN(test_fuzz);
TEST_MAIN_END()
