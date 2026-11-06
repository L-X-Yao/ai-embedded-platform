// SPDX-License-Identifier: MIT
/*
 * test_hw_model_apds9960.c — tests using hardware register model
 *
 * Validates APDS-9960 driver against a datasheet-accurate register model.
 * The model enforces read-only chip ID (0xAB), RGBC data, proximity,
 * gesture FIFO behavior, and enable register control. Data appears only
 * after a cycle of an enabled engine, which tests run with
 * apds9960_model_cycle().
 */

#include "test_harness.h"
#include "hw_model_apds9960.h"
#include "../apds9960.h"
#include <math.h>

static apds9960_model_t model;

static const apds9960_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = apds9960_model_i2c_read_regs,  /* custom for gesture FIFO */
    .delay_ms  = NULL,
    .ctx       = &model,
};

static void setup(void)
{
    apds9960_model_init(&model);
}

static uint8_t reg_read(uint8_t reg)
{
    uint8_t val = 0;
    model_i2c.read_regs(&model, APDS9960_DEV_ADDR, reg, &val, 1);
    return val;
}

static void reg_write(uint8_t reg, uint8_t val)
{
    model_i2c.write_reg(&model, APDS9960_DEV_ADDR, reg, val);
}

/* PON, GEN and GMODE: the gesture state machine is running */
static void start_gesture_engine(void)
{
    reg_write(APDS_REG_ENABLE, 0x41);
    reg_write(APDS_REG_GCONF4, 0x01);
}

/* Datasets 0..n-1, each four different bytes that tell it from the others */
static void load_datasets(unsigned first, unsigned n)
{
    for (unsigned i = first; i < first + n; i++) {
        uint8_t set[4] = { (uint8_t)i, (uint8_t)(i + 64),
                           (uint8_t)(i + 128), (uint8_t)(i + 192) };
        apds9960_model_set_gesture(&model, 1, set);
    }
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    apds9960_t dev;
    apds9960_err_t err = apds9960_init(&dev, &model_i2c);
    ASSERT_EQ(err, APDS9960_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[APDS_REG_ID] = 0x00;
    hw_model_reset(&model.hw);

    apds9960_t dev;
    apds9960_err_t err = apds9960_init(&dev, &model_i2c);
    ASSERT_EQ(err, APDS9960_ERR_ID);
}

TEST(test_init_powers_on)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    /* Enable register should have PON bit set */
    ASSERT_TRUE(model.hw.regs[APDS_REG_ENABLE] & 0x01u);
}

TEST(test_init_sets_atime)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    ASSERT_EQ(model.hw.regs[APDS_REG_ATIME], 0xFFu);
}

TEST(test_init_sets_ppulse)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    /* p23: PPLEN 1 in bits 7:6 is 8 us, PPULSE 7 in bits 5:0 is 8 pulses */
    ASSERT_EQ_HEX(model.hw.regs[APDS_REG_PPULSE], (1u << 6) | 7u);
}

/* --- proximity tests --------------------------------------------------- */

TEST(test_enable_proximity)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_proximity(&dev);
    /* Enable should have PEN + PON */
    ASSERT_TRUE(model.hw.regs[APDS_REG_ENABLE] & 0x04u);
    ASSERT_TRUE(model.hw.regs[APDS_REG_ENABLE] & 0x01u);
}

TEST(test_read_proximity)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_proximity(&dev);
    apds9960_model_cycle(&model);

    uint8_t prox = 0;
    apds9960_err_t err = apds9960_read_proximity(&dev, &prox);
    ASSERT_EQ(err, APDS9960_OK);
    ASSERT_EQ(prox, 128);
}

/* --- color tests ------------------------------------------------------- */

TEST(test_read_color)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_color(&dev);
    apds9960_model_cycle(&model);

    apds9960_color_t color;
    apds9960_err_t err = apds9960_read_color(&dev, &color);
    ASSERT_EQ(err, APDS9960_OK);
    ASSERT_EQ(color.c, 1200);
    ASSERT_EQ(color.r, 400);
    ASSERT_EQ(color.g, 500);
    ASSERT_EQ(color.b, 300);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, APDS_REG_ID, 0x00);
    ASSERT_EQ(model.hw.regs[APDS_REG_ID], 0xABu);
}

