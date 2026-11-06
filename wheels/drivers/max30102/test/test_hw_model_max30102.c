// SPDX-License-Identifier: MIT
/*
 * test_hw_model_max30102.c -- tests using the MAX30102 hardware register model
 *
 * Runs the driver against hw_model_max30102.h. Expected values come from the
 * Maxim MAX30102 data sheet 19-7740 Rev 0, and page numbers are those of that
 * PDF.
 *
 * Tests: power-on and RESET state, FIFO pointers and the full FIFO, overflow
 * count, sample rate limits, the temperature conversion, read-clear flags.
 */

#include "test_harness.h"
#include "hw_model_max30102.h"
#include "../max30102.h"

static max30102_model_t model;

static const max30102_i2c_t model_i2c = {
    .write_reg = max30102_model_write_reg,
    .read_regs = max30102_model_read_regs,
    .delay_ms  = max30102_model_delay,
    .ctx       = &model,
};

static max30102_t dev;

static void setup(void)
{
    max30102_model_init(&model);
    memset(&dev, 0, sizeof(dev));
}

static void setup_and_init(void)
{
    setup();
    (void)max30102_init(&dev, &model_i2c);
}

/* A raw register write and read, as the host would do them */
static void reg_write(uint8_t reg, uint8_t val)
{
    (void)model_i2c.write_reg(model_i2c.ctx, MAX30102_ADDR, reg, val);
}

static uint8_t reg_read(uint8_t reg)
{
    uint8_t val = 0;
    (void)model_i2c.read_regs(model_i2c.ctx, MAX30102_ADDR, reg, &val, 1);
    return val;
}

static void inject(unsigned n, uint32_t first)
{
    for (unsigned i = 0; i < n; i++) {
        max30102_model_inject_sample(&model, first + i, 0x20000u + first + i);
    }
}

/* --- 1. Power-on state --- */

/* Register maps, p.11 and p.12: POR STATE of the registers the driver uses */
static const struct { uint8_t reg, val; } por_state[] = {
    {0x02, 0x00},   /* Interrupt Enable 1 */
    {0x03, 0x00},   /* Interrupt Enable 2 */
    {0x04, 0x00},   /* FIFO_WR_PTR */
    {0x05, 0x00},   /* OVF_COUNTER */
    {0x06, 0x00},   /* FIFO_RD_PTR */
    {0x08, 0x00},   /* FIFO Configuration */
    {0x09, 0x00},   /* Mode Configuration */
    {0x0A, 0x00},   /* SpO2 Configuration */
    {0x0C, 0x00},   /* LED1_PA */
    {0x0D, 0x00},   /* LED2_PA */
    {0x13, 0xFF},   /* Reserved */
    {0x17, 0xFF},
    {0x21, 0x00},   /* Die Temperature Config */
    {0x30, 0x00},   /* Proximity Interrupt Threshold */
    {0xFF, 0x15},   /* Part ID */
};

static void assert_por_state(void)
{
    for (size_t i = 0; i < sizeof(por_state) / sizeof(por_state[0]); i++) {
        ASSERT_EQ_HEX(model.hw.regs[por_state[i].reg], por_state[i].val);
    }
}

TEST(test_model_power_on_state)
{
    setup();
    assert_por_state();
}

/* p.12: "On power-up ... a power-ready interrupt is triggered". p.14: "The
 * interrupts are cleared whenever the interrupt status register is read" */
TEST(test_model_pwr_rdy_is_set_at_power_up_and_clears_on_read)
{
    setup();
    ASSERT_EQ(reg_read(0x00) & 0x01U, 0x01);
    ASSERT_EQ(reg_read(0x00), 0x00);
}

/* p.11 and p.12: part ID, revision and the die temperature are R */
TEST(test_model_writes_leave_read_only_registers)
{
    setup();
    reg_write(0xFF, 0x00);
    ASSERT_EQ(reg_read(0xFF), 0x15);
    reg_write(0x1F, 0x55);
    ASSERT_EQ(reg_read(0x1F), 0x00);
    reg_write(0x20, 0x05);
    ASSERT_EQ(reg_read(0x20), 0x00);
}

