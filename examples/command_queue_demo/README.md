# command_queue_demo -- ASCII Command Dispatcher

Integration recipe showing how to receive ASCII commands over UART,
queue them by priority, and dispatch from the main loop. This is the
canonical pattern for a CLI shell where parsing happens fast in one
context (UART ISR / poll) and actions happen later in priority order.

## Wheels used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `ring_buffer` | UART RX byte buffer between input and parser |
| L0 | `cli_parser` | Tokenize and route a complete line to a handler |
| L0 | `string_utils` | Numeric argument parsing (`str_to_int`) |
| L3 | `command_queue` | Priority queue drained by the dispatch loop |

## What it demonstrates

1. **Two-stage flow** -- parse-fast (push onto queue) then execute-later
   (pop in priority order). Critical work like `reboot` runs before
   background tasks even when it arrives last on the wire.
2. **Out-of-order execution** -- scene 1 feeds `ping/led/reboot/log`
   but dispatch order is `reboot -> led -> log -> ping` because of
   priority insertion sort inside `command_queue`.
3. **Error tolerance** -- bad numeric args (`led not_a_number`) are
   rejected by the handler without queueing; unknown commands (`fly`)
   are logged by `cli_parser` but don't crash.
4. **Byte-level RX simulation** -- bytes are dripped into `ring_buffer`
   and only consumed when a complete line ending in `\n` is available.
5. **Queue-full handling** -- pushing past `QUEUE_CAPACITY` returns
   `COMMAND_QUEUE_ERR_FULL` instead of corrupting state.

## Build (host, WSL or Linux)

```bash
cd examples/command_queue_demo
mkdir -p build && cd build
cmake -S .. -B .
cmake --build .
./command_queue_demo
```

The program exits with status `0` on success and prints `=== command_queue_demo complete ===`.

## Expected output (abridged)

```
=== command_queue_demo -- ASCII command dispatcher ===
Wheels: ring_buffer + cli_parser + string_utils + command_queue

--- Scene 1: priority reorders execution ---
  [line] ping
    [parser] queued: ping (priority=9)
  [line] led 1
    [parser] queued: led 1 (priority=5)
  [line] reboot
    [parser] queued: reboot (priority=0)
  [line] log
    [parser] queued: log (priority=7)
  --> dispatch phase (high prio first)
  [dispatch] popped id=1 (reboot) prio=0 len=0
    [action] REBOOT scheduled (highest priority)
  [dispatch] popped id=2 (led) prio=5 len=1
    [action] LED -> ON
  ...

--- Scene 4: queue-full handling ---
  Pushed 16 items, 4 rejected with FULL (capacity=16)
```

## Architecture notes

- All wheels are passive: no threads, no heap, no OS calls.
- The dispatch loop is a tight `while (!empty) pop -> dispatch`. On a
  real MCU it runs from the main loop, between sensor reads.
- `command_queue_push` performs an insertion sort by priority. Items
  with equal priority preserve FIFO order, which matches scene 4's
  pings landing in arrival order.
- A real UART driver would call `ring_buffer_write(..., 1)` from the RX
  ISR; the main loop polls `ring_buffer_count() > 0` and pulls bytes
  out to feed into `cli_parser_run()` once a `\n` is observed.