TEST(test_color_data_readonly)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    uint8_t orig = model.hw.regs[APDS_REG_CDATAL];
    hw_model__write_reg(&model.hw, APDS_REG_CDATAL, 0xFF);
    ASSERT_EQ(model.hw.regs[APDS_REG_CDATAL], orig);
}

TEST(test_disable_all)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_proximity(&dev);
    apds9960_disable_all(&dev);
    ASSERT_EQ(model.hw.regs[APDS_REG_ENABLE], 0x00u);
}

TEST(test_gesture_detection)
{
    setup();
    apds9960_t dev;
    apds9960_init(&dev, &model_i2c);
    apds9960_enable_gesture(&dev);

    /* Load gesture FIFO: U=200 D=50 L=100 R=100 -> UP gesture */
    uint8_t fifo_data[4] = { 200, 50, 100, 100 };
    apds9960_model_set_gesture(&model, 1, fifo_data);

    apds9960_gesture_t gest = APDS9960_GESTURE_NONE;
    apds9960_err_t err = apds9960_read_gesture(&dev, &gest);
    ASSERT_EQ(err, APDS9960_OK);
    ASSERT_EQ(gest, APDS9960_GESTURE_UP);
}

/* --- the model against the manual ------------------------------------- */

/* Register Set, p19 and p20 */
TEST(test_reset_values)
{
    static const struct { uint8_t reg, val; } table[] = {
        { 0x80, 0x00 }, { 0x81, 0xFF }, { 0x83, 0xFF }, { 0x8C, 0x00 },
        { 0x8D, 0x40 }, { 0x8E, 0x40 }, { 0x8F, 0x00 }, { 0x90, 0x01 },
        { 0x92, 0xAB }, { 0x93, 0x00 }, { 0x9C, 0x00 }, { 0xA2, 0x00 },
        { 0xA6, 0x40 }, { 0xAB, 0x00 }, { 0xAE, 0x00 }, { 0xAF, 0x00 },
    };
    setup();
    for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        ASSERT_EQ_HEX(reg_read(table[i].reg), table[i].val);
    }
}

/* AEN "activates ALS function", PEN the proximity function (p20): with PON
 * alone there is nothing to fill the data registers */
TEST(test_no_data_without_an_engine)
{
    setup();
    apds9960_t dev;
    ASSERT_EQ(apds9960_init(&dev, &model_i2c), APDS9960_OK);
    apds9960_model_cycle(&model);
    ASSERT_EQ_HEX(reg_read(APDS_REG_STATUS) & 0x03, 0x00);
    ASSERT_EQ_HEX(reg_read(APDS_REG_PDATA), 0x00);
    ASSERT_EQ_HEX(reg_read(APDS_REG_CDATAL), 0x00);
}

TEST(test_only_the_enabled_engine_converts)
{
    setup();
    reg_write(APDS_REG_ENABLE, 0x05);
    apds9960_model_cycle(&model);
    ASSERT_EQ_HEX(reg_read(APDS_REG_STATUS) & 0x03, 0x02);
    ASSERT_EQ_HEX(reg_read(APDS_REG_CDATAL), 0x00);
    ASSERT_EQ(reg_read(APDS_REG_PDATA), 128);
}

/* PON off is the Sleep state (p9) */
TEST(test_sleep_stops_the_engines)
{
    setup();
    reg_write(APDS_REG_ENABLE, 0x06);
    apds9960_model_cycle(&model);
    ASSERT_EQ_HEX(reg_read(APDS_REG_STATUS) & 0x03, 0x00);
    ASSERT_EQ_HEX(reg_read(APDS_REG_PDATA), 0x00);
}

/* PVALID: "a proximity cycle has completed since PEN was asserted or since
 * PDATA was last read. A read of PDATA automatically clears PVALID", p25 */
