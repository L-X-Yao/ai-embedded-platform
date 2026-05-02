// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "json_parser.h"

/* ── Test helper infrastructure ──────────────────────────────────── */

#define MAX_EVENTS 64

typedef struct {
    json_event_t event;
    char         value[64];
    uint16_t     value_len;
} recorded_event_t;

static recorded_event_t g_events[MAX_EVENTS];
static int              g_event_count;

static void record_cb(void *ctx, json_event_t event,
                       const char *value, uint16_t value_len)
{
    (void)ctx;
    if (g_event_count < MAX_EVENTS) {
        g_events[g_event_count].event = event;
        if (value && value_len > 0) {
            uint16_t copy_len = value_len < 63 ? value_len : 63;
            memcpy(g_events[g_event_count].value, value, copy_len);
            g_events[g_event_count].value[copy_len] = '\0';
        } else {
            g_events[g_event_count].value[0] = '\0';
        }
        g_events[g_event_count].value_len = value_len;
        g_event_count++;
    }
}

static void reset_events(void)
{
    g_event_count = 0;
    memset(g_events, 0, sizeof(g_events));
}

static json_parser_err_t parse_full(const char *json)
{
    json_parser_t p;
    reset_events();
    json_parser_err_t err = json_parser_init(&p, record_cb, NULL, 16);
    if (err != JSON_PARSER_OK) return err;
    err = json_parser_feed(&p, json, (uint16_t)strlen(json));
    if (err != JSON_PARSER_OK) return err;
    return json_parser_finish(&p);
}

/* ── Tests ───────────────────────────────────────────────────────── */

TEST(test_init_null_params)
{
    json_parser_t p;
    ASSERT_EQ(json_parser_init(NULL, record_cb, NULL, 16), JSON_PARSER_ERR_PARAM);
    ASSERT_EQ(json_parser_init(&p, NULL, NULL, 16), JSON_PARSER_ERR_PARAM);
}

TEST(test_simple_object)
{
    json_parser_err_t err = parse_full("{\"key\": \"value\"}");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 4);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_KEY);
    ASSERT_MEMEQ(g_events[1].value, "key");
    ASSERT_EQ(g_events[2].event, JSON_EVENT_STRING);
    ASSERT_MEMEQ(g_events[2].value, "value");
    ASSERT_EQ(g_events[3].event, JSON_EVENT_OBJECT_END);
}

TEST(test_nested_object)
{
    json_parser_err_t err = parse_full("{\"a\": {\"b\": 1}}");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 7);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_KEY);
    ASSERT_MEMEQ(g_events[1].value, "a");
    ASSERT_EQ(g_events[2].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[3].event, JSON_EVENT_KEY);
    ASSERT_MEMEQ(g_events[3].value, "b");
    ASSERT_EQ(g_events[4].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[4].value, "1");
    ASSERT_EQ(g_events[5].event, JSON_EVENT_OBJECT_END);
    ASSERT_EQ(g_events[6].event, JSON_EVENT_OBJECT_END);
}

TEST(test_array)
{
    json_parser_err_t err = parse_full("[1, 2, 3]");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 5);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_ARRAY_START);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[1].value, "1");
    ASSERT_EQ(g_events[2].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[2].value, "2");
    ASSERT_EQ(g_events[3].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[3].value, "3");
    ASSERT_EQ(g_events[4].event, JSON_EVENT_ARRAY_END);
}

TEST(test_all_value_types)
{
    json_parser_err_t err = parse_full(
        "{\"s\": \"hello\", \"n\": 42, \"t\": true, \"f\": false, \"z\": null}");
    ASSERT_EQ(err, JSON_PARSER_OK);
    /* OBJECT_START + 5*(KEY+VALUE) + OBJECT_END = 12 */
    ASSERT_EQ(g_event_count, 12);
    ASSERT_EQ(g_events[2].event, JSON_EVENT_STRING);
    ASSERT_MEMEQ(g_events[2].value, "hello");
    ASSERT_EQ(g_events[4].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[4].value, "42");
    ASSERT_EQ(g_events[6].event, JSON_EVENT_TRUE);
    ASSERT_EQ(g_events[8].event, JSON_EVENT_FALSE);
    ASSERT_EQ(g_events[10].event, JSON_EVENT_NULL);
}

