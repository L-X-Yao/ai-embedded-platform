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

#if VSF_HAL_USE_SPI == ENABLED

#include "hal/vsf_hal.h"

#include "../vendor/Include/gd32f4xx_spi.h"

/*============================ MACROS ========================================*/

#ifndef VSF_HW_SPI_CFG_MULTI_CLASS
#   define VSF_HW_SPI_CFG_MULTI_CLASS           VSF_SPI_CFG_MULTI_CLASS
#endif

#define VSF_SPI_CFG_IMP_PREFIX                  vsf_hw
#define VSF_SPI_CFG_IMP_UPCASE_PREFIX           VSF_HW

/*============================ TYPES =========================================*/

typedef struct VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) {
#if VSF_HW_SPI_CFG_MULTI_CLASS == ENABLED
    vsf_spi_t               vsf_spi;
#endif
    uint32_t                reg;
    uint32_t                pclk_hz;
    vsf_hw_peripheral_en_t  en;
    vsf_hw_peripheral_rst_t rst;
    IRQn_Type               irqn;
    vsf_spi_isr_t           isr;
} VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t);

/*============================ IMPLEMENTATION ================================*/

static uint8_t __spi_calc_prescaler(uint32_t pclk, uint32_t clock_hz)
{
    // PSC[2:0]: 0=/2, 1=/4, 2=/8, ..., 7=/256
    for (uint8_t psc = 0; psc < 8; psc++) {
        if (pclk / (2U << psc) <= clock_hz) {
            return psc;
        }
    }
    return 7;
}

vsf_err_t VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_init)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr,
    vsf_spi_cfg_t *cfg_ptr
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    VSF_HAL_ASSERT(NULL != cfg_ptr);
    VSF_HAL_ASSERT(!(cfg_ptr->mode & __VSF_HW_SPI_NOT_SUPPORT_MASK));

    uint32_t reg = spi_ptr->reg;

    vsf_hw_peripheral_enable(spi_ptr->en);

    // disable SPI before configuration
    SPI_CTL0(reg) &= ~SPI_CTL0_SPIEN;

    uint8_t psc = __spi_calc_prescaler(spi_ptr->pclk_hz, cfg_ptr->clock_hz);
    uint32_t ctl0 = cfg_ptr->mode & __VSF_HW_SPI_CTL0_MASK;
    ctl0 |= (uint32_t)psc << 3;
    if (cfg_ptr->mode & VSF_SPI_CS_SOFTWARE_MODE) {
        ctl0 |= SPI_CTL0_SWNSS;
    }
    SPI_CTL0(reg) = ctl0;

    vsf_spi_isr_t *isr_ptr = &cfg_ptr->isr;
    spi_ptr->isr = *isr_ptr;
    if (isr_ptr->handler_fn != NULL) {
        NVIC_SetPriority(spi_ptr->irqn, (uint32_t)isr_ptr->prio);
        NVIC_EnableIRQ(spi_ptr->irqn);
    } else {
        NVIC_DisableIRQ(spi_ptr->irqn);
    }
    return VSF_ERR_NONE;
}

void VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_fini)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    vsf_hw_peripheral_rst_set(spi_ptr->rst);
    vsf_hw_peripheral_rst_clear(spi_ptr->rst);
}

fsm_rt_t VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_enable)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    SPI_CTL0(spi_ptr->reg) |= SPI_CTL0_SPIEN;
    return fsm_rt_cpl;
}

fsm_rt_t VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_disable)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    SPI_CTL0(spi_ptr->reg) &= ~SPI_CTL0_SPIEN;
    return fsm_rt_cpl;
}

void VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_irq_enable)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr,
    vsf_spi_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    SPI_CTL1(spi_ptr->reg) |= irq_mask & __VSF_HW_SPI_IRQ_MASK;
}

void VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_irq_disable)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr,
    vsf_spi_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    SPI_CTL1(spi_ptr->reg) &= ~(irq_mask & __VSF_HW_SPI_IRQ_MASK);
}

vsf_spi_status_t VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_status)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    return (vsf_spi_status_t) {
        .is_busy = !!(SPI_STAT(spi_ptr->reg) & SPI_STAT_TRANS),
    };
}

void VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_fifo_transfer)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr,
    void *out_buffer_ptr, uint_fast32_t *out_offset_ptr,
    void *in_buffer_ptr,  uint_fast32_t *in_offset_ptr,
    uint_fast32_t count
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);

    uint32_t reg = spi_ptr->reg;
    uint32_t stat;
    uint32_t is16bit = SPI_CTL0(reg) & SPI_CTL0_FF16;
    uint_fast32_t tx_offset = out_offset_ptr ? *out_offset_ptr : 0;
    uint_fast32_t rx_offset = in_offset_ptr  ? *in_offset_ptr  : 0;

    while (tx_offset < count || rx_offset < count) {
        stat = SPI_STAT(reg);
        if ((tx_offset < count) && (stat & SPI_STAT_TBE)) {
            if (out_buffer_ptr != NULL) {
                if (is16bit) {
                    SPI_DATA(reg) = ((uint16_t *)out_buffer_ptr)[tx_offset];
                } else {
                    SPI_DATA(reg) = ((uint8_t *)out_buffer_ptr)[tx_offset];
                }
            } else {
                SPI_DATA(reg) = 0xFFFF;
            }
            tx_offset++;
        }
        if ((rx_offset < count) && (stat & SPI_STAT_RBNE)) {
            uint16_t data = SPI_DATA(reg);
            if (in_buffer_ptr != NULL) {
                if (is16bit) {
                    ((uint16_t *)in_buffer_ptr)[rx_offset] = data;
                } else {
                    ((uint8_t *)in_buffer_ptr)[rx_offset] = (uint8_t)data;
                }
            }
            rx_offset++;
        }
    }

    if (out_offset_ptr) *out_offset_ptr = tx_offset;
    if (in_offset_ptr)  *in_offset_ptr  = rx_offset;
}

