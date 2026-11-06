// SPDX-License-Identifier: MIT
/*
 * test_hw_model_ccs811.c — driver against the hardware model
 *
 * Checks what hw_model_ccs811.h takes from ams CCS811 datasheet v1-00: the
 * change from boot to application mode needs the single byte 0xF4.
 */

#include "test_harness.h"
#include "hw_model_ccs811.h"
#include "../ccs811.h"

static ccs811_model_t model;

static const ccs811_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = hw_model_delay_ms,
    .ctx       = &model.hw,
    .write_raw = ccs811_model_write_raw,
};

static const ccs811_cfg_t cfg = {
    .addr = CCS811_ADDR_PRIMARY,
    .mode = CCS811_MODE_1SEC,
};

TEST(test_init_starts_the_application)
{
    ccs811_t d;

    ccs811_model_init(&model);
    ASSERT_FALSE(model.app_mode);
    ASSERT_EQ(ccs811_init(&d, &model_i2c, &cfg), CCS811_OK);
    ASSERT_TRUE(model.app_mode);
    ASSERT_EQ_HEX(model.hw.regs[CCS811_REG_STATUS] & 0x90u, 0x90u);
}

TEST(test_a_write_with_data_does_not_start_the_application)
{
    /* p24: "a write with no data is required" */
    static const uint8_t with_data[] = { CCS811_REG_APP_START, 0x00 };

    ccs811_model_init(&model);
    ASSERT_TRUE(ccs811_model_write_raw(&model.hw, CCS811_ADDR_PRIMARY,
                                       with_data, sizeof(with_data)));
    ASSERT_TRUE(hw_model_i2c_write_reg(&model.hw, CCS811_ADDR_PRIMARY,
                                       CCS811_REG_APP_START, 0x00));
    ASSERT_FALSE(model.app_mode);
}

TEST_MAIN("hw_model_ccs811")
    RUN(test_init_starts_the_application);
    RUN(test_a_write_with_data_does_not_start_the_application);
TEST_MAIN_END()
