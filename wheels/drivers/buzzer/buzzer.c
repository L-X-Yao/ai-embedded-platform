// SPDX-License-Identifier: MIT
/*
 * buzzer --- passive buzzer driver via PWM
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      pwm_hal (injected)
 * Instances: multi
 */
#include "buzzer.h"
#include "contract.h"

/*
 * MIDI note frequency table.
 *
 * Standard tuning: A4 (note 69) = 440 Hz.
 * Formula: f = 440 * 2^((n - 69) / 12)
 *
 * Pre-computed integer lookup for notes 0-127 to avoid float math
 * on embedded targets.  Values rounded to nearest Hz.
 */
static const uint16_t midi_freq_table[128] = {
    /*   0 */     8,     9,     9,    10,    10,    11,    12,    12,
    /*   8 */    13,    14,    15,    15,    16,    17,    18,    19,
    /*  16 */    21,    22,    23,    24,    26,    28,    29,    31,
    /*  24 */    33,    35,    37,    39,    41,    44,    46,    49,
    /*  32 */    52,    55,    58,    62,    65,    69,    73,    78,
    /*  40 */    82,    87,    92,    98,   104,   110,   117,   123,
    /*  48 */   131,   139,   147,   156,   165,   175,   185,   196,
    /*  56 */   208,   220,   233,   247,   262,   277,   294,   311,
    /*  64 */   330,   349,   370,   392,   415,   440,   466,   494,
    /*  72 */   523,   554,   587,   622,   659,   698,   740,   784,
    /*  80 */   831,   880,   932,   988,  1047,  1109,  1175,  1245,
    /*  88 */  1319,  1397,  1480,  1568,  1661,  1760,  1865,  1976,
    /*  96 */  2093,  2217,  2349,  2489,  2637,  2794,  2960,  3136,
    /* 104 */  3322,  3520,  3729,  3951,  4186,  4435,  4699,  4978,
    /* 112 */  5274,  5588,  5920,  6272,  6645,  7040,  7459,  7902,
    /* 120 */  8372,  8870,  9397,  9956, 10548, 11175, 11840, 12544,
};

/* Default beep frequency (Hz) when using buzzer_beep(). */
#define BUZZER_BEEP_FREQ_HZ  1000u

/* Default duty cycle for loudest tone (percent). */
#define BUZZER_DEFAULT_DUTY  50u

buzzer_err_t buzzer_init(buzzer_t *dev, const buzzer_pwm_t *pwm)
{
    if (!dev || !pwm)
        return BUZZER_ERR_PARAM;
    if (!pwm->set_freq || !pwm->set_duty || !pwm->delay_ms)
        return BUZZER_ERR_PARAM;

    dev->pwm  = pwm;
    dev->duty = BUZZER_DEFAULT_DUTY;

    /* Ensure buzzer starts silent. */
    pwm->set_freq(pwm->ctx, 0);
    pwm->set_duty(pwm->ctx, 0);

    INVARIANT(dev->pwm != NULL);
    return BUZZER_OK;
}

buzzer_err_t buzzer_tone(buzzer_t *dev, uint16_t freq_hz,
                         uint16_t duration_ms)
{
    if (!dev)
        return BUZZER_ERR_PARAM;

    if (freq_hz == 0) {
        /* Silence for duration (rest). */
        dev->pwm->set_freq(dev->pwm->ctx, 0);
        dev->pwm->set_duty(dev->pwm->ctx, 0);
    } else {
        dev->pwm->set_freq(dev->pwm->ctx, (uint32_t)freq_hz);
        dev->pwm->set_duty(dev->pwm->ctx, dev->duty);
    }

    if (duration_ms > 0) {
        dev->pwm->delay_ms(dev->pwm->ctx, duration_ms);
    }

    /* Silence after tone. */
    dev->pwm->set_freq(dev->pwm->ctx, 0);
    dev->pwm->set_duty(dev->pwm->ctx, 0);

    return BUZZER_OK;
}

buzzer_err_t buzzer_off(buzzer_t *dev)
{
    if (!dev)
        return BUZZER_ERR_PARAM;

    dev->pwm->set_freq(dev->pwm->ctx, 0);
    dev->pwm->set_duty(dev->pwm->ctx, 0);

    return BUZZER_OK;
}

buzzer_err_t buzzer_play(buzzer_t *dev, const buzzer_note_t *melody,
                         uint16_t count)
{
    if (!dev)
        return BUZZER_ERR_PARAM;
    if (!melody || count == 0)
        return BUZZER_ERR_PARAM;

    for (uint16_t i = 0; i < count; i++) {
        if (melody[i].freq_hz == 0) {
            /* Rest note: silence. */
            dev->pwm->set_freq(dev->pwm->ctx, 0);
            dev->pwm->set_duty(dev->pwm->ctx, 0);
        } else {
            dev->pwm->set_freq(dev->pwm->ctx,
                               (uint32_t)melody[i].freq_hz);
            dev->pwm->set_duty(dev->pwm->ctx, dev->duty);
        }

        if (melody[i].duration_ms > 0) {
            dev->pwm->delay_ms(dev->pwm->ctx, melody[i].duration_ms);
        }
    }

    /* Silence after melody. */
    dev->pwm->set_freq(dev->pwm->ctx, 0);
    dev->pwm->set_duty(dev->pwm->ctx, 0);

    return BUZZER_OK;
}

buzzer_err_t buzzer_beep(buzzer_t *dev, uint16_t count,
                         uint16_t on_ms, uint16_t off_ms)
{
    if (!dev)
        return BUZZER_ERR_PARAM;
    if (count == 0)
        return BUZZER_ERR_PARAM;

    for (uint16_t i = 0; i < count; i++) {
        dev->pwm->set_freq(dev->pwm->ctx, BUZZER_BEEP_FREQ_HZ);
        dev->pwm->set_duty(dev->pwm->ctx, dev->duty);

        if (on_ms > 0) {
            dev->pwm->delay_ms(dev->pwm->ctx, on_ms);
        }

        dev->pwm->set_freq(dev->pwm->ctx, 0);
        dev->pwm->set_duty(dev->pwm->ctx, 0);

        /* Gap between beeps, but not after the last one. */
        if (off_ms > 0 && i < (uint16_t)(count - 1)) {
            dev->pwm->delay_ms(dev->pwm->ctx, off_ms);
        }
    }

    return BUZZER_OK;
}

uint16_t buzzer_note_freq(uint8_t midi_note)
{
    if (midi_note > 127)
        return 0;
    return midi_freq_table[midi_note];
}
