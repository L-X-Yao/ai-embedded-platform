#include "test_harness.h"
#include "syslog_packet.h"
#include <string.h>

/* ---- syslog_pri tests ------------------------------------------------ */

TEST(test_pri_kern_emerg)
{
    ASSERT_EQ(syslog_pri(SYSLOG_FAC_KERN, SYSLOG_SEV_EMERG), 0);
}

TEST(test_pri_user_info)
{
    /* user(1)*8 + info(6) = 14 */
    ASSERT_EQ(syslog_pri(SYSLOG_FAC_USER, SYSLOG_SEV_INFO), 14);
}

TEST(test_pri_local0_debug)
{
    /* local0(16)*8 + debug(7) = 135 */
    ASSERT_EQ(syslog_pri(SYSLOG_FAC_LOCAL0, SYSLOG_SEV_DEBUG), 135);
}

TEST(test_pri_local7_emerg)
{
    /* local7(23)*8 + emerg(0) = 184 */
    ASSERT_EQ(syslog_pri(SYSLOG_FAC_LOCAL7, SYSLOG_SEV_EMERG), 184);
}

/* ---- syslog_format tests --------------------------------------------- */

TEST(test_format_all_fields)
{
    syslog_msg_t msg = {
        .facility  = SYSLOG_FAC_LOCAL0,
        .severity  = SYSLOG_SEV_INFO,
        .hostname  = "myhost",
        .app_name  = "myapp",
        .proc_id   = "1234",
        .msg_id    = "ID42",
        .message   = "hello world",
        .timestamp = 1700000000,  /* 2023-11-14T22:13:20Z */
    };
    char buf[256];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* Expect: <134>1 2023-11-14T22:13:20Z myhost myapp 1234 ID42 - hello world */
    ASSERT_MEMEQ(buf, "<134>1 2023-11-14T22:13:20Z myhost myapp 1234 ID42 - hello world");
    ASSERT_EQ(n, (int)strlen(buf));
}

TEST(test_format_nilvalue_fields)
{
    syslog_msg_t msg = {
        .facility  = SYSLOG_FAC_KERN,
        .severity  = SYSLOG_SEV_EMERG,
        .hostname  = NULL,
        .app_name  = NULL,
        .proc_id   = NULL,
        .msg_id    = NULL,
        .message   = NULL,
        .timestamp = 0,
    };
    char buf[128];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* <0>1 - - - - - - */
    ASSERT_MEMEQ(buf, "<0>1 - - - - - -");
    ASSERT_EQ(n, (int)strlen(buf));
}

TEST(test_format_empty_strings_as_nilvalue)
{
    syslog_msg_t msg = {
        .facility  = SYSLOG_FAC_USER,
        .severity  = SYSLOG_SEV_ERR,
        .hostname  = "",
        .app_name  = "",
        .proc_id   = "",
        .msg_id    = "",
        .message   = "",
        .timestamp = 0,
    };
    char buf[128];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* user(1)*8 + err(3) = 11 */
    ASSERT_MEMEQ(buf, "<11>1 - - - - - -");
}

TEST(test_format_timestamp_epoch_zero)
{
    syslog_msg_t msg = {
        .facility  = SYSLOG_FAC_USER,
        .severity  = SYSLOG_SEV_NOTICE,
        .hostname  = "h",
        .app_name  = "a",
        .proc_id   = "1",
        .msg_id    = "M",
        .message   = "test",
        .timestamp = 0,
    };
    char buf[128];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* Timestamp should be NILVALUE "-" when 0 */
    ASSERT_MEMEQ(buf, "<13>1 - h a 1 M - test");
}

