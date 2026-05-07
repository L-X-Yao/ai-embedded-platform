// SPDX-License-Identifier: MIT
/*
 * onewire -- 1-Wire bus protocol (Dallas/Maxim)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    ONEWIRE_OK           =  0,
    ONEWIRE_ERR_PARAM    = -1,
    ONEWIRE_ERR_PRESENCE = -2, /* no device responded to reset */
    ONEWIRE_ERR_CRC      = -3,
} onewire_err_t;

/* ---------- GPIO HAL injection ---------- */

typedef struct {
    void (*pin_low)(void *ctx);               /* drive DQ low (output low)       */
    void (*pin_release)(void *ctx);           /* release DQ (input / float)      */
    bool (*pin_read)(void *ctx);              /* read DQ state (true = high)     */
    void (*delay_us)(void *ctx, uint16_t us); /* blocking microsecond delay      */
    void *ctx;
} onewire_gpio_t;

/* ---------- instance ---------- */

typedef struct {
    const onewire_gpio_t *gpio; /* internal */
} onewire_t;

/* ---------- ROM commands ---------- */

#define ONEWIRE_CMD_SEARCH_ROM  0xF0
#define ONEWIRE_CMD_READ_ROM    0x33
#define ONEWIRE_CMD_MATCH_ROM   0x55
#define ONEWIRE_CMD_SKIP_ROM    0xCC

/* ---------- API ---------- */

/** Initialize bus instance. @thread_safe: no @isr_safe: no */
onewire_err_t onewire_init(onewire_t *bus, const onewire_gpio_t *gpio);

/** Reset bus and detect presence. @thread_safe: no @isr_safe: no */
onewire_err_t onewire_reset(onewire_t *bus, bool *presence);

/** Write one byte (LSB first). @thread_safe: no @isr_safe: no */
onewire_err_t onewire_write_byte(onewire_t *bus, uint8_t byte);

/** Read one byte (LSB first). @thread_safe: no @isr_safe: no */
onewire_err_t onewire_read_byte(onewire_t *bus, uint8_t *byte);

/** Write multiple bytes. @thread_safe: no @isr_safe: no */
onewire_err_t onewire_write(onewire_t *bus, const uint8_t *data, uint16_t len);

/** Read multiple bytes. @thread_safe: no @isr_safe: no */
onewire_err_t onewire_read(onewire_t *bus, uint8_t *buf, uint16_t len);

/** Issue MATCH_ROM + 8-byte ROM address. @thread_safe: no @isr_safe: no */
onewire_err_t onewire_select(onewire_t *bus, const uint8_t rom[8]);

/** Issue SKIP_ROM (address all devices). @thread_safe: no @isr_safe: no */
onewire_err_t onewire_skip(onewire_t *bus);

/** Issue READ_ROM (single device on bus). @thread_safe: no @isr_safe: no */
onewire_err_t onewire_read_rom(onewire_t *bus, uint8_t rom[8]);

/** Compute Dallas/Maxim CRC-8 (polynomial 0x8C reflected). Pure function. */
uint8_t onewire_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* ONEWIRE_H */