/* p.15: "The FIFO registers (0x04-0x07) can all be written and read" */
TEST(test_model_fifo_registers_take_writes)
{
    setup();
    reg_write(0x09, 0x03);
    reg_write(0x04, 0x05);
    reg_write(0x05, 0x02);
    reg_write(0x06, 0x03);
    reg_write(0x07, 0xA5);
    ASSERT_EQ(reg_read(0x04), 0x05);
    ASSERT_EQ(reg_read(0x05), 0x02);
    ASSERT_EQ(reg_read(0x06), 0x03);
    ASSERT_EQ(model.hw.regs[0x07], 0xA5);
}

/* A device at another address does not answer */
TEST(test_model_answers_only_at_0x57)
{
    uint8_t val = 0;
    setup();
    ASSERT_FALSE(model_i2c.read_regs(model_i2c.ctx, 0x56, 0xFF, &val, 1));
    ASSERT_FALSE(model_i2c.write_reg(model_i2c.ctx, 0x56, 0x09, 0x03));
    ASSERT_EQ(model.hw.regs[0x09], 0x00);
}

/* --- 2. Init and RESET --- */

TEST(test_init_ends_in_spo2_with_an_empty_fifo)
{
    setup();
    ASSERT_EQ(max30102_init(&dev, &model_i2c), MAX30102_OK);

    ASSERT_EQ(model.reset_count, 1);
    ASSERT_EQ(reg_read(0x09) & 0x07U, 0x03);        /* Table 4: SpO2 */
    ASSERT_EQ(model.fifo_unread, 0);
    ASSERT_EQ(reg_read(0x04), reg_read(0x06));
    ASSERT_EQ(reg_read(0x05), 0);
    /* Table 6: 100 sps, Table 7: 411 us */
    ASSERT_EQ((reg_read(0x0A) >> 2) & 0x07U, 0x01);
    ASSERT_EQ(reg_read(0x0A) & 0x03U, 0x03);
    /* Table 8: 6.4 mA typical at 0x1F */
    ASSERT_EQ(reg_read(0x0C), 0x1F);
    ASSERT_EQ(reg_read(0x0D), 0x1F);
}

/* p.19: "When the RESET bit is set to one, all configuration, threshold, and
 * data registers are reset to their power-on-state through a power-on reset.
 * The RESET bit is cleared automatically back to zero" */
TEST(test_reset_loses_the_configuration)
{
    setup_and_init();
    inject(3, 100);
    ASSERT_TRUE(reg_read(0x0A) != 0);

    ASSERT_EQ(max30102_reset(&dev), MAX30102_OK);

    ASSERT_EQ(model.reset_count, 2);
    assert_por_state();
    ASSERT_EQ(model.fifo_unread, 0);
}

/* p.19: "Setting the RESET bit does not trigger a PWR_RDY interrupt event." */
TEST(test_model_reset_sets_no_pwr_rdy)
{
    setup();
    ASSERT_EQ(reg_read(0x00) & 0x01U, 0x01);       /* power-up flag read away */
    reg_write(0x09, 0x40);
    ASSERT_EQ(reg_read(0x00), 0x00);
}

TEST(test_init_after_reset_configures_again)
{
    setup_and_init();
    ASSERT_EQ(max30102_reset(&dev), MAX30102_OK);
    ASSERT_EQ(max30102_init(&dev, &model_i2c), MAX30102_OK);
    ASSERT_EQ(reg_read(0x09) & 0x07U, 0x03);
    ASSERT_EQ(reg_read(0x0C), 0x1F);
}

/* --- 3. Reading samples --- */

