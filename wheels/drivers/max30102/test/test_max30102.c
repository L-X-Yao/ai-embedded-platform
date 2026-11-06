// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "max30102.h"

/* --- register addresses (duplicated from .c for test access) --- */
#define REG_FIFO_WR_PTR  0x04u
#define REG_OVF_COUNTER  0x05u
#define REG_FIFO_RD_PTR  0x06u
#define REG_FIFO_DATA    0x07u
#define REG_FIFO_CONFIG  0x08u
#define REG_MODE_CONFIG  0x09u
#define REG_SPO2_CONFIG  0x0Au
#define REG_LED1_PA      0x0Cu
#define REG_LED2_PA      0x0Du
#define REG_TEMP_INT     0x1Fu
#define REG_TEMP_FRAC    0x20u
#define REG_TEMP_CONFIG  0x21u
#define REG_PART_ID      0xFFu

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* set part ID */
    mock_regs[REG_PART_ID] = 0x15;
}

static const max30102_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* 19-7740 Rev 0 p14: FIFO_WR_PTR "can also be changed through the I2C
 * interface when MODE[2:0] is 010, 011, or 111".  Other writes pass through. */
static bool chip_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    uint8_t m = (uint8_t)(mock_regs[REG_MODE_CONFIG] & 0x07u);
    if (reg == REG_FIFO_WR_PTR && m != 2u && m != 3u && m != 7u)
        return true;
    return mock_i2c_write_reg(ctx, dev_addr, reg, val);
}

static const max30102_i2c_t chip_i2c = {
    .write_reg = chip_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */
static max30102_t dev;

static void setup(void)
{
    mock_reset();
    max30102_init(&dev, &mock_i2c);
}

/* Plant FIFO samples for SpO2 mode (6 bytes per sample: 3 RED + 3 IR).
 * wr_ptr and rd_ptr control how many samples appear available. */
static void plant_fifo_spo2(uint8_t rd, uint8_t wr,
                             const uint8_t *data, uint8_t data_len)
{
    mock_regs[REG_FIFO_WR_PTR] = wr;
    mock_regs[REG_FIFO_RD_PTR] = rd;
    /* Plant raw bytes starting at FIFO_DATA register.
     * The mock reads sequentially from mock_regs[0x07..]. */
    for (uint8_t i = 0; i < data_len; i++)
    {
        mock_regs[REG_FIFO_DATA + i] = data[i];
    }
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    max30102_t d;
    max30102_err_t e = max30102_init(&d, &mock_i2c);
    ASSERT_EQ(e, MAX30102_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[REG_PART_ID] = 0x00;
    max30102_t d;
    max30102_err_t e = max30102_init(&d, &mock_i2c);
    ASSERT_EQ(e, MAX30102_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;   /* fail on first I2C operation (part ID read) */
    max30102_t d;
    max30102_err_t e = max30102_init(&d, &mock_i2c);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(max30102_init(NULL, &mock_i2c), MAX30102_ERR_PARAM);

    max30102_t d;
    ASSERT_EQ(max30102_init(&d, NULL), MAX30102_ERR_PARAM);

    ASSERT_EQ(max30102_set_mode(NULL, MAX30102_MODE_HR), MAX30102_ERR_PARAM);

    ASSERT_EQ(max30102_set_led_current(NULL, 10, 10), MAX30102_ERR_PARAM);

    ASSERT_EQ(max30102_set_sample_rate(NULL, 100), MAX30102_ERR_PARAM);

    max30102_sample_t samp;
    uint8_t cnt;
    ASSERT_EQ(max30102_read_fifo(NULL, &samp, 1, &cnt), MAX30102_ERR_PARAM);
    ASSERT_EQ(max30102_read_fifo(&d, NULL, 1, &cnt), MAX30102_ERR_PARAM);
    ASSERT_EQ(max30102_read_fifo(&d, &samp, 1, NULL), MAX30102_ERR_PARAM);
    ASSERT_EQ(max30102_read_fifo(&d, &samp, 0, &cnt), MAX30102_ERR_PARAM);

    float temp;
    ASSERT_EQ(max30102_read_temp(NULL, &temp), MAX30102_ERR_PARAM);
    ASSERT_EQ(max30102_read_temp(&d, NULL), MAX30102_ERR_PARAM);

    ASSERT_EQ(max30102_shutdown(NULL, true), MAX30102_ERR_PARAM);
    ASSERT_EQ(max30102_reset(NULL), MAX30102_ERR_PARAM);

    /* read_part_id returns 0 on NULL */
    ASSERT_EQ(max30102_read_part_id(NULL), 0);
}

TEST(test_init_writes_defaults)
{
    setup();
    /* After init: FIFO config = 0x10 (rollover on) */
    ASSERT_EQ(mock_regs[REG_FIFO_CONFIG], 0x10);
    /* SPO2 config = 0x27 (ADC 4096, 100 sps, 411 us) */
    ASSERT_EQ(mock_regs[REG_SPO2_CONFIG], 0x27);
    /* LED defaults: 0x20 each */
    ASSERT_EQ(mock_regs[REG_LED1_PA], 0x20);
    ASSERT_EQ(mock_regs[REG_LED2_PA], 0x20);
    /* Mode = SpO2 (0x03) */
    ASSERT_EQ(mock_regs[REG_MODE_CONFIG] & 0x07, 0x03);
}

TEST(test_set_mode_hr)
{
    setup();
    max30102_err_t e = max30102_set_mode(&dev, MAX30102_MODE_HR);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_MODE_CONFIG] & 0x07, 0x02);
}

TEST(test_set_mode_spo2)
{
    setup();
    /* switch to HR first, then back to SpO2 */
    max30102_set_mode(&dev, MAX30102_MODE_HR);
    max30102_err_t e = max30102_set_mode(&dev, MAX30102_MODE_SPO2);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_MODE_CONFIG] & 0x07, 0x03);
}

