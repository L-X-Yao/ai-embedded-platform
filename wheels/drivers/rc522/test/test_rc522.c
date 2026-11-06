// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_time.h"
#include <string.h>
#include "rc522.h"
#include "hw_model_rc522.h"

/* ---------- mock SPI: the data sheet model of hw_model_rc522.h ---------- */

static const rc522_spi_t mock_spi = {
    .cs_low   = hw_model_cs_low,
    .cs_high  = hw_model_cs_high,
    .xfer     = hw_model_xfer,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    hw_model_reset();
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x92);
}

TEST(test_init_version_v1)
{
    hw_model_reset();
    hw_regs[0x37] = 0x91;  /* v1.0 */
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x91);
}

TEST(test_init_bad_version)
{
    hw_model_reset();
    hw_regs[0x37] = 0x00;  /* invalid */
    rc522_t dev;
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_ERR_ID);
}

TEST(test_null_safety)
{
    rc522_t dev;
    ASSERT_EQ(rc522_init(NULL, &mock_spi), RC522_ERR_PARAM);
    ASSERT_EQ(rc522_init(&dev, NULL), RC522_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    rc522_spi_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    rc522_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(rc522_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)RC522_ERR_PARAM);
    ASSERT_EQ(-2, (int)RC522_ERR_SPI);
    ASSERT_EQ(-3, (int)RC522_ERR_ID);
    ASSERT_EQ(-4, (int)RC522_ERR_TIMEOUT);
    ASSERT_EQ(-5, (int)RC522_ERR_COLL);
}

TEST(test_get_version)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);
    ASSERT_EQ(rc522_get_version(&dev), 0x92);
    ASSERT_EQ(rc522_get_version(NULL), 0);
}

TEST(test_antenna_on)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);
    ASSERT_EQ(rc522_antenna_on(&dev), RC522_OK);
    /* TxControlReg should have bits 0,1 set */
    ASSERT_TRUE(hw_regs[0x14] & 0x03);
}

TEST(test_antenna_on_null)
{
    ASSERT_EQ(rc522_antenna_on(NULL), RC522_ERR_PARAM);
}

TEST(test_detect_card)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm ATQA response (Mifare 1K: 0x04, 0x00) */
    uint8_t atqa[2] = {0x04, 0x00};
    hw_model_arm_response(atqa, 2);

    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
}

TEST(test_detect_card_null)
{
    ASSERT_EQ(rc522_detect_card(NULL), RC522_ERR_PARAM);
}

TEST(test_detect_card_timeout)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* No response armed: the model's timer runs out after the transmission */
    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_TIMEOUT);
}

TEST(test_read_uid)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm UID response: 0xDE 0xAD 0xBE 0xEF + BCC */
    uint8_t uid_resp[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0};
    uid_resp[4] = (uint8_t)(0xDE ^ 0xAD ^ 0xBE ^ 0xEF);
    hw_model_arm_response(uid_resp, 5);

    rc522_uid_t uid;
    ASSERT_EQ(rc522_read_card_uid(&dev, &uid), RC522_OK);
    ASSERT_EQ(uid.len, 4);
    ASSERT_EQ(uid.bytes[0], 0xDE);
    ASSERT_EQ(uid.bytes[1], 0xAD);
    ASSERT_EQ(uid.bytes[2], 0xBE);
    ASSERT_EQ(uid.bytes[3], 0xEF);
}

TEST(test_read_uid_bad_bcc)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm UID response with wrong BCC */
    uint8_t uid_resp[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFF};
    hw_model_arm_response(uid_resp, 5);

    rc522_uid_t uid;
    ASSERT_EQ(rc522_read_card_uid(&dev, &uid), RC522_ERR_COLL);
}

TEST(test_read_uid_null)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);
    ASSERT_EQ(rc522_read_card_uid(NULL, &(rc522_uid_t){0}), RC522_ERR_PARAM);
    ASSERT_EQ(rc522_read_card_uid(&dev, NULL), RC522_ERR_PARAM);
}

TEST(test_halt)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* HALT expects timeout (card stops responding), which the model raises */
    ASSERT_EQ(rc522_halt(&dev), RC522_OK);
}

TEST(test_halt_null)
{
    ASSERT_EQ(rc522_halt(NULL), RC522_ERR_PARAM);
}

TEST(test_self_test_null)
{
    ASSERT_EQ(rc522_self_test(NULL), RC522_ERR_PARAM);
}

TEST(test_self_test_ok)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    ASSERT_EQ(rc522_self_test(&dev), RC522_OK);
}

TEST(test_self_test_all_zero_fails)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* A chip that returns all zeros (fail case) */
    hw_self_test_all_zero = true;

    ASSERT_EQ(rc522_self_test(&dev), RC522_ERR_SPI);
}

TEST(test_self_test_timeout)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* A chip that never finishes: FIFO stays short, poll times out */
    hw_self_test_hangs = true;

    ASSERT_EQ(rc522_self_test(&dev), RC522_ERR_TIMEOUT);
}

TEST(test_self_test_uninit_dev)
{
    rc522_t dev;
    memset(&dev, 0, sizeof(dev));
    ASSERT_EQ(rc522_self_test(&dev), RC522_ERR_PARAM);
}

