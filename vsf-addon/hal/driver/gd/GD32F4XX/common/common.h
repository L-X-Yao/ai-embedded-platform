/*****************************************************************************
 *   Copyright(C)2009-2022 by VSF Team                                       *
 *                                                                           *
 *  Licensed under the Apache License, Version 2.0 (the "License");          *
 *  you may not use this file except in compliance with the License.         *
 *  You may obtain a copy of the License at                                  *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 *  Unless required by applicable law or agreed to in writing, software      *
 *  distributed under the License is distributed on an "AS IS" BASIS,        *
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *  See the License for the specific language governing permissions and      *
 *  limitations under the License.                                           *
 *                                                                           *
 ****************************************************************************/

#ifndef __HAL_DRIVER_GIGADEVICE_GD32F4XX_COMMON_H__
#define __HAL_DRIVER_GIGADEVICE_GD32F4XX_COMMON_H__

/*============================ INCLUDES ======================================*/

#include "hal/vsf_hal_cfg.h"
#include "hal/arch/vsf_arch.h"

/*============================ MACROS ========================================*/

/*
 * VSF_HW_CLKRST_REGION encodes an RCU register bit as:
 *   word_offset = (RCU register byte offset from RCU_BASE) / 4
 *   bit_offset  = bit position within that register
 *   bit_length  = number of bits (typically 1 for enable/reset)
 *
 * GD32F4xx RCU register byte offsets (from RCU_BASE = 0x40023800):
 *   0x10 AHB1RST  → word 0x04      0x30 AHB1EN  → word 0x0C
 *   0x14 AHB2RST  → word 0x05      0x34 AHB2EN  → word 0x0D
 *   0x18 AHB3RST  → word 0x06      0x38 AHB3EN  → word 0x0E
 *   0x20 APB1RST  → word 0x08      0x40 APB1EN  → word 0x10
 *   0x24 APB2RST  → word 0x09      0x44 APB2EN  → word 0x11
 */

#define VSF_HW_CLKRST_REGION(__WORD_OFFSET, __BIT_OFFSET, __BIT_LENGTH)         \
            (((__WORD_OFFSET) << 16) | ((__BIT_LENGTH) << 8) | ((__BIT_OFFSET) << 0))

/*============================ MACROFIED FUNCTIONS ===========================*/

#define vsf_hw_peripheral_clk_set       vsf_hw_clkrst_region_set
#define vsf_hw_peripheral_clk_get       vsf_hw_clkrst_region_get

#define vsf_hw_peripheral_rst_set       vsf_hw_clkrst_region_set_bit
#define vsf_hw_peripheral_rst_clear     vsf_hw_clkrst_region_clear_bit
#define vsf_hw_peripheral_rst_get       vsf_hw_clkrst_region_get_bit

#define vsf_hw_peripheral_enable        vsf_hw_clkrst_region_set_bit
#define vsf_hw_peripheral_disable       vsf_hw_clkrst_region_clear_bit

/*============================ TYPES =========================================*/

// ======================== RST (Reset) ========================

typedef enum vsf_hw_peripheral_rst_t {
    // RCU_AHB1RST (offset 0x10, word 0x04)
    VSF_HW_RST_GPIOA               = VSF_HW_CLKRST_REGION(0x04, 0, 1),
    VSF_HW_RST_GPIOB               = VSF_HW_CLKRST_REGION(0x04, 1, 1),
    VSF_HW_RST_GPIOC               = VSF_HW_CLKRST_REGION(0x04, 2, 1),
    VSF_HW_RST_GPIOD               = VSF_HW_CLKRST_REGION(0x04, 3, 1),
    VSF_HW_RST_GPIOE               = VSF_HW_CLKRST_REGION(0x04, 4, 1),
    VSF_HW_RST_GPIOF               = VSF_HW_CLKRST_REGION(0x04, 5, 1),
    VSF_HW_RST_GPIOG               = VSF_HW_CLKRST_REGION(0x04, 6, 1),
    VSF_HW_RST_GPIOH               = VSF_HW_CLKRST_REGION(0x04, 7, 1),
    VSF_HW_RST_GPIOI               = VSF_HW_CLKRST_REGION(0x04, 8, 1),
    VSF_HW_RST_DMA0                = VSF_HW_CLKRST_REGION(0x04, 21, 1),
    VSF_HW_RST_DMA1                = VSF_HW_CLKRST_REGION(0x04, 22, 1),

    // RCU_APB1RST (offset 0x20, word 0x08)
    VSF_HW_RST_SPI1                = VSF_HW_CLKRST_REGION(0x08, 14, 1),
    VSF_HW_RST_SPI2                = VSF_HW_CLKRST_REGION(0x08, 15, 1),
    VSF_HW_RST_USART1              = VSF_HW_CLKRST_REGION(0x08, 17, 1),
    VSF_HW_RST_USART2              = VSF_HW_CLKRST_REGION(0x08, 18, 1),
    VSF_HW_RST_UART3               = VSF_HW_CLKRST_REGION(0x08, 19, 1),
    VSF_HW_RST_UART4               = VSF_HW_CLKRST_REGION(0x08, 20, 1),
    VSF_HW_RST_I2C0                = VSF_HW_CLKRST_REGION(0x08, 21, 1),
    VSF_HW_RST_I2C1                = VSF_HW_CLKRST_REGION(0x08, 22, 1),
    VSF_HW_RST_I2C2                = VSF_HW_CLKRST_REGION(0x08, 23, 1),

    // RCU_APB2RST (offset 0x24, word 0x09)
    VSF_HW_RST_USART0              = VSF_HW_CLKRST_REGION(0x09, 4, 1),
    VSF_HW_RST_USART5              = VSF_HW_CLKRST_REGION(0x09, 5, 1),
    VSF_HW_RST_SPI0                = VSF_HW_CLKRST_REGION(0x09, 12, 1),
    VSF_HW_RST_SPI3                = VSF_HW_CLKRST_REGION(0x09, 13, 1),
    VSF_HW_RST_SPI4                = VSF_HW_CLKRST_REGION(0x09, 20, 1),
    VSF_HW_RST_SPI5                = VSF_HW_CLKRST_REGION(0x09, 21, 1),
} vsf_hw_peripheral_rst_t;

