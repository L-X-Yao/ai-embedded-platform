// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "telemetry.h"
#include <string.h>
#include <math.h>

/* ---- Send callback capture ---- */

static char sent_buf[2048];
static size_t sent_len;
static int send_called;
static int send_fail_mode;  /* 0=ok, 1=fail */

static telemetry_err_t mock_send(void *ctx, const char *data, uint16_t len)
{
    (void)ctx;
    send_called++;
    if (send_fail_mode) {
        return TELEMETRY_ERR_SEND;
    }
    if (sent_len + len < sizeof(sent_buf)) {
        memcpy(sent_buf + sent_len, data, len);
        sent_len += len;
    }
    sent_buf[sent_len] = '\0';
    return TELEMETRY_OK;
}

static void capture_reset(void)
{
    sent_len = 0;
    sent_buf[0] = '\0';
    send_called = 0;
    send_fail_mode = 0;
}

/* ---- Common setup ---- */

static telemetry_t telem;

static void setup(telemetry_format_t fmt)
{
    capture_reset();
    telemetry_init(&telem, mock_send, NULL, fmt);
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    telemetry_err_t err = telemetry_init(&telem, mock_send, NULL,
                                         TELEMETRY_FMT_JSON);
    ASSERT_EQ(err, TELEMETRY_OK);
    ASSERT_EQ(telem.field_count, 0);
    ASSERT_EQ(telem.send_count, 0);
    ASSERT_EQ(telem.error_count, 0);
    ASSERT_EQ(telem.format, TELEMETRY_FMT_JSON);
}

TEST(test_init_null_telemetry)
{
    telemetry_err_t err = telemetry_init(NULL, mock_send, NULL,
                                         TELEMETRY_FMT_JSON);
    ASSERT_EQ(err, TELEMETRY_ERR_PARAM);
}

TEST(test_init_null_send)
{
    telemetry_err_t err = telemetry_init(&telem, NULL, NULL,
                                         TELEMETRY_FMT_JSON);
    ASSERT_EQ(err, TELEMETRY_ERR_PARAM);
}

TEST(test_register_fields)
{
    setup(TELEMETRY_FMT_JSON);
    ASSERT_EQ(telemetry_register(&telem, "temp"), TELEMETRY_OK);
    ASSERT_EQ(telemetry_register(&telem, "hum"), TELEMETRY_OK);
    ASSERT_EQ(telem.field_count, 2);
    ASSERT_TRUE(strcmp(telem.fields[0].name, "temp") == 0);
    ASSERT_TRUE(strcmp(telem.fields[1].name, "hum") == 0);
}

TEST(test_register_null_params)
{
    setup(TELEMETRY_FMT_JSON);
    ASSERT_EQ(telemetry_register(NULL, "temp"), TELEMETRY_ERR_PARAM);
    ASSERT_EQ(telemetry_register(&telem, NULL), TELEMETRY_ERR_PARAM);
}

TEST(test_register_max_fields)
{
    setup(TELEMETRY_FMT_JSON);
    char names[TELEMETRY_MAX_FIELDS + 1][8];
    for (int i = 0; i < TELEMETRY_MAX_FIELDS; i++) {
        snprintf(names[i], sizeof(names[i]), "f%d", i);
        ASSERT_EQ(telemetry_register(&telem, names[i]), TELEMETRY_OK);
    }
    /* table full */
    ASSERT_EQ(telemetry_register(&telem, "extra"), TELEMETRY_ERR_FULL);
}

TEST(test_update_by_name)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    telemetry_register(&telem, "hum");

    ASSERT_EQ(telemetry_update(&telem, "temp", 25.5f), TELEMETRY_OK);
    ASSERT_NEAR(telem.fields[0].value, 25.5f, 0.01f);
    ASSERT_TRUE(telem.fields[0].updated);

    ASSERT_EQ(telemetry_update(&telem, "hum", 60.0f), TELEMETRY_OK);
    ASSERT_NEAR(telem.fields[1].value, 60.0f, 0.01f);
    ASSERT_TRUE(telem.fields[1].updated);
}

TEST(test_update_unknown_field)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    ASSERT_EQ(telemetry_update(&telem, "nosuch", 1.0f),
              TELEMETRY_ERR_PARAM);
}

TEST(test_update_null_params)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    ASSERT_EQ(telemetry_update(NULL, "temp", 1.0f), TELEMETRY_ERR_PARAM);
    ASSERT_EQ(telemetry_update(&telem, NULL, 1.0f), TELEMETRY_ERR_PARAM);
}

