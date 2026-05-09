#include "test_harness.h"
#include "hw_model_rc522.h"
#include "rc522.h"

static const rc522_spi_t hw_spi = {
    .cs_low   = hw_model_cs_low,
    .cs_high  = hw_model_cs_high,
    .xfer     = hw_model_xfer,
    .delay_ms = hw_model_delay_ms,
    .ctx      = NULL,
};

static rc522_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x92);
}

TEST(test_hw_init_v1)
{
    hw_model_reset();
    hw_regs[0x37] = RC522_VERSION_1_0;
    ASSERT_EQ(rc522_init(&dev, &hw_spi), RC522_OK);
    ASSERT_EQ(dev.version, 0x91);
}

TEST(test_hw_detect_card_with_atqa)
{
    hw_model_reset();
    rc522_init(&dev, &hw_spi);

    /* Inject ATQA into FIFO and set RxIRq */
    hw_model_inject_atqa(0x04, 0x00);

    ASSERT_EQ(rc522_detect_card(&dev), RC522_OK);
}

TEST(test_hw_read_uid)
{
    hw_model_reset();
    rc522_init(&dev, &hw_spi);

    /* Inject UID response */
    hw_model_inject_uid(0xCA, 0xFE, 0xBA, 0xBE);

    rc522_uid_t uid;
    ASSERT_EQ(rc522_read_card_uid(&dev, &uid), RC522_OK);
    ASSERT_EQ(uid.len, 4);
    ASSERT_EQ(uid.bytes[0], 0xCA);
    ASSERT_EQ(uid.bytes[1], 0xFE);
    ASSERT_EQ(uid.bytes[2], 0xBA);
    ASSERT_EQ(uid.bytes[3], 0xBE);
}

TEST_MAIN("rc522_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_v1);
    RUN(test_hw_detect_card_with_atqa);
    RUN(test_hw_read_uid);
TEST_MAIN_END()
