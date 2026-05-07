// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include <math.h>
#include "qmc5883p.h"

/* ---------- chip-specific: write log on top of shared mock ---------- */

#define WRITE_LOG_MAX 32
static struct { uint8_t reg; uint8_t val; } write_log[WRITE_LOG_MAX];
static int write_log_idx;

static bool qmc_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    mock_regs[reg] = val;
    if (write_log_idx < WRITE_LOG_MAX) {
        write_log[write_log_idx].reg = reg;
        write_log[write_log_idx].val = val;
        write_log_idx++;
    }
    return true;
}

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0x80; /* CHIP_ID */
    mock_regs[0x09] = 0x01; /* STATUS: DRDY */
    write_log_idx = 0;
}

static const qmc5883p_i2c_t mock_i2c = {
    .write_reg = qmc_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_OK);
}

TEST(test_init_wake_register)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);
    ASSERT_TRUE(write_log_idx >= 1);
    ASSERT_EQ(write_log[0].reg, 0x0D);
    ASSERT_EQ(write_log[0].val, 0x40);
}

TEST(test_init_registers)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);
    ASSERT_EQ(mock_regs[0x29], 0x06);
    ASSERT_EQ(mock_regs[0x0B], 0x0C);
    ASSERT_EQ(mock_regs[0x0A], 0x0F);
}

TEST(test_init_soft_reset)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);
    int found = 0;
    for (int i = 0; i < write_log_idx; i++) {
        if (write_log[i].reg == 0x0B && write_log[i].val == 0x80) { found = 1; break; }
    }
    ASSERT_TRUE(found);
}

TEST(test_init_bad_chip_id)
{
    mock_reset();
    mock_regs[0x00] = 0xFF;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_ID);
}

TEST(test_init_i2c_write_fail)
{
    mock_reset();
    mock_fail_next = 1;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_init_i2c_read_fail)
{
    mock_reset();
    mock_fail_at = 1;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_read_ok)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    mock_regs[0x01] = 0xDC; mock_regs[0x02] = 0x05;
    mock_regs[0x03] = 0x24; mock_regs[0x04] = 0xFA;
    mock_regs[0x05] = 0xB8; mock_regs[0x06] = 0x0B;
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_NEAR(d.mx_gauss, 1500.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.my_gauss, -1500.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.mz_gauss, 3000.0f / 15000.0f, 0.001f);
}

TEST(test_read_not_ready)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);
    mock_regs[0x09] = 0x00;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_ERR_NOT_READY);
}

TEST(test_read_i2c_fail)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);
    mock_fail_next = 1;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_read_negative)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    int16_t rx = -7500, ry = -15000, rz = -1;
    mock_regs[0x01] = (uint8_t)(rx & 0xFF); mock_regs[0x02] = (uint8_t)((rx >> 8) & 0xFF);
    mock_regs[0x03] = (uint8_t)(ry & 0xFF); mock_regs[0x04] = (uint8_t)((ry >> 8) & 0xFF);
    mock_regs[0x05] = (uint8_t)(rz & 0xFF); mock_regs[0x06] = (uint8_t)((rz >> 8) & 0xFF);
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_NEAR(d.mx_gauss, -7500.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.my_gauss, -15000.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.mz_gauss, -1.0f / 15000.0f, 0.001f);
}

TEST(test_read_int16_extremes)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    mock_regs[0x01] = 0xFF; mock_regs[0x02] = 0x7F;
    mock_regs[0x03] = 0x00; mock_regs[0x04] = 0x80;
    mock_regs[0x05] = 0x00; mock_regs[0x06] = 0x00;
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_NEAR(d.mx_gauss, 32767.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.my_gauss, -32768.0f / 15000.0f, 0.001f);
}

TEST(test_read_zero)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    mock_regs[0x01] = 0; mock_regs[0x02] = 0;
    mock_regs[0x03] = 0; mock_regs[0x04] = 0;
    mock_regs[0x05] = 0; mock_regs[0x06] = 0;
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_NEAR(d.mx_gauss, 0.0f, 0.001f);
    ASSERT_NEAR(d.my_gauss, 0.0f, 0.001f);
    ASSERT_NEAR(d.mz_gauss, 0.0f, 0.001f);
}

TEST(test_init_write_order)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    ASSERT_TRUE(write_log_idx >= 5);
    ASSERT_EQ(write_log[0].reg, 0x0D);
    ASSERT_EQ(write_log[1].reg, 0x0B);
    ASSERT_EQ(write_log[1].val, 0x80);
    ASSERT_EQ(write_log[2].reg, 0x29);
    ASSERT_EQ(write_log[3].reg, 0x0B);
    ASSERT_EQ(write_log[4].reg, 0x0A);
}

TEST(test_init_mid_step_fail)
{
    mock_reset();
    mock_fail_at = 3;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_init_soft_reset_fail)
{
    mock_reset();
    mock_fail_at = 2;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_init_cr2_write_fail)
{
    mock_reset();
    mock_fail_at = 4;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_init_cr1_write_fail)
{
    mock_reset();
    mock_fail_at = 5;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_null_safety)
{
    qmc5883p_data_t d;
    ASSERT_EQ(qmc5883p_init(NULL), QMC5883P_ERR_I2C);
    ASSERT_EQ(qmc5883p_read(NULL, &d), QMC5883P_ERR_I2C);
    mock_reset();
    qmc5883p_init(&mock_i2c);
    ASSERT_EQ(qmc5883p_read(&mock_i2c, NULL), QMC5883P_ERR_I2C);
}

TEST(test_reinit)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    mock_regs[0x01] = 0xDC; mock_regs[0x02] = 0x05;
    mock_regs[0x03] = 0; mock_regs[0x04] = 0;
    mock_regs[0x05] = 0; mock_regs[0x06] = 0;
    mock_regs[0x09] = 0x01;
    qmc5883p_data_t d;
    qmc5883p_read(&mock_i2c, &d);

    write_log_idx = 0;
    mock_regs[0x00] = 0x80;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_TRUE(write_log_idx >= 5);
    ASSERT_EQ(mock_regs[0x0A], 0x0F);
}

TEST(test_read_data_i2c_fail)
{
    /* STATUS read succeeds (DRDY set), bulk XYZ read fails. */
    mock_reset();
    qmc5883p_init(&mock_i2c);
    /* init consumed 6 ops (write_reg x5, read_regs x1).
     * In read: op[6]=STATUS(ok), op[7]=XYZ read → fail. */
    mock_fail_at = 7;
    mock_regs[0x09] = 0x01; /* DRDY set so STATUS read passes the check */

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

/* ---------- main ---------- */

TEST_MAIN("qmc5883p")
    RUN(test_init_ok);
    RUN(test_init_wake_register);
    RUN(test_init_registers);
    RUN(test_init_soft_reset);
    RUN(test_init_write_order);
    RUN(test_init_bad_chip_id);
    RUN(test_init_i2c_write_fail);
    RUN(test_init_i2c_read_fail);
    RUN(test_init_mid_step_fail);
    RUN(test_init_soft_reset_fail);
    RUN(test_init_cr2_write_fail);
    RUN(test_init_cr1_write_fail);
    RUN(test_read_ok);
    RUN(test_read_not_ready);
    RUN(test_read_i2c_fail);
    RUN(test_read_negative);
    RUN(test_read_int16_extremes);
    RUN(test_read_zero);
    RUN(test_null_safety);
    RUN(test_reinit);
    RUN(test_read_data_i2c_fail);
TEST_MAIN_END()