vsf_err_t VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_request_transfer)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr,
    void *out_buffer_ptr,
    void *in_buffer_ptr,
    uint_fast32_t count
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    return VSF_ERR_NOT_SUPPORT;
}

vsf_err_t VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_cancel_transfer)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    return VSF_ERR_NOT_SUPPORT;
}

void VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_get_transferred_count)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr,
    uint_fast32_t *tx_count, uint_fast32_t *rx_count
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);
    if (tx_count) *tx_count = 0;
    if (rx_count) *rx_count = 0;
}

vsf_spi_capability_t VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_capability)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr
) {
    return (vsf_spi_capability_t) {
        .irq_mask                   = __VSF_HW_SPI_IRQ_MASK,
        .support_hardware_cs              = 0,
        .support_software_cs              = 1,
        .cs_count                   = 1,
        .max_clock_hz               = 0,
        .min_clock_hz               = 0,
    };
}

static void VSF_MCONNECT(__, VSF_SPI_CFG_IMP_PREFIX, _spi_irqhandler)(
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t) *spi_ptr
) {
    VSF_HAL_ASSERT(NULL != spi_ptr);

    uint32_t reg = spi_ptr->reg;
    uint32_t stat = SPI_STAT(reg);
    uint32_t ctl1 = SPI_CTL1(reg);
    vsf_spi_isr_t *isr_ptr = &spi_ptr->isr;
    vsf_spi_irq_mask_t irq_mask = 0;

    if ((stat & SPI_STAT_RBNE) && (ctl1 & SPI_CTL1_RBNEIE)) {
        irq_mask |= VSF_SPI_IRQ_MASK_RX;
    }
    if ((stat & SPI_STAT_TBE) && (ctl1 & SPI_CTL1_TBEIE)) {
        irq_mask |= VSF_SPI_IRQ_MASK_TX;
    }
    if ((stat & SPI_STAT_RXORERR) && (ctl1 & SPI_CTL1_ERRIE)) {
        irq_mask |= VSF_SPI_IRQ_MASK_RX_OVERFLOW_ERR;
        volatile uint32_t __dummy = SPI_DATA(reg);
        (void)__dummy;
        (void)SPI_STAT(reg);
    }

    if ((irq_mask != 0) && (isr_ptr->handler_fn != NULL)) {
        isr_ptr->handler_fn(isr_ptr->target_ptr, (vsf_spi_t *)spi_ptr, irq_mask);
    }
}

/*============================ TEMPLATE INSTANTIATION ========================*/

#define VSF_SPI_CFG_REIMPLEMENT_API_CAPABILITY      ENABLED
#define VSF_SPI_CFG_IMP_LV0(__IDX, __HAL_OP)                                   \
    VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi_t)                               \
        VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi, __IDX) = {                  \
        .reg            = VSF_MCONNECT(VSF_SPI_CFG_IMP_UPCASE_PREFIX, _SPI, __IDX, _REG_BASE),\
        .pclk_hz        = VSF_MCONNECT(VSF_SPI_CFG_IMP_UPCASE_PREFIX, _SPI, __IDX, _PCLK_HZ),\
        .en             = VSF_MCONNECT(VSF_SPI_CFG_IMP_UPCASE_PREFIX, _SPI, __IDX, _EN),\
        .rst            = VSF_MCONNECT(VSF_SPI_CFG_IMP_UPCASE_PREFIX, _SPI, __IDX, _RST),\
        .irqn           = VSF_MCONNECT(VSF_SPI_CFG_IMP_UPCASE_PREFIX, _SPI, __IDX, _IRQN),\
        __HAL_OP                                                                \
    };                                                                          \
    void VSF_MCONNECT(VSF_SPI_CFG_IMP_UPCASE_PREFIX, _SPI, __IDX, _IRQHandler)(void)\
    {                                                                           \
        uintptr_t ctx = vsf_hal_irq_enter();                                    \
        VSF_MCONNECT(__, VSF_SPI_CFG_IMP_PREFIX, _spi_irqhandler)(              \
            &VSF_MCONNECT(VSF_SPI_CFG_IMP_PREFIX, _spi, __IDX)                  \
        );                                                                      \
        vsf_hal_irq_leave(ctx);                                                 \
    }
#include "hal/driver/common/spi/spi_template.inc"

#endif      // VSF_HAL_USE_SPI
