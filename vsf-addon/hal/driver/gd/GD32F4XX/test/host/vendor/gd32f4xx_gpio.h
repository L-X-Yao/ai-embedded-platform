// SPDX-License-Identifier: MIT
/*
 * Host model of the GPIO registers. Accesses to a port whose RCU clock is off go
 * nowhere, as on the chip. Offsets: GD32F4xx user manual Rev3.0, GPIO chapter.
 */
#ifndef HOST_GD32F4XX_GPIO_H
#define HOST_GD32F4XX_GPIO_H

#include <stdint.h>

extern volatile uint32_t *sim_gpio_reg(uint32_t reg, uint32_t off);

#define GPIO_CTL(r)         (*sim_gpio_reg((r), 0x00U))
#define GPIO_OMODE(r)       (*sim_gpio_reg((r), 0x04U))
#define GPIO_OSPD(r)        (*sim_gpio_reg((r), 0x08U))
#define GPIO_PUD(r)         (*sim_gpio_reg((r), 0x0CU))
#define GPIO_ISTAT(r)       (*sim_gpio_reg((r), 0x10U))
#define GPIO_OCTL(r)        (*sim_gpio_reg((r), 0x14U))
#define GPIO_AFSEL0(r)      (*sim_gpio_reg((r), 0x20U))
#define GPIO_AFSEL1(r)      (*sim_gpio_reg((r), 0x24U))
#define GPIO_TG(r)          (*sim_gpio_reg((r), 0x2CU))

#endif
