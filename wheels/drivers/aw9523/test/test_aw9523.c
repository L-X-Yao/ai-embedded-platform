// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "aw9523.h"

/* ---- register addresses (mirror driver header for test readability) ---- */
#define REG_INPUT_P0    0x00
#define REG_INPUT_P1    0x01
#define REG_OUTPUT_P0   0x02
#define REG_OUTPUT_P1   0x03
#define REG_CONFIG_P0   0x04
#define REG_CONFIG_P1   0x05
#define REG_INT_P0      0x06
#define REG_INT_P1      0x07
#define REG_ID          0x10
#define REG_CTL         0x11
#define REG_LED_MODE_P0 0x12
#define REG_LED_MODE_P1 0x13
#define REG_DIM_BASE    0x20

#define TEST_ADDR       0x58

static const aw9523_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static aw9523_t dev;

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[REG_ID] = 0x23;  /* AW9523B chip ID */
}

static aw9523_err_t init_ok(void)
{
    mock_reset();
    return aw9523_init(&dev, &mock_i2c, TEST_ADDR);
}

/* ========== init tests ========== */

TEST(test_init_ok)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* After init: all pins input, GPIO mode, interrupts disabled, outputs cleared */
    ASSERT_EQ(mock_regs[REG_CONFIG_P0], 0xFF);
    ASSERT_EQ(mock_regs[REG_CONFIG_P1], 0xFF);
    ASSERT_EQ(mock_regs[REG_LED_MODE_P0], 0xFF);
    ASSERT_EQ(mock_regs[REG_LED_MODE_P1], 0xFF);
    ASSERT_EQ(mock_regs[REG_INT_P0], 0xFF);
    ASSERT_EQ(mock_regs[REG_INT_P1], 0xFF);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P0], 0x00);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P1], 0x00);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[REG_ID] = 0x99;  /* wrong chip ID */
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, TEST_ADDR), AW9523_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;  /* fail on first I2C op (ID read) */
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, TEST_ADDR), AW9523_ERR_I2C);
}

TEST(test_init_i2c_fail_ctl_write)
{
    mock_reset();
    /* ID read succeeds (op 0), CTL write fails (op 1) */
    mock_fail_at = 1;
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, TEST_ADDR), AW9523_ERR_I2C);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(aw9523_init(NULL, &mock_i2c, TEST_ADDR), AW9523_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    ASSERT_EQ(aw9523_init(&dev, NULL, TEST_ADDR), AW9523_ERR_PARAM);
}

TEST(test_init_null_write_reg)
{
    aw9523_i2c_t bad_i2c = mock_i2c;
    bad_i2c.write_reg = NULL;
    ASSERT_EQ(aw9523_init(&dev, &bad_i2c, TEST_ADDR), AW9523_ERR_PARAM);
}

TEST(test_init_null_read_regs)
{
    aw9523_i2c_t bad_i2c = mock_i2c;
    bad_i2c.read_regs = NULL;
    ASSERT_EQ(aw9523_init(&dev, &bad_i2c, TEST_ADDR), AW9523_ERR_PARAM);
}

TEST(test_init_addr_0x59)
{
    mock_reset();
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, 0x59), AW9523_OK);
}

/* V1.1.1 p.16: the address is 1 0 1 1 0 AD1 AD0, so AD1 high is 0x5A/0x5B */
TEST(test_init_addr_ad1_high)
{
    mock_reset();
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, 0x5A), AW9523_OK);
    ASSERT_EQ(dev.dev_addr, 0x5A);
    mock_reset();
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, 0x5B), AW9523_OK);
    ASSERT_EQ(dev.dev_addr, 0x5B);
}

TEST(test_init_bad_addr)
{
    mock_reset();
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, 0x57), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, 0x5C), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_init(&dev, &mock_i2c, 0x20), AW9523_ERR_PARAM);
}

/* ========== direction tests ========== */

TEST(test_set_direction_output)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* CONFIG starts at 0xFF (all input), set pin 3 to output */
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT0, 3, AW9523_DIR_OUTPUT),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_CONFIG_P0], 0xF7);  /* bit 3 cleared */
}

