// SPDX-License-Identifier: MIT
/*
 * Host stand-in for the VSF HAL headers: just enough for the port's .c files
 * to compile unmodified. Types and error values mirror the VSF templates under
 * vsf/source/hal/driver/common/template and utilities/compiler/__common/__type.h.
 */
#ifndef HOST_VSF_HAL_H
#define HOST_VSF_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal/vsf_hal_cfg.h"

typedef enum {
    VSF_ERR_NOT_READY           = 1,
    VSF_ERR_NONE                = 0,
    VSF_ERR_NOT_SUPPORT         = -2,
    VSF_ERR_NOT_AVAILABLE       = -4,
    VSF_ERR_FAIL                = -7,
    VSF_ERR_INVALID_PARAMETER   = -8,
    VSF_ERR_INVALID_RANGE       = -9,
    VSF_ERR_INVALID_PTR         = -10,
    VSF_ERR_TIMEOUT             = -19,
    VSF_ERR_BUSY                = -20,
} vsf_err_t;
typedef enum { fsm_rt_err = -1, fsm_rt_cpl = 0 } fsm_rt_t;
typedef int IRQn_Type;

#define __HC1(a)            a
#define __HC2(a, b)         a##b
#define __HC3(a, b, c)      a##b##c
#define __HC4(a, b, c, d)   a##b##c##d
#define _HC1(a)             __HC1(a)
#define _HC2(a, b)          __HC2(a, b)
#define _HC3(a, b, c)       __HC3(a, b, c)
#define _HC4(a, b, c, d)    __HC4(a, b, c, d)
#define __HSEL(_1, _2, _3, _4, N, ...) N
#define VSF_MCONNECT(...)   __HSEL(__VA_ARGS__, _HC4, _HC3, _HC2, _HC1, 0)(__VA_ARGS__)
#define VSF_CAL_ROOT

/* The real assert spins for ever: count it and carry on, so a test can see it. */
extern unsigned host_asserts;
#define VSF_HAL_ASSERT(c)   do { if (!(c)) { host_asserts++; } } while (0)

/* One thread here, so the plain sequence is the same as the LDREX/STREX loop. */
#define vsf_atom32_op(__ptr, ...)                                               \
    ({                                                                          \
        volatile uint32_t *__p = (volatile uint32_t *)(__ptr);                  \
        int32_t _ = (int32_t)*__p;                                              \
        *__p = (uint32_t)(__VA_ARGS__);                                         \
        _;                                                                      \
    })

#define __ISB()             ((void)0)
#define __DSB()             ((void)0)
extern uint8_t host_nvic_on[256];
static inline void NVIC_SetPriority(IRQn_Type n, uint32_t p) { (void)n; (void)p; }
static inline void NVIC_EnableIRQ(IRQn_Type n) { host_nvic_on[n] = 1; }
static inline void NVIC_DisableIRQ(IRQn_Type n) { host_nvic_on[n] = 0; }
static inline uintptr_t vsf_hal_irq_enter(void) { return 0; }
static inline void vsf_hal_irq_leave(uintptr_t c) { (void)c; }

#include "common/common.h"

/* ---- flash ---- */
#if VSF_HAL_USE_FLASH == ENABLED
#define VSF_FLASH_CFG_MULTI_CLASS   DISABLED
typedef uint32_t vsf_flash_size_t;      /* uint_fast32_t on arm-none-eabi */
typedef struct vsf_flash_t vsf_flash_t;
#include "common/flash/flash.h"
#define VSF_FLASH_IRQ_ALL_BITS_MASK (__VSF_HW_FLASH_IRQ_MASK | VSF_FLASH_IRQ_READ_MASK)
typedef void vsf_flash_isr_handler_t(void *target, vsf_flash_t *f, vsf_flash_irq_mask_t m);
typedef struct {
    vsf_flash_isr_handler_t *handler_fn;
    void *target_ptr;
    int prio;
} vsf_flash_isr_t;
typedef struct { vsf_flash_isr_t isr; } vsf_flash_cfg_t;
typedef struct { uint32_t is_busy; } vsf_flash_status_t;
typedef struct {
    uint32_t irq_mask, base_address, max_size, erase_sector_size, write_sector_size;
    uint8_t none_sector_aligned_write, none_sector_aligned_read;
} vsf_flash_capability_t;
#define VSF_HW_FLASH0_ADDR          0x08000000
#define VSF_HW_FLASH0_REG_BASE      0
#define VSF_HW_FLASH0_IRQN          4
#define VSF_HW_FLASH0_IRQHandler    FMC_IRQHandler
#endif

