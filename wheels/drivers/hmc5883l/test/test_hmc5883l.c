#include "hmc5883l.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_pass = 0, g_fail = 0;

#define TEST(name) static void name(void)
#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL %s:%d: %d != %d\n", __func__, __LINE__, \
               (int)(a), (int)(b)); \
        g_fail++; return; \
    } } while(0)
#define ASSERT_NEAR(a, b, tol) do { \
    if (fabsf((a) - (b)) > (tol)) { \
        printf("  FAIL %s:%d: %.6f not near %.6f\n", __func__, __LINE__, \
               (double)(a), (double)(b)); \
        g_fail++; return; \
    } } while(0)
#define RUN(fn) do { int pf = g_fail; fn(); if(g_fail == pf) { \
    printf("  PASS %s\n", #fn); g_pass++; } } while(0)

/* Mock */
#define MOCK_REG_COUNT 16
static uint8_t mock_regs[MOCK_REG_COUNT];
static bool mock_i2c_fail;

static bool mock_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    if (reg < MOCK_REG_COUNT)
        mock_regs[reg] = val;
    return true;
}

static bool mock_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_fail) return false;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t r = (uint8_t)(reg + i);
        buf[i] = (r < MOCK_REG_COUNT) ? mock_regs[r] : 0;
    }
    return true;
}

static hmc5883l_i2c_t mock_i2c = {
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
    .ctx       = NULL
};

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_regs[0x0A] = 'H';
    mock_regs[0x0B] = '4';
    mock_regs[0x0C] = '3';
    mock_i2c_fail = false;
}

static void set_mock_data(int16_t x, int16_t y, int16_t z)
{
    mock_regs[0x03] = (uint8_t)((uint16_t)x >> 8);
    mock_regs[0x04] = (uint8_t)(x & 0xFF);
    mock_regs[0x05] = (uint8_t)((uint16_t)z >> 8);
    mock_regs[0x06] = (uint8_t)(z & 0xFF);
    mock_regs[0x07] = (uint8_t)((uint16_t)y >> 8);
    mock_regs[0x08] = (uint8_t)(y & 0xFF);
}

TEST(test_init_null)
{
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(NULL, &mock_i2c, &cfg), HMC5883L_ERR_PARAM);
    hmc5883l_t dev;
    ASSERT_EQ(hmc5883l_init(&dev, NULL, &cfg), HMC5883L_ERR_PARAM);
}

TEST(test_init_id_check)
{
    mock_reset();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_OK);
}

TEST(test_init_bad_id)
{
    mock_reset();
    mock_regs[0x0A] = 'X';
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_ERR_ID);
}

TEST(test_read_raw)
{
    mock_reset();
    set_mock_data(1090, -545, 200);
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);

    hmc5883l_raw_t raw;
    ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_OK);
    ASSERT_EQ(raw.x, 1090);
    ASSERT_EQ(raw.y, -545);
    ASSERT_EQ(raw.z, 200);
}

TEST(test_read_gauss)
{
    mock_reset();
    set_mock_data(1090, -1090, 0);
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);

    hmc5883l_data_t data;
    ASSERT_EQ(hmc5883l_read_gauss(&dev, &data), HMC5883L_OK);
    ASSERT_NEAR(data.x, 1.0f, 0.01f);
    ASSERT_NEAR(data.y, -1.0f, 0.01f);
    ASSERT_NEAR(data.z, 0.0f, 0.01f);
}

TEST(test_different_gain)
{
    mock_reset();
    set_mock_data(440, 0, 0);
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_440, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);

    hmc5883l_data_t data;
    ASSERT_EQ(hmc5883l_read_gauss(&dev, &data), HMC5883L_OK);
    ASSERT_NEAR(data.x, 1.0f, 0.01f);
}

TEST(test_i2c_failure)
{
    mock_reset();
    mock_i2c_fail = true;
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    ASSERT_EQ(hmc5883l_init(&dev, &mock_i2c, &cfg), HMC5883L_ERR_I2C);
}

TEST(test_read_null)
{
    mock_reset();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &mock_i2c, &cfg);
    ASSERT_EQ(hmc5883l_read_raw(&dev, NULL), HMC5883L_ERR_PARAM);
    ASSERT_EQ(hmc5883l_read_gauss(&dev, NULL), HMC5883L_ERR_PARAM);
}

int main(void)
{
    printf("hmc5883l tests:\n");
    RUN(test_init_null);
    RUN(test_init_id_check);
    RUN(test_init_bad_id);
    RUN(test_read_raw);
    RUN(test_read_gauss);
    RUN(test_different_gain);
    RUN(test_i2c_failure);
    RUN(test_read_null);
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