TEST(test_update_by_index)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    telemetry_register(&telem, "hum");

    ASSERT_EQ(telemetry_update_by_index(&telem, 0, 22.3f), TELEMETRY_OK);
    ASSERT_NEAR(telem.fields[0].value, 22.3f, 0.01f);
    ASSERT_TRUE(telem.fields[0].updated);

    ASSERT_EQ(telemetry_update_by_index(&telem, 1, 55.0f), TELEMETRY_OK);
    ASSERT_NEAR(telem.fields[1].value, 55.0f, 0.01f);
}

TEST(test_update_by_index_out_of_range)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    ASSERT_EQ(telemetry_update_by_index(&telem, 5, 1.0f),
              TELEMETRY_ERR_PARAM);
    ASSERT_EQ(telemetry_update_by_index(NULL, 0, 1.0f),
              TELEMETRY_ERR_PARAM);
}

TEST(test_publish_json)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    telemetry_register(&telem, "hum");
    telemetry_update(&telem, "temp", 25.5f);
    telemetry_update(&telem, "hum", 60.0f);

    ASSERT_EQ(telemetry_publish(&telem, 1000), TELEMETRY_OK);
    ASSERT_TRUE(send_called == 1);
    /* verify JSON structure */
    ASSERT_TRUE(strstr(sent_buf, "\"ts\":1000") != NULL);
    ASSERT_TRUE(strstr(sent_buf, "\"temp\":25.5") != NULL);
    ASSERT_TRUE(strstr(sent_buf, "\"hum\":60") != NULL);
    ASSERT_TRUE(sent_buf[0] == '{');
    ASSERT_TRUE(sent_buf[sent_len - 1] == '}');
}

TEST(test_publish_csv)
{
    setup(TELEMETRY_FMT_CSV);
    telemetry_register(&telem, "temp");
    telemetry_register(&telem, "hum");
    telemetry_update(&telem, "temp", 25.5f);
    telemetry_update(&telem, "hum", 60.0f);

    ASSERT_EQ(telemetry_publish(&telem, 1000), TELEMETRY_OK);
    ASSERT_TRUE(send_called == 1);
    /* CSV: timestamp,val1,val2 */
    ASSERT_TRUE(strstr(sent_buf, "1000,25.5,60") != NULL);
}

TEST(test_publish_key_value)
{
    setup(TELEMETRY_FMT_KEY_VALUE);
    telemetry_register(&telem, "temp");
    telemetry_register(&telem, "hum");
    telemetry_update(&telem, "temp", 25.5f);
    telemetry_update(&telem, "hum", 60.0f);

    ASSERT_EQ(telemetry_publish(&telem, 1000), TELEMETRY_OK);
    ASSERT_TRUE(send_called == 1);
    ASSERT_TRUE(strstr(sent_buf, "ts=1000") != NULL);
    ASSERT_TRUE(strstr(sent_buf, "temp=25.5") != NULL);
    ASSERT_TRUE(strstr(sent_buf, "hum=60") != NULL);
}

TEST(test_publish_timestamp)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "v");
    telemetry_update(&telem, "v", 1.0f);

    ASSERT_EQ(telemetry_publish(&telem, 999999), TELEMETRY_OK);
    ASSERT_TRUE(strstr(sent_buf, "\"ts\":999999") != NULL);
}

TEST(test_send_callback_invoked)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "x");
    telemetry_update(&telem, "x", 42.0f);

    ASSERT_EQ(send_called, 0);
    telemetry_publish(&telem, 0);
    ASSERT_EQ(send_called, 1);
}

TEST(test_send_failure_error_count)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "x");
    telemetry_update(&telem, "x", 1.0f);

    send_fail_mode = 1;
    telemetry_err_t err = telemetry_publish(&telem, 0);
    ASSERT_EQ(err, TELEMETRY_ERR_SEND);
    ASSERT_EQ(telemetry_get_error_count(&telem), 1);
    ASSERT_EQ(telemetry_get_send_count(&telem), 0);

    /* second failure */
    err = telemetry_publish(&telem, 0);
    ASSERT_EQ(err, TELEMETRY_ERR_SEND);
    ASSERT_EQ(telemetry_get_error_count(&telem), 2);
}

