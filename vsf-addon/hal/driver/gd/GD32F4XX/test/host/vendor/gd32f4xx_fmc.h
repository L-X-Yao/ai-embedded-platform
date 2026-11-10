// SPDX-License-Identifier: MIT
/*
 * Host model of the FMC registers: plain variables that test_flash.c drives.
 * Bit positions: GD32F4xx user manual Rev3.0, sections 2.4.4 and 2.4.5.
 */
#ifndef HOST_GD32F4XX_FMC_H
#define HOST_GD32F4XX_FMC_H

#include <stdint.h>

extern volatile uint32_t sim_fmc_ctl, sim_fmc_stat, sim_fmc_key;

#define FMC_CTL             sim_fmc_ctl
#define FMC_STAT            sim_fmc_stat
#define FMC_KEY             sim_fmc_key

#define FMC_STAT_END        (1U << 0)
#define FMC_STAT_OPERR      (1U << 1)
#define FMC_STAT_WPERR      (1U << 4)
#define FMC_STAT_PGMERR     (1U << 6)
#define FMC_STAT_PGSERR     (1U << 7)
#define FMC_STAT_RDDERR     (1U << 8)
#define FMC_STAT_BUSY       (1U << 16)

#define FMC_CTL_PG          (1U << 0)
#define FMC_CTL_SER         (1U << 1)
#define FMC_CTL_MER0        (1U << 2)
#define FMC_CTL_SN          (0x1FU << 3)
#define FMC_CTL_PSZ         (3U << 8)
#define FMC_CTL_MER1        (1U << 15)
#define FMC_CTL_START       (1U << 16)
#define FMC_CTL_ENDIE       (1U << 24)
#define FMC_CTL_ERRIE       (1U << 25)
#define FMC_CTL_LK          (1U << 31)

#define UNLOCK_KEY0         0x45670123U
#define UNLOCK_KEY1         0xCDEF89ABU

#endif