TEST(test_set_mode_clears_fifo)
{
    setup();
    /* p16: pointers "should be cleared (back to 0x00) upon entering SpO2
     * mode or HR mode, so that there is no old data represented in the FIFO" */
    mock_regs[REG_FIFO_WR_PTR] = 5;
    mock_regs[REG_OVF_COUNTER] = 2;
    mock_regs[REG_FIFO_RD_PTR] = 1;

    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_FIFO_WR_PTR], 0);
    ASSERT_EQ(mock_regs[REG_OVF_COUNTER], 0);
    ASSERT_EQ(mock_regs[REG_FIFO_RD_PTR], 0);
}

TEST(test_set_mode_writes_mode_before_clearing)
{
    mock_reset();
    max30102_t d;
    ASSERT_EQ(max30102_init(&d, &chip_i2c), MAX30102_OK);
    ASSERT_EQ(max30102_reset(&d), MAX30102_OK);

    /* RESET has self-cleared and MODE is 000 (p19): WR_PTR is not writable
     * until the new mode is in */
    mock_regs[REG_MODE_CONFIG] = 0;
    mock_regs[REG_FIFO_WR_PTR] = 6;

    ASSERT_EQ(max30102_set_mode(&d, MAX30102_MODE_HR), MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_FIFO_WR_PTR], 0);
}

TEST(test_set_mode_fifo_clear_i2c_fail)
{
    /* ops: SPO2_CONFIG read, MODE read, MODE write, then the three clears */
    for (int op = 3; op < 6; op++)
    {
        setup();
        mock_fail_at = mock_op_count + op;
        ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_ERR_I2C);
        /* the chip is already in HR mode, so samples are 3 bytes from now on */
        ASSERT_EQ(dev.mode, MAX30102_MODE_HR);
    }
}

TEST(test_set_mode_invalid)
{
    setup();
    max30102_err_t e = max30102_set_mode(&dev, (max30102_mode_t)0xFF);
    ASSERT_EQ(e, MAX30102_ERR_PARAM);
}

TEST(test_set_mode_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    max30102_err_t e = max30102_set_mode(&dev, MAX30102_MODE_HR);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

TEST(test_set_led_current)
{
    setup();
    /* 10.0 mA = 100 in 0.1 mA units -> register = 100/2 = 50 (0x32)
     * 20.0 mA = 200 in 0.1 mA units -> register = 200/2 = 100 (0x64) */
    max30102_err_t e = max30102_set_led_current(&dev, 100, 200);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_LED1_PA], 50);
    ASSERT_EQ(mock_regs[REG_LED2_PA], 100);
}

TEST(test_set_led_current_zero)
{
    setup();
    max30102_err_t e = max30102_set_led_current(&dev, 0, 0);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_LED1_PA], 0);
    ASSERT_EQ(mock_regs[REG_LED2_PA], 0);
}

