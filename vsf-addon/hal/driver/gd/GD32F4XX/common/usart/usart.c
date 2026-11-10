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

#if VSF_HAL_USE_USART == ENABLED

#include "hal/vsf_hal.h"

#include "../vendor/Include/gd32f4xx_usart.h"

/*============================ MACROS ========================================*/

#ifndef VSF_HW_USART_CFG_MULTI_CLASS
#   define VSF_HW_USART_CFG_MULTI_CLASS         VSF_USART_CFG_MULTI_CLASS
#endif

#define VSF_USART_CFG_IMP_PREFIX                vsf_hw
#define VSF_USART_CFG_IMP_UPCASE_PREFIX         VSF_HW

/*============================ TYPES =========================================*/

typedef struct VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) {
#if VSF_HW_USART_CFG_MULTI_CLASS == ENABLED
    vsf_usart_t             vsf_usart;
#endif
    uint32_t                reg;
    uint32_t                pclk_hz;
    vsf_hw_peripheral_en_t  en;
    vsf_hw_peripheral_rst_t rst;
    bool                    support_sync;
    IRQn_Type               irqn;
    vsf_usart_isr_t         isr;
} VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t);

/*============================ IMPLEMENTATION ================================*/

// Elements are 16 bits wide for 9 data bits only. With parity on, WL=1 carries
// 8 data bits and bit 8 of USART_DATA is the parity bit.
static bool __usart_is_9bit(uint32_t reg)
{
    uint32_t ctl0 = USART_CTL0(reg);
    return (ctl0 & USART_CTL0_WL) && !(ctl0 & USART_CTL0_PCEN);
}

vsf_err_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_init)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    vsf_usart_cfg_t *cfg_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    VSF_HAL_ASSERT(NULL != cfg_ptr);
    VSF_HAL_ASSERT(!(cfg_ptr->mode & __VSF_HW_USART_NOT_SUPPORT_MASK));

    uint32_t reg = usart_ptr->reg;
    uint32_t pclk = usart_ptr->pclk_hz;
    uint32_t mode = cfg_ptr->mode;
    uint32_t over8 = (mode & VSF_USART_OVERSAMPLE_MASK) >> 3;
    mode &= ~VSF_USART_OVERSAMPLE_MASK;
    uint32_t baudval = cfg_ptr->baudrate;

    // PCEN makes the last bit of the frame the parity bit and a frame is 9 bits
    // at most, so 8 data bits need WL=1 and 9 data bits cannot have parity.
    if (mode & USART_CTL0_PCEN) {
        if (mode & VSF_USART_9_BIT_LENGTH) {
            return VSF_ERR_NOT_SUPPORT;
        }
        mode |= VSF_USART_9_BIT_LENGTH;
    }

    if (0 == baudval) {
        return VSF_ERR_INVALID_PARAMETER;
    }

    // baud rate: same formula as H7XX (F4XX BAUD register is identical)
    uint32_t udiv, intdiv, fradiv;
    if (over8) {
        udiv = ((2U * pclk) + baudval / 2U) / baudval;
        intdiv = udiv & 0x0000FFF0U;
        fradiv = (udiv >> 1U) & 0x00000007U;
    } else {
        udiv = (pclk + baudval / 2U) / baudval;
        intdiv = udiv & 0x0000FFF0U;
        fradiv = udiv & 0x0000000FU;
    }
    // USARTDIV is 12.4 bits and at least 1: anything else would be cut to a rate
    // nobody asked for
    if ((udiv < 0x10U) || (udiv > 0xFFFFU)) {
        return VSF_ERR_INVALID_PARAMETER;
    }

    vsf_hw_peripheral_enable(usart_ptr->en);

    uint32_t ctl, cfg;
    uint32_t enabled = USART_CTL0(reg) & USART_CTL0_UEN;
    if (enabled) {
        vsf_atom32_op(&USART_CTL0(reg), _ & ~USART_CTL0_UEN);
    }

    // USART_BAUD cannot be written with UEN set
    USART_BAUD(reg) = (USART_BAUD_FRADIV | USART_BAUD_INTDIV) & (intdiv | fradiv);

    // CTL0: REN, TEN, PM, PCEN, WL (direct bit positions in mode), OVSMOD
    ctl = USART_CTL0(reg);
    ctl &= ~(USART_CTL0_REN | USART_CTL0_TEN | USART_CTL0_PM
            | USART_CTL0_PCEN | USART_CTL0_WL | USART_CTL0_OVSMOD);
    ctl |= mode & __VSF_HW_USART_CTL0_MASK;
    USART_CTL0(reg) = ctl | over8;

    // CTL1: STB from mode[0:1] << 12, sync clock from mode[20:23] >> 12
    ctl = USART_CTL1(reg);
    ctl &= ~(USART_CTL1_CKEN | USART_CTL1_STB | USART_CTL1_CPL
            | USART_CTL1_CPH | USART_CTL1_CLEN);
    cfg = mode & __VSF_HW_USART_CTL1_MASK;
    ctl |= ((cfg & 0x03U) << 12) | ((cfg & 0x00F00000U) >> 12);
    USART_CTL1(reg) = ctl;

    // CTL2: RTSEN/CTSEN from mode[4:5] << 4, HDEN from mode[6] >> 3
    ctl = USART_CTL2(reg);
    ctl &= ~(USART_CTL2_RTSEN | USART_CTL2_CTSEN | USART_CTL2_HDEN);
    cfg = mode & __VSF_HW_USART_CTL2_MASK;
    ctl |= ((cfg & 0x30U) << 4) | ((cfg & 0x40U) >> 3);
    USART_CTL2(reg) = ctl;

    if (enabled) {
        vsf_atom32_op(&USART_CTL0(reg), _ | USART_CTL0_UEN);
    }

    vsf_usart_isr_t *isr_ptr = &cfg_ptr->isr;
    usart_ptr->isr = *isr_ptr;
    if (isr_ptr->handler_fn != NULL) {
        NVIC_SetPriority(usart_ptr->irqn, (uint32_t)isr_ptr->prio);
        NVIC_EnableIRQ(usart_ptr->irqn);
    } else {
        NVIC_DisableIRQ(usart_ptr->irqn);
    }
    return VSF_ERR_NONE;
}

void VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_fini)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    vsf_hw_peripheral_rst_set(usart_ptr->rst);
    vsf_hw_peripheral_rst_clear(usart_ptr->rst);
}

fsm_rt_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_enable)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    vsf_atom32_op(&USART_CTL0(usart_ptr->reg), _ | USART_CTL0_UEN);
    return fsm_rt_cpl;
}

fsm_rt_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_disable)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    vsf_atom32_op(&USART_CTL0(usart_ptr->reg), _ & ~USART_CTL0_UEN);
    return fsm_rt_cpl;
}

void VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_irq_enable)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    vsf_usart_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    VSF_HAL_ASSERT(!(irq_mask & ~__VSF_HW_USART_IRQ_MASK));

    vsf_atom32_op(&USART_CTL0(usart_ptr->reg), _ | (irq_mask & __VSF_HW_USART_CTL0_IRQ_MASK));
    vsf_atom32_op(&USART_CTL2(usart_ptr->reg), _ | (irq_mask & __VSF_HW_USART_CTL2_IRQ_MASK));
}

void VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_irq_disable)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    vsf_usart_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);

    vsf_atom32_op(&USART_CTL0(usart_ptr->reg), _ & ~(irq_mask & __VSF_HW_USART_CTL0_IRQ_MASK));
    vsf_atom32_op(&USART_CTL2(usart_ptr->reg), _ & ~(irq_mask & __VSF_HW_USART_CTL2_IRQ_MASK));
}

vsf_usart_status_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_status)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    uint32_t reg = usart_ptr->reg;
    return (vsf_usart_status_t) {
        .value = USART_STAT0(reg) | USART_STAT1(reg),
    };
}

