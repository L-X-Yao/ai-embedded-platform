/*
 * qmc5883p — QMC5883P 3-axis magnetometer driver (I2C)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal
 */

#ifndef QMC5883P_H
#define QMC5883P_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QMC5883P_OK            =  0,
    QMC5883P_ERR_I2C       = -1,
    QMC5883P_ERR_ID        = -2,
    QMC5883P_ERR_NOT_READY = -3,
} qmc5883p_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} qmc5883p_i2c_t;

#define QMC5883P_DEV_ADDR   0x2Cu
#define QMC5883P_CHIP_ID    0x80u

typedef struct {
    float mx, my, mz;
} qmc5883p_data_t;

/** Configure and verify chip ID. @thread_safe: caller_must_lock */
qmc5883p_err_t qmc5883p_init(const qmc5883p_i2c_t *i2c);

/** Read magnetometer sample (Gauss). @thread_safe: caller_must_lock */
qmc5883p_err_t qmc5883p_read(const qmc5883p_i2c_t *i2c, qmc5883p_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* QMC5883P_H */
