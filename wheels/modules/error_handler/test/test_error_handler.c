// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "error_handler.h"
#include <string.h>

/* ---- Mock storage ---- */

static error_record_t saved_records[ERROR_HANDLER_MAX_RECORDS];
static uint8_t saved_count;
static int save_called;
static int load_called;
static error_handler_err_t save_retval;
static error_handler_err_t load_retval;

/* Records pre-loaded into mock storage for load tests */
static error_record_t preloaded_records[ERROR_HANDLER_MAX_RECORDS];
static uint8_t preloaded_count;

static error_handler_err_t mock_save(void *ctx, const error_record_t *records,
                                      uint8_t count)
{
    (void)ctx;
    save_called = 1;
    if (save_retval != ERROR_HANDLER_OK) {
        return save_retval;
    }
    saved_count = count;
    if (count > 0) {
        memcpy(saved_records, records, (size_t)count * sizeof(error_record_t));
    }
    return ERROR_HANDLER_OK;
}

static error_handler_err_t mock_load(void *ctx, error_record_t *records,
                                      uint8_t max, uint8_t *count)
{
    (void)ctx;
    load_called = 1;
    if (load_retval != ERROR_HANDLER_OK) {
        return load_retval;
    }
    uint8_t n = (preloaded_count < max) ? preloaded_count : max;
    if (n > 0) {
        memcpy(records, preloaded_records, (size_t)n * sizeof(error_record_t));
    }
    *count = n;
    return ERROR_HANDLER_OK;
}

static const error_handler_storage_t mock_storage = {
    mock_save, mock_load, NULL
};

static void reset_mock(void)
{
    memset(saved_records, 0, sizeof(saved_records));
    saved_count = 0;
    save_called = 0;
    load_called = 0;
    save_retval = ERROR_HANDLER_OK;
    load_retval = ERROR_HANDLER_OK;
    memset(preloaded_records, 0, sizeof(preloaded_records));
    preloaded_count = 0;
}

/* ---- Fatal callback tracker ---- */

static int fatal_called;
static uint16_t fatal_code;
static void *fatal_ctx_received;

static void fatal_cb(void *ctx, const error_record_t *record)
{
    fatal_called = 1;
    fatal_code = record->code;
    fatal_ctx_received = ctx;
}

static void reset_fatal(void)
{
    fatal_called = 0;
    fatal_code = 0;
    fatal_ctx_received = NULL;
}

/* ---- Helpers ---- */

static error_handler_t eh;

static void setup(const error_handler_storage_t *storage)
{
    reset_mock();
    reset_fatal();
    error_handler_init(&eh, storage);
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    error_handler_err_t err = error_handler_init(&eh, NULL);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_EQ(eh.count, 0);
    ASSERT_EQ(eh.head, 0);
    ASSERT_EQ(eh.total_errors, 0);
    ASSERT_EQ(eh.total_fatals, 0);
    ASSERT_TRUE(eh.storage == NULL);
    ASSERT_TRUE(eh.on_fatal == NULL);
}

TEST(test_init_null_handler)
{
    error_handler_err_t err = error_handler_init(NULL, NULL);
    ASSERT_EQ(err, ERROR_HANDLER_ERR_PARAM);
}

TEST(test_init_with_storage)
{
    error_handler_err_t err = error_handler_init(&eh, &mock_storage);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_TRUE(eh.storage == &mock_storage);
}

TEST(test_report_and_get)
{
    setup(NULL);
    error_handler_err_t err = error_handler_report(&eh, 100,
                                                    ERROR_SEVERITY_WARNING,
                                                    1000, 0xDEAD);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_EQ(error_handler_get_count(&eh), 1);

    error_record_t rec;
    err = error_handler_get(&eh, 0, &rec);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_EQ(rec.code, 100);
    ASSERT_EQ(rec.severity, ERROR_SEVERITY_WARNING);
    ASSERT_EQ(rec.timestamp, 1000);
    ASSERT_EQ_HEX(rec.context, 0xDEAD);
}

