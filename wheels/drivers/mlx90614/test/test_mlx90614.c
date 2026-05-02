#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "mlx90614.h"

/* ---------- SMBus mock (command-based, not register-based) ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;
static int     mock_delay_total;

/* Simulated SMBus register file: cmd -> {LSB, MSB} */
static uint8_t mock_mem[256][2];
static uint8_t mock_last_cmd;
static uint8_t mock_last_write[8];
static int     mock_last_write_len;

static void mock_reset(void)
{
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    mock_delay_total = 0;
    mock_last_cmd   = 0;
    mock_last_write_len = 0;
    memset(mock_mem, 0, sizeof(mock_mem));
    memset(mock_last_write, 0, sizeof(mock_last_write));
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

static bool mock_write(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    if (len > 0)
    {
        mock_last_cmd = data[0];
        if (len <= (uint16_t)sizeof(mock_last_write))
        {
            memcpy(mock_last_write, data, len);
            mock_last_write_len = (int)len;
        }
        /* SMBus write-word: cmd + LSB + MSB + PEC */
        if (len >= 3)
        {
            mock_mem[data[0]][0] = data[1]; /* LSB */
            mock_mem[data[0]][1] = data[2]; /* MSB */
        }
    }
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx; (void)addr;
    if (mock_should_fail()) return false;
    /* Return {LSB, MSB, PEC=0} for the last written command */
    if (len >= 1) data[0] = mock_mem[mock_last_cmd][0]; /* LSB */
    if (len >= 2) data[1] = mock_mem[mock_last_cmd][1]; /* MSB */
    if (len >= 3) data[2] = 0; /* PEC placeholder */
    return true;
}

static void mock_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    mock_delay_total += (int)ms;
}

static const mlx90614_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

/* ---------- helpers ---------- */

static void plant_temp(uint8_t reg, float temp_c)
{
    /* raw = (temp_c + 273.15) / 0.02 */
    uint16_t raw = (uint16_t)((temp_c + 273.15f) / 0.02f);
    mock_mem[reg][0] = (uint8_t)(raw & 0xFFu);
    mock_mem[reg][1] = (uint8_t)(raw >> 8);
}

static void plant_raw(uint8_t reg, uint16_t raw)
{
    mock_mem[reg][0] = (uint8_t)(raw & 0xFFu);
    mock_mem[reg][1] = (uint8_t)(raw >> 8);
}

static mlx90614_t dev;
static mlx90614_cfg_t default_cfg = { .addr = MLX90614_ADDR_DEFAULT };

static void setup(void)
{
    mock_reset();
    plant_temp(0x06, 25.0f); /* ambient = 25 C for init check */
    mlx90614_init(&dev, &mock_i2c, &default_cfg);
    mock_op_count = 0;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    plant_temp(0x06, 25.0f);
    mlx90614_t d;
    mlx90614_err_t e = mlx90614_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MLX90614_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    mlx90614_t d;
    mlx90614_err_t e = mlx90614_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, MLX90614_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(mlx90614_init(NULL, &mock_i2c, &default_cfg), MLX90614_ERR_PARAM);
    mlx90614_t d;
    ASSERT_EQ(mlx90614_init(&d, NULL, &default_cfg), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_init(&d, &mock_i2c, NULL), MLX90614_ERR_PARAM);
    float temp;
    ASSERT_EQ(mlx90614_read_object_temp(NULL, &temp), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_read_object_temp(&d, NULL), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_read_ambient_temp(NULL, &temp), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_read_ambient_temp(&d, NULL), MLX90614_ERR_PARAM);
    float em;
    ASSERT_EQ(mlx90614_read_emissivity(NULL, &em), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_read_emissivity(&d, NULL), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_set_emissivity(NULL, 0.95f), MLX90614_ERR_PARAM);
}

TEST(test_read_object_25c)
{
    setup();
    plant_temp(0x07, 25.0f);

    float temp;
    mlx90614_err_t e = mlx90614_read_object_temp(&dev, &temp);
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_NEAR(temp, 25.0f, 0.1f);
}