uint_fast16_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_rxfifo_get_data_count)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return (USART_STAT0(usart_ptr->reg) & USART_STAT0_RBNE) ? 1 : 0;
}

uint_fast32_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_rxfifo_read)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    void *buffer_ptr,
    uint_fast16_t count
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    VSF_HAL_ASSERT(NULL != buffer_ptr);

    uint32_t reg = usart_ptr->reg;
    uint_fast32_t cnt = 0;
    bool is9bit = __usart_is_9bit(reg);
    while (cnt < count) {
        if (USART_STAT0(reg) & USART_STAT0_RBNE) {
            if (is9bit) {
                *(uint16_t *)buffer_ptr = USART_DATA(reg) & USART_DATA_DATA;
                buffer_ptr = (void *)((uintptr_t)buffer_ptr + 2);
            } else {
                *(uint8_t *)buffer_ptr = (uint8_t)USART_DATA(reg);
                buffer_ptr = (void *)((uintptr_t)buffer_ptr + 1);
            }
            cnt++;
        } else {
            break;
        }
    }
    return cnt;
}

uint_fast16_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_txfifo_get_free_count)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return (USART_STAT0(usart_ptr->reg) & USART_STAT0_TBE) ? 1 : 0;
}

uint_fast32_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_txfifo_write)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    void *buffer_ptr,
    uint_fast16_t count
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    VSF_HAL_ASSERT(NULL != buffer_ptr);

    uint32_t reg = usart_ptr->reg;
    uint_fast32_t cnt = 0;
    bool is9bit = __usart_is_9bit(reg);
    while (cnt < count) {
        if (USART_STAT0(reg) & USART_STAT0_TBE) {
            if (is9bit) {
                USART_DATA(reg) = *(uint16_t *)buffer_ptr & USART_DATA_DATA;
                buffer_ptr = (void *)((uintptr_t)buffer_ptr + 2);
            } else {
                USART_DATA(reg) = *(uint8_t *)buffer_ptr;
                buffer_ptr = (void *)((uintptr_t)buffer_ptr + 1);
            }
            cnt++;
        } else {
            break;
        }
    }
    return cnt;
}

vsf_err_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_ctrl)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    vsf_usart_ctrl_t cmd,
    void *param
) {
    VSF_HAL_ASSERT(!(cmd & ~__VSF_HW_USART_SUPPORT_CMD_MASK));
    if (cmd & VSF_USART_CTRL_SEND_BREAK) {
        vsf_atom32_op(&USART_CTL0(usart_ptr->reg), _ | USART_CTL0_SBKCMD);
    }
    return VSF_ERR_NONE;
}

vsf_usart_capability_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_capability)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    return (vsf_usart_capability_t) {
        .irq_mask                   = __VSF_HW_USART_IRQ_MASK,
        .max_baudrate               = 0,
        .min_baudrate               = 0,
        .min_data_bits              = 8,
        .max_data_bits              = 9,
        .txfifo_depth               = 1,
        .rxfifo_depth               = 1,
        .support_rx_timeout         = 0,
        .support_send_break         = 1,
    };
}

static void VSF_MCONNECT(__, VSF_USART_CFG_IMP_PREFIX, _usart_irqhandler)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);

    uint32_t reg = usart_ptr->reg;
    vsf_usart_isr_t *isr_ptr = &usart_ptr->isr;
    uint32_t stat = USART_STAT0(reg);
    uint32_t ctl0 = USART_CTL0(reg);
    vsf_usart_irq_mask_t irq_mask = 0;

    if ((stat & USART_STAT0_RBNE) && (ctl0 & USART_CTL0_RBNEIE)) {
        irq_mask |= VSF_USART_IRQ_MASK_RX;
    }
    if ((stat & USART_STAT0_TBE) && (ctl0 & USART_CTL0_TBEIE)) {
        irq_mask |= VSF_USART_IRQ_MASK_TX;
    }
    if ((stat & USART_STAT0_PERR) && (ctl0 & USART_CTL0_PERRIE)) {
        irq_mask |= VSF_USART_IRQ_MASK_PARITY_ERR;
    }
    if ((stat & USART_STAT0_CTSF) && (USART_CTL2(reg) & USART_CTL2_CTSIE)) {
        irq_mask |= VSF_USART_IRQ_MASK_CTS;
    }
    // ORERR: clear by reading STAT0 (done above) then reading DATA
    if (stat & USART_STAT0_ORERR) {
        volatile uint32_t __dummy = USART_DATA(reg);
        (void)__dummy;
    }
    // CTSF: only software clears it (rc_w0). A plain store, not &=, so that RBNE
    // or TC set after the read above is written as 1 and survives.
    if (irq_mask & VSF_USART_IRQ_MASK_CTS) {
        USART_STAT0(reg) = ~USART_STAT0_CTSF;
    }

    if ((irq_mask != 0) && (isr_ptr->handler_fn != NULL)) {
        isr_ptr->handler_fn(isr_ptr->target_ptr, (vsf_usart_t *)usart_ptr, irq_mask);
    }
}

