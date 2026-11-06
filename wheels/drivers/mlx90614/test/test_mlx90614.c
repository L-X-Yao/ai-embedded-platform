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
#define MOCK_WRITE_LOG 2
static uint8_t mock_write_log[MOCK_WRITE_LOG][8];
static int     mock_write_count;

/* The last read word: address, command bytes sent, bytes read */
static int      mock_read_word_count;
static uint8_t  mock_read_word_addr;
static uint16_t mock_read_word_tx_len;
static uint8_t  mock_read_word_cmd;
static uint16_t mock_read_word_rx_len;

static void mock_reset(void)
{
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    mock_time_reset();
    mock_write_count = 0;
    mock_read_word_count  = 0;
    mock_read_word_addr   = 0;
    mock_read_word_tx_len = 0;
    mock_read_word_cmd    = 0;
    mock_read_word_rx_len = 0;
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
            /* p.13: an erase (write 0) has to come first */
            if ((mock_mem[data[0]][0] | mock_mem[data[0]][1]) != 0 &&
                (data[1] | data[2]) != 0) return true;
            mock_mem[data[0]][0] = data[1]; /* LSB */
            mock_mem[data[0]][1] = data[2]; /* MSB */
        }
    }
    return true;
}

/* Figure 6, p20: S SA W cmd Sr SA R LSB MSB PEC P */
static bool mock_write_read(void *ctx, uint8_t addr,
                            const uint8_t *tx, uint16_t tx_len,
                            uint8_t *rx, uint16_t rx_len)
{
    (void)ctx;
    if (mock_should_fail()) return false;
    mock_read_word_count++;
    mock_read_word_addr   = addr;
    mock_read_word_tx_len = tx_len;
    mock_read_word_cmd    = tx[0];
    mock_read_word_rx_len = rx_len;
    if (tx_len != 1 || rx_len != 3) return false;

    rx[0] = mock_mem[tx[0]][0]; /* LSB */
    rx[1] = mock_mem[tx[0]][1]; /* MSB */
    uint8_t frame[5] = { (uint8_t)(addr << 1), tx[0],
                         (uint8_t)((addr << 1) | 1u), rx[0], rx[1] };
    rx[2] = ref_pec(frame, 5);
    return true;
}