TEST(test_read_object_0c)
{
    setup();
    plant_temp(0x07, 0.0f);

    float temp;
    mlx90614_err_t e = mlx90614_read_object_temp(&dev, &temp);
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_NEAR(temp, 0.0f, 0.1f);
}

TEST(test_read_object_negative)
{
    setup();
    plant_temp(0x07, -20.0f);

    float temp;
    mlx90614_err_t e = mlx90614_read_object_temp(&dev, &temp);
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_NEAR(temp, -20.0f, 0.1f);
}

TEST(test_read_ambient)
{
    setup();
    plant_temp(0x06, 22.5f);

    float temp;
    mlx90614_err_t e = mlx90614_read_ambient_temp(&dev, &temp);
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_NEAR(temp, 22.5f, 0.1f);
}

TEST(test_read_object_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    float temp;
    mlx90614_err_t e = mlx90614_read_object_temp(&dev, &temp);
    ASSERT_EQ(e, MLX90614_ERR_I2C);
}

TEST(test_read_error_flag)
{
    setup();
    /* Set error flag bit (0x8000) */
    plant_raw(0x07, 0x8000u);

    float temp;
    mlx90614_err_t e = mlx90614_read_object_temp(&dev, &temp);
    ASSERT_EQ(e, MLX90614_ERR_RANGE);
}

TEST(test_read_emissivity)
{
    setup();
    /* 1.0 emissivity = 0xFFFF */
    plant_raw(0x24, 0xFFFFu);

    float em;
    mlx90614_err_t e = mlx90614_read_emissivity(&dev, &em);
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_NEAR(em, 1.0f, 0.001f);
}

TEST(test_set_emissivity)
{
    setup();
    mlx90614_err_t e = mlx90614_set_emissivity(&dev, 0.95f);
    ASSERT_EQ(e, MLX90614_OK);

    /* Verify the written value by reading back from mock */
    float em;
    e = mlx90614_read_emissivity(&dev, &em);
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_NEAR(em, 0.95f, 0.001f);
}

TEST(test_set_emissivity_range)
{
    setup();
    ASSERT_EQ(mlx90614_set_emissivity(&dev, 0.05f), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_set_emissivity(&dev, 1.1f), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_set_emissivity(&dev, 0.0f), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_set_emissivity(&dev, -0.5f), MLX90614_ERR_PARAM);
}

TEST(test_set_emissivity_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    mlx90614_err_t e = mlx90614_set_emissivity(&dev, 0.95f);
    ASSERT_EQ(e, MLX90614_ERR_I2C);
}

TEST(test_high_temp)
{
    setup();
    /* 300 C — near upper range of MLX90614 */
    plant_temp(0x07, 300.0f);

    float temp;
    mlx90614_err_t e = mlx90614_read_object_temp(&dev, &temp);
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_NEAR(temp, 300.0f, 0.1f);
}

TEST(test_set_emissivity_eeprom_delay)
{
    setup();
    mock_delay_total = 0;

    mlx90614_err_t e = mlx90614_set_emissivity(&dev, 0.90f);
    ASSERT_EQ(e, MLX90614_OK);

    /* Mandatory EEPROM write-cycle delay between erase and write (>= 5 ms) */
    ASSERT_TRUE(mock_delay_total >= 5);
}

TEST_MAIN("mlx90614")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_object_25c);
    RUN(test_read_object_0c);
    RUN(test_read_object_negative);
    RUN(test_read_ambient);
    RUN(test_read_object_i2c_fail);
    RUN(test_read_error_flag);
    RUN(test_read_emissivity);
    RUN(test_set_emissivity);
    RUN(test_set_emissivity_range);
    RUN(test_set_emissivity_i2c_fail);
    RUN(test_high_temp);
    RUN(test_set_emissivity_eeprom_delay);
TEST_MAIN_END()
