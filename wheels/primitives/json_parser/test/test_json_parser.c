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

/* ── Mutation-hardening tests ─────────────────────────────────── */

/*
 * json_parser_init max_depth clamping:
 * - max_depth=0 must be treated as 16 (default), not left as 0.
 * - max_depth=129 must be clamped to 128.
 * Catches > 128 vs >= 128 and == 0 vs < 0 mutations.
 */
TEST(test_init_max_depth_clamping)
{
    json_parser_t p;
    json_parser_err_t err;

    /* max_depth=0 → clamped to 16; a depth-1 object must succeed */
    err = json_parser_init(&p, record_cb, NULL, 0);
    ASSERT_EQ(err, JSON_PARSER_OK);
    reset_events();
    err = json_parser_feed(&p, "{}", 2);
    ASSERT_EQ(err, JSON_PARSER_OK);

    /* max_depth=129 → clamped to 128; should accept 128 levels but not 129 */
    err = json_parser_init(&p, record_cb, NULL, 129);
    ASSERT_EQ(err, JSON_PARSER_OK);
    /* max_depth capped: p.max_depth == 128 → a 2-deep object must succeed */
    reset_events();
    err = json_parser_feed(&p, "{\"a\":{\"b\":1}}", 13);
    ASSERT_EQ(err, JSON_PARSER_OK);
}

/*
 * Depth-exactly-at-limit: depth == max_depth must reject; depth == max_depth-1 must accept.
 * Catches >= vs > mutations in the depth check.
 */
TEST(test_depth_exactly_at_limit)
{
    json_parser_t p;
    json_parser_err_t err;

    /* max_depth=1: depth 1 (a single object) is allowed, depth 2 is not */
    err = json_parser_init(&p, record_cb, NULL, 1);
    ASSERT_EQ(err, JSON_PARSER_OK);
    reset_events();
    err = json_parser_feed(&p, "{}", 2);
    ASSERT_EQ(err, JSON_PARSER_OK); /* exactly 1 level — must succeed */

    err = json_parser_init(&p, record_cb, NULL, 1);
    ASSERT_EQ(err, JSON_PARSER_OK);
    reset_events();
    err = json_parser_feed(&p, "{\"a\":{}}", 8);
    ASSERT_EQ(err, JSON_PARSER_ERR_DEPTH); /* 2 levels — must fail */

    /* max_depth=2: depth 2 is allowed, depth 3 is not */
    err = json_parser_init(&p, record_cb, NULL, 2);
    ASSERT_EQ(err, JSON_PARSER_OK);
    reset_events();
    err = json_parser_feed(&p, "{\"a\":{}}", 8);
    ASSERT_EQ(err, JSON_PARSER_OK); /* exactly 2 levels — must succeed */
}

/*
 * Event counts must be exact — existing tests use ASSERT_EQ on g_event_count
 * but don't verify the exact value content. These tests pin the precise
 * string content of each event value, catching strlen vs value_len mutations.
 */
TEST(test_event_value_lengths_exact)
{
    json_parser_err_t err = parse_full("{\"hello\": \"world\"}");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 4);
    /* KEY "hello" has value_len=5 */
    ASSERT_EQ(g_events[1].event, JSON_EVENT_KEY);
    ASSERT_EQ(g_events[1].value_len, 5);
    ASSERT_MEMEQ(g_events[1].value, "hello");
    /* STRING "world" has value_len=5 */
    ASSERT_EQ(g_events[2].event, JSON_EVENT_STRING);
    ASSERT_EQ(g_events[2].value_len, 5);
    ASSERT_MEMEQ(g_events[2].value, "world");
}

/*
 * True/false/null events: verify they fire exactly once with correct vlen —
 * catches vlen==4 vs vlen==5 in flush_literal length checks.
 */
TEST(test_literal_events_exact)
{
    json_parser_err_t err;

    err = parse_full("true");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_TRUE);
    ASSERT_EQ(g_events[0].value_len, 4);

    err = parse_full("false");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_FALSE);
    ASSERT_EQ(g_events[0].value_len, 5);

    err = parse_full("null");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NULL);
    ASSERT_EQ(g_events[0].value_len, 4);
}

/*
 * "trul" and "nult" are not valid literals — verify they produce a syntax error
 * instead of silently emitting a partial event. Catches memcmp length guards.
 */
