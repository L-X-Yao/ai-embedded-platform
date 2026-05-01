/*
 * pcf8574 — I2C 8-bit GPIO expander driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * NXP PCF8574 / PCF8574A: command-based I2C (no registers).
 * Read or write a single byte representing 8 quasi-bidirectional I/O pins.
 * Pin writes use read-modify-write on a cached output_state.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef PCF8574_H
#define PCF8574_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PCF8574_OK         =  0,
    PCF8574_ERR_I2C    = -1,
    PCF8574_ERR_PARAMS = -2,
} pcf8574_err_t;

/*
 * I2C transport — command-based (no register address).
 * write: send len bytes to dev_addr.
 * read:  receive len bytes from dev_addr.
 */
typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr,
                  const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr,
                 uint8_t *data, uint16_t len);
    void *ctx;
} pcf8574_i2c_t;

typedef struct {
    const pcf8574_i2c_t *i2c;
    uint8_t dev_addr;       /* 7-bit I2C address */
    uint8_t output_state;   /* cached pin state for read-modify-write */
} pcf8574_t;

/** Initialize device context. Writes 0xFF (all high / input) on success.
 *  @param dev_addr 7-bit I2C address (0x20-0x27 PCF8574, 0x38-0x3F PCF8574A)
 *  @thread_safe: no */
pcf8574_err_t pcf8574_init(pcf8574_t *dev, const pcf8574_i2c_t *i2c,
                           uint8_t dev_addr);

/** Write all 8 pins at once.  @thread_safe: no */
pcf8574_err_t pcf8574_write(pcf8574_t *dev, uint8_t value);

/** Read current pin levels (actual bus state).  @thread_safe: no */
pcf8574_err_t pcf8574_read(pcf8574_t *dev, uint8_t *value);

/** Write a single pin (0-7) using cached output_state.  @thread_safe: no */
pcf8574_err_t pcf8574_write_pin(pcf8574_t *dev, uint8_t pin, bool level);

/** Read a single pin (0-7) from actual bus state.  @thread_safe: no */
pcf8574_err_t pcf8574_read_pin(pcf8574_t *dev, uint8_t pin, bool *level);

/** Toggle a single pin (0-7) using cached output_state.  @thread_safe: no */
pcf8574_err_t pcf8574_toggle_pin(pcf8574_t *dev, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* PCF8574_H */
