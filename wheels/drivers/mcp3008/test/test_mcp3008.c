// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mcp3008.h"
#include "hw_model_mcp3008.h"

/* ---------- model-backed mock SPI ---------- */

static mcp3008_model_t model;

static const mcp3008_spi_t mock_spi = {
    .ctx         = &model,
    .cs_select   = mcp3008_model_cs_select,
    .cs_deselect = mcp3008_model_cs_deselect,
    .transfer    = mcp3008_model_transfer,
};

static mcp3008_t dev;

static void setup(void)
{
    mcp3008_model_init(&model);
    (void)mcp3008_init(&dev, &mock_spi);
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mcp3008_model_init(&model);
    mcp3008_t d;
    mcp3008_err_t e = mcp3008_init(&d, &mock_spi);
    ASSERT_EQ(e, MCP3008_OK);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(mcp3008_init(NULL, &mock_spi), MCP3008_ERR_PARAM);
}

TEST(test_init_null_spi)
{
    mcp3008_t d;
    ASSERT_EQ(mcp3008_init(&d, NULL), MCP3008_ERR_PARAM);
}

TEST(test_init_null_spi_fn)
{
    mcp3008_t d;
    mcp3008_spi_t bad = mock_spi;

    bad.cs_select = NULL;
    ASSERT_EQ(mcp3008_init(&d, &bad), MCP3008_ERR_PARAM);

    bad = mock_spi;
    bad.cs_deselect = NULL;
    ASSERT_EQ(mcp3008_init(&d, &bad), MCP3008_ERR_PARAM);

    bad = mock_spi;
    bad.transfer = NULL;
    ASSERT_EQ(mcp3008_init(&d, &bad), MCP3008_ERR_PARAM);
}

/* ---------- single-ended read tests ---------- */

TEST(test_read_single_ch0)
{
    setup();
    mcp3008_model_set_single(&model, 0, 512);

    uint16_t raw;
    mcp3008_err_t e = mcp3008_read_single(&dev, 0, &raw);
    ASSERT_EQ(e, MCP3008_OK);
    ASSERT_EQ(raw, 512);
}

TEST(test_read_single_all_channels)
{
    setup();
    for (uint8_t ch = 0; ch < 8; ch++)
    {
        mcp3008_model_set_single(&model, ch, (uint16_t)(ch * 100 + 50));
    }

    for (uint8_t ch = 0; ch < 8; ch++)
    {
        uint16_t raw;
        mcp3008_err_t e = mcp3008_read_single(&dev, ch, &raw);
        ASSERT_EQ(e, MCP3008_OK);
        ASSERT_EQ(raw, ch * 100 + 50);
    }
}

TEST(test_read_single_max_value)
{
    setup();
    mcp3008_model_set_single(&model, 0, 1023);

    uint16_t raw;
    mcp3008_err_t e = mcp3008_read_single(&dev, 0, &raw);
    ASSERT_EQ(e, MCP3008_OK);
    ASSERT_EQ(raw, 1023);
}

TEST(test_read_single_zero)
{
    setup();
    mcp3008_model_set_single(&model, 0, 0);

    uint16_t raw;
    mcp3008_err_t e = mcp3008_read_single(&dev, 0, &raw);
    ASSERT_EQ(e, MCP3008_OK);
    ASSERT_EQ(raw, 0);
}

TEST(test_read_single_channel_out_of_range)
{
    setup();
    uint16_t raw;
    ASSERT_EQ(mcp3008_read_single(&dev, 8, &raw), MCP3008_ERR_PARAM);
    ASSERT_EQ(mcp3008_read_single(&dev, 255, &raw), MCP3008_ERR_PARAM);
}

/* ---------- differential read tests ---------- */

TEST(test_read_diff_pair0)
{
    setup();
    mcp3008_model_set_diff(&model, 0, 300);

    uint16_t raw;
    mcp3008_err_t e = mcp3008_read_diff(&dev, 0, &raw);
    ASSERT_EQ(e, MCP3008_OK);
    ASSERT_EQ(raw, 300);
}

