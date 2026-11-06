// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "config_store.h"
#include <string.h>

/* ---- Mock storage ---- */

#define MOCK_MAX_ITEMS  16

typedef struct {
    char     key[32];
    uint8_t  data[sizeof(config_value_t)];
    uint16_t len;
    bool     used;
} mock_item_t;

static mock_item_t mock_store[MOCK_MAX_ITEMS];
static bool mock_fail_save;
static bool mock_fail_load;

static void mock_reset(void)
{
    memset(mock_store, 0, sizeof(mock_store));
    mock_fail_save = false;
    mock_fail_load = false;
}

static config_store_err_t mock_save(void *ctx, const char *key,
                                    const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (mock_fail_save) {
        return CONFIG_STORE_ERR_STORAGE;
    }
    for (int i = 0; i < MOCK_MAX_ITEMS; i++) {
        if (mock_store[i].used &&
            strcmp(mock_store[i].key, key) == 0) {
            memcpy(mock_store[i].data, data, len);
            mock_store[i].len = len;
            return CONFIG_STORE_OK;
        }
    }
    for (int i = 0; i < MOCK_MAX_ITEMS; i++) {
        if (!mock_store[i].used) {
            strncpy(mock_store[i].key, key,
                    sizeof(mock_store[i].key) - 1);
            memcpy(mock_store[i].data, data, len);
            mock_store[i].len  = len;
            mock_store[i].used = true;
            return CONFIG_STORE_OK;
        }
    }
    return CONFIG_STORE_ERR_STORAGE;
}

static config_store_err_t mock_load(void *ctx, const char *key,
                                    uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (mock_fail_load) {
        return CONFIG_STORE_ERR_STORAGE;
    }
    for (int i = 0; i < MOCK_MAX_ITEMS; i++) {
        if (mock_store[i].used &&
            strcmp(mock_store[i].key, key) == 0) {
            uint16_t copy_len = len < mock_store[i].len
                                ? len : mock_store[i].len;
            memcpy(data, mock_store[i].data, copy_len);
            return CONFIG_STORE_OK;
        }
    }
    return CONFIG_STORE_ERR_NOT_FOUND;
}

static const config_store_storage_t mock_storage = {
    mock_save, mock_load, NULL
};

/* ---- Helpers ---- */

static config_store_t cs;

static void setup(const config_store_storage_t *storage)
{
    mock_reset();
    config_store_init(&cs, storage);
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_init_valid)
{
    config_store_err_t err = config_store_init(&cs, NULL);
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_EQ(config_store_count(&cs), 0);
}

TEST(test_init_null_store)
{
    config_store_err_t err = config_store_init(NULL, NULL);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);
}

TEST(test_define_int_and_get_default)
{
    setup(NULL);
    config_store_err_t err = config_store_define_int(&cs, "volume",
                                                     50, 0, 100);
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_EQ(config_store_count(&cs), 1);

    int32_t val = 0;
    err = config_store_get_int(&cs, "volume", &val);
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_EQ(val, 50);
}

TEST(test_define_uint_and_get_default)
{
    setup(NULL);
    config_store_err_t err = config_store_define_uint(&cs, "baud",
                                                      115200, 9600,
                                                      921600);
    ASSERT_EQ(err, CONFIG_STORE_OK);

    uint32_t val = 0;
    err = config_store_get_uint(&cs, "baud", &val);
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_EQ(val, 115200);
}

TEST(test_define_float_and_get_default)
{
    setup(NULL);
    config_store_err_t err = config_store_define_float(&cs, "gain",
                                                       1.0f, 0.0f,
                                                       10.0f);
    ASSERT_EQ(err, CONFIG_STORE_OK);

    float val = 0.0f;
    err = config_store_get_float(&cs, "gain", &val);
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_NEAR(val, 1.0f, 0.001f);
}

TEST(test_define_bool_and_get_default)
{
    setup(NULL);
    config_store_err_t err = config_store_define_bool(&cs, "debug",
                                                      true);
    ASSERT_EQ(err, CONFIG_STORE_OK);

    bool val = false;
    err = config_store_get_bool(&cs, "debug", &val);
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_TRUE(val);
}

