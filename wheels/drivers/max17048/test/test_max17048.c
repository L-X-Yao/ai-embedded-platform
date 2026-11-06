// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "max17048.h"

#define REG_VCELL   0x02
#define REG_SOC     0x04
#define REG_MODE    0x06
#define REG_VERSION 0x08
#define REG_CONFIG  0x0C
#define REG_CRATE   0x16
#define REG_STATUS  0x1A

/*
 * The mock chip. Every I2C write lands in bus_log as the exact bytes after
 * the address, whichever HAL callback carried them, so a test sees what the
 * chip would see. Maxim 19-6171 Rev 7 p10: "8-bit writes cause no effect".
 * p17: "A valid write must include both register bytes". A register therefore
 * changes only on a write of MAddr, Data0, Data1.
 */
#define BUS_LOG_MAX 8
#define BUS_TX_MAX  4

typedef struct {
    uint8_t  addr;
    uint16_t len;
    uint8_t  data[BUS_TX_MAX];
} bus_tx_t;

static bus_tx_t bus_log[BUS_LOG_MAX];
static int      bus_count;

static void bus_clear(void)
{
    memset(bus_log, 0, sizeof(bus_log));
    bus_count = 0;
}

static bool bus_write(uint8_t dev_addr, const uint8_t *data, uint16_t len)
{
    if (mock_i2c_should_fail()) return false;

    if (bus_count < BUS_LOG_MAX) {
        bus_tx_t *tx = &bus_log[bus_count];
        tx->addr = dev_addr;
        tx->len  = len;
        memcpy(tx->data, data, len < BUS_TX_MAX ? len : BUS_TX_MAX);
    }
    bus_count++;

    if (len == 3) {
        mock_regs[data[0]]                = data[1];
        mock_regs[(uint8_t)(data[0] + 1)] = data[2];
    }
    return true;
}

static bool bus_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    const uint8_t tx[2] = { reg, val };

    (void)ctx;
    return bus_write(dev_addr, tx, 2);
}

static bool bus_write_raw(void *ctx, uint8_t dev_addr, const uint8_t *data,
                          uint16_t len)
{
    (void)ctx;
    return bus_write(dev_addr, data, len);
}

/* Assert that write transaction i was exactly these bytes */
#define ASSERT_TX(i, ...) do { \
    static const uint8_t want_[] = { __VA_ARGS__ }; \
    ASSERT_TRUE((i) < bus_count); \
    ASSERT_EQ(bus_log[i].len, sizeof(want_)); \
    ASSERT_MEMEQ_N(bus_log[i].data, want_, sizeof(want_)); \
} while (0)

static const max17048_i2c_t mock_bus = {
    .write_reg = bus_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
    .write_raw = bus_write_raw,
};

static max17048_t dev;

static void mock_reset_chip(void)
{
    mock_i2c_reset();
    bus_clear();
    mock_regs[REG_VERSION]     = 0x00;
    mock_regs[REG_VERSION + 1] = 0x11;
    mock_regs[REG_CONFIG]      = 0x97;
    mock_regs[REG_CONFIG + 1]  = 0x1C;
}

/* ─── init tests ─────────────────────────────────────────────── */

TEST(test_init_ok)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
}

TEST(test_init_null)
{
    ASSERT_EQ(max17048_init(NULL, &mock_bus), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_init(&dev, NULL), MAX17048_ERR_PARAM);
    max17048_i2c_t bad = { .read_regs = mock_i2c_read_regs, .write_raw = NULL };
    ASSERT_EQ(max17048_init(&dev, &bad), MAX17048_ERR_PARAM);
    max17048_i2c_t bad2 = { .read_regs = NULL, .write_raw = bus_write_raw };
    ASSERT_EQ(max17048_init(&dev, &bad2), MAX17048_ERR_PARAM);
}

TEST(test_init_refuses_a_hal_without_write_raw)
{
    max17048_i2c_t hal = mock_bus;
    max17048_t d;

    hal.write_raw = NULL;
    mock_reset_chip();
    mock_regs[REG_STATUS] = 0x01;  /* RI set, so init would have to write */
    ASSERT_EQ(max17048_init(&d, &hal), MAX17048_ERR_PARAM);
    /* nothing on the bus at all, reads included */
    ASSERT_EQ(mock_op_count, 0);
    ASSERT_EQ(bus_count, 0);
}

