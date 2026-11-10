// SPDX-License-Identifier: MIT
/*
 * Host model of the I2C registers. Every access goes through sim_i2c_reg(), so
 * test_i2c.c can apply the side effects of reading STAT1 and of reading or
 * writing DATA. Bit positions: GD32F4xx user manual Rev3.0, section 20.4.
 */
#ifndef HOST_GD32F4XX_I2C_H
#define HOST_GD32F4XX_I2C_H

#include <stdint.h>

extern volatile uint32_t *sim_i2c_reg(uint32_t reg, uint32_t off);

#define BIT(x)              ((uint32_t)1U << (x))
#define BITS(s, e)          ((0xFFFFFFFFUL << (s)) & (0xFFFFFFFFUL >> (31U - (uint32_t)(e))))

#define I2C_CTL0(r)         (*sim_i2c_reg((r), 0x00U))
#define I2C_CTL1(r)         (*sim_i2c_reg((r), 0x04U))
#define I2C_DATA(r)         (*sim_i2c_reg((r), 0x10U))
#define I2C_STAT0(r)        (*sim_i2c_reg((r), 0x14U))
#define I2C_STAT1(r)        (*sim_i2c_reg((r), 0x18U))
#define I2C_CKCFG(r)        (*sim_i2c_reg((r), 0x1CU))
#define I2C_RT(r)           (*sim_i2c_reg((r), 0x20U))

#define I2C_CTL0_I2CEN      BIT(0)
#define I2C_CTL0_START      BIT(8)
#define I2C_CTL0_STOP       BIT(9)
#define I2C_CTL0_ACKEN      BIT(10)
#define I2C_CTL0_POAP       BIT(11)

#define I2C_CTL1_ERRIE      BIT(8)
#define I2C_CTL1_EVIE       BIT(9)
#define I2C_CTL1_BUFIE      BIT(10)

#define I2C_STAT0_SBSEND    BIT(0)
#define I2C_STAT0_ADDSEND   BIT(1)
#define I2C_STAT0_BTC       BIT(2)
#define I2C_STAT0_ADD10SEND BIT(3)
#define I2C_STAT0_RBNE      BIT(6)
#define I2C_STAT0_TBE       BIT(7)
#define I2C_STAT0_BERR      BIT(8)
#define I2C_STAT0_LOSTARB   BIT(9)
#define I2C_STAT0_AERR      BIT(10)
#define I2C_STAT0_OUERR     BIT(11)

#define I2C_STAT1_I2CBSY    BIT(1)

#define I2C_CKCFG_FAST      BIT(15)

#endif