TEST(test_define_string_and_get_default)
{
    setup(NULL);
    config_store_err_t err = config_store_define_string(&cs, "name",
                                                        "device-1");
    ASSERT_EQ(err, CONFIG_STORE_OK);

    char val[32] = {0};
    err = config_store_get_string(&cs, "name", val, sizeof(val));
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_TRUE(strcmp(val, "device-1") == 0);
}

TEST(test_set_int_and_get)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    config_store_err_t err = config_store_set_int(&cs, "volume", 75);
    ASSERT_EQ(err, CONFIG_STORE_OK);

    int32_t val = 0;
    err = config_store_get_int(&cs, "volume", &val);
    ASSERT_EQ(err, CONFIG_STORE_OK);
    ASSERT_EQ(val, 75);
}

TEST(test_set_int_range_violation)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    config_store_err_t err = config_store_set_int(&cs, "volume", 101);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);

    err = config_store_set_int(&cs, "volume", -1);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);

    /* value should remain at default */
    int32_t val = 0;
    config_store_get_int(&cs, "volume", &val);
    ASSERT_EQ(val, 50);
}

TEST(test_set_uint_range_violation)
{
    setup(NULL);
    config_store_define_uint(&cs, "baud", 115200, 9600, 921600);

    config_store_err_t err = config_store_set_uint(&cs, "baud", 1000);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);

    err = config_store_set_uint(&cs, "baud", 1000000);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);
}

TEST(test_set_float_range_violation)
{
    setup(NULL);
    config_store_define_float(&cs, "gain", 1.0f, 0.0f, 10.0f);

    config_store_err_t err = config_store_set_float(&cs, "gain", -0.1f);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);

    err = config_store_set_float(&cs, "gain", 10.1f);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);
}

TEST(test_type_mismatch_set_int_on_float)
{
    setup(NULL);
    config_store_define_float(&cs, "gain", 1.0f, 0.0f, 10.0f);

    config_store_err_t err = config_store_set_int(&cs, "gain", 5);
    ASSERT_EQ(err, CONFIG_STORE_ERR_TYPE);
}

TEST(test_type_mismatch_get_uint_on_bool)
{
    setup(NULL);
    config_store_define_bool(&cs, "debug", true);

    uint32_t val = 0;
    config_store_err_t err = config_store_get_uint(&cs, "debug", &val);
    ASSERT_EQ(err, CONFIG_STORE_ERR_TYPE);
}

TEST(test_type_mismatch_set_bool_on_string)
{
    setup(NULL);
    config_store_define_string(&cs, "name", "dev");

    config_store_err_t err = config_store_set_bool(&cs, "name", true);
    ASSERT_EQ(err, CONFIG_STORE_ERR_TYPE);
}

TEST(test_not_found)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    int32_t val = 0;
    config_store_err_t err = config_store_get_int(&cs, "nosuch", &val);
    ASSERT_EQ(err, CONFIG_STORE_ERR_NOT_FOUND);

    err = config_store_set_int(&cs, "nosuch", 10);
    ASSERT_EQ(err, CONFIG_STORE_ERR_NOT_FOUND);
}

TEST(test_max_entries)
{
    setup(NULL);
    char keys[CONFIG_STORE_MAX_ENTRIES + 1][16];

    for (int i = 0; i < CONFIG_STORE_MAX_ENTRIES; i++) {
        snprintf(keys[i], sizeof(keys[i]), "key%d", i);
        config_store_err_t err = config_store_define_int(
            &cs, keys[i], i, 0, 1000);
        ASSERT_EQ(err, CONFIG_STORE_OK);
    }
    ASSERT_EQ(config_store_count(&cs), CONFIG_STORE_MAX_ENTRIES);

    /* next define should fail */
    snprintf(keys[CONFIG_STORE_MAX_ENTRIES],
             sizeof(keys[CONFIG_STORE_MAX_ENTRIES]), "overflow");
    config_store_err_t err = config_store_define_int(
        &cs, keys[CONFIG_STORE_MAX_ENTRIES], 0, 0, 100);
    ASSERT_EQ(err, CONFIG_STORE_ERR_FULL);
}

