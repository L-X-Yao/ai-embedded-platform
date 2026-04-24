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

#ifndef __HAL_DRIVER_GIGADEVICE_GD32F4XX_FLASH_H__
#define __HAL_DRIVER_GIGADEVICE_GD32F4XX_FLASH_H__

/*============================ INCLUDES ======================================*/

#include "hal/vsf_hal_cfg.h"

#if VSF_HAL_USE_FLASH == ENABLED

#include "../../__device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================ MACROS ========================================*/

#define VSF_FLASH_CFG_REIMPLEMENT_TYPE_IRQ_MASK     ENABLED

/*============================ TYPES =========================================*/

/*
 * GD32F4XX FMC IRQ mask encoding.
 *
 * F4XX FMC_CTL: ENDIE(24), ERRIE(25)
 * F4XX FMC_STAT: END(0), OPERR(1), WPERR(4), PGMERR(6), PGSERR(7), RDDERR(8)
 *
 * We encode IRQ masks at non-conflicting bit positions and remap in the driver.
 */
typedef enum vsf_flash_irq_mask_t {
    // 24: ENDIE(24) in FMC_CTL
    VSF_FLASH_IRQ_ERASE_MASK            = (1 << 24),
    // 23: use bit 23 (unused) to avoid conflict with ERASE
    VSF_FLASH_IRQ_WRITE_MASK            = (1 << 23),
    // 22
    VSF_FLASH_IRQ_READ_MASK             = (1 << 22),

    // error masks mapped from FMC_STAT
    // WPERR(4) in FMC_STAT
    VSF_FLASH_IRQ_ERASE_ERROR_MASK      = (1 << 4),
    // WPERR(4) | PGMERR(6) | PGSERR(7) in FMC_STAT
    VSF_FLASH_IRQ_WRITE_ERROR_MASK      = (1 << 4) | (1 << 6) | (1 << 7),
    // RDDERR(8) in FMC_STAT
    VSF_FLASH_IRQ_READ_ERROR_MASK       = (1 << 8),

    __VSF_HW_FLASH_ERR_IRQ_MASK         = VSF_FLASH_IRQ_ERASE_ERROR_MASK
                                        | VSF_FLASH_IRQ_WRITE_ERROR_MASK
                                        | VSF_FLASH_IRQ_READ_ERROR_MASK,
    __VSF_HW_FLASH_IRQ_MASK             = VSF_FLASH_IRQ_ERASE_MASK
                                        | VSF_FLASH_IRQ_WRITE_MASK
                                        | __VSF_HW_FLASH_ERR_IRQ_MASK,
} vsf_flash_irq_mask_t;

/*============================ PROTOTYPES ====================================*/

#ifdef __cplusplus
}
#endif

#endif      // VSF_HAL_USE_FLASH
#endif      // __HAL_DRIVER_GIGADEVICE_GD32F4XX_FLASH_H__
/* EOF */
