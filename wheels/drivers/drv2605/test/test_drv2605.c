// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "drv2605.h"

/* ---------- chip-specific mock setup ---------- */

/* DRV2605L device ID = 7, stored in bits [7:5] of status reg 0x00 */
#define MOCK_STATUS_DEVID_2605L  (7u << 5)

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = MOCK_STATUS_DEVID_2605L; /* status: device ID */
    mock_regs[0x01] = 0x40;                     /* mode: standby */
    mock_regs[0x1A] = 0x36;                     /* feedback ctrl default */
}

static const drv2605_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static drv2605_t dev;

static void setup(void)
{
    mock_reset();
    drv2605_init(&dev, &mock_i2c);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    drv2605_t d;
    drv2605_err_t e = drv2605_init(&d, &mock_i2c);
    ASSERT_EQ(e, DRV2605_OK);
    /* Mode register should be set to internal trigger (0x00) */
    ASSERT_EQ(mock_regs[0x01], DRV2605_MODE_INTERNAL_TRIGGER);
}

TEST(test_init_drv2605_non_l)
{
    mock_reset();
    /* DRV2605 (non-L) has device ID = 3 */
    mock_regs[0x00] = (3u << 5);
    drv2605_t d;
    drv2605_err_t e = drv2605_init(&d, &mock_i2c);
    ASSERT_EQ(e, DRV2605_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x00] = (1u << 5); /* invalid device ID */
    drv2605_t d;
    drv2605_err_t e = drv2605_init(&d, &mock_i2c);
    ASSERT_EQ(e, DRV2605_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    drv2605_t d;
    drv2605_err_t e = drv2605_init(&d, &mock_i2c);
    ASSERT_EQ(e, DRV2605_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(drv2605_init(NULL, &mock_i2c), DRV2605_ERR_PARAM);
    drv2605_t d;
    ASSERT_EQ(drv2605_init(&d, NULL), DRV2605_ERR_PARAM);
    ASSERT_EQ(drv2605_set_actuator(NULL, DRV2605_ACTUATOR_ERM), DRV2605_ERR_PARAM);
    ASSERT_EQ(drv2605_set_waveform(NULL, 0, 1), DRV2605_ERR_PARAM);
    ASSERT_EQ(drv2605_go(NULL), DRV2605_ERR_PARAM);
    ASSERT_EQ(drv2605_stop(NULL), DRV2605_ERR_PARAM);
    ASSERT_EQ(drv2605_set_realtime_value(NULL, 0x7F), DRV2605_ERR_PARAM);
}

/* ---------- set_mode tests ---------- */

TEST(test_set_actuator_erm)
{
    setup();
    /* Pre-set LRA bit to verify it gets cleared */
    mock_regs[0x1A] = 0xB6; /* bit 7 set = LRA */
    drv2605_err_t e = drv2605_set_actuator(&dev, DRV2605_ACTUATOR_ERM);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x1A] & 0x80, 0x00); /* LRA bit cleared */
}

TEST(test_set_actuator_lra)
{
    setup();
    mock_regs[0x1A] = 0x36; /* bit 7 clear = ERM */
    drv2605_err_t e = drv2605_set_actuator(&dev, DRV2605_ACTUATOR_LRA);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x1A] & 0x80, 0x80); /* LRA bit set */
}

TEST(test_set_actuator_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    drv2605_err_t e = drv2605_set_actuator(&dev, DRV2605_ACTUATOR_ERM);
    ASSERT_EQ(e, DRV2605_ERR_I2C);
}

/* ---------- waveform tests ---------- */

TEST(test_set_waveform)
{
    setup();
    drv2605_err_t e = drv2605_set_waveform(&dev, 0, 47);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x04], 47);

    e = drv2605_set_waveform(&dev, 7, 100);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x0B], 100);
}

TEST(test_set_waveform_slot_out_of_range)
{
    setup();
    drv2605_err_t e = drv2605_set_waveform(&dev, 8, 1);
    ASSERT_EQ(e, DRV2605_ERR_PARAM);
}

/* ---------- go / stop tests ---------- */

TEST(test_go_and_stop)
{
    setup();
    drv2605_err_t e = drv2605_go(&dev);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x0C], 0x01);

    e = drv2605_stop(&dev);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x0C], 0x00);
}

TEST(test_go_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(drv2605_go(&dev), DRV2605_ERR_I2C);
}

/* ---------- realtime value test ---------- */

TEST(test_realtime_value)
{
    setup();
    drv2605_err_t e = drv2605_set_realtime_value(&dev, 0x7F);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x02], 0x7F);

    e = drv2605_set_realtime_value(&dev, 0x00);
    ASSERT_EQ(e, DRV2605_OK);
    ASSERT_EQ(mock_regs[0x02], 0x00);
}

/* ---------- additional I2C failure paths ---------- */

/* drv2605_init: write to MODE register fails (op 1, after status read) */
TEST(test_init_mode_write_fail)
{
    mock_reset();
    mock_fail_at = 1; /* op 0 = read STATUS (ok), op 1 = write MODE (fail) */
    drv2605_t d;
    drv2605_err_t e = drv2605_init(&d, &mock_i2c);
    ASSERT_EQ(e, DRV2605_ERR_I2C);
}

/* drv2605_init: write to a waveform slot fails during clear loop */
TEST(test_init_waveform_clear_fail)
{
    mock_reset();
    mock_fail_at = 2; /* op 2 = first waveform slot clear write */
    drv2605_t d;
    drv2605_err_t e = drv2605_init(&d, &mock_i2c);
    ASSERT_EQ(e, DRV2605_ERR_I2C);
}

