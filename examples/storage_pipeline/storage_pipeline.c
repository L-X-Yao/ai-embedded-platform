// SPDX-License-Identifier: MIT
/*
 * storage_pipeline -- integration recipe: sensor-to-KVS data pipeline
 *
 * Demonstrates seven wheels working together:
 *   L0  fal            -- flash abstraction layer (RAM-backed mock)
 *   L0  kvs (FlashDB)  -- persistent key-value store on fal
 *   L0  fifo_queue     -- fixed-size FIFO for buffering sensor samples
 *   L0  byte_buffer    -- linear buffer for packet assembly
 *   L0  string_utils   -- string formatting / parsing helpers
 *   L0  hash_fnv       -- FNV-1a hash for key generation
 *   L1  at24cxx        -- I2C EEPROM driver (calibration storage)
 *
 * Data flow:
 *   mock sensor -> fifo_queue (buffering)
 *                -> byte_buffer (assembly)
 *                -> string_utils (formatting)
 *                -> hash_fnv (key hashing)
 *                -> kvs (persistent storage via fal)
 *   at24cxx stores calibration data separately
 *
 * Runs entirely on host with mocked flash + I2C -- no hardware required.
 * Build:  cmake -GNinja -B build && ninja -C build && ./build/storage_pipeline
 */

#include "test_harness.h"

/* wheels */
#include "fal.h"
#include "flashdb.h"
#include "fifo_queue.h"
#include "byte_buffer.h"
#include "string_utils.h"
#include "hash_fnv.h"
#include "at24cxx.h"

#include <string.h>
#include <stdio.h>

/* ======================================================================
 * Configuration
 * ====================================================================== */

#define MOCK_FLASH_SIZE      (64u * 1024u)
#define MOCK_BLOCK_SIZE      4096u
#define EEPROM_SIZE          256u     /* AT24C02 = 256 bytes */
#define FIFO_CAPACITY        16u     /* max buffered sensor samples */
#define BYTE_BUF_SIZE        128u    /* assembly buffer size */
#define STR_BUF_SIZE         64u     /* string formatting buffer */

/* ======================================================================
 * Mock flash device for fal (RAM-backed, NOR semantics)
 * ====================================================================== */

static uint8_t mock_flash_buf[MOCK_FLASH_SIZE];

static int mock_flash_init(void)
{
    memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
    return 0;
}

static int mock_flash_read(long offset, uint8_t *buf, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) {
        return -1;
    }
    memcpy(buf, mock_flash_buf + offset, size);
    return (int)size;
}

static int mock_flash_write(long offset, const uint8_t *buf, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) {
        return -1;
    }
    for (size_t i = 0; i < size; i++) {
        mock_flash_buf[(size_t)offset + i] &= buf[i];
    }
    return (int)size;
}

static int mock_flash_erase(long offset, size_t size)
{
    if (offset < 0 || (size_t)offset + size > MOCK_FLASH_SIZE) {
        return -1;
    }
    memset(mock_flash_buf + offset, 0xFF, size);
    return (int)size;
}

const struct fal_flash_dev mock_flash_dev = {
    .name       = "mock_flash",
    .addr       = 0,
    .len        = MOCK_FLASH_SIZE,
    .blk_size   = MOCK_BLOCK_SIZE,
    .ops        = {
        .init   = mock_flash_init,
        .read   = mock_flash_read,
        .write  = mock_flash_write,
        .erase  = mock_flash_erase,
    },
    .write_gran = 1,
};

/* ======================================================================
 * Mock I2C for at24cxx (EEPROM page array simulation)
 * ====================================================================== */

static uint8_t eeprom_mem[EEPROM_SIZE];
static int     eeprom_op_count;

static void eeprom_mock_reset(void)
{
    memset(eeprom_mem, 0xFF, sizeof(eeprom_mem));
    eeprom_op_count = 0;
}

static bool eeprom_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                             uint8_t val)
{
    (void)ctx; (void)dev_addr;
    /* reg is uint8_t (0..255), EEPROM_SIZE is 256 -- always in range */
    eeprom_mem[reg] = val;
    eeprom_op_count++;
    return true;
}

static bool eeprom_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                             uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        uint16_t addr = (uint16_t)reg + i;
        if (addr >= EEPROM_SIZE) {
            return false;
        }
        buf[i] = eeprom_mem[addr];
    }
    eeprom_op_count++;
    return true;
}