TEST(test_set_led_current_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    max30102_err_t e = max30102_set_led_current(&dev, 100, 100);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

/* 19-7740 Rev 0 p24, Tables 11 and 12.  Rows are the samples per second of
 * rate_list, columns LED_PW 00..11 (69, 118, 215, 411 us), 1 is an "O". */
static const uint16_t rate_list[8] = {50, 100, 200, 400, 800, 1000, 1600, 3200};
static const uint8_t allowed_spo2[8][4] = {
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0},
};
static const uint8_t allowed_hr[8][4] = {
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 0}, {1, 0, 0, 0},
};

TEST(test_set_sample_rate_tables_11_12)
{
    for (int hr = 0; hr < 2; hr++)
    {
        for (uint8_t pw = 0; pw < 4; pw++)
        {
            for (uint8_t i = 0; i < 8; i++)
            {
                setup();
                ASSERT_EQ(max30102_set_mode(&dev, hr ? MAX30102_MODE_HR
                                                     : MAX30102_MODE_SPO2),
                          MAX30102_OK);
                uint8_t before = (uint8_t)(0x20u | pw);   /* 50 sps */
                mock_regs[REG_SPO2_CONFIG] = before;

                max30102_err_t e = max30102_set_sample_rate(&dev, rate_list[i]);
                if (hr ? allowed_hr[i][pw] : allowed_spo2[i][pw])
                {
                    ASSERT_EQ(e, MAX30102_OK);
                    /* SR bits are [4:2] of SPO2_CONFIG */
                    uint8_t sr_bits =
                        (uint8_t)((mock_regs[REG_SPO2_CONFIG] >> 2) & 0x07);
                    ASSERT_EQ(sr_bits, i);
                }
                else
                {
                    ASSERT_EQ(e, MAX30102_ERR_PARAM);
                    ASSERT_EQ_HEX(mock_regs[REG_SPO2_CONFIG], before);
                }
            }
        }
    }
}

TEST(test_set_sample_rate_default_pw_limits)
{
    /* init leaves LED_PW at 411 us: Table 11 stops at 400, Table 12 at 1000 */
    setup();
    ASSERT_EQ(max30102_set_sample_rate(&dev, 400), MAX30102_OK);
    ASSERT_EQ(max30102_set_sample_rate(&dev, 800), MAX30102_ERR_PARAM);

    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);
    ASSERT_EQ(max30102_set_sample_rate(&dev, 1000), MAX30102_OK);
    ASSERT_EQ(max30102_set_sample_rate(&dev, 1600), MAX30102_ERR_PARAM);
}

TEST(test_set_mode_spo2_refuses_rate_above_limit)
{
    setup();
    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);
    ASSERT_EQ(max30102_set_sample_rate(&dev, 1000), MAX30102_OK);

    /* 1000 sps at 411 us is in Table 12 but not in Table 11 */
    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_SPO2), MAX30102_ERR_PARAM);
    ASSERT_EQ(mock_regs[REG_MODE_CONFIG] & 0x07, 0x02);
    ASSERT_EQ(dev.mode, MAX30102_MODE_HR);

    /* lowering the rate first makes the switch legal */
    ASSERT_EQ(max30102_set_sample_rate(&dev, 400), MAX30102_OK);
    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_SPO2), MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_MODE_CONFIG] & 0x07, 0x03);
}

TEST(test_set_mode_rate_check_i2c_fail)
{
    /* every read and write inside set_mode must report a bus failure */
    for (int op = 0; op < 3; op++)
    {
        setup();
        mock_fail_at = mock_op_count + op;
        ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_ERR_I2C);
    }
}

TEST(test_set_sample_rate_invalid)
{
    setup();
    max30102_err_t e = max30102_set_sample_rate(&dev, 999);
    ASSERT_EQ(e, MAX30102_ERR_PARAM);
}

TEST(test_set_sample_rate_preserves_other_bits)
{
    setup();
    /* SPO2_CONFIG after init = 0x27 (ADC range[6:5]=01, SR[4:2]=001, PW[1:0]=11) */
    /* Set rate to 400 (SR=3): expect bits [6:5]=01, [4:2]=011, [1:0]=11 = 0x2F */
    max30102_err_t e = max30102_set_sample_rate(&dev, 400);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ_HEX(mock_regs[REG_SPO2_CONFIG], 0x2F);
}

