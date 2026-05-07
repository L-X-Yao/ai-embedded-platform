# buzzer

**Status: candidate**
**Layer: L1 (drivers)**

Passive buzzer driver via PWM. Generates tones by controlling PWM frequency
(~50% duty for loudest output). Supports single tones, beep patterns, and
melody playback from note arrays. Includes MIDI note-to-frequency lookup
(notes 0-127, A4=440 Hz).

## Origin

Proactive build for common embedded components (2026-05-07 batch).

## Why self-written?

Passive buzzer driving is trivial PWM control with no chip-specific protocol.
No platform-agnostic pure C library exists to vendor. Self-written with HAL
injection per platform conventions.

## Dependencies

PWM HAL (injected via callbacks: set_freq, set_duty, delay_ms).

## API

```c
buzzer_err_t buzzer_init(buzzer_t *dev, const buzzer_pwm_t *pwm);
buzzer_err_t buzzer_tone(buzzer_t *dev, uint16_t freq_hz, uint16_t duration_ms);
buzzer_err_t buzzer_off(buzzer_t *dev);
buzzer_err_t buzzer_play(buzzer_t *dev, const buzzer_note_t *melody, uint16_t count);
buzzer_err_t buzzer_beep(buzzer_t *dev, uint16_t count, uint16_t on_ms, uint16_t off_ms);
uint16_t     buzzer_note_freq(uint8_t midi_note);
```

## Supported platforms

- [x] Linux host (sim) -- passed 2026-05-07
- [ ] Windows (TDM-GCC)

## Known issues

None.

## How to test

```bash
cd wheels/drivers/buzzer/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **28 C cases** -- init (6), tone (4), off (2), play melody (6), beep (5), MIDI freq (5)
- Null parameter tests: **7 auto-generated cases**

## API convention deviations

None.

## API review

- Proposal date: 2026-05-07
- Reviewer: @user (pending review)

## Responsible human

@user
