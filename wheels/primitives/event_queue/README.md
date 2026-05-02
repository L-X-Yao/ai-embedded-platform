# event_queue

**Status: candidate**
**Layer: L0 (primitives)**

Fixed-size circular FIFO event queue for bare-metal event-driven architectures.
Each event carries a type (uint16) and data payload (uint32). Single-producer
single-consumer safe (ISR push, main loop pop). No heap, caller provides buffer.
## Origin

Proactive build. Event queues are the backbone of bare-metal event-driven
firmware: ISR detects event → push to queue → main loop pops and handles.

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`.

## API

```c
event_queue_err_t event_queue_init(event_queue_t *q, event_t *buf, size_t capacity);
event_queue_err_t event_queue_push(event_queue_t *q, uint16_t type, uint32_t data);
event_queue_err_t event_queue_pop(event_queue_t *q, event_t *out);
event_queue_err_t event_queue_peek(const event_queue_t *q, event_t *out);
size_t            event_queue_count(const event_queue_t *q);
bool              event_queue_is_empty(const event_queue_t *q);
bool              event_queue_is_full(const event_queue_t *q);
event_queue_err_t event_queue_clear(event_queue_t *q);
```

## Configuration (compile-time)

None. Queue capacity is runtime (size of caller-provided array).

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

- Usable capacity is N-1 (one slot reserved for full/empty disambiguation).
- Only SPSC (single-producer single-consumer) safe. Multi-producer needs external locking.

## How to test

```bash
cd wheels/primitives/event_queue/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_event_queue
```

50 assertions across 11 test cases: init, null safety (11 paths), push+pop
round-trip, FIFO ordering, full queue rejection, empty pop, peek without
consume, peek empty, clear, wraparound with index recycling, data payload
preservation.

## Test status

- Unit tests (sim): PASS (11 C tests, 50 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Circular buffer with head/tail (classic lock-free SPSC pattern); event_t uses uint32_t data field (fits pointer on 32-bit, or small payload directly); volatile head/tail for ISR safety; capacity-1 usable slots (simpler than counter-based full detection); push returns ERR_FULL instead of overwriting (caller decides drop policy).

## Responsible human
@user
