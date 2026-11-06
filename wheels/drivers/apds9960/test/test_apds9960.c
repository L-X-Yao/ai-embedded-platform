// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "apds9960.h"

/* ---------- register addresses (match driver) ---------- */
#define REG_ENABLE  0x80u
#define REG_ATIME   0x81u
#define REG_PPULSE  0x8Eu
#define REG_CONTROL 0x8Fu
#define REG_ID      0x92u
#define REG_CDATAL  0x94u
#define REG_RDATAL  0x96u
#define REG_GDATAL  0x98u
#define REG_BDATAL  0x9Au
#define REG_PDATA   0x9Cu
#define REG_GPENTH  0xA0u
#define REG_GEXTH   0xA1u
#define REG_GCONF1  0xA2u
#define REG_GCONF2  0xA3u
#define REG_GPULSE  0xA6u
#define REG_GCONF4  0xABu
#define REG_GFLVL   0xAEu
#define REG_GFIFO_U 0xFCu

#define CHIP_ID     0xABu

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[REG_ID] = CHIP_ID;
}

static const apds9960_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static apds9960_t dev;

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    /* Verify power-on bit set */
    ASSERT_EQ(mock_regs[REG_ENABLE] & 0x01, 0x01);
    /* Verify default ATIME */
    ASSERT_EQ(mock_regs[REG_ATIME], 0xFF);
    /* p23: PPLEN 1 in bits 7:6 is 8 us, PPULSE 7 in bits 5:0 is 8 pulses */
    ASSERT_EQ_HEX(mock_regs[REG_PPULSE], (1u << 6) | 7u);
    /* Verify CONTROL configured */
    ASSERT_EQ(mock_regs[REG_CONTROL], 0x00);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[REG_ID] = 0x00;
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(apds9960_init(NULL, &mock_i2c), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_init(&dev, NULL), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_read_proximity(NULL, NULL), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_read_color(NULL, NULL), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_read_gesture(NULL, NULL), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_enable_proximity(NULL), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_enable_color(NULL), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_enable_gesture(NULL), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_disable_all(NULL), APDS9960_ERR_PARAM);
}

/* ---------- proximity tests ---------- */

TEST(test_enable_proximity)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_proximity(&dev), APDS9960_OK);
    /* PEN (bit 2) + PON (bit 0) should be set */
    ASSERT_TRUE(mock_regs[REG_ENABLE] & 0x04);
    ASSERT_TRUE(mock_regs[REG_ENABLE] & 0x01);
}

TEST(test_read_proximity)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_proximity(&dev), APDS9960_OK);
    mock_regs[REG_PDATA] = 200;
    uint8_t prox = 0;
    ASSERT_EQ(apds9960_read_proximity(&dev, &prox), APDS9960_OK);
    ASSERT_EQ(prox, 200);
}

TEST(test_proximity_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    mock_fail_next = 1;
    uint8_t prox = 0;
    ASSERT_EQ(apds9960_read_proximity(&dev, &prox), APDS9960_ERR_I2C);
}

/* ---------- color tests ---------- */

TEST(test_enable_color)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_color(&dev), APDS9960_OK);
    /* AEN (bit 1) + PON (bit 0) should be set */
    ASSERT_TRUE(mock_regs[REG_ENABLE] & 0x02);
    ASSERT_TRUE(mock_regs[REG_ENABLE] & 0x01);
}

TEST(test_read_color)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_color(&dev), APDS9960_OK);

    /* Set RGBC data: C=0x1234, R=0x5678, G=0x9ABC, B=0xDEF0 (little-endian) */
    mock_regs[REG_CDATAL]     = 0x34;
    mock_regs[REG_CDATAL + 1] = 0x12;
    mock_regs[REG_RDATAL]     = 0x78;
    mock_regs[REG_RDATAL + 1] = 0x56;
    mock_regs[REG_GDATAL]     = 0xBC;
    mock_regs[REG_GDATAL + 1] = 0x9A;
    mock_regs[REG_BDATAL]     = 0xF0;
    mock_regs[REG_BDATAL + 1] = 0xDE;

    apds9960_color_t color;
    ASSERT_EQ(apds9960_read_color(&dev, &color), APDS9960_OK);
    ASSERT_EQ_HEX(color.c, 0x1234);
    ASSERT_EQ_HEX(color.r, 0x5678);
    ASSERT_EQ_HEX(color.g, 0x9ABC);
    ASSERT_EQ_HEX(color.b, 0xDEF0);
}

