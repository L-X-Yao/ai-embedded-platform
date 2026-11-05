// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "opt3001.h"

/* ---------- chip-specific 16-bit register mock ---------- */

/*
 * OPT3001 has 16-bit registers addressed by 8-bit pointer.
 * The shared mock_i2c.h uses byte-indexed storage which doesn't
 * model 16-bit register devices cleanly (consecutive register
 * addresses overlap in byte storage). We overlay a small 16-bit
 * register table and provide custom read/write wrappers.
 */

#define REG_RESULT          0x00
#define REG_CONFIG          0x01
#define REG_MANUFACTURER_ID 0x7E
#define REG_DEVICE_ID       0x7F

#define REG16_COUNT         4
static struct { uint8_t addr; uint16_t val; } reg16_table[REG16_COUNT];

static void reg16_reset(void)
{
    reg16_table[0] = (typeof(reg16_table[0])){REG_RESULT,          0x0000};
    reg16_table[1] = (typeof(reg16_table[0])){REG_CONFIG,          0xC810};
    reg16_table[2] = (typeof(reg16_table[0])){REG_MANUFACTURER_ID, 0x5449};
    reg16_table[3] = (typeof(reg16_table[0])){REG_DEVICE_ID,       0x3001};
}

static int reg16_find(uint8_t addr)
{
    for (int i = 0; i < REG16_COUNT; i++)
        if (reg16_table[i].addr == addr) return i;
    return -1;
}

static void set_reg16(uint8_t reg, uint16_t val)
{
    int idx = reg16_find(reg);
    if (idx >= 0) reg16_table[idx].val = val;
}

static uint16_t get_reg16(uint8_t reg)
{
    int idx = reg16_find(reg);
    return (idx >= 0) ? reg16_table[idx].val : 0;
}

/* Fail injection: reuse mock_i2c.h counters */

static bool opt_mock_write_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    if (len == 2) {
        uint16_t val = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
        set_reg16(reg, val);
    }
    return true;
}

static bool opt_mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                               uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_read_count++;
    if (len == 2) {
        uint16_t val = get_reg16(reg);
        buf[0] = (uint8_t)(val >> 8);
        buf[1] = (uint8_t)(val & 0xFF);
    }
    return true;
}

static void mock_reset(void)
{
    mock_i2c_reset();
    reg16_reset();
}

static const opt3001_i2c_t mock_i2c = {
    .write_regs = opt_mock_write_regs,
    .read_regs  = opt_mock_read_regs,
    .ctx        = NULL,
};

static opt3001_t dev;

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    ASSERT_EQ(dev.addr, 0x44);
}

TEST(test_init_all_addrs)
{
    for (uint8_t a = 0x44; a <= 0x47; a++) {
        mock_reset();
        ASSERT_EQ(opt3001_init(&dev, &mock_i2c, a), OPT3001_OK);
        ASSERT_EQ(dev.addr, a);
    }
}

/* SBOS681C Table 1: 1000100 GND, 1000101 VDD, 1000110 SDA, 1000111 SCL.
 * The older two-pin names must keep compiling with the same values. */
TEST(test_addr_macros_match_table_1)
{
    static const struct { uint8_t pin; uint8_t older; uint8_t value; } rows[] = {
        { OPT3001_ADDR_GND, OPT3001_ADDR_GND_GND, 0x44u },
        { OPT3001_ADDR_VDD, OPT3001_ADDR_GND_VDD, 0x45u },
        { OPT3001_ADDR_SDA, OPT3001_ADDR_VDD_GND, 0x46u },
        { OPT3001_ADDR_SCL, OPT3001_ADDR_VDD_VDD, 0x47u },
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        ASSERT_EQ_HEX(rows[i].pin, rows[i].value);
        ASSERT_EQ_HEX(rows[i].older, rows[i].value);
        mock_reset();
        ASSERT_EQ(opt3001_init(&dev, &mock_i2c, rows[i].pin), OPT3001_OK);
        ASSERT_EQ(dev.addr, rows[i].value);
    }
}

TEST(test_init_null_dev)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(NULL, &mock_i2c, 0x44), OPT3001_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, NULL, 0x44), OPT3001_ERR_PARAM);
}

TEST(test_init_bad_addr)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x43), OPT3001_ERR_PARAM);
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x48), OPT3001_ERR_PARAM);
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x00), OPT3001_ERR_PARAM);
}

TEST(test_init_wrong_manufacturer_id)
{
    mock_reset();
    set_reg16(REG_MANUFACTURER_ID, 0x1234);
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_ERR_ID);
}

