// SPDX-License-Identifier: MIT
/*
 * soft_i2c — GPIO bit-bang I2C master with bus recovery
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "soft_i2c.h"
#include "contract.h"

#define SOFT_I2C_ADDR_MAX 0x7FU /* addresses are 7 bits, shifted left on the wire */

/* ---------- internal helpers ---------- */

static inline void scl_high(const soft_i2c_gpio_t *g)
{
    g->scl_set(g->ctx, true);
    g->delay(g->ctx);
}

static inline void scl_low(const soft_i2c_gpio_t *g)
{
    g->scl_set(g->ctx, false);
    g->delay(g->ctx);
}

static inline void sda_high(const soft_i2c_gpio_t *g)
{
    g->sda_dir_out(g->ctx);
    g->sda_set(g->ctx, true);
    g->delay(g->ctx);
}

static inline void sda_low(const soft_i2c_gpio_t *g)
{
    g->sda_dir_out(g->ctx);
    g->sda_set(g->ctx, false);
    g->delay(g->ctx);
}

static inline bool sda_read(const soft_i2c_gpio_t *g)
{
    g->sda_dir_in(g->ctx);
    return g->sda_get(g->ctx);
}

static void i2c_start(const soft_i2c_gpio_t *g)
{
    sda_high(g);
    scl_high(g);
    sda_low(g);
    g->delay(g->ctx);
    scl_low(g);
    g->delay(g->ctx);
}

static void i2c_stop(const soft_i2c_gpio_t *g)
{
    sda_low(g);
    scl_high(g);
    sda_high(g);
    g->delay(g->ctx);
}

static bool i2c_wait_ack(const soft_i2c_gpio_t *g)
{
    sda_high(g);
    scl_high(g);
    bool nack = sda_read(g);
    scl_low(g);
    return !nack;
}

static void i2c_send_ack(const soft_i2c_gpio_t *g)
{
    sda_low(g);
    scl_high(g);
    scl_low(g);
    sda_high(g);
}

static void i2c_send_nack(const soft_i2c_gpio_t *g)
{
    sda_high(g);
    scl_high(g);
    scl_low(g);
}

static bool i2c_write_byte(const soft_i2c_gpio_t *g, uint8_t byte)
{
    g->sda_dir_out(g->ctx);
    for (int i = 7; i >= 0; i--) {
        g->sda_set(g->ctx, (byte >> i) & 1);
        g->delay(g->ctx);
        scl_high(g);
        scl_low(g);
    }
    return i2c_wait_ack(g);
}

static uint8_t i2c_read_byte(const soft_i2c_gpio_t *g)
{
    uint8_t byte = 0;
    sda_high(g);
    for (int i = 7; i >= 0; i--) {
        scl_high(g);
        if (sda_read(g)) {
            byte |= (uint8_t)(1 << i);
        }
        scl_low(g);
    }
    return byte;
}

/* ---------- public API ---------- */

soft_i2c_err_t soft_i2c_init(soft_i2c_t *bus, const soft_i2c_gpio_t *gpio)
{
    REQUIRE(bus != NULL);
    REQUIRE(gpio != NULL);
    if (bus == NULL || gpio == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }
    REQUIRE(gpio->scl_set != NULL);
    REQUIRE(gpio->sda_set != NULL);
    REQUIRE(gpio->sda_dir_out != NULL);
    REQUIRE(gpio->sda_dir_in != NULL);
    REQUIRE(gpio->sda_get != NULL);
    REQUIRE(gpio->delay != NULL);
    if (gpio->scl_set == NULL || gpio->sda_set == NULL ||
        gpio->sda_dir_out == NULL || gpio->sda_dir_in == NULL ||
        gpio->sda_get == NULL || gpio->delay == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }
    bus->gpio = gpio;
    /* Release bus: both lines high */
    gpio->scl_set(gpio->ctx, true);
    sda_high(gpio);
    return SOFT_I2C_OK;
}

