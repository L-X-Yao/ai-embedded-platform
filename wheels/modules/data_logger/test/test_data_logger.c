#include "test_harness.h"
#include "data_logger.h"
#include <string.h>
#include <stdio.h>

/* ---- Mock storage ---- */

static uint8_t store_buf[4096];
static uint16_t store_len;
static int write_called;
static int flush_called;
static data_logger_err_t write_retval;

static data_logger_err_t mock_write(void *ctx, const uint8_t *data,
                                    uint16_t len)
{
    (void)ctx;
    write_called++;
    if (write_retval != DATA_LOGGER_OK) {
        return write_retval;
    }
    if (store_len + len < sizeof(store_buf)) {
        memcpy(store_buf + store_len, data, len);
        store_len = (uint16_t)(store_len + len);
    }
    store_buf[store_len] = '\0';
    return DATA_LOGGER_OK;
}

static data_logger_err_t mock_flush(void *ctx)
{
    (void)ctx;
    flush_called++;
    return DATA_LOGGER_OK;
}

static void mock_reset(void)
{
    store_len = 0;
    store_buf[0] = '\0';
    write_called = 0;
    flush_called = 0;
    write_retval = DATA_LOGGER_OK;
}

static const data_logger_storage_t test_storage = {
    mock_write, mock_flush, NULL
};

/* ---- Helpers ---- */

static data_logger_t dl;

static const data_logger_field_t fields_int[] = {
    { "value", DATA_LOGGER_FIELD_INT32 },
};

static const data_logger_field_t fields_multi[] = {
    { "temp",    DATA_LOGGER_FIELD_INT32  },
    { "count",   DATA_LOGGER_FIELD_UINT32 },
    { "voltage", DATA_LOGGER_FIELD_FLOAT  },
    { "label",   DATA_LOGGER_FIELD_STRING },
};

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    mock_reset();
    data_logger_err_t err = data_logger_init(&dl, &test_storage,
                                             fields_int, 1);
    ASSERT_EQ(err, DATA_LOGGER_OK);
    ASSERT_EQ(dl.field_count, 1);
    ASSERT_EQ(dl.record_count, 0);
    ASSERT_EQ(dl.buf_pos, 0);
}

TEST(test_init_null_dl)
{
    data_logger_err_t err = data_logger_init(NULL, &test_storage,
                                             fields_int, 1);
    ASSERT_EQ(err, DATA_LOGGER_ERR_PARAMS);
}

TEST(test_init_null_storage)
{
    data_logger_err_t err = data_logger_init(&dl, NULL, fields_int, 1);
    ASSERT_EQ(err, DATA_LOGGER_ERR_PARAMS);
}

TEST(test_init_null_write_fn)
{
    static const data_logger_storage_t bad = { NULL, NULL, NULL };
    data_logger_err_t err = data_logger_init(&dl, &bad, fields_int, 1);
    ASSERT_EQ(err, DATA_LOGGER_ERR_PARAMS);
}

TEST(test_init_null_fields_nonzero_count)
{
    mock_reset();
    data_logger_err_t err = data_logger_init(&dl, &test_storage, NULL, 3);
    ASSERT_EQ(err, DATA_LOGGER_ERR_PARAMS);
}

TEST(test_init_zero_fields)
{
    mock_reset();
    data_logger_err_t err = data_logger_init(&dl, &test_storage, NULL, 0);
    ASSERT_EQ(err, DATA_LOGGER_OK);
    ASSERT_EQ(dl.field_count, 0);
}

TEST(test_single_int_record)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);

    ASSERT_EQ(data_logger_begin(&dl, 1000), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_add_int(&dl, -42), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_commit(&dl), DATA_LOGGER_OK);

    /* expected CSV: "1000,-42\n" */
    ASSERT_TRUE(strcmp((char *)store_buf, "1000,-42\n") == 0);
}

TEST(test_multi_field_record)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_multi, 4);

    ASSERT_EQ(data_logger_begin(&dl, 5000), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_add_int(&dl, 25), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_add_uint(&dl, 100), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_add_float(&dl, 3.14f), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_add_string(&dl, "OK"), DATA_LOGGER_OK);
    ASSERT_EQ(data_logger_commit(&dl), DATA_LOGGER_OK);

    ASSERT_TRUE(strcmp((char *)store_buf, "5000,25,100,3.14,OK\n") == 0);
}

TEST(test_timestamp_in_record)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);

    data_logger_begin(&dl, 99999);
    data_logger_add_int(&dl, 0);
    data_logger_commit(&dl);

    /* timestamp 99999 should be the first field */
    ASSERT_TRUE(memcmp(store_buf, "99999,", 6) == 0);
}

