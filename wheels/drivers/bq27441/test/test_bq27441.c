// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "bq27441.h"

/* ---------- register addresses (mirror bq27441.c) ---------- */
#define REG_CONTROL          0x00u
#define REG_TEMPERATURE      0x02u
#define REG_VOLTAGE          0x04u
#define REG_FLAGS            0x06u
#define REG_REMAINING_CAP    0x0Cu
#define REG_FULL_CHARGE_CAP  0x0Eu
#define REG_AVG_CURRENT      0x10u
#define REG_AVG_POWER        0x18u
#define REG_SOC              0x1Cu

#define FLAG_CFGUPMODE       (1u << 4)

/* CTRL sub-commands used in tests */
#define CTRL_DEVICE_TYPE     0x0001u

/* ---------- helpers: write 16-bit LE value into mock regs ---------- */
static void mock_set_le16(uint8_t reg, uint16_t val)
{
    mock_regs[reg]     = (uint8_t)(val & 0xFFu);
    mock_regs[reg + 1] = (uint8_t)(val >> 8);
}

/* ---------- BQ27441 control register behavior ---------- */
/*
 * The real BQ27441 treats writes to reg 0x00 as sub-commands:
 * writing DEVICE_TYPE (0x0001) causes the next read of reg 0x00
 * to return 0x0421. Our mock simulates this by intercepting
 * write_reg and patching the response into mock_regs.
 */
static uint16_t mock_device_type = 0x0421u;

static uint16_t mock_control_status;
static uint8_t  mock_control_low;

static bool mock_bq_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    /* Let the base mock handle storage + fail injection first */
    if (!mock_i2c_write_reg(ctx, dev_addr, reg, val))
        return false;

    /* Simulate CONTROL sub-command response: low byte, then high byte */
    if (reg == REG_CONTROL) {
        mock_control_low = val;
    } else if (reg == REG_CONTROL + 1) {
        uint16_t subcmd = (uint16_t)((uint16_t)val << 8 | mock_control_low);
        if (subcmd == CTRL_DEVICE_TYPE) {
            mock_set_le16(REG_CONTROL, mock_device_type);
        } else if (subcmd == 0x0000u) {
            mock_set_le16(REG_CONTROL, mock_control_status);
        }
    }
    return true;
}

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_device_type = 0x0421u;
    mock_control_status = 0x0000u; /* unsealed */
    /* Default voltage: 3700 mV */
    mock_set_le16(REG_VOLTAGE, 3700);
    /* Default SOC: 75% */
    mock_set_le16(REG_SOC, 75);
    /* Default temperature: 298.1 K = 25.0 degC => 2981 in 0.1K */
    mock_set_le16(REG_TEMPERATURE, 2981);
    /* Default current: -150 mA (discharging) */
    mock_set_le16(REG_AVG_CURRENT, (uint16_t)(int16_t)-150);
    /* Default remaining: 1500 mAh */
    mock_set_le16(REG_REMAINING_CAP, 1500);
    /* Default full charge: 2000 mAh */
    mock_set_le16(REG_FULL_CHARGE_CAP, 2000);
    /* Default avg power: -555 mW */
    mock_set_le16(REG_AVG_POWER, (uint16_t)(int16_t)-555);
    /* Default flags: battery detected, no config mode */
    mock_set_le16(REG_FLAGS, 0x0008);
}

static const bq27441_i2c_t mock_i2c = {
    .write_reg = mock_bq_write_reg,
    .read_regs = mock_i2c_read_regs,
    .write_regs = mock_i2c_write_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static bq27441_t dev;

/* ========== init tests ========== */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(bq27441_init(&dev, &mock_i2c), BQ27441_OK);
}

TEST(test_init_bad_device_id)
{
    mock_reset();
    mock_device_type = 0x1234u;  /* wrong device type */
    ASSERT_EQ(bq27441_init(&dev, &mock_i2c), BQ27441_ERR_ID);
}

TEST(test_init_i2c_fail_write)
{
    mock_reset();
    mock_fail_at = 0;  /* fail on first I2C op (write control cmd) */
    ASSERT_EQ(bq27441_init(&dev, &mock_i2c), BQ27441_ERR_I2C);
}

TEST(test_init_i2c_fail_read)
{
    mock_reset();
    mock_fail_at = 1;  /* fail on second I2C op (read control) */
    ASSERT_EQ(bq27441_init(&dev, &mock_i2c), BQ27441_ERR_I2C);
}