/* p.16: 3 bytes per channel, and Figure 2 has RED before IR */
TEST(test_read_fifo_spo2_gives_red_then_ir)
{
    setup_and_init();
    max30102_model_inject_sample(&model, 0x12345u, 0x2ABCDu);

    max30102_sample_t s[4];
    uint8_t n = 0;
    ASSERT_EQ(max30102_read_fifo(&dev, s, 4, &n), MAX30102_OK);

    ASSERT_EQ(n, 1);
    ASSERT_EQ_HEX(s[0].red, 0x12345);
    ASSERT_EQ_HEX(s[0].ir, 0x2ABCD);
    ASSERT_EQ(model.fifo_empty_reads, 0);
}

/* p.19 Table 4: Heart Rate mode is "Red only", so a sample is 3 bytes */
TEST(test_read_fifo_hr_gives_red_only)
{
    setup_and_init();
    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);
    max30102_model_inject_sample(&model, 0x00111u, 0u);
    max30102_model_inject_sample(&model, 0x00222u, 0u);

    max30102_sample_t s[4];
    uint8_t n = 0;
    ASSERT_EQ(max30102_read_fifo(&dev, s, 4, &n), MAX30102_OK);

    ASSERT_EQ(n, 2);
    ASSERT_EQ_HEX(s[0].red, 0x00111);
    ASSERT_EQ_HEX(s[1].red, 0x00222);
    ASSERT_EQ(s[1].ir, 0);
}

/* p.17: NUM_AVAILABLE_SAMPLES = FIFO_WR_PTR - FIFO_RD_PTR, "(Note: pointer wrap
 * around should be taken into account)" */
TEST(test_read_fifo_follows_the_pointers_round_the_wrap)
{
    setup_and_init();
    max30102_sample_t s[32];
    uint8_t n = 0;
    uint32_t next = 1000;

    for (unsigned round = 0; round < 6U; round++) {
        inject(10, next);
        ASSERT_EQ(max30102_read_fifo(&dev, s, 32, &n), MAX30102_OK);
        ASSERT_EQ(n, 10);
        for (unsigned i = 0; i < 10U; i++) {
            ASSERT_EQ(s[i].red, next + i);
        }
        next += 10U;
    }
    ASSERT_EQ(model.fifo_rd, 60U % 32U);
}

TEST(test_read_fifo_leaves_what_does_not_fit)
{
    setup_and_init();
    max30102_sample_t s[4];
    uint8_t n = 0;
    inject(6, 200);

    ASSERT_EQ(max30102_read_fifo(&dev, s, 4, &n), MAX30102_OK);
    ASSERT_EQ(n, 4);
    ASSERT_EQ(model.fifo_unread, 2);

    ASSERT_EQ(max30102_read_fifo(&dev, s, 4, &n), MAX30102_OK);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(s[0].red, 204);
    ASSERT_EQ(model.fifo_empty_reads, 0);
}

TEST(test_read_fifo_on_an_empty_fifo_reads_no_data)
{
    setup_and_init();
    max30102_sample_t s[4];
    uint8_t n = 9;

    ASSERT_EQ(max30102_read_fifo(&dev, s, 4, &n), MAX30102_OK);

    ASSERT_EQ(n, 0);
    ASSERT_EQ(model.fifo_empty_reads, 0);
}

/* p.16: "The FIFO read pointer is automatically incremented after the first
 * byte of each sample is read." */
TEST(test_model_rd_ptr_moves_on_the_first_byte)
{
    setup_and_init();
    inject(2, 300);

    ASSERT_EQ(reg_read(0x07), 0x00);   /* byte 1 of a sample below 0x10000 */

    ASSERT_EQ(reg_read(0x06), 1);
}

/* p.15: "the address pointer does not increment, but the FIFO_RD_PTR does", so
 * one burst can carry several samples */
TEST(test_model_burst_read_walks_the_samples)
{
    setup_and_init();
    max30102_model_inject_sample(&model, 0x10203u, 0x10405u);
    max30102_model_inject_sample(&model, 0x10607u, 0x10809u);

    uint8_t buf[12];
    ASSERT_TRUE(model_i2c.read_regs(model_i2c.ctx, MAX30102_ADDR, 0x07, buf, 12));

    ASSERT_EQ(buf[2], 0x03);
    ASSERT_EQ(buf[5], 0x05);
    ASSERT_EQ(buf[8], 0x07);
    ASSERT_EQ(buf[11], 0x09);
    ASSERT_EQ(model.fifo_rd, 2);
}