TEST(test_report_null_handler)
{
    error_handler_err_t err = error_handler_report(NULL, 1,
                                                    ERROR_SEVERITY_ERROR,
                                                    0, 0);
    ASSERT_EQ(err, ERROR_HANDLER_ERR_PARAM);
}

TEST(test_ring_buffer_wraparound)
{
    setup(NULL);

    /* Fill buffer completely */
    for (uint16_t i = 0; i < ERROR_HANDLER_MAX_RECORDS; i++) {
        error_handler_report(&eh, i, ERROR_SEVERITY_INFO, i * 10, 0);
    }
    ASSERT_EQ(error_handler_get_count(&eh), ERROR_HANDLER_MAX_RECORDS);

    /* Add one more -- should overwrite oldest (code=0) */
    error_handler_report(&eh, 999, ERROR_SEVERITY_ERROR, 9990, 0);
    ASSERT_EQ(error_handler_get_count(&eh), ERROR_HANDLER_MAX_RECORDS);

    /* Oldest should now be code=1 (code=0 was overwritten) */
    error_record_t rec;
    error_handler_get(&eh, 0, &rec);
    ASSERT_EQ(rec.code, 1);

    /* Newest should be code=999 */
    error_handler_get(&eh, (uint8_t)(ERROR_HANDLER_MAX_RECORDS - 1), &rec);
    ASSERT_EQ(rec.code, 999);
}

TEST(test_fatal_callback_triggered)
{
    setup(NULL);
    static int ctx_val = 42;
    error_handler_set_fatal_callback(&eh, fatal_cb, &ctx_val);

    /* Non-fatal should not trigger */
    error_handler_report(&eh, 1, ERROR_SEVERITY_ERROR, 100, 0);
    ASSERT_FALSE(fatal_called);

    /* Fatal should trigger */
    error_handler_report(&eh, 2, ERROR_SEVERITY_FATAL, 200, 0);
    ASSERT_TRUE(fatal_called);
    ASSERT_EQ(fatal_code, 2);
    ASSERT_TRUE(fatal_ctx_received == &ctx_val);
}

TEST(test_fatal_callback_not_set)
{
    setup(NULL);
    /* No callback set -- fatal should not crash */
    error_handler_err_t err = error_handler_report(&eh, 1,
                                                    ERROR_SEVERITY_FATAL,
                                                    100, 0);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_EQ(eh.total_fatals, 1);
}

TEST(test_severity_levels)
{
    setup(NULL);
    error_handler_report(&eh, 1, ERROR_SEVERITY_INFO, 100, 0);
    error_handler_report(&eh, 2, ERROR_SEVERITY_WARNING, 200, 0);
    error_handler_report(&eh, 3, ERROR_SEVERITY_ERROR, 300, 0);
    error_handler_report(&eh, 4, ERROR_SEVERITY_FATAL, 400, 0);

    error_record_t rec;
    error_handler_get(&eh, 0, &rec);
    ASSERT_EQ(rec.severity, ERROR_SEVERITY_INFO);
    error_handler_get(&eh, 1, &rec);
    ASSERT_EQ(rec.severity, ERROR_SEVERITY_WARNING);
    error_handler_get(&eh, 2, &rec);
    ASSERT_EQ(rec.severity, ERROR_SEVERITY_ERROR);
    error_handler_get(&eh, 3, &rec);
    ASSERT_EQ(rec.severity, ERROR_SEVERITY_FATAL);
}

TEST(test_total_counters)
{
    setup(NULL);
    error_handler_report(&eh, 1, ERROR_SEVERITY_INFO, 100, 0);
    error_handler_report(&eh, 2, ERROR_SEVERITY_WARNING, 200, 0);
    error_handler_report(&eh, 3, ERROR_SEVERITY_FATAL, 300, 0);
    error_handler_report(&eh, 4, ERROR_SEVERITY_ERROR, 400, 0);
    error_handler_report(&eh, 5, ERROR_SEVERITY_FATAL, 500, 0);

    ASSERT_EQ(error_handler_total_errors(&eh), 5);
    ASSERT_EQ(error_handler_total_fatals(&eh), 2);
}

