// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_time.h"
#include <string.h>
#include <math.h>
#include "mlx90614.h"

/* ---------- SMBus mock (command-based, not register-based) ---------- */

static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;

/* Simulated SMBus register file: cmd -> {LSB, MSB} */
static uint8_t mock_mem[256][2];
static uint8_t mock_last_cmd;
#define MOCK_WRITE_LOG 2
static uint8_t mock_write_log[MOCK_WRITE_LOG][8];
static int     mock_write_count;

static void mock_reset(void)
{
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    mock_time_reset();
    mock_last_cmd   = 0;
    mock_write_count = 0;
    memset(mock_mem, 0, sizeof(mock_mem));
    memset(mock_write_log, 0, sizeof(mock_write_log));
}

static bool mock_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

/* PEC from the manual, not the driver. p18: "The PEC is a CRC-8 with
 * polynomial X8+X2+X1+1" over every byte on the wire, MSb first. */
static uint8_t ref_pec(const uint8_t *frame, uint16_t len)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= frame[i];
        for (int bit = 0; bit < 8; bit++)
        {
            bool top = (crc & 0x80u) != 0u;
            crc = (uint8_t)(crc << 1);
            if (top) crc ^= 0x07u;
        }
    }
    return crc;
}

static bool mock_write(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (mock_should_fail()) return false;
    if (len > 0)
    {
        mock_last_cmd = data[0];
        if (mock_write_count < MOCK_WRITE_LOG &&
            len <= (uint16_t)sizeof(mock_write_log[0]))
        {
            memcpy(mock_write_log[mock_write_count], data, len);
        }
        mock_write_count++;
        /* SMBus write-word: cmd + LSB + MSB + PEC. p18: the sensor NACKs
         * a message whose PEC is wrong. */
        if (len >= 3)
        {
            uint8_t frame[4] = { (uint8_t)(addr << 1), data[0], data[1], data[2] };
            if (len != 4 || ref_pec(frame, 4) != data[3]) return false;
            mock_mem[data[0]][0] = data[1]; /* LSB */
            mock_mem[data[0]][1] = data[2]; /* MSB */
        }
    }
    return true;
}

static bool mock_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (mock_should_fail()) return false;
    /* Return {LSB, MSB, PEC} for the last written command */
    if (len >= 1) data[0] = mock_mem[mock_last_cmd][0]; /* LSB */
    if (len >= 2) data[1] = mock_mem[mock_last_cmd][1]; /* MSB */
    if (len >= 3)
    {
        uint8_t frame[5] = { (uint8_t)(addr << 1), mock_last_cmd,
                             (uint8_t)((addr << 1) | 1u), data[0], data[1] };
        data[2] = ref_pec(frame, 5);
    }
    return true;
}

static const mlx90614_i2c_t mock_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay_ms,
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

TEST(test_ref_pec_matches_manual_figures)
{
    /* Figures 6-9, p20: every byte of each frame, R/W bit included */
    static const uint8_t fig6[] = { 0xB4, 0x07, 0xB5, 0xD2, 0x3A };
    static const uint8_t fig7[] = { 0xB4, 0x22, 0x07, 0xC8 };
    static const uint8_t fig8[] = { 0xB4, 0xFF };
    static const uint8_t fig9[] = { 0xB5, 0xF0, 0x10, 0x00 };
    ASSERT_EQ_HEX(ref_pec(fig6, sizeof(fig6)), 0x30);
    ASSERT_EQ_HEX(ref_pec(fig7, sizeof(fig7)), 0x48);
    ASSERT_EQ_HEX(ref_pec(fig8, sizeof(fig8)), 0xE8);
    ASSERT_EQ_HEX(ref_pec(fig9, sizeof(fig9)), 0xED);
}

TEST(test_set_emissivity_frames_carry_pec)
{
    /* CRC-8 of B4 24 00 00 is 0x28, of B4 24 00 80 is 0xA1 (p18, p20).
     * HAL write gets cmd, LSB, MSB, PEC; it sends the address itself. */
    static const uint8_t erase[] = { 0x24, 0x00, 0x00, 0x28 };
    static const uint8_t value[] = { 0x24, 0x00, 0x80, 0xA1 };
    setup();
    mock_write_count = 0;

    mlx90614_err_t e = mlx90614_set_emissivity(&dev, 0.5f);
    ASSERT_MEMEQ_N(mock_write_log[0], erase, sizeof(erase));
    ASSERT_EQ(e, MLX90614_OK);
    ASSERT_EQ(mock_write_count, 2);
    ASSERT_MEMEQ_N(mock_write_log[1], value, sizeof(value));
}

TEST(test_set_emissivity_pec_covers_address)
{
    /* Same erase at address 0x5B: CRC-8 of B6 24 00 00 is 0x04 */
    static const uint8_t erase[] = { 0x24, 0x00, 0x00, 0x04 };
    static const mlx90614_cfg_t cfg = { .addr = 0x5Bu };
    mock_reset();
    plant_temp(0x06, 25.0f);
    ASSERT_EQ(mlx90614_init(&dev, &mock_i2c, &cfg), MLX90614_OK);
    mock_write_count = 0;

    mlx90614_err_t e = mlx90614_set_emissivity(&dev, 1.0f);
    ASSERT_MEMEQ_N(mock_write_log[0], erase, sizeof(erase));
    ASSERT_EQ(e, MLX90614_OK);
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
    mock_time_delay_total_ms = 0;

    mlx90614_err_t e = mlx90614_set_emissivity(&dev, 0.90f);
    ASSERT_EQ(e, MLX90614_OK);

    /* Mandatory EEPROM write-cycle delay between erase and write (>= 5 ms) */
    ASSERT_TRUE(mock_time_delay_total_ms >= 5);
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
    RUN(test_ref_pec_matches_manual_figures);
    RUN(test_set_emissivity_frames_carry_pec);
    RUN(test_set_emissivity_pec_covers_address);
    RUN(test_set_emissivity_range);
    RUN(test_set_emissivity_i2c_fail);
    RUN(test_high_temp);
    RUN(test_set_emissivity_eeprom_delay);
TEST_MAIN_END()