TEST(test_read_diff_all_pairs)
{
    setup();
    for (uint8_t p = 0; p < 4; p++)
    {
        mcp3008_model_set_diff(&model, p, (uint16_t)(p * 200 + 100));
    }

    for (uint8_t p = 0; p < 4; p++)
    {
        uint16_t raw;
        mcp3008_err_t e = mcp3008_read_diff(&dev, p, &raw);
        ASSERT_EQ(e, MCP3008_OK);
        ASSERT_EQ(raw, p * 200 + 100);
    }
}

/* DS21295D Table 5-2, with SGL/DIFF D2 D1 D0 in the top nibble of byte 2
 * (Figure 6-1). The header promises pair 1 = CH2-CH3, 2 = CH4-CH5 and
 * 3 = CH6-CH7, even channel = IN+. */
TEST(test_read_diff_config_byte_per_pair)
{
    static const uint8_t expected[4] = {
        0x00u,  /* 0 0 0 0: CH0 = IN+, CH1 = IN- */
        0x20u,  /* 0 0 1 0: CH2 = IN+, CH3 = IN- */
        0x40u,  /* 0 1 0 0: CH4 = IN+, CH5 = IN- */
        0x60u,  /* 0 1 1 0: CH6 = IN+, CH7 = IN- */
    };

    setup();
    for (uint8_t p = 0; p < 4; p++)
    {
        uint16_t raw;
        model.last_config = 0xFFu;
        ASSERT_EQ(mcp3008_read_diff(&dev, p, &raw), MCP3008_OK);
        ASSERT_EQ_HEX(model.last_config, expected[p]);
    }
}

TEST(test_read_diff_pair_out_of_range)
{
    setup();
    uint16_t raw;
    ASSERT_EQ(mcp3008_read_diff(&dev, 4, &raw), MCP3008_ERR_PARAM);
    ASSERT_EQ(mcp3008_read_diff(&dev, 255, &raw), MCP3008_ERR_PARAM);
}

/* ---------- voltage conversion tests ---------- */

TEST(test_read_voltage_zero)
{
    setup();
    mcp3008_model_set_single(&model, 0, 0);

    float v;
    mcp3008_err_t e = mcp3008_read_voltage(&dev, 0, 3.3f, &v);
    ASSERT_EQ(e, MCP3008_OK);
    ASSERT_NEAR(v, 0.0f, 0.001f);
}

TEST(test_read_voltage_midscale)
{
    setup();
    mcp3008_model_set_single(&model, 0, 512);

    float v;
    mcp3008_err_t e = mcp3008_read_voltage(&dev, 0, 3.3f, &v);
    ASSERT_EQ(e, MCP3008_OK);
    /* DS21295D Equation 4-2: code = 1024 x VIN / VREF, so 512 is VREF / 2 */
    ASSERT_NEAR(v, 1.65f, 0.0005f);
}

TEST(test_read_voltage_fullscale)
{
    setup();
    mcp3008_model_set_single(&model, 0, 1023);

    float v;
    mcp3008_err_t e = mcp3008_read_voltage(&dev, 0, 3.3f, &v);
    ASSERT_EQ(e, MCP3008_OK);
    /* Equation 4-1: 1 LSB = VREF / 1024, and 3FFh sits 1 LSB below VREF */
    ASSERT_NEAR(v, 3.3f - 3.3f / 1024.0f, 0.0005f);
}

TEST(test_read_voltage_vref_zero)
{
    setup();
    float v;
    ASSERT_EQ(mcp3008_read_voltage(&dev, 0, 0.0f, &v), MCP3008_ERR_PARAM);
}

TEST(test_read_voltage_vref_negative)
{
    setup();
    float v;
    ASSERT_EQ(mcp3008_read_voltage(&dev, 0, -1.0f, &v), MCP3008_ERR_PARAM);
}