TEST(test_set_sample_rate_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    max30102_err_t e = max30102_set_sample_rate(&dev, 100);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

TEST(test_read_fifo_spo2_one_sample)
{
    setup();
    /* Plant one SpO2 sample: RED=0x012345, IR=0x06789A (both 18-bit masked) */
    uint8_t fifo_data[] = {
        0x01, 0x23, 0x45,   /* RED: 0x012345 & 0x03FFFF = 0x012345 */
        0x02, 0x78, 0x9A,   /* IR:  0x02789A & 0x03FFFF = 0x02789A */
    };
    plant_fifo_spo2(0, 1, fifo_data, sizeof(fifo_data));

    max30102_sample_t samples[4];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 4, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 1);
    ASSERT_EQ_HEX(samples[0].red, 0x012345u);
    ASSERT_EQ_HEX(samples[0].ir,  0x02789Au);
}

TEST(test_read_fifo_spo2_adc_mask)
{
    setup();
    /* Test 18-bit mask: top 6 bits should be stripped.
     * 0xFF 0xFF 0xFF => masked to 0x03FFFF */
    uint8_t fifo_data[] = {
        0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF,
    };
    plant_fifo_spo2(0, 1, fifo_data, sizeof(fifo_data));

    max30102_sample_t samples[1];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 1, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 1);
    ASSERT_EQ_HEX(samples[0].red, 0x03FFFFu);
    ASSERT_EQ_HEX(samples[0].ir,  0x03FFFFu);
}

TEST(test_read_fifo_empty)
{
    setup();
    /* wr_ptr == rd_ptr => no samples */
    mock_regs[REG_FIFO_WR_PTR] = 5;
    mock_regs[REG_FIFO_RD_PTR] = 5;

    max30102_sample_t samples[4];
    uint8_t count = 99;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 4, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 0);
}

TEST(test_read_fifo_full_equal_pointers)
{
    setup();
    /* 19-7740 Rev 0 p18: a full FIFO has "all 32 FIFO words" unread, so the
     * 5-bit pointers (p14) are equal again.  OVF_COUNTER (p14) only counts
     * once the FIFO is full, which sets this apart from an empty FIFO. */
    mock_regs[REG_FIFO_WR_PTR] = 5;
    mock_regs[REG_FIFO_RD_PTR] = 5;
    mock_regs[REG_OVF_COUNTER] = 3;

    max30102_sample_t samples[32];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 32, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 32);
}

TEST(test_read_fifo_full_max_count_limit)
{
    setup();
    mock_regs[REG_FIFO_WR_PTR] = 9;
    mock_regs[REG_FIFO_RD_PTR] = 9;
    mock_regs[REG_OVF_COUNTER] = 0x0F;   /* saturated, p14 */

    max30102_sample_t samples[4];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 4, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 4);
}

TEST(test_read_fifo_ovf_unused_bits_ignored)
{
    setup();
    /* OVF_COUNTER is bits [4:0] (p14): bits 7:5 say nothing about the FIFO */
    mock_regs[REG_FIFO_WR_PTR] = 5;
    mock_regs[REG_FIFO_RD_PTR] = 5;
    mock_regs[REG_OVF_COUNTER] = 0xE0;

    max30102_sample_t samples[4];
    uint8_t count = 99;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 4, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 0);
}

TEST(test_read_fifo_ovf_i2c_fail)
{
    setup();
    mock_regs[REG_FIFO_WR_PTR] = 5;
    mock_regs[REG_FIFO_RD_PTR] = 5;
    /* the two pointer reads pass, the OVF_COUNTER read fails */
    mock_fail_at = mock_op_count + 2;

    max30102_sample_t samples[4];
    uint8_t count = 99;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 4, &count);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
    ASSERT_EQ(count, 0);
}

TEST(test_read_fifo_hr_mode)
{
    setup();
    max30102_set_mode(&dev, MAX30102_MODE_HR);

    /* In HR mode, only 3 bytes per sample (RED only) */
    uint8_t fifo_data[] = {
        0x00, 0xAB, 0xCD,   /* RED: 0x00ABCD */
    };
    plant_fifo_spo2(0, 1, fifo_data, sizeof(fifo_data));

    max30102_sample_t samples[1];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 1, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 1);
    ASSERT_EQ_HEX(samples[0].red, 0x00ABCDu);
    ASSERT_EQ(samples[0].ir, 0u);  /* IR not populated in HR mode */
}

