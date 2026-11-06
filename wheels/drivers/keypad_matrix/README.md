# keypad_matrix — GPIO matrix keypad scanner

**Status: candidate**
**Layer: L1 (drivers)**

Row/column scanning driver for matrix keypads (e.g. 4x4 membrane keypad).
Drives rows low one at a time, reads columns to detect pressed keys.
Includes 2-scan debounce and press/release edge detection.

## Origin

Proactive build for generic matrix keypad input on MCU projects.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Simple GPIO scanning, platform-tied. Self-written with HAL injection per platform conventions.

## Dependencies

None (HAL callbacks injected by caller).

## API

```c
keypad_err_t   keypad_matrix_init(keypad_matrix_t *kp, const keypad_hal_t *hal,
                                  uint8_t rows, uint8_t cols, const char *keymap);
void           keypad_matrix_scan(keypad_matrix_t *kp);
keypad_event_t keypad_matrix_get_event(keypad_matrix_t *kp, char *key);
char           keypad_matrix_get_pressed(const keypad_matrix_t *kp);
bool           keypad_matrix_is_pressed(const keypad_matrix_t *kp, char key);
```

## Configuration (compile-time)

| Define          | Default | Purpose                     |
|-----------------|---------|-----------------------------|
| KEYPAD_MAX_ROWS | 4       | Maximum number of row lines |
| KEYPAD_MAX_COLS | 4       | Maximum number of col lines, 16 at most: a higher value fails the build |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/keypad_matrix/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_keypad_matrix
```

## Test status

- Unit tests (sim): 14 pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Poll-based scan (no interrupts); 2-scan debounce; first-key-wins for multi-press; keymap is a flat char array indexed by row*cols+col.

## Responsible human

@user