TEST(test_record_count_increments)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);
    ASSERT_EQ(data_logger_get_count(&dl), 0);

    data_logger_begin(&dl, 1);
    data_logger_add_int(&dl, 10);
    data_logger_commit(&dl);
    ASSERT_EQ(data_logger_get_count(&dl), 1);

    data_logger_begin(&dl, 2);
    data_logger_add_int(&dl, 20);
    data_logger_commit(&dl);
    ASSERT_EQ(data_logger_get_count(&dl), 2);

    data_logger_begin(&dl, 3);
    data_logger_add_int(&dl, 30);
    data_logger_commit(&dl);
    ASSERT_EQ(data_logger_get_count(&dl), 3);
}

TEST(test_buffer_overflow_long_string)
{
    mock_reset();
    static const data_logger_field_t f[] = {
        { "msg", DATA_LOGGER_FIELD_STRING },
    };
    /* Use a very small buffer by testing with a string that exceeds it.
     * Default DATA_LOGGER_BUF_SIZE is 256; a 300-char string will overflow.
     */
    data_logger_init(&dl, &test_storage, f, 1);

    data_logger_begin(&dl, 1);
    char long_str[300];
    memset(long_str, 'X', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    data_logger_err_t err = data_logger_add_string(&dl, long_str);
    ASSERT_EQ(err, DATA_LOGGER_ERR_OVERFLOW);
}

TEST(test_flush_calls_storage)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);

    ASSERT_EQ(flush_called, 0);
    ASSERT_EQ(data_logger_flush(&dl), DATA_LOGGER_OK);
    ASSERT_EQ(flush_called, 1);
}

TEST(test_flush_no_callback)
{
    mock_reset();
    static const data_logger_storage_t no_flush = {
        mock_write, NULL, NULL
    };
    data_logger_init(&dl, &no_flush, fields_int, 1);

    /* flush with no callback should succeed silently */
    ASSERT_EQ(data_logger_flush(&dl), DATA_LOGGER_OK);
}

TEST(test_multiple_records_sequential)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);

    data_logger_begin(&dl, 100);
    data_logger_add_int(&dl, 1);
    data_logger_commit(&dl);

    data_logger_begin(&dl, 200);
    data_logger_add_int(&dl, 2);
    data_logger_commit(&dl);

    /* Both records stored sequentially */
    ASSERT_TRUE(strcmp((char *)store_buf, "100,1\n200,2\n") == 0);
    ASSERT_EQ(data_logger_get_count(&dl), 2);
}

TEST(test_float_positive)
{
    mock_reset();
    static const data_logger_field_t f[] = {
        { "val", DATA_LOGGER_FIELD_FLOAT },
    };
    data_logger_init(&dl, &test_storage, f, 1);

    data_logger_begin(&dl, 0);
    data_logger_add_float(&dl, 123.45f);
    data_logger_commit(&dl);

    /* snprintf %.2f rounds, check starts-with "0,123.4" */
    ASSERT_TRUE(strstr((char *)store_buf, "123.4") != NULL);
}

TEST(test_float_negative)
{
    mock_reset();
    static const data_logger_field_t f[] = {
        { "val", DATA_LOGGER_FIELD_FLOAT },
    };
    data_logger_init(&dl, &test_storage, f, 1);

    data_logger_begin(&dl, 0);
    data_logger_add_float(&dl, -9.99f);
    data_logger_commit(&dl);

    ASSERT_TRUE(strstr((char *)store_buf, "-9.99") != NULL);
}

TEST(test_float_zero)
{
    mock_reset();
    static const data_logger_field_t f[] = {
        { "val", DATA_LOGGER_FIELD_FLOAT },
    };
    data_logger_init(&dl, &test_storage, f, 1);

    data_logger_begin(&dl, 0);
    data_logger_add_float(&dl, 0.0f);
    data_logger_commit(&dl);

    ASSERT_TRUE(strcmp((char *)store_buf, "0,0.00\n") == 0);
}

TEST(test_string_plain)
{
    mock_reset();
    static const data_logger_field_t f[] = {
        { "msg", DATA_LOGGER_FIELD_STRING },
    };
    data_logger_init(&dl, &test_storage, f, 1);

    data_logger_begin(&dl, 42);
    data_logger_add_string(&dl, "hello");
    data_logger_commit(&dl);

    ASSERT_TRUE(strcmp((char *)store_buf, "42,hello\n") == 0);
}

TEST(test_too_many_fields)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);

    data_logger_begin(&dl, 0);
    ASSERT_EQ(data_logger_add_int(&dl, 1), DATA_LOGGER_OK);
    /* second add exceeds field_count=1 */
    ASSERT_EQ(data_logger_add_int(&dl, 2), DATA_LOGGER_ERR_PARAMS);
}