TEST(test_read_fifo_max_count_limit)
{
    setup();
    /* 5 samples available, but buffer only holds 2 */
    mock_regs[REG_FIFO_WR_PTR] = 5;
    mock_regs[REG_FIFO_RD_PTR] = 0;
    /* plant enough data */
    for (uint8_t i = 0; i < 30; i++)
        mock_regs[REG_FIFO_DATA + i] = i;

    max30102_sample_t samples[2];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 2, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 2);
}

TEST(test_read_fifo_i2c_fail)
{
    setup();
    mock_regs[REG_FIFO_WR_PTR] = 1;
    mock_regs[REG_FIFO_RD_PTR] = 0;
    /* fail on first operation inside read_fifo (the WR_PTR read) */
    mock_fail_next = 1;

    max30102_sample_t samples[1];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 1, &count);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

TEST(test_read_fifo_wrap)
{
    setup();
    /* Wrap: rd=30, wr=2 => (2-30) & 0x1F = 4 samples */
    mock_regs[REG_FIFO_WR_PTR] = 2;
    mock_regs[REG_FIFO_RD_PTR] = 30;
    for (uint8_t i = 0; i < 24; i++)
        mock_regs[REG_FIFO_DATA + i] = (uint8_t)(i + 1u);

    max30102_sample_t samples[4];
    uint8_t count = 0;
    max30102_err_t e = max30102_read_fifo(&dev, samples, 4, &count);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_EQ(count, 4);
}

TEST(test_read_temp)
{
    setup();
    /* 25.5 C = int_part=25 (0x19), frac_part=8 (8*0.0625=0.5) */
    mock_regs[REG_TEMP_INT]  = 25;
    mock_regs[REG_TEMP_FRAC] = 8;

    float temp = 0.0f;
    max30102_err_t e = max30102_read_temp(&dev, &temp);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_NEAR(temp, 25.5f, 0.01f);
}

TEST(test_read_temp_negative)
{
    setup();
    /* -2.75 C: int_part = -3 (0xFD as int8_t), frac = 4 (4*0.0625=0.25)
     * Result = -3 + 0.25 = -2.75 */
    mock_regs[REG_TEMP_INT]  = 0xFD;  /* -3 as int8_t */
    mock_regs[REG_TEMP_FRAC] = 4;

    float temp = 0.0f;
    max30102_err_t e = max30102_read_temp(&dev, &temp);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_NEAR(temp, -2.75f, 0.01f);
}

TEST(test_read_temp_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    float temp;
    max30102_err_t e = max30102_read_temp(&dev, &temp);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

TEST(test_shutdown_enter)
{
    setup();
    max30102_err_t e = max30102_shutdown(&dev, true);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_TRUE(mock_regs[REG_MODE_CONFIG] & 0x80);
}

TEST(test_shutdown_exit)
{
    setup();
    max30102_shutdown(&dev, true);
    max30102_err_t e = max30102_shutdown(&dev, false);
    ASSERT_EQ(e, MAX30102_OK);
    ASSERT_FALSE(mock_regs[REG_MODE_CONFIG] & 0x80);
}

TEST(test_shutdown_preserves_mode)
{
    setup();
    max30102_set_mode(&dev, MAX30102_MODE_HR);
    max30102_shutdown(&dev, true);
    /* mode bits should still be HR (0x02) */
    ASSERT_EQ(mock_regs[REG_MODE_CONFIG] & 0x07, 0x02);
    max30102_shutdown(&dev, false);
    ASSERT_EQ(mock_regs[REG_MODE_CONFIG] & 0x07, 0x02);
}

TEST(test_shutdown_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    max30102_err_t e = max30102_shutdown(&dev, true);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    max30102_err_t e = max30102_reset(&dev);
    ASSERT_EQ(e, MAX30102_OK);
    /* reset writes bit 6 of MODE_CONFIG */
    ASSERT_TRUE(mock_regs[REG_MODE_CONFIG] & 0x40);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    max30102_err_t e = max30102_reset(&dev);
    ASSERT_EQ(e, MAX30102_ERR_I2C);
}

TEST(test_read_part_id)
{
    setup();
    uint8_t id = max30102_read_part_id(&dev);
    ASSERT_EQ(id, 0x15);
}

TEST(test_read_part_id_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    uint8_t id = max30102_read_part_id(&dev);
    ASSERT_EQ(id, 0);
}

TEST(test_init_missing_callbacks)
{
    mock_reset();
    max30102_t d;
    /* i2c with NULL write_reg */
    max30102_i2c_t bad_i2c = {
        .write_reg = NULL,
        .read_regs = mock_i2c_read_regs,
        .ctx       = NULL,
    };
    ASSERT_EQ(max30102_init(&d, &bad_i2c), MAX30102_ERR_PARAM);

    /* i2c with NULL read_regs */
    max30102_i2c_t bad_i2c2 = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = NULL,
        .ctx       = NULL,
    };
    ASSERT_EQ(max30102_init(&d, &bad_i2c2), MAX30102_ERR_PARAM);
}