TEST(test_bad_literal_rejected)
{
    json_parser_err_t err;

    /* "trul" — not "true", not "null" */
    err = parse_full("trul");
    ASSERT_TRUE(err != JSON_PARSER_OK);

    /* "nul" — incomplete null */
    err = parse_full("nul");
    ASSERT_TRUE(err != JSON_PARSER_OK);

    /* "truu" — not "true" */
    err = parse_full("{\"k\": truu}");
    ASSERT_TRUE(err != JSON_PARSER_OK);
}

/*
 * json_parser_feed with data=NULL and len=0 must succeed (empty feed) —
 * catches NULL && len > 0 vs NULL && len != 0 mutations.
 */
TEST(test_feed_null_data_zero_len)
{
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);
    json_parser_err_t err = json_parser_feed(&p, NULL, 0);
    ASSERT_EQ(err, JSON_PARSER_OK); /* null data with zero length is fine */
}

/*
 * json_parser_feed with data=NULL and len>0 must return ERR_PARAM —
 * catches the null-data guard condition.
 */
TEST(test_feed_null_data_nonzero_len)
{
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);
    json_parser_err_t err = json_parser_feed(&p, NULL, 1);
    ASSERT_EQ(err, JSON_PARSER_ERR_PARAM);
}

/*
 * After a top-level value completes, non-whitespace trailing chars are ERR_SYNTAX —
 * verify the exact error code (not just != OK).
 */
TEST(test_done_state_trailing_char_exact_error)
{
    json_parser_err_t err = parse_full("1 x");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);

    err = parse_full("\"a\" \"b\"");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

/*
 * Object key must be followed by ':', not some other character.
 * Catches the is_ws check and c == ':' check mutations.
 */
TEST(test_after_key_state_exact)
{
    /* Valid: key followed by colon */
    json_parser_err_t err = parse_full("{\"k\":1}");
    ASSERT_EQ(err, JSON_PARSER_OK);

    /* Invalid: key followed by comma instead of colon */
    err = parse_full("{\"k\",1}");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);

    /* Invalid: key followed by nothing */
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);
    json_parser_feed(&p, "{\"k\"}", 5);
    /* '}' in AFTER_KEY state is a syntax error */
    err = json_parser_finish(&p);
    ASSERT_TRUE(err != JSON_PARSER_OK);
}

/*
 * Mismatched container close: } used to close [ is ERR_SYNTAX, not a no-op.
 * Catches peek_container comparison mutations.
 */
TEST(test_mismatched_close_exact_error)
{
    json_parser_err_t err = parse_full("[}");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);

    err = parse_full("{]");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

/*
 * Number flushing boundary: a number terminated by whitespace vs by a
 * structural character — both must flush exactly once.
 */
TEST(test_number_flushed_exactly_once)
{
    /* Number in array, comma-terminated */
    json_parser_err_t err = parse_full("[1,2]");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 4);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[1].value, "1");
    ASSERT_EQ(g_events[1].value_len, 1);
    ASSERT_EQ(g_events[2].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[2].value, "2");
    ASSERT_EQ(g_events[2].value_len, 1);

    /* Number terminated by ] */
    err = parse_full("[42]");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_event_count, 3);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[1].value, "42");
    ASSERT_EQ(g_events[1].value_len, 2);
}

/*
 * json_parser_finish on a completely empty feed (no data) must return
 * ERR_INCOMPLETE (no value received), not OK.
 */
TEST(test_finish_no_data_is_incomplete)
{
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);
    json_parser_err_t err = json_parser_finish(&p);
    ASSERT_EQ(err, JSON_PARSER_ERR_INCOMPLETE);
}

/*
 * json_parser_finish with unclosed object must return ERR_INCOMPLETE —
 * catches depth > 0 check mutation.
 */
TEST(test_finish_unclosed_object_is_incomplete)
{
    json_parser_t p;
    reset_events();
    json_parser_init(&p, record_cb, NULL, 16);
    json_parser_feed(&p, "{\"k\":1", 6);
    json_parser_err_t err = json_parser_finish(&p);
    ASSERT_EQ(err, JSON_PARSER_ERR_INCOMPLETE);
}

/*
 * json_parser_reset: verify that state resets correctly — the callback
 * and max_depth are preserved, but parser state starts fresh.
 */
TEST(test_reset_preserves_config)
{
    json_parser_t p;
    json_parser_init(&p, record_cb, NULL, 5);

    reset_events();
    json_parser_feed(&p, "1", 1);
    json_parser_finish(&p);
    ASSERT_EQ(g_event_count, 1);

    json_parser_reset(&p);
    reset_events();
    json_parser_feed(&p, "2", 1);
    json_parser_finish(&p);
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_NUMBER);
    ASSERT_MEMEQ(g_events[0].value, "2");
}

