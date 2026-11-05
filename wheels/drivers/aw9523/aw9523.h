// SPDX-License-Identifier: MIT
/*
 * aw9523 — AW9523B I2C GPIO expander and LED driver (16 pins)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Awinic AW9523B: two 8-bit ports (P0/P1), each pin configurable as
 * GPIO (push-pull output or input) or LED driver (256-step constant
 * current sink, max ~37mA). Interrupt-on-change with per-pin mask.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef AW9523_H
#define AW9523_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    AW9523_OK        =  0,
    AW9523_ERR_I2C   = -1,
    AW9523_ERR_ID    = -2,
    AW9523_ERR_PARAM = -3,
} aw9523_err_t;

/* ---------- port / pin-mode enums ---------- */

typedef enum {
    AW9523_PORT0 = 0,   /* P0.0 - P0.7 */
    AW9523_PORT1 = 1,   /* P1.0 - P1.7 */
} aw9523_port_t;

typedef enum {
    AW9523_DIR_OUTPUT = 0,
    AW9523_DIR_INPUT  = 1,
} aw9523_dir_t;

typedef enum {
    AW9523_PIN_LOW  = 0,
    AW9523_PIN_HIGH = 1,
} aw9523_level_t;

typedef enum {
    AW9523_MODE_LED  = 0,   /* constant-current LED driver */
    AW9523_MODE_GPIO = 1,   /* GPIO push-pull */
} aw9523_led_mode_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} aw9523_i2c_t;

/* ---------- constants ---------- */

#define AW9523_DEV_ADDR       0x58u  /* AD1=AD0=GND; 1 0 1 1 0 AD1 AD0 */
#define AW9523_CHIP_ID        0x23u  /* expected ID register value */

/* ---------- register addresses ---------- */

#define AW9523_REG_INPUT_P0    0x00u
#define AW9523_REG_INPUT_P1    0x01u
#define AW9523_REG_OUTPUT_P0   0x02u
#define AW9523_REG_OUTPUT_P1   0x03u
#define AW9523_REG_CONFIG_P0   0x04u  /* direction: 0=output, 1=input */
#define AW9523_REG_CONFIG_P1   0x05u
#define AW9523_REG_INT_P0      0x06u  /* interrupt enable: 0=enabled */
#define AW9523_REG_INT_P1      0x07u
#define AW9523_REG_ID          0x10u
#define AW9523_REG_CTL         0x11u  /* global control */
#define AW9523_REG_LED_MODE_P0 0x12u  /* 0=LED, 1=GPIO */
#define AW9523_REG_LED_MODE_P1 0x13u
/* DIM0..DIM15: individual LED current (0x20-0x2F) */
#define AW9523_REG_DIM_BASE    0x20u

/* ---------- device context ---------- */

typedef struct {
    const aw9523_i2c_t *i2c;
    uint8_t dev_addr;
} aw9523_t;

/* ---------- API ---------- */

/** Initialize device: read ID register (expect 0x23), reset to defaults.
 *  @param dev_addr 7-bit I2C address (0x58 to 0x5B, set by AD1/AD0)
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_init(aw9523_t *dev, const aw9523_i2c_t *i2c,
                         uint8_t dev_addr);

/** Set I/O direction for a single pin (0-7) on a port.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_set_direction(aw9523_t *dev, aw9523_port_t port,
                                  uint8_t pin, aw9523_dir_t dir);

/** Write a single pin (0-7) output level via read-modify-write on output reg.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_write_pin(aw9523_t *dev, aw9523_port_t port,
                              uint8_t pin, aw9523_level_t level);

/** Read a single pin (0-7) input level.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_read_pin(aw9523_t *dev, aw9523_port_t port,
                             uint8_t pin, aw9523_level_t *val);

/** Write all 8 pins of a port at once.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_write_port(aw9523_t *dev, aw9523_port_t port,
                               uint8_t val);

/** Read all 8 pins of a port at once.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_read_port(aw9523_t *dev, aw9523_port_t port,
                              uint8_t *val);

/** Set LED/GPIO mode for a single pin. LED mode = constant-current sink.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_set_led_mode(aw9523_t *dev, aw9523_port_t port,
                                 uint8_t pin, aw9523_led_mode_t mode);

/** Set LED current for a single pin (0-255). Only effective in LED mode.
 *  Pin numbering: port0 pin0-7 = DIM4-DIM11, port1 pin0-3 = DIM0-DIM3,
 *  port1 pin4-7 = DIM12-DIM15 (per AW9523B datasheet).
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_set_led_current(aw9523_t *dev, aw9523_port_t port,
                                    uint8_t pin, uint8_t current);

/** Set interrupt enable mask for a port.
 *  Note: AW9523B uses inverted logic: 0=interrupt enabled, 1=disabled.
 *  This function accepts a mask where 1=enabled and handles the inversion.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_set_interrupt_enable(aw9523_t *dev, aw9523_port_t port,
                                         uint8_t mask);

/** Software reset: write reset sequence to CTL register.
 *  @thread_safe: no @isr_safe: no */
aw9523_err_t aw9523_reset(aw9523_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* AW9523_H */