TEST(test_modified_flag)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    /* initially not modified */
    ASSERT_FALSE(cs.entries[0].modified);

    /* set to different value */
    config_store_set_int(&cs, "volume", 75);
    ASSERT_TRUE(cs.entries[0].modified);

    /* set back to default */
    config_store_set_int(&cs, "volume", 50);
    ASSERT_FALSE(cs.entries[0].modified);
}

TEST(test_modified_flag_string)
{
    setup(NULL);
    config_store_define_string(&cs, "name", "dev");

    ASSERT_FALSE(cs.entries[0].modified);

    config_store_set_string(&cs, "name", "prod");
    ASSERT_TRUE(cs.entries[0].modified);

    config_store_set_string(&cs, "name", "dev");
    ASSERT_FALSE(cs.entries[0].modified);
}

TEST(test_save_load_roundtrip)
{
    setup(&mock_storage);
    config_store_define_int(&cs, "volume", 50, 0, 100);
    config_store_define_uint(&cs, "baud", 115200, 9600, 921600);
    config_store_define_bool(&cs, "debug", false);
    config_store_define_string(&cs, "name", "dev");

    /* change values */
    config_store_set_int(&cs, "volume", 80);
    config_store_set_uint(&cs, "baud", 9600);
    config_store_set_bool(&cs, "debug", true);
    config_store_set_string(&cs, "name", "prod");

    /* save */
    config_store_err_t err = config_store_save_all(&cs);
    ASSERT_EQ(err, CONFIG_STORE_OK);

    /* reset to defaults */
    config_store_reset_defaults(&cs);
    int32_t vi = 0;
    config_store_get_int(&cs, "volume", &vi);
    ASSERT_EQ(vi, 50);  /* back to default */

    /* load */
    err = config_store_load_all(&cs);
    ASSERT_EQ(err, CONFIG_STORE_OK);

    config_store_get_int(&cs, "volume", &vi);
    ASSERT_EQ(vi, 80);

    uint32_t vu = 0;
    config_store_get_uint(&cs, "baud", &vu);
    ASSERT_EQ(vu, 9600);

    bool vb = false;
    config_store_get_bool(&cs, "debug", &vb);
    ASSERT_TRUE(vb);

    char vs[32] = {0};
    config_store_get_string(&cs, "name", vs, sizeof(vs));
    ASSERT_TRUE(strcmp(vs, "prod") == 0);
}

TEST(test_save_no_storage)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    config_store_err_t err = config_store_save_all(&cs);
    ASSERT_EQ(err, CONFIG_STORE_ERR_STORAGE);
}

TEST(test_load_no_storage)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    config_store_err_t err = config_store_load_all(&cs);
    ASSERT_EQ(err, CONFIG_STORE_ERR_STORAGE);
}

TEST(test_save_failure)
{
    setup(&mock_storage);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    mock_fail_save = true;
    config_store_err_t err = config_store_save_all(&cs);
    ASSERT_EQ(err, CONFIG_STORE_ERR_STORAGE);
}

TEST(test_load_failure)
{
    setup(&mock_storage);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    mock_fail_load = true;
    config_store_err_t err = config_store_load_all(&cs);
    ASSERT_EQ(err, CONFIG_STORE_ERR_STORAGE);
}

/* Stores a value the way erased flash or a corrupt record would look. */
static void mock_put_raw(const char *key, uint8_t fill)
{
    uint8_t raw[sizeof(config_value_t)];
    memset(raw, fill, sizeof(raw));
    ASSERT_EQ(mock_save(NULL, key, raw, (uint16_t)sizeof(raw)),
              CONFIG_STORE_OK);
}

/* A bool is 0 or 1: any other stored byte is out of range, like an int
 * outside min..max, so the entry keeps its value. */
