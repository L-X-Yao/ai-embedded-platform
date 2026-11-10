// SPDX-License-Identifier: MIT
/*
 * onewire -- 1-Wire bus protocol (Dallas/Maxim)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Timing reference (standard speed):
 *   Reset:   master pulls low >= 480us, releases, waits 15-60us for presence,
 *            presence pulse = device pulls low 60-240us, total >= 480us
 *   Write 0: pull low 60us, release, 2us recovery
 *   Write 1: pull low 6us, release for remainder of 60us slot, 2us recovery
 *   Read:    pull low 6us, release, sample at ~15us into slot, total >= 60us
 */

#include "onewire.h"
#include "contract.h"

/* ---------- timing constants (microseconds, standard speed) ---------- */

#define T_RSTL       480  /* reset low time                               */
#define T_RSTH       480  /* reset high (wait after release)               */
#define T_PDHIGH      60  /* wait before sampling presence                 */
#define T_WDLOW_0     60  /* write-0 low time                              */
#define T_WDLOW_1      6  /* write-1 low time                              */
#define T_SLOT        60  /* total time slot                               */
#define T_REC          2  /* recovery between slots                        */
#define T_RDLOW        6  /* read initiate low time                        */
#define T_RDSAMPLE     9  /* additional delay before sample (6+9 = 15us)   */
#define T_RDRELEASE   45  /* remainder of slot after sample                */
#define T_IDLE_STEP    2  /* poll step while waiting for the line to rise  */
#define IDLE_TRIES   125  /* 125 * 2us = 250us before calling the line low */

/* ---------- internal helpers ---------- */

static inline void bus_low(const onewire_gpio_t *g)
{
    g->pin_low(g->ctx);
}

static inline void bus_release(const onewire_gpio_t *g)
{
    g->pin_release(g->ctx);
}

static inline bool bus_read(const onewire_gpio_t *g)
{
    return g->pin_read(g->ctx);
}

static inline void bus_delay(const onewire_gpio_t *g, uint16_t us)
{
    g->delay_us(g->ctx, us);
}

/*
 * Write a single bit (LSB first at caller level).
 *   bit=0: pull low for T_WDLOW_0, release, recovery
 *   bit=1: pull low for T_WDLOW_1, release for remainder, recovery
 */
static void write_bit(const onewire_gpio_t *g, bool bit)
{
    if (bit) {
        bus_low(g);
        bus_delay(g, T_WDLOW_1);
        bus_release(g);
        bus_delay(g, (uint16_t)(T_SLOT - T_WDLOW_1));
    } else {
        bus_low(g);
        bus_delay(g, T_WDLOW_0);
        bus_release(g);
    }
    bus_delay(g, T_REC);
}

/*
 * Read a single bit.
 *   Pull low briefly, release, wait, then sample.
 */
static bool read_bit(const onewire_gpio_t *g)
{
    bus_low(g);
    bus_delay(g, T_RDLOW);
    bus_release(g);
    bus_delay(g, T_RDSAMPLE);
    bool val = bus_read(g);
    bus_delay(g, T_RDRELEASE);
    return val;
}

/*
 * Wait for the line to sit high. On a loaded bus it can still be rising after
 * the last slot, so one low read is not a fault: only a low that lasts is.
 */
static bool wait_idle_high(const onewire_gpio_t *g)
{
    for (uint8_t i = 0; i < IDLE_TRIES; i++) {
        if (bus_read(g)) {
            return true;
        }
        bus_delay(g, T_IDLE_STEP);
    }
    return false;
}

/* ---------- public API ---------- */

onewire_err_t onewire_init(onewire_t *bus, const onewire_gpio_t *gpio)
{
    REQUIRE(bus != NULL);
    REQUIRE(gpio != NULL);
    if (bus == NULL || gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }
    REQUIRE(gpio->pin_low != NULL);
    REQUIRE(gpio->pin_release != NULL);
    REQUIRE(gpio->pin_read != NULL);
    REQUIRE(gpio->delay_us != NULL);
    if (gpio->pin_low == NULL || gpio->pin_release == NULL ||
        gpio->pin_read == NULL || gpio->delay_us == NULL) {
        return ONEWIRE_ERR_PARAM;
    }
    bus->gpio = gpio;
    /* Release bus (idle high) */
    gpio->pin_release(gpio->ctx);
    return ONEWIRE_OK;
}

