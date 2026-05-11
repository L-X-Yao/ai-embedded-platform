// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_sh1106.h"
#include "sh1106.h"

static const sh1106_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static sh1106_t dev;
static uint8_t framebuf[SH1106_BUF_SIZE];

TEST(test_hw_init_sends_display_off_then_on)
{
    hw_model_reset();
    sh1106_err_t e = sh1106_init(&dev, &hw_i2c, SH1106_ADDR_A, framebuf);
    ASSERT_EQ(e, SH1106_OK);

    /* First command should be display off (0xAE) */
    ASSERT_TRUE(hw_cmd_count > 0);
    ASSERT_EQ_HEX(hw_cmd_log[0], 0xAE);

    /* Last command should be display on (0xAF) */
    ASSERT_EQ_HEX(hw_cmd_log[hw_cmd_count - 1], 0xAF);
}

TEST(test_hw_init_sets_mux_ratio_63)
{
    hw_model_reset();
    sh1106_init(&dev, &hw_i2c, SH1106_ADDR_A, framebuf);

    /* Find mux ratio command 0xA8 followed by 0x3F (63) */
    int found = 0;
    for (int i = 0; i < hw_cmd_count - 1; i++) {
        if (hw_cmd_log[i] == 0xA8) {
            ASSERT_EQ_HEX(hw_cmd_log[i + 1], 0x3F);
            found = 1;
            break;
        }
    }
    ASSERT_TRUE(found);
}

TEST(test_hw_display_sends_page_commands)
{
    hw_model_reset();
    sh1106_init(&dev, &hw_i2c, SH1106_ADDR_A, framebuf);
    hw_model_reset();

    sh1106_set_pixel(&dev, 0, 0, true);
    sh1106_display(&dev);

    /* Should have sent page address commands 0xB0..0xB7 */
    int page_cmds = 0;
    for (int i = 0; i < hw_cmd_count; i++) {
        if (hw_cmd_log[i] >= 0xB0 && hw_cmd_log[i] <= 0xB7)
            page_cmds++;
    }
    ASSERT_EQ(page_cmds, 8);

    /* Should have sent 128 * 8 = 1024 data bytes */
    ASSERT_EQ(hw_data_count, 1024);
}

TEST(test_hw_column_offset_is_2)
{
    hw_model_reset();
    sh1106_init(&dev, &hw_i2c, SH1106_ADDR_A, framebuf);
    hw_model_reset();

    sh1106_display(&dev);

    /* For each page, column low nibble should be 0x02 (offset 2, low nibble) */
    /* Column commands appear after each page address: B0, 02, 10, ... */
    int col_low_found = 0;
    for (int i = 0; i < hw_cmd_count - 1; i++) {
        if (hw_cmd_log[i] >= 0xB0 && hw_cmd_log[i] <= 0xB7) {
            /* Next command should be column low = 0x02 */
            ASSERT_EQ_HEX(hw_cmd_log[i + 1], 0x02);
            col_low_found++;
        }
    }
    ASSERT_EQ(col_low_found, 8);
}

TEST_MAIN("sh1106_hw_model")
    RUN(test_hw_init_sends_display_off_then_on);
    RUN(test_hw_init_sets_mux_ratio_63);
    RUN(test_hw_display_sends_page_commands);
    RUN(test_hw_column_offset_is_2);
TEST_MAIN_END()