/* p.16: "To reread a sample from the FIFO, decrement its value by one and read
 * the data register again." */
TEST(test_model_rd_ptr_write_rereads_a_sample)
{
    setup_and_init();
    max30102_sample_t s[1];
    uint8_t n = 0;
    max30102_model_inject_sample(&model, 0x00ABCu, 0x00DEFu);
    ASSERT_EQ(max30102_read_fifo(&dev, s, 1, &n), MAX30102_OK);

    reg_write(0x06, (uint8_t)(reg_read(0x06) - 1U));

    ASSERT_EQ(max30102_read_fifo(&dev, s, 1, &n), MAX30102_OK);
    ASSERT_EQ(n, 1);
    ASSERT_EQ_HEX(s[0].red, 0x00ABC);
}

/* p.14: FIFO_WR_PTR "can also be changed through the I2C interface when
 * MODE[2:0] is 010, 011, or 111." */
TEST(test_model_wr_ptr_takes_writes_in_three_modes_only)
{
    static const struct { uint8_t mode; bool takes; } cases[] = {
        {0, false}, {1, false}, {2, true},  {3, true},
        {4, false}, {5, false}, {6, false}, {7, true},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        setup();
        reg_write(0x09, cases[i].mode);
        reg_write(0x04, 0x07);
        ASSERT_EQ(reg_read(0x04), cases[i].takes ? 0x07 : 0x00);
    }
}

/* --- 4. A full FIFO --- */

/* p.14: "When the FIFO is full, samples are not pushed on to the FIFO, samples
 * are lost." That is the rollover-off case of p.18: "the FIFO is not updated
 * until FIFO_DATA is read or the WRITE/READ pointer positions are changed." */
static void rollover_off(void)
{
    reg_write(0x08, (uint8_t)(reg_read(0x08) & ~0x10U));
}

/* All 32 unread has equal pointers: p.18 calls FIFO_A_FULL 0x0 "all 32 FIFO
 * words have unread data" */
TEST(test_model_32_unread_samples_have_equal_pointers)
{
    setup_and_init();
    rollover_off();

    inject(32, 1);

    ASSERT_EQ(model.fifo_unread, 32);
    ASSERT_EQ(reg_read(0x04), reg_read(0x06));
    ASSERT_EQ(reg_read(0x05), 0);
}

TEST(test_model_full_fifo_counts_lost_samples)
{
    setup_and_init();
    rollover_off();
    inject(35, 1);

    ASSERT_EQ(reg_read(0x05), 3);
    ASSERT_EQ(model.fifo_unread, 32);
    ASSERT_EQ(reg_read(0x04), reg_read(0x06));
}

/* p.14: "It saturates at 0xF." */
TEST(test_model_ovf_counter_saturates_at_0xf)
{
    setup_and_init();
    rollover_off();
    inject(32 + 40, 1);

    ASSERT_EQ(reg_read(0x05), 0x0F);
}

/* p.14: "When a complete sample is 'popped' ... OVF_COUNTER is reset to zero." */
TEST(test_model_ovf_counter_clears_when_a_sample_is_popped)
{
    setup_and_init();
    rollover_off();
    inject(34, 1);
    ASSERT_EQ(reg_read(0x05), 2);

    uint8_t buf[6];
    ASSERT_TRUE(model_i2c.read_regs(model_i2c.ctx, MAX30102_ADDR, 0x07, buf, 6));

    ASSERT_EQ(reg_read(0x05), 0);
    ASSERT_EQ(model.fifo_unread, 31);
}