// STUB: DMA not implemented — returns NOT_SUPPORT so callers know transfer did not start

vsf_err_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_request_rx)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    void *buffer_ptr,
    uint_fast32_t count
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return VSF_ERR_NOT_SUPPORT;
}

vsf_err_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_request_tx)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr,
    void *buffer_ptr,
    uint_fast32_t count
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return VSF_ERR_NOT_SUPPORT;
}

vsf_err_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_cancel_rx)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return VSF_ERR_NOT_SUPPORT;
}

vsf_err_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_cancel_tx)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return VSF_ERR_NOT_SUPPORT;
}

int_fast32_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_get_rx_count)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return 0;
}

int_fast32_t VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_get_tx_count)(
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t) *usart_ptr
) {
    VSF_HAL_ASSERT(NULL != usart_ptr);
    return 0;
}

/*============================ MACROFIED FUNCTIONS ===========================*/

#define VSF_USART_CFG_REIMPLEMENT_API_CAPABILITY        ENABLED
#define VSF_USART_CFG_REIMPLEMENT_API_REQUEST           ENABLED
#define VSF_USART_CFG_REIMPLEMENT_API_CTRL              ENABLED
#define VSF_USART_CFG_IMP_LV0(__IDX, __HAL_OP)                                 \
    VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart_t)                           \
        VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart, __IDX) = {              \
        .reg            = VSF_MCONNECT(VSF_USART_CFG_IMP_UPCASE_PREFIX, _USART, __IDX, _REG_BASE),\
        .pclk_hz        = VSF_MCONNECT(VSF_USART_CFG_IMP_UPCASE_PREFIX, _USART, __IDX, _PCLK_HZ),\
        .en             = VSF_MCONNECT(VSF_USART_CFG_IMP_UPCASE_PREFIX, _USART, __IDX, _EN),\
        .rst            = VSF_MCONNECT(VSF_USART_CFG_IMP_UPCASE_PREFIX, _USART, __IDX, _RST),\
        .support_sync   = VSF_MCONNECT(VSF_USART_CFG_IMP_UPCASE_PREFIX, _USART, __IDX, _SYNC),\
        .irqn           = VSF_MCONNECT(VSF_USART_CFG_IMP_UPCASE_PREFIX, _USART, __IDX, _IRQN),\
        __HAL_OP                                                                \
    };                                                                          \
    void VSF_MCONNECT(VSF_USART_CFG_IMP_UPCASE_PREFIX, _USART, __IDX, _IRQHandler)(void)\
    {                                                                           \
        uintptr_t ctx = vsf_hal_irq_enter();                                    \
        VSF_MCONNECT(__, VSF_USART_CFG_IMP_PREFIX, _usart_irqhandler)(          \
            &VSF_MCONNECT(VSF_USART_CFG_IMP_PREFIX, _usart, __IDX)              \
        );                                                                      \
        vsf_hal_irq_leave(ctx);                                                 \
    }
#include "hal/driver/common/usart/usart_template.inc"

#endif /* VSF_HAL_USE_USART */