TEST(test_init_i2c_fail_during_fifo_clear)
{
    /* ops 0-6: part ID, reset, FIFO_CONFIG, SPO2_CONFIG, LED1, LED2, MODE.
     * ops 7-9 are the three FIFO clears. */
    for (int op = 7; op < 10; op++)
    {
        mock_reset();
        max30102_t d;
        mock_fail_at = op;
        max30102_err_t e = max30102_init(&d, &mock_i2c);
        ASSERT_EQ(e, MAX30102_ERR_I2C);
    }
}

TEST(test_init_clears_fifo_after_mode_write)
{
    mock_reset();
    mock_regs[REG_FIFO_WR_PTR] = 7;
    mock_regs[REG_OVF_COUNTER] = 2;
    mock_regs[REG_FIFO_RD_PTR] = 3;

    max30102_t d;
    ASSERT_EQ(max30102_init(&d, &chip_i2c), MAX30102_OK);
    ASSERT_EQ(mock_regs[REG_FIFO_WR_PTR], 0);
    ASSERT_EQ(mock_regs[REG_OVF_COUNTER], 0);
    ASSERT_EQ(mock_regs[REG_FIFO_RD_PTR], 0);
}

TEST_MAIN("max30102")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_init_writes_defaults);
    RUN(test_init_missing_callbacks);
    RUN(test_init_i2c_fail_during_fifo_clear);
    RUN(test_init_clears_fifo_after_mode_write);
    RUN(test_set_mode_hr);
    RUN(test_set_mode_spo2);
    RUN(test_set_mode_clears_fifo);
    RUN(test_set_mode_writes_mode_before_clearing);
    RUN(test_set_mode_fifo_clear_i2c_fail);
    RUN(test_set_mode_invalid);
    RUN(test_set_mode_i2c_fail);
    RUN(test_set_led_current);
    RUN(test_set_led_current_zero);
    RUN(test_set_led_current_i2c_fail);
    RUN(test_set_sample_rate_tables_11_12);
    RUN(test_set_sample_rate_default_pw_limits);
    RUN(test_set_mode_spo2_refuses_rate_above_limit);
    RUN(test_set_mode_rate_check_i2c_fail);
    RUN(test_set_sample_rate_invalid);
    RUN(test_set_sample_rate_preserves_other_bits);
    RUN(test_set_sample_rate_i2c_fail);
    RUN(test_read_fifo_spo2_one_sample);
    RUN(test_read_fifo_spo2_adc_mask);
    RUN(test_read_fifo_empty);
    RUN(test_read_fifo_full_equal_pointers);
    RUN(test_read_fifo_full_max_count_limit);
    RUN(test_read_fifo_ovf_unused_bits_ignored);
    RUN(test_read_fifo_ovf_i2c_fail);
    RUN(test_read_fifo_hr_mode);
    RUN(test_read_fifo_max_count_limit);
    RUN(test_read_fifo_i2c_fail);
    RUN(test_read_fifo_wrap);
    RUN(test_read_temp);
    RUN(test_read_temp_negative);
    RUN(test_read_temp_i2c_fail);
    RUN(test_shutdown_enter);
    RUN(test_shutdown_exit);
    RUN(test_shutdown_preserves_mode);
    RUN(test_shutdown_i2c_fail);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_read_part_id);
    RUN(test_read_part_id_i2c_fail);
    RUN(test_init_missing_callbacks);
TEST_MAIN_END()
