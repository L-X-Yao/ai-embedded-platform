// SPDX-License-Identifier: MIT
/*
 * is31fl3731 — ISSI IS31FL3731 LED matrix controller driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "is31fl3731.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses ---------- */

/* Command register: selects current page (frame 0-7 or function page 0x0B) */
#define REG_COMMAND         0xFD

/* Function register page */
#define PAGE_FUNCTION       0x0B

/* Function registers (on function page) */
#define FUNC_CONFIG         0x00   /* Configuration register */
#define FUNC_PICTURE_DISP   0x01   /* Picture Display register */
#define FUNC_AUDIO_SYNC     0x06   /* Audio Sync register */
#define FUNC_SHUTDOWN       0x0A   /* Shutdown register */

/* Frame page layout */
#define FRAME_LED_CTRL_BASE 0x00   /* LED on/off: 18 bytes (0x00-0x11) */
#define FRAME_LED_CTRL_SIZE 18u
#define FRAME_BLINK_BASE    0x12   /* Blink control: 18 bytes (0x12-0x23) */
#define FRAME_BLINK_SIZE    18u
#define FRAME_PWM_BASE      0x24   /* PWM: 144 bytes (0x24-0xB3) */

/* Config register values */
#define CONFIG_PICTURE_MODE 0x00

/* Shutdown register values */
#define SHUTDOWN_OFF        0x00   /* chip in shutdown */
#define SHUTDOWN_NORMAL     0x01   /* chip running */

/* ---------- internal helpers ---------- */

static bool write_reg(const is31fl3731_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->cfg.addr, reg, val);
}

static bool select_page(is31fl3731_t *dev, uint8_t page)
{
    return write_reg(dev, REG_COMMAND, page);
}

static bool select_function_page(is31fl3731_t *dev)
{
    return select_page(dev, PAGE_FUNCTION);
}

static bool select_frame_page(is31fl3731_t *dev, uint8_t frame)
{
    dev->current_frame = frame;
    return select_page(dev, frame);
}

/* Put one frame in a known state: all LEDs enabled, no blink, PWM 0 */
static bool init_frame(is31fl3731_t *dev, uint8_t frame)
{
    if (!select_frame_page(dev, frame))
    {
        return false;
    }
    for (uint8_t i = 0; i < FRAME_LED_CTRL_SIZE; i++)
    {
        if (!write_reg(dev, (uint8_t)(FRAME_LED_CTRL_BASE + i), 0xFF))
        {
            return false;
        }
        dev->led_ctrl[frame][i] = 0xFF;
    }
    for (uint8_t i = 0; i < FRAME_BLINK_SIZE; i++)
    {
        if (!write_reg(dev, (uint8_t)(FRAME_BLINK_BASE + i), 0x00))
        {
            return false;
        }
    }
    for (uint16_t i = 0; i < IS31FL3731_NUM_LEDS; i++)
    {
        if (!write_reg(dev, (uint8_t)(FRAME_PWM_BASE + i), 0x00))
        {
            return false;
        }
    }
    return true;
}

/* ---------- public API ---------- */

/* The steps of init that talk to the chip */
static is31fl3731_err_t bring_up(is31fl3731_t *dev)
{
    /* Select function register page */
    if (!select_function_page(dev))
    {
        return IS31FL3731_ERR_I2C;
    }

    /* Enter shutdown first (required for clean config) */
    if (!write_reg(dev, FUNC_SHUTDOWN, SHUTDOWN_OFF))
    {
        return IS31FL3731_ERR_I2C;
    }

    /* Set picture display mode */
    if (!write_reg(dev, FUNC_CONFIG, CONFIG_PICTURE_MODE))
    {
        return IS31FL3731_ERR_I2C;
    }

    /* Display frame 0 */
    if (!write_reg(dev, FUNC_PICTURE_DISP, 0x00))
    {
        return IS31FL3731_ERR_I2C;
    }

    /* Disable audio sync */
    if (!write_reg(dev, FUNC_AUDIO_SYNC, 0x00))
    {
        return IS31FL3731_ERR_I2C;
    }

    /* Frame data is not assured at power-on, so set up every frame, not just
     * the displayed one. Frame 0 goes last and stays the current frame. */
    for (uint8_t frame = IS31FL3731_NUM_FRAMES; frame > 0u; frame--)
    {
        if (!init_frame(dev, (uint8_t)(frame - 1u)))
        {
            return IS31FL3731_ERR_I2C;
        }
    }

    /* Exit shutdown (normal operation) */
    if (!select_function_page(dev))
    {
        return IS31FL3731_ERR_I2C;
    }
    if (!write_reg(dev, FUNC_SHUTDOWN, SHUTDOWN_NORMAL))
    {
        return IS31FL3731_ERR_I2C;
    }

    return IS31FL3731_OK;
}

