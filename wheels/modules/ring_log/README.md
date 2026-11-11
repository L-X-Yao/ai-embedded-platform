# ring_log — ring buffer structured log storage with read-back

**Status: candidate**
**Layer: L3 (modules)**

Fixed-size ring buffer of structured log entries for post-crash analysis, CLI
"show log" commands, or any scenario needing offline read-back. Oldest entries
are silently overwritten when the buffer is full. Passive library — no OS,
no malloc, no I/O.
## Origin

Proactive build for post-crash log analysis and CLI diagnostic commands.


## Why self-written?

Searched GitHub (2026-05-02). No suitable alternative found. Structured log ring with severity/timestamps; not a raw ring buffer. Self-written as architecture-specific passive module per platform conventions.

## Dependencies

None.

## API

```c
ring_log_err_t ring_log_init(ring_log_t *rl, ring_log_level_t min_level);
ring_log_err_t ring_log_write(ring_log_t *rl, ring_log_level_t level,
                              uint32_t timestamp, const char *fmt, ...);
ring_log_err_t ring_log_read(const ring_log_t *rl, uint16_t index,
                             ring_log_entry_t *entry);
ring_log_err_t ring_log_read_oldest(const ring_log_t *rl, uint16_t nth,
                                    ring_log_entry_t *entry);
uint16_t       ring_log_count(const ring_log_t *rl);
uint32_t       ring_log_total(const ring_log_t *rl);
uint32_t       ring_log_dropped(const ring_log_t *rl);
void           ring_log_clear(ring_log_t *rl);
ring_log_err_t ring_log_set_level(ring_log_t *rl, ring_log_level_t level);
```

- `read(index=0)` returns the newest entry.
- `read_oldest(nth=0)` returns the oldest entry.
- `write()` uses `vsnprintf` internally (no I/O, still passive).

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `RING_LOG_MAX_ENTRIES` | 32 | Maximum entries in the ring buffer |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/modules/ring_log/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ring_log
```

## Test status

- Unit tests (sim): 16 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Separate read (newest-first) and read_oldest (oldest-first) accessors; level filter rejects below min_level and increments dropped counter; clear preserves min_level.

## Responsible human

@L-X-Yao
