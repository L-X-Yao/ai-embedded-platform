#include "qmc5883p.h"

#define REG_CHIP_ID     0x00u
#define REG_DATA_X_LSB  0x01u
#define REG_STATUS      0x09u
#define REG_CR1         0x0Au
#define REG_CR2         0x0Bu
#define REG_SIGN        0x29u

#define CR1_ODR_200HZ   0x0Cu
#define CR1_OSR_8X      0x00u
#define CR1_MODE_CONT   0x03u

#define CR2_RANGE_2G    (0x03u << 2)
#define CR2_SOFT_RST    0x80u

#define STATUS_DRDY     0x01u

#define SENSITIVITY_2G  15000.0f

qmc5883p_err_t qmc5883p_init(const qmc5883p_i2c_t *i2c)
{
    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, QMC5883P_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return QMC5883P_ERR_I2C;
    if (id != QMC5883P_CHIP_ID)
        return QMC5883P_ERR_ID;

    if (!i2c->write_reg(i2c->ctx, QMC5883P_DEV_ADDR, REG_CR2, CR2_SOFT_RST))
        return QMC5883P_ERR_I2C;

    for (volatile uint32_t d = 0; d < 200000U; d++) { }

    if (!i2c->write_reg(i2c->ctx, QMC5883P_DEV_ADDR, REG_SIGN, 0x06u))
        return QMC5883P_ERR_I2C;
    if (!i2c->write_reg(i2c->ctx, QMC5883P_DEV_ADDR, REG_CR2, CR2_RANGE_2G))
        return QMC5883P_ERR_I2C;
    if (!i2c->write_reg(i2c->ctx, QMC5883P_DEV_ADDR, REG_CR1,
                         CR1_OSR_8X | CR1_ODR_200HZ | CR1_MODE_CONT))
        return QMC5883P_ERR_I2C;

    return QMC5883P_OK;
}

qmc5883p_err_t qmc5883p_read(const qmc5883p_i2c_t *i2c, qmc5883p_data_t *out)
{
    uint8_t st = 0;
    if (!i2c->read_regs(i2c->ctx, QMC5883P_DEV_ADDR, REG_STATUS, &st, 1))
        return QMC5883P_ERR_I2C;
    if (!(st & STATUS_DRDY))
        return QMC5883P_ERR_NOT_READY;

    uint8_t buf[6];
    if (!i2c->read_regs(i2c->ctx, QMC5883P_DEV_ADDR, REG_DATA_X_LSB, buf, 6))
        return QMC5883P_ERR_I2C;

    int16_t raw_x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    int16_t raw_y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    int16_t raw_z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    out->mx = (float)raw_x / SENSITIVITY_2G;
    out->my = (float)raw_y / SENSITIVITY_2G;
    out->mz = (float)raw_z / SENSITIVITY_2G;

    return QMC5883P_OK;
}
