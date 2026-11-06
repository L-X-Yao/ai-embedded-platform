// SPDX-License-Identifier: MIT
/*
 * max30102 -- Maxim MAX30102 pulse oximeter and heart rate sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map and FIFO protocol from MAX30102 datasheet (19-6368, Rev 1).
 * 18-bit ADC, up to 32-sample FIFO, RED+IR LEDs for SpO2 computation.
 */

#include "max30102.h"
#include "contract.h"
#include <string.h>

/* --- register addresses (MAX30102 datasheet Table 1) --- */

#define REG_INT_STATUS_1   0x00u
#define REG_INT_STATUS_2   0x01u
#define REG_INT_ENABLE_1   0x02u
#define REG_INT_ENABLE_2   0x03u

#define REG_FIFO_WR_PTR    0x04u
#define REG_OVF_COUNTER    0x05u
#define REG_FIFO_RD_PTR    0x06u
#define REG_FIFO_DATA      0x07u

#define REG_FIFO_CONFIG    0x08u
#define REG_MODE_CONFIG    0x09u
#define REG_SPO2_CONFIG    0x0Au

#define REG_LED1_PA        0x0Cu  /* RED LED pulse amplitude  */
#define REG_LED2_PA        0x0Du  /* IR  LED pulse amplitude  */

#define REG_TEMP_INT       0x1Fu
#define REG_TEMP_FRAC      0x20u
#define REG_TEMP_CONFIG    0x21u

#define REG_REV_ID         0xFEu
#define REG_PART_ID        0xFFu

/* --- bit positions --- */

#define MODE_SHDN_BIT      7u
#define MODE_RESET_BIT     6u
#define MODE_MASK          0x07u

#define FIFO_DEPTH         32u

/* --- helpers --- */

static bool write_reg(const max30102_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, MAX30102_ADDR, reg, val);
}

static bool read_regs(const max30102_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, MAX30102_ADDR, reg, buf, len);
}

static void delay_ms(const max30102_t *dev, uint32_t ms)
{
    if (dev->i2c->delay_ms)
    {
        dev->i2c->delay_ms(dev->i2c->ctx, ms);
    }
}

/* Map sample rate (Hz) to SPO2_CONFIG SR[4:2] bits.
 * Returns 0xFF if invalid. */
static uint8_t rate_to_sr_bits(uint16_t rate)
{
    switch (rate)
    {
    case 50:   return 0;
    case 100:  return 1;
    case 200:  return 2;
    case 400:  return 3;
    case 800:  return 4;
    case 1000: return 5;
    case 1600: return 6;
    case 3200: return 7;
    default:   return 0xFF;
    }
}

/* Highest SR code allowed for a mode and the LED_PW[1:0] bits of SPO2_CONFIG:
 * data sheet p24, Tables 11 and 12.  Above it the chip programs a lower rate
 * than the one written, and nothing reports that (p20). */
static uint8_t max_sr_bits(max30102_mode_t mode, uint8_t spo2_config)
{
    /* per LED_PW 69, 118, 215, 411 us */
    static const uint8_t spo2[4] = {6u, 5u, 4u, 3u};  /* 1600..400 sps  */
    static const uint8_t hr[4]   = {7u, 6u, 6u, 5u};  /* 3200..1000 sps */
    uint8_t pw = (uint8_t)(spo2_config & 0x03u);

    return (mode == MAX30102_MODE_SPO2) ? spo2[pw] : hr[pw];
}

/* Empty the FIFO.  Call after writing MODE: FIFO_WR_PTR can only be changed
 * while MODE is 010, 011 or 111 (data sheet p14). */
static bool clear_fifo(const max30102_t *dev)
{
    return write_reg(dev, REG_FIFO_WR_PTR, 0) &&
           write_reg(dev, REG_OVF_COUNTER, 0) &&
           write_reg(dev, REG_FIFO_RD_PTR, 0);
}

/* --- public API --- */