// ======================== EN (Clock Enable) ========================

typedef enum vsf_hw_peripheral_en_t {
    // RCU_AHB1EN (offset 0x30, word 0x0C)
    VSF_HW_EN_GPIOA                = VSF_HW_CLKRST_REGION(0x0C, 0, 1),
    VSF_HW_EN_GPIOB                = VSF_HW_CLKRST_REGION(0x0C, 1, 1),
    VSF_HW_EN_GPIOC                = VSF_HW_CLKRST_REGION(0x0C, 2, 1),
    VSF_HW_EN_GPIOD                = VSF_HW_CLKRST_REGION(0x0C, 3, 1),
    VSF_HW_EN_GPIOE                = VSF_HW_CLKRST_REGION(0x0C, 4, 1),
    VSF_HW_EN_GPIOF                = VSF_HW_CLKRST_REGION(0x0C, 5, 1),
    VSF_HW_EN_GPIOG                = VSF_HW_CLKRST_REGION(0x0C, 6, 1),
    VSF_HW_EN_GPIOH                = VSF_HW_CLKRST_REGION(0x0C, 7, 1),
    VSF_HW_EN_GPIOI                = VSF_HW_CLKRST_REGION(0x0C, 8, 1),
    VSF_HW_EN_DMA0                 = VSF_HW_CLKRST_REGION(0x0C, 21, 1),
    VSF_HW_EN_DMA1                 = VSF_HW_CLKRST_REGION(0x0C, 22, 1),

    // RCU_APB1EN (offset 0x40, word 0x10)
    VSF_HW_EN_SPI1                 = VSF_HW_CLKRST_REGION(0x10, 14, 1),
    VSF_HW_EN_SPI2                 = VSF_HW_CLKRST_REGION(0x10, 15, 1),
    VSF_HW_EN_USART1               = VSF_HW_CLKRST_REGION(0x10, 17, 1),
    VSF_HW_EN_USART2               = VSF_HW_CLKRST_REGION(0x10, 18, 1),
    VSF_HW_EN_UART3                = VSF_HW_CLKRST_REGION(0x10, 19, 1),
    VSF_HW_EN_UART4                = VSF_HW_CLKRST_REGION(0x10, 20, 1),
    VSF_HW_EN_I2C0                 = VSF_HW_CLKRST_REGION(0x10, 21, 1),
    VSF_HW_EN_I2C1                 = VSF_HW_CLKRST_REGION(0x10, 22, 1),
    VSF_HW_EN_I2C2                 = VSF_HW_CLKRST_REGION(0x10, 23, 1),

    // RCU_APB2EN (offset 0x44, word 0x11)
    VSF_HW_EN_USART0               = VSF_HW_CLKRST_REGION(0x11, 4, 1),
    VSF_HW_EN_USART5               = VSF_HW_CLKRST_REGION(0x11, 5, 1),
    VSF_HW_EN_SPI0                 = VSF_HW_CLKRST_REGION(0x11, 12, 1),
    VSF_HW_EN_SPI3                 = VSF_HW_CLKRST_REGION(0x11, 13, 1),
    VSF_HW_EN_SPI4                 = VSF_HW_CLKRST_REGION(0x11, 20, 1),
    VSF_HW_EN_SPI5                 = VSF_HW_CLKRST_REGION(0x11, 21, 1),
} vsf_hw_peripheral_en_t;

/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
/*============================ PROTOTYPES ====================================*/

extern void vsf_hw_clkrst_region_set(uint32_t region, uint_fast8_t value);
extern uint_fast8_t vsf_hw_clkrst_region_get(uint32_t region);
extern void vsf_hw_clkrst_region_set_bit(uint32_t region);
extern void vsf_hw_clkrst_region_clear_bit(uint32_t region);
extern uint_fast8_t vsf_hw_clkrst_region_get_bit(uint32_t region);

#endif      // __HAL_DRIVER_GIGADEVICE_GD32F4XX_COMMON_H__
/* EOF */
