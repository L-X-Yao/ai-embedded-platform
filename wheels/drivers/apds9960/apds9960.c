// SPDX-License-Identifier: MIT
/*
 * apds9960 — I2C proximity, ambient light, RGB color, and gesture sensor
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 */

#include "apds9960.h"

/* --- Register map (APDS-9960 datasheet) --- */
#define REG_ENABLE      0x80u
#define REG_ATIME       0x81u
#define REG_PILT        0x89u
#define REG_PIHT        0x8Bu
#define REG_PERS        0x8Cu
#define REG_CONFIG1     0x8Du
#define REG_PPULSE      0x8Eu
#define REG_CONTROL     0x8Fu
#define REG_ID          0x92u
#define REG_STATUS      0x93u
#define REG_CDATAL      0x94u
#define REG_CDATAH      0x95u
#define REG_RDATAL      0x96u
#define REG_RDATAH      0x97u
#define REG_GDATAL      0x98u
#define REG_GDATAH      0x99u
#define REG_BDATAL      0x9Au
#define REG_BDATAH      0x9Bu
#define REG_PDATA       0x9Cu
#define REG_GPENTH      0xA0u
#define REG_GEXTH       0xA1u
#define REG_GCONF1      0xA2u
#define REG_GCONF2      0xA3u
#define REG_GCONF4      0xABu
#define REG_GFLVL       0xAEu
#define REG_GSTATUS     0xAFu
#define REG_GFIFO_U     0xFCu
#define REG_GFIFO_D     0xFDu
#define REG_GFIFO_L     0xFEu
#define REG_GFIFO_R     0xFFu

/* ENABLE register bits */
#define ENABLE_PON   (1u << 0)
#define ENABLE_AEN   (1u << 1)
#define ENABLE_PEN   (1u << 2)
#define ENABLE_GEN   (1u << 6)

/* Expected chip IDs (0xAB = original, 0xA8 = common variant) */
#define CHIP_ID_AB   0xABu
#define CHIP_ID_A8   0xA8u

/* Gesture FIFO threshold for detection */
#define GESTURE_THRESHOLD  20

/* --- Helpers --- */

static apds9960_err_t write_reg(const apds9960_t *dev, uint8_t reg, uint8_t val)
{
    if (!dev->i2c->write_reg(dev->i2c->ctx, APDS9960_DEV_ADDR, reg, val))
        return APDS9960_ERR_I2C;
    return APDS9960_OK;
}

static apds9960_err_t read_regs(const apds9960_t *dev, uint8_t reg,
                                uint8_t *buf, uint16_t len)
{
    if (!dev->i2c->read_regs(dev->i2c->ctx, APDS9960_DEV_ADDR, reg, buf, len))
        return APDS9960_ERR_I2C;
    return APDS9960_OK;
}

static apds9960_err_t read_reg(const apds9960_t *dev, uint8_t reg, uint8_t *val)
{
    return read_regs(dev, reg, val, 1);
}

/* --- Public API --- */

apds9960_err_t apds9960_init(apds9960_t *dev, const apds9960_i2c_t *i2c)
{
    if (!dev || !i2c) return APDS9960_ERR_PARAM;
    dev->i2c = i2c;

    /* Verify chip ID */
    uint8_t id = 0;
    apds9960_err_t err = read_reg(dev, REG_ID, &id);
    if (err != APDS9960_OK) return err;
    if (id != CHIP_ID_AB && id != CHIP_ID_A8) return APDS9960_ERR_ID;

    /* Power on, all features off */
    err = write_reg(dev, REG_ENABLE, ENABLE_PON);
    if (err != APDS9960_OK) return err;

    /* Default ATIME: 256 - 1 cycle (~2.78ms) */
    err = write_reg(dev, REG_ATIME, 0xFFu);
    if (err != APDS9960_OK) return err;

    /* Default proximity pulse: 8 pulses, 8us */
    err = write_reg(dev, REG_PPULSE, 0x87u);
    if (err != APDS9960_OK) return err;

    /* Default control: gain x1 for ALS/prox, LED 100mA */
    err = write_reg(dev, REG_CONTROL, 0x00u);
    if (err != APDS9960_OK) return err;

    return APDS9960_OK;
}

apds9960_err_t apds9960_enable_proximity(apds9960_t *dev)
{
    if (!dev || !dev->i2c) return APDS9960_ERR_PARAM;

    uint8_t en = 0;
    apds9960_err_t err = read_reg(dev, REG_ENABLE, &en);
    if (err != APDS9960_OK) return err;

    en |= (uint8_t)(ENABLE_PEN | ENABLE_PON);
    return write_reg(dev, REG_ENABLE, en);
}

