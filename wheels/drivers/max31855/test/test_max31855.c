// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <math.h>
#include "max31855.h"

/* ---------- mock SPI ---------- */

static uint8_t mock_frame[4];   /* bytes returned by xfer in order */
static int     mock_xfer_idx;
static uint8_t mock_cs_state;

static void mock_reset(void)
{
    memset(mock_frame, 0, sizeof(mock_frame));
    mock_xfer_idx = 0;
    mock_cs_state = 1;
}

static void mock_cs_low(void *ctx)  { (void)ctx; mock_cs_state = 0; }
static void mock_cs_high(void *ctx) { (void)ctx; mock_cs_state = 1; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx; (void)tx;
    if (mock_xfer_idx < 4)
        return mock_frame[mock_xfer_idx++];
    return 0x00;
}

static const max31855_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* Helper: set the 32-bit frame bytes from a uint32_t value */
static void set_frame(uint32_t raw)
{
    mock_frame[0] = (uint8_t)(raw >> 24);
    mock_frame[1] = (uint8_t)(raw >> 16);
    mock_frame[2] = (uint8_t)(raw >> 8);
    mock_frame[3] = (uint8_t)(raw);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    max31855_t dev;
    max31855_err_t e = max31855_init(&dev, &mock_spi);
    ASSERT_EQ(e, MAX31855_OK);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(max31855_init(NULL, &mock_spi), MAX31855_ERR_PARAM);
}

TEST(test_init_null_spi)
{
    max31855_t dev;
    ASSERT_EQ(max31855_init(&dev, NULL), MAX31855_ERR_PARAM);
}

TEST(test_init_incomplete_spi)
{
    max31855_t dev;
    max31855_spi_t bad = { .cs_low = mock_cs_low, .cs_high = NULL, .xfer = mock_xfer, .ctx = NULL };
    ASSERT_EQ(max31855_init(&dev, &bad), MAX31855_ERR_SPI);
}

TEST(test_read_normal_positive)
{
    /* +25.00 C thermocouple: 100 * 4 = 0x190 in bits[31:18]
     * +24.0625 C internal: 385 in bits[15:4]
     * Frame: (0x190 << 18) | (385 << 4) = 0x06400180 + 0x1810 ... let's compute:
     * TC=100 (25.00C): bits[31:18] = 0b00000001_100100 = 0x064
     *   shifted << 18 = 0x01900000
     * Internal=385 (24.0625C): bits[15:4] = 0x181
     *   shifted << 4 = 0x1810
     * No fault: bit 16=0, bits[2:0]=0
     * Raw = 0x01901810
     */
    mock_reset();
    set_frame(0x01901810U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = 0.0f, ic = 0.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_OK);
    ASSERT_TRUE(fabsf(tc - 25.00f) < 0.01f);
    ASSERT_TRUE(fabsf(ic - 24.0625f) < 0.01f);
}

TEST(test_read_negative_thermocouple)
{
    /* -25.00 C thermocouple: -100 in 14-bit signed = 0x3F9C
     * bits[31:18] = 0x3F9C, shifted << 18 = 0xFE700000
     * Internal = 0 (0 C): bits[15:4] = 0x000
     * Raw = 0xFE700000
     */
    mock_reset();
    set_frame(0xFE700000U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = 0.0f, ic = 0.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_OK);
    ASSERT_TRUE(fabsf(tc - (-25.00f)) < 0.01f);
    ASSERT_TRUE(fabsf(ic - 0.0f) < 0.01f);
}

TEST(test_read_internal_temp)
{
    /* TC = 0 C (bits[31:18] = 0)
     * Internal = -10.0625 C: -161 in 12-bit signed = 0xF5F
     * bits[15:4] = 0xF5F, shifted << 4 = 0xF5F0
     * Raw = 0x0000F5F0
     */
    mock_reset();
    set_frame(0x0000F5F0U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = 0.0f, ic = 0.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_OK);
    ASSERT_TRUE(fabsf(tc - 0.0f) < 0.01f);
    ASSERT_TRUE(fabsf(ic - (-10.0625f)) < 0.01f);
}

TEST(test_fault_open_circuit)
{
    /* p.9: "If T+ and T- are unconnected, the thermocouple temperature sign
     * bit (D31) is 0, and the remainder of the thermocouple temperature value
     * (D[30:18]) is 1." Fault bit [16] = 1, internal 25 C, OC bit [0] = 1 */
    mock_reset();
    set_frame(0x7FFD1901U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = -1.0f, ic = -1.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_ERR_OPEN);
    ASSERT_TRUE(fabsf(tc - (-1.0f)) < 0.01f);  /* no 2047.75 C reaches the caller */
    ASSERT_TRUE(fabsf(ic - (-1.0f)) < 0.01f);
}

TEST(test_fault_short_gnd)
{
    /* Fault bit [16] = 1, SCG bit [1] = 1 */
    mock_reset();
    set_frame(0x00010002U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = 0.0f, ic = 0.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_ERR_SHORT_GND);
}