TEST(test_init_wrong_device_id)
{
    mock_reset();
    set_reg16(REG_DEVICE_ID, 0x9999);
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_ERR_I2C);
}

TEST(test_init_writes_config)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    /* Init should write default config 0xC010 to config register */
    uint16_t cfg = get_reg16(REG_CONFIG);
    ASSERT_EQ_HEX(cfg, 0xC010);
}

/* ---------- read_lux tests ---------- */

TEST(test_read_lux_zero)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    set_reg16(REG_RESULT, 0x0000);
    float lux;
    ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_OK);
    ASSERT_NEAR(lux, 0.0f, 0.001f);
}

TEST(test_read_lux_known_value)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    /* E=0, R=100 -> lux = 0.01 * 2^0 * 100 = 1.0 */
    set_reg16(REG_RESULT, (0u << 12) | 100u);
    float lux;
    ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_OK);
    ASSERT_NEAR(lux, 1.0f, 0.01f);
}

TEST(test_read_lux_exponent)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    /* E=4, R=2048 -> lux = 0.01 * 2^4 * 2048 = 0.01 * 16 * 2048 = 327.68 */
    set_reg16(REG_RESULT, (4u << 12) | 2048u);
    float lux;
    ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_OK);
    ASSERT_NEAR(lux, 327.68f, 0.1f);
}

TEST(test_read_lux_max)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    /* E=11, R=4095 -> lux = 0.01 * 2^11 * 4095 = 0.01 * 2048 * 4095 = 83865.6 */
    set_reg16(REG_RESULT, (11u << 12) | 4095u);
    float lux;
    ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_OK);
    ASSERT_NEAR(lux, 83865.6f, 1.0f);
}

TEST(test_read_lux_null)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    float lux;
    ASSERT_EQ(opt3001_read_lux(NULL, &lux), OPT3001_ERR_PARAM);
    ASSERT_EQ(opt3001_read_lux(&dev, NULL), OPT3001_ERR_PARAM);
}

TEST(test_read_lux_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    mock_fail_next = 1;
    float lux;
    ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_ERR_I2C);
}

/* ---------- mode tests ---------- */

TEST(test_start_single)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    ASSERT_EQ(opt3001_start_single(&dev), OPT3001_OK);
    uint16_t cfg = get_reg16(REG_CONFIG);
    /* Mode bits [10:9] should be 01 (single-shot) */
    ASSERT_EQ((cfg >> 9) & 0x03, 0x01);
}

TEST(test_set_continuous)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    ASSERT_EQ(opt3001_set_continuous(&dev), OPT3001_OK);
    uint16_t cfg = get_reg16(REG_CONFIG);
    /* Mode bits [10:9] should be 11 (continuous) */
    ASSERT_EQ((cfg >> 9) & 0x03, 0x03);
}

TEST(test_start_single_null)
{
    ASSERT_EQ(opt3001_start_single(NULL), OPT3001_ERR_PARAM);
}

TEST(test_set_continuous_null)
{
    ASSERT_EQ(opt3001_set_continuous(NULL), OPT3001_ERR_PARAM);
}

TEST(test_mode_preserves_other_bits)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    /* After init, config is 0xC010; set continuous should only change mode bits */
    ASSERT_EQ(opt3001_set_continuous(&dev), OPT3001_OK);
    uint16_t cfg = get_reg16(REG_CONFIG);
    /* Bits outside mode field (bits 10:9) should be preserved */
    ASSERT_EQ_HEX(cfg & ~(0x03u << 9), 0xC010 & ~(0x03u << 9));
}

/* ---------- data_ready tests ---------- */

TEST(test_data_ready_true)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    /* Set CRF bit (bit 7 of low byte = bit 7 of 16-bit config) */
    uint16_t cfg = get_reg16(REG_CONFIG);
    set_reg16(REG_CONFIG, cfg | (1u << 7));
    ASSERT_TRUE(opt3001_data_ready(&dev));
}

TEST(test_data_ready_false)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    /* Clear CRF bit */
    uint16_t cfg = get_reg16(REG_CONFIG);
    set_reg16(REG_CONFIG, (uint16_t)(cfg & (uint16_t)~(1u << 7)));
    ASSERT_FALSE(opt3001_data_ready(&dev));
}

TEST(test_data_ready_null)
{
    ASSERT_FALSE(opt3001_data_ready(NULL));
}

TEST(test_data_ready_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    mock_fail_next = 1;
    ASSERT_FALSE(opt3001_data_ready(&dev));
}