soft_i2c_err_t soft_i2c_write(soft_i2c_t *bus, uint8_t addr,
                              const uint8_t *data, uint16_t len)
{
    REQUIRE(bus != NULL);
    REQUIRE(data != NULL || len == 0);
    if (bus == NULL || bus->gpio == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }
    if (addr > SOFT_I2C_ADDR_MAX || (len > 0 && data == NULL)) {
        return SOFT_I2C_ERR_PARAM;
    }

    const soft_i2c_gpio_t *g = bus->gpio;

    i2c_start(g);
    if (!i2c_write_byte(g, (uint8_t)(addr << 1))) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }
    for (uint16_t i = 0; i < len; i++) {
        if (!i2c_write_byte(g, data[i])) {
            i2c_stop(g);
            return SOFT_I2C_ERR_NACK;
        }
    }
    i2c_stop(g);
    return SOFT_I2C_OK;
}

soft_i2c_err_t soft_i2c_read(soft_i2c_t *bus, uint8_t addr,
                             uint8_t *buf, uint16_t len)
{
    REQUIRE(bus != NULL);
    REQUIRE(buf != NULL);
    REQUIRE(len > 0);
    if (bus == NULL || bus->gpio == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }
    if (addr > SOFT_I2C_ADDR_MAX || len == 0 || buf == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }

    const soft_i2c_gpio_t *g = bus->gpio;

    i2c_start(g);
    if (!i2c_write_byte(g, (uint8_t)((addr << 1) | 0x01))) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = i2c_read_byte(g);
        if (i == len - 1) {
            i2c_send_nack(g);
        } else {
            i2c_send_ack(g);
        }
    }
    i2c_stop(g);
    return SOFT_I2C_OK;
}

soft_i2c_err_t soft_i2c_write_reg(soft_i2c_t *bus, uint8_t addr,
                                  uint8_t reg, uint8_t val)
{
    REQUIRE(bus != NULL);
    if (bus == NULL || bus->gpio == NULL || addr > SOFT_I2C_ADDR_MAX) {
        return SOFT_I2C_ERR_PARAM;
    }

    const soft_i2c_gpio_t *g = bus->gpio;

    i2c_start(g);
    if (!i2c_write_byte(g, (uint8_t)(addr << 1))) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }
    if (!i2c_write_byte(g, reg)) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }
    if (!i2c_write_byte(g, val)) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }
    i2c_stop(g);
    return SOFT_I2C_OK;
}

soft_i2c_err_t soft_i2c_read_regs(soft_i2c_t *bus, uint8_t addr,
                                  uint8_t reg, uint8_t *buf, uint16_t len)
{
    REQUIRE(bus != NULL);
    REQUIRE(buf != NULL);
    REQUIRE(len > 0);
    if (bus == NULL || bus->gpio == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }
    if (addr > SOFT_I2C_ADDR_MAX || len == 0 || buf == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }

    const soft_i2c_gpio_t *g = bus->gpio;

    /* Write phase: send register address */
    i2c_start(g);
    if (!i2c_write_byte(g, (uint8_t)(addr << 1))) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }
    if (!i2c_write_byte(g, reg)) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }

    /* Repeated start + read phase */
    i2c_start(g);
    if (!i2c_write_byte(g, (uint8_t)((addr << 1) | 0x01))) {
        i2c_stop(g);
        return SOFT_I2C_ERR_NACK;
    }
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = i2c_read_byte(g);
        if (i == len - 1) {
            i2c_send_nack(g);
        } else {
            i2c_send_ack(g);
        }
    }
    i2c_stop(g);
    return SOFT_I2C_OK;
}

soft_i2c_err_t soft_i2c_bus_recover(soft_i2c_t *bus)
{
    REQUIRE(bus != NULL);
    if (bus == NULL || bus->gpio == NULL) {
        return SOFT_I2C_ERR_PARAM;
    }

    const soft_i2c_gpio_t *g = bus->gpio;

    /* Release both lines */
    g->scl_set(g->ctx, true);
    sda_high(g);
    g->delay(g->ctx);
    g->delay(g->ctx);

    /* Clock out up to 18 pulses to free stuck slave */
    for (int i = 0; i < 18; i++) {
        scl_high(g);
        scl_low(g);
        if (sda_read(g)) {
            break;
        }
    }

    /* Generate STOP condition */
    i2c_stop(g);
    return SOFT_I2C_OK;
}