static bool eeprom_write_regs8(void *ctx, uint8_t dev_addr, uint8_t reg,
                               const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    for (uint16_t i = 0; i < len; i++) {
        uint16_t addr = (uint16_t)reg + i;
        if (addr >= EEPROM_SIZE) {
            return false;
        }
        eeprom_mem[addr] = data[i];
    }
    eeprom_op_count++;
    return true;
}

static void eeprom_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
    /* no real delay in host test */
}

static const at24cxx_i2c_t eeprom_i2c = {
    .write_reg   = eeprom_write_reg,
    .read_regs   = eeprom_read_regs,
    .write_regs8 = eeprom_write_regs8,
    .write_regs  = NULL,     /* not needed for 8-bit addr chips */
    .read_regs16 = NULL,     /* not needed for 8-bit addr chips */
    .delay_ms    = eeprom_delay_ms,
    .ctx         = NULL,
};

/* ======================================================================
 * Simulated sensor sample
 * ====================================================================== */

typedef struct {
    uint16_t sensor_id;
    int32_t  value_milli;   /* sensor value in milli-units */
    uint32_t timestamp;
} sensor_sample_t;

/* ======================================================================
 * Global instances
 * ====================================================================== */

static fifo_queue_t   sample_fifo;
static sensor_sample_t fifo_storage[FIFO_CAPACITY];

static byte_buffer_t  assembly_buf;
static uint8_t        assembly_storage[BYTE_BUF_SIZE];

static at24cxx_t      eeprom_dev;

static struct fdb_kvdb pipeline_kvdb;

/* ======================================================================
 * KVS helpers
 * ====================================================================== */

static struct fdb_default_kv_node default_kv_table[] = {
    { "sample_count", "0", 0 },
};

static void reset_all_flash(void)
{
    memset(mock_flash_buf, 0xFF, sizeof(mock_flash_buf));
}

static fdb_err_t init_kvdb(void)
{
    struct fdb_default_kv default_kv;
    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

    memset(&pipeline_kvdb, 0, sizeof(pipeline_kvdb));
    uint32_t sec_size = MOCK_BLOCK_SIZE;
    fdb_kvdb_control(&pipeline_kvdb, FDB_KVDB_CTRL_SET_SEC_SIZE, &sec_size);

    return fdb_kvdb_init(&pipeline_kvdb, "pipeline", "kvdb", &default_kv,
                         NULL);
}

/* ======================================================================
 * Tests
 * ====================================================================== */

TEST(test_init_all)
{
    /* 1. fal -- flash abstraction */
    int n = fal_init();
    ASSERT_TRUE(n == 3);
    ASSERT_TRUE(fal_init_check() == 1);

    /* 2. kvs (FlashDB) on fal */
    reset_all_flash();
    fdb_err_t fdb_err = init_kvdb();
    ASSERT_EQ(fdb_err, FDB_NO_ERR);

    /* 3. fifo_queue -- sensor sample buffer */
    fifo_queue_err_t fq_err = fifo_queue_init(&sample_fifo, fifo_storage,
                                               FIFO_CAPACITY,
                                               (uint16_t)sizeof(sensor_sample_t));
    ASSERT_EQ(fq_err, FIFO_QUEUE_OK);
    ASSERT_TRUE(fifo_queue_is_empty(&sample_fifo));

    /* 4. byte_buffer -- packet assembly */
    byte_buffer_err_t bb_err = byte_buffer_init(&assembly_buf,
                                                 assembly_storage,
                                                 BYTE_BUF_SIZE);
    ASSERT_EQ(bb_err, BYTE_BUFFER_OK);

    /* 5. at24cxx -- EEPROM for calibration data */
    eeprom_mock_reset();
    at24cxx_err_t ee_err = at24cxx_init(&eeprom_dev, &eeprom_i2c,
                                         AT24CXX_ADDR_DEFAULT,
                                         AT24CXX_C02);
    ASSERT_EQ(ee_err, AT24CXX_OK);

    /* string_utils and hash_fnv are stateless -- no init needed */
    printf("  All 7 wheels initialized successfully\n");
}