TEST(test_set_direction_input)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* First set to output, then back to input */
    mock_regs[REG_CONFIG_P1] = 0x00;  /* all output */
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT1, 5, AW9523_DIR_INPUT),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_CONFIG_P1], 0x20);  /* bit 5 set */
}

TEST(test_set_direction_pin_out_of_range)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT0, 8, AW9523_DIR_OUTPUT),
              AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT0, 255, AW9523_DIR_INPUT),
              AW9523_ERR_PARAM);
}

TEST(test_set_direction_invalid_dir)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT0, 0, (aw9523_dir_t)2),
              AW9523_ERR_PARAM);
}

TEST(test_set_direction_i2c_fail_read)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT0, 0, AW9523_DIR_OUTPUT),
              AW9523_ERR_I2C);
}

TEST(test_set_direction_i2c_fail_write)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* Read succeeds, write fails */
    mock_fail_at = mock_op_count + 1;
    ASSERT_EQ(aw9523_set_direction(&dev, AW9523_PORT0, 0, AW9523_DIR_OUTPUT),
              AW9523_ERR_I2C);
    mock_fail_at = -1;
}

/* ========== write pin tests ========== */

TEST(test_write_pin_high)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_regs[REG_OUTPUT_P0] = 0x00;
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT0, 4, AW9523_PIN_HIGH),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P0], 0x10);  /* bit 4 set */
}

TEST(test_write_pin_low)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_regs[REG_OUTPUT_P1] = 0xFF;
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT1, 2, AW9523_PIN_LOW),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P1], 0xFB);  /* bit 2 cleared */
}

TEST(test_write_pin_preserves_others)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_regs[REG_OUTPUT_P0] = 0xA5;  /* 1010_0101 */
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT0, 1, AW9523_PIN_HIGH),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P0], 0xA7);  /* 1010_0111 */
}

TEST(test_write_pin_out_of_range)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT0, 8, AW9523_PIN_HIGH),
              AW9523_ERR_PARAM);
}

TEST(test_write_pin_invalid_level)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT0, 0, (aw9523_level_t)2),
              AW9523_ERR_PARAM);
}

TEST(test_write_pin_i2c_fail_read)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT0, 0, AW9523_PIN_HIGH),
              AW9523_ERR_I2C);
}

TEST(test_write_pin_i2c_fail_write)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_at = mock_op_count + 1;
    ASSERT_EQ(aw9523_write_pin(&dev, AW9523_PORT0, 0, AW9523_PIN_HIGH),
              AW9523_ERR_I2C);
    mock_fail_at = -1;
}

/* ========== read pin tests ========== */

TEST(test_read_pin_high)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_regs[REG_INPUT_P0] = 0x42;  /* bits 1 and 6 set */
    aw9523_level_t val = AW9523_PIN_LOW;
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT0, 6, &val), AW9523_OK);
    ASSERT_EQ(val, AW9523_PIN_HIGH);
}

TEST(test_read_pin_low)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_regs[REG_INPUT_P1] = 0x42;
    aw9523_level_t val = AW9523_PIN_HIGH;
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT1, 0, &val), AW9523_OK);
    ASSERT_EQ(val, AW9523_PIN_LOW);
}

TEST(test_read_pin_null_val)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT0, 0, NULL), AW9523_ERR_PARAM);
}

TEST(test_read_pin_out_of_range)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    aw9523_level_t val;
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT0, 8, &val), AW9523_ERR_PARAM);
}

TEST(test_read_pin_i2c_fail)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    aw9523_level_t val;
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_read_pin(&dev, AW9523_PORT0, 0, &val), AW9523_ERR_I2C);
}

/* ========== write/read port tests ========== */

TEST(test_write_port)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_write_port(&dev, AW9523_PORT0, 0xAA), AW9523_OK);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P0], 0xAA);
    ASSERT_EQ(aw9523_write_port(&dev, AW9523_PORT1, 0x55), AW9523_OK);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P1], 0x55);
}

