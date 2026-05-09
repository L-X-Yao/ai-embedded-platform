// SPDX-License-Identifier: MIT
/*
 * prop_ft6x36.c — property-based tests for ft6x36
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_ft6x36.c ../ft6x36.c -o prop_ft6x36
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "ft6x36.h"

static const ft6x36_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Set valid chip ID so init proceeds */
    mock_regs[FT6X36_REG_CHIP_ID] = FT6X36_CHIP_ID_6236;

    ft6x36_t dev;
    ft6x36_init(&dev, &prop_i2c);
    /* Must not crash */
}

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[FT6X36_REG_CHIP_ID] = FT6X36_CHIP_ID_6236;

    ft6x36_t dev;
    ft6x36_err_t err = ft6x36_init(&dev, &prop_i2c);
    if (err != FT6X36_OK) return;

    ft6x36_touch_t touch;
    ft6x36_read_touch(&dev, &touch);
    /* Must not crash with arbitrary register values */
}

PROPERTY(touch_coords_bounded, 5000)
{
    mock_i2c_reset();
    mock_regs[FT6X36_REG_CHIP_ID] = FT6X36_CHIP_ID_6236;

    ft6x36_t dev;
    ft6x36_err_t err = ft6x36_init(&dev, &prop_i2c);
    if (err != FT6X36_OK) return;

    /* Plant random touch data */
    uint8_t event = ARBI_UINT8(seed) % 3;
    uint16_t x = ARBI_UINT16(seed) & 0x0FFF;
    uint16_t y = ARBI_UINT16(seed) & 0x0FFF;

    mock_regs[FT6X36_REG_TD_STATUS] = 1;
    mock_regs[FT6X36_REG_P1_XH] = (uint8_t)((event << 6) | ((x >> 8) & 0x0Fu));
    mock_regs[FT6X36_REG_P1_XL] = (uint8_t)(x & 0xFFu);
    mock_regs[FT6X36_REG_P1_YH] = (uint8_t)((y >> 8) & 0x0Fu);
    mock_regs[FT6X36_REG_P1_YL] = (uint8_t)(y & 0xFFu);

    ft6x36_touch_t touch;
    err = ft6x36_read_touch(&dev, &touch);
    if (err != FT6X36_OK) return;

    PROP_ASSERT(touch.touch_count >= 0 && touch.touch_count <= 2);
    /* Coordinates are 12-bit: 0..4095 */
    PROP_ASSERT(touch.x1 <= 4095);
    PROP_ASSERT(touch.y1 <= 4095);
}

PROPERTY(touch_count_clamped, 5000)
{
    mock_i2c_reset();
    mock_regs[FT6X36_REG_CHIP_ID] = FT6X36_CHIP_ID_6236;

    ft6x36_t dev;
    ft6x36_err_t err = ft6x36_init(&dev, &prop_i2c);
    if (err != FT6X36_OK) return;

    /* Plant arbitrary touch count */
    mock_regs[FT6X36_REG_TD_STATUS] = ARBI_UINT8(seed);

    ft6x36_touch_t touch;
    err = ft6x36_read_touch(&dev, &touch);
    if (err != FT6X36_OK) return;

    /* Driver must clamp to 0..2 */
    PROP_ASSERT(touch.touch_count <= 2);
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    mock_regs[FT6X36_REG_CHIP_ID] = FT6X36_CHIP_ID_6236;

    ft6x36_t dev;
    ft6x36_init(&dev, &prop_i2c);
    mock_i2c_reset();
    mock_regs[FT6X36_REG_CHIP_ID] = FT6X36_CHIP_ID_6336;
    ft6x36_init(&dev, &prop_i2c);
    /* Must not crash; accepts both chip IDs */
}

PROPERTY(threshold_any_value, 5000)
{
    mock_i2c_reset();
    mock_regs[FT6X36_REG_CHIP_ID] = FT6X36_CHIP_ID_6236;

    ft6x36_t dev;
    ft6x36_err_t err = ft6x36_init(&dev, &prop_i2c);
    if (err != FT6X36_OK) return;

    uint8_t threshold = ARBI_UINT8(seed);
    err = ft6x36_set_threshold(&dev, threshold);
    PROP_ASSERT(err == FT6X36_OK);
    PROP_ASSERT_EQ(mock_regs[FT6X36_REG_TH_GROUP], threshold);
}

TEST_MAIN("ft6x36 properties")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(touch_coords_bounded);
    RUN(touch_count_clamped);
    RUN(double_init_safe);
    RUN(threshold_any_value);
TEST_MAIN_END()