is31fl3731_err_t is31fl3731_init(is31fl3731_t *dev,
                                 const is31fl3731_i2c_t *i2c,
                                 const is31fl3731_cfg_t *cfg)
{
    if (!dev)
    {
        return IS31FL3731_ERR_PARAM;
    }

    /* dev->i2c stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!i2c || !cfg)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (!i2c->read_regs || !i2c->write_reg)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (cfg->addr < IS31FL3731_ADDR_BASE || cfg->addr > IS31FL3731_ADDR_MAX)
    {
        return IS31FL3731_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->cfg = *cfg;

    is31fl3731_err_t err = bring_up(dev);
    if (err != IS31FL3731_OK)
    {
        dev->i2c = NULL;
        return err;
    }

    INVARIANT(dev->i2c != NULL);
    return IS31FL3731_OK;
}

is31fl3731_err_t is31fl3731_set_frame(is31fl3731_t *dev, uint8_t frame)
{
    if (!dev || !dev->i2c)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (frame >= IS31FL3731_NUM_FRAMES)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (!select_frame_page(dev, frame))
    {
        return IS31FL3731_ERR_I2C;
    }
    return IS31FL3731_OK;
}

is31fl3731_err_t is31fl3731_set_pixel(is31fl3731_t *dev, uint8_t frame,
                                      uint8_t led_idx, uint8_t pwm)
{
    if (!dev || !dev->i2c)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (frame >= IS31FL3731_NUM_FRAMES)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (led_idx >= IS31FL3731_NUM_LEDS)
    {
        return IS31FL3731_ERR_PARAM;
    }

    if (!select_frame_page(dev, frame))
    {
        return IS31FL3731_ERR_I2C;
    }

    uint8_t reg = (uint8_t)(FRAME_PWM_BASE + led_idx);
    if (!write_reg(dev, reg, pwm))
    {
        return IS31FL3731_ERR_I2C;
    }

    return IS31FL3731_OK;
}

is31fl3731_err_t is31fl3731_set_led_enable(is31fl3731_t *dev, uint8_t frame,
                                           uint8_t led_idx, bool enable)
{
    if (!dev || !dev->i2c)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (frame >= IS31FL3731_NUM_FRAMES)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (led_idx >= IS31FL3731_NUM_LEDS)
    {
        return IS31FL3731_ERR_PARAM;
    }

    if (!select_frame_page(dev, frame))
    {
        return IS31FL3731_ERR_I2C;
    }

    /* LED control register: 18 bytes, 8 LEDs per byte */
    uint8_t byte_idx = led_idx / 8u;
    uint8_t bit_idx  = led_idx % 8u;
    uint8_t reg = (uint8_t)(FRAME_LED_CTRL_BASE + byte_idx);

    /* Rev. G p.7: frame registers can only be read in software shutdown, and
     * the chip normally runs here, so the current byte comes from the shadow */
    uint8_t val = dev->led_ctrl[frame][byte_idx];

    if (enable)
    {
        val |= (uint8_t)(1u << bit_idx);
    }
    else
    {
        val &= (uint8_t)~(1u << bit_idx);
    }

    if (!write_reg(dev, reg, val))
    {
        return IS31FL3731_ERR_I2C;
    }

    dev->led_ctrl[frame][byte_idx] = val;
    return IS31FL3731_OK;
}

is31fl3731_err_t is31fl3731_fill(is31fl3731_t *dev, uint8_t frame,
                                 uint8_t pwm)
{
    if (!dev || !dev->i2c)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (frame >= IS31FL3731_NUM_FRAMES)
    {
        return IS31FL3731_ERR_PARAM;
    }

    if (!select_frame_page(dev, frame))
    {
        return IS31FL3731_ERR_I2C;
    }

    for (uint16_t i = 0; i < IS31FL3731_NUM_LEDS; i++)
    {
        if (!write_reg(dev, (uint8_t)(FRAME_PWM_BASE + i), pwm))
        {
            return IS31FL3731_ERR_I2C;
        }
    }

    return IS31FL3731_OK;
}

is31fl3731_err_t is31fl3731_display_frame(is31fl3731_t *dev, uint8_t frame)
{
    if (!dev || !dev->i2c)
    {
        return IS31FL3731_ERR_PARAM;
    }
    if (frame >= IS31FL3731_NUM_FRAMES)
    {
        return IS31FL3731_ERR_PARAM;
    }

    if (!select_function_page(dev))
    {
        return IS31FL3731_ERR_I2C;
    }
    if (!write_reg(dev, FUNC_PICTURE_DISP, frame))
    {
        return IS31FL3731_ERR_I2C;
    }

    return IS31FL3731_OK;
}

is31fl3731_err_t is31fl3731_shutdown(is31fl3731_t *dev, bool enable)
{
    if (!dev || !dev->i2c)
    {
        return IS31FL3731_ERR_PARAM;
    }

    if (!select_function_page(dev))
    {
        return IS31FL3731_ERR_I2C;
    }
    if (!write_reg(dev, FUNC_SHUTDOWN, enable ? SHUTDOWN_OFF : SHUTDOWN_NORMAL))
    {
        return IS31FL3731_ERR_I2C;
    }

    return IS31FL3731_OK;
}