TEST(test_multiple_publishes)
{
    setup(TELEMETRY_FMT_CSV);
    telemetry_register(&telem, "v");
    telemetry_update(&telem, "v", 10.0f);

    ASSERT_EQ(telemetry_publish(&telem, 100), TELEMETRY_OK);
    ASSERT_EQ(telemetry_get_send_count(&telem), 1);

    /* update and publish again */
    capture_reset();
    telemetry_update(&telem, "v", 20.0f);
    ASSERT_EQ(telemetry_publish(&telem, 200), TELEMETRY_OK);
    ASSERT_EQ(telemetry_get_send_count(&telem), 2);
    ASSERT_TRUE(strstr(sent_buf, "200,20") != NULL);
}

TEST(test_reset)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "temp");
    telemetry_register(&telem, "hum");
    telemetry_update(&telem, "temp", 25.0f);
    telemetry_update(&telem, "hum", 60.0f);
    telemetry_publish(&telem, 0);

    telemetry_reset(&telem);

    /* values and flags cleared, counters reset */
    ASSERT_NEAR(telem.fields[0].value, 0.0f, 0.01f);
    ASSERT_NEAR(telem.fields[1].value, 0.0f, 0.01f);
    ASSERT_FALSE(telem.fields[0].updated);
    ASSERT_FALSE(telem.fields[1].updated);
    ASSERT_EQ(telemetry_get_send_count(&telem), 0);
    ASSERT_EQ(telemetry_get_error_count(&telem), 0);
    /* field_count preserved */
    ASSERT_EQ(telem.field_count, 2);
}

TEST(test_reset_null)
{
    /* should not crash */
    telemetry_reset(NULL);
    ASSERT_TRUE(1);
}

TEST(test_publish_null)
{
    ASSERT_EQ(telemetry_publish(NULL, 0), TELEMETRY_ERR_PARAM);
}

TEST(test_updated_flag_tracking)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "a");
    telemetry_register(&telem, "b");

    /* initially not updated */
    ASSERT_FALSE(telem.fields[0].updated);
    ASSERT_FALSE(telem.fields[1].updated);

    /* update one field */
    telemetry_update(&telem, "a", 10.0f);
    ASSERT_TRUE(telem.fields[0].updated);
    ASSERT_FALSE(telem.fields[1].updated);

    /* publish clears updated flags */
    telemetry_publish(&telem, 0);
    ASSERT_FALSE(telem.fields[0].updated);
    ASSERT_FALSE(telem.fields[1].updated);
}

TEST(test_buffer_overflow)
{
    /* Short names, so that only the whole buffer can run out: each field is
     * ,"f00":-3e38 in full, 47 characters, and 16 of them take 752 bytes. */
    setup(TELEMETRY_FMT_JSON);

    char names[TELEMETRY_MAX_FIELDS][16];
    for (int i = 0; i < TELEMETRY_MAX_FIELDS; i++) {
        snprintf(names[i], sizeof(names[i]), "f%02d", i);
        telemetry_register(&telem, names[i]);
        telemetry_update_by_index(&telem, (uint8_t)i, -3e38f);
    }

    ASSERT_EQ(telemetry_publish(&telem, 7), TELEMETRY_ERR_OVERFLOW);
    ASSERT_EQ(send_called, 0);
    ASSERT_EQ(telemetry_get_error_count(&telem), 1);
    ASSERT_EQ(telemetry_get_send_count(&telem), 0);
}

/*
 * CSV of the timestamp 7 and 12 fields of 41 characters takes 493 bytes.  A
 * 13th of 18 characters makes 511, the most that fits with the NUL in 512
 * bytes, and one of 19 makes 512.
 */
static telemetry_err_t publish_csv_ending_in(float last)
{
    setup(TELEMETRY_FMT_CSV);
    for (uint8_t i = 0; i < 13; i++) {
        telemetry_register(&telem, "f");
        telemetry_update_by_index(&telem, i, i < 12 ? -3e38f : last);
    }
    return telemetry_publish(&telem, 7);
}

TEST(test_text_of_buf_size_minus_one_fits)
{
    ASSERT_EQ(publish_csv_ending_in(1e16f), TELEMETRY_OK);  /* 17 digits */
    ASSERT_EQ(send_called, 1);
    ASSERT_EQ(sent_len, TELEMETRY_BUF_SIZE - 1);
}

TEST(test_text_of_buf_size_is_refused)
{
    ASSERT_EQ(publish_csv_ending_in(2e17f), TELEMETRY_ERR_OVERFLOW);  /* 18 */
    ASSERT_EQ(send_called, 0);
    ASSERT_EQ(telemetry_get_error_count(&telem), 1);
}