/* ---- i2c ---- */
#if VSF_HAL_USE_I2C == ENABLED
#define VSF_I2C_CFG_MULTI_CLASS     DISABLED
typedef struct vsf_i2c_t vsf_i2c_t;
#include "common/i2c/i2c.h"
typedef void vsf_i2c_isr_handler_t(void *target, vsf_i2c_t *p, vsf_i2c_irq_mask_t m);
typedef struct {
    vsf_i2c_isr_handler_t *handler_fn;
    void *target_ptr;
    int prio;
} vsf_i2c_isr_t;
typedef struct {
    vsf_i2c_mode_t mode;
    uint32_t clock_hz;
    vsf_i2c_isr_t isr;
} vsf_i2c_cfg_t;
typedef struct { uint32_t is_busy; } vsf_i2c_status_t;
typedef struct {
    uint32_t irq_mask;
    uint8_t support_restart;
    uint32_t max_transfer_size, min_transfer_size;
} vsf_i2c_capability_t;
#define VSF_HW_I2C0_REG_BASE        0
#define VSF_HW_I2C0_PCLK_HZ         42000000U
#define VSF_HW_I2C0_EN              VSF_HW_EN_I2C0
#define VSF_HW_I2C0_RST             VSF_HW_RST_I2C0
#define VSF_HW_I2C0_EV_IRQN         31
#define VSF_HW_I2C0_ER_IRQN         32
#define VSF_HW_I2C0_EV_IRQHandler   I2C0_EV_IRQHandler
#define VSF_HW_I2C0_ER_IRQHandler   I2C0_ER_IRQHandler
#endif

/* ---- gpio ---- */
#if VSF_HAL_USE_GPIO == ENABLED
#define VSF_GPIO_CFG_MULTI_CLASS    DISABLED
typedef struct vsf_gpio_t vsf_gpio_t;
typedef uint16_t vsf_gpio_pin_mask_t;
#include "common/gpio/gpio.h"
typedef struct {
    uint32_t mode;
    uint16_t alternate_function;
} vsf_gpio_cfg_t;
typedef struct { uint32_t pin_mask; } vsf_gpio_exti_irq_cfg_t;
typedef struct {
    uint8_t is_async, support_output_and_set, support_output_and_clear, support_interrupt;
    uint8_t pin_count;
    uint32_t pin_mask;
} vsf_gpio_capability_t;
#define VSF_HW_GPIO_PIN_COUNT       16
#define VSF_HW_GPIO_PORT0_REG_BASE  0
#define VSF_HW_GPIO_PORT0_EN        VSF_HW_EN_GPIOA
#define VSF_HW_GPIO_PORT0_RST       VSF_HW_RST_GPIOA
#endif

/* ---- spi ---- */
#if VSF_HAL_USE_SPI == ENABLED
#define VSF_SPI_CFG_MULTI_CLASS     DISABLED
typedef struct vsf_spi_t vsf_spi_t;
#include "common/spi/spi.h"
typedef void vsf_spi_isr_handler_t(void *target, vsf_spi_t *s, vsf_spi_irq_mask_t m);
typedef struct {
    vsf_spi_isr_handler_t *handler_fn;
    void *target_ptr;
    int prio;
} vsf_spi_isr_t;
typedef struct {
    uint32_t mode;
    uint32_t clock_hz;
    vsf_spi_isr_t isr;
} vsf_spi_cfg_t;
typedef struct { uint32_t is_busy; } vsf_spi_status_t;
typedef struct {
    uint32_t irq_mask;
    uint8_t support_hardware_cs : 1;
    uint8_t support_software_cs : 1;
    uint8_t cs_count : 6;
    uint32_t max_clock_hz, min_clock_hz;
} vsf_spi_capability_t;
#define VSF_HW_SPI0_REG_BASE        0
#define VSF_HW_SPI0_PCLK_HZ         42000000U
#define VSF_HW_SPI0_EN              VSF_HW_EN_SPI0
#define VSF_HW_SPI0_RST             VSF_HW_RST_SPI0
#define VSF_HW_SPI0_IRQN            35
#define VSF_HW_SPI0_IRQHandler      SPI0_IRQHandler
#endif

/* ---- usart ---- */
#if VSF_HAL_USE_USART == ENABLED
#define VSF_USART_CFG_MULTI_CLASS   DISABLED
typedef struct vsf_usart_t vsf_usart_t;
#include "common/usart/usart.h"
typedef void vsf_usart_isr_handler_t(void *target, vsf_usart_t *u, vsf_usart_irq_mask_t m);
typedef struct {
    vsf_usart_isr_handler_t *handler_fn;
    void *target_ptr;
    int prio;
} vsf_usart_isr_t;
typedef struct {
    uint32_t mode;
    uint32_t baudrate;
    uint32_t rx_timeout;
    vsf_usart_isr_t isr;
} vsf_usart_cfg_t;
typedef struct {
    uint32_t irq_mask, max_baudrate, min_baudrate;
    uint8_t min_data_bits, max_data_bits, txfifo_depth, rxfifo_depth;
    uint8_t support_rx_timeout, support_send_break;
} vsf_usart_capability_t;
#define VSF_HW_USART0_REG_BASE      0
#define VSF_HW_USART0_PCLK_HZ       84000000U
#define VSF_HW_USART0_EN            VSF_HW_EN_USART0
#define VSF_HW_USART0_RST           VSF_HW_RST_USART0
#define VSF_HW_USART0_SYNC          true
#define VSF_HW_USART0_IRQN          37
#define VSF_HW_USART0_IRQHandler    USART0_IRQHandler
#endif

#endif
