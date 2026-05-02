/*
 * tcs34725 — AMS TCS34725 I2C RGB color sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Register-based I2C with command register protocol (0x80 | reg).
 * Addr 0x29, ID register 0x12 returns 0x44 or 0x4D.
 * RGBC data in registers 0x14-0x1B (16-bit little-endian).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the tcs34725_dev_t struct.
 */

#ifndef TCS34725_H
#define TCS34725_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TCS34725_OK        =  0,
    TCS34725_ERR_I2C   = -1,
    TCS34725_ERR_ID    = -2,
    TCS34725_ERR_PARAM = -3,
} tcs34725_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} tcs34725_i2c_t;

#define TCS34725_ADDR  0x29u

typedef enum {
    TCS34725_INTEG_2_4MS  = 0xFF,   /* 1 cycle   */
    TCS34725_INTEG_24MS   = 0xF6,   /* 10 cycles */
    TCS34725_INTEG_50MS   = 0xEB,   /* 20 cycles */
    TCS34725_INTEG_101MS  = 0xD5,   /* 42 cycles */
    TCS34725_INTEG_154MS  = 0xC0,   /* 64 cycles */
    TCS34725_INTEG_700MS  = 0x00,   /* 256 cycles */
} tcs34725_integ_t;

typedef enum {
    TCS34725_GAIN_1X  = 0x00,
    TCS34725_GAIN_4X  = 0x01,
    TCS34725_GAIN_16X = 0x02,
    TCS34725_GAIN_60X = 0x03,
} tcs34725_gain_t;

typedef struct {
    uint16_t c;  /* clear channel */
    uint16_t r;  /* red channel   */
    uint16_t g;  /* green channel */
    uint16_t b;  /* blue channel  */
} tcs34725_rgbc_t;

typedef struct {
    tcs34725_i2c_t   i2c;
    tcs34725_integ_t integ;
    tcs34725_gain_t  gain;
} tcs34725_dev_t;

/** Init, verify chip ID (0x44 or 0x4D), configure integration time and gain.
 *  @thread_safe: no */
tcs34725_err_t tcs34725_init(tcs34725_dev_t *dev,
                             const tcs34725_i2c_t *i2c,
                             tcs34725_integ_t integ,
                             tcs34725_gain_t gain);

/** Read RGBC data (clear, red, green, blue — 16-bit each).
 *  @thread_safe: caller_must_lock */
tcs34725_err_t tcs34725_read_rgbc(tcs34725_dev_t *dev,
                                  tcs34725_rgbc_t *out);

/** Set integration time. @thread_safe: caller_must_lock */
tcs34725_err_t tcs34725_set_integration_time(tcs34725_dev_t *dev,
                                             tcs34725_integ_t integ);

/** Set gain. @thread_safe: caller_must_lock */
tcs34725_err_t tcs34725_set_gain(tcs34725_dev_t *dev,
                                 tcs34725_gain_t gain);

/** Enable RGBC measurement (power on + ADC enable).
 *  @thread_safe: caller_must_lock */
tcs34725_err_t tcs34725_enable(tcs34725_dev_t *dev);

/** Disable RGBC measurement (power off).
 *  @thread_safe: caller_must_lock */
tcs34725_err_t tcs34725_disable(tcs34725_dev_t *dev);

/** Estimate correlated color temperature (CCT) in Kelvin from RGBC data.
 *  Uses the DN40 application note formula.
 *  @thread_safe: caller_must_lock */
tcs34725_err_t tcs34725_get_color_temp(const tcs34725_rgbc_t *rgbc,
                                       uint16_t *cct_k);

/** Estimate illuminance (lux) from RGBC data.
 *  @thread_safe: caller_must_lock */
tcs34725_err_t tcs34725_get_lux(const tcs34725_rgbc_t *rgbc,
                                float *lux);

#ifdef __cplusplus
}
#endif

#endif /* TCS34725_H */
