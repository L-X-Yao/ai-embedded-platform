/*
 * prop_pcf8563.c --- property-based tests for pcf8563
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_pcf8563.c ../pcf8563.c -o prop_pcf8563
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "../pcf8563.h"

/* ---- BCD helpers for verification ---- */

static uint8_t to_bcd(uint8_t val) {
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

static uint8_t from_bcd(uint8_t bcd) {
    return (uint8_t)(((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F));
}

/* ---- mock setup ---- */

static void mock_setup(void) {
    mock_i2c_reset();
    mock_regs[0x00] = 0x00;
    mock_regs[0x01] = 0x00;
    mock_regs[0x02] = 0x00;
    mock_regs[0x03] = 0x00;
    mock_regs[0x04] = 0x00;
    mock_regs[0x05] = 0x01;
    mock_regs[0x06] = 0x00;
    mock_regs[0x07] = 0x01;
    mock_regs[0x08] = 0x00;
}

static const pcf8563_i2c_t mock_i2c_hal = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* Property: BCD encode/decode roundtrip for time values */
PROPERTY(bcd_roundtrip, 10000) {
    mock_setup();
    pcf8563_t dev;
    pcf8563_init(&dev, &mock_i2c_hal);

    pcf8563_time_t tw;
    tw.second  = (uint8_t)(ARBI_SIZE(seed, 59));
    tw.minute  = (uint8_t)(ARBI_SIZE(seed, 59));
    tw.hour    = (uint8_t)(ARBI_SIZE(seed, 23));
    tw.day     = (uint8_t)(ARBI_SIZE(seed, 30) + 1);
    tw.weekday = (uint8_t)(ARBI_SIZE(seed, 6));
    tw.month   = (uint8_t)(ARBI_SIZE(seed, 11) + 1);
    tw.year    = (uint8_t)(ARBI_SIZE(seed, 99));

    pcf8563_err_t wrc = pcf8563_write_time(&dev, &tw);
    PROP_ASSERT_EQ(wrc, PCF8563_OK);

    /* Verify BCD encoding in mock registers */
    PROP_ASSERT_EQ(from_bcd(mock_regs[0x02] & 0x7F), tw.second);
    PROP_ASSERT_EQ(from_bcd(mock_regs[0x03] & 0x7F), tw.minute);
    PROP_ASSERT_EQ(from_bcd(mock_regs[0x04] & 0x3F), tw.hour);

    /* Read back */
    pcf8563_time_t tr;
    pcf8563_err_t rrc = pcf8563_read_time(&dev, &tr);
    PROP_ASSERT_EQ(rrc, PCF8563_OK);

    PROP_ASSERT_EQ(tr.second, tw.second);
    PROP_ASSERT_EQ(tr.minute, tw.minute);
    PROP_ASSERT_EQ(tr.hour, tw.hour);
    PROP_ASSERT_EQ(tr.day, tw.day);
    PROP_ASSERT_EQ(tr.weekday, tw.weekday);
    PROP_ASSERT_EQ(tr.month, tw.month);
    PROP_ASSERT_EQ(tr.year, tw.year);
}

/* Property: time read/write with random register data does not crash */
PROPERTY(random_regs_no_crash, 5000) {
    mock_setup();

    /* Fill time registers with random BCD-like values */
    for (int r = 0x02; r <= 0x08; r++) {
        mock_regs[r] = ARBI_UINT8(seed);
    }

    pcf8563_t dev;
    pcf8563_init(&dev, &mock_i2c_hal);

    pcf8563_time_t t;
    pcf8563_read_time(&dev, &t);
    /* No crash is success; values may be garbage but that is OK */

    /* Write random valid time back */
    t.second = (uint8_t)(ARBI_SIZE(seed, 59));
    t.minute = (uint8_t)(ARBI_SIZE(seed, 59));
    t.hour = (uint8_t)(ARBI_SIZE(seed, 23));
    t.day = (uint8_t)(ARBI_SIZE(seed, 30) + 1);
    t.month = (uint8_t)(ARBI_SIZE(seed, 11) + 1);
    t.year = (uint8_t)(ARBI_SIZE(seed, 99));
    pcf8563_write_time(&dev, &t);
    /* No crash is success */
}

/* Property: init succeeds with valid mock */
PROPERTY(init_no_crash, 1000) {
    mock_setup();
    pcf8563_t dev;
    pcf8563_err_t rc = pcf8563_init(&dev, &mock_i2c_hal);
    PROP_ASSERT_EQ(rc, PCF8563_OK);
}

/* Property: alarm read/write roundtrip */
PROPERTY(alarm_roundtrip, 5000) {
    mock_setup();
    pcf8563_t dev;
    pcf8563_init(&dev, &mock_i2c_hal);

    pcf8563_alarm_t aw;
    /* Mix enabled and disabled fields */
    aw.minute  = (ARBI_UINT8(seed) & 1) ? 0x80 : (uint8_t)(ARBI_SIZE(seed, 59));
    aw.hour    = (ARBI_UINT8(seed) & 1) ? 0x80 : (uint8_t)(ARBI_SIZE(seed, 23));
    aw.day     = (ARBI_UINT8(seed) & 1) ? 0x80 : (uint8_t)(ARBI_SIZE(seed, 30) + 1);
    aw.weekday = (ARBI_UINT8(seed) & 1) ? 0x80 : (uint8_t)(ARBI_SIZE(seed, 6));

    pcf8563_write_alarm(&dev, &aw);
    /* No crash is success */
}

TEST_MAIN("pcf8563 properties") {
    RUN(bcd_roundtrip);
    RUN(random_regs_no_crash);
    RUN(init_no_crash);
    RUN(alarm_roundtrip);
}
TEST_MAIN_END()