TEST(test_fifo_enqueue_dequeue)
{
    fifo_queue_clear(&sample_fifo);

    /* Enqueue 5 sensor samples */
    for (uint16_t i = 0; i < 5; i++) {
        sensor_sample_t s = {
            .sensor_id   = i,
            .value_milli = 25000 + (int32_t)i * 100,
            .timestamp   = 1000u + (uint32_t)i,
        };
        fifo_queue_err_t err = fifo_queue_push(&sample_fifo, &s);
        ASSERT_EQ(err, FIFO_QUEUE_OK);
    }
    ASSERT_EQ(fifo_queue_count(&sample_fifo), 5);

    /* Dequeue and verify FIFO order */
    for (uint16_t i = 0; i < 5; i++) {
        sensor_sample_t s;
        fifo_queue_err_t err = fifo_queue_pop(&sample_fifo, &s);
        ASSERT_EQ(err, FIFO_QUEUE_OK);
        ASSERT_EQ(s.sensor_id, i);
        ASSERT_EQ(s.value_milli, 25000 + (int32_t)i * 100);
    }
    ASSERT_TRUE(fifo_queue_is_empty(&sample_fifo));
}

TEST(test_byte_buffer_append_read)
{
    byte_buffer_err_t err = byte_buffer_reset(&assembly_buf);
    ASSERT_EQ(err, BYTE_BUFFER_OK);

    /* Assemble a sample as: [sensor_id:u16be][value:u32be][timestamp:u32be] */
    sensor_sample_t s = { .sensor_id = 0x0042, .value_milli = 25300,
                          .timestamp = 1001 };

    err = byte_buffer_put_u16_be(&assembly_buf, s.sensor_id);
    ASSERT_EQ(err, BYTE_BUFFER_OK);

    err = byte_buffer_put_u32_be(&assembly_buf, (uint32_t)s.value_milli);
    ASSERT_EQ(err, BYTE_BUFFER_OK);

    err = byte_buffer_put_u32_be(&assembly_buf, s.timestamp);
    ASSERT_EQ(err, BYTE_BUFFER_OK);

    /* Verify written length: 2 + 4 + 4 = 10 bytes */
    ASSERT_EQ(byte_buffer_readable(&assembly_buf), 10);

    /* Read back and verify */
    uint16_t id_back = 0;
    uint32_t val_back = 0;
    uint32_t ts_back = 0;

    err = byte_buffer_get_u16_be(&assembly_buf, &id_back);
    ASSERT_EQ(err, BYTE_BUFFER_OK);
    ASSERT_EQ(id_back, 0x0042);

    err = byte_buffer_get_u32_be(&assembly_buf, &val_back);
    ASSERT_EQ(err, BYTE_BUFFER_OK);
    ASSERT_EQ(val_back, 25300);

    err = byte_buffer_get_u32_be(&assembly_buf, &ts_back);
    ASSERT_EQ(err, BYTE_BUFFER_OK);
    ASSERT_EQ(ts_back, 1001);

    ASSERT_EQ(byte_buffer_readable(&assembly_buf), 0);
}

TEST(test_string_format_and_parse)
{
    /* Format: build a KVS value string from sensor data */
    char val_str[STR_BUF_SIZE];
    const char *parts[] = { "25300", "1001", "TEMP" };
    string_utils_err_t su_err = str_join(parts, 3, ",", val_str,
                                          sizeof(val_str));
    ASSERT_EQ(su_err, STRING_UTILS_OK);
    ASSERT_STR_EQ(val_str, "25300,1001,TEMP");

    /* Parse back: split by comma */
    char parse_buf[STR_BUF_SIZE];
    memcpy(parse_buf, val_str, strlen(val_str) + 1);
    char *tokens[4];
    int count = str_split(parse_buf, ',', tokens, 4);
    ASSERT_EQ(count, 3);

    /* Verify parsed values */
    int32_t parsed_val = 0;
    su_err = str_to_int(tokens[0], &parsed_val);
    ASSERT_EQ(su_err, STRING_UTILS_OK);
    ASSERT_EQ(parsed_val, 25300);

    int32_t parsed_ts = 0;
    su_err = str_to_int(tokens[1], &parsed_ts);
    ASSERT_EQ(su_err, STRING_UTILS_OK);
    ASSERT_EQ(parsed_ts, 1001);

    ASSERT_STR_EQ(tokens[2], "TEMP");

    /* Verify trim works on padded input */
    char trimmed[STR_BUF_SIZE];
    su_err = str_trim("  hello  ", trimmed, sizeof(trimmed));
    ASSERT_EQ(su_err, STRING_UTILS_OK);
    ASSERT_STR_EQ(trimmed, "hello");
}