TEST(test_transceive_collision_error)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* CollErr is bit 3 (0x08). The error mask checks 0x13 first, so CollErr
     * must be combined with at least one of those bits to enter the error
     * handler. Set CollErr + ParityErr to exercise the collision path. */
    uint8_t dummy[2] = {0x04, 0x00};
    hw_model_arm_response(dummy, 2);
    hw_rx_errors = 0x0A;  /* CollErr (0x08) | ParityErr (0x02) */

    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_COLL);
}

TEST(test_transceive_pure_collision_error)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Pure CollErr (0x08 only, no other error bits).
     * Before the fix, outer mask 0x13 missed CollErr, returning RC522_OK.
     * After fix to mask 0x1B, this must return RC522_ERR_COLL. */
    uint8_t dummy[2] = {0x04, 0x00};
    hw_model_arm_response(dummy, 2);
    hw_rx_errors = 0x08;  /* CollErr only */

    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_COLL);
}

TEST(test_transceive_parity_error)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm a dummy response but inject parity error (bit 1 of ErrorReg) */
    uint8_t dummy[2] = {0x04, 0x00};
    hw_model_arm_response(dummy, 2);
    hw_rx_errors = 0x02;  /* ParityErr */

    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_SPI);
}

TEST(test_transceive_buffer_overflow_error)
{
    hw_model_reset();
    rc522_t dev;
    rc522_init(&dev, &mock_spi);

    /* Arm a dummy response but inject buffer overflow (bit 4 of ErrorReg) */
    uint8_t dummy[2] = {0x04, 0x00};
    hw_model_arm_response(dummy, 2);
    hw_rx_errors = 0x10;  /* BufferOvfl */

    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_SPI);
}

TEST(test_init_switches_the_field_on)
{
    rc522_t dev;
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    /* TxControlReg resets to 80h, which leaves both drivers off */
    ASSERT_EQ(hw_regs[0x14] & 0x03, 0x03);
}

TEST(test_self_test_restores_the_configuration)
{
    rc522_t dev;
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    ASSERT_EQ(rc522_self_test(&dev), RC522_OK);

    ASSERT_EQ_HEX(hw_regs[0x2A], 0x8D);
    ASSERT_EQ_HEX(hw_regs[0x15], 0x40);
    ASSERT_EQ_HEX(hw_regs[0x11], 0x3D);
    ASSERT_EQ(hw_regs[0x14] & 0x03, 0x03);
}

TEST(test_self_test_refuses_a_wrong_signature)
{
    rc522_t dev;
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    hw_self_test_corrupt = true;
    ASSERT_EQ(rc522_self_test(&dev), RC522_ERR_SPI);
}

TEST(test_detect_card_needs_the_field)
{
    rc522_t dev;
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);

    /* p.50: without Tx1RFEn or Tx2RFEn there is no carrier for a card */
    hw_regs[0x14] = 0x80;
    uint8_t atqa[2] = {0x04, 0x00};
    hw_model_arm_response(atqa, 2);

    ASSERT_EQ(rc522_detect_card(&dev), RC522_ERR_TIMEOUT);
}

TEST(test_self_test_version_1)
{
    rc522_t dev;
    hw_model_reset();
    hw_regs[0x37] = 0x91;
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);
    ASSERT_EQ(rc522_self_test(&dev), RC522_OK);
}

TEST(test_init_leaves_the_rest_at_reset)
{
    rc522_t dev;
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &mock_spi), RC522_OK);

    /* Reset values from pp.52-59, none of which the driver sets */
    ASSERT_EQ_HEX(hw_regs[0x17], 0x84);
    ASSERT_EQ_HEX(hw_regs[0x19], 0x4D);
    ASSERT_EQ_HEX(hw_regs[0x24], 0x26);
    ASSERT_EQ_HEX(hw_regs[0x26], 0x48);
}

TEST_MAIN("rc522")
    RUN(test_init_ok);
    RUN(test_init_version_v1);
    RUN(test_init_bad_version);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_get_version);
    RUN(test_antenna_on);
    RUN(test_antenna_on_null);
    RUN(test_detect_card);
    RUN(test_detect_card_null);
    RUN(test_detect_card_timeout);
    RUN(test_read_uid);
    RUN(test_read_uid_bad_bcc);
    RUN(test_read_uid_null);
    RUN(test_halt);
    RUN(test_halt_null);
    RUN(test_self_test_null);
    RUN(test_self_test_ok);
    RUN(test_self_test_all_zero_fails);
    RUN(test_self_test_timeout);
    RUN(test_self_test_uninit_dev);
    RUN(test_transceive_collision_error);
    RUN(test_transceive_pure_collision_error);
    RUN(test_transceive_parity_error);
    RUN(test_transceive_buffer_overflow_error);
    RUN(test_init_switches_the_field_on);
    RUN(test_self_test_restores_the_configuration);
    RUN(test_self_test_refuses_a_wrong_signature);
    RUN(test_detect_card_needs_the_field);
    RUN(test_self_test_version_1);
    RUN(test_init_leaves_the_rest_at_reset);
TEST_MAIN_END()