/* Name lengths bring each piece of the text in turn to the end of the buffer.
 * rest is the length of the text around the name: whatever runs out, nothing
 * cut may be sent. */
static void sweep_name_length(telemetry_format_t fmt, size_t rest)
{
    static char name[TELEMETRY_BUF_SIZE + 16];

    for (size_t len = 1; len < sizeof(name); len++) {
        memset(name, 'n', len);
        name[len] = '\0';
        setup(fmt);
        telemetry_register(&telem, name);
        telemetry_update_by_index(&telem, 0, 1.5f);

        telemetry_err_t err = telemetry_publish(&telem, 7);
        if (len + rest < TELEMETRY_BUF_SIZE) {
            ASSERT_EQ(err, TELEMETRY_OK);
            ASSERT_EQ(sent_len, len + rest);
        } else {
            ASSERT_EQ(err, TELEMETRY_ERR_OVERFLOW);
            ASSERT_EQ(send_called, 0);
        }
    }
}

TEST(test_json_never_sends_a_cut_text)
{
    sweep_name_length(TELEMETRY_FMT_JSON, 15);  /* {"ts":7,"":1.5} */
}

TEST(test_key_value_never_sends_a_cut_text)
{
    sweep_name_length(TELEMETRY_FMT_KEY_VALUE, 9);  /* ts=7 =1.5 */
}

TEST(test_get_count_null)
{
    ASSERT_EQ(telemetry_get_send_count(NULL), 0);
    ASSERT_EQ(telemetry_get_error_count(NULL), 0);
}

TEST(test_publish_no_fields)
{
    setup(TELEMETRY_FMT_JSON);
    /* publish with no registered fields -- should still produce ts */
    ASSERT_EQ(telemetry_publish(&telem, 42), TELEMETRY_OK);
    ASSERT_TRUE(strstr(sent_buf, "\"ts\":42") != NULL);
    ASSERT_TRUE(strstr(sent_buf, "{") != NULL);
    ASSERT_TRUE(strstr(sent_buf, "}") != NULL);
}

/* ctx-capture send callback for test_send_ctx_passed */
static void *ctx_received_ptr;
static telemetry_err_t ctx_capture_send(void *ctx, const char *data,
                                        uint16_t len)
{
    (void)data;
    (void)len;
    ctx_received_ptr = ctx;
    return TELEMETRY_OK;
}

TEST(test_send_ctx_passed)
{
    static int ctx_val = 77;
    ctx_received_ptr = NULL;

    telemetry_init(&telem, ctx_capture_send, &ctx_val, TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "x");
    telemetry_update(&telem, "x", 1.0f);
    telemetry_publish(&telem, 0);
    ASSERT_TRUE(ctx_received_ptr == &ctx_val);
}

TEST(test_float_formatting_integers)
{
    /* integer values should not have trailing .00 */
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "v");
    telemetry_update(&telem, "v", 42.0f);
    telemetry_publish(&telem, 0);
    /* should be "v":42 not "v":42.00 */
    ASSERT_TRUE(strstr(sent_buf, "\"v\":42") != NULL);
    ASSERT_TRUE(strstr(sent_buf, "42.00") == NULL);
}

TEST(test_update_nan_rejected)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "v");
    ASSERT_EQ(telemetry_update(&telem, "v", NAN), TELEMETRY_ERR_PARAM);
    /* value should remain at default 0 */
    ASSERT_NEAR(telem.fields[0].value, 0.0f, 0.01f);
    ASSERT_FALSE(telem.fields[0].updated);
}

TEST(test_update_inf_rejected)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "v");
    ASSERT_EQ(telemetry_update(&telem, "v", INFINITY), TELEMETRY_ERR_PARAM);
    ASSERT_EQ(telemetry_update(&telem, "v", -INFINITY), TELEMETRY_ERR_PARAM);
}

TEST(test_update_by_index_nan_rejected)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "v");
    ASSERT_EQ(telemetry_update_by_index(&telem, 0, NAN), TELEMETRY_ERR_PARAM);
    ASSERT_EQ(telemetry_update_by_index(&telem, 0, INFINITY), TELEMETRY_ERR_PARAM);
}

/* The exact value of -3e38f: a cut text would read nine orders of magnitude
 * too small, and still look like a number. */
#define MINUS_3E38F_TEXT "-300000000549775575777803994281145270272"

