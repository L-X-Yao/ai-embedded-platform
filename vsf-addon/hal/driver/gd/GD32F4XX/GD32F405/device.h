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

/*============================ INCLUDES ======================================*/

#include "hal/vsf_hal_cfg.h"

/*============================ MACROS ========================================*/

#ifdef __VSF_HEADER_ONLY_SHOW_ARCH_INFO__

#   ifndef __CPU_ARM__
#       define __CPU_ARM__
#   endif

#   define VSF_ARCH_PRI_NUM             16
#   define VSF_ARCH_PRI_BIT             4

#   ifndef VSF_DEV_SWI_NUM
#       define VSF_DEV_SWI_NUM          0
#   endif

#elif defined(__VSF_HAL_SHOW_VENDOR_INFO__)

#   define __VSF_HEADER_ONLY_SHOW_VENDOR_INFO__
#   include "../common/vendor/Include/gd32f4xx.h"

#else

#ifndef __HAL_DEVICE_GIGADEVICE_GD32F405_H__
#define __HAL_DEVICE_GIGADEVICE_GD32F405_H__

/*============================ INCLUDES ======================================*/

#include "../common/common.h"

/*============================ MACROS ========================================*/

// GPIO: 9 ports (A-I), 16 pins each
#define VSF_HW_GPIO_PORT_COUNT          9
#define VSF_HW_GPIO_PORT_MASK           0x1FF
#define VSF_HW_GPIO_PIN_COUNT           16
#define VSF_HW_GPIO_FUNCTION_MAX        16
#define VSF_HW_GPIO_PORT0_REG_BASE      GPIOA
#define VSF_HW_GPIO_PORT0_EN            VSF_HW_EN_GPIOA
#define VSF_HW_GPIO_PORT0_RST           VSF_HW_RST_GPIOA
#define VSF_HW_GPIO_PORT1_REG_BASE      GPIOB
#define VSF_HW_GPIO_PORT1_EN            VSF_HW_EN_GPIOB
#define VSF_HW_GPIO_PORT1_RST           VSF_HW_RST_GPIOB
#define VSF_HW_GPIO_PORT2_REG_BASE      GPIOC
#define VSF_HW_GPIO_PORT2_EN            VSF_HW_EN_GPIOC
#define VSF_HW_GPIO_PORT2_RST           VSF_HW_RST_GPIOC
#define VSF_HW_GPIO_PORT3_REG_BASE      GPIOD
#define VSF_HW_GPIO_PORT3_EN            VSF_HW_EN_GPIOD
#define VSF_HW_GPIO_PORT3_RST           VSF_HW_RST_GPIOD
#define VSF_HW_GPIO_PORT4_REG_BASE      GPIOE
#define VSF_HW_GPIO_PORT4_EN            VSF_HW_EN_GPIOE
#define VSF_HW_GPIO_PORT4_RST           VSF_HW_RST_GPIOE
#define VSF_HW_GPIO_PORT5_REG_BASE      GPIOF
#define VSF_HW_GPIO_PORT5_EN            VSF_HW_EN_GPIOF
#define VSF_HW_GPIO_PORT5_RST           VSF_HW_RST_GPIOF
#define VSF_HW_GPIO_PORT6_REG_BASE      GPIOG
#define VSF_HW_GPIO_PORT6_EN            VSF_HW_EN_GPIOG
#define VSF_HW_GPIO_PORT6_RST           VSF_HW_RST_GPIOG
#define VSF_HW_GPIO_PORT7_REG_BASE      GPIOH
#define VSF_HW_GPIO_PORT7_EN            VSF_HW_EN_GPIOH
#define VSF_HW_GPIO_PORT7_RST           VSF_HW_RST_GPIOH
#define VSF_HW_GPIO_PORT8_REG_BASE      GPIOI
#define VSF_HW_GPIO_PORT8_EN            VSF_HW_EN_GPIOI
#define VSF_HW_GPIO_PORT8_RST           VSF_HW_RST_GPIOI

// USART: 6 units (USART0/1/2/5 sync, UART3/4 async-only)
// Flybox uses: USART0 (debug PA9/PA10), USART1 (Air780EG PA2/PA3)
#define VSF_HW_USART_COUNT              6
#define VSF_HW_USART0_REG_BASE         USART0
#define VSF_HW_USART0_EN               VSF_HW_EN_USART0
#define VSF_HW_USART0_RST              VSF_HW_RST_USART0
#define VSF_HW_USART0_SYNC             true
#define VSF_HW_USART0_IRQN             USART0_IRQn
#define VSF_HW_USART0_IRQHandler       USART0_IRQHandler
#define VSF_HW_USART1_REG_BASE         USART1
#define VSF_HW_USART1_EN               VSF_HW_EN_USART1
#define VSF_HW_USART1_RST              VSF_HW_RST_USART1
#define VSF_HW_USART1_SYNC             true
#define VSF_HW_USART1_IRQN             USART1_IRQn
#define VSF_HW_USART1_IRQHandler       USART1_IRQHandler
#define VSF_HW_USART2_REG_BASE         USART2
#define VSF_HW_USART2_EN               VSF_HW_EN_USART2
#define VSF_HW_USART2_RST              VSF_HW_RST_USART2
#define VSF_HW_USART2_SYNC             true
#define VSF_HW_USART2_IRQN             USART2_IRQn
#define VSF_HW_USART2_IRQHandler       USART2_IRQHandler
#define VSF_HW_USART3_REG_BASE         UART3
#define VSF_HW_USART3_EN               VSF_HW_EN_UART3
#define VSF_HW_USART3_RST              VSF_HW_RST_UART3
#define VSF_HW_USART3_SYNC             false
#define VSF_HW_USART3_IRQN             UART3_IRQn
#define VSF_HW_USART3_IRQHandler       UART3_IRQHandler
#define VSF_HW_USART4_REG_BASE         UART4
#define VSF_HW_USART4_EN               VSF_HW_EN_UART4
#define VSF_HW_USART4_RST              VSF_HW_RST_UART4
#define VSF_HW_USART4_SYNC             false
#define VSF_HW_USART4_IRQN             UART4_IRQn
#define VSF_HW_USART4_IRQHandler       UART4_IRQHandler
#define VSF_HW_USART5_REG_BASE         USART5
#define VSF_HW_USART5_EN               VSF_HW_EN_USART5
#define VSF_HW_USART5_RST              VSF_HW_RST_USART5
#define VSF_HW_USART5_SYNC             true
#define VSF_HW_USART5_IRQN             USART5_IRQn
#define VSF_HW_USART5_IRQHandler       USART5_IRQHandler

// Flash
#define VSF_HW_FLASH_COUNT              1
#define VSF_HW_FLASH0_ADDR              0x08000000
#define VSF_HW_FLASH0_REG_BASE          FMC
#define VSF_HW_FLASH0_IRQN              FMC_IRQn
#define VSF_HW_FLASH0_IRQHandler        FMC_IRQHandler

/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
/*============================ PROTOTYPES ====================================*/

#endif      // __HAL_DEVICE_GIGADEVICE_GD32F405_H__
#endif      // __VSF_HEADER_ONLY_SHOW_ARCH_INFO__
/* EOF */