TEST(test_read_port)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_regs[REG_INPUT_P0] = 0xDE;
    mock_regs[REG_INPUT_P1] = 0xAD;
    uint8_t v0 = 0, v1 = 0;
    ASSERT_EQ(aw9523_read_port(&dev, AW9523_PORT0, &v0), AW9523_OK);
    ASSERT_EQ(aw9523_read_port(&dev, AW9523_PORT1, &v1), AW9523_OK);
    ASSERT_EQ(v0, 0xDE);
    ASSERT_EQ(v1, 0xAD);
}

TEST(test_read_port_null_val)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_read_port(&dev, AW9523_PORT0, NULL), AW9523_ERR_PARAM);
}

TEST(test_write_port_i2c_fail)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_write_port(&dev, AW9523_PORT0, 0xFF), AW9523_ERR_I2C);
}

TEST(test_read_port_i2c_fail)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    uint8_t val;
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_read_port(&dev, AW9523_PORT0, &val), AW9523_ERR_I2C);
}

/* ========== ports independent ========== */

TEST(test_ports_independent)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_write_port(&dev, AW9523_PORT0, 0xAA), AW9523_OK);
    ASSERT_EQ(aw9523_write_port(&dev, AW9523_PORT1, 0x55), AW9523_OK);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P0], 0xAA);
    ASSERT_EQ(mock_regs[REG_OUTPUT_P1], 0x55);
}

/* ========== LED mode tests ========== */

TEST(test_set_led_mode_to_led)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* After init LED_MODE is 0xFF (all GPIO). Set pin 3 to LED mode */
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT0, 3, AW9523_MODE_LED),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_LED_MODE_P0], 0xF7);  /* bit 3 cleared = LED */
}

TEST(test_set_led_mode_to_gpio)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_regs[REG_LED_MODE_P1] = 0x00;  /* all LED */
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT1, 5, AW9523_MODE_GPIO),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_LED_MODE_P1], 0x20);  /* bit 5 set = GPIO */
}

TEST(test_set_led_mode_invalid)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT0, 0, (aw9523_led_mode_t)2),
              AW9523_ERR_PARAM);
}

TEST(test_set_led_mode_pin_out_of_range)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT0, 8, AW9523_MODE_LED),
              AW9523_ERR_PARAM);
}

TEST(test_set_led_mode_i2c_fail_read)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT0, 0, AW9523_MODE_LED),
              AW9523_ERR_I2C);
}

TEST(test_set_led_mode_i2c_fail_write)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_at = mock_op_count + 1;
    ASSERT_EQ(aw9523_set_led_mode(&dev, AW9523_PORT0, 0, AW9523_MODE_LED),
              AW9523_ERR_I2C);
    mock_fail_at = -1;
}

/* ========== LED current tests ========== */

TEST(test_set_led_current_port0)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* Port0 pin0 = DIM4 (0x24), pin7 = DIM11 (0x2B) */
    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT0, 0, 128), AW9523_OK);
    ASSERT_EQ(mock_regs[0x24], 128);

    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT0, 7, 255), AW9523_OK);
    ASSERT_EQ(mock_regs[0x2B], 255);
}

TEST(test_set_led_current_port1)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* Port1 pin0 = DIM0 (0x20), pin3 = DIM3 (0x23) */
    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT1, 0, 64), AW9523_OK);
    ASSERT_EQ(mock_regs[0x20], 64);

    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT1, 3, 200), AW9523_OK);
    ASSERT_EQ(mock_regs[0x23], 200);

    /* Port1 pin4 = DIM12 (0x2C), pin7 = DIM15 (0x2F) */
    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT1, 4, 100), AW9523_OK);
    ASSERT_EQ(mock_regs[0x2C], 100);

    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT1, 7, 50), AW9523_OK);
    ASSERT_EQ(mock_regs[0x2F], 50);
}

TEST(test_set_led_current_zero_and_max)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT0, 0, 0), AW9523_OK);
    ASSERT_EQ(mock_regs[0x24], 0);

    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT0, 0, 255), AW9523_OK);
    ASSERT_EQ(mock_regs[0x24], 255);
}