TEST(test_init_needs_no_write_reg)
{
    max17048_i2c_t hal = mock_bus;
    max17048_t d;

    hal.write_reg = NULL;
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&d, &hal), MAX17048_OK);
    ASSERT_EQ(max17048_quickstart(&d), MAX17048_OK);
    ASSERT_TX(0, 0x06, 0x40, 0x00);
}

TEST(test_init_i2c_fail)
{
    mock_reset_chip();
    mock_fail_at = 0;
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_ERR_I2C);
}

TEST(test_init_bad_version_zero)
{
    mock_i2c_reset();
    mock_regs[REG_VERSION] = 0x00;
    mock_regs[REG_VERSION + 1] = 0x00;
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_ERR_ID);
}

TEST(test_init_bad_version_ffff)
{
    mock_i2c_reset();
    mock_regs[REG_VERSION] = 0xFF;
    mock_regs[REG_VERSION + 1] = 0xFF;
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_ERR_ID);
}

/* ─── read tests ─────────────────────────────────────────────── */

TEST(test_read_full)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);

    mock_regs[REG_VCELL]     = 0xA0;
    mock_regs[REG_VCELL + 1] = 0x00;
    mock_regs[REG_SOC]       = 0x32;
    mock_regs[REG_SOC + 1]   = 0x80;
    mock_regs[REG_CRATE]     = 0x00;
    mock_regs[REG_CRATE + 1] = 0x00;

    max17048_data_t data;
    ASSERT_EQ(max17048_read(&dev, &data), MAX17048_OK);
    ASSERT_NEAR(data.voltage_v, (float)0xA000 * 78.125e-6f, 0.01f);
    ASSERT_NEAR(data.soc_pct, 50.5f, 0.01f);
    ASSERT_NEAR(data.charge_rate, 0.0f, 0.01f);
}

TEST(test_read_null)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    max17048_data_t data;
    ASSERT_EQ(max17048_read(NULL, &data), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_read(&dev, NULL), MAX17048_ERR_PARAM);
}

TEST(test_read_i2c_fail_vcell)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_fail_at = (int)mock_op_count;
    max17048_data_t data;
    ASSERT_EQ(max17048_read(&dev, &data), MAX17048_ERR_I2C);
}

TEST(test_read_voltage)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_regs[REG_VCELL]     = 0x80;
    mock_regs[REG_VCELL + 1] = 0x00;
    float v;
    ASSERT_EQ(max17048_read_voltage(&dev, &v), MAX17048_OK);
    ASSERT_NEAR(v, (float)0x8000 * 78.125e-6f, 0.01f);
}

TEST(test_read_voltage_null)
{
    ASSERT_EQ(max17048_read_voltage(NULL, NULL), MAX17048_ERR_PARAM);
}

TEST(test_read_soc)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_regs[REG_SOC]     = 0x64;
    mock_regs[REG_SOC + 1] = 0x00;
    float soc;
    ASSERT_EQ(max17048_read_soc(&dev, &soc), MAX17048_OK);
    ASSERT_NEAR(soc, 100.0f, 0.01f);
}

TEST(test_read_soc_null)
{
    ASSERT_EQ(max17048_read_soc(NULL, NULL), MAX17048_ERR_PARAM);
}

/* ─── quickstart test ────────────────────────────────────────── */

TEST(test_quickstart)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_quickstart(&dev), MAX17048_OK);
    ASSERT_EQ(mock_regs[REG_MODE], 0x40);
}

TEST(test_quickstart_null)
{
    ASSERT_EQ(max17048_quickstart(NULL), MAX17048_ERR_PARAM);
}

/* ─── alert threshold tests ──────────────────────────────────── */

TEST(test_set_alert_threshold)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 10), MAX17048_OK);
    ASSERT_EQ(mock_regs[REG_CONFIG + 1] & 0x1F, 22);
}

TEST(test_set_alert_threshold_bounds)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 0), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 33), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 1), MAX17048_OK);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 32), MAX17048_OK);
}

TEST(test_set_alert_null)
{
    ASSERT_EQ(max17048_set_alert_threshold(NULL, 10), MAX17048_ERR_PARAM);
}

/* ─── clear alert test ───────────────────────────────────────── */

TEST(test_clear_alert)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    mock_regs[REG_CONFIG] = 0x97;
    mock_regs[REG_CONFIG + 1] = 0x3C;
    ASSERT_EQ(max17048_clear_alert(&dev), MAX17048_OK);
    ASSERT_EQ(mock_regs[REG_CONFIG + 1] & 0x20, 0);
}