TEST(test_format_timestamp_known_date)
{
    /* 2000-01-01T00:00:00Z = 946684800 */
    syslog_msg_t msg = {
        .facility  = SYSLOG_FAC_KERN,
        .severity  = SYSLOG_SEV_INFO,
        .hostname  = "h",
        .app_name  = "a",
        .proc_id   = "-",
        .msg_id    = "-",
        .message   = "y2k",
        .timestamp = 946684800,
    };
    char buf[128];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_MEMEQ(buf, "<6>1 2000-01-01T00:00:00Z h a - - - y2k");
}

TEST(test_format_buffer_overflow)
{
    syslog_msg_t msg = {
        .facility  = SYSLOG_FAC_LOCAL0,
        .severity  = SYSLOG_SEV_INFO,
        .hostname  = "myhost",
        .app_name  = "myapp",
        .proc_id   = "1234",
        .msg_id    = "ID42",
        .message   = "hello world",
        .timestamp = 1700000000,
    };
    char buf[10];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_EQ(n, (int)SYSLOG_ERR_OVERFLOW);
}

TEST(test_format_null_params)
{
    char buf[64];
    syslog_msg_t msg = {0};

    ASSERT_EQ(syslog_format(NULL, buf, sizeof(buf)), (int)SYSLOG_ERR_PARAMS);
    ASSERT_EQ(syslog_format(&msg, NULL, sizeof(buf)), (int)SYSLOG_ERR_PARAMS);
    ASSERT_EQ(syslog_format(&msg, buf, 0), (int)SYSLOG_ERR_PARAMS);
}

TEST(test_format_long_message_truncation)
{
    /* Message longer than buffer => overflow */
    syslog_msg_t msg = {
        .facility = SYSLOG_FAC_USER,
        .severity = SYSLOG_SEV_DEBUG,
        .hostname = "h",
        .app_name = "a",
        .proc_id  = "1",
        .msg_id   = "M",
        .message  = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        .timestamp = 0,
    };
    char buf[32];  /* too small for header + 52-char message */
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_EQ(n, (int)SYSLOG_ERR_OVERFLOW);
}

TEST(test_format_exact_fit)
{
    /* Build a minimal message and size the buffer exactly */
    syslog_msg_t msg = {
        .facility  = SYSLOG_FAC_KERN,
        .severity  = SYSLOG_SEV_EMERG,
        .hostname  = NULL,
        .app_name  = NULL,
        .proc_id   = NULL,
        .msg_id    = NULL,
        .message   = NULL,
        .timestamp = 0,
    };
    /* Expected: "<0>1 - - - - - -" = 16 chars + NUL = 17 bytes */
    char buf[17];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_EQ(n, 16);
    ASSERT_EQ(buf[16], '\0');

    /* One byte too small => overflow */
    char small[16];
    n = syslog_format(&msg, small, sizeof(small));
    ASSERT_EQ(n, (int)SYSLOG_ERR_OVERFLOW);
}

/* ---- syslog_parse_pri tests ------------------------------------------ */

TEST(test_parse_pri_basic)
{
    uint8_t pri = 0;
    uint16_t offset = 0;
    ASSERT_EQ(syslog_parse_pri("<134>", 5, &pri, &offset), SYSLOG_OK);
    ASSERT_EQ(pri, 134);
    ASSERT_EQ(offset, 5);
}

TEST(test_parse_pri_zero)
{
    uint8_t pri = 255;
    uint16_t offset = 0;
    ASSERT_EQ(syslog_parse_pri("<0>", 3, &pri, &offset), SYSLOG_OK);
    ASSERT_EQ(pri, 0);
    ASSERT_EQ(offset, 3);
}

TEST(test_parse_pri_max)
{
    /* Max PRI = 23*8+7 = 191 */
    uint8_t pri = 0;
    uint16_t offset = 0;
    ASSERT_EQ(syslog_parse_pri("<191>", 5, &pri, &offset), SYSLOG_OK);
    ASSERT_EQ(pri, 191);
    ASSERT_EQ(offset, 5);
}