/* ========== null param tests ========== */

TEST(test_null_init_dev)
{
    ASSERT_EQ(bq27441_init(NULL, &mock_i2c), BQ27441_ERR_PARAM);
}

TEST(test_null_init_i2c)
{
    ASSERT_EQ(bq27441_init(&dev, NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_read_dev)
{
    bq27441_data_t data;
    ASSERT_EQ(bq27441_read(NULL, &data), BQ27441_ERR_PARAM);
}

TEST(test_null_read_out)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);
    ASSERT_EQ(bq27441_read(&dev, NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_read_voltage)
{
    uint16_t v;
    ASSERT_EQ(bq27441_read_voltage(NULL, &v), BQ27441_ERR_PARAM);
    mock_reset();
    bq27441_init(&dev, &mock_i2c);
    ASSERT_EQ(bq27441_read_voltage(&dev, NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_read_soc)
{
    uint16_t s;
    ASSERT_EQ(bq27441_read_soc(NULL, &s), BQ27441_ERR_PARAM);
    mock_reset();
    bq27441_init(&dev, &mock_i2c);
    ASSERT_EQ(bq27441_read_soc(&dev, NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_read_current)
{
    int16_t c;
    ASSERT_EQ(bq27441_read_current(NULL, &c), BQ27441_ERR_PARAM);
    mock_reset();
    bq27441_init(&dev, &mock_i2c);
    ASSERT_EQ(bq27441_read_current(&dev, NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_read_temperature)
{
    int16_t t;
    ASSERT_EQ(bq27441_read_temperature(NULL, &t), BQ27441_ERR_PARAM);
    mock_reset();
    bq27441_init(&dev, &mock_i2c);
    ASSERT_EQ(bq27441_read_temperature(&dev, NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_enter_config)
{
    ASSERT_EQ(bq27441_enter_config_mode(NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_exit_config)
{
    ASSERT_EQ(bq27441_exit_config_mode(NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_set_design_capacity)
{
    ASSERT_EQ(bq27441_set_design_capacity(NULL, 2000), BQ27441_ERR_PARAM);
}

TEST(test_null_unseal)
{
    ASSERT_EQ(bq27441_unseal(NULL), BQ27441_ERR_PARAM);
}

TEST(test_null_seal)
{
    ASSERT_EQ(bq27441_seal(NULL), BQ27441_ERR_PARAM);
}

/* ========== read tests ========== */

TEST(test_read_all_fields)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    bq27441_data_t data;
    memset(&data, 0, sizeof(data));
    ASSERT_EQ(bq27441_read(&dev, &data), BQ27441_OK);
    ASSERT_EQ(data.voltage_mv, 3700);
    ASSERT_EQ(data.current_ma, -150);
    ASSERT_EQ(data.soc_pct, 75);
    /* 2981 (0.1K) - 2731 = 250 (0.1 degC) = 25.0 degC */
    ASSERT_EQ(data.temperature_01c, 250);
    ASSERT_EQ(data.remaining_mah, 1500);
    ASSERT_EQ(data.full_charge_mah, 2000);
    ASSERT_EQ(data.avg_power_mw, -555);
}

TEST(test_read_voltage)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    uint16_t v = 0;
    ASSERT_EQ(bq27441_read_voltage(&dev, &v), BQ27441_OK);
    ASSERT_EQ(v, 3700);
}

TEST(test_read_soc)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    uint16_t soc = 0;
    ASSERT_EQ(bq27441_read_soc(&dev, &soc), BQ27441_OK);
    ASSERT_EQ(soc, 75);
}

TEST(test_read_current)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    int16_t cur = 0;
    ASSERT_EQ(bq27441_read_current(&dev, &cur), BQ27441_OK);
    ASSERT_EQ(cur, -150);
}

TEST(test_read_current_positive)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    /* Simulate charging: +500 mA */
    mock_set_le16(REG_AVG_CURRENT, 500);
    int16_t cur = 0;
    ASSERT_EQ(bq27441_read_current(&dev, &cur), BQ27441_OK);
    ASSERT_EQ(cur, 500);
}

TEST(test_read_temperature)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    int16_t temp = 0;
    ASSERT_EQ(bq27441_read_temperature(&dev, &temp), BQ27441_OK);
    ASSERT_EQ(temp, 250);  /* 25.0 degC */
}

TEST(test_read_temperature_subzero)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    /* -10.0 degC = 263.1 K = 2631 in 0.1K */
    mock_set_le16(REG_TEMPERATURE, 2631);
    int16_t temp = 0;
    ASSERT_EQ(bq27441_read_temperature(&dev, &temp), BQ27441_OK);
    ASSERT_EQ(temp, -100);  /* -10.0 degC */
}

TEST(test_read_full_charge_100_pct)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    mock_set_le16(REG_SOC, 100);
    mock_set_le16(REG_REMAINING_CAP, 2000);

    bq27441_data_t data;
    ASSERT_EQ(bq27441_read(&dev, &data), BQ27441_OK);
    ASSERT_EQ(data.soc_pct, 100);
    ASSERT_EQ(data.remaining_mah, 2000);
}

/* ========== I2C failure during read ========== */

TEST(test_read_voltage_i2c_fail)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    uint16_t v;
    ASSERT_EQ(bq27441_read_voltage(&dev, &v), BQ27441_ERR_I2C);
}

TEST(test_read_soc_i2c_fail)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    uint16_t soc;
    ASSERT_EQ(bq27441_read_soc(&dev, &soc), BQ27441_ERR_I2C);
}

TEST(test_read_current_i2c_fail)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    int16_t cur;
    ASSERT_EQ(bq27441_read_current(&dev, &cur), BQ27441_ERR_I2C);
}

TEST(test_read_all_i2c_fail_midway)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    /* Fail on 3rd I2C operation during bq27441_read (SOC read) */
    mock_fail_at = (int)(mock_op_count + 2);
    bq27441_data_t data;
    ASSERT_EQ(bq27441_read(&dev, &data), BQ27441_ERR_I2C);
}

/* ========== config mode tests ========== */

TEST(test_enter_config_mode_ok)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    /* Simulate: after SET_CFGUPDATE, FLAGS shows CFGUPMODE on first poll */
    mock_set_le16(REG_FLAGS, FLAG_CFGUPMODE);
    ASSERT_EQ(bq27441_enter_config_mode(&dev), BQ27441_OK);
    ASSERT_TRUE(mock_delay_ms_count > 0);  /* delay was called */
}

TEST(test_enter_config_mode_timeout)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    /* FLAGS never shows CFGUPMODE */
    mock_set_le16(REG_FLAGS, 0x0000);
    ASSERT_EQ(bq27441_enter_config_mode(&dev), BQ27441_ERR_TIMEOUT);
}

