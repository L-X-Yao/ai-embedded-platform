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

#if VSF_HAL_USE_GPIO == ENABLED

#include "hal/vsf_hal.h"

#include "../vendor/Include/gd32f4xx_gpio.h"

/*============================ MACROS ========================================*/

#ifndef VSF_HW_GPIO_CFG_MULTI_CLASS
#   define VSF_HW_GPIO_CFG_MULTI_CLASS          VSF_GPIO_CFG_MULTI_CLASS
#endif

#define VSF_GPIO_CFG_IMP_PREFIX                 vsf_hw
#define VSF_GPIO_CFG_IMP_UPCASE_PREFIX          VSF_HW

/*============================ TYPES =========================================*/

typedef struct VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) {
#if VSF_HW_GPIO_CFG_MULTI_CLASS == ENABLED
    vsf_gpio_t                  vsf_gpio;
#endif
    uint32_t                    reg;
    vsf_hw_peripheral_en_t      en;
    vsf_hw_peripheral_rst_t     rst;
} VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t);

/*============================ IMPLEMENTATION ================================*/

uint32_t __vsf_hw_gpio_get_regbase(vsf_hw_gpio_t *gpio_ptr)
{
    return gpio_ptr->reg;
}

vsf_err_t VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_port_config_pins)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_pin_mask_t pin_mask,
    vsf_gpio_cfg_t *cfg_ptr
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);

    uint32_t reg = gpio_ptr->reg, offset_len2;
    uint32_t ctl = (cfg_ptr->mode >> 0) & 3;
    uint32_t omode = (cfg_ptr->mode >> 2) & 1;
    uint32_t pud = (cfg_ptr->mode >> 3) & 3;
    uint32_t ospd = (cfg_ptr->mode >> 5) & 3;
    uint32_t current_pin_mask;
    uint32_t function = cfg_ptr->alternate_function, offset_len4;

    // On a gated port these writes go nowhere. Done here and in set_direction, not
    // in vsf_driver_init: every VSF firmware runs that one, this only when it uses
    // this driver, and a port that startup code has clocked just stays on.
    vsf_hw_peripheral_enable(gpio_ptr->en);

    for (int pin = 0; pin < VSF_HW_GPIO_PIN_COUNT; pin++) {
        current_pin_mask = 1 << pin;
        if (pin_mask & current_pin_mask) {
            offset_len2 = pin << 1;

            if (VSF_GPIO_AF == ctl) {
                if (pin < 8) {
                    offset_len4 = pin << 2;
                    vsf_atom32_op(&GPIO_AFSEL0(reg), (_ & ~(15 << offset_len4)) | (function << offset_len4));
                } else {
                    offset_len4 = (pin - 8) << 2;
                    vsf_atom32_op(&GPIO_AFSEL1(reg), (_ & ~(15 << offset_len4)) | (function << offset_len4));
                }
            }
            vsf_atom32_op(&GPIO_CTL(reg), (_ & ~(3 << offset_len2)) | (ctl << offset_len2));
            vsf_atom32_op(&GPIO_OMODE(reg), (_ & ~(1 << pin)) | (omode << pin));
            vsf_atom32_op(&GPIO_OSPD(reg), (_ & ~(3 << offset_len2)) | (ospd << offset_len2));
            vsf_atom32_op(&GPIO_PUD(reg), (_ & ~(3 << offset_len2)) | (pud << offset_len2));
        }
    }
    return VSF_ERR_NONE;
}

void VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_set_direction)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_pin_mask_t pin_mask,
    vsf_gpio_pin_mask_t direction_mask
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);

    uint32_t reg = gpio_ptr->reg, offset_len2;
    uint32_t ctl, current_pin_mask;

    vsf_hw_peripheral_enable(gpio_ptr->en);

    for (int i = 0; i < VSF_HW_GPIO_PIN_COUNT; i++) {
        current_pin_mask = 1 << i;
        if (pin_mask & current_pin_mask) {
            ctl = (direction_mask >> i) & 1;
            offset_len2 = i << 1;
            vsf_atom32_op(&GPIO_CTL(reg), (_ & ~(3 << offset_len2)) | (ctl << offset_len2));
        }
    }
}

vsf_gpio_pin_mask_t VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_get_direction)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_pin_mask_t pin_mask
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);

    uint32_t ctl = GPIO_CTL(gpio_ptr->reg), current_pin_mask;
    vsf_gpio_pin_mask_t direction = 0;

    for (int i = 0; i < VSF_HW_GPIO_PIN_COUNT; i++, ctl >>= 2) {
        current_pin_mask = 1 << i;
        if (pin_mask & current_pin_mask) {
            if ((ctl & 3) == 1) {
                direction |= current_pin_mask;
            }
        }
    }
    return direction;
}

vsf_gpio_pin_mask_t VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_read)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);
    return GPIO_ISTAT(gpio_ptr->reg);
}

void VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_write)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_pin_mask_t pin_mask,
    vsf_gpio_pin_mask_t value
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);

    value &= pin_mask;
    vsf_atom32_op(&GPIO_OCTL(gpio_ptr->reg), (_ & ~pin_mask) | value);
}

void VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_toggle)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_pin_mask_t pin_mask
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);
    GPIO_TG(gpio_ptr->reg) = pin_mask;
}

vsf_err_t VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_exti_irq_config)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_exti_irq_cfg_t *irq_cfg_ptr
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);
    VSF_HAL_ASSERT(false);
    return VSF_ERR_NOT_SUPPORT;
}

vsf_err_t VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_exti_irq_enable)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_pin_mask_t pin_mask
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);
    VSF_HAL_ASSERT(false);
    return VSF_ERR_NOT_SUPPORT;
}

vsf_err_t VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_exti_irq_disable)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr,
    vsf_gpio_pin_mask_t pin_mask
) {
    VSF_HAL_ASSERT(NULL != gpio_ptr);
    VSF_HAL_ASSERT(false);
    return VSF_ERR_NOT_SUPPORT;
}

vsf_gpio_capability_t VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_capability)(
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t) *gpio_ptr
) {
    return (vsf_gpio_capability_t){
        .is_async                   = false,
        .support_output_and_set     = true,
        .support_output_and_clear   = true,
        .support_interrupt          = false,
        .pin_count                  = 16,
        .pin_mask                   = 0xFFFF,
    };
}

/*============================ INCLUDES ======================================*/

#define VSF_GPIO_CFG_CAPABILITY_SUPPORT_OUTPUT_AND_SET      1
#define VSF_GPIO_CFG_CAPABILITY_SUPPORT_OUTPUT_AND_CLEAR    1
#define VSF_GPIO_CFG_CHANGE_DIR_FIRST                       DISABLED

#define VSF_GPIO_CFG_REIMPLEMENT_API_CAPABILITY             ENABLED

#define VSF_GPIO_CFG_IMP_LV0(__IDX, __HAL_OP)                                   \
    VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio_t)                              \
        VSF_MCONNECT(VSF_GPIO_CFG_IMP_PREFIX, _gpio, __IDX) = {                 \
        __HAL_OP                                                                \
        .reg                        = VSF_HW_GPIO_PORT ## __IDX ## _REG_BASE,   \
        .en                         = VSF_HW_GPIO_PORT ## __IDX ## _EN,         \
        .rst                        = VSF_HW_GPIO_PORT ## __IDX ## _RST,        \
    };

#include "hal/driver/common/gpio/gpio_template.inc"

#endif      // VSF_HAL_USE_GPIO