TEST(test_parse_pri_overflow)
{
    uint8_t pri = 0;
    uint16_t offset = 0;
    /* 192 exceeds max PRI */
    ASSERT_EQ(syslog_parse_pri("<192>", 5, &pri, &offset), SYSLOG_ERR_PARAMS);
}

TEST(test_parse_pri_malformed)
{
    uint8_t pri = 0;
    uint16_t offset = 0;
    /* Missing '<' */
    ASSERT_EQ(syslog_parse_pri("134>", 4, &pri, &offset), SYSLOG_ERR_PARAMS);
    /* Empty angle brackets */
    ASSERT_EQ(syslog_parse_pri("<>", 2, &pri, &offset), SYSLOG_ERR_PARAMS);
    /* Non-digit */
    ASSERT_EQ(syslog_parse_pri("<ab>", 4, &pri, &offset), SYSLOG_ERR_PARAMS);
    /* NULL input */
    ASSERT_EQ(syslog_parse_pri(NULL, 5, &pri, &offset), SYSLOG_ERR_PARAMS);
}

TEST(test_parse_pri_with_trailing_data)
{
    /* PRI followed by version — should stop at '>' */
    uint8_t pri = 0;
    uint16_t offset = 0;
    ASSERT_EQ(syslog_parse_pri("<14>1 - - - - - - msg", 21, &pri, &offset), SYSLOG_OK);
    ASSERT_EQ(pri, 14);
    ASSERT_EQ(offset, 4);
}

TEST(test_pri_roundtrip)
{
    /* Format then parse the PRI back */
    syslog_msg_t msg = {
        .facility = SYSLOG_FAC_LOCAL0,
        .severity = SYSLOG_SEV_WARNING,
        .hostname = "h",
        .app_name = "a",
        .timestamp = 0,
    };
    char buf[128];
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    uint8_t pri = 0;
    uint16_t offset = 0;
    ASSERT_EQ(syslog_parse_pri(buf, (uint16_t)n, &pri, &offset), SYSLOG_OK);
    ASSERT_EQ(pri, syslog_pri(SYSLOG_FAC_LOCAL0, SYSLOG_SEV_WARNING));
}

/* ---- different facilities -------------------------------------------- */

TEST(test_format_different_facilities)
{
    syslog_msg_t msg = {
        .severity = SYSLOG_SEV_ERR,
        .hostname = "h",
        .app_name = "a",
        .timestamp = 0,
    };
    char buf[128];

    /* DAEMON facility = 3*8+3 = 27 */
    msg.facility = SYSLOG_FAC_DAEMON;
    int n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_MEMEQ(buf, "<27>");

    /* AUTH facility = 4*8+3 = 35 */
    msg.facility = SYSLOG_FAC_AUTH;
    n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_MEMEQ(buf, "<35>");

    /* LOCAL7 facility = 23*8+3 = 187 */
    msg.facility = SYSLOG_FAC_LOCAL7;
    n = syslog_format(&msg, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_MEMEQ(buf, "<187>");
}

TEST_MAIN("syslog_packet")
    RUN(test_pri_kern_emerg);
    RUN(test_pri_user_info);
    RUN(test_pri_local0_debug);
    RUN(test_pri_local7_emerg);
    RUN(test_format_all_fields);
    RUN(test_format_nilvalue_fields);
    RUN(test_format_empty_strings_as_nilvalue);
    RUN(test_format_timestamp_epoch_zero);
    RUN(test_format_timestamp_known_date);
    RUN(test_format_buffer_overflow);
    RUN(test_format_null_params);
    RUN(test_format_long_message_truncation);
    RUN(test_format_exact_fit);
    RUN(test_parse_pri_basic);
    RUN(test_parse_pri_zero);
    RUN(test_parse_pri_max);
    RUN(test_parse_pri_overflow);
    RUN(test_parse_pri_malformed);
    RUN(test_parse_pri_with_trailing_data);
    RUN(test_pri_roundtrip);
    RUN(test_format_different_facilities);
TEST_MAIN_END()