TEST(test_long_value_is_not_cut)
{
    setup(TELEMETRY_FMT_KEY_VALUE);
    telemetry_register(&telem, "x");
    ASSERT_EQ(telemetry_update(&telem, "x", -3e38f), TELEMETRY_OK);

    ASSERT_EQ(telemetry_publish(&telem, 2), TELEMETRY_OK);
    ASSERT_STR_EQ(sent_buf, "ts=2 x=" MINUS_3E38F_TEXT);
}

TEST(test_long_value_is_sent_in_every_format)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "x");
    telemetry_update(&telem, "x", -3e38f);
    ASSERT_EQ(telemetry_publish(&telem, 2), TELEMETRY_OK);
    ASSERT_STR_EQ(sent_buf, "{\"ts\":2,\"x\":" MINUS_3E38F_TEXT "}");

    setup(TELEMETRY_FMT_CSV);
    telemetry_register(&telem, "x");
    telemetry_update(&telem, "x", -3e38f);
    ASSERT_EQ(telemetry_publish(&telem, 2), TELEMETRY_OK);
    ASSERT_STR_EQ(sent_buf, "2," MINUS_3E38F_TEXT);
}

/* Only the whole buffer limits a field: a value that grows at run time must
 * not stop every field of the device. */
TEST(test_field_is_limited_only_by_the_buffer)
{
    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, "engine_coolant_temp");
    telemetry_update(&telem, "engine_coolant_temp", 25.5f);
    ASSERT_EQ(telemetry_publish(&telem, 7), TELEMETRY_OK);
    ASSERT_STR_EQ(sent_buf, "{\"ts\":7,\"engine_coolant_temp\":25.5}");

    capture_reset();
    telemetry_update(&telem, "engine_coolant_temp", 123456.75f);
    ASSERT_EQ(telemetry_publish(&telem, 7), TELEMETRY_OK);
    ASSERT_STR_EQ(sent_buf, "{\"ts\":7,\"engine_coolant_temp\":123456.75}");
    ASSERT_EQ(telemetry_get_error_count(&telem), 0);
}

TEST(test_long_name_is_sent)
{
    static const char name[] =
        "a_field_name_far_longer_than_any_one_field_used_to_be_allowed_to_be";

    setup(TELEMETRY_FMT_KEY_VALUE);
    telemetry_register(&telem, name);
    telemetry_update(&telem, name, 1.5f);
    ASSERT_EQ(telemetry_publish(&telem, 3), TELEMETRY_OK);
    ASSERT_TRUE(strncmp(sent_buf, "ts=3 a_field_name", 17) == 0);
    ASSERT_TRUE(strstr(sent_buf, "_to_be=1.5") != NULL);

    setup(TELEMETRY_FMT_JSON);
    telemetry_register(&telem, name);
    telemetry_update(&telem, name, 1.5f);
    ASSERT_EQ(telemetry_publish(&telem, 3), TELEMETRY_OK);
    ASSERT_TRUE(strstr(sent_buf, "_to_be\":1.5}") != NULL);
}

/* ================================================================ */

TEST_MAIN("telemetry")
    RUN(test_init_valid);
    RUN(test_init_null_telemetry);
    RUN(test_init_null_send);
    RUN(test_register_fields);
    RUN(test_register_null_params);
    RUN(test_register_max_fields);
    RUN(test_update_by_name);
    RUN(test_update_unknown_field);
    RUN(test_update_null_params);
    RUN(test_update_by_index);
    RUN(test_update_by_index_out_of_range);
    RUN(test_publish_json);
    RUN(test_publish_csv);
    RUN(test_publish_key_value);
    RUN(test_publish_timestamp);
    RUN(test_send_callback_invoked);
    RUN(test_send_failure_error_count);
    RUN(test_multiple_publishes);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_publish_null);
    RUN(test_updated_flag_tracking);
    RUN(test_buffer_overflow);
    RUN(test_text_of_buf_size_minus_one_fits);
    RUN(test_text_of_buf_size_is_refused);
    RUN(test_json_never_sends_a_cut_text);
    RUN(test_key_value_never_sends_a_cut_text);
    RUN(test_get_count_null);
    RUN(test_publish_no_fields);
    RUN(test_send_ctx_passed);
    RUN(test_float_formatting_integers);
    RUN(test_update_nan_rejected);
    RUN(test_update_inf_rejected);
    RUN(test_update_by_index_nan_rejected);
    RUN(test_long_value_is_not_cut);
    RUN(test_long_value_is_sent_in_every_format);
    RUN(test_field_is_limited_only_by_the_buffer);
    RUN(test_long_name_is_sent);
TEST_MAIN_END()