TEST(test_clear_alert_null)
{
    ASSERT_EQ(max17048_clear_alert(NULL), MAX17048_ERR_PARAM);
}

/* ─── sleep / wake tests ─────────────────────────────────────── */

TEST(test_sleep_wake)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
    ASSERT_TRUE(mock_regs[REG_CONFIG + 1] & 0x80);
    ASSERT_EQ(max17048_wake(&dev), MAX17048_OK);
    ASSERT_FALSE(mock_regs[REG_CONFIG + 1] & 0x80);
}

TEST(test_sleep_null)
{
    ASSERT_EQ(max17048_sleep(NULL), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_wake(NULL), MAX17048_ERR_PARAM);
}

/* ─── version test ───────────────────────────────────────────── */

TEST(test_version)
{
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    uint16_t ver;
    ASSERT_EQ(max17048_version(&dev, &ver), MAX17048_OK);
    ASSERT_EQ(ver, 0x0011);
}

TEST(test_version_null)
{
    ASSERT_EQ(max17048_version(NULL, NULL), MAX17048_ERR_PARAM);
    mock_reset_chip();
    max17048_init(&dev, &mock_bus);
    ASSERT_EQ(max17048_version(&dev, NULL), MAX17048_ERR_PARAM);
}

TEST(test_sleep_sets_ensleep_before_sleep)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);

    ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
    /* MODE.EnSleep is bit 13, CONFIG.SLEEP is bit 7 */
    ASSERT_EQ(mock_regs[0x06] & 0x20, 0x20);
    ASSERT_EQ(mock_regs[REG_CONFIG + 1] & 0x80, 0x80);
}

TEST(test_clear_alert_clears_the_status_flags)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);

    /* EnVR stays set, the SC and VL flags go */
    mock_regs[0x1A] = 0x64;
    ASSERT_EQ(max17048_clear_alert(&dev), MAX17048_OK);
    ASSERT_EQ_HEX(mock_regs[0x1A], 0x40);
}

TEST(test_init_clears_the_reset_indicator)
{
    mock_reset_chip();
    mock_regs[0x1A] = 0x41;  /* EnVR and RI */
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    ASSERT_EQ_HEX(mock_regs[0x1A], 0x40);
}

/* ─── exact bus traffic: register, MSB, LSB in one transaction ── */

TEST(test_quickstart_is_one_word_write)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    ASSERT_EQ(max17048_quickstart(&dev), MAX17048_OK);
    /* p11: Quick-Start is MODE bit 14. p16: MSB first, at the even address */
    ASSERT_EQ(bus_count, 1);
    ASSERT_TX(0, 0x06, 0x40, 0x00);
    ASSERT_EQ(bus_log[0].addr, 0x36);
}

TEST(test_alert_threshold_is_one_word_write)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    ASSERT_EQ(max17048_set_alert_threshold(&dev, 10), MAX17048_OK);
    /* p12: the value is (32 - ATHD)%, so 10% is ATHD 22, under RCOMP 0x97 */
    ASSERT_EQ(bus_count, 1);
    ASSERT_TX(0, 0x0C, 0x97, 0x16);
}

TEST(test_clear_alert_is_two_word_writes)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    mock_regs[REG_STATUS]     = 0x64;  /* EnVR, SC, VL */
    mock_regs[REG_CONFIG + 1] = 0x3C;  /* ALRT set */
    ASSERT_EQ(max17048_clear_alert(&dev), MAX17048_OK);
    /* p12, p14: clear the cause in STATUS, keep EnVR, then clear ALRT */
    ASSERT_EQ(bus_count, 2);
    ASSERT_TX(0, 0x1A, 0x40, 0x00);
    ASSERT_TX(1, 0x0C, 0x97, 0x1C);
}

TEST(test_sleep_is_two_word_writes)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    ASSERT_EQ(max17048_sleep(&dev), MAX17048_OK);
    /* p10, p12: MODE.EnSleep is bit 13, then CONFIG.SLEEP is bit 7 */
    ASSERT_EQ(bus_count, 2);
    ASSERT_TX(0, 0x06, 0x20, 0x00);
    ASSERT_TX(1, 0x0C, 0x97, 0x9C);
}

