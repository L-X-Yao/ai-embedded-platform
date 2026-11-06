// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "buzzer.h"
#include <string.h>

/* ---- Mock PWM ---------------------------------------------------------- */

#define MOCK_LOG_MAX 128

typedef struct {
    uint32_t freq;
    uint8_t  duty;
} mock_event_t;

typedef struct {
    /* Event log: every set_freq/set_duty call appended. */
    enum { EVT_FREQ, EVT_DUTY, EVT_DELAY } types[MOCK_LOG_MAX];
    uint32_t       values[MOCK_LOG_MAX];
    uint16_t       count;
    /* Current state. */
    uint32_t       cur_freq;
    uint8_t        cur_duty;
    uint32_t       total_delay_ms;
} mock_pwm_state_t;

static mock_pwm_state_t mock;

static void mock_reset(void)
{
    memset(&mock, 0, sizeof(mock));
}

static void mock_set_freq(void *ctx, uint32_t freq_hz)
{
    (void)ctx;
    mock.cur_freq = freq_hz;
    if (mock.count < MOCK_LOG_MAX) {
        mock.types[mock.count]  = EVT_FREQ;
        mock.values[mock.count] = freq_hz;
        mock.count++;
    }
}

static void mock_set_duty(void *ctx, uint8_t percent)
{
    (void)ctx;
    mock.cur_duty = percent;
    if (mock.count < MOCK_LOG_MAX) {
        mock.types[mock.count]  = EVT_DUTY;
        mock.values[mock.count] = percent;
        mock.count++;
    }
}

static void mock_delay_ms(void *ctx, uint16_t ms)
{
    (void)ctx;
    mock.total_delay_ms += ms;
    if (mock.count < MOCK_LOG_MAX) {
        mock.types[mock.count]  = EVT_DELAY;
        mock.values[mock.count] = ms;
        mock.count++;
    }
}

static const buzzer_pwm_t mock_pwm = {
    .set_freq = mock_set_freq,
    .set_duty = mock_set_duty,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

/* ---- Helpers ----------------------------------------------------------- */

/* Count freq-set events with a specific value. */
static uint16_t count_freq_events(uint32_t freq)
{
    uint16_t n = 0;
    for (uint16_t i = 0; i < mock.count; i++) {
        if (mock.types[i] == EVT_FREQ && mock.values[i] == freq)
            n++;
    }
    return n;
}

/* ---- Init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    mock_reset();
    buzzer_t dev;
    ASSERT_EQ(buzzer_init(&dev, &mock_pwm), BUZZER_OK);
    /* Init should silence the buzzer. */
    ASSERT_EQ(mock.cur_freq, 0);
    ASSERT_EQ(mock.cur_duty, 0);
}

TEST(test_init_null_dev)
{
    ASSERT_EQ(buzzer_init(NULL, &mock_pwm), BUZZER_ERR_PARAM);
}

TEST(test_init_null_pwm)
{
    buzzer_t dev;
    ASSERT_EQ(buzzer_init(&dev, NULL), BUZZER_ERR_PARAM);
}

TEST(test_init_missing_set_freq)
{
    buzzer_pwm_t bad = { .set_freq = NULL, .set_duty = mock_set_duty,
                         .delay_ms = mock_delay_ms, .ctx = NULL };
    buzzer_t dev;
    ASSERT_EQ(buzzer_init(&dev, &bad), BUZZER_ERR_PARAM);
}

TEST(test_init_missing_set_duty)
{
    buzzer_pwm_t bad = { .set_freq = mock_set_freq, .set_duty = NULL,
                         .delay_ms = mock_delay_ms, .ctx = NULL };
    buzzer_t dev;
    ASSERT_EQ(buzzer_init(&dev, &bad), BUZZER_ERR_PARAM);
}

TEST(test_init_missing_delay)
{
    buzzer_pwm_t bad = { .set_freq = mock_set_freq, .set_duty = mock_set_duty,
                         .delay_ms = NULL, .ctx = NULL };
    buzzer_t dev;
    ASSERT_EQ(buzzer_init(&dev, &bad), BUZZER_ERR_PARAM);
}

/* ---- Tone tests -------------------------------------------------------- */

