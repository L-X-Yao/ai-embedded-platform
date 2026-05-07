// SPDX-License-Identifier: MIT
/*
 * tm1637 -- TM1637 4-digit 7-segment LED display driver (bit-bang)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 *
 * Custom 2-wire serial protocol (CLK + DIO), NOT standard I2C.
 * GPIO interface injected via callbacks. Supports brightness control,
 * raw segment display, number formatting, and colon control.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef TM1637_H
#define TM1637_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TM1637_OK        =  0,
    TM1637_ERR_PARAM = -1,
    TM1637_ERR_ACK   = -2,
} tm1637_err_t;

/* ---------- GPIO HAL (caller provides) ---------- */

typedef struct {
    void (*clk_low)(void *ctx);
    void (*clk_high)(void *ctx);
    void (*dio_low)(void *ctx);
    void (*dio_high)(void *ctx);
    bool (*dio_read)(void *ctx);
    void (*delay_us)(void *ctx, uint16_t us);
    void *ctx;
} tm1637_gpio_t;

/* ---------- device context ---------- */

#define TM1637_DIGITS  4

typedef struct {
    const tm1637_gpio_t *gpio;       /* caller-owned, must outlive dev */
    uint8_t segments[TM1637_DIGITS]; /* shadow buffer */
    uint8_t brightness;              /* display control byte (0x80-0x8F) */
} tm1637_t;

/* ---------- commands ---------- */

#define TM1637_CMD_DATA_AUTO   0x40  /* data command: auto-increment addr */
#define TM1637_CMD_DATA_FIXED  0x44  /* data command: fixed address */
#define TM1637_CMD_ADDR_BASE   0xC0  /* address command: grid 0 */
#define TM1637_CMD_DISP_OFF    0x80  /* display off */
#define TM1637_CMD_DISP_ON     0x88  /* display on + brightness 0 */

/* ---------- API ---------- */

/** Initialize device. Sets display on with brightness 2.
 *  @thread_safe: no @isr_safe: no */
tm1637_err_t tm1637_init(tm1637_t *dev, const tm1637_gpio_t *gpio);

/** Set brightness (0-7) and on/off state.
 *  @thread_safe: no @isr_safe: no */
tm1637_err_t tm1637_set_brightness(tm1637_t *dev, uint8_t level, bool on);

/** Write raw segment data to display. count must be 1..TM1637_DIGITS.
 *  @thread_safe: no @isr_safe: no */
tm1637_err_t tm1637_display_segments(tm1637_t *dev,
                                     const uint8_t *segments, uint8_t count);

/** Display a decimal number (-999..9999). Clamps if out of range.
 *  leading_zeros: pad with zeros instead of blanks.
 *  @thread_safe: no @isr_safe: no */
tm1637_err_t tm1637_display_number(tm1637_t *dev, int16_t number,
                                   bool leading_zeros);

/** Display a single hex digit (0-15) at position (0-3).
 *  @thread_safe: no @isr_safe: no */
tm1637_err_t tm1637_display_digit(tm1637_t *dev, uint8_t position,
                                  uint8_t digit);

/** Set or clear the colon (segment bit 7 on position 1).
 *  @thread_safe: no @isr_safe: no */
tm1637_err_t tm1637_set_colon(tm1637_t *dev, bool on);

/** Clear all segments. @thread_safe: no @isr_safe: no */
tm1637_err_t tm1637_clear(tm1637_t *dev);

/** Encode a digit value (0-15 for hex) to 7-segment pattern.
 *  Returns 0x00 for out-of-range input.
 *  @thread_safe: yes (pure) @isr_safe: yes */
uint8_t tm1637_encode_digit(uint8_t digit);

#ifdef __cplusplus
}
#endif

#endif /* TM1637_H */
