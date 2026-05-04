/*
 * prop_json_parser.c — property-based tests for json_parser
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_json_parser.c ../json_parser.c -o prop_json_parser
 */

#include "test_harness.h"
#include "property_test.h"
#include "../json_parser.h"
#include <string.h>

static int g_event_count;

static void counting_cb(void *ctx, json_event_t event,
                         const char *value, uint16_t value_len)
{
    (void)ctx; (void)event; (void)value; (void)value_len;
    g_event_count++;
}

/* Property: no-crash on arbitrary bytes */
PROPERTY(no_crash_arbitrary, 50000) {
    json_parser_t p;
    json_parser_init(&p, counting_cb, NULL, 16);

    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 127) + 1;
    ARBI_FILL(seed, data, len);

    g_event_count = 0;
    /* Feed arbitrary bytes — must not crash regardless of content */
    json_parser_feed(&p, (const char *)data, (uint16_t)len);
    json_parser_finish(&p);

    PROP_ASSERT(1); /* reached without crash */
}

/* Property: deterministic parse — same input always produces same event count */
PROPERTY(deterministic_parse, 10000) {
    uint8_t data[64];
    size_t len = ARBI_SIZE(seed, 63) + 1;
    uint32_t fill_seed = seed;
    ARBI_FILL(fill_seed, data, len);

    /* First parse */
    json_parser_t p1;
    json_parser_init(&p1, counting_cb, NULL, 16);
    g_event_count = 0;
    json_parser_feed(&p1, (const char *)data, (uint16_t)len);
    json_parser_finish(&p1);
    int count1 = g_event_count;

    /* Regenerate same data */
    uint32_t fill_seed2 = seed;
    uint8_t data2[64];
    ARBI_FILL(fill_seed2, data2, len);

    /* Second parse */
    json_parser_t p2;
    json_parser_init(&p2, counting_cb, NULL, 16);
    g_event_count = 0;
    json_parser_feed(&p2, (const char *)data2, (uint16_t)len);
    json_parser_finish(&p2);
    int count2 = g_event_count;

    PROP_ASSERT_EQ(count1, count2);
}

/* Property: reset then re-parse produces same result as fresh init */
PROPERTY(reset_idempotent, 10000) {
    /* Valid JSON to ensure non-trivial parsing */
    static const char *valid_jsons[] = {
        "{\"a\":1}", "[1,2,3]", "\"hello\"", "42", "true", "null",
        "{\"x\":[1,{\"y\":2}]}", "[]", "{}", "[null,false,true]",
    };
    int idx = ARBI_RANGE(seed, 0, 9);
    const char *json = valid_jsons[idx];
    uint16_t len = (uint16_t)strlen(json);

    /* First parse */
    json_parser_t p;
    json_parser_init(&p, counting_cb, NULL, 16);
    g_event_count = 0;
    json_parser_feed(&p, json, len);
    json_parser_finish(&p);
    int count1 = g_event_count;

    /* Reset and re-parse */
    json_parser_reset(&p);
    g_event_count = 0;
    json_parser_feed(&p, json, len);
    json_parser_finish(&p);
    int count2 = g_event_count;

    PROP_ASSERT_EQ(count1, count2);
}

/* Property: chunked feeding produces same result as single feed */
PROPERTY(chunked_same_as_whole, 5000) {
    static const char *valid_jsons[] = {
        "{\"key\":\"value\"}", "[1,2,3,4,5]", "{\"a\":{\"b\":true}}",
        "[\"hello\",null,42]", "{\"x\":1,\"y\":2,\"z\":3}",
    };
    int idx = ARBI_RANGE(seed, 0, 4);
    const char *json = valid_jsons[idx];
    uint16_t len = (uint16_t)strlen(json);

    /* Whole parse */
    json_parser_t pw;
    json_parser_init(&pw, counting_cb, NULL, 16);
    g_event_count = 0;
    json_parser_err_t err_w = json_parser_feed(&pw, json, len);
    json_parser_finish(&pw);
    int count_whole = g_event_count;

    /* Chunked parse — split at random point */
    if (len < 2) return;
    uint16_t split = (uint16_t)(1 + ARBI_SIZE(seed, (size_t)(len - 2)));
    json_parser_t pc;
    json_parser_init(&pc, counting_cb, NULL, 16);
    g_event_count = 0;
    json_parser_err_t err_c1 = json_parser_feed(&pc, json, split);
    json_parser_err_t err_c2 = JSON_PARSER_OK;
    if (err_c1 == JSON_PARSER_OK) {
        err_c2 = json_parser_feed(&pc, json + split, (uint16_t)(len - split));
    }
    json_parser_finish(&pc);
    int count_chunked = g_event_count;

    /* If whole parse succeeded, chunked should match */
    if (err_w == JSON_PARSER_OK && err_c1 == JSON_PARSER_OK && err_c2 == JSON_PARSER_OK) {
        PROP_ASSERT_EQ(count_whole, count_chunked);
    }
}

TEST_MAIN("json_parser properties") {
    RUN(no_crash_arbitrary);
    RUN(deterministic_parse);
    RUN(reset_idempotent);
    RUN(chunked_same_as_whole);
}
TEST_MAIN_END()