TEST(test_tone_440hz_100ms)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();  /* Clear init events. */

    ASSERT_EQ(buzzer_tone(&dev, 440, 100), BUZZER_OK);

    /* Should have: set_freq(440), set_duty(50), delay(100),
       set_freq(0), set_duty(0) */
    ASSERT_TRUE(mock.count >= 5);
    /* First event: freq 440. */
    ASSERT_EQ(mock.types[0], EVT_FREQ);
    ASSERT_EQ(mock.values[0], 440);
    /* Second event: duty 50. */
    ASSERT_EQ(mock.types[1], EVT_DUTY);
    ASSERT_EQ(mock.values[1], 50);
    /* Third: delay 100ms. */
    ASSERT_EQ(mock.types[2], EVT_DELAY);
    ASSERT_EQ(mock.values[2], 100);
    /* Ends silent. */
    ASSERT_EQ(mock.cur_freq, 0);
    ASSERT_EQ(mock.cur_duty, 0);
    ASSERT_EQ(mock.total_delay_ms, 100);
}

TEST(test_tone_rest)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    /* freq_hz=0 means rest (silence for duration). */
    ASSERT_EQ(buzzer_tone(&dev, 0, 200), BUZZER_OK);

    /* No non-zero freq should appear. */
    ASSERT_EQ(count_freq_events(0), 2);  /* silence + final silence */
    ASSERT_EQ(mock.total_delay_ms, 200);
    ASSERT_EQ(mock.cur_freq, 0);
}

TEST(test_tone_zero_duration)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    ASSERT_EQ(buzzer_tone(&dev, 1000, 0), BUZZER_OK);
    /* No delay call expected. */
    ASSERT_EQ(mock.total_delay_ms, 0);
    /* Should still end silent. */
    ASSERT_EQ(mock.cur_freq, 0);
    ASSERT_EQ(mock.cur_duty, 0);
}

TEST(test_tone_null_dev)
{
    ASSERT_EQ(buzzer_tone(NULL, 440, 100), BUZZER_ERR_PARAM);
}

/* ---- Off tests --------------------------------------------------------- */

TEST(test_off)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    /* Manually set freq to simulate an active tone. */
    mock.cur_freq = 1000;
    mock.cur_duty = 50;

    ASSERT_EQ(buzzer_off(&dev), BUZZER_OK);
    ASSERT_EQ(mock.cur_freq, 0);
    ASSERT_EQ(mock.cur_duty, 0);
}

TEST(test_off_null_dev)
{
    ASSERT_EQ(buzzer_off(NULL), BUZZER_ERR_PARAM);
}

/* ---- Play melody tests ------------------------------------------------- */

TEST(test_play_simple_melody)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    buzzer_note_t melody[] = {
        { .freq_hz = 262, .duration_ms = 100 },  /* C4 */
        { .freq_hz = 330, .duration_ms = 100 },  /* E4 */
        { .freq_hz = 392, .duration_ms = 100 },  /* G4 */
    };

    ASSERT_EQ(buzzer_play(&dev, melody, 3), BUZZER_OK);
    ASSERT_EQ(mock.total_delay_ms, 300);
    /* Ends silent. */
    ASSERT_EQ(mock.cur_freq, 0);
    ASSERT_EQ(mock.cur_duty, 0);
}

TEST(test_play_with_rest)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    buzzer_note_t melody[] = {
        { .freq_hz = 440, .duration_ms = 100 },
        { .freq_hz = 0,   .duration_ms = 50  },  /* rest */
        { .freq_hz = 880, .duration_ms = 100 },
    };

    ASSERT_EQ(buzzer_play(&dev, melody, 3), BUZZER_OK);
    ASSERT_EQ(mock.total_delay_ms, 250);
}

TEST(test_play_single_note)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    buzzer_note_t note = { .freq_hz = 1000, .duration_ms = 50 };
    ASSERT_EQ(buzzer_play(&dev, &note, 1), BUZZER_OK);
    ASSERT_EQ(mock.total_delay_ms, 50);
    ASSERT_EQ(mock.cur_freq, 0);
}

TEST(test_play_null_dev)
{
    buzzer_note_t note = { .freq_hz = 440, .duration_ms = 100 };
    ASSERT_EQ(buzzer_play(NULL, &note, 1), BUZZER_ERR_PARAM);
}

TEST(test_play_null_melody)
{
    buzzer_t dev;
    mock_reset();
    buzzer_init(&dev, &mock_pwm);
    ASSERT_EQ(buzzer_play(&dev, NULL, 3), BUZZER_ERR_PARAM);
}

TEST(test_play_zero_count)
{
    buzzer_t dev;
    mock_reset();
    buzzer_init(&dev, &mock_pwm);
    buzzer_note_t note = { .freq_hz = 440, .duration_ms = 100 };
    ASSERT_EQ(buzzer_play(&dev, &note, 0), BUZZER_ERR_PARAM);
}

/* ---- Beep tests -------------------------------------------------------- */