onewire_err_t onewire_reset(onewire_t *bus, bool *presence)
{
    REQUIRE(bus != NULL);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    const onewire_gpio_t *g = bus->gpio;

    if (presence != NULL) {
        *presence = false;
    }

    /* A line that is already low would read as a presence pulse below */
    if (!wait_idle_high(g)) {
        return ONEWIRE_ERR_BUS;
    }

    /* Master reset pulse: pull low for >= 480us */
    bus_low(g);
    bus_delay(g, T_RSTL);

    /* Release and wait for device presence */
    bus_release(g);
    bus_delay(g, T_PDHIGH);

    /* Sample presence: device pulls low if present */
    bool present = !bus_read(g);

    /* Wait for remainder of reset high period */
    bus_delay(g, (uint16_t)(T_RSTH - T_PDHIGH));

    if (presence != NULL) {
        *presence = present;
    }

    return present ? ONEWIRE_OK : ONEWIRE_ERR_PRESENCE;
}

onewire_err_t onewire_write_byte(onewire_t *bus, uint8_t byte)
{
    REQUIRE(bus != NULL);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    const onewire_gpio_t *g = bus->gpio;

    /* 1-Wire sends LSB first */
    for (uint8_t i = 0; i < 8; i++) {
        write_bit(g, (bool)((byte >> i) & 1));
    }
    return ONEWIRE_OK;
}

onewire_err_t onewire_read_byte(onewire_t *bus, uint8_t *byte)
{
    REQUIRE(bus != NULL);
    REQUIRE(byte != NULL);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }
    if (byte == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    const onewire_gpio_t *g = bus->gpio;
    uint8_t val = 0;

    /* 1-Wire reads LSB first */
    for (uint8_t i = 0; i < 8; i++) {
        if (read_bit(g)) {
            val |= (uint8_t)(1U << i);
        }
    }
    *byte = val;
    return ONEWIRE_OK;
}

onewire_err_t onewire_write(onewire_t *bus, const uint8_t *data, uint16_t len)
{
    REQUIRE(bus != NULL);
    REQUIRE(data != NULL || len == 0);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }
    if (len > 0 && data == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    for (uint16_t i = 0; i < len; i++) {
        onewire_err_t err = onewire_write_byte(bus, data[i]);
        if (err != ONEWIRE_OK) {
            return err;
        }
    }
    return ONEWIRE_OK;
}

onewire_err_t onewire_read(onewire_t *bus, uint8_t *buf, uint16_t len)
{
    REQUIRE(bus != NULL);
    REQUIRE(buf != NULL || len == 0);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }
    if (len > 0 && buf == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    for (uint16_t i = 0; i < len; i++) {
        onewire_err_t err = onewire_read_byte(bus, &buf[i]);
        if (err != ONEWIRE_OK) {
            return err;
        }
    }
    return ONEWIRE_OK;
}

onewire_err_t onewire_select(onewire_t *bus, const uint8_t rom[8])
{
    REQUIRE(bus != NULL);
    REQUIRE(rom != NULL);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }
    if (rom == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    onewire_err_t err = onewire_write_byte(bus, ONEWIRE_CMD_MATCH_ROM);
    if (err != ONEWIRE_OK) {
        return err;
    }
    return onewire_write(bus, rom, 8);
}

onewire_err_t onewire_skip(onewire_t *bus)
{
    REQUIRE(bus != NULL);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    return onewire_write_byte(bus, ONEWIRE_CMD_SKIP_ROM);
}

onewire_err_t onewire_read_rom(onewire_t *bus, uint8_t rom[8])
{
    REQUIRE(bus != NULL);
    REQUIRE(rom != NULL);
    if (bus == NULL || bus->gpio == NULL) {
        return ONEWIRE_ERR_PARAM;
    }
    if (rom == NULL) {
        return ONEWIRE_ERR_PARAM;
    }

    onewire_err_t err = onewire_write_byte(bus, ONEWIRE_CMD_READ_ROM);
    if (err != ONEWIRE_OK) {
        return err;
    }

    err = onewire_read(bus, rom, 8);
    if (err != ONEWIRE_OK) {
        return err;
    }

    /* Verify CRC: byte[7] is CRC of bytes[0..6] */
    uint8_t crc = onewire_crc8(rom, 7);
    if (crc != rom[7]) {
        return ONEWIRE_ERR_CRC;
    }

    /* CRC-8 of zeros is 0, so a line held low gets this far */
    uint8_t any_bit = 0;
    for (uint8_t i = 0; i < 8; i++) {
        any_bit |= rom[i];
    }
    if (any_bit == 0) {
        return ONEWIRE_ERR_BUS;
    }

    return ONEWIRE_OK;
}

uint8_t onewire_crc8(const uint8_t *data, uint16_t len)
{
    REQUIRE(data != NULL);
    if (data == NULL) {
        return 0;
    }

    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (uint8_t)((crc ^ byte) & 0x01);
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C; /* reflected polynomial x^8+x^5+x^4+1 */
            }
            byte >>= 1;
        }
    }
    return crc;
}