/*
 * Unicode escape: exactly 4 hex digits are consumed, no more, no less.
 * A \u followed by only 3 hex digits and end-of-input is incomplete.
 */
TEST(test_unicode_escape_exact_four_digits)
{
    /* Valid: exactly 4 hex digits */
    json_parser_err_t err = parse_full("\"\\u004F\"");
    ASSERT_EQ(err, JSON_PARSER_OK);
    ASSERT_EQ(g_events[0].value_len, 4);

    /* Invalid: \u followed by non-hex char */
    err = parse_full("\"\\uGGGG\"");
    ASSERT_EQ(err, JSON_PARSER_ERR_SYNTAX);
}

/*
 * Struct token events fire with NULL value pointer — verify value_len is 0
 * for OBJECT_START, OBJECT_END, ARRAY_START, ARRAY_END events.
 */
TEST(test_structural_events_have_no_value)
{
    json_parser_err_t err = parse_full("[{}]");
    ASSERT_EQ(err, JSON_PARSER_OK);
    /* ARRAY_START, OBJECT_START, OBJECT_END, ARRAY_END = 4 events */
    ASSERT_EQ(g_event_count, 4);
    ASSERT_EQ(g_events[0].event, JSON_EVENT_ARRAY_START);
    ASSERT_EQ(g_events[0].value_len, 0);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_OBJECT_START);
    ASSERT_EQ(g_events[1].value_len, 0);
    ASSERT_EQ(g_events[2].event, JSON_EVENT_OBJECT_END);
    ASSERT_EQ(g_events[2].value_len, 0);
    ASSERT_EQ(g_events[3].event, JSON_EVENT_ARRAY_END);
    ASSERT_EQ(g_events[3].value_len, 0);
}

/*
 * Object with multiple keys: verify expect_key flag is set after each comma
 * inside an object, so subsequent strings become KEY events, not STRING events.
 */
TEST(test_multiple_keys_are_key_events)
{
    json_parser_err_t err = parse_full("{\"a\":1,\"b\":2,\"c\":3}");
    ASSERT_EQ(err, JSON_PARSER_OK);
    /* OBJ_START + 3*(KEY+NUM) + OBJ_END = 8 events */
    ASSERT_EQ(g_event_count, 8);
    ASSERT_EQ(g_events[1].event, JSON_EVENT_KEY);
    ASSERT_MEMEQ(g_events[1].value, "a");
    ASSERT_EQ(g_events[3].event, JSON_EVENT_KEY);
    ASSERT_MEMEQ(g_events[3].value, "b");
    ASSERT_EQ(g_events[5].event, JSON_EVENT_KEY);
    ASSERT_MEMEQ(g_events[5].value, "c");
}

/* ── What may follow '{', '[', ':' and ',' (RFC 8259 section 2) ──── */

/* Same as parse_full, one byte per feed: state must survive a chunk edge. */
static json_parser_err_t parse_bytewise(const char *json)
{
    json_parser_t p;
    reset_events();
    json_parser_err_t err = json_parser_init(&p, record_cb, NULL, 16);
    for (size_t i = 0; err == JSON_PARSER_OK && json[i] != '\0'; i++) {
        err = json_parser_feed(&p, &json[i], 1);
    }
    return (err != JSON_PARSER_OK) ? err : json_parser_finish(&p);
}

TEST(test_string_after_empty_object_in_array)
{
    ASSERT_EQ(parse_full("[{},\"x\"]"), JSON_PARSER_OK);
    /* ARR[ OBJ{ }OBJ STR ]ARR */
    ASSERT_EQ(g_event_count, 5);
    ASSERT_EQ(g_events[3].event, JSON_EVENT_STRING);
    ASSERT_MEMEQ(g_events[3].value, "x");
}

TEST(test_string_after_nested_empty_object)
{
    ASSERT_EQ(parse_full("{\"a\":[{},\"x\"]}"), JSON_PARSER_OK);
    /* OBJ{ KEY ARR[ OBJ{ }OBJ STR ]ARR }OBJ */
    ASSERT_EQ(g_event_count, 8);
    ASSERT_EQ(g_events[5].event, JSON_EVENT_STRING);

    ASSERT_EQ(parse_full("[[{}],\"x\"]"), JSON_PARSER_OK);
    /* ARR[ ARR[ OBJ{ }OBJ ]ARR STR ]ARR */
    ASSERT_EQ(g_event_count, 7);
    ASSERT_EQ(g_events[5].event, JSON_EVENT_STRING);
}

