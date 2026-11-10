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

#if VSF_HAL_USE_FLASH == ENABLED

#include "hal/vsf_hal.h"

#include "../vendor/Include/gd32f4xx_fmc.h"

/*============================ MACROS ========================================*/

#ifndef VSF_HW_FLASH_CFG_MULTI_CLASS
#   define VSF_HW_FLASH_CFG_MULTI_CLASS         VSF_FLASH_CFG_MULTI_CLASS
#endif

#define VSF_FLASH_CFG_IMP_PREFIX                vsf_hw
#define VSF_FLASH_CFG_IMP_UPCASE_PREFIX         VSF_HW

#define __VSF_HW_FLASH_SIZE                     (1024U * 1024U)

/*============================ TYPES =========================================*/

typedef struct VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) {
#if VSF_HW_FLASH_CFG_MULTI_CLASS == ENABLED
    vsf_flash_t             vsf_flash;
#endif
    uint32_t                addr;
    uint32_t                reg;
    vsf_flash_irq_mask_t    irq_mask;
    IRQn_Type               irqn;
    uint8_t                 erase0_write1;
    vsf_flash_isr_t         isr;

    vsf_flash_size_t        offset_orig;
    vsf_flash_size_t        size_orig;
    vsf_flash_size_t        offset_cur;
    // not zero while a request is running; the interrupt counts it down
    volatile vsf_flash_size_t size_cur;
    uint8_t                 *buffer;
} VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t);

/*============================ HELPERS =======================================*/

/*
 * GD32F405 (1MB) sector layout — bank 0 only:
 *   Sector 0-3:  16KB each  (0x00000 - 0x0FFFF)
 *   Sector 4:    64KB       (0x10000 - 0x1FFFF)
 *   Sector 5-11: 128KB each (0x20000 - 0xFFFFF)
 */
static int __flash_offset_to_sector(uint32_t offset)
{
    if (offset < 0x10000U)  return (int)(offset / 0x4000U);
    if (offset < 0x20000U)  return 4;
    return 5 + (int)((offset - 0x20000U) / 0x20000U);
}

static uint32_t __flash_sector_size(int sector)
{
    if (sector < 4) return 0x4000U;
    if (sector == 4) return 0x10000U;
    return 0x20000U;
}

// FMC_CTL must already be unlocked. The interrupt starts every later sector.
static void __flash_start_sector_erase(int sector)
{
    uint32_t ctl = FMC_CTL;
    ctl &= ~(FMC_CTL_SN | FMC_CTL_PG | FMC_CTL_MER0 | FMC_CTL_MER1);
    ctl |= FMC_CTL_SER | ((uint32_t)sector << 3);
    FMC_CTL = ctl;
    FMC_CTL = ctl | FMC_CTL_START;
}

/*============================ IMPLEMENTATION ================================*/

// Write and erase advance only in the interrupt. Without a handler the NVIC line
// is off, and without irq_enable ENDIE is: one word would land, the call would
// return OK, and the rest of the request would never happen.
static vsf_err_t __flash_check_irq_armed(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr
) {
    if (    (NULL == flash_ptr->isr.handler_fn)
        ||  ((FMC_CTL & (FMC_CTL_ENDIE | FMC_CTL_ERRIE)) != (FMC_CTL_ENDIE | FMC_CTL_ERRIE))) {
        return VSF_ERR_NOT_AVAILABLE;
    }
    return VSF_ERR_NONE;
}

vsf_err_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_init)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_cfg_t *cfg_ptr
) {
    VSF_HAL_ASSERT((flash_ptr != NULL) && (cfg_ptr != NULL));

    if (FMC_CTL & FMC_CTL_LK) {
        FMC_KEY = UNLOCK_KEY0;
        FMC_KEY = UNLOCK_KEY1;
    }

    // word-size programming (32-bit)
    FMC_CTL = (FMC_CTL & ~FMC_CTL_PSZ) | ((uint32_t)2U << 8);

    flash_ptr->isr = cfg_ptr->isr;
    if (flash_ptr->isr.handler_fn != NULL) {
        NVIC_SetPriority(flash_ptr->irqn, (uint32_t)flash_ptr->isr.prio);
        NVIC_EnableIRQ(flash_ptr->irqn);
    } else {
        NVIC_DisableIRQ(flash_ptr->irqn);
    }
    return VSF_ERR_NONE;
}

void VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_fini)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);
    FMC_CTL |= FMC_CTL_LK;
}

fsm_rt_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_enable)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);
    return fsm_rt_cpl;
}

fsm_rt_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_disable)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);
    return fsm_rt_cpl;
}

void VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_irq_enable)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);
    flash_ptr->irq_mask = irq_mask;
    FMC_CTL |= FMC_CTL_ENDIE | FMC_CTL_ERRIE;
}

void VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_irq_disable)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);
    flash_ptr->irq_mask &= ~irq_mask;
    if (!(flash_ptr->irq_mask & __VSF_HW_FLASH_IRQ_MASK)) {
        FMC_CTL &= ~(FMC_CTL_ENDIE | FMC_CTL_ERRIE);
    }
}

vsf_err_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_erase_one_sector)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_size_t offset
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);

    vsf_err_t err = __flash_check_irq_armed(flash_ptr);
    if (err != VSF_ERR_NONE) {
        return err;
    }

    flash_ptr->erase0_write1 = 0;
    __flash_start_sector_erase(__flash_offset_to_sector(offset));
    return VSF_ERR_NONE;
}

vsf_err_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_erase_multi_sector)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_size_t offset,
    vsf_flash_size_t size
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);

    vsf_err_t err = __flash_check_irq_armed(flash_ptr);
    if (err != VSF_ERR_NONE) {
        return err;
    }

    flash_ptr->offset_orig = flash_ptr->offset_cur = offset;
    flash_ptr->size_orig = flash_ptr->size_cur = size;
    flash_ptr->erase0_write1 = 0;
    __flash_start_sector_erase(__flash_offset_to_sector(offset));
    return VSF_ERR_NONE;
}

vsf_err_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_write_multi_sector)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_size_t offset,
    uint8_t *buffer,
    vsf_flash_size_t size
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);
    VSF_HAL_ASSERT(NULL != buffer);

    // Word programming only: the interrupt walks size_cur down to zero in steps
    // of 4, so any other size would run on past the end of the buffer.
    if ((0 == size) || (size & 3) || (offset & 3)) {
        return VSF_ERR_INVALID_PARAMETER;
    }
    if ((offset >= __VSF_HW_FLASH_SIZE) || (size > __VSF_HW_FLASH_SIZE - offset)) {
        return VSF_ERR_INVALID_RANGE;
    }
    vsf_err_t err = __flash_check_irq_armed(flash_ptr);
    if (err != VSF_ERR_NONE) {
        return err;
    }

    flash_ptr->erase0_write1 = 1;
    flash_ptr->offset_orig = flash_ptr->offset_cur = offset;
    flash_ptr->size_orig = flash_ptr->size_cur = size;
    flash_ptr->buffer = buffer;

    { uint32_t guard = 500000; while ((FMC_STAT & FMC_STAT_BUSY) && --guard) { } }
    FMC_CTL |= FMC_CTL_PG;
    __ISB();
    __DSB();
    *(volatile uint32_t *)(flash_ptr->addr + offset) = ((uint32_t *)buffer)[0];
    __ISB();
    __DSB();

    return VSF_ERR_NONE;
}

vsf_err_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_write_one_sector)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_size_t offset,
    uint8_t *buffer,
    vsf_flash_size_t size
) {
    return VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_write_multi_sector)(
        flash_ptr, offset, buffer, size);
}

vsf_err_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_read_multi_sector)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr,
    vsf_flash_size_t offset,
    uint8_t *buffer,
    vsf_flash_size_t size
) {
    VSF_HAL_ASSERT(flash_ptr != NULL);
    VSF_HAL_ASSERT(NULL != buffer);

    memcpy(buffer, (void *)(flash_ptr->addr + offset), size);

    vsf_flash_isr_t *isr_ptr = &flash_ptr->isr;
    if (    (flash_ptr->irq_mask & VSF_FLASH_IRQ_READ_MASK)
        &&  (isr_ptr->handler_fn != NULL)) {
        isr_ptr->handler_fn(isr_ptr->target_ptr, (vsf_flash_t *)flash_ptr,
                            VSF_FLASH_IRQ_READ_MASK);
    }
    return VSF_ERR_NONE;
}

vsf_flash_status_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_status)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr
) {
    return (vsf_flash_status_t) {
        // BUSY drops between words and sectors: size_cur covers the whole request
        .is_busy = !!(FMC_STAT & FMC_STAT_BUSY) || (flash_ptr->size_cur != 0),
    };
}

vsf_flash_capability_t VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_capability)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr
) {
    return (vsf_flash_capability_t) {
        .irq_mask                  = VSF_FLASH_IRQ_ALL_BITS_MASK,
        .base_address              = 0x08000000,
        .max_size                  = __VSF_HW_FLASH_SIZE,
        .erase_sector_size         = 0x20000, /* largest sector (128KB, sectors 5-11) */
        .write_sector_size         = 4,
        .none_sector_aligned_write = 0,
        .none_sector_aligned_read  = 1,
    };
}

