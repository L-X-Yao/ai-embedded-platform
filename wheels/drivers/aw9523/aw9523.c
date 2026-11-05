// SPDX-License-Identifier: MIT
/*
 * aw9523 — AW9523B I2C GPIO expander and LED driver (16 pins)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map and DIM-to-pin mapping from Awinic AW9523B datasheet rev 1.5.
 */

#include "aw9523.h"
#include "contract.h"

/* ---- DIM register mapping (datasheet Table 3) ----
 * Port1 pin0 = DIM0 (0x20)   Port0 pin0 = DIM4  (0x24)
 * Port1 pin1 = DIM1 (0x21)   Port0 pin1 = DIM5  (0x25)
 * Port1 pin2 = DIM2 (0x22)   Port0 pin2 = DIM6  (0x26)
 * Port1 pin3 = DIM3 (0x23)   Port0 pin3 = DIM7  (0x27)
 * Port1 pin4 = DIM12 (0x2C)  Port0 pin4 = DIM8  (0x28)
 * Port1 pin5 = DIM13 (0x2D)  Port0 pin5 = DIM9  (0x29)
 * Port1 pin6 = DIM14 (0x2E)  Port0 pin6 = DIM10 (0x2A)
 * Port1 pin7 = DIM15 (0x2F)  Port0 pin7 = DIM11 (0x2B)
 */

/* CTL register bits */
#define CTL_P0_PUSH_PULL  0x10u  /* bit 4: P0 output mode 1=push-pull */

/* ---- internal helpers ---- */

static bool write_reg(const aw9523_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->dev_addr, reg, val);
}

static bool read_reg(const aw9523_t *dev, uint8_t reg, uint8_t *val)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->dev_addr, reg, val, 1);
}

static bool valid_port(aw9523_port_t port)
{
    return port == AW9523_PORT0 || port == AW9523_PORT1;
}

/* Port-to-register lookup: avoids ternary promotion to unsigned int */
static uint8_t port_reg(aw9523_port_t port, uint8_t reg_p0, uint8_t reg_p1)
{
    return (port == AW9523_PORT0) ? reg_p0 : reg_p1;
}

static uint8_t dim_reg_for_pin(aw9523_port_t port, uint8_t pin)
{
    /*
     * Port0 pin0-3 → DIM4-DIM7   (0x24-0x27)
     * Port0 pin4-7 → DIM8-DIM11  (0x28-0x2B)
     * Port1 pin0-3 → DIM0-DIM3   (0x20-0x23)
     * Port1 pin4-7 → DIM12-DIM15 (0x2C-0x2F)
     */
    if (port == AW9523_PORT0) {
        if (pin < 4u) {
            return (uint8_t)(AW9523_REG_DIM_BASE + 4u + pin);    /* 0x24-0x27 */
        }
        return (uint8_t)(AW9523_REG_DIM_BASE + 4u + pin);        /* 0x28-0x2B */
    }
    /* PORT1 */
    if (pin < 4u) {
        return (uint8_t)(AW9523_REG_DIM_BASE + pin);             /* 0x20-0x23 */
    }
    return (uint8_t)(AW9523_REG_DIM_BASE + 8u + pin);            /* 0x2C-0x2F */
}

/* ---- public API ---- */