TEST(test_escape_sequences)
{
    json_parser_err_t err = parse_full("\"hello\\nworld\\t!\"");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_STRING);
    /* \n and \t become actual control characters */
    ASSERT_EQ(g_events[0].value[5], '\n');
    ASSERT_EQ(g_events[0].value[11], '\t');
}

TEST(test_escape_backslash_quote)
{
    json_parser_err_t err = parse_full("\"a\\\"b\\\\c\"");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_STRING);
    /* a"b\c */
    ASSERT_EQ(g_events[0].value[0], 'a');
    ASSERT_EQ(g_events[0].value[1], '"');
    ASSERT_EQ(g_events[0].value[2], 'b');
    ASSERT_EQ(g_events[0].value[3], '\\');
    ASSERT_EQ(g_events[0].value[4], 'c');
    ASSERT_EQ(g_events[0].value_len, 5);
}

TEST(test_unicode_escape)
{
    json_parser_err_t err = parse_full("\"\\u0041\"");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_STRING);
    /* \uXXXX stores hex digits literally: 0041 */
    ASSERT_EQ(g_events[0].value_len, 4);
    ASSERT_MEMEQ(g_events[0].value, "0041");
}

TEST(test_error_missing_quote)
{
    json_parser_err_t err = parse_full("{\"key: \"value\"}");
    ASSERT_TRUE(err != JSON_PARSER_OK);
}

TEST(test_error_max_depth)
{
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 2);
    /* depth 3: [[[ should exceed max_depth=2 */
    json_parser_err_t err = json_parser_feed(&p, "[[[1]]]", 7);
    ASSERT_EQ(err, JSON_PARSER_ERR_DEPTH);
}

TEST(test_chunked_feed)
{
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);

    const char *part1 = "{\"ke";
    const char *part2 = "y\": \"va";
    const char *part3 = "lue\"}";

    json_parser_err_t err;
    err = json_parser_feed(&p, part1, (uint16_t)strlen(part1));
    ASSERT_EQ(err, JSON_PARSER_OK);
    err = json_parser_feed(&p, part2, (uint16_t)strlen(part2));
    ASSERT_EQ(err, JSON_PARSER_OK);
    err = json_parser_feed(&p, part3, (uint16_t)strlen(part3));
    ASSERT_EQ(err, JSON_PARSER_OK);
    err = json_parser_finish(&p);
    ASSERT_EQ(err, JSON_PARSER_OK);

    ASSERT_EQ(g_event_count, 4);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_KEY);
    ASSERT_MEMEQ(g_events[1].value, "key");
    ASSERT_EQ(g_events[2].event, JSON_EVENT_STRING);
    ASSERT_MEMEQ(g_events[2].value, "value");
}

TEST(test_empty_object)
{
    json_parser_err_t err = parse_full("{}");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 2);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_OBJECT_END);
}

TEST(test_empty_array)
{
    json_parser_err_t err = parse_full("[]");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 2);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_ARRAY_START);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_ARRAY_END);
}

TEST(test_negative_number)
{
    json_parser_err_t err = parse_full("-42");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[0].value, "-42");
}

TEST(test_float_number)
{
    json_parser_err_t err = parse_full("3.14");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[0].value, "3.14");
}

TEST(test_exponent_number)
{
    json_parser_err_t err = parse_full("1.5e10");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[0].value, "1.5e10");
}

TEST(test_negative_exponent)
{
    json_parser_err_t err = parse_full("2.5E-3");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[0].value, "2.5E-3");
}

TEST(test_reset)
{
    json_parser_t p;
    json_parser_init(&p, record_cb, NULL, 16);

    reset_events();
    json_parser_feed(&p, "42", 2);
    json_parser_finish(&p);
    ASSERT_EQ(g_event_count, 1);

    json_parser_reset(&p);
    reset_events();
    json_parser_feed(&p, "\"hello\"", 7);
    json_parser_finish(&p);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_STRING);
    ASSERT_MEMEQ(g_events[0].value, "hello");
}

TEST(test_incomplete)
{
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);
    json_parser_feed(&p, "{\"key\":", 6);
    json_parser_err_t err = json_parser_finish(&p);
    ASSERT_EQ(err, JSON_PARSER_ERR_INCOMPLETE);
}

