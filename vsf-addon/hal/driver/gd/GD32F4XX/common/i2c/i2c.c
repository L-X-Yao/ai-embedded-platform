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

#if VSF_HAL_USE_I2C == ENABLED

#include "hal/vsf_hal.h"

#include "../vendor/Include/gd32f4xx_i2c.h"

/*============================ MACROS ========================================*/

#ifndef VSF_HW_I2C_CFG_MULTI_CLASS
#   define VSF_HW_I2C_CFG_MULTI_CLASS           VSF_I2C_CFG_MULTI_CLASS
#endif

#define VSF_I2C_CFG_IMP_PREFIX                  vsf_hw
#define VSF_I2C_CFG_IMP_UPCASE_PREFIX           VSF_HW

/*============================ TYPES =========================================*/

typedef struct VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) {
#if VSF_HW_I2C_CFG_MULTI_CLASS == ENABLED
    vsf_i2c_t               vsf_i2c;
#endif
    uint32_t                reg;
    uint32_t                pclk_hz;
    vsf_hw_peripheral_en_t  en;
    vsf_hw_peripheral_rst_t rst;
    IRQn_Type               ev_irqn;
    IRQn_Type               er_irqn;
    vsf_i2c_isr_t           isr;

    uint16_t                addr;
    vsf_i2c_cmd_t           cmd;
    uint8_t                 *buffer;
    uint16_t                count;
    uint16_t                transferred;
} VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t);

/*============================ IMPLEMENTATION ================================*/

vsf_err_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_init)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr,
    vsf_i2c_cfg_t *cfg_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    VSF_HAL_ASSERT(NULL != cfg_ptr);

    uint32_t reg = i2c_ptr->reg;
    uint32_t pclk = i2c_ptr->pclk_hz;
    uint32_t pclk_mhz = pclk / 1000000U;

    vsf_hw_peripheral_enable(i2c_ptr->en);

    // disable I2C before configuration
    I2C_CTL0(reg) &= ~I2C_CTL0_I2CEN;

    // set peripheral clock frequency in CTL1
    I2C_CTL1(reg) = (I2C_CTL1(reg) & ~BITS(0, 6)) | (pclk_mhz & 0x3FU);

    // clock configuration
    uint32_t clkc;
    if (cfg_ptr->mode & VSF_I2C_SPEED_FAST_MODE) {
        // fast mode: Tlow/Thigh = 2 (duty=0), SCL = pclk / (3 * clkc)
        clkc = pclk / (3U * cfg_ptr->clock_hz);
        if (clkc < 1) clkc = 1;
        I2C_CKCFG(reg) = (uint32_t)(clkc | I2C_CKCFG_FAST);
    } else {
        // standard mode: SCL = pclk / (2 * clkc)
        clkc = pclk / (2U * cfg_ptr->clock_hz);
        if (clkc < 4) clkc = 4;
        I2C_CKCFG(reg) = (uint32_t)clkc;
    }

    // rise time: standard = pclk_mhz + 1, fast = pclk_mhz * 300/1000 + 1
    if (cfg_ptr->mode & VSF_I2C_SPEED_FAST_MODE) {
        I2C_RT(reg) = (pclk_mhz * 300U / 1000U) + 1U;
    } else {
        I2C_RT(reg) = pclk_mhz + 1U;
    }

    vsf_i2c_isr_t *isr_ptr = &cfg_ptr->isr;
    i2c_ptr->isr = *isr_ptr;
    if (isr_ptr->handler_fn != NULL) {
        NVIC_SetPriority(i2c_ptr->ev_irqn, (uint32_t)isr_ptr->prio);
        NVIC_EnableIRQ(i2c_ptr->ev_irqn);
        NVIC_SetPriority(i2c_ptr->er_irqn, (uint32_t)isr_ptr->prio);
        NVIC_EnableIRQ(i2c_ptr->er_irqn);
    } else {
        NVIC_DisableIRQ(i2c_ptr->ev_irqn);
        NVIC_DisableIRQ(i2c_ptr->er_irqn);
    }

    I2C_CTL0(reg) |= I2C_CTL0_I2CEN;
    return VSF_ERR_NONE;
}

void VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_fini)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    vsf_hw_peripheral_rst_set(i2c_ptr->rst);
    vsf_hw_peripheral_rst_clear(i2c_ptr->rst);
}