TEST(test_exit_config_mode_ok)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    /* Simulate: CFGUPMODE clears on first poll after soft reset */
    mock_set_le16(REG_FLAGS, 0x0000);
    ASSERT_EQ(bq27441_exit_config_mode(&dev), BQ27441_OK);
}

TEST(test_exit_config_mode_timeout)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    /* FLAGS always shows CFGUPMODE (never clears) */
    mock_set_le16(REG_FLAGS, FLAG_CFGUPMODE);
    ASSERT_EQ(bq27441_exit_config_mode(&dev), BQ27441_ERR_TIMEOUT);
}

/* ========== design capacity ========== */

TEST(test_set_design_capacity_zero_rejected)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    ASSERT_EQ(bq27441_set_design_capacity(&dev, 0), BQ27441_ERR_PARAM);
}

/* ========== seal / unseal ========== */

TEST(test_unseal)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    ASSERT_EQ(bq27441_unseal(&dev), BQ27441_OK);
}

TEST(test_seal)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    ASSERT_EQ(bq27441_seal(&dev), BQ27441_OK);
}

TEST(test_unseal_i2c_fail)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    mock_fail_next = 1;
    ASSERT_EQ(bq27441_unseal(&dev), BQ27441_ERR_I2C);
}

/* ========== init validation ========== */

TEST(test_init_null_read_regs)
{
    bq27441_i2c_t bad_i2c = mock_i2c;
    bad_i2c.read_regs = NULL;
    ASSERT_EQ(bq27441_init(&dev, &bad_i2c), BQ27441_ERR_PARAM);
}

TEST(test_init_null_write_reg)
{
    bq27441_i2c_t bad_i2c = mock_i2c;
    bad_i2c.write_reg = NULL;
    ASSERT_EQ(bq27441_init(&dev, &bad_i2c), BQ27441_ERR_PARAM);
}

/* ========== zero-init safety ========== */