TEST(test_set_led_current_pin_out_of_range)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT0, 8, 128),
              AW9523_ERR_PARAM);
}

TEST(test_set_led_current_i2c_fail)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT0, 0, 128),
              AW9523_ERR_I2C);
}

/* ========== interrupt tests ========== */

TEST(test_set_interrupt_enable)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* Enable interrupts on pins 0, 1, 7 (mask=0x83). Register is inverted. */
    ASSERT_EQ(aw9523_set_interrupt_enable(&dev, AW9523_PORT0, 0x83),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_INT_P0], (uint8_t)~0x83);  /* 0x7C */
}

TEST(test_set_interrupt_enable_port1)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_interrupt_enable(&dev, AW9523_PORT1, 0xFF),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_INT_P1], 0x00);  /* all enabled = all bits 0 */
}

TEST(test_set_interrupt_disable_all)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_set_interrupt_enable(&dev, AW9523_PORT0, 0x00),
              AW9523_OK);
    ASSERT_EQ(mock_regs[REG_INT_P0], 0xFF);  /* all disabled */
}

TEST(test_set_interrupt_i2c_fail)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_set_interrupt_enable(&dev, AW9523_PORT0, 0xFF),
              AW9523_ERR_I2C);
}

/* ========== reset test ========== */

TEST(test_reset)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    ASSERT_EQ(aw9523_reset(&dev), AW9523_OK);
    ASSERT_EQ(mock_regs[0x7F], 0x00);
}

TEST(test_reset_i2c_fail)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    mock_fail_next = 1;
    ASSERT_EQ(aw9523_reset(&dev), AW9523_ERR_I2C);
}

TEST(test_reset_null_dev)
{
    ASSERT_EQ(aw9523_reset(NULL), AW9523_ERR_PARAM);
}

/* ========== invalid port for all functions ========== */

TEST(test_invalid_port_all_funcs)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    aw9523_port_t bad = (aw9523_port_t)2;
    aw9523_level_t val;
    uint8_t port_val;

    ASSERT_EQ(aw9523_set_direction(&dev, bad, 0, AW9523_DIR_OUTPUT), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_write_pin(&dev, bad, 0, AW9523_PIN_HIGH), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_read_pin(&dev, bad, 0, &val), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_write_port(&dev, bad, 0xFF), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_read_port(&dev, bad, &port_val), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_led_mode(&dev, bad, 0, AW9523_MODE_LED), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_led_current(&dev, bad, 0, 128), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_interrupt_enable(&dev, bad, 0xFF), AW9523_ERR_PARAM);
}

/* ========== NULL dev on every public function ========== */

TEST(test_null_dev_all_funcs)
{
    aw9523_level_t val;
    uint8_t port_val;

    ASSERT_EQ(aw9523_init(NULL, &mock_i2c, TEST_ADDR), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_direction(NULL, AW9523_PORT0, 0, AW9523_DIR_OUTPUT), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_write_pin(NULL, AW9523_PORT0, 0, AW9523_PIN_HIGH), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_read_pin(NULL, AW9523_PORT0, 0, &val), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_write_port(NULL, AW9523_PORT0, 0xFF), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_read_port(NULL, AW9523_PORT0, &port_val), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_led_mode(NULL, AW9523_PORT0, 0, AW9523_MODE_LED), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_led_current(NULL, AW9523_PORT0, 0, 128), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_set_interrupt_enable(NULL, AW9523_PORT0, 0xFF), AW9523_ERR_PARAM);
    ASSERT_EQ(aw9523_reset(NULL), AW9523_ERR_PARAM);
}

/* ========== zero-init struct safety ========== */

TEST(test_zero_init_safety)
{
    aw9523_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* init on zero struct must not crash (i2c is NULL, returns ERR_PARAM) */
    ASSERT_EQ(aw9523_init(&zero_dev, NULL, TEST_ADDR), AW9523_ERR_PARAM);
}