fsm_rt_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_enable)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    I2C_CTL0(i2c_ptr->reg) |= I2C_CTL0_I2CEN;
    return fsm_rt_cpl;
}

fsm_rt_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_disable)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    I2C_CTL0(i2c_ptr->reg) &= ~I2C_CTL0_I2CEN;
    return fsm_rt_cpl;
}

void VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_irq_enable)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr,
    vsf_i2c_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    I2C_CTL1(i2c_ptr->reg) |= I2C_CTL1_EVIE | I2C_CTL1_BUFIE | I2C_CTL1_ERRIE;
}

void VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_irq_disable)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr,
    vsf_i2c_irq_mask_t irq_mask
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    I2C_CTL1(i2c_ptr->reg) &= ~(I2C_CTL1_EVIE | I2C_CTL1_BUFIE | I2C_CTL1_ERRIE);
}

vsf_i2c_status_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_status)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    return (vsf_i2c_status_t) {
        .is_busy = !!(I2C_STAT1(i2c_ptr->reg) & I2C_STAT1_I2CBSY),
    };
}

vsf_err_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_master_request)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr,
    uint16_t address,
    vsf_i2c_cmd_t cmd,
    uint_fast16_t count,
    uint8_t *buffer_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);

    uint32_t reg = i2c_ptr->reg;

    i2c_ptr->addr = address;
    i2c_ptr->cmd = cmd;
    i2c_ptr->buffer = buffer_ptr;
    i2c_ptr->count = count;
    i2c_ptr->transferred = 0;

    if (count == 1 && (cmd & VSF_I2C_CMD_READ)) {
        I2C_CTL0(reg) &= ~I2C_CTL0_ACKEN;
    } else {
        I2C_CTL0(reg) |= I2C_CTL0_ACKEN;
    }

    I2C_CTL1(reg) |= I2C_CTL1_EVIE | I2C_CTL1_BUFIE | I2C_CTL1_ERRIE;

    if (cmd & (VSF_I2C_CMD_START | VSF_I2C_CMD_RESTART)) {
        I2C_CTL0(reg) |= I2C_CTL0_START;
    }

    return VSF_ERR_NONE;
}

fsm_rt_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_master_fifo_transfer)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr,
    uint16_t address,
    vsf_i2c_cmd_t cmd,
    uint_fast16_t count,
    uint8_t *buffer_ptr,
    vsf_i2c_cmd_t *cur_cmd_ptr,
    uint_fast16_t *offset_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    return fsm_rt_err;
}

uint_fast16_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_slave_fifo_transfer)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr,
    bool transmit_or_receive,
    uint_fast16_t count,
    uint8_t *buffer_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    return 0;
}

vsf_err_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_slave_request)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr,
    bool transmit_or_receive,
    uint_fast16_t count,
    uint8_t *buffer_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    return VSF_ERR_NOT_SUPPORT;
}

uint_fast16_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_slave_get_transferred_count)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    return 0;
}

uint_fast16_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_master_get_transferred_count)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);
    return i2c_ptr->transferred;
}

vsf_i2c_capability_t VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_capability)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    return (vsf_i2c_capability_t) {
        .irq_mask           = __VSF_HW_I2C_IRQ_MASK,
        .support_restart    = 1,
        .max_transfer_size  = 0xFFFF,
        .min_transfer_size  = 1,
    };
}

/*
 * GD32F4XX I2C event IRQ handler (EV5/EV6/EV7/EV8/EV8_2 state machine).
 *
 * Master transmitter: START → EV5(SBSEND) → addr+W → EV6(ADDSEND) → [EV8(TBE) → data]* → EV8_2(BTC) → STOP
 * Master receiver:    START → EV5(SBSEND) → addr+R → EV6(ADDSEND) → [EV7(RBNE) → data]* → STOP
 */
