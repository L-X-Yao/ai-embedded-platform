// SPDX-License-Identifier: MIT
/*
 * prop_buzzer.c — property-based tests for buzzer
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_buzzer.c ../buzzer.c -o prop_buzzer -lm
 */

#include "test_harness.h"
#include "property_test.h"
#include "buzzer.h"
#include <string.h>

/* ---------- mock PWM ---------- */

static uint32_t mock_last_freq;
static uint8_t  mock_last_duty;
static uint32_t mock_total_delay;

static void mock_reset(void)
{
    mock_last_freq  = 0;
    mock_last_duty  = 0;
    mock_total_delay = 0;
}

static void mock_set_freq(void *ctx, uint32_t freq_hz)
{
    (void)ctx;
    mock_last_freq = freq_hz;
}

static void mock_set_duty(void *ctx, uint8_t percent)
{
    (void)ctx;
    mock_last_duty = percent;
}

static void mock_delay_ms(void *ctx, uint16_t ms)
{
    (void)ctx;
    mock_total_delay += ms;
}

static const buzzer_pwm_t prop_pwm = {
    .set_freq = mock_set_freq,
    .set_duty = mock_set_duty,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* ------------------------------------------------------------------ */

PROPERTY(tone_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    buzzer_t dev;
    buzzer_init(&dev, &prop_pwm);

    uint16_t freq = ARBI_UINT16(seed);
    uint16_t dur  = ARBI_UINT16(seed);
    buzzer_tone(&dev, freq, dur);
    /* Must not crash */
}

PROPERTY(tone_ends_silent, 5000)
{
    mock_reset();

    buzzer_t dev;
    buzzer_init(&dev, &prop_pwm);

    uint16_t freq = (uint16_t)(ARBI_UINT16(seed) % 10000 + 1);
    uint16_t dur  = (uint16_t)(ARBI_UINT16(seed) % 5000 + 1);

    buzzer_err_t err = buzzer_tone(&dev, freq, dur);
    PROP_ASSERT(err == BUZZER_OK);
    /* After tone completes, buzzer must be silent */
    PROP_ASSERT_EQ(mock_last_freq, 0);
    PROP_ASSERT_EQ(mock_last_duty, 0);
}

PROPERTY(beep_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    buzzer_t dev;
    buzzer_init(&dev, &prop_pwm);

    uint16_t count  = (uint16_t)(ARBI_UINT8(seed) % 10 + 1);
    uint16_t on_ms  = ARBI_UINT16(seed);
    uint16_t off_ms = ARBI_UINT16(seed);

    buzzer_beep(&dev, count, on_ms, off_ms);
    /* Must not crash */
}

PROPERTY(play_no_crash, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    buzzer_t dev;
    buzzer_init(&dev, &prop_pwm);

    buzzer_note_t melody[8];
    int n = (int)(ARBI_UINT8(seed) % 8) + 1;
    for (int i = 0; i < n; i++) {
        melody[i].freq_hz    = ARBI_UINT16(seed);
        melody[i].duration_ms = ARBI_UINT16(seed);
    }

    buzzer_play(&dev, melody, (uint16_t)n);
    /* Must not crash */
}

PROPERTY(midi_note_bounded, 10000)
{
    uint8_t note = ARBI_UINT8(seed) % 128;
    uint16_t freq = buzzer_note_freq(note);
    /* Valid MIDI notes produce frequencies > 0 */
    PROP_ASSERT(freq > 0);
    /* Highest MIDI note (127) is ~12544 Hz */
    PROP_ASSERT(freq <= 13000);
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();

    buzzer_t dev;
    buzzer_init(&dev, &prop_pwm);
    buzzer_init(&dev, &prop_pwm);
    /* Must not crash */
}

TEST_MAIN("buzzer properties")
    RUN(tone_no_crash);
    RUN(tone_ends_silent);
    RUN(beep_no_crash);
    RUN(play_no_crash);
    RUN(midi_note_bounded);
    RUN(double_init_safe);
TEST_MAIN_END()