/* ========== DIM register mapping edge cases ========== */

TEST(test_dim_mapping_port0_all_pins)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* Port0: pin0=0x24, pin1=0x25, ..., pin7=0x2B */
    uint8_t expected_regs[] = { 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B };
    for (uint8_t pin = 0; pin < 8; pin++) {
        ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT0, pin,
                                         (uint8_t)(pin + 10u)), AW9523_OK);
        ASSERT_EQ(mock_regs[expected_regs[pin]], (uint8_t)(pin + 10u));
    }
}

TEST(test_dim_mapping_port1_all_pins)
{
    ASSERT_EQ(init_ok(), AW9523_OK);
    /* Port1: pin0=0x20, pin1=0x21, pin2=0x22, pin3=0x23,
     *        pin4=0x2C, pin5=0x2D, pin6=0x2E, pin7=0x2F */
    uint8_t expected_regs[] = { 0x20, 0x21, 0x22, 0x23, 0x2C, 0x2D, 0x2E, 0x2F };
    for (uint8_t pin = 0; pin < 8; pin++) {
        ASSERT_EQ(aw9523_set_led_current(&dev, AW9523_PORT1, pin,
                                         (uint8_t)(pin + 20u)), AW9523_OK);
        ASSERT_EQ(mock_regs[expected_regs[pin]], (uint8_t)(pin + 20u));
    }
}

TEST_MAIN("aw9523")
    /* init */
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_init_i2c_fail_ctl_write);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_null_write_reg);
    RUN(test_init_null_read_regs);
    RUN(test_init_addr_0x59);
    RUN(test_init_addr_ad1_high);
    RUN(test_init_bad_addr);
    /* direction */
    RUN(test_set_direction_output);
    RUN(test_set_direction_input);
    RUN(test_set_direction_pin_out_of_range);
    RUN(test_set_direction_invalid_dir);
    RUN(test_set_direction_i2c_fail_read);
    RUN(test_set_direction_i2c_fail_write);
    /* write pin */
    RUN(test_write_pin_high);
    RUN(test_write_pin_low);
    RUN(test_write_pin_preserves_others);
    RUN(test_write_pin_out_of_range);
    RUN(test_write_pin_invalid_level);
    RUN(test_write_pin_i2c_fail_read);
    RUN(test_write_pin_i2c_fail_write);
    /* read pin */
    RUN(test_read_pin_high);
    RUN(test_read_pin_low);
    RUN(test_read_pin_null_val);
    RUN(test_read_pin_out_of_range);
    RUN(test_read_pin_i2c_fail);
    /* write/read port */
    RUN(test_write_port);
    RUN(test_read_port);
    RUN(test_read_port_null_val);
    RUN(test_write_port_i2c_fail);
    RUN(test_read_port_i2c_fail);
    RUN(test_ports_independent);
    /* LED mode */
    RUN(test_set_led_mode_to_led);
    RUN(test_set_led_mode_to_gpio);
    RUN(test_set_led_mode_invalid);
    RUN(test_set_led_mode_pin_out_of_range);
    RUN(test_set_led_mode_i2c_fail_read);
    RUN(test_set_led_mode_i2c_fail_write);
    /* LED current */
    RUN(test_set_led_current_port0);
    RUN(test_set_led_current_port1);
    RUN(test_set_led_current_zero_and_max);
    RUN(test_set_led_current_pin_out_of_range);
    RUN(test_set_led_current_i2c_fail);
    /* interrupt */
    RUN(test_set_interrupt_enable);
    RUN(test_set_interrupt_enable_port1);
    RUN(test_set_interrupt_disable_all);
    RUN(test_set_interrupt_i2c_fail);
    /* reset */
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_reset_null_dev);
    /* cross-cutting */
    RUN(test_invalid_port_all_funcs);
    RUN(test_null_dev_all_funcs);
    RUN(test_zero_init_safety);
    /* DIM mapping */
    RUN(test_dim_mapping_port0_all_pins);
    RUN(test_dim_mapping_port1_all_pins);
TEST_MAIN_END()