/* ---------- i2c function pointer validation ---------- */

TEST(test_init_null_write_regs)
{
    mock_reset();
    opt3001_i2c_t bad_i2c = {
        .write_regs = NULL,
        .read_regs  = opt_mock_read_regs,
        .ctx        = NULL,
    };
    ASSERT_EQ(opt3001_init(&dev, &bad_i2c, 0x44), OPT3001_ERR_PARAM);
}

TEST(test_init_null_read_regs)
{
    mock_reset();
    opt3001_i2c_t bad_i2c = {
        .write_regs = opt_mock_write_regs,
        .read_regs  = NULL,
        .ctx        = NULL,
    };
    ASSERT_EQ(opt3001_init(&dev, &bad_i2c, 0x44), OPT3001_ERR_PARAM);
}

/* ---------- OPT3001_ERR_NOT_READY: read_lux when conversion not done -----
 * The current API does not return ERR_NOT_READY (read_lux reads the
 * result register unconditionally; the caller should check data_ready()
 * first).  However, the error code is defined for future use.
 *
 * These tests verify the expected workflow: data_ready() returns false
 * when CRF is clear, and the result register holds stale data. */

TEST(test_read_lux_before_conversion_done)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);

    /* Start single-shot measurement */
    ASSERT_EQ(opt3001_start_single(&dev), OPT3001_OK);

    /* CRF bit is clear (conversion not done) */
    uint16_t cfg = get_reg16(REG_CONFIG);
    set_reg16(REG_CONFIG, (uint16_t)(cfg & (uint16_t)~(1u << 7)));

    /* data_ready should report false */
    ASSERT_FALSE(opt3001_data_ready(&dev));

    /* read_lux still succeeds (reads stale result) — this is by design:
     * the driver does not gate read_lux on CRF.  Callers must poll
     * data_ready() themselves. */
    set_reg16(REG_RESULT, 0x0000);
    float lux;
    ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_OK);
    ASSERT_NEAR(lux, 0.0f, 0.001f);
}

/* ABI stability: verify the not-ready enum value */
TEST(test_err_not_ready_value)
{
    ASSERT_EQ((int)OPT3001_ERR_NOT_READY, -4);
}

/* Exercise full single-shot cycle: start → poll → read */
TEST(test_single_shot_cycle)
{
    mock_reset();
    ASSERT_EQ(opt3001_init(&dev, &mock_i2c, 0x44), OPT3001_OK);
    ASSERT_EQ(opt3001_start_single(&dev), OPT3001_OK);

    /* Not ready yet */
    uint16_t cfg = get_reg16(REG_CONFIG);
    set_reg16(REG_CONFIG, (uint16_t)(cfg & (uint16_t)~(1u << 7)));
    ASSERT_FALSE(opt3001_data_ready(&dev));

    /* Now conversion completes: set CRF bit */
    cfg = get_reg16(REG_CONFIG);
    set_reg16(REG_CONFIG, cfg | (1u << 7));
    ASSERT_TRUE(opt3001_data_ready(&dev));

    /* Result: E=2, R=500 → lux = 0.01 * 4 * 500 = 20.0 */
    set_reg16(REG_RESULT, (2u << 12) | 500u);
    float lux;
    ASSERT_EQ(opt3001_read_lux(&dev, &lux), OPT3001_OK);
    ASSERT_NEAR(lux, 20.0f, 0.1f);
}

TEST_MAIN("opt3001")
    RUN(test_init_ok);
    RUN(test_init_all_addrs);
    RUN(test_addr_macros_match_table_1);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_bad_addr);
    RUN(test_init_wrong_manufacturer_id);
    RUN(test_init_wrong_device_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_writes_config);
    RUN(test_read_lux_zero);
    RUN(test_read_lux_known_value);
    RUN(test_read_lux_exponent);
    RUN(test_read_lux_max);
    RUN(test_read_lux_null);
    RUN(test_read_lux_i2c_fail);
    RUN(test_start_single);
    RUN(test_set_continuous);
    RUN(test_start_single_null);
    RUN(test_set_continuous_null);
    RUN(test_mode_preserves_other_bits);
    RUN(test_data_ready_true);
    RUN(test_data_ready_false);
    RUN(test_data_ready_null);
    RUN(test_data_ready_i2c_fail);
    RUN(test_init_null_write_regs);
    RUN(test_init_null_read_regs);
    RUN(test_read_lux_before_conversion_done);
    RUN(test_err_not_ready_value);
    RUN(test_single_shot_cycle);
TEST_MAIN_END()
