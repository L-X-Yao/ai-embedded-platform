#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "qmc5883p.h"

/* ---------- mock I2C ---------- */

#define MOCK_REG_COUNT 256
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;    /* -1 = disabled; fail when mock_op_count == mock_fail_at */

#define WRITE_LOG_MAX 32
static struct { uint8_t reg; uint8_t val; } write_log[WRITE_LOG_MAX];
static int write_log_idx;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_regs[0x00] = 0x80; /* CHIP_ID */
    mock_regs[0x09] = 0x01; /* STATUS: DRDY */
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    write_log_idx   = 0;
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    mock_regs[reg] = val;
    if (write_log_idx < WRITE_LOG_MAX) {
        write_log[write_log_idx].reg = reg;
        write_log[write_log_idx].val = val;
        write_log_idx++;
    }
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t addr, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    for (uint16_t i = 0; i < len; i++)
        buf[i] = ((reg + i) < MOCK_REG_COUNT) ? mock_regs[reg + i] : 0xFF;
    return true;
}

static const qmc5883p_i2c_t mock_i2c = {
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */
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
    /* op 0 = wake write (succeed), op 1 = chip ID read (fail) */
    mock_fail_at = 1;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C);
}

TEST(test_read_ok)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    /* Plant known raw values: x=1500, y=-1500, z=3000 */
    mock_regs[0x01] = 0xDC; mock_regs[0x02] = 0x05; /* x = 0x05DC = 1500 */
    mock_regs[0x03] = 0x24; mock_regs[0x04] = 0xFA; /* y = 0xFA24 = -1500 */
    mock_regs[0x05] = 0xB8; mock_regs[0x06] = 0x0B; /* z = 0x0BB8 = 3000 */
    mock_regs[0x09] = 0x01; /* DRDY */

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_NEAR(d.mx, 1500.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.my, -1500.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.mz, 3000.0f / 15000.0f, 0.001f);
}

TEST(test_read_not_ready)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);
    mock_regs[0x09] = 0x00; /* DRDY not set */

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

    /* x = -7500 (0xE29C), y = -15000 (0xC568), z = -1 (0xFFFF) */
    int16_t rx = -7500, ry = -15000, rz = -1;
    mock_regs[0x01] = (uint8_t)(rx & 0xFF); mock_regs[0x02] = (uint8_t)((rx >> 8) & 0xFF);
    mock_regs[0x03] = (uint8_t)(ry & 0xFF); mock_regs[0x04] = (uint8_t)((ry >> 8) & 0xFF);
    mock_regs[0x05] = (uint8_t)(rz & 0xFF); mock_regs[0x06] = (uint8_t)((rz >> 8) & 0xFF);
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_NEAR(d.mx, -7500.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.my, -15000.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.mz, -1.0f / 15000.0f, 0.001f);
}

TEST(test_read_int16_extremes)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    /* x = INT16_MAX (32767), y = INT16_MIN (-32768) */
    mock_regs[0x01] = 0xFF; mock_regs[0x02] = 0x7F; /* 0x7FFF */
    mock_regs[0x03] = 0x00; mock_regs[0x04] = 0x80; /* 0x8000 */
    mock_regs[0x05] = 0x00; mock_regs[0x06] = 0x00;
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK);
    ASSERT_NEAR(d.mx, 32767.0f / 15000.0f, 0.001f);
    ASSERT_NEAR(d.my, -32768.0f / 15000.0f, 0.001f);
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
    ASSERT_NEAR(d.mx, 0.0f, 0.001f);
    ASSERT_NEAR(d.my, 0.0f, 0.001f);
    ASSERT_NEAR(d.mz, 0.0f, 0.001f);
}

TEST(test_init_write_order)
{
    mock_reset();
    qmc5883p_init(&mock_i2c);

    /* Expected order: wake(0x0D), soft_reset(0x0B), sign(0x29), cr2(0x0B), cr1(0x0A) */
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
    /* op0=wake_write(ok), op1=chip_id_read(ok), op2=soft_reset_write(ok),
     * op3=sign_write(fail) — tests that failure at 4th I2C op propagates */
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
TEST_MAIN_END()