static const mlx90614_i2c_t mock_i2c = {
    .write      = mock_write,
    .delay_ms   = mock_delay_ms,
    .ctx        = NULL,
    .write_read = mock_write_read,
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

TEST(test_init_null_write_read)
{
    /* A HAL built for the old driver: no fallback to write then read */
    mlx90614_i2c_t no_wr = mock_i2c;
    no_wr.write_read = NULL;
    mlx90614_t d;
    mock_reset();
    ASSERT_EQ(mlx90614_init(&d, &no_wr, &default_cfg), MLX90614_ERR_PARAM);
    ASSERT_EQ(mock_op_count, 0);
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

TEST(test_reads_are_one_read_word_each)
{
    /* Figure 6, p20: command, repeated START, then LSB, MSB and PEC. A
     * write that ends with STOP must not appear anywhere in a read. */
    float value;
    setup();
    mock_read_word_count = 0;

    ASSERT_EQ(mlx90614_read_object_temp(&dev, &value), MLX90614_OK);
    ASSERT_EQ(mock_read_word_count, 1);
    ASSERT_EQ_HEX(mock_read_word_addr, 0x5A);
    ASSERT_EQ(mock_read_word_tx_len, 1);
    ASSERT_EQ_HEX(mock_read_word_cmd, 0x07);
    ASSERT_EQ(mock_read_word_rx_len, 3);

    ASSERT_EQ(mlx90614_read_ambient_temp(&dev, &value), MLX90614_OK);
    ASSERT_EQ(mock_read_word_count, 2);
    ASSERT_EQ_HEX(mock_read_word_cmd, 0x06);

    ASSERT_EQ(mlx90614_read_emissivity(&dev, &value), MLX90614_OK);
    ASSERT_EQ(mock_read_word_count, 3);
    ASSERT_EQ_HEX(mock_read_word_cmd, 0x24);

    ASSERT_EQ(mock_write_count, 0);
}

TEST(test_init_reads_ta_in_one_read_word)
{
    mock_reset();
    plant_temp(0x06, 25.0f);
    mlx90614_t d;
    ASSERT_EQ(mlx90614_init(&d, &mock_i2c, &default_cfg), MLX90614_OK);
    ASSERT_EQ(mock_read_word_count, 1);
    ASSERT_EQ_HEX(mock_read_word_cmd, 0x06);
    ASSERT_EQ(mock_write_count, 0);
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

TEST(test_set_emissivity_requires_delay)
{
    /* p16: after each write, "Wait at least 5ms (10ms to be on the safe
     * side)". Without a delay callback the cell must stay untouched. */
    mlx90614_i2c_t no_delay = mock_i2c;
    no_delay.delay_ms = NULL;
    mlx90614_t d;
    mock_reset();
    plant_temp(0x06, 25.0f);
    ASSERT_EQ(mlx90614_init(&d, &no_delay, &default_cfg), MLX90614_OK);
    mock_write_count = 0;

    ASSERT_EQ(mlx90614_set_emissivity(&d, 0.95f), MLX90614_ERR_PARAM);
    ASSERT_EQ(mock_write_count, 0);
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

/* p.13: "Erase (write 0) must take place before the write of the desired data
 * is done", and the cell comes with the factory 0xFFFF */
TEST(test_set_emissivity_over_a_written_cell)
{
    setup();
    plant_raw(0x24, 0xFFFF);
    ASSERT_EQ(mlx90614_set_emissivity(&dev, 0.95f), MLX90614_OK);

    float em;
    ASSERT_EQ(mlx90614_read_emissivity(&dev, &em), MLX90614_OK);
    ASSERT_NEAR(em, 0.95f, 0.001f);
}

/* A write over an unerased cell must not take, or that test proves nothing */
TEST(test_mock_refuses_a_write_over_a_written_cell)
{
    static const uint8_t wire[4] = { 0xB4, 0x24, 0x32, 0xF3 };
    uint8_t frame[4] = { 0x24, 0x32, 0xF3, ref_pec(wire, 4) };

    setup();
    plant_raw(0x24, 0xFFFF);
    ASSERT_TRUE(mock_write(NULL, 0x5A, frame, 4));
    ASSERT_EQ_HEX(mock_mem[0x24][0], 0xFF);
    ASSERT_EQ_HEX(mock_mem[0x24][1], 0xFF);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(mlx90614_t *d)
{
    float f = 0.0f;
    ASSERT_EQ(mlx90614_read_object_temp(d, &f), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_read_ambient_temp(d, &f), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_read_emissivity(d, &f), MLX90614_ERR_PARAM);
    ASSERT_EQ(mlx90614_set_emissivity(d, 0.9f), MLX90614_ERR_PARAM);
}

TEST(test_zeroed_device_refused)
{
    static mlx90614_t zeroed;
    assert_device_refused(&zeroed);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_failed_init)
{
    mlx90614_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(mlx90614_init(&d, NULL, &default_cfg), MLX90614_ERR_PARAM);
    assert_device_refused(&d);

    /* Nothing answered the ambient read, and the bus is fine again */
    mock_fail_at = 0;
    ASSERT_EQ(mlx90614_init(&d, &mock_i2c, &default_cfg), MLX90614_ERR_I2C);
    mock_reset();
    assert_device_refused(&d);
}

TEST_MAIN("mlx90614")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_init_null_write_read);
    RUN(test_null_safety);
    RUN(test_read_object_25c);
    RUN(test_read_object_0c);
    RUN(test_read_object_negative);
    RUN(test_read_ambient);
    RUN(test_reads_are_one_read_word_each);
    RUN(test_init_reads_ta_in_one_read_word);
    RUN(test_read_object_i2c_fail);
    RUN(test_read_error_flag);
    RUN(test_read_emissivity);
    RUN(test_set_emissivity);
    RUN(test_ref_pec_matches_manual_figures);
    RUN(test_set_emissivity_frames_carry_pec);
    RUN(test_set_emissivity_pec_covers_address);
    RUN(test_set_emissivity_requires_delay);
    RUN(test_set_emissivity_range);
    RUN(test_set_emissivity_i2c_fail);
    RUN(test_high_temp);
    RUN(test_set_emissivity_eeprom_delay);
    RUN(test_set_emissivity_over_a_written_cell);
    RUN(test_mock_refuses_a_write_over_a_written_cell);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_failed_init);
TEST_MAIN_END()
