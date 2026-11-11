# command_queue — prioritized command queue for task dispatching

**Status: candidate**
**Layer: L3 (modules)**

Fixed-size priority queue for dispatching commands. Commands are
insertion-sorted by priority (lower numeric value = higher priority).
Commands with equal priority are dequeued in FIFO order.
Caller provides the backing array — no heap, fully reentrant per-context.

## Origin

Proactive build for embedded task dispatching patterns (command handlers,
remote-control packet routing, multi-source command arbitration).

## Why self-written?

Searched GitHub (2026-05-02). No suitable alternative found. Priority-sorted command dispatcher; generic queues don't replace. Self-written as architecture-specific passive module per platform conventions.

## Dependencies

None.

## API

```c
command_queue_err_t command_queue_init(command_queue_t *q,
                                      command_queue_item_t *buf,
                                      size_t capacity);

command_queue_err_t command_queue_push(command_queue_t *q,
                                      uint16_t cmd_id,
                                      uint8_t priority,
                                      const void *data,
                                      uint8_t len);

command_queue_err_t command_queue_pop(command_queue_t *q,
                                     command_queue_item_t *out);

command_queue_err_t command_queue_peek(const command_queue_t *q,
                                      command_queue_item_t *out);

size_t              command_queue_count(const command_queue_t *q);

command_queue_err_t command_queue_clear(command_queue_t *q);

bool                command_queue_is_empty(const command_queue_t *q);
bool                command_queue_is_full(const command_queue_t *q);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `COMMAND_QUEUE_DATA_MAX` | 16 | Max payload bytes per command item |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/modules/command_queue/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_command_queue
```

## Test status

- Unit tests (sim): 15 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Insertion-sorted array chosen for simplicity and
  predictable O(n) push in small fixed-size queues (typical embedded
  command queues are <32 items). Priority 0 = highest.

## Responsible human

@L-X-Yao