apds9960_err_t apds9960_read_proximity(apds9960_t *dev, uint8_t *prox)
{
    if (!dev || !prox || !dev->i2c) return APDS9960_ERR_PARAM;
    return read_reg(dev, REG_PDATA, prox);
}

apds9960_err_t apds9960_enable_color(apds9960_t *dev)
{
    if (!dev || !dev->i2c) return APDS9960_ERR_PARAM;

    uint8_t en = 0;
    apds9960_err_t err = read_reg(dev, REG_ENABLE, &en);
    if (err != APDS9960_OK) return err;

    en |= (uint8_t)(ENABLE_AEN | ENABLE_PON);
    return write_reg(dev, REG_ENABLE, en);
}

apds9960_err_t apds9960_read_color(apds9960_t *dev, apds9960_color_t *color)
{
    if (!dev || !color || !dev->i2c) return APDS9960_ERR_PARAM;

    uint8_t buf[8];
    apds9960_err_t err = read_regs(dev, REG_CDATAL, buf, 8);
    if (err != APDS9960_OK) return err;

    color->c = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
    color->r = (uint16_t)((uint16_t)buf[3] << 8 | buf[2]);
    color->g = (uint16_t)((uint16_t)buf[5] << 8 | buf[4]);
    color->b = (uint16_t)((uint16_t)buf[7] << 8 | buf[6]);
    return APDS9960_OK;
}

apds9960_err_t apds9960_enable_gesture(apds9960_t *dev)
{
    if (!dev || !dev->i2c) return APDS9960_ERR_PARAM;

    /* Set gesture entry/exit thresholds */
    apds9960_err_t err = write_reg(dev, REG_GPENTH, 40);
    if (err != APDS9960_OK) return err;
    err = write_reg(dev, REG_GEXTH, 30);
    if (err != APDS9960_OK) return err;

    /* GCONF1: 4 datasets per FIFO entry, no side masking */
    err = write_reg(dev, REG_GCONF1, 0x00u);
    if (err != APDS9960_OK) return err;

    /* GCONF2: gain x1, LED 100mA, 1 wait time */
    err = write_reg(dev, REG_GCONF2, 0x00u);
    if (err != APDS9960_OK) return err;

    /* GCONF4: enable gesture interrupt, gesture mode */
    err = write_reg(dev, REG_GCONF4, 0x01u);
    if (err != APDS9960_OK) return err;

    /* Enable gesture + proximity + power */
    uint8_t en = 0;
    err = read_reg(dev, REG_ENABLE, &en);
    if (err != APDS9960_OK) return err;

    en |= (uint8_t)(ENABLE_GEN | ENABLE_PEN | ENABLE_PON);
    return write_reg(dev, REG_ENABLE, en);
}

apds9960_err_t apds9960_read_gesture(apds9960_t *dev, apds9960_gesture_t *gesture)
{
    if (!dev || !gesture || !dev->i2c) return APDS9960_ERR_PARAM;
    *gesture = APDS9960_GESTURE_NONE;

    /* Check FIFO level */
    uint8_t level = 0;
    apds9960_err_t err = read_reg(dev, REG_GFLVL, &level);
    if (err != APDS9960_OK) return err;
    if (level == 0) return APDS9960_OK;

    /* Read FIFO datasets (U, D, L, R per set) */
    uint8_t fifo[4];
    int32_t ud_delta = 0;
    int32_t lr_delta = 0;
    uint8_t count = 0;

    for (uint8_t i = 0; i < level; i++) {
        err = read_regs(dev, REG_GFIFO_U, fifo, 4);
        if (err != APDS9960_OK) return err;

        ud_delta += (int32_t)fifo[0] - (int32_t)fifo[1]; /* U - D */
        lr_delta += (int32_t)fifo[2] - (int32_t)fifo[3]; /* L - R */
        count++;
    }

    if (count == 0) return APDS9960_OK;

    /* Determine gesture from accumulated deltas */
    int32_t ud_abs = ud_delta < 0 ? -ud_delta : ud_delta;
    int32_t lr_abs = lr_delta < 0 ? -lr_delta : lr_delta;

    if (ud_abs < GESTURE_THRESHOLD && lr_abs < GESTURE_THRESHOLD)
        return APDS9960_OK;

    if (ud_abs > lr_abs) {
        *gesture = ud_delta > 0 ? APDS9960_GESTURE_UP : APDS9960_GESTURE_DOWN;
    } else {
        *gesture = lr_delta > 0 ? APDS9960_GESTURE_LEFT : APDS9960_GESTURE_RIGHT;
    }

    return APDS9960_OK;
}

apds9960_err_t apds9960_disable_all(apds9960_t *dev)
{
    if (!dev || !dev->i2c) return APDS9960_ERR_PARAM;
    return write_reg(dev, REG_ENABLE, 0x00u);
}