TEST(test_color_i2c_fail)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    mock_fail_next = 1;
    apds9960_color_t color;
    ASSERT_EQ(apds9960_read_color(&dev, &color), APDS9960_ERR_I2C);
}

/* ---------- gesture tests ---------- */

TEST(test_enable_gesture)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);
    /* GEN (bit 6) + PEN (bit 2) + PON (bit 0) should be set */
    ASSERT_TRUE(mock_regs[REG_ENABLE] & 0x40);
    ASSERT_TRUE(mock_regs[REG_ENABLE] & 0x04);
    ASSERT_TRUE(mock_regs[REG_ENABLE] & 0x01);
    /* Gesture config should be written */
    ASSERT_EQ(mock_regs[REG_GPENTH], 40);
    ASSERT_EQ(mock_regs[REG_GEXTH], 30);
}

/* The reset value 0x40 of GPULSE is one pulse of 8 us. SparkFun and Adafruit
 * both run gesture on 10 pulses of 32 us at 4x gain. */
TEST(test_enable_gesture_sets_pulses_and_gain)
{
    mock_reset();
    mock_regs[REG_GPULSE] = 0x40;
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);
    /* p29: GPLEN in bits 7:6, GPULSE in bits 5:0 as pulses - 1 */
    ASSERT_EQ_HEX(mock_regs[REG_GPULSE], (3u << 6) | 9u);
    /* p29: GGAIN in bits 6:5, with GLDRIVE 100 mA and GWTIME 0 */
    ASSERT_EQ_HEX(mock_regs[REG_GCONF2], 2u << 5);
}

/* Each of the 8 bus operations of enable_gesture reports its failure */
TEST(test_enable_gesture_i2c_fail)
{
    for (int op = 0; op < 8; op++) {
        mock_reset();
        ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
        mock_fail_at = mock_op_count + op;
        ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_ERR_I2C);
    }
}

TEST(test_gesture_up)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);

    /* Simulate FIFO: U=100, D=20, L=50, R=50 -> UP gesture (U-D=80 >> threshold) */
    mock_regs[REG_GFLVL]   = 1;
    mock_regs[REG_GFIFO_U] = 100;
    mock_regs[REG_GFIFO_U + 1] = 20;  /* D */
    mock_regs[REG_GFIFO_U + 2] = 50;  /* L */
    mock_regs[REG_GFIFO_U + 3] = 50;  /* R */

    apds9960_gesture_t gesture = APDS9960_GESTURE_NONE;
    ASSERT_EQ(apds9960_read_gesture(&dev, &gesture), APDS9960_OK);
    ASSERT_EQ(gesture, APDS9960_GESTURE_UP);
}

TEST(test_gesture_down)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);

    /* U=20, D=100 -> DOWN gesture (U-D=-80) */
    mock_regs[REG_GFLVL]   = 1;
    mock_regs[REG_GFIFO_U] = 20;
    mock_regs[REG_GFIFO_U + 1] = 100;
    mock_regs[REG_GFIFO_U + 2] = 50;
    mock_regs[REG_GFIFO_U + 3] = 50;

    apds9960_gesture_t gesture = APDS9960_GESTURE_NONE;
    ASSERT_EQ(apds9960_read_gesture(&dev, &gesture), APDS9960_OK);
    ASSERT_EQ(gesture, APDS9960_GESTURE_DOWN);
}

