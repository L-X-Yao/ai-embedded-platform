// SPDX-License-Identifier: MIT
/*
 * test_dummy2 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#include "test_dummy2.h"

/* TODO: register defines */
#define REG_CHIP_ID     0x00u

test_dummy2_err_t test_dummy2_init(test_dummy2_t *dev,
                               const test_dummy2_i2c_t *i2c)
{
    if (!dev || !i2c) return TEST_DUMMY2_ERR_PARAM;
    dev->i2c = i2c;

    uint8_t id = 0;
    if (!i2c->read_regs(i2c->ctx, TEST_DUMMY2_DEV_ADDR, REG_CHIP_ID, &id, 1))
        return TEST_DUMMY2_ERR_I2C;

    /* TODO: check chip ID */
    /* TODO: configure sensor */

    return TEST_DUMMY2_OK;
}

test_dummy2_err_t test_dummy2_read(test_dummy2_t *dev,
                               test_dummy2_data_t *out)
{
    if (!dev || !out) return TEST_DUMMY2_ERR_PARAM;
    (void)dev;
    /* TODO: implement */
    return TEST_DUMMY2_ERR_I2C;
}