TEST(test_wake_is_one_word_write)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    mock_regs[REG_CONFIG + 1] = 0x9C;  /* SLEEP set */
    ASSERT_EQ(max17048_wake(&dev), MAX17048_OK);
    ASSERT_EQ(bus_count, 1);
    ASSERT_TX(0, 0x0C, 0x97, 0x1C);
}

TEST(test_init_clears_ri_with_one_word_write)
{
    mock_reset_chip();
    mock_regs[REG_STATUS]     = 0x41;  /* EnVR and RI */
    mock_regs[REG_STATUS + 1] = 0xA5;  /* don't-care low byte goes back as read */
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    ASSERT_EQ(bus_count, 1);
    ASSERT_TX(0, 0x1A, 0x40, 0xA5);
}

TEST(test_init_without_ri_writes_nothing)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    ASSERT_EQ(bus_count, 0);
}

TEST(test_write_i2c_fail)
{
    mock_reset_chip();
    ASSERT_EQ(max17048_init(&dev, &mock_bus), MAX17048_OK);
    mock_fail_next = 1;
    ASSERT_EQ(max17048_quickstart(&dev), MAX17048_ERR_I2C);
    ASSERT_EQ(bus_count, 0);
}

/* ---------- a device init did not finish ---------- */

static void assert_device_refused(max17048_t *d)
{
    max17048_data_t out;
    float f;
    uint16_t ver;
    ASSERT_EQ(max17048_read(d, &out), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_read_voltage(d, &f), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_read_soc(d, &f), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_quickstart(d), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_set_alert_threshold(d, 10), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_clear_alert(d), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_sleep(d), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_wake(d), MAX17048_ERR_PARAM);
    ASSERT_EQ(max17048_version(d, &ver), MAX17048_ERR_PARAM);
}

TEST(test_zeroed_device_refused)
{
    static max17048_t zeroed;
    assert_device_refused(&zeroed);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_failed_init)
{
    max17048_t d;
    mock_reset_chip();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(max17048_init(&d, NULL), MAX17048_ERR_PARAM);
    assert_device_refused(&d);

    static max17048_i2c_t zeroed_hal;
    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(max17048_init(&d, &zeroed_hal), MAX17048_ERR_PARAM);
    assert_device_refused(&d);

    /* No such part answers version 0xFFFF */
    mock_regs[REG_VERSION]     = 0xFF;
    mock_regs[REG_VERSION + 1] = 0xFF;
    ASSERT_EQ(max17048_init(&d, &mock_bus), MAX17048_ERR_ID);
    assert_device_refused(&d);

    /* The version read worked, the status read did not */
    mock_reset_chip();
    mock_fail_at = 1;
    ASSERT_EQ(max17048_init(&d, &mock_bus), MAX17048_ERR_I2C);
    mock_reset_chip();
    assert_device_refused(&d);
}

TEST_MAIN("max17048")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_i2c_fail);
    RUN(test_init_bad_version_zero);
    RUN(test_init_bad_version_ffff);
    RUN(test_read_full);
    RUN(test_read_null);
    RUN(test_read_i2c_fail_vcell);
    RUN(test_read_voltage);
    RUN(test_read_voltage_null);
    RUN(test_read_soc);
    RUN(test_read_soc_null);
    RUN(test_quickstart);
    RUN(test_quickstart_null);
    RUN(test_set_alert_threshold);
    RUN(test_set_alert_threshold_bounds);
    RUN(test_set_alert_null);
    RUN(test_clear_alert);
    RUN(test_clear_alert_null);
    RUN(test_sleep_wake);
    RUN(test_sleep_null);
    RUN(test_version);
    RUN(test_version_null);
    RUN(test_sleep_sets_ensleep_before_sleep);
    RUN(test_clear_alert_clears_the_status_flags);
    RUN(test_init_clears_the_reset_indicator);
    RUN(test_init_refuses_a_hal_without_write_raw);
    RUN(test_init_needs_no_write_reg);
    RUN(test_quickstart_is_one_word_write);
    RUN(test_alert_threshold_is_one_word_write);
    RUN(test_clear_alert_is_two_word_writes);
    RUN(test_sleep_is_two_word_writes);
    RUN(test_wake_is_one_word_write);
    RUN(test_init_clears_ri_with_one_word_write);
    RUN(test_init_without_ri_writes_nothing);
    RUN(test_write_i2c_fail);
    RUN(test_zeroed_device_refused);
    RUN(test_device_refused_after_failed_init);
TEST_MAIN_END()