TEST(test_pvalid_follows_the_cycle)
{
    setup();
    apds9960_t dev;
    ASSERT_EQ(apds9960_init(&dev, &model_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_proximity(&dev), APDS9960_OK);
    ASSERT_EQ(reg_read(APDS_REG_STATUS) & 0x02, 0);

    apds9960_model_cycle(&model);
    ASSERT_TRUE(reg_read(APDS_REG_STATUS) & 0x02);

    uint8_t prox = 0;
    ASSERT_EQ(apds9960_read_proximity(&dev, &prox), APDS9960_OK);
    ASSERT_EQ(prox, 128);
    ASSERT_EQ(reg_read(APDS_REG_STATUS) & 0x02, 0);

    /* Asserting PEN again starts the count over */
    apds9960_model_cycle(&model);
    reg_write(APDS_REG_ENABLE, 0x01);
    reg_write(APDS_REG_ENABLE, 0x05);
    ASSERT_EQ(reg_read(APDS_REG_STATUS) & 0x02, 0);
}

/* AVALID: "a read from any of the ALS/Color data registers", p25 */
TEST(test_avalid_follows_the_cycle)
{
    setup();
    apds9960_t dev;
    ASSERT_EQ(apds9960_init(&dev, &model_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_color(&dev), APDS9960_OK);
    ASSERT_EQ(reg_read(APDS_REG_STATUS) & 0x01, 0);

    apds9960_model_cycle(&model);
    ASSERT_TRUE(reg_read(APDS_REG_STATUS) & 0x01);

    apds9960_color_t color;
    ASSERT_EQ(apds9960_read_color(&dev, &color), APDS9960_OK);
    ASSERT_EQ(color.c, 1200);
    ASSERT_EQ(reg_read(APDS_REG_STATUS) & 0x01, 0);
}

/* "Exit of the gesture engine will not occur until GMODE is reset to zero",
 * p16, so proximity and color wait for it */
TEST(test_gesture_engine_holds_the_others)
{
    setup();
    reg_write(APDS_REG_ENABLE, 0x47);
    reg_write(APDS_REG_GCONF4, 0x01);
    apds9960_model_cycle(&model);
    ASSERT_EQ_HEX(reg_read(APDS_REG_STATUS) & 0x03, 0x00);

    reg_write(APDS_REG_GCONF4, 0x00);
    apds9960_model_cycle(&model);
    ASSERT_EQ_HEX(reg_read(APDS_REG_STATUS) & 0x03, 0x03);
}

/* A write of ENABLE is no event for the FIFO */
TEST(test_enable_write_leaves_the_fifo)
{
    setup();
    start_gesture_engine();
    load_datasets(0, 2);
    uint8_t buf[4];
    model_i2c.read_regs(&model, APDS9960_DEV_ADDR, APDS_REG_GFIFO_U, buf, 4);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 1);

    reg_write(APDS_REG_ENABLE, 0x41);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 1);
    model_i2c.read_regs(&model, APDS9960_DEV_ADDR, APDS_REG_GFIFO_U, buf, 4);
    ASSERT_EQ(buf[0], 1);
}

/* The FIFO fills only in the gesture engine, entered with GEN and GMODE
 * both set, p16 */
TEST(test_fifo_needs_the_gesture_engine)
{
    setup();
    reg_write(APDS_REG_ENABLE, 0x41);
    load_datasets(0, 1);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 0);
    reg_write(APDS_REG_GCONF4, 0x01);
    load_datasets(0, 1);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 1);
}

/* "Thirty-two separate datasets are stored", and past that "new datasets
 * will not replace existing datasets; instead an overflow flag will be set
 * and new data will be lost", p33 */
TEST(test_fifo_holds_32_datasets)
{
    setup();
    start_gesture_engine();
    load_datasets(0, 32);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 32);
    ASSERT_EQ(reg_read(APDS_REG_GSTATUS) & 0x02, 0);

    load_datasets(32, 1);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 32);
    ASSERT_TRUE(reg_read(APDS_REG_GSTATUS) & 0x02);

    static uint8_t page[128];
    ASSERT_TRUE(model_i2c.read_regs(&model, APDS9960_DEV_ADDR, APDS_REG_GFIFO_U,
                                    page, 128));
    ASSERT_EQ(page[0], 0);
    ASSERT_EQ(page[124], 31);
    ASSERT_EQ(page[127], (uint8_t)(31 + 192));
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 0);
}

/* "if GFLVL = 2, then the host should initiate a read at address 0xFC, and
 * sequentially read all eight bytes", p33 */