static void VSF_MCONNECT(__, VSF_I2C_CFG_IMP_PREFIX, _i2c_ev_irqhandler)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);

    uint32_t reg = i2c_ptr->reg;
    uint32_t stat0 = I2C_STAT0(reg);

    // EV5: start condition sent
    if (stat0 & I2C_STAT0_SBSEND) {
        uint8_t addr_byte;
        if (i2c_ptr->cmd & VSF_I2C_CMD_10_BITS) {
            addr_byte = 0xF0 | ((i2c_ptr->addr >> 8) & 0x06);
        } else {
            addr_byte = (uint8_t)(i2c_ptr->addr << 1);
        }
        if (i2c_ptr->cmd & VSF_I2C_CMD_READ) {
            addr_byte |= 1;
        }
        I2C_DATA(reg) = addr_byte;
        return;
    }

    // EV6: address sent, clear by reading STAT0 then STAT1
    if (stat0 & I2C_STAT0_ADDSEND) {
        if ((i2c_ptr->cmd & VSF_I2C_CMD_READ) && (i2c_ptr->count == 2)) {
            // I2C v1 2-byte RX: disable ACK before clearing ADDR
            I2C_CTL0(reg) &= ~I2C_CTL0_ACKEN;
        }
        (void)I2C_STAT1(reg);
        if ((i2c_ptr->cmd & VSF_I2C_CMD_READ) && (i2c_ptr->count == 1)) {
            I2C_CTL0(reg) &= ~I2C_CTL0_ACKEN;
            if (i2c_ptr->cmd & VSF_I2C_CMD_STOP) {
                I2C_CTL0(reg) |= I2C_CTL0_STOP;
            }
        }
        return;
    }

    if (i2c_ptr->cmd & VSF_I2C_CMD_READ) {
        // 2-byte RX: wait for BTC (both bytes received), then read both
        if (i2c_ptr->count == 2 && (stat0 & I2C_STAT0_BTC)) {
            if (i2c_ptr->cmd & VSF_I2C_CMD_STOP) {
                I2C_CTL0(reg) |= I2C_CTL0_STOP;
            }
            if (i2c_ptr->buffer) {
                i2c_ptr->buffer[0] = (uint8_t)I2C_DATA(reg);
                i2c_ptr->buffer[1] = (uint8_t)I2C_DATA(reg);
            }
            i2c_ptr->transferred = 2;
            I2C_CTL1(reg) &= ~(I2C_CTL1_EVIE | I2C_CTL1_BUFIE);
            vsf_i2c_isr_t *isr_ptr = &i2c_ptr->isr;
            if (isr_ptr->handler_fn != NULL) {
                isr_ptr->handler_fn(isr_ptr->target_ptr,
                    (vsf_i2c_t *)i2c_ptr,
                    VSF_I2C_IRQ_MASK_MASTER_TRANSFER_COMPLETE);
            }
            return;
        }
        // EV7: RBNE — receive data (N=1 or N>=3)
        if (stat0 & I2C_STAT0_RBNE) {
            if (i2c_ptr->buffer && i2c_ptr->transferred < i2c_ptr->count) {
                i2c_ptr->buffer[i2c_ptr->transferred] = (uint8_t)I2C_DATA(reg);
                i2c_ptr->transferred++;
            }
            // Disable ACK before receiving last byte (N>=3 path)
            if (i2c_ptr->count >= 3 && i2c_ptr->transferred == i2c_ptr->count - 1) {
                I2C_CTL0(reg) &= ~I2C_CTL0_ACKEN;
                if (i2c_ptr->cmd & VSF_I2C_CMD_STOP) {
                    I2C_CTL0(reg) |= I2C_CTL0_STOP;
                }
            }
            if (i2c_ptr->transferred >= i2c_ptr->count) {
                I2C_CTL1(reg) &= ~(I2C_CTL1_EVIE | I2C_CTL1_BUFIE);
                vsf_i2c_isr_t *isr_ptr = &i2c_ptr->isr;
                if (isr_ptr->handler_fn != NULL) {
                    isr_ptr->handler_fn(isr_ptr->target_ptr,
                        (vsf_i2c_t *)i2c_ptr,
                        VSF_I2C_IRQ_MASK_MASTER_TRANSFER_COMPLETE);
                }
            }
        }
    } else {
        // EV8: TBE — load next byte
        if (stat0 & I2C_STAT0_TBE) {
            if (i2c_ptr->transferred < i2c_ptr->count) {
                I2C_DATA(reg) = i2c_ptr->buffer[i2c_ptr->transferred];
                i2c_ptr->transferred++;
            }
        }
        // EV8_2: BTC — last byte fully clocked out, safe to STOP
        if ((stat0 & I2C_STAT0_BTC) && (i2c_ptr->transferred >= i2c_ptr->count)) {
            if (i2c_ptr->cmd & VSF_I2C_CMD_STOP) {
                I2C_CTL0(reg) |= I2C_CTL0_STOP;
            }
            I2C_CTL1(reg) &= ~(I2C_CTL1_EVIE | I2C_CTL1_BUFIE);
            vsf_i2c_isr_t *isr_ptr = &i2c_ptr->isr;
            if (isr_ptr->handler_fn != NULL) {
                isr_ptr->handler_fn(isr_ptr->target_ptr,
                    (vsf_i2c_t *)i2c_ptr,
                    VSF_I2C_IRQ_MASK_MASTER_TRANSFER_COMPLETE);
            }
        }
    }
}