/* drv2605_init: Initialization Procedure step 6 (DRV2605 sheet p.53). 0x03
 * starts as the Empty library of that sheet's p.33, and nothing would play. */
TEST(test_init_selects_a_library)
{
    mock_reset();
    mock_regs[0x00] = (3u << 5);  /* DRV2605 */
    mock_regs[0x03] = 0x10;       /* HI_Z stays as it is */
    drv2605_t d;
    ASSERT_EQ(drv2605_init(&d, &mock_i2c), DRV2605_OK);
    ASSERT_EQ(mock_regs[0x03], 0x11);
}

/* drv2605_init: the library matches the actuator FEEDBACK already holds */
TEST(test_init_library_follows_the_actuator)
{
    mock_reset();
    mock_regs[0x1A] = 0xB6;  /* N_ERM_LRA set */
    drv2605_t d;
    ASSERT_EQ(drv2605_init(&d, &mock_i2c), DRV2605_OK);
    ASSERT_EQ(mock_regs[0x03], 0x06);
}

/* drv2605_init: ops 10, 11 and 12 read FEEDBACK, read 0x03 and write 0x03 */
TEST(test_init_library_i2c_fail)
{
    for (int op = 10; op <= 12; op++) {
        mock_reset();
        mock_fail_at = op;
        drv2605_t d;
        ASSERT_EQ(drv2605_init(&d, &mock_i2c), DRV2605_ERR_I2C);
    }
}

/* drv2605_set_actuator: invalid type value rejected */
TEST(test_set_actuator_invalid_type)
{
    setup();
    drv2605_err_t e = drv2605_set_actuator(&dev, (drv2605_actuator_t)2);
    ASSERT_EQ(e, DRV2605_ERR_PARAM);
}

/* drv2605_set_actuator: write to FEEDBACK register fails (read succeeds) */
TEST(test_set_actuator_write_fail)
{
    setup();
    /* After setup(): mock_op_count == 13 (1 read + 1 write_mode + 8 waveform writes
     * + 3 to select the library).
     * set_actuator does: op 13 = read FEEDBACK (ok), op 14 = write FEEDBACK (fail). */
    mock_fail_at = 14;
    drv2605_err_t e = drv2605_set_actuator(&dev, DRV2605_ACTUATOR_LRA);
    ASSERT_EQ(e, DRV2605_ERR_I2C);
}

/* drv2605_set_waveform: I2C write fails */
TEST(test_set_waveform_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    drv2605_err_t e = drv2605_set_waveform(&dev, 0, 47);
    ASSERT_EQ(e, DRV2605_ERR_I2C);
}

/* drv2605_stop: I2C write fails */
TEST(test_stop_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(drv2605_stop(&dev), DRV2605_ERR_I2C);
}

/* drv2605_set_realtime_value: I2C write fails */
TEST(test_realtime_value_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    ASSERT_EQ(drv2605_set_realtime_value(&dev, 0x40), DRV2605_ERR_I2C);
}

TEST(test_realtime_enters_rtp_mode)
{
    setup();
    ASSERT_EQ(drv2605_set_realtime_value(&dev, 0x40), DRV2605_OK);
    ASSERT_EQ(mock_regs[0x01], DRV2605_MODE_REALTIME);
    ASSERT_EQ(mock_regs[0x02], 0x40);
}

TEST(test_stop_and_go_leave_rtp_mode)
{
    setup();
    drv2605_set_realtime_value(&dev, 0x40);
    ASSERT_EQ(drv2605_stop(&dev), DRV2605_OK);
    ASSERT_EQ(mock_regs[0x01], DRV2605_MODE_INTERNAL_TRIGGER);

    drv2605_set_realtime_value(&dev, 0x40);
    ASSERT_EQ(drv2605_go(&dev), DRV2605_OK);
    ASSERT_EQ(mock_regs[0x01], DRV2605_MODE_INTERNAL_TRIGGER);
    ASSERT_EQ(mock_regs[0x0C], 0x01);
}

TEST(test_set_actuator_picks_the_library)
{
    setup();
    mock_regs[0x03] = 0x10;  /* HI_Z stays as it is */
    ASSERT_EQ(drv2605_set_actuator(&dev, DRV2605_ACTUATOR_LRA), DRV2605_OK);
    ASSERT_EQ(mock_regs[0x03], 0x16);
    ASSERT_EQ(drv2605_set_actuator(&dev, DRV2605_ACTUATOR_ERM), DRV2605_OK);
    ASSERT_EQ(mock_regs[0x03], 0x11);
}

TEST_MAIN("drv2605")
    RUN(test_init_ok);
    RUN(test_init_drv2605_non_l);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_set_actuator_erm);
    RUN(test_set_actuator_lra);
    RUN(test_set_actuator_i2c_fail);
    RUN(test_set_waveform);
    RUN(test_set_waveform_slot_out_of_range);
    RUN(test_go_and_stop);
    RUN(test_go_i2c_fail);
    RUN(test_realtime_value);
    RUN(test_init_mode_write_fail);
    RUN(test_init_waveform_clear_fail);
    RUN(test_set_actuator_invalid_type);
    RUN(test_set_actuator_write_fail);
    RUN(test_set_waveform_i2c_fail);
    RUN(test_stop_i2c_fail);
    RUN(test_realtime_value_i2c_fail);
    RUN(test_realtime_enters_rtp_mode);
    RUN(test_stop_and_go_leave_rtp_mode);
    RUN(test_init_selects_a_library);
    RUN(test_init_library_follows_the_actuator);
    RUN(test_init_library_i2c_fail);
    RUN(test_set_actuator_picks_the_library);
TEST_MAIN_END()
