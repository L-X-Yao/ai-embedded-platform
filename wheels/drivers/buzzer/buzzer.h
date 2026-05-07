// SPDX-License-Identifier: MIT
/*
 * buzzer --- passive buzzer driver via PWM
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      pwm_hal (injected)
 * Instances: multi
 *
 * Drives a passive buzzer through frequency-controlled PWM.
 * Supports single tones, beep patterns, and melody playback.
 * MIDI note-to-frequency conversion (note 0-127).
 * Caller provides PWM and delay callbacks.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the buzzer_t struct.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BUZZER_OK        =  0,
    BUZZER_ERR_PARAM = -1,
} buzzer_err_t;

/*
 * PWM HAL injection.  Caller must provide all three function pointers.
 *
 * set_freq:  Set PWM frequency in Hz.  0 = silence (output off).
 * set_duty:  Set duty cycle 0-100 (percent).
 * delay_ms:  Blocking delay for the given milliseconds.
 */
typedef struct {
    void (*set_freq)(void *ctx, uint32_t freq_hz);
    void (*set_duty)(void *ctx, uint8_t percent);
    void (*delay_ms)(void *ctx, uint16_t ms);
    void *ctx;
} buzzer_pwm_t;

/* A single note: frequency + duration.  freq_hz == 0 means rest/silence. */
typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} buzzer_note_t;

typedef struct {
    const buzzer_pwm_t *pwm;
    uint8_t             duty;   /* default duty cycle (percent) */
} buzzer_t;

/** Initialize buzzer with injected PWM HAL.  duty defaults to 50%. */
/** @thread_safe: no  @isr_safe: no */
buzzer_err_t buzzer_init(buzzer_t *dev, const buzzer_pwm_t *pwm);

/** Play a single tone for the given duration then silence.  Blocks. */
/** @thread_safe: no  @isr_safe: no */
buzzer_err_t buzzer_tone(buzzer_t *dev, uint16_t freq_hz,
                         uint16_t duration_ms);

/** Silence the buzzer immediately. */
/** @thread_safe: no  @isr_safe: no */
buzzer_err_t buzzer_off(buzzer_t *dev);

/** Play a melody (array of notes).  Blocks for full duration. */
/** @thread_safe: no  @isr_safe: no */
buzzer_err_t buzzer_play(buzzer_t *dev, const buzzer_note_t *melody,
                         uint16_t count);

/** Beep pattern: count on/off cycles at 1 kHz.  Blocks. */
/** @thread_safe: no  @isr_safe: no */
buzzer_err_t buzzer_beep(buzzer_t *dev, uint16_t count,
                         uint16_t on_ms, uint16_t off_ms);

/** Convert MIDI note number (0-127) to frequency in Hz.
 *  Returns 0 for notes outside 0-127. */
uint16_t buzzer_note_freq(uint8_t midi_note);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */
