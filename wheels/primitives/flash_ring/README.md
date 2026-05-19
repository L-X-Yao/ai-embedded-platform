# flash_ring -- flash-backed circular FIFO with persistent header

**Status: candidate**
**Layer: L0 (primitives)**

Flash-backed circular FIFO for storing fixed-size data frames across
power cycles. Manages a persistent header sector that tracks write/read
indices and frame count. Supports push, peek by index, pop, bulk discard,
and configurable header flush interval to reduce flash wear.

## Origin

Proactive build. Flash-backed ring buffers are essential for data loggers,
black-box recorders, and telemetry systems that must survive power loss.

## Why self-written?

No existing open-source equivalent for a standalone flash ring buffer. Self-written for
transport-agnostic flash HAL injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Flash HAL injected via `flash_ring_io_t` callbacks (erase_sector,
write, read).

## API

```c
flash_ring_err_t flash_ring_init(flash_ring_t *ring,
                                 const flash_ring_io_t *io,
                                 const flash_ring_cfg_t *cfg);
flash_ring_err_t flash_ring_push(flash_ring_t *ring,
                                 const uint8_t *frame, uint16_t len);
flash_ring_err_t flash_ring_peek(const flash_ring_t *ring, uint32_t idx,
                                 uint8_t *buf, uint16_t len);
flash_ring_err_t flash_ring_pop(flash_ring_t *ring);
uint32_t         flash_ring_count(const flash_ring_t *ring);
flash_ring_err_t flash_ring_discard_all(flash_ring_t *ring);
flash_ring_err_t flash_ring_flush_header(flash_ring_t *ring);
```

- `push()` -- append a frame (overwrites oldest when full)
- `peek()` -- read frame by index without consuming
- `pop()` -- discard the oldest frame
- `flush_header()` -- force-write persistent header to flash

## Usage

```c
flash_ring_t ring;
flash_ring_cfg_t cfg = {
    .slot_size = 64,
    .total_slots = 256,
    .sector_size = 4096,
    .header_addr = 0x00000,
    .data_start_addr = 0x01000,
    .persist_interval = 16    /* flush header every 16 writes */
};
flash_ring_init(&ring, &flash_io, &cfg);

uint8_t frame[64];
flash_ring_push(&ring, frame, 64);

uint8_t out[64];
flash_ring_peek(&ring, 0, out, 64);
flash_ring_pop(&ring);
```

## Configuration (compile-time)

None. All parameters are runtime-configurable via `flash_ring_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected flash HAL)

## Known issues

- Slot size is fixed at init time; variable-length frames are not supported.
- Header sector must be a dedicated flash sector (not shared with data).

## How to test

```bash
cd wheels/primitives/flash_ring/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_flash_ring
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
