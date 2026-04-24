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

#ifndef __HAL_DRIVER_GIGADEVICE_GD32F4XX_I2C_H__
#define __HAL_DRIVER_GIGADEVICE_GD32F4XX_I2C_H__

/*============================ INCLUDES ======================================*/

#include "hal/vsf_hal_cfg.h"

#if VSF_HAL_USE_I2C == ENABLED

#include "../../__device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================ MACROS ========================================*/

#ifndef VSF_HW_I2C_CFG_MULTI_CLASS
#   define VSF_HW_I2C_CFG_MULTI_CLASS               VSF_I2C_CFG_MULTI_CLASS
#endif

#define VSF_I2C_CFG_REIMPLEMENT_TYPE_MODE           ENABLED
#define VSF_I2C_CFG_REIMPLEMENT_TYPE_CMD            ENABLED
#define VSF_I2C_CFG_REIMPLEMENT_TYPE_IRQ_MASK       ENABLED

/*============================ TYPES =========================================*/

typedef enum vsf_i2c_mode_t {
    VSF_I2C_MODE_MASTER                 = (1 << 0),
    VSF_I2C_MODE_SLAVE                  = (0 << 0),

    VSF_I2C_SPEED_STANDARD_MODE         = (0 << 1),
    VSF_I2C_SPEED_FAST_MODE             = (1 << 1),
    VSF_I2C_SPEED_FAST_MODE_PLUS        = (2 << 1),
    VSF_I2C_SPEED_HIGH_SPEED_MODE       = (3 << 1),

    VSF_I2C_ADDR_7_BITS                 = (0 << 4),
    VSF_I2C_ADDR_10_BITS                = (1 << 4),

    __VSF_HW_I2C_MODE_MASK              = VSF_I2C_MODE_MASTER
                                        | (3 << 1)
                                        | VSF_I2C_ADDR_10_BITS,
    __VSF_HW_I2C_NOT_SUPPORT_MASK       = VSF_I2C_SPEED_FAST_MODE_PLUS
                                        | VSF_I2C_SPEED_HIGH_SPEED_MODE,
} vsf_i2c_mode_t;

typedef enum vsf_i2c_cmd_t {
    VSF_I2C_CMD_WRITE                   = (0 << 0),
    VSF_I2C_CMD_READ                    = (1 << 0),

    VSF_I2C_CMD_START                   = (1 << 1),
    VSF_I2C_CMD_STOP                    = (1 << 2),
    VSF_I2C_CMD_RESTART                 = (1 << 3),
    VSF_I2C_CMD_NO_START                = (0 << 1),
    VSF_I2C_CMD_NO_STOP                 = (0 << 2),
    VSF_I2C_CMD_NO_RESTART              = (0 << 3),

    VSF_I2C_CMD_7_BITS                  = (0 << 4),
    VSF_I2C_CMD_10_BITS                 = (1 << 4),

    __VSF_I2C_CMD_MASK                  = VSF_I2C_CMD_READ
                                        | VSF_I2C_CMD_START
                                        | VSF_I2C_CMD_STOP
                                        | VSF_I2C_CMD_RESTART
                                        | VSF_I2C_CMD_10_BITS,
} vsf_i2c_cmd_t;

typedef enum vsf_i2c_irq_mask_t {
    VSF_I2C_IRQ_MASK_MASTER_TX                  = (1 << 0),
    VSF_I2C_IRQ_MASK_MASTER_RX                  = (1 << 1),
    VSF_I2C_IRQ_MASK_MASTER_TRANSFER_COMPLETE   = (1 << 2),
    VSF_I2C_IRQ_MASK_MASTER_ARBITRATION_LOST    = (1 << 3),
    VSF_I2C_IRQ_MASK_MASTER_ADDRESS_NACK        = (1 << 4),
    VSF_I2C_IRQ_MASK_MASTER_NACK_DETECT         = (1 << 5),

    VSF_I2C_IRQ_MASK_MASTER_TX_NACK_DETECT      = (1 << 6),
    VSF_I2C_IRQ_MASK_MASTER_STARTED             = (1 << 10),
    VSF_I2C_IRQ_MASK_MASTER_STOPPED             = (1 << 11),
    VSF_I2C_IRQ_MASK_MASTER_START_OR_RESTART_DETECT = (1 << 12),
    VSF_I2C_IRQ_MASK_MASTER_STOP_DETECT         = (1 << 13),

    // slave mode (not implemented, required by template)
    VSF_I2C_IRQ_MASK_SLAVE_START_OR_RESTART_DETECT = (1 << 16),
    VSF_I2C_IRQ_MASK_SLAVE_STOP_DETECT           = (1 << 17),
    VSF_I2C_IRQ_MASK_SLAVE_TX                    = (1 << 18),
    VSF_I2C_IRQ_MASK_SLAVE_RX                    = (1 << 19),
    VSF_I2C_IRQ_MASK_SLAVE_TRANSFER_COMPLETE     = (1 << 20),

    __VSF_HW_I2C_IRQ_MASK              = VSF_I2C_IRQ_MASK_MASTER_TRANSFER_COMPLETE
                                        | VSF_I2C_IRQ_MASK_MASTER_ARBITRATION_LOST
                                        | VSF_I2C_IRQ_MASK_MASTER_ADDRESS_NACK
                                        | VSF_I2C_IRQ_MASK_MASTER_NACK_DETECT,
} vsf_i2c_irq_mask_t;

/*============================ PROTOTYPES ====================================*/

#ifdef __cplusplus
}
#endif

#endif      // VSF_HAL_USE_I2C
#endif      // __HAL_DRIVER_GIGADEVICE_GD32F4XX_I2C_H__
/* EOF */