TEST(test_hash_fnv_consistency)
{
    /* Same key must always produce the same hash */
    const char *key = "sensor_0042_temp";
    uint32_t h1 = fnv1a_32_string(key);
    uint32_t h2 = fnv1a_32_string(key);
    ASSERT_EQ(h1, h2);

    /* Different keys should (very likely) produce different hashes */
    uint32_t h3 = fnv1a_32_string("sensor_0043_temp");
    ASSERT_TRUE(h1 != h3);

    /* Incremental hash must match single-shot */
    uint32_t inc = FNV1A_32_OFFSET_BASIS;
    inc = fnv1a_32_update(inc, (const uint8_t *)key, strlen(key));
    ASSERT_EQ(h1, inc);

    /* 64-bit variant consistency */
    uint64_t h64a = fnv1a_64_string(key);
    uint64_t h64b = fnv1a_64_string(key);
    ASSERT_EQ(h64a, h64b);
}

TEST(test_fal_read_write_erase)
{
    const struct fal_partition *part = fal_partition_find("config");
    ASSERT_NOT_NULL(part);

    /* Erase partition first */
    int ret = fal_partition_erase_all(part);
    ASSERT_TRUE(ret >= 0);

    /* Write test pattern */
    uint8_t write_data[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    ret = fal_partition_write(part, 0, write_data, sizeof(write_data));
    ASSERT_EQ(ret, (int)sizeof(write_data));

    /* Read back and verify */
    uint8_t read_data[4] = {0};
    ret = fal_partition_read(part, 0, read_data, sizeof(read_data));
    ASSERT_EQ(ret, (int)sizeof(read_data));
    ASSERT_MEMEQ_N(read_data, write_data, sizeof(write_data));

    /* Erase and verify all 0xFF */
    ret = fal_partition_erase_all(part);
    ASSERT_TRUE(ret >= 0);
    ret = fal_partition_read(part, 0, read_data, sizeof(read_data));
    ASSERT_EQ(ret, (int)sizeof(read_data));
    uint8_t expected_erased[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    ASSERT_MEMEQ_N(read_data, expected_erased, sizeof(expected_erased));
}

TEST(test_kvs_set_get_delete)
{
    reset_all_flash();
    fdb_err_t err = init_kvdb();
    ASSERT_EQ(err, FDB_NO_ERR);

    /* Set a key */
    err = fdb_kv_set(&pipeline_kvdb, "sensor_temp", "25300");
    ASSERT_EQ(err, FDB_NO_ERR);

    /* Get it back */
    char *val = fdb_kv_get(&pipeline_kvdb, "sensor_temp");
    ASSERT_NOT_NULL(val);
    ASSERT_STR_EQ(val, "25300");

    /* Overwrite */
    err = fdb_kv_set(&pipeline_kvdb, "sensor_temp", "25400");
    ASSERT_EQ(err, FDB_NO_ERR);
    val = fdb_kv_get(&pipeline_kvdb, "sensor_temp");
    ASSERT_NOT_NULL(val);
    ASSERT_STR_EQ(val, "25400");

    /* Delete */
    err = fdb_kv_del(&pipeline_kvdb, "sensor_temp");
    ASSERT_EQ(err, FDB_NO_ERR);
    val = fdb_kv_get(&pipeline_kvdb, "sensor_temp");
    ASSERT_TRUE(val == NULL);

    /* Default KV should still be accessible */
    val = fdb_kv_get(&pipeline_kvdb, "sample_count");
    ASSERT_NOT_NULL(val);
    ASSERT_STR_EQ(val, "0");
}

TEST(test_at24cxx_page_write_read)
{
    eeprom_mock_reset();

    /* Write calibration data: offset(float) + gain(float) = 8 bytes */
    uint8_t cal_data[8] = { 0x00, 0x00, 0xC8, 0x42,   /* 100.0f LE */
                            0x00, 0x00, 0x80, 0x3F };  /* 1.0f   LE */

    at24cxx_err_t err = at24cxx_write(&eeprom_dev, 0, cal_data,
                                       sizeof(cal_data));
    ASSERT_EQ(err, AT24CXX_OK);

    /* Read back */
    uint8_t read_back[8] = {0};
    err = at24cxx_read(&eeprom_dev, 0, read_back, sizeof(read_back));
    ASSERT_EQ(err, AT24CXX_OK);
    ASSERT_MEMEQ_N(read_back, cal_data, sizeof(cal_data));

    /* Write a single byte at offset 64 */
    err = at24cxx_write_byte(&eeprom_dev, 64, 0xAB);
    ASSERT_EQ(err, AT24CXX_OK);

    uint8_t single = 0;
    err = at24cxx_read(&eeprom_dev, 64, &single, 1);
    ASSERT_EQ(err, AT24CXX_OK);
    ASSERT_EQ(single, 0xAB);

    ASSERT_TRUE(eeprom_op_count > 0);
}

TEST(test_full_pipeline)
{
    /*
     * Full pipeline:
     *   1. Generate sensor samples into fifo_queue
     *   2. Dequeue each sample
     *   3. Assemble into byte_buffer (binary packet)
     *   4. Format key with string_utils + hash_fnv
     *   5. Format value with string_utils
     *   6. Store in kvs via fal
     *   7. Store calibration in at24cxx
     *   8. Read everything back and verify
     */

    /* Reset all subsystems */
    fifo_queue_clear(&sample_fifo);
    (void)byte_buffer_reset(&assembly_buf);
    eeprom_mock_reset();
    reset_all_flash();
    fdb_err_t fdb_err = init_kvdb();
    ASSERT_EQ(fdb_err, FDB_NO_ERR);

    printf("\n  --- Storage Pipeline Demo ---\n");

    /* Step 1: Store calibration in EEPROM via at24cxx */
    uint8_t cal_offset[4] = { 0x00, 0x00, 0x48, 0x42 }; /* 50.0f LE */
    at24cxx_err_t ee_err = at24cxx_write(&eeprom_dev, 0, cal_offset, 4);
    ASSERT_EQ(ee_err, AT24CXX_OK);
    printf("  Calibration offset written to EEPROM\n");

    /* Step 2: Push sensor samples into FIFO */
    #define PIPELINE_SAMPLES 6
    for (uint16_t i = 0; i < PIPELINE_SAMPLES; i++) {
        sensor_sample_t s = {
            .sensor_id   = 42,
            .value_milli = 25000 + (int32_t)i * 150,
            .timestamp   = 2000u + (uint32_t)i * 10u,
        };
        fifo_queue_err_t fq_err = fifo_queue_push(&sample_fifo, &s);
        ASSERT_EQ(fq_err, FIFO_QUEUE_OK);
    }
    printf("  %d samples queued in FIFO\n", PIPELINE_SAMPLES);

    /* Step 3: Process each sample through the pipeline */
    for (uint16_t i = 0; i < PIPELINE_SAMPLES; i++) {
        /* 3a. Dequeue from FIFO */
        sensor_sample_t s;
        fifo_queue_err_t fq_err = fifo_queue_pop(&sample_fifo, &s);
        ASSERT_EQ(fq_err, FIFO_QUEUE_OK);

        /* 3b. Assemble into byte_buffer (binary representation) */
        (void)byte_buffer_reset(&assembly_buf);
        byte_buffer_err_t bb_err;
        bb_err = byte_buffer_put_u16_be(&assembly_buf, s.sensor_id);
        ASSERT_EQ(bb_err, BYTE_BUFFER_OK);
        bb_err = byte_buffer_put_u32_be(&assembly_buf,
                                         (uint32_t)s.value_milli);
        ASSERT_EQ(bb_err, BYTE_BUFFER_OK);
        bb_err = byte_buffer_put_u32_be(&assembly_buf, s.timestamp);
        ASSERT_EQ(bb_err, BYTE_BUFFER_OK);
        ASSERT_EQ(byte_buffer_written(&assembly_buf), 10);

        /* 3c. Generate KVS key using hash_fnv for uniqueness */
        char key_input[STR_BUF_SIZE];
        snprintf(key_input, sizeof(key_input), "s%u_t%u",
                 (unsigned)s.sensor_id, (unsigned)s.timestamp);
        uint32_t hash = fnv1a_32_string(key_input);
        char kvs_key[24];
        snprintf(kvs_key, sizeof(kvs_key), "d_%08x", (unsigned)hash);

        /* 3d. Format value string with string_utils */
        char val_parts[3][16];
        snprintf(val_parts[0], sizeof(val_parts[0]), "%d",
                 (int)s.value_milli);
        snprintf(val_parts[1], sizeof(val_parts[1]), "%u",
                 (unsigned)s.timestamp);
        snprintf(val_parts[2], sizeof(val_parts[2]), "%u",
                 (unsigned)s.sensor_id);

        const char *ptrs[3] = { val_parts[0], val_parts[1], val_parts[2] };
        char kvs_val[STR_BUF_SIZE];
        string_utils_err_t su_err = str_join(ptrs, 3, "|", kvs_val,
                                              sizeof(kvs_val));
        ASSERT_EQ(su_err, STRING_UTILS_OK);

        /* 3e. Store in KVS */
        fdb_err = fdb_kv_set(&pipeline_kvdb, kvs_key, kvs_val);
        ASSERT_EQ(fdb_err, FDB_NO_ERR);

        printf("  [%u] key=%s  val=%s\n", (unsigned)i, kvs_key, kvs_val);
    }
    ASSERT_TRUE(fifo_queue_is_empty(&sample_fifo));

    /* Step 4: Verify all stored data by re-reading from KVS */
    printf("  --- Verify read-back ---\n");
    for (uint16_t i = 0; i < PIPELINE_SAMPLES; i++) {
        int32_t expected_val = 25000 + (int32_t)i * 150;
        uint32_t expected_ts = 2000u + (uint32_t)i * 10u;

        /* Reconstruct the key */
        char key_input[STR_BUF_SIZE];
        snprintf(key_input, sizeof(key_input), "s42_t%u",
                 (unsigned)expected_ts);
        uint32_t hash = fnv1a_32_string(key_input);
        char kvs_key[24];
        snprintf(kvs_key, sizeof(kvs_key), "d_%08x", (unsigned)hash);

        /* Read from KVS */
        char *stored = fdb_kv_get(&pipeline_kvdb, kvs_key);
        ASSERT_NOT_NULL(stored);

        /* Parse the value back */
        char parse_buf[STR_BUF_SIZE];
        memcpy(parse_buf, stored, strlen(stored) + 1);
        char *tokens[4];
        int count = str_split(parse_buf, '|', tokens, 4);
        ASSERT_EQ(count, 3);

        int32_t got_val = 0;
        string_utils_err_t su_err = str_to_int(tokens[0], &got_val);
        ASSERT_EQ(su_err, STRING_UTILS_OK);
        ASSERT_EQ(got_val, expected_val);

        int32_t got_ts = 0;
        su_err = str_to_int(tokens[1], &got_ts);
        ASSERT_EQ(su_err, STRING_UTILS_OK);
        ASSERT_EQ(got_ts, (int32_t)expected_ts);

        printf("  [%u] verified: val=%d ts=%d\n", (unsigned)i,
               (int)got_val, (int)got_ts);
    }

    /* Step 5: Verify EEPROM calibration still intact */
    uint8_t cal_readback[4] = {0};
    ee_err = at24cxx_read(&eeprom_dev, 0, cal_readback, 4);
    ASSERT_EQ(ee_err, AT24CXX_OK);
    ASSERT_MEMEQ_N(cal_readback, cal_offset, 4);
    printf("  EEPROM calibration verified\n");

    printf("  --- Pipeline complete: %d samples stored and verified ---\n",
           PIPELINE_SAMPLES);
}

/* ======================================================================
 * Main -- run all integration tests in order
 * ====================================================================== */

TEST_MAIN("storage_pipeline integration")
    RUN(test_init_all);
    RUN(test_fifo_enqueue_dequeue);
    RUN(test_byte_buffer_append_read);
    RUN(test_string_format_and_parse);
    RUN(test_hash_fnv_consistency);
    RUN(test_fal_read_write_erase);
    RUN(test_kvs_set_get_delete);
    RUN(test_at24cxx_page_write_read);
    RUN(test_full_pipeline);
TEST_MAIN_END()