TEST(test_total_counters_null)
{
    ASSERT_EQ(error_handler_total_errors(NULL), 0);
    ASSERT_EQ(error_handler_total_fatals(NULL), 0);
}

TEST(test_save_load_roundtrip)
{
    setup(&mock_storage);

    /* Report some errors */
    error_handler_report(&eh, 10, ERROR_SEVERITY_INFO, 100, 0xAA);
    error_handler_report(&eh, 20, ERROR_SEVERITY_WARNING, 200, 0xBB);
    error_handler_report(&eh, 30, ERROR_SEVERITY_ERROR, 300, 0xCC);

    /* Save */
    error_handler_err_t err = error_handler_save(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_TRUE(save_called);
    ASSERT_EQ(saved_count, 3);
    ASSERT_EQ(saved_records[0].code, 10);
    ASSERT_EQ(saved_records[1].code, 20);
    ASSERT_EQ(saved_records[2].code, 30);

    /* Prepare preloaded data for load */
    memcpy(preloaded_records, saved_records,
           (size_t)saved_count * sizeof(error_record_t));
    preloaded_count = saved_count;

    /* Clear and load */
    error_handler_clear(&eh);
    ASSERT_EQ(error_handler_get_count(&eh), 0);

    err = error_handler_load(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_TRUE(load_called);
    ASSERT_EQ(error_handler_get_count(&eh), 3);

    error_record_t rec;
    error_handler_get(&eh, 0, &rec);
    ASSERT_EQ(rec.code, 10);
    ASSERT_EQ_HEX(rec.context, 0xAA);
    error_handler_get(&eh, 2, &rec);
    ASSERT_EQ(rec.code, 30);
}

TEST(test_clear)
{
    setup(NULL);
    error_handler_report(&eh, 1, ERROR_SEVERITY_FATAL, 100, 0);
    error_handler_report(&eh, 2, ERROR_SEVERITY_ERROR, 200, 0);
    ASSERT_EQ(error_handler_get_count(&eh), 2);
    ASSERT_EQ(error_handler_total_errors(&eh), 2);
    ASSERT_EQ(error_handler_total_fatals(&eh), 1);

    error_handler_clear(&eh);
    ASSERT_EQ(error_handler_get_count(&eh), 0);
    ASSERT_EQ(error_handler_total_errors(&eh), 0);
    ASSERT_EQ(error_handler_total_fatals(&eh), 0);
    ASSERT_EQ(eh.head, 0);
}

TEST(test_clear_null)
{
    /* Should not crash */
    error_handler_clear(NULL);
    ASSERT_TRUE(1);
}

TEST(test_storage_error_save)
{
    setup(&mock_storage);
    save_retval = ERROR_HANDLER_ERR_STORAGE;
    error_handler_report(&eh, 1, ERROR_SEVERITY_INFO, 100, 0);

    error_handler_err_t err = error_handler_save(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_ERR_STORAGE);
}

TEST(test_storage_error_load)
{
    setup(&mock_storage);
    load_retval = ERROR_HANDLER_ERR_STORAGE;

    error_handler_err_t err = error_handler_load(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_ERR_STORAGE);
}

TEST(test_save_no_storage)
{
    setup(NULL);
    error_handler_err_t err = error_handler_save(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_ERR_STORAGE);
}

TEST(test_load_no_storage)
{
    setup(NULL);
    error_handler_err_t err = error_handler_load(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_ERR_STORAGE);
}

TEST(test_get_null_params)
{
    setup(NULL);
    error_handler_report(&eh, 1, ERROR_SEVERITY_INFO, 100, 0);

    error_record_t rec;
    ASSERT_EQ(error_handler_get(NULL, 0, &rec), ERROR_HANDLER_ERR_PARAM);
    ASSERT_EQ(error_handler_get(&eh, 0, NULL), ERROR_HANDLER_ERR_PARAM);
}

TEST(test_get_invalid_index)
{
    setup(NULL);
    error_handler_report(&eh, 1, ERROR_SEVERITY_INFO, 100, 0);

    error_record_t rec;
    /* Only 1 record, index 1 is out of range */
    ASSERT_EQ(error_handler_get(&eh, 1, &rec), ERROR_HANDLER_ERR_PARAM);
    /* Index 255 out of range */
    ASSERT_EQ(error_handler_get(&eh, 255, &rec), ERROR_HANDLER_ERR_PARAM);
}

TEST(test_get_empty_buffer)
{
    setup(NULL);
    error_record_t rec;
    ASSERT_EQ(error_handler_get(&eh, 0, &rec), ERROR_HANDLER_ERR_PARAM);
}

TEST(test_max_records_fill)
{
    setup(NULL);
    for (uint16_t i = 0; i < ERROR_HANDLER_MAX_RECORDS; i++) {
        error_handler_report(&eh, i, ERROR_SEVERITY_INFO, i * 100, i);
    }
    ASSERT_EQ(error_handler_get_count(&eh), ERROR_HANDLER_MAX_RECORDS);
    ASSERT_EQ(error_handler_total_errors(&eh), ERROR_HANDLER_MAX_RECORDS);

    /* Verify all records accessible in order */
    for (uint8_t i = 0; i < ERROR_HANDLER_MAX_RECORDS; i++) {
        error_record_t rec;
        error_handler_err_t err = error_handler_get(&eh, i, &rec);
        ASSERT_EQ(err, ERROR_HANDLER_OK);
        ASSERT_EQ(rec.code, i);
    }
}

TEST(test_multiple_errors_sequence)
{
    setup(NULL);
    error_handler_report(&eh, 1, ERROR_SEVERITY_INFO, 100, 0x10);
    error_handler_report(&eh, 2, ERROR_SEVERITY_WARNING, 200, 0x20);
    error_handler_report(&eh, 3, ERROR_SEVERITY_ERROR, 300, 0x30);

    ASSERT_EQ(error_handler_get_count(&eh), 3);

    error_record_t rec;
    error_handler_get(&eh, 0, &rec);
    ASSERT_EQ(rec.code, 1);
    ASSERT_EQ(rec.timestamp, 100);
    ASSERT_EQ_HEX(rec.context, 0x10);

    error_handler_get(&eh, 1, &rec);
    ASSERT_EQ(rec.code, 2);
    ASSERT_EQ(rec.timestamp, 200);

    error_handler_get(&eh, 2, &rec);
    ASSERT_EQ(rec.code, 3);
    ASSERT_EQ(rec.timestamp, 300);
}

TEST(test_load_on_init)
{
    reset_mock();
    /* Preload 2 records */
    preloaded_records[0].code = 50;
    preloaded_records[0].severity = ERROR_SEVERITY_WARNING;
    preloaded_records[0].timestamp = 500;
    preloaded_records[0].context = 0x50;
    preloaded_records[1].code = 60;
    preloaded_records[1].severity = ERROR_SEVERITY_ERROR;
    preloaded_records[1].timestamp = 600;
    preloaded_records[1].context = 0x60;
    preloaded_count = 2;

    error_handler_init(&eh, &mock_storage);
    error_handler_err_t err = error_handler_load(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_EQ(error_handler_get_count(&eh), 2);

    error_record_t rec;
    error_handler_get(&eh, 0, &rec);
    ASSERT_EQ(rec.code, 50);
    ASSERT_EQ(rec.timestamp, 500);

    error_handler_get(&eh, 1, &rec);
    ASSERT_EQ(rec.code, 60);
    ASSERT_EQ(rec.timestamp, 600);
}

TEST(test_get_count_null)
{
    ASSERT_EQ(error_handler_get_count(NULL), 0);
}

TEST(test_set_fatal_callback_null_handler)
{
    /* Should not crash */
    error_handler_set_fatal_callback(NULL, fatal_cb, NULL);
    ASSERT_TRUE(1);
}

TEST(test_save_empty_buffer)
{
    setup(&mock_storage);
    error_handler_err_t err = error_handler_save(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_TRUE(save_called);
    ASSERT_EQ(saved_count, 0);
}

TEST(test_save_after_wraparound)
{
    setup(&mock_storage);

    /* Fill + 2 extra to wrap */
    for (uint16_t i = 0; i < ERROR_HANDLER_MAX_RECORDS + 2; i++) {
        error_handler_report(&eh, i, ERROR_SEVERITY_INFO, i * 10, 0);
    }

    error_handler_err_t err = error_handler_save(&eh);
    ASSERT_EQ(err, ERROR_HANDLER_OK);
    ASSERT_EQ(saved_count, ERROR_HANDLER_MAX_RECORDS);

    /* Saved records should be linearized oldest-first: codes 2..17 */
    ASSERT_EQ(saved_records[0].code, 2);
    ASSERT_EQ(saved_records[ERROR_HANDLER_MAX_RECORDS - 1].code,
              ERROR_HANDLER_MAX_RECORDS + 1);
}

TEST(test_report_after_load)
{
    reset_mock();
    preloaded_records[0].code = 100;
    preloaded_records[0].severity = ERROR_SEVERITY_INFO;
    preloaded_records[0].timestamp = 1000;
    preloaded_count = 1;

    error_handler_init(&eh, &mock_storage);
    error_handler_load(&eh);

    /* Report a new error after load */
    error_handler_report(&eh, 200, ERROR_SEVERITY_WARNING, 2000, 0);
    ASSERT_EQ(error_handler_get_count(&eh), 2);

    error_record_t rec;
    error_handler_get(&eh, 0, &rec);
    ASSERT_EQ(rec.code, 100);
    error_handler_get(&eh, 1, &rec);
    ASSERT_EQ(rec.code, 200);
}

TEST(test_save_null_handler)
{
    ASSERT_EQ(error_handler_save(NULL), ERROR_HANDLER_ERR_PARAM);
}

TEST(test_load_null_handler)
{
    ASSERT_EQ(error_handler_load(NULL), ERROR_HANDLER_ERR_PARAM);
}

/* ================================================================ */

TEST_MAIN("error_handler")
    RUN(test_init_valid);
    RUN(test_init_null_handler);
    RUN(test_init_with_storage);
    RUN(test_report_and_get);
    RUN(test_report_null_handler);
    RUN(test_ring_buffer_wraparound);
    RUN(test_fatal_callback_triggered);
    RUN(test_fatal_callback_not_set);
    RUN(test_severity_levels);
    RUN(test_total_counters);
    RUN(test_total_counters_null);
    RUN(test_save_load_roundtrip);
    RUN(test_clear);
    RUN(test_clear_null);
    RUN(test_storage_error_save);
    RUN(test_storage_error_load);
    RUN(test_save_no_storage);
    RUN(test_load_no_storage);
    RUN(test_get_null_params);
    RUN(test_get_invalid_index);
    RUN(test_get_empty_buffer);
    RUN(test_max_records_fill);
    RUN(test_multiple_errors_sequence);
    RUN(test_load_on_init);
    RUN(test_get_count_null);
    RUN(test_set_fatal_callback_null_handler);
    RUN(test_save_empty_buffer);
    RUN(test_save_after_wraparound);
    RUN(test_report_after_load);
    RUN(test_save_null_handler);
    RUN(test_load_null_handler);
TEST_MAIN_END()