aw9523_err_t aw9523_init(aw9523_t *dev, const aw9523_i2c_t *i2c,
                         uint8_t dev_addr)
{
    if (!dev || !i2c || !i2c->write_reg || !i2c->read_regs)
        return AW9523_ERR_PARAM;
    if (dev_addr < 0x58 || dev_addr > 0x5B)
        return AW9523_ERR_PARAM;

    dev->i2c      = i2c;
    dev->dev_addr = dev_addr;

    /* Read and verify chip ID */
    uint8_t id = 0;
    if (!read_reg(dev, AW9523_REG_ID, &id))
        return AW9523_ERR_I2C;

    if (id != AW9523_CHIP_ID)
        return AW9523_ERR_ID;

    /* Software reset via CTL register bit 0 */
    if (!write_reg(dev, AW9523_REG_CTL, 0x00))
        return AW9523_ERR_I2C;

    /* Set all pins as input (POR default: 0xFF = all input) */
    if (!write_reg(dev, AW9523_REG_CONFIG_P0, 0xFF))
        return AW9523_ERR_I2C;
    if (!write_reg(dev, AW9523_REG_CONFIG_P1, 0xFF))
        return AW9523_ERR_I2C;

    /* Set all pins to GPIO mode (not LED) */
    if (!write_reg(dev, AW9523_REG_LED_MODE_P0, 0xFF))
        return AW9523_ERR_I2C;
    if (!write_reg(dev, AW9523_REG_LED_MODE_P1, 0xFF))
        return AW9523_ERR_I2C;

    /* Disable all interrupts (1 = disabled) */
    if (!write_reg(dev, AW9523_REG_INT_P0, 0xFF))
        return AW9523_ERR_I2C;
    if (!write_reg(dev, AW9523_REG_INT_P1, 0xFF))
        return AW9523_ERR_I2C;

    /* Clear outputs */
    if (!write_reg(dev, AW9523_REG_OUTPUT_P0, 0x00))
        return AW9523_ERR_I2C;
    if (!write_reg(dev, AW9523_REG_OUTPUT_P1, 0x00))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_set_direction(aw9523_t *dev, aw9523_port_t port,
                                  uint8_t pin, aw9523_dir_t dir)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return AW9523_ERR_PARAM;
    if (pin > 7)
        return AW9523_ERR_PARAM;
    if (dir != AW9523_DIR_OUTPUT && dir != AW9523_DIR_INPUT)
        return AW9523_ERR_PARAM;

    uint8_t reg = port_reg(port, AW9523_REG_CONFIG_P0, AW9523_REG_CONFIG_P1);
    uint8_t cur;
    if (!read_reg(dev, reg, &cur))
        return AW9523_ERR_I2C;

    if (dir == AW9523_DIR_INPUT) {
        cur |= (uint8_t)(1u << pin);
    } else {
        cur &= (uint8_t)~(1u << pin);
    }

    if (!write_reg(dev, reg, cur))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_write_pin(aw9523_t *dev, aw9523_port_t port,
                              uint8_t pin, aw9523_level_t level)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return AW9523_ERR_PARAM;
    if (pin > 7)
        return AW9523_ERR_PARAM;
    if (level != AW9523_PIN_LOW && level != AW9523_PIN_HIGH)
        return AW9523_ERR_PARAM;

    uint8_t reg = port_reg(port, AW9523_REG_OUTPUT_P0, AW9523_REG_OUTPUT_P1);
    uint8_t cur;
    if (!read_reg(dev, reg, &cur))
        return AW9523_ERR_I2C;

    if (level == AW9523_PIN_HIGH) {
        cur |= (uint8_t)(1u << pin);
    } else {
        cur &= (uint8_t)~(1u << pin);
    }

    if (!write_reg(dev, reg, cur))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_read_pin(aw9523_t *dev, aw9523_port_t port,
                             uint8_t pin, aw9523_level_t *val)
{
    if (!dev || !dev->i2c || !val || !valid_port(port))
        return AW9523_ERR_PARAM;
    if (pin > 7)
        return AW9523_ERR_PARAM;

    uint8_t reg = port_reg(port, AW9523_REG_INPUT_P0, AW9523_REG_INPUT_P1);
    uint8_t raw;
    if (!read_reg(dev, reg, &raw))
        return AW9523_ERR_I2C;

    *val = (raw & (1u << pin)) ? AW9523_PIN_HIGH : AW9523_PIN_LOW;
    return AW9523_OK;
}

aw9523_err_t aw9523_write_port(aw9523_t *dev, aw9523_port_t port,
                               uint8_t val)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return AW9523_ERR_PARAM;

    uint8_t reg = port_reg(port, AW9523_REG_OUTPUT_P0, AW9523_REG_OUTPUT_P1);
    if (!write_reg(dev, reg, val))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_read_port(aw9523_t *dev, aw9523_port_t port,
                              uint8_t *val)
{
    if (!dev || !dev->i2c || !val || !valid_port(port))
        return AW9523_ERR_PARAM;

    uint8_t reg = port_reg(port, AW9523_REG_INPUT_P0, AW9523_REG_INPUT_P1);
    if (!read_reg(dev, reg, val))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_set_led_mode(aw9523_t *dev, aw9523_port_t port,
                                 uint8_t pin, aw9523_led_mode_t mode)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return AW9523_ERR_PARAM;
    if (pin > 7)
        return AW9523_ERR_PARAM;
    if (mode != AW9523_MODE_LED && mode != AW9523_MODE_GPIO)
        return AW9523_ERR_PARAM;

    uint8_t reg = port_reg(port, AW9523_REG_LED_MODE_P0, AW9523_REG_LED_MODE_P1);
    uint8_t cur;
    if (!read_reg(dev, reg, &cur))
        return AW9523_ERR_I2C;

    if (mode == AW9523_MODE_GPIO) {
        cur |= (uint8_t)(1u << pin);
    } else {
        cur &= (uint8_t)~(1u << pin);  /* 0 = LED mode */
    }

    if (!write_reg(dev, reg, cur))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_set_led_current(aw9523_t *dev, aw9523_port_t port,
                                    uint8_t pin, uint8_t current)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return AW9523_ERR_PARAM;
    if (pin > 7)
        return AW9523_ERR_PARAM;

    uint8_t reg = dim_reg_for_pin(port, pin);
    if (!write_reg(dev, reg, current))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_set_interrupt_enable(aw9523_t *dev, aw9523_port_t port,
                                         uint8_t mask)
{
    if (!dev || !dev->i2c || !valid_port(port))
        return AW9523_ERR_PARAM;

    /* AW9523B interrupt register: 0=enabled, 1=disabled. Invert user mask. */
    uint8_t reg = port_reg(port, AW9523_REG_INT_P0, AW9523_REG_INT_P1);
    uint8_t inverted = (uint8_t)~mask;
    if (!write_reg(dev, reg, inverted))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}

aw9523_err_t aw9523_reset(aw9523_t *dev)
{
    if (!dev || !dev->i2c)
        return AW9523_ERR_PARAM;

    /* AW9523B software reset: write 0x00 to reg 0x7F */
    if (!write_reg(dev, 0x7F, 0x00))
        return AW9523_ERR_I2C;

    return AW9523_OK;
}