TEST(test_array_of_objects)
{
    json_parser_err_t err = parse_full("[{\"a\":1},{\"b\":2}]");
    ASSERT_EQ(err, JSON_PARSER_OK);
    /* ARRAY_START + OBJ_START + KEY + NUM + OBJ_END
     *            + OBJ_START + KEY + NUM + OBJ_END + ARRAY_END = 10 */
    ASSERT_EQ(g_event_count, 10);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_ARRAY_START);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[4].event, JSON_EVENT_OBJECT_END);
    ASSERT_EQ(g_events[5].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[8].event, JSON_EVENT_OBJECT_END);
    ASSERT_EQ(g_events[9].event, JSON_EVENT_ARRAY_END);
}

TEST(test_whitespace_handling)
{
    json_parser_err_t err = parse_full("  {  \"k\"  :  \"v\"  }  ");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 4);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[3].event, JSON_EVENT_OBJECT_END);
}

TEST(test_zero_number)
{
    json_parser_err_t err = parse_full("0");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[0].value, "0");
}

TEST(test_nested_arrays)
{
    json_parser_err_t err = parse_full("[[1,[2]],3]");
    ASSERT_EQ(err, JSON_PARSER_OK);
    /* [  [  1  ,  [  2  ]  ]  ,  3  ] */
    ASSERT_EQ(g_events[0].event, JSON_EVENT_ARRAY_START);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_ARRAY_START);
    ASSERT_EQ(g_events[2].event, JSON_EVENT_NUMBER);
    ASSERT_EQ(g_events[3].event, JSON_EVENT_ARRAY_START);
    ASSERT_EQ(g_events[4].event, JSON_EVENT_NUMBER);
    ASSERT_EQ(g_events[5].event, JSON_EVENT_ARRAY_END);
    ASSERT_EQ(g_events[6].event, JSON_EVENT_ARRAY_END);
    ASSERT_EQ(g_events[7].event, JSON_EVENT_NUMBER);
    ASSERT_EQ(g_events[8].event, JSON_EVENT_ARRAY_END);
}

TEST(test_error_syntax_trailing_garbage)
{
    /* Trailing non-whitespace after a complete value */
    json_parser_err_t err = parse_full("42 xyz");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

TEST(test_error_syntax_invalid_literal)
{
    /* Misspelled literal: "tru" followed by non-alpha triggers syntax error */
    json_parser_err_t err = parse_full("{\"k\": tru}");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

TEST(test_error_syntax_bad_escape)
{
    /* Invalid escape sequence \q inside a string */
    json_parser_err_t err = parse_full("\"hello\\qworld\"");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

TEST(test_error_syntax_mismatched_bracket)
{
    /* Close with } when [ was opened */
    json_parser_err_t err = parse_full("[1, 2}");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

TEST(test_error_syntax_missing_colon)
{
    /* Object key not followed by colon */
    json_parser_err_t err = parse_full("{\"key\" \"value\"}");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

TEST(test_error_syntax_bare_comma)
{
    /* Leading comma in array */
    json_parser_err_t err = parse_full("[,1]");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

TEST(test_chunked_number)
{
    /* Split a number across two feed() calls */
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);
    json_parser_feed(&p, "12", 2);
    json_parser_feed(&p, "34", 2);
    json_parser_finish(&p);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[0].value, "1234");
}

TEST_MAIN("json_parser")
    RUN(test_init_null_params);
    RUN(test_simple_object);
    RUN(test_nested_object);
    RUN(test_array);
    RUN(test_all_value_types);
    RUN(test_escape_sequences);
    RUN(test_escape_backslash_quote);
    RUN(test_unicode_escape);
    RUN(test_error_missing_quote);
    RUN(test_error_max_depth);
    RUN(test_chunked_feed);
    RUN(test_empty_object);
    RUN(test_empty_array);
    RUN(test_negative_number);
    RUN(test_float_number);
    RUN(test_exponent_number);
    RUN(test_negative_exponent);
    RUN(test_reset);
    RUN(test_incomplete);
    RUN(test_array_of_objects);
    RUN(test_whitespace_handling);
    RUN(test_zero_number);
    RUN(test_nested_arrays);
    RUN(test_error_syntax_trailing_garbage);
    RUN(test_error_syntax_invalid_literal);
    RUN(test_error_syntax_bad_escape);
    RUN(test_error_syntax_mismatched_bracket);
    RUN(test_error_syntax_missing_colon);
    RUN(test_error_syntax_bare_comma);
    RUN(test_chunked_number);
TEST_MAIN_END()