TEST(test_beep_single)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    ASSERT_EQ(buzzer_beep(&dev, 1, 100, 50), BUZZER_OK);

    /* 1 beep: on(100ms), no trailing off gap. */
    ASSERT_EQ(mock.total_delay_ms, 100);
    ASSERT_EQ(mock.cur_freq, 0);
    ASSERT_EQ(mock.cur_duty, 0);
}

TEST(test_beep_triple)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    ASSERT_EQ(buzzer_beep(&dev, 3, 100, 50), BUZZER_OK);

    /* 3 beeps: on(100)+off(50) + on(100)+off(50) + on(100)
       = 300 on + 100 off = 400ms total. */
    ASSERT_EQ(mock.total_delay_ms, 400);
    /* Beep uses 1000 Hz. */
    ASSERT_TRUE(count_freq_events(1000) >= 3);
}

TEST(test_beep_null_dev)
{
    ASSERT_EQ(buzzer_beep(NULL, 1, 100, 50), BUZZER_ERR_PARAM);
}

TEST(test_beep_zero_count)
{
    buzzer_t dev;
    mock_reset();
    buzzer_init(&dev, &mock_pwm);
    ASSERT_EQ(buzzer_beep(&dev, 0, 100, 50), BUZZER_ERR_PARAM);
}

TEST(test_beep_zero_on_ms)
{
    mock_reset();
    buzzer_t dev;
    buzzer_init(&dev, &mock_pwm);
    mock_reset();

    /* Zero on_ms: still valid, just very short beeps. */
    ASSERT_EQ(buzzer_beep(&dev, 2, 0, 50), BUZZER_OK);
    /* Only off delays: 1 gap between 2 beeps = 50ms. */
    ASSERT_EQ(mock.total_delay_ms, 50);
}

/* ---- MIDI note freq tests ---------------------------------------------- */

TEST(test_midi_a4_440)
{
    ASSERT_EQ(buzzer_note_freq(69), 440);
}

TEST(test_midi_c4_262)
{
    /* Middle C = MIDI 60. */
    ASSERT_EQ(buzzer_note_freq(60), 262);
}

TEST(test_midi_c5_523)
{
    ASSERT_EQ(buzzer_note_freq(72), 523);
}

TEST(test_midi_note_0)
{
    /* Lowest MIDI note: ~8 Hz. */
    ASSERT_EQ(buzzer_note_freq(0), 8);
}

TEST(test_midi_note_127)
{
    /* Highest MIDI note: ~12544 Hz. */
    ASSERT_EQ(buzzer_note_freq(127), 12544);
}

/* ---- Main -------------------------------------------------------------- */

/* ---------- a device init refused ---------- */

static void assert_device_refused(buzzer_t *d)
{
    static const buzzer_note_t note = { 440, 10 };
    ASSERT_EQ(buzzer_tone(d, 440, 10), BUZZER_ERR_PARAM);
    ASSERT_EQ(buzzer_off(d), BUZZER_ERR_PARAM);
    ASSERT_EQ(buzzer_play(d, &note, 1), BUZZER_ERR_PARAM);
    ASSERT_EQ(buzzer_beep(d, 1, 10, 10), BUZZER_ERR_PARAM);
}

/* dev holds whatever the stack did, as nothing else wrote it */
TEST(test_device_refused_after_rejected_init)
{
    static const buzzer_pwm_t no_callbacks;
    buzzer_t d;
    mock_reset();

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(buzzer_init(&d, NULL), BUZZER_ERR_PARAM);
    assert_device_refused(&d);

    memset(&d, 0xA5, sizeof(d));
    ASSERT_EQ(buzzer_init(&d, &no_callbacks), BUZZER_ERR_PARAM);
    assert_device_refused(&d);
}

TEST_MAIN("buzzer")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_pwm);
    RUN(test_init_missing_set_freq);
    RUN(test_init_missing_set_duty);
    RUN(test_init_missing_delay);
    RUN(test_tone_440hz_100ms);
    RUN(test_tone_rest);
    RUN(test_tone_zero_duration);
    RUN(test_tone_null_dev);
    RUN(test_off);
    RUN(test_off_null_dev);
    RUN(test_play_simple_melody);
    RUN(test_play_with_rest);
    RUN(test_play_single_note);
    RUN(test_play_null_dev);
    RUN(test_play_null_melody);
    RUN(test_play_zero_count);
    RUN(test_beep_single);
    RUN(test_beep_triple);
    RUN(test_beep_null_dev);
    RUN(test_beep_zero_count);
    RUN(test_beep_zero_on_ms);
    RUN(test_midi_a4_440);
    RUN(test_midi_c4_262);
    RUN(test_midi_c5_523);
    RUN(test_midi_note_0);
    RUN(test_midi_note_127);
    RUN(test_device_refused_after_rejected_init);
TEST_MAIN_END()