max30102_err_t max30102_init(max30102_t *dev, const max30102_i2c_t *i2c)
{
    if (!dev || !i2c)
        return MAX30102_ERR_PARAM;
    if (!i2c->write_reg || !i2c->read_regs)
        return MAX30102_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;

    /* verify part ID */
    uint8_t id = 0;
    if (!read_regs(dev, REG_PART_ID, &id, 1))
        return MAX30102_ERR_I2C;
    if (id != MAX30102_PART_ID)
        return MAX30102_ERR_ID;

    /* software reset */
    if (!write_reg(dev, REG_MODE_CONFIG, (uint8_t)(1u << MODE_RESET_BIT)))
        return MAX30102_ERR_I2C;
    delay_ms(dev, 100);

    /* FIFO config: no averaging, rollover enabled, a_full = 0 (32 unread) */
    if (!write_reg(dev, REG_FIFO_CONFIG, 0x10))
        return MAX30102_ERR_I2C;

    /* SPO2 config: ADC range 4096, 100 sps, 411 us pulse width (18-bit) */
    if (!write_reg(dev, REG_SPO2_CONFIG, 0x27))
        return MAX30102_ERR_I2C;

    /* default LED currents: ~6.4 mA each (register value 0x20 * 0.2 mA) */
    if (!write_reg(dev, REG_LED1_PA, 0x20))
        return MAX30102_ERR_I2C;
    if (!write_reg(dev, REG_LED2_PA, 0x20))
        return MAX30102_ERR_I2C;

    /* default mode: SpO2 */
    dev->mode = MAX30102_MODE_SPO2;
    if (!write_reg(dev, REG_MODE_CONFIG, (uint8_t)MAX30102_MODE_SPO2))
        return MAX30102_ERR_I2C;

    if (!clear_fifo(dev))
        return MAX30102_ERR_I2C;

    INVARIANT(dev->i2c != NULL);
    return MAX30102_OK;
}

max30102_err_t max30102_set_mode(max30102_t *dev, max30102_mode_t mode)
{
    if (!dev || !dev->i2c)
        return MAX30102_ERR_PARAM;
    if (mode != MAX30102_MODE_HR && mode != MAX30102_MODE_SPO2)
        return MAX30102_ERR_PARAM;

    /* a rate allowed in HR mode can be above the SpO2 limit */
    uint8_t cfg = 0;
    if (!read_regs(dev, REG_SPO2_CONFIG, &cfg, 1))
        return MAX30102_ERR_I2C;
    if (((cfg >> 2) & 0x07u) > max_sr_bits(mode, cfg))
        return MAX30102_ERR_PARAM;

    /* read current MODE_CONFIG to preserve SHDN bit */
    uint8_t reg = 0;
    if (!read_regs(dev, REG_MODE_CONFIG, &reg, 1))
        return MAX30102_ERR_I2C;

    reg = (uint8_t)((reg & ~MODE_MASK) | ((uint8_t)mode & MODE_MASK));
    if (!write_reg(dev, REG_MODE_CONFIG, reg))
        return MAX30102_ERR_I2C;

    dev->mode = mode;

    /* samples left from the old mode have the old size */
    if (!clear_fifo(dev))
        return MAX30102_ERR_I2C;

    return MAX30102_OK;
}

max30102_err_t max30102_set_led_current(max30102_t *dev,
                                        uint8_t red_ma_x10,
                                        uint8_t ir_ma_x10)
{
    if (!dev || !dev->i2c)
        return MAX30102_ERR_PARAM;

    /* Each register step = 0.2 mA.  Input is in 0.1 mA units.
     * Register value = red_ma_x10 / 2 (integer division, rounds down).
     * Max input 255 (25.5 mA representable; full range 0-50 mA needs
     * uint16_t, but API uses uint8_t so max 25.5 mA). */
    uint8_t red_reg = (uint8_t)(red_ma_x10 / 2u);
    uint8_t ir_reg  = (uint8_t)(ir_ma_x10  / 2u);

    if (!write_reg(dev, REG_LED1_PA, red_reg))
        return MAX30102_ERR_I2C;
    if (!write_reg(dev, REG_LED2_PA, ir_reg))
        return MAX30102_ERR_I2C;

    return MAX30102_OK;
}

max30102_err_t max30102_set_sample_rate(max30102_t *dev, uint16_t rate)
{
    if (!dev || !dev->i2c)
        return MAX30102_ERR_PARAM;

    uint8_t sr = rate_to_sr_bits(rate);
    if (sr == 0xFF)
        return MAX30102_ERR_PARAM;

    /* read-modify-write SPO2_CONFIG, preserving ADC_RANGE[6:5] and PW[1:0] */
    uint8_t reg = 0;
    if (!read_regs(dev, REG_SPO2_CONFIG, &reg, 1))
        return MAX30102_ERR_I2C;

    if (sr > max_sr_bits(dev->mode, reg))
        return MAX30102_ERR_PARAM;

    reg = (uint8_t)((reg & 0x63u) | (uint8_t)(sr << 2));
    if (!write_reg(dev, REG_SPO2_CONFIG, reg))
        return MAX30102_ERR_I2C;

    return MAX30102_OK;
}