/* With pointers equal and samples lost, all 32 are there to read, oldest first */
TEST(test_read_fifo_gets_all_32_of_an_overflowed_fifo)
{
    setup_and_init();
    rollover_off();
    inject(33, 500);

    max30102_sample_t s[32];
    uint8_t n = 0;
    ASSERT_EQ(max30102_read_fifo(&dev, s, 32, &n), MAX30102_OK);

    ASSERT_EQ(n, 32);
    ASSERT_EQ(s[0].red, 500);
    ASSERT_EQ(s[31].red, 531);          /* number 33 was lost */
    ASSERT_EQ(model.fifo_unread, 0);
    ASSERT_EQ(model.fifo_empty_reads, 0);
    ASSERT_EQ(reg_read(0x05), 0);
}

/* p.18: with FIFO_ROLLOVER_EN "the FIFO address rolls over to zero and the FIFO
 * continues to fill with new data". The manual stops there, so this checks
 * nothing about FIFO_RD_PTR or about what the driver reads next. */
TEST(test_model_rollover_keeps_filling)
{
    setup_and_init();
    ASSERT_TRUE(reg_read(0x08) & 0x10U);             /* init turns it on */

    inject(33, 0x700);

    ASSERT_EQ(reg_read(0x04), 1);
    /* Sample 33 sits in slot 0, in the place of sample 1 */
    ASSERT_EQ(model.fifo[1], 0x07);
    ASSERT_EQ(model.fifo[2], 0x20);
}

/* --- 5. Changing mode --- */

/* p.19: "Changing modes does not change any other setting, nor does it erase
 * any previously stored data inside the data registers." */
TEST(test_model_mode_write_keeps_the_fifo)
{
    setup_and_init();
    inject(3, 1);

    reg_write(0x09, 0x02);

    ASSERT_EQ(model.fifo_unread, 3);
    ASSERT_EQ(reg_read(0x04), 3);
    ASSERT_EQ(reg_read(0x0C), 0x1F);
}

/* p.16: "The FIFO write/read pointers should be cleared (back to 0x00) upon
 * entering SpO2 mode or HR mode, so that there is no old data represented in
 * the FIFO." */
TEST(test_set_mode_empties_the_fifo_of_the_old_sample_size)
{
    setup_and_init();
    inject(5, 1);

    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);

    ASSERT_EQ(model.fifo_unread, 0);
    ASSERT_EQ(reg_read(0x04), 0);
    ASSERT_EQ(reg_read(0x06), 0);
    ASSERT_EQ(reg_read(0x05), 0);

    max30102_model_inject_sample(&model, 0x00333u, 0u);
    max30102_sample_t s[4];
    uint8_t n = 0;
    ASSERT_EQ(max30102_read_fifo(&dev, s, 4, &n), MAX30102_OK);
    ASSERT_EQ(n, 1);
    ASSERT_EQ_HEX(s[0].red, 0x00333);
}

TEST(test_set_mode_keeps_shutdown)
{
    setup_and_init();
    ASSERT_EQ(max30102_shutdown(&dev, true), MAX30102_OK);
    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);
    ASSERT_EQ(reg_read(0x09), 0x82);
}

/* --- 6. Sample rate --- */

/* Table 11, p.24: rows 50 to 3200 sps, columns 69, 118, 215 and 411 us */
static const uint8_t spo2_allowed[8][4] = {
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0},
};

/* Table 12, p.24 */
static const uint8_t hr_allowed[8][4] = {
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
    {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 0}, {1, 0, 0, 0},
};

/* The highest allowed code at or below sr for this pulse width */
static uint8_t highest_allowed(const uint8_t table[8][4], uint8_t sr, uint8_t pw)
{
    while (sr > 0U && !table[sr][pw]) sr--;
    return sr;
}

/* p.20: "If the user selects a sample rate that is too high for the selected
 * LED_PW setting, the highest possible sample rate is programmed instead into
 * the register." */
TEST(test_model_programs_the_highest_rate_the_pulse_width_allows)
{
    for (uint8_t mode = 2; mode <= 3U; mode++) {
        const uint8_t (*table)[4] = (mode == 3U) ? spo2_allowed : hr_allowed;
        setup();
        reg_write(0x09, mode);
        for (uint8_t pw = 0; pw < 4U; pw++) {
            for (uint8_t sr = 0; sr < 8U; sr++) {
                reg_write(0x0A, (uint8_t)((sr << 2) | pw));
                ASSERT_EQ((reg_read(0x0A) >> 2) & 0x07U,
                          highest_allowed(table, sr, pw));
                ASSERT_EQ(reg_read(0x0A) & 0x03U, pw);
            }
        }
    }
}