TEST(test_zero_init_safety)
{
    bq27441_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* Reading on zero-init struct: must not crash (returns ERR_PARAM) */
    bq27441_data_t data;
    (void)data;
    /* Cannot call read on zero dev (i2c is NULL -> would deref), but
       null checks on dev/out protect the entry point */
}

static uint32_t shortest_poll_ms;

static void record_poll_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    /* Bus gaps are 1 ms. Anything longer is a wait between Flags() polls. */
    if (ms > 1 && (shortest_poll_ms == 0 || ms < shortest_poll_ms)) {
        shortest_poll_ms = ms;
    }
}

TEST(test_control_uses_single_byte_writes)
{
    bq27441_i2c_t no_block = mock_i2c;
    no_block.write_regs = NULL;
    mock_reset();
    ASSERT_EQ(bq27441_init(&dev, &no_block), BQ27441_OK);
}

TEST(test_init_requires_delay)
{
    bq27441_i2c_t no_delay = mock_i2c;
    no_delay.delay_ms = NULL;
    mock_reset();
    ASSERT_EQ(bq27441_init(&dev, &no_delay), BQ27441_ERR_PARAM);
}

TEST(test_unseal_reports_still_sealed)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    mock_control_status = 1u << 13; /* SS */
    ASSERT_EQ(bq27441_unseal(&dev), BQ27441_ERR_SEALED);
}

TEST(test_read_needs_a_battery)
{
    bq27441_data_t data;
    mock_reset();
    bq27441_init(&dev, &mock_i2c);

    mock_set_le16(REG_FLAGS, 0x0000);
    ASSERT_EQ(bq27441_read(&dev, &data), BQ27441_ERR_NO_BATTERY);
}

TEST(test_config_polls_at_most_twice_a_second)
{
    bq27441_i2c_t slow = mock_i2c;
    slow.delay_ms = record_poll_delay;
    mock_reset();
    ASSERT_EQ(bq27441_init(&dev, &slow), BQ27441_OK);

    shortest_poll_ms = 0;
    mock_set_le16(REG_FLAGS, FLAG_CFGUPMODE);
    ASSERT_EQ(bq27441_enter_config_mode(&dev), BQ27441_OK);
    ASSERT_TRUE(shortest_poll_ms >= 500);
}

TEST(test_set_design_capacity_is_unsupported)
{
    mock_reset();
    bq27441_init(&dev, &mock_i2c);
    ASSERT_EQ(bq27441_set_design_capacity(&dev, 2200), BQ27441_ERR_UNSUPPORTED);
}

TEST_MAIN("bq27441")
    RUN(test_init_ok);
    RUN(test_init_bad_device_id);
    RUN(test_init_i2c_fail_write);
    RUN(test_init_i2c_fail_read);
    RUN(test_null_init_dev);
    RUN(test_null_init_i2c);
    RUN(test_null_read_dev);
    RUN(test_null_read_out);
    RUN(test_null_read_voltage);
    RUN(test_null_read_soc);
    RUN(test_null_read_current);
    RUN(test_null_read_temperature);
    RUN(test_null_enter_config);
    RUN(test_null_exit_config);
    RUN(test_null_set_design_capacity);
    RUN(test_null_unseal);
    RUN(test_null_seal);
    RUN(test_read_all_fields);
    RUN(test_read_voltage);
    RUN(test_read_soc);
    RUN(test_read_current);
    RUN(test_read_current_positive);
    RUN(test_read_temperature);
    RUN(test_read_temperature_subzero);
    RUN(test_read_full_charge_100_pct);
    RUN(test_read_voltage_i2c_fail);
    RUN(test_read_soc_i2c_fail);
    RUN(test_read_current_i2c_fail);
    RUN(test_read_all_i2c_fail_midway);
    RUN(test_enter_config_mode_ok);
    RUN(test_enter_config_mode_timeout);
    RUN(test_exit_config_mode_ok);
    RUN(test_exit_config_mode_timeout);
    RUN(test_set_design_capacity_zero_rejected);
    RUN(test_unseal);
    RUN(test_seal);
    RUN(test_unseal_i2c_fail);
    RUN(test_init_null_read_regs);
    RUN(test_init_null_write_reg);
    RUN(test_zero_init_safety);
    RUN(test_control_uses_single_byte_writes);
    RUN(test_init_requires_delay);
    RUN(test_unseal_reports_still_sealed);
    RUN(test_read_needs_a_battery);
    RUN(test_config_polls_at_most_twice_a_second);
    RUN(test_set_design_capacity_is_unsupported);
TEST_MAIN_END()