TEST(test_load_bool_rejects_invalid_byte)
{
    setup(&mock_storage);
    config_store_define_bool(&cs, "flag", false);

    mock_put_raw("flag", 0xFFU);
    ASSERT_EQ(config_store_load_all(&cs), CONFIG_STORE_OK);

    uint8_t stored;
    memcpy(&stored, &cs.entries[0].current, 1);
    ASSERT_EQ(stored, 0U);
    ASSERT_FALSE(cs.entries[0].modified);

    mock_put_raw("flag", 0x02U);
    ASSERT_EQ(config_store_load_all(&cs), CONFIG_STORE_OK);
    memcpy(&stored, &cs.entries[0].current, 1);
    ASSERT_EQ(stored, 0U);
    ASSERT_FALSE(cs.entries[0].modified);

    /* the two legal bytes still load */
    mock_put_raw("flag", 0x00U);
    ((uint8_t *)mock_store[0].data)[0] = 1U;
    ASSERT_EQ(config_store_load_all(&cs), CONFIG_STORE_OK);
    bool val = false;
    ASSERT_EQ(config_store_get_bool(&cs, "flag", &val), CONFIG_STORE_OK);
    ASSERT_TRUE(val);
    ASSERT_TRUE(cs.entries[0].modified);
}

/* Erased flash has no zero byte anywhere in the value. */
TEST(test_load_string_without_terminator_keeps_default)
{
    setup(&mock_storage);
    config_store_define_string(&cs, "name", "dflt");

    mock_put_raw("name", 0xFFU);
    ASSERT_EQ(config_store_load_all(&cs), CONFIG_STORE_OK);

    char val[CONFIG_STORE_STRING_SIZE];
    ASSERT_EQ(config_store_get_string(&cs, "name", val, sizeof(val)),
              CONFIG_STORE_OK);
    ASSERT_STR_EQ(val, "dflt");
    ASSERT_FALSE(cs.entries[0].modified);
}

/* The longest legal value ends on the last byte and still loads. */
TEST(test_load_string_full_length_loads)
{
    setup(&mock_storage);
    config_store_define_string(&cs, "name", "dflt");

    mock_put_raw("name", 'x');
    mock_store[0].data[CONFIG_STORE_STRING_SIZE - 1] = 0;
    ASSERT_EQ(config_store_load_all(&cs), CONFIG_STORE_OK);

    char val[CONFIG_STORE_STRING_SIZE];
    ASSERT_EQ(config_store_get_string(&cs, "name", val, sizeof(val)),
              CONFIG_STORE_OK);
    ASSERT_EQ(strlen(val), (size_t)(CONFIG_STORE_STRING_SIZE - 1));
    ASSERT_TRUE(cs.entries[0].modified);
}

TEST(test_reset_defaults)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);
    config_store_define_bool(&cs, "debug", false);

    config_store_set_int(&cs, "volume", 99);
    config_store_set_bool(&cs, "debug", true);

    config_store_err_t err = config_store_reset_defaults(&cs);
    ASSERT_EQ(err, CONFIG_STORE_OK);

    int32_t vi = 0;
    config_store_get_int(&cs, "volume", &vi);
    ASSERT_EQ(vi, 50);

    bool vb = true;
    config_store_get_bool(&cs, "debug", &vb);
    ASSERT_FALSE(vb);

    ASSERT_FALSE(cs.entries[0].modified);
    ASSERT_FALSE(cs.entries[1].modified);
}

TEST(test_null_params_define)
{
    setup(NULL);
    config_store_err_t err;

    err = config_store_define_int(NULL, "x", 0, 0, 10);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);

    err = config_store_define_int(&cs, NULL, 0, 0, 10);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);

    err = config_store_define_string(&cs, NULL, "x");
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);

    err = config_store_define_string(&cs, "x", NULL);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);
}

TEST(test_null_params_get_set)
{
    setup(NULL);
    config_store_define_int(&cs, "volume", 50, 0, 100);
    config_store_err_t err;

    err = config_store_set_int(NULL, "volume", 50);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);

    err = config_store_set_int(&cs, NULL, 50);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);

    err = config_store_get_int(NULL, "volume", NULL);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);

    int32_t val = 0;
    err = config_store_get_int(&cs, NULL, &val);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);

    err = config_store_get_int(&cs, "volume", NULL);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);
}