TEST(test_fifo_page_read)
{
    setup();
    start_gesture_engine();
    load_datasets(5, 3);
    uint8_t page[8];
    ASSERT_TRUE(model_i2c.read_regs(&model, APDS9960_DEV_ADDR, APDS_REG_GFIFO_U,
                                    page, 8));
    ASSERT_EQ(page[0], 5);
    ASSERT_EQ(page[1], 5 + 64);
    ASSERT_EQ(page[2], 5 + 128);
    ASSERT_EQ(page[3], 5 + 192);
    ASSERT_EQ(page[4], 6);
    ASSERT_EQ(page[7], 6 + 192);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 1);
}

/* The level moves "when address 0xFF is read" in single byte transactions,
 * p33 */
TEST(test_fifo_steps_at_0xff)
{
    setup();
    start_gesture_engine();
    load_datasets(1, 2);
    ASSERT_EQ(reg_read(0xFC), 1);
    ASSERT_EQ(reg_read(0xFD), 1 + 64);
    ASSERT_EQ(reg_read(0xFE), 1 + 128);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 2);
    ASSERT_EQ(reg_read(0xFF), 1 + 192);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 1);
    ASSERT_EQ(reg_read(0xFC), 2);
}

/* "If FIFO continues to be accessed after GFLVL register is zero, dataset
 * will be read as zero values", p33 */
TEST(test_fifo_reads_zero_when_empty)
{
    setup();
    start_gesture_engine();
    load_datasets(7, 1);
    uint8_t page[8];
    ASSERT_TRUE(model_i2c.read_regs(&model, APDS9960_DEV_ADDR, APDS_REG_GFIFO_U,
                                    page, 8));
    ASSERT_EQ(page[0], 7);
    ASSERT_EQ(page[4], 0);
    ASSERT_EQ(page[7], 0);
}

/* GVALID rises when GFLVL reaches GFIFOTH (p17, p28), and "is reset when
 * GMODE = 0 and the GFLVL=0" (p33) */
TEST(test_gvalid_follows_the_fifo_level)
{
    setup();
    start_gesture_engine();
    reg_write(APDS_REG_GCONF1, 0x40);  /* GFIFOTH 1: four datasets */
    load_datasets(0, 3);
    ASSERT_EQ(reg_read(APDS_REG_GSTATUS) & 0x01, 0);
    load_datasets(3, 1);
    ASSERT_TRUE(reg_read(APDS_REG_GSTATUS) & 0x01);

    uint8_t page[16];
    reg_write(APDS_REG_GCONF4, 0x00);
    model_i2c.read_regs(&model, APDS9960_DEV_ADDR, APDS_REG_GFIFO_U, page, 16);
    ASSERT_EQ(reg_read(APDS_REG_GSTATUS) & 0x01, 0);
}

/* A long swipe: the driver has to take everything GFLVL announces */
TEST(test_driver_drains_a_long_swipe)
{
    setup();
    apds9960_t dev;
    ASSERT_EQ(apds9960_init(&dev, &model_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);
    load_datasets(0, 20);

    apds9960_gesture_t gest = APDS9960_GESTURE_NONE;
    ASSERT_EQ(apds9960_read_gesture(&dev, &gest), APDS9960_OK);
    ASSERT_EQ(reg_read(APDS_REG_GFLVL), 0);
}

TEST_MAIN("apds9960 hardware model")
    RUN(test_reset_values);
    RUN(test_no_data_without_an_engine);
    RUN(test_only_the_enabled_engine_converts);
    RUN(test_sleep_stops_the_engines);
    RUN(test_pvalid_follows_the_cycle);
    RUN(test_avalid_follows_the_cycle);
    RUN(test_gesture_engine_holds_the_others);
    RUN(test_enable_write_leaves_the_fifo);
    RUN(test_fifo_needs_the_gesture_engine);
    RUN(test_fifo_holds_32_datasets);
    RUN(test_fifo_page_read);
    RUN(test_fifo_steps_at_0xff);
    RUN(test_fifo_reads_zero_when_empty);
    RUN(test_gvalid_follows_the_fifo_level);
    RUN(test_driver_drains_a_long_swipe);
    RUN(test_init_ok);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_powers_on);
    RUN(test_init_sets_atime);
    RUN(test_init_sets_ppulse);
    RUN(test_enable_proximity);
    RUN(test_read_proximity);
    RUN(test_read_color);
    RUN(test_chip_id_readonly);
    RUN(test_color_data_readonly);
    RUN(test_disable_all);
    RUN(test_gesture_detection);
TEST_MAIN_END()