static void VSF_MCONNECT(__, VSF_I2C_CFG_IMP_PREFIX, _i2c_er_irqhandler)(
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t) *i2c_ptr
) {
    VSF_HAL_ASSERT(NULL != i2c_ptr);

    uint32_t reg = i2c_ptr->reg;
    uint32_t stat0 = I2C_STAT0(reg);
    vsf_i2c_irq_mask_t irq_mask = 0;

    if (stat0 & I2C_STAT0_BERR) {
        I2C_STAT0(reg) &= ~I2C_STAT0_BERR;
    }
    if (stat0 & I2C_STAT0_LOSTARB) {
        I2C_STAT0(reg) &= ~I2C_STAT0_LOSTARB;
        irq_mask |= VSF_I2C_IRQ_MASK_MASTER_ARBITRATION_LOST;
    }
    if (stat0 & I2C_STAT0_AERR) {
        I2C_STAT0(reg) &= ~I2C_STAT0_AERR;
        irq_mask |= VSF_I2C_IRQ_MASK_MASTER_ADDRESS_NACK;
        I2C_CTL0(reg) |= I2C_CTL0_STOP;
    }
    if (stat0 & I2C_STAT0_OUERR) {
        I2C_STAT0(reg) &= ~I2C_STAT0_OUERR;
    }

    I2C_CTL1(reg) &= ~(I2C_CTL1_EVIE | I2C_CTL1_BUFIE);

    if (irq_mask != 0) {
        vsf_i2c_isr_t *isr_ptr = &i2c_ptr->isr;
        if (isr_ptr->handler_fn != NULL) {
            isr_ptr->handler_fn(isr_ptr->target_ptr, (vsf_i2c_t *)i2c_ptr, irq_mask);
        }
    }
}

/*============================ TEMPLATE INSTANTIATION ========================*/

#define VSF_I2C_CFG_REIMPLEMENT_API_CAPABILITY      ENABLED
#define VSF_I2C_CFG_IMP_LV0(__IDX, __HAL_OP)                                   \
    VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c_t)                               \
        VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c, __IDX) = {                  \
        .reg            = VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _REG_BASE),\
        .pclk_hz        = VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _PCLK_HZ),\
        .en             = VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _EN),\
        .rst            = VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _RST),\
        .ev_irqn        = VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _EV_IRQN),\
        .er_irqn        = VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _ER_IRQN),\
        __HAL_OP                                                                \
    };                                                                          \
    void VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _EV_IRQHandler)(void)\
    {                                                                           \
        uintptr_t ctx = vsf_hal_irq_enter();                                    \
        VSF_MCONNECT(__, VSF_I2C_CFG_IMP_PREFIX, _i2c_ev_irqhandler)(           \
            &VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c, __IDX)                  \
        );                                                                      \
        vsf_hal_irq_leave(ctx);                                                 \
    }                                                                           \
    void VSF_MCONNECT(VSF_I2C_CFG_IMP_UPCASE_PREFIX, _I2C, __IDX, _ER_IRQHandler)(void)\
    {                                                                           \
        uintptr_t ctx = vsf_hal_irq_enter();                                    \
        VSF_MCONNECT(__, VSF_I2C_CFG_IMP_PREFIX, _i2c_er_irqhandler)(           \
            &VSF_MCONNECT(VSF_I2C_CFG_IMP_PREFIX, _i2c, __IDX)                  \
        );                                                                      \
        vsf_hal_irq_leave(ctx);                                                 \
    }
#include "hal/driver/common/i2c/i2c_template.inc"

#endif      // VSF_HAL_USE_I2C