TEST(test_fault_short_vcc)
{
    /* Fault bit [16] = 1, SCV bit [2] = 1 */
    mock_reset();
    set_frame(0x00010004U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = 0.0f, ic = 0.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_ERR_SHORT_VCC);
}

/* D16 only reads 1 with a fault in D2..D0, so alone it marks a corrupted frame */
TEST(test_fault_bit_alone_is_a_bad_frame)
{
    mock_reset();
    set_frame(0x00010000U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = -999.0f, ic = -999.0f;
    ASSERT_EQ(max31855_read(&dev, &tc, &ic), MAX31855_ERR_SPI);
    ASSERT_TRUE(tc == -999.0f);
    ASSERT_TRUE(ic == -999.0f);
}

TEST(test_read_raw)
{
    mock_reset();
    set_frame(0xDEADBEEFU);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    uint32_t raw = 0;
    max31855_err_t e = max31855_read_raw(&dev, &raw);
    ASSERT_EQ(e, MAX31855_OK);
    ASSERT_EQ(raw, 0xDEADBEEFU);
}

TEST(test_read_raw_null_out)
{
    mock_reset();
    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    ASSERT_EQ(max31855_read_raw(&dev, NULL), MAX31855_ERR_PARAM);
}

TEST(test_read_null_dev)
{
    float tc = 0.0f, ic = 0.0f;
    ASSERT_EQ(max31855_read(NULL, &tc, &ic), MAX31855_ERR_PARAM);
}

TEST(test_read_zero_temp)
{
    /* All zeros: TC=0C, internal=0C, no fault */
    mock_reset();
    set_frame(0x00000000U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = 99.0f, ic = 99.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_OK);
    ASSERT_TRUE(fabsf(tc) < 0.01f);
    ASSERT_TRUE(fabsf(ic) < 0.01f);
}

TEST(test_read_max_positive_temp)
{
    /* TC = +1023.75 C = 4095 counts = 0x0FFF
     * bits[31:18] = 0x0FFF << 18 = 0x3FFC0000
     * Internal = +127.9375 C = 2047 counts = 0x7FF
     * bits[15:4] = 0x7FF << 4 = 0x7FF0
     * bits[17:16] = 0 (no fault), bits[2:0] = 0
     * Raw = 0x3FFC0000 | 0x7FF0 = 0x3FFC7FF0
     */
    mock_reset();
    set_frame(0x3FFC7FF0U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    float tc = 0.0f, ic = 0.0f;
    max31855_err_t e = max31855_read(&dev, &tc, &ic);
    ASSERT_EQ(e, MAX31855_OK);
    ASSERT_TRUE(fabsf(tc - 1023.75f) < 0.01f);
    ASSERT_TRUE(fabsf(ic - 127.9375f) < 0.01f);
}

TEST(test_read_optional_null_outputs)
{
    /* Both output pointers NULL — should still succeed without fault */
    mock_reset();
    set_frame(0x01901810U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    max31855_err_t e = max31855_read(&dev, NULL, NULL);
    ASSERT_EQ(e, MAX31855_OK);
}

TEST(test_cs_toggle)
{
    /* Verify CS goes low then high during read */
    mock_reset();
    set_frame(0x00000000U);

    max31855_t dev;
    max31855_init(&dev, &mock_spi);

    ASSERT_EQ(mock_cs_state, 1);  /* CS starts high */
    uint32_t raw = 0;
    max31855_read_raw(&dev, &raw);
    ASSERT_EQ(mock_cs_state, 1);  /* CS ends high after read */
}

/* ---------- a device init refused ---------- */

static void assert_device_refused(max31855_t *d)
{
    float tc = 0.0f, ic = 0.0f;
    uint32_t raw = 0;
    ASSERT_EQ(max31855_read(d, &tc, &ic), MAX31855_ERR_PARAM);
    ASSERT_EQ(max31855_read_raw(d, &raw), MAX31855_ERR_PARAM);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    max31855_t d;
    max31855_spi_t zeroed;
    memset(&zeroed, 0, sizeof(zeroed));

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(max31855_init(&d, NULL), MAX31855_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(max31855_init(&d, &zeroed), MAX31855_ERR_SPI);
    assert_device_refused(&d);
}

TEST_MAIN("max31855")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_spi);
    RUN(test_init_incomplete_spi);
    RUN(test_read_normal_positive);
    RUN(test_read_negative_thermocouple);
    RUN(test_read_internal_temp);
    RUN(test_fault_open_circuit);
    RUN(test_fault_short_gnd);
    RUN(test_fault_short_vcc);
    RUN(test_fault_bit_alone_is_a_bad_frame);
    RUN(test_read_raw);
    RUN(test_read_raw_null_out);
    RUN(test_read_null_dev);
    RUN(test_read_zero_temp);
    RUN(test_read_max_positive_temp);
    RUN(test_read_optional_null_outputs);
    RUN(test_cs_toggle);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
