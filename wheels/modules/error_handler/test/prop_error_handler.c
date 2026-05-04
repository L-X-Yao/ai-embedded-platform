// SPDX-License-Identifier: MIT
/*
 * prop_error_handler.c — property-based tests for error_handler module
 *
 * Properties:
 *   1. report+get roundtrip preserves error record fields
 *   2. total_errors count matches number of reports
 *   3. ring buffer wraps without crash at MAX_RECORDS
 *   4. fatal callback fires exactly for FATAL severity
 */

#include "test_harness.h"
#include "property_test.h"
#include "error_handler.h"
#include <string.h>

/* ---------- mock fatal callback ---------- */

static int fatal_called;
static uint16_t last_fatal_code;

static void mock_fatal_cb(void *ctx, const error_record_t *record)
{
    (void)ctx;
    fatal_called++;
    last_fatal_code = record->code;
}

static void mock_reset(void)
{
    fatal_called = 0;
    last_fatal_code = 0;
}

/* ------------------------------------------------------------------ */

PROPERTY(report_get_roundtrip, 5000)
{
    mock_reset();
    error_handler_t eh;
    error_handler_init(&eh, NULL);

    uint16_t code = ARBI_UINT16(seed);
    uint32_t ts = ARBI_UINT32(seed);
    uint32_t ctx_val = ARBI_UINT32(seed);
    int sev_raw = (int)(ARBI_UINT8(seed) % 3); /* INFO, WARNING, ERROR */
    error_severity_t sev = (error_severity_t)sev_raw;

    error_handler_report(&eh, code, sev, ts, ctx_val);

    error_record_t rec;
    error_handler_err_t err = error_handler_get(&eh, 0, &rec);
    PROP_ASSERT(err == ERROR_HANDLER_OK);
    PROP_ASSERT_EQ(rec.code, code);
    PROP_ASSERT_EQ(rec.timestamp, ts);
    PROP_ASSERT_EQ(rec.context, ctx_val);
    PROP_ASSERT_EQ((int)rec.severity, sev_raw);
}

PROPERTY(total_count_accurate, 5000)
{
    mock_reset();
    error_handler_t eh;
    error_handler_init(&eh, NULL);

    int n = (int)(1 + ARBI_UINT8(seed) % 50);
    for (int i = 0; i < n; i++) {
        uint16_t code = ARBI_UINT16(seed);
        error_severity_t sev = (error_severity_t)(ARBI_UINT8(seed) % 4);
        error_handler_report(&eh, code, sev, (uint32_t)i, 0);
    }

    PROP_ASSERT_EQ(error_handler_total_errors(&eh), (uint32_t)n);
    /* get_count should be min(n, MAX_RECORDS) since ring buffer wraps */
    uint8_t expected_count = (uint8_t)(n < ERROR_HANDLER_MAX_RECORDS
                                       ? n : ERROR_HANDLER_MAX_RECORDS);
    PROP_ASSERT_EQ(error_handler_get_count(&eh), expected_count);
}

PROPERTY(ring_wrap_no_crash, 5000)
{
    mock_reset();
    error_handler_t eh;
    error_handler_init(&eh, NULL);

    /* Report more than MAX_RECORDS to force ring buffer wrap */
    int n = ERROR_HANDLER_MAX_RECORDS + (int)(ARBI_UINT8(seed) % 32) + 1;
    for (int i = 0; i < n; i++) {
        error_handler_report(&eh, ARBI_UINT16(seed),
                             (error_severity_t)(ARBI_UINT8(seed) % 4),
                             ARBI_UINT32(seed), ARBI_UINT32(seed));
    }

    /* Read all stored records — must not crash */
    uint8_t stored = error_handler_get_count(&eh);
    PROP_ASSERT(stored <= ERROR_HANDLER_MAX_RECORDS);
    for (uint8_t i = 0; i < stored; i++) {
        error_record_t rec;
        error_handler_err_t err = error_handler_get(&eh, i, &rec);
        PROP_ASSERT(err == ERROR_HANDLER_OK);
    }
}

PROPERTY(fatal_callback_fires, 5000)
{
    mock_reset();
    error_handler_t eh;
    error_handler_init(&eh, NULL);
    error_handler_set_fatal_callback(&eh, mock_fatal_cb, NULL);

    int n = (int)(1 + ARBI_UINT8(seed) % 20);
    int expected_fatals = 0;
    uint16_t last_code = 0;

    for (int i = 0; i < n; i++) {
        uint16_t code = ARBI_UINT16(seed);
        error_severity_t sev = (error_severity_t)(ARBI_UINT8(seed) % 4);
        if (sev == ERROR_SEVERITY_FATAL) {
            expected_fatals++;
            last_code = code;
        }
        error_handler_report(&eh, code, sev, (uint32_t)i, 0);
    }

    PROP_ASSERT_EQ(fatal_called, expected_fatals);
    PROP_ASSERT_EQ(error_handler_total_fatals(&eh), (uint32_t)expected_fatals);
    if (expected_fatals > 0) {
        PROP_ASSERT_EQ(last_fatal_code, last_code);
    }
}

TEST_MAIN("prop_error_handler")
    RUN(report_get_roundtrip);
    RUN(total_count_accurate);
    RUN(ring_wrap_no_crash);
    RUN(fatal_callback_fires);
TEST_MAIN_END()