/* Init sets 411 us: Table 11 stops at 400 sps */
TEST(test_set_sample_rate_stops_at_the_table_limit_in_spo2)
{
    setup_and_init();

    ASSERT_EQ(max30102_set_sample_rate(&dev, 400), MAX30102_OK);
    ASSERT_EQ((reg_read(0x0A) >> 2) & 0x07U, 3);    /* Table 6: 011 */

    ASSERT_EQ(max30102_set_sample_rate(&dev, 800), MAX30102_ERR_PARAM);
    ASSERT_EQ((reg_read(0x0A) >> 2) & 0x07U, 3);
}

/* Table 12 stops at 1000 sps */
TEST(test_set_sample_rate_stops_at_the_table_limit_in_hr)
{
    setup_and_init();
    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);

    ASSERT_EQ(max30102_set_sample_rate(&dev, 1000), MAX30102_OK);
    ASSERT_EQ((reg_read(0x0A) >> 2) & 0x07U, 5);    /* Table 6: 101 */

    ASSERT_EQ(max30102_set_sample_rate(&dev, 1600), MAX30102_ERR_PARAM);
}

/* 1000 sps is allowed in HR mode only at 411 us, so SpO2 mode must not start */
TEST(test_set_mode_refuses_a_rate_the_new_mode_cannot_reach)
{
    setup_and_init();
    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_HR), MAX30102_OK);
    ASSERT_EQ(max30102_set_sample_rate(&dev, 1000), MAX30102_OK);

    ASSERT_EQ(max30102_set_mode(&dev, MAX30102_MODE_SPO2), MAX30102_ERR_PARAM);

    ASSERT_EQ(reg_read(0x09) & 0x07U, 0x02);
}

/* --- 7. Temperature --- */

/* p.5: "Temperature ADC Acquisition Time TT TA = +25C 29 ms". p.23: TEMP_EN
 * "clears automatically back to zero at the conclusion of the temperature
 * reading". p.12: DIE_TEMP_RDY sets "When an internal die temperature conversion
 * is finished", and reading "the TFRAC register" clears it. */
TEST(test_model_temperature_takes_29_ms)
{
    setup();
    max30102_model_set_temperature(&model, 30.0f);
    reg_write(0x21, 0x01);

    model_i2c.delay_ms(model_i2c.ctx, 28);
    ASSERT_EQ(reg_read(0x21), 0x01);
    ASSERT_EQ(reg_read(0x1F), 0x00);

    model_i2c.delay_ms(model_i2c.ctx, 1);
    ASSERT_EQ(reg_read(0x21), 0x00);
    ASSERT_EQ(reg_read(0x1F), 30);
    ASSERT_EQ(model.hw.regs[0x01] & 0x02U, 0x02);

    ASSERT_EQ(reg_read(0x20), 0x00);
    ASSERT_EQ(model.hw.regs[0x01] & 0x02U, 0x00);
}

/* p.12: DIE_TEMP_RDY "is cleared by reading either the Interrupt Status 2
 * register (0x01) or the TFRAC register (0x20)" */
TEST(test_model_die_temp_rdy_clears_on_status_read)
{
    setup();
    reg_write(0x21, 0x01);
    model_i2c.delay_ms(model_i2c.ctx, 29);

    ASSERT_EQ(reg_read(0x01) & 0x02U, 0x02);
    ASSERT_EQ(reg_read(0x01), 0x00);
}

TEST(test_read_temp_gets_the_new_reading)
{
    setup_and_init();
    float t = 0.0f;
    max30102_model_set_temperature(&model, 36.5f);

    ASSERT_EQ(max30102_read_temp(&dev, &t), MAX30102_OK);

    ASSERT_NEAR(t, 36.5f, 0.001f);
    ASSERT_EQ(model.temp_trigger_count, 1);
}

