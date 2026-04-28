#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../qmc5883p.h"

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

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_NEQ(a, b, msg)  ASSERT((a) != (b), msg)
#define ASSERT_NEAR(a, b, tol, msg) ASSERT(fabsf((a)-(b)) < (tol), msg)

/* ---------- tests ---------- */

static void test_init_ok(void)
{
    printf("test_init_ok\n");
    mock_reset();
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_OK, "init returns OK");
}

static void test_init_wake_register(void)
{
    printf("test_init_wake_register\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);
    ASSERT(write_log_idx >= 1, "at least one write");
    ASSERT_EQ(write_log[0].reg, 0x0D, "first write is wake register 0x0D");
    ASSERT_EQ(write_log[0].val, 0x40, "wake value is 0x40");
}

static void test_init_registers(void)
{
    printf("test_init_registers\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);
    ASSERT_EQ(mock_regs[0x29], 0x06, "SIGN = 0x06");
    ASSERT_EQ(mock_regs[0x0B], 0x0C, "CR2 = range ±2G");
    ASSERT_EQ(mock_regs[0x0A], 0x0F, "CR1 = OSR_8x | ODR_200Hz | MODE_CONT");
}

static void test_init_soft_reset(void)
{
    printf("test_init_soft_reset\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);
    int found = 0;
    for (int i = 0; i < write_log_idx; i++) {
        if (write_log[i].reg == 0x0B && write_log[i].val == 0x80) { found = 1; break; }
    }
    ASSERT(found, "soft reset (CR2=0x80) was issued");
}

static void test_init_bad_chip_id(void)
{
    printf("test_init_bad_chip_id\n");
    mock_reset();
    mock_regs[0x00] = 0xFF;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_ID, "wrong chip ID detected");
}

static void test_init_i2c_write_fail(void)
{
    printf("test_init_i2c_write_fail\n");
    mock_reset();
    mock_fail_next = 1;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C, "I2C write failure propagated");
}

static void test_init_i2c_read_fail(void)
{
    printf("test_init_i2c_read_fail\n");
    mock_reset();
    /* op 0 = wake write (succeed), op 1 = chip ID read (fail) */
    mock_fail_at = 1;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C, "chip ID read failure propagated");
}

static void test_read_ok(void)
{
    printf("test_read_ok\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);

    /* Plant known raw values: x=1500, y=-1500, z=3000 */
    mock_regs[0x01] = 0xDC; mock_regs[0x02] = 0x05; /* x = 0x05DC = 1500 */
    mock_regs[0x03] = 0x24; mock_regs[0x04] = 0xFA; /* y = 0xFA24 = -1500 */
    mock_regs[0x05] = 0xB8; mock_regs[0x06] = 0x0B; /* z = 0x0BB8 = 3000 */
    mock_regs[0x09] = 0x01; /* DRDY */

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK, "read OK");
    ASSERT_NEAR(d.mx, 1500.0f / 15000.0f, 0.001f, "mx = 0.1 Gauss");
    ASSERT_NEAR(d.my, -1500.0f / 15000.0f, 0.001f, "my = -0.1 Gauss");
    ASSERT_NEAR(d.mz, 3000.0f / 15000.0f, 0.001f, "mz = 0.2 Gauss");
}

static void test_read_not_ready(void)
{
    printf("test_read_not_ready\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);
    mock_regs[0x09] = 0x00; /* DRDY not set */

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_ERR_NOT_READY, "not ready detected");
}

static void test_read_i2c_fail(void)
{
    printf("test_read_i2c_fail\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);
    mock_fail_next = 1;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_ERR_I2C, "read I2C error propagated");
}

static void test_read_negative(void)
{
    printf("test_read_negative\n");
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
    ASSERT_EQ(e, QMC5883P_OK, "read negative OK");
    ASSERT_NEAR(d.mx, -7500.0f / 15000.0f, 0.001f, "mx = -0.5 Gauss");
    ASSERT_NEAR(d.my, -15000.0f / 15000.0f, 0.001f, "my = -1.0 Gauss");
    ASSERT_NEAR(d.mz, -1.0f / 15000.0f, 0.001f, "mz ~ 0");
}

static void test_read_int16_extremes(void)
{
    printf("test_read_int16_extremes\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);

    /* x = INT16_MAX (32767), y = INT16_MIN (-32768) */
    mock_regs[0x01] = 0xFF; mock_regs[0x02] = 0x7F; /* 0x7FFF */
    mock_regs[0x03] = 0x00; mock_regs[0x04] = 0x80; /* 0x8000 */
    mock_regs[0x05] = 0x00; mock_regs[0x06] = 0x00;
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK, "read int16 extremes OK");
    ASSERT_NEAR(d.mx, 32767.0f / 15000.0f, 0.001f, "mx = INT16_MAX / sensitivity");
    ASSERT_NEAR(d.my, -32768.0f / 15000.0f, 0.001f, "my = INT16_MIN / sensitivity");
}

static void test_read_zero(void)
{
    printf("test_read_zero\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);

    mock_regs[0x01] = 0; mock_regs[0x02] = 0;
    mock_regs[0x03] = 0; mock_regs[0x04] = 0;
    mock_regs[0x05] = 0; mock_regs[0x06] = 0;
    mock_regs[0x09] = 0x01;

    qmc5883p_data_t d;
    qmc5883p_err_t e = qmc5883p_read(&mock_i2c, &d);
    ASSERT_EQ(e, QMC5883P_OK, "read zero OK");
    ASSERT_NEAR(d.mx, 0.0f, 0.001f, "mx = 0");
    ASSERT_NEAR(d.my, 0.0f, 0.001f, "my = 0");
    ASSERT_NEAR(d.mz, 0.0f, 0.001f, "mz = 0");
}

static void test_init_write_order(void)
{
    printf("test_init_write_order\n");
    mock_reset();
    qmc5883p_init(&mock_i2c);

    /* Expected order: wake(0x0D), soft_reset(0x0B), sign(0x29), cr2(0x0B), cr1(0x0A) */
    ASSERT(write_log_idx >= 5, "at least 5 writes during init");
    ASSERT_EQ(write_log[0].reg, 0x0D, "1st: wake register");
    ASSERT_EQ(write_log[1].reg, 0x0B, "2nd: soft reset (CR2)");
    ASSERT_EQ(write_log[1].val, 0x80, "2nd: soft reset value");
    ASSERT_EQ(write_log[2].reg, 0x29, "3rd: SIGN register");
    ASSERT_EQ(write_log[3].reg, 0x0B, "4th: CR2 (range)");
    ASSERT_EQ(write_log[4].reg, 0x0A, "5th: CR1 (mode)");
}

static void test_init_mid_step_fail(void)
{
    printf("test_init_mid_step_fail\n");
    mock_reset();
    /* op0=wake_write(ok), op1=chip_id_read(ok), op2=soft_reset_write(ok),
     * op3=sign_write(fail) — tests that failure at 4th I2C op propagates */
    mock_fail_at = 3;
    qmc5883p_err_t e = qmc5883p_init(&mock_i2c);
    ASSERT_EQ(e, QMC5883P_ERR_I2C, "mid-init I2C failure propagated");
}

/* ---------- main ---------- */

int main(void)
{
    test_init_ok();
    test_init_wake_register();
    test_init_registers();
    test_init_soft_reset();
    test_init_write_order();
    test_init_bad_chip_id();
    test_init_i2c_write_fail();
    test_init_i2c_read_fail();
    test_init_mid_step_fail();
    test_read_ok();
    test_read_not_ready();
    test_read_i2c_fail();
    test_read_negative();
    test_read_int16_extremes();
    test_read_zero();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