static void VSF_MCONNECT(__, VSF_FLASH_CFG_IMP_PREFIX, _flash_irqhandler)(
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t) *flash_ptr
) {
    VSF_HAL_ASSERT(NULL != flash_ptr);

    uint32_t stat = FMC_STAT;
    vsf_flash_isr_t *isr_ptr = &flash_ptr->isr;
    vsf_flash_irq_mask_t irq_mask = 0;

    // clear status flags
    FMC_STAT = stat;

    if (stat & (FMC_STAT_WPERR | FMC_STAT_PGMERR | FMC_STAT_PGSERR | FMC_STAT_RDDERR | FMC_STAT_OPERR)) {
        FMC_CTL &= ~(FMC_CTL_PG | FMC_CTL_SER);
        flash_ptr->size_cur = 0;
        irq_mask = flash_ptr->erase0_write1
                 ? VSF_FLASH_IRQ_WRITE_ERROR_MASK
                 : VSF_FLASH_IRQ_ERASE_ERROR_MASK;
    } else if (stat & FMC_STAT_END) {
        if (flash_ptr->erase0_write1) {
            // write: advance by 4 bytes (word programming)
            FMC_CTL &= ~FMC_CTL_PG;
            if (flash_ptr->size_cur < 4) {
                return;     // stray END, no write in progress
            }
            flash_ptr->size_cur -= 4;
            flash_ptr->offset_cur += 4;
            flash_ptr->buffer += 4;
            if (flash_ptr->size_cur > 0) {
                FMC_CTL |= FMC_CTL_PG;
                __ISB();
                __DSB();
                *(volatile uint32_t *)(flash_ptr->addr + flash_ptr->offset_cur) =
                    ((uint32_t *)flash_ptr->buffer)[0];
                __ISB();
                __DSB();
                return;
            }
            irq_mask = VSF_FLASH_IRQ_WRITE_MASK;
        } else {
            // erase: advance to next sector
            FMC_CTL &= ~FMC_CTL_SER;
            if (flash_ptr->size_cur > 0) {
                int sector = __flash_offset_to_sector(flash_ptr->offset_cur);
                uint32_t sec_size = __flash_sector_size(sector);
                if (flash_ptr->size_cur >= sec_size) {
                    flash_ptr->size_cur -= sec_size;
                    flash_ptr->offset_cur += sec_size;
                } else {
                    flash_ptr->size_cur = 0;
                }
            }
            if (flash_ptr->size_cur > 0) {
                __flash_start_sector_erase(__flash_offset_to_sector(flash_ptr->offset_cur));
                return;
            }
            irq_mask = VSF_FLASH_IRQ_ERASE_MASK;
        }
    }

    if ((irq_mask != 0) && (isr_ptr->handler_fn != NULL)) {
        isr_ptr->handler_fn(isr_ptr->target_ptr, (vsf_flash_t *)flash_ptr, irq_mask);
    }
}

/*============================ TEMPLATE INSTANTIATION ========================*/

#define VSF_FLASH_CFG_REIMPLEMENT_API_CAPABILITY    ENABLED
#define VSF_FLASH_CFG_ERASE_ALL_TEMPLATE            ENABLED
#define VSF_FLASH_CFG_READ_ONE_SECTOR_TEMPLATE      ENABLED

#define VSF_FLASH_CFG_IMP_LV0(__IDX, __HAL_OP)                                 \
    VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash_t)                            \
        VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash ## __IDX) = {             \
        .addr               = VSF_MCONNECT(VSF_FLASH_CFG_IMP_UPCASE_PREFIX, _FLASH, __IDX, _ADDR),\
        .reg                = VSF_MCONNECT(VSF_FLASH_CFG_IMP_UPCASE_PREFIX, _FLASH, __IDX, _REG_BASE),\
        .irqn               = VSF_MCONNECT(VSF_FLASH_CFG_IMP_UPCASE_PREFIX, _FLASH, __IDX, _IRQN),\
        __HAL_OP                                                                \
    };                                                                          \
    VSF_CAL_ROOT void VSF_MCONNECT(VSF_FLASH_CFG_IMP_UPCASE_PREFIX, _FLASH, __IDX, _IRQHandler)(void)\
    {                                                                           \
        uintptr_t ctx = vsf_hal_irq_enter();                                    \
        VSF_MCONNECT(__, VSF_FLASH_CFG_IMP_PREFIX, _flash_irqhandler)(          \
            &VSF_MCONNECT(VSF_FLASH_CFG_IMP_PREFIX, _flash, __IDX)              \
        );                                                                      \
        vsf_hal_irq_leave(ctx);                                                 \
    }
#include "hal/driver/common/flash/flash_template.inc"

#endif      // VSF_HAL_USE_FLASH