TEST(test_wrong_field_type)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);

    data_logger_begin(&dl, 0);
    /* fields_int[0] is INT32, but we try to add UINT32 */
    ASSERT_EQ(data_logger_add_uint(&dl, 1), DATA_LOGGER_ERR_PARAMS);
}

TEST(test_storage_write_failure)
{
    mock_reset();
    write_retval = DATA_LOGGER_ERR_WRITE;
    data_logger_init(&dl, &test_storage, fields_int, 1);

    data_logger_begin(&dl, 0);
    data_logger_add_int(&dl, 1);
    data_logger_err_t err = data_logger_commit(&dl);
    ASSERT_EQ(err, DATA_LOGGER_ERR_WRITE);

    /* record count should not increment on write failure */
    ASSERT_EQ(data_logger_get_count(&dl), 0);
}

TEST(test_begin_null)
{
    ASSERT_EQ(data_logger_begin(NULL, 0), DATA_LOGGER_ERR_PARAMS);
}

TEST(test_commit_without_begin)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);
    /* commit without begin should fail */
    ASSERT_EQ(data_logger_commit(&dl), DATA_LOGGER_ERR_PARAMS);
}

TEST(test_add_without_begin)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);
    /* add without begin should fail */
    ASSERT_EQ(data_logger_add_int(&dl, 1), DATA_LOGGER_ERR_PARAMS);
}

TEST(test_get_count_null)
{
    ASSERT_EQ(data_logger_get_count(NULL), 0);
}

TEST(test_flush_null)
{
    ASSERT_EQ(data_logger_flush(NULL), DATA_LOGGER_ERR_PARAMS);
}

TEST(test_add_string_null_value)
{
    mock_reset();
    static const data_logger_field_t f[] = {
        { "msg", DATA_LOGGER_FIELD_STRING },
    };
    data_logger_init(&dl, &test_storage, f, 1);
    data_logger_begin(&dl, 0);
    ASSERT_EQ(data_logger_add_string(&dl, NULL), DATA_LOGGER_ERR_PARAMS);
}

TEST(test_max_fields_exceeded_at_init)
{
    mock_reset();
    /* DATA_LOGGER_MAX_FIELDS is 8; try 9 */
    data_logger_field_t many[9];
    for (int i = 0; i < 9; i++) {
        many[i].name = "x";
        many[i].type = DATA_LOGGER_FIELD_INT32;
    }
    data_logger_err_t err = data_logger_init(&dl, &test_storage, many, 9);
    ASSERT_EQ(err, DATA_LOGGER_ERR_PARAMS);
}

TEST(test_zero_fields_timestamp_only)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, NULL, 0);

    data_logger_begin(&dl, 777);
    data_logger_commit(&dl);

    /* timestamp-only record */
    ASSERT_TRUE(strcmp((char *)store_buf, "777\n") == 0);
    ASSERT_EQ(data_logger_get_count(&dl), 1);
}

TEST(test_large_timestamp)
{
    mock_reset();
    data_logger_init(&dl, &test_storage, fields_int, 1);

    data_logger_begin(&dl, 4294967295UL);  /* UINT32_MAX */
    data_logger_add_int(&dl, 0);
    data_logger_commit(&dl);

    ASSERT_TRUE(strstr((char *)store_buf, "4294967295") != NULL);
}

/* ================================================================ */

TEST_MAIN("data_logger")
    RUN(test_init_valid);
    RUN(test_init_null_dl);
    RUN(test_init_null_storage);
    RUN(test_init_null_write_fn);
    RUN(test_init_null_fields_nonzero_count);
    RUN(test_init_zero_fields);
    RUN(test_single_int_record);
    RUN(test_multi_field_record);
    RUN(test_timestamp_in_record);
    RUN(test_record_count_increments);
    RUN(test_buffer_overflow_long_string);
    RUN(test_flush_calls_storage);
    RUN(test_flush_no_callback);
    RUN(test_multiple_records_sequential);
    RUN(test_float_positive);
    RUN(test_float_negative);
    RUN(test_float_zero);
    RUN(test_string_plain);
    RUN(test_too_many_fields);
    RUN(test_wrong_field_type);
    RUN(test_storage_write_failure);
    RUN(test_begin_null);
    RUN(test_commit_without_begin);
    RUN(test_add_without_begin);
    RUN(test_get_count_null);
    RUN(test_flush_null);
    RUN(test_add_string_null_value);
    RUN(test_max_fields_exceeded_at_init);
    RUN(test_zero_fields_timestamp_only);
    RUN(test_large_timestamp);
TEST_MAIN_END()