max30102_err_t max30102_read_fifo(max30102_t *dev,
                                  max30102_sample_t *samples,
                                  uint8_t max_count,
                                  uint8_t *out_count)
{
    if (!dev || !dev->i2c || !samples || !out_count)
        return MAX30102_ERR_PARAM;
    if (max_count == 0)
        return MAX30102_ERR_PARAM;

    *out_count = 0;

    /* read write and read pointers */
    uint8_t wr_ptr = 0;
    uint8_t rd_ptr = 0;
    if (!read_regs(dev, REG_FIFO_WR_PTR, &wr_ptr, 1))
        return MAX30102_ERR_I2C;
    if (!read_regs(dev, REG_FIFO_RD_PTR, &rd_ptr, 1))
        return MAX30102_ERR_I2C;

    /* number of available samples (5-bit pointers, wraps at 32) */
    uint8_t available = (uint8_t)((wr_ptr - rd_ptr) & 0x1F);
    if (available == 0)
    {
        /* Equal pointers mean empty or full.  OVF_COUNTER only counts once
         * the FIFO is full, so non-zero means all 32 are unread.  A FIFO
         * that has just reached 32 with nothing lost still reads as empty,
         * until the next sample. */
        uint8_t ovf = 0;
        if (!read_regs(dev, REG_OVF_COUNTER, &ovf, 1))
            return MAX30102_ERR_I2C;
        if ((ovf & 0x1Fu) == 0)
            return MAX30102_OK;
        available = FIFO_DEPTH;
    }

    uint8_t to_read = (available < max_count) ? available : max_count;

    /* bytes per sample depends on mode:
     *   HR mode:   3 bytes (RED only)
     *   SpO2 mode: 6 bytes (RED + IR) */
    uint8_t bytes_per_sample = (dev->mode == MAX30102_MODE_SPO2) ? 6u : 3u;

    for (uint8_t i = 0; i < to_read; i++)
    {
        uint8_t buf[6] = {0};
        if (!read_regs(dev, REG_FIFO_DATA, buf, bytes_per_sample))
            return MAX30102_ERR_I2C;

        /* 18-bit ADC: data is left-justified in 3 bytes, mask upper 6 bits */
        samples[i].red = (((uint32_t)buf[0] << 16) |
                          ((uint32_t)buf[1] << 8) |
                          ((uint32_t)buf[2])) & 0x03FFFFu;

        if (dev->mode == MAX30102_MODE_SPO2)
        {
            samples[i].ir = (((uint32_t)buf[3] << 16) |
                             ((uint32_t)buf[4] << 8) |
                             ((uint32_t)buf[5])) & 0x03FFFFu;
        }
        else
        {
            samples[i].ir = 0;
        }
    }

    *out_count = to_read;
    return MAX30102_OK;
}

max30102_err_t max30102_read_temp(max30102_t *dev, float *temp_c)
{
    if (!dev || !dev->i2c || !temp_c)
        return MAX30102_ERR_PARAM;

    /* trigger one-shot temperature measurement */
    if (!write_reg(dev, REG_TEMP_CONFIG, 0x01))
        return MAX30102_ERR_I2C;

    delay_ms(dev, 30);

    uint8_t int_part = 0;
    uint8_t frac_part = 0;
    if (!read_regs(dev, REG_TEMP_INT, &int_part, 1))
        return MAX30102_ERR_I2C;
    if (!read_regs(dev, REG_TEMP_FRAC, &frac_part, 1))
        return MAX30102_ERR_I2C;

    /* TEMP_INT is signed 8-bit, TEMP_FRAC is 4-bit (0.0625 C steps) */
    *temp_c = (float)(int8_t)int_part + (float)(frac_part & 0x0Fu) * 0.0625f;

    ENSURE(*temp_c == *temp_c);  /* not NaN */
    return MAX30102_OK;
}

max30102_err_t max30102_shutdown(max30102_t *dev, bool enable)
{
    if (!dev || !dev->i2c)
        return MAX30102_ERR_PARAM;

    uint8_t reg = 0;
    if (!read_regs(dev, REG_MODE_CONFIG, &reg, 1))
        return MAX30102_ERR_I2C;

    if (enable)
        reg = (uint8_t)(reg | (1u << MODE_SHDN_BIT));
    else
        reg = (uint8_t)(reg & ~(1u << MODE_SHDN_BIT));

    if (!write_reg(dev, REG_MODE_CONFIG, reg))
        return MAX30102_ERR_I2C;

    return MAX30102_OK;
}

max30102_err_t max30102_reset(max30102_t *dev)
{
    if (!dev || !dev->i2c)
        return MAX30102_ERR_PARAM;

    if (!write_reg(dev, REG_MODE_CONFIG, (uint8_t)(1u << MODE_RESET_BIT)))
        return MAX30102_ERR_I2C;

    delay_ms(dev, 100);
    return MAX30102_OK;
}

uint8_t max30102_read_part_id(max30102_t *dev)
{
    if (!dev)
        return 0;

    uint8_t id = 0;
    if (!read_regs(dev, REG_PART_ID, &id, 1))
        return 0;

    return id;
}