/* p.23: the fraction "still adds as a positive fractional value (e.g., -128C +
 * 0.5C = -127.5C)" */
TEST(test_read_temp_below_zero)
{
    setup_and_init();
    float t = 0.0f;
    max30102_model_set_temperature(&model, -127.5f);

    ASSERT_EQ(max30102_read_temp(&dev, &t), MAX30102_OK);

    ASSERT_EQ(model.hw.regs[0x1F], 0x80);
    ASSERT_EQ(model.hw.regs[0x20], 0x08);
    ASSERT_NEAR(t, -127.5f, 0.001f);
}

/* --- 8. Shutdown --- */

/* p.19: "While in power-save mode, all registers retain their values, and
 * write/read operations function as normal. All interrupts are cleared to zero
 * in this mode." */
TEST(test_shutdown_keeps_the_registers_and_clears_the_flags)
{
    setup_and_init();
    model.hw.regs[0x00] = 0x41U;

    ASSERT_EQ(max30102_shutdown(&dev, true), MAX30102_OK);

    ASSERT_EQ(reg_read(0x09), 0x83);
    ASSERT_EQ(model.hw.regs[0x00], 0x00);
    ASSERT_EQ(reg_read(0x0C), 0x1F);

    ASSERT_EQ(max30102_shutdown(&dev, false), MAX30102_OK);
    ASSERT_EQ(reg_read(0x09), 0x03);
}

TEST_MAIN("max30102 hardware model")
    RUN(test_model_power_on_state);
    RUN(test_model_pwr_rdy_is_set_at_power_up_and_clears_on_read);
    RUN(test_model_writes_leave_read_only_registers);
    RUN(test_model_fifo_registers_take_writes);
    RUN(test_model_answers_only_at_0x57);
    RUN(test_init_ends_in_spo2_with_an_empty_fifo);
    RUN(test_reset_loses_the_configuration);
    RUN(test_model_reset_sets_no_pwr_rdy);
    RUN(test_init_after_reset_configures_again);
    RUN(test_read_fifo_spo2_gives_red_then_ir);
    RUN(test_read_fifo_hr_gives_red_only);
    RUN(test_read_fifo_follows_the_pointers_round_the_wrap);
    RUN(test_read_fifo_leaves_what_does_not_fit);
    RUN(test_read_fifo_on_an_empty_fifo_reads_no_data);
    RUN(test_model_rd_ptr_moves_on_the_first_byte);
    RUN(test_model_burst_read_walks_the_samples);
    RUN(test_model_rd_ptr_write_rereads_a_sample);
    RUN(test_model_wr_ptr_takes_writes_in_three_modes_only);
    RUN(test_model_32_unread_samples_have_equal_pointers);
    RUN(test_model_full_fifo_counts_lost_samples);
    RUN(test_model_ovf_counter_saturates_at_0xf);
    RUN(test_model_ovf_counter_clears_when_a_sample_is_popped);
    RUN(test_read_fifo_gets_all_32_of_an_overflowed_fifo);
    RUN(test_model_rollover_keeps_filling);
    RUN(test_model_mode_write_keeps_the_fifo);
    RUN(test_set_mode_empties_the_fifo_of_the_old_sample_size);
    RUN(test_set_mode_keeps_shutdown);
    RUN(test_model_programs_the_highest_rate_the_pulse_width_allows);
    RUN(test_set_sample_rate_stops_at_the_table_limit_in_spo2);
    RUN(test_set_sample_rate_stops_at_the_table_limit_in_hr);
    RUN(test_set_mode_refuses_a_rate_the_new_mode_cannot_reach);
    RUN(test_model_temperature_takes_29_ms);
    RUN(test_model_die_temp_rdy_clears_on_status_read);
    RUN(test_read_temp_gets_the_new_reading);
    RUN(test_read_temp_below_zero);
    RUN(test_shutdown_keeps_the_registers_and_clears_the_flags);
TEST_MAIN_END()
