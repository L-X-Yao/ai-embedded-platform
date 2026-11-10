// SPDX-License-Identifier: MIT
/*
 * Host model of the USART registers. STAT0 and DATA have side effects, so every
 * access goes through sim_usart_reg(). Bit positions: GD32F4xx user manual
 * Rev3.0, section 19.4.
 */
#ifndef HOST_GD32F4XX_USART_H
#define HOST_GD32F4XX_USART_H

#include <stdint.h>

extern volatile uint32_t *sim_usart_reg(uint32_t reg, uint32_t off);

#define BIT(x)              ((uint32_t)1U << (x))
#define BITS(s, e)          ((0xFFFFFFFFUL << (s)) & (0xFFFFFFFFUL >> (31U - (uint32_t)(e))))

#define USART_STAT0(r)      (*sim_usart_reg((r), 0x00U))
#define USART_DATA(r)       (*sim_usart_reg((r), 0x04U))
#define USART_BAUD(r)       (*sim_usart_reg((r), 0x08U))
#define USART_CTL0(r)       (*sim_usart_reg((r), 0x0CU))
#define USART_CTL1(r)       (*sim_usart_reg((r), 0x10U))
#define USART_CTL2(r)       (*sim_usart_reg((r), 0x14U))
#define USART_STAT1(r)      (*sim_usart_reg((r), 0x88U))

#define USART_STAT0_PERR    BIT(0)
#define USART_STAT0_FERR    BIT(1)
#define USART_STAT0_NERR    BIT(2)
#define USART_STAT0_ORERR   BIT(3)
#define USART_STAT0_IDLEF   BIT(4)
#define USART_STAT0_RBNE    BIT(5)
#define USART_STAT0_TC      BIT(6)
#define USART_STAT0_TBE     BIT(7)
#define USART_STAT0_LBDF    BIT(8)
#define USART_STAT0_CTSF    BIT(9)

#define USART_DATA_DATA     BITS(0, 8)
#define USART_BAUD_FRADIV   BITS(0, 3)
#define USART_BAUD_INTDIV   BITS(4, 15)

#define USART_CTL0_SBKCMD   BIT(0)
#define USART_CTL0_REN      BIT(2)
#define USART_CTL0_TEN      BIT(3)
#define USART_CTL0_RBNEIE   BIT(5)
#define USART_CTL0_TBEIE    BIT(7)
#define USART_CTL0_PERRIE   BIT(8)
#define USART_CTL0_PM       BIT(9)
#define USART_CTL0_PCEN     BIT(10)
#define USART_CTL0_WL       BIT(12)
#define USART_CTL0_UEN      BIT(13)
#define USART_CTL0_OVSMOD   BIT(15)

#define USART_CTL1_CLEN     BIT(8)
#define USART_CTL1_CPH      BIT(9)
#define USART_CTL1_CPL      BIT(10)
#define USART_CTL1_CKEN     BIT(11)
#define USART_CTL1_STB      BITS(12, 13)

#define USART_CTL2_HDEN     BIT(3)
#define USART_CTL2_RTSEN    BIT(8)
#define USART_CTL2_CTSEN    BIT(9)
#define USART_CTL2_CTSIE    BIT(10)

#endif