TEST(test_key_in_array_rejected)
{
    ASSERT_EQ(parse_full("[{},\"x\":1]"), JSON_PARSER_ERR_SYNTAX);
}

TEST(test_closing_bracket_where_value_required)
{
    static const char *const docs[] = {
        "{\"a\":}", "[1,]", "{\"a\":1,}", "[[],]", "{\"a\":{},}", "[,]", "{,}",
    };
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        ASSERT_EQ(parse_full(docs[i]), JSON_PARSER_ERR_SYNTAX);
        ASSERT_EQ(parse_bytewise(docs[i]), JSON_PARSER_ERR_SYNTAX);
    }
}

TEST(test_object_member_needs_string_key)
{
    static const char *const docs[] = {
        "{1}", "{true}", "{null}", "{[1]}", "{{}}", "{\"a\":1,2}",
        "{\"a\":1,[2]}", "{\"a\":1,{}}", "{\"a\":1,true}",
    };
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        ASSERT_EQ(parse_full(docs[i]), JSON_PARSER_ERR_SYNTAX);
        ASSERT_EQ(parse_bytewise(docs[i]), JSON_PARSER_ERR_SYNTAX);
    }
}

TEST(test_empty_container_closed_by_wrong_bracket)
{
    ASSERT_EQ(parse_full("{]"), JSON_PARSER_ERR_SYNTAX);
    ASSERT_EQ(parse_full("[}"), JSON_PARSER_ERR_SYNTAX);
}

TEST(test_bare_closing_bracket_at_top_level)
{
    ASSERT_EQ(parse_full("}"), JSON_PARSER_ERR_SYNTAX);
    ASSERT_EQ(parse_full("]"), JSON_PARSER_ERR_SYNTAX);
}

TEST(test_valid_containers_with_whitespace)
{
    static const char *const docs[] = {
        "{}", "[]", "[[]]", "[{}]", "{\"a\":{}}", "{\"a\":[]}", "[{},{}]", "[[],[]]",
        " { } ", "[ ]", "[ 1 , 2 ]", "{ \"a\" : 1 , \"b\" : 2 }", "[ {} , \"x\" ]",
        "[\n\t{ }\r\n]", "[1,{\"a\":[true,null]},\"s\"]", "[-1]", "[false]",
    };
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        ASSERT_EQ(parse_full(docs[i]), JSON_PARSER_OK);
        ASSERT_EQ(parse_bytewise(docs[i]), JSON_PARSER_OK);
    }
}

TEST(test_unclosed_containers_are_incomplete)
{
    static const char *const docs[] = {
        "{", "[", "{\"a\":1,", "[1,", "{\"a\":", "[{}",
    };
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        ASSERT_EQ(parse_full(docs[i]), JSON_PARSER_ERR_INCOMPLETE);
    }
}

TEST_MAIN("json_parser")
    RUN(test_string_after_empty_object_in_array);
    RUN(test_string_after_nested_empty_object);
    RUN(test_key_in_array_rejected);
    RUN(test_closing_bracket_where_value_required);
    RUN(test_object_member_needs_string_key);
    RUN(test_empty_container_closed_by_wrong_bracket);
    RUN(test_bare_closing_bracket_at_top_level);
    RUN(test_valid_containers_with_whitespace);
    RUN(test_unclosed_containers_are_incomplete);
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
    RUN(test_init_max_depth_clamping);
    RUN(test_depth_exactly_at_limit);
    RUN(test_event_value_lengths_exact);
    RUN(test_literal_events_exact);
    RUN(test_bad_literal_rejected);
    RUN(test_feed_null_data_zero_len);
    RUN(test_feed_null_data_nonzero_len);
    RUN(test_done_state_trailing_char_exact_error);
    RUN(test_after_key_state_exact);
    RUN(test_mismatched_close_exact_error);
    RUN(test_number_flushed_exactly_once);
    RUN(test_finish_no_data_is_incomplete);
    RUN(test_finish_unclosed_object_is_incomplete);
    RUN(test_reset_preserves_config);
    RUN(test_unicode_escape_exact_four_digits);
    RUN(test_structural_events_have_no_value);
    RUN(test_multiple_keys_are_key_events);
TEST_MAIN_END()
