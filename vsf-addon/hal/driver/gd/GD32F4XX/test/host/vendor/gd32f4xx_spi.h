// SPDX-License-Identifier: MIT
/*
 * Host model of the SPI registers. STAT and DATA have side effects, so every
 * access goes through sim_spi_reg(). Bit positions: GD32F4xx user manual Rev3.0,
 * section 21.11.
 */
#ifndef HOST_GD32F4XX_SPI_H
#define HOST_GD32F4XX_SPI_H

#include <stdint.h>

extern volatile uint32_t *sim_spi_reg(uint32_t reg, uint32_t off);

#define BIT(x)              ((uint32_t)1U << (x))

#define SPI_CTL0(r)         (*sim_spi_reg((r), 0x00U))
#define SPI_CTL1(r)         (*sim_spi_reg((r), 0x04U))
#define SPI_STAT(r)         (*sim_spi_reg((r), 0x08U))
#define SPI_DATA(r)         (*sim_spi_reg((r), 0x0CU))

#define SPI_CTL0_MSTMOD     BIT(2)
#define SPI_CTL0_SPIEN      BIT(6)
#define SPI_CTL0_SWNSS      BIT(8)
#define SPI_CTL0_SWNSSEN    BIT(9)
#define SPI_CTL0_FF16       BIT(11)

#define SPI_CTL1_NSSDRV     BIT(2)
#define SPI_CTL1_ERRIE      BIT(5)
#define SPI_CTL1_RBNEIE     BIT(6)
#define SPI_CTL1_TBEIE      BIT(7)

#define SPI_STAT_RBNE       BIT(0)
#define SPI_STAT_TBE        BIT(1)
#define SPI_STAT_CONFERR    BIT(5)
#define SPI_STAT_RXORERR    BIT(6)
#define SPI_STAT_TRANS      BIT(7)

#endif