TEST(test_gesture_left)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);

    /* U=50, D=50, L=100, R=20 -> LEFT gesture (L-R=80) */
    mock_regs[REG_GFLVL]   = 1;
    mock_regs[REG_GFIFO_U] = 50;
    mock_regs[REG_GFIFO_U + 1] = 50;
    mock_regs[REG_GFIFO_U + 2] = 100;
    mock_regs[REG_GFIFO_U + 3] = 20;

    apds9960_gesture_t gesture = APDS9960_GESTURE_NONE;
    ASSERT_EQ(apds9960_read_gesture(&dev, &gesture), APDS9960_OK);
    ASSERT_EQ(gesture, APDS9960_GESTURE_LEFT);
}

TEST(test_gesture_right)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);

    /* U=50, D=50, L=20, R=100 -> RIGHT gesture (L-R=-80) */
    mock_regs[REG_GFLVL]   = 1;
    mock_regs[REG_GFIFO_U] = 50;
    mock_regs[REG_GFIFO_U + 1] = 50;
    mock_regs[REG_GFIFO_U + 2] = 20;
    mock_regs[REG_GFIFO_U + 3] = 100;

    apds9960_gesture_t gesture = APDS9960_GESTURE_NONE;
    ASSERT_EQ(apds9960_read_gesture(&dev, &gesture), APDS9960_OK);
    ASSERT_EQ(gesture, APDS9960_GESTURE_RIGHT);
}

TEST(test_gesture_none_empty_fifo)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);

    mock_regs[REG_GFLVL] = 0;

    apds9960_gesture_t gesture = APDS9960_GESTURE_UP;
    ASSERT_EQ(apds9960_read_gesture(&dev, &gesture), APDS9960_OK);
    ASSERT_EQ(gesture, APDS9960_GESTURE_NONE);
}

TEST(test_gesture_below_threshold)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_gesture(&dev), APDS9960_OK);

    /* Small deltas below threshold -> no gesture */
    mock_regs[REG_GFLVL]   = 1;
    mock_regs[REG_GFIFO_U] = 51;
    mock_regs[REG_GFIFO_U + 1] = 50;
    mock_regs[REG_GFIFO_U + 2] = 50;
    mock_regs[REG_GFIFO_U + 3] = 51;

    apds9960_gesture_t gesture = APDS9960_GESTURE_UP;
    ASSERT_EQ(apds9960_read_gesture(&dev, &gesture), APDS9960_OK);
    ASSERT_EQ(gesture, APDS9960_GESTURE_NONE);
}

/* ---------- disable test ---------- */

TEST(test_disable_all)
{
    mock_reset();
    ASSERT_EQ(apds9960_init(&dev, &mock_i2c), APDS9960_OK);
    ASSERT_EQ(apds9960_enable_proximity(&dev), APDS9960_OK);
    ASSERT_EQ(apds9960_disable_all(&dev), APDS9960_OK);
    ASSERT_EQ(mock_regs[REG_ENABLE], 0x00);
}

/* ---------- zero-init safety ---------- */

TEST(test_zero_init_safety)
{
    apds9960_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    /* Zero-init struct must not crash on NULL i2c check */
    ASSERT_EQ(apds9960_enable_proximity(&zero_dev), APDS9960_ERR_PARAM);
    ASSERT_EQ(apds9960_disable_all(&zero_dev), APDS9960_ERR_PARAM);
}

TEST_MAIN("apds9960")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_enable_proximity);
    RUN(test_read_proximity);
    RUN(test_proximity_i2c_fail);
    RUN(test_enable_color);
    RUN(test_read_color);
    RUN(test_color_i2c_fail);
    RUN(test_enable_gesture);
    RUN(test_enable_gesture_sets_pulses_and_gain);
    RUN(test_enable_gesture_i2c_fail);
    RUN(test_gesture_up);
    RUN(test_gesture_down);
    RUN(test_gesture_left);
    RUN(test_gesture_right);
    RUN(test_gesture_none_empty_fifo);
    RUN(test_gesture_below_threshold);
    RUN(test_disable_all);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