TEST(test_string_set_too_long)
{
    setup(NULL);
    config_store_define_string(&cs, "name", "ok");

    /* build a string longer than CONFIG_STORE_STRING_SIZE */
    char long_str[CONFIG_STORE_STRING_SIZE + 10];
    memset(long_str, 'x', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    config_store_err_t err = config_store_set_string(&cs, "name",
                                                     long_str);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);
}

TEST(test_string_define_too_long)
{
    setup(NULL);

    char long_str[CONFIG_STORE_STRING_SIZE + 10];
    memset(long_str, 'y', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';

    config_store_err_t err = config_store_define_string(&cs, "name",
                                                        long_str);
    ASSERT_EQ(err, CONFIG_STORE_ERR_RANGE);
}

TEST(test_count_null)
{
    ASSERT_EQ(config_store_count(NULL), 0);
}

TEST(test_reset_null)
{
    config_store_err_t err = config_store_reset_defaults(NULL);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);
}

TEST(test_save_null)
{
    config_store_err_t err = config_store_save_all(NULL);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);
}

TEST(test_load_null)
{
    config_store_err_t err = config_store_load_all(NULL);
    ASSERT_EQ(err, CONFIG_STORE_ERR_PARAM);
}

TEST(test_load_updates_modified_flag)
{
    setup(&mock_storage);
    config_store_define_int(&cs, "volume", 50, 0, 100);

    /* save with modified value */
    config_store_set_int(&cs, "volume", 80);
    config_store_save_all(&cs);

    /* reset and reload */
    config_store_reset_defaults(&cs);
    ASSERT_FALSE(cs.entries[0].modified);

    config_store_load_all(&cs);
    ASSERT_TRUE(cs.entries[0].modified);  /* loaded != default */
}

TEST(test_multiple_entries_independent)
{
    setup(NULL);
    config_store_define_int(&cs, "a", 10, 0, 100);
    config_store_define_int(&cs, "b", 20, 0, 100);

    config_store_set_int(&cs, "a", 99);

    int32_t va = 0, vb = 0;
    config_store_get_int(&cs, "a", &va);
    config_store_get_int(&cs, "b", &vb);
    ASSERT_EQ(va, 99);
    ASSERT_EQ(vb, 20);  /* b unchanged */
}

/* ================================================================ */

TEST_MAIN("config_store")
    RUN(test_init_valid);
    RUN(test_init_null_store);
    RUN(test_define_int_and_get_default);
    RUN(test_define_uint_and_get_default);
    RUN(test_define_float_and_get_default);
    RUN(test_define_bool_and_get_default);
    RUN(test_define_string_and_get_default);
    RUN(test_set_int_and_get);
    RUN(test_set_int_range_violation);
    RUN(test_set_uint_range_violation);
    RUN(test_set_float_range_violation);
    RUN(test_type_mismatch_set_int_on_float);
    RUN(test_type_mismatch_get_uint_on_bool);
    RUN(test_type_mismatch_set_bool_on_string);
    RUN(test_not_found);
    RUN(test_max_entries);
    RUN(test_modified_flag);
    RUN(test_modified_flag_string);
    RUN(test_save_load_roundtrip);
    RUN(test_save_no_storage);
    RUN(test_load_no_storage);
    RUN(test_save_failure);
    RUN(test_load_failure);
    RUN(test_load_bool_rejects_invalid_byte);
    RUN(test_load_string_without_terminator_keeps_default);
    RUN(test_load_string_full_length_loads);
    RUN(test_reset_defaults);
    RUN(test_null_params_define);
    RUN(test_null_params_get_set);
    RUN(test_string_set_too_long);
    RUN(test_string_define_too_long);
    RUN(test_count_null);
    RUN(test_reset_null);
    RUN(test_save_null);
    RUN(test_load_null);
    RUN(test_load_updates_modified_flag);
    RUN(test_multiple_entries_independent);
TEST_MAIN_END()