TEST(test_read_voltage_channel_out_of_range)
{
    setup();
    float v;
    ASSERT_EQ(mcp3008_read_voltage(&dev, 8,   3.3f, &v), MCP3008_ERR_PARAM);
    ASSERT_EQ(mcp3008_read_voltage(&dev, 255, 3.3f, &v), MCP3008_ERR_PARAM);
}

/* ---------- SPI failure tests ---------- */

TEST(test_spi_transfer_failure)
{
    setup();
    model.fail_transfer = true;

    uint16_t raw;
    ASSERT_EQ(mcp3008_read_single(&dev, 0, &raw), MCP3008_ERR_SPI);
}

TEST(test_spi_cs_failure)
{
    setup();
    model.fail_cs = true;

    uint16_t raw;
    ASSERT_EQ(mcp3008_read_single(&dev, 0, &raw), MCP3008_ERR_SPI);
}

/* ---------- NULL parameter tests ---------- */

TEST(test_read_single_null_dev)
{
    uint16_t raw;
    ASSERT_EQ(mcp3008_read_single(NULL, 0, &raw), MCP3008_ERR_PARAM);
}

TEST(test_read_single_null_out)
{
    setup();
    ASSERT_EQ(mcp3008_read_single(&dev, 0, NULL), MCP3008_ERR_PARAM);
}

TEST(test_read_diff_null_dev)
{
    uint16_t raw;
    ASSERT_EQ(mcp3008_read_diff(NULL, 0, &raw), MCP3008_ERR_PARAM);
}

TEST(test_read_diff_null_out)
{
    setup();
    ASSERT_EQ(mcp3008_read_diff(&dev, 0, NULL), MCP3008_ERR_PARAM);
}

TEST(test_read_voltage_null_dev)
{
    float v;
    ASSERT_EQ(mcp3008_read_voltage(NULL, 0, 3.3f, &v), MCP3008_ERR_PARAM);
}

TEST(test_read_voltage_null_out)
{
    setup();
    ASSERT_EQ(mcp3008_read_voltage(&dev, 0, 3.3f, NULL), MCP3008_ERR_PARAM);
}

/* ---------- a device init refused ---------- */

static void assert_device_refused(mcp3008_t *d)
{
    uint16_t raw = 0;
    float volts = 0.0f;
    ASSERT_EQ(mcp3008_read_single(d, 0, &raw), MCP3008_ERR_PARAM);
    ASSERT_EQ(mcp3008_read_diff(d, 0, &raw), MCP3008_ERR_PARAM);
    ASSERT_EQ(mcp3008_read_voltage(d, 0, 3.3f, &volts), MCP3008_ERR_PARAM);
}

TEST(test_zeroed_device_refused)
{
    static mcp3008_t zeroed;
    assert_device_refused(&zeroed);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    mcp3008_t d;
    mcp3008_spi_t zeroed;
    memset(&zeroed, 0, sizeof(zeroed));

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(mcp3008_init(&d, NULL), MCP3008_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(mcp3008_init(&d, &zeroed), MCP3008_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("mcp3008")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_null_spi_fn);
    RUN(test_read_single_ch0);
    RUN(test_read_single_all_channels);
    RUN(test_read_single_max_value);
    RUN(test_read_single_zero);
    RUN(test_read_single_channel_out_of_range);
    RUN(test_read_diff_pair0);
    RUN(test_read_diff_all_pairs);
    RUN(test_read_diff_config_byte_per_pair);
    RUN(test_read_diff_pair_out_of_range);
    RUN(test_read_voltage_zero);
    RUN(test_read_voltage_midscale);
    RUN(test_read_voltage_fullscale);
    RUN(test_read_voltage_vref_zero);
    RUN(test_read_voltage_vref_negative);
    RUN(test_read_voltage_channel_out_of_range);
    RUN(test_spi_transfer_failure);
    RUN(test_spi_cs_failure);
    RUN(test_read_single_null_dev);
    RUN(test_read_single_null_out);
    RUN(test_read_diff_null_dev);
    RUN(test_read_diff_null_out);
    RUN(test_read_voltage_null_dev);
    RUN(test_read_voltage_null_out);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
