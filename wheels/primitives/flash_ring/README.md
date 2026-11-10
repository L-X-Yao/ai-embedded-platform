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

- `push()` -- append a frame. When the ring is full it drops the oldest
  frames a sector at a time: see Capacity. OK means the frame is stored.
  `FLASH_RING_ERR_IO` means it is not, and a retry is safe: a program that
  failed but wrote something leaves its slot behind, so the retry takes the
  next slot.
- `peek()` -- read a frame without consuming it. `idx` is an absolute slot
  index between `read_idx` and `write_idx - 1`, so the oldest frame is at
  `ring.read_idx`. That is the one field of `flash_ring_t` a caller may read,
  and there is no call that returns it. Returns `FLASH_RING_ERR_EMPTY` on an
  empty ring and `FLASH_RING_ERR_PARAM` for any other slot or for `len` above
  `slot_size`.
- `pop()` -- discard the oldest frame
- `flush_header()` -- force-write persistent header to flash. Unlike push and
  pop this reports a failed header write.

## Capacity

The writer keeps the rest of its own sector blank, and erases the next
sector just before it programs the last slot of the current one. So:

- The ring needs more than one sector. `flash_ring_init` returns
  `FLASH_RING_ERR_PARAM` for `total_slots <= sector_size / slot_size`.
- It holds at most `total_slots - 1` frames, and right after an erase it
  holds one sector less.
- Frames are lost a sector at a time: up to `sector_size / slot_size` of the
  oldest go when the writer erases the sector they are in, and `read_idx`
  moves past them. It moves even if that erase fails, because the sector may
  be damaged and the retry erases it anyway.

## Power loss

- The header on flash is up to `persist_interval` operations old. Init reads
  forward from the stored `write_idx` and finds the real one, so no frame that
  was pushed is lost or programmed over.
- The header is rewritten before the writer can come back round to the stored
  position, even if `persist_interval` is larger than that, and however often
  the device resets: init counts what it walked over towards the next write.
- A failed header write does not fail push or pop. The write stays due, so the
  next push or pop tries again.
- Frames popped since the last header write come back after a reset, so a
  reader sees each frame at least once, not exactly once.
- Blank means 0xFF: the flash must erase to 0xFF. A frame of all 0xFF at the
  very end of what was written looks blank and is lost at a reset. On the last
  slot of a sector it also hides anything written after it that the header
  does not yet know about, so give frames a field that is never all 0xFF.
- A cut during a program leaves one torn frame as the newest. Frames carry no
  checksum here, so put one in the frame if that matters.
- A reset with `write_idx` on the last slot of a sector gives up the frames in
  the next sector, because the next push erases them anyway and a cut during
  that erase cannot be told apart.
- `test_powercut_flash_ring.c` cuts the power at every erase and program of a
  three lap script, with the operation lost or half done.

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
flash_ring_peek(&ring, ring.read_idx, out, 64);
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
- The header has one copy, so a cut between its erase and its program resets
  the ring to empty with the frames still in flash.
- A partial last sector is still erased whole by `erase_sector`, so nothing
  else can live in the rest of that sector.
- The header sector wears much faster than the data. It is erased once per
  header write, so for each erase of a data sector it sees about
  `total_slots / persist_interval` erases, and twice that when every frame is
  also popped. With 32752 slots and an interval of 64, a part rated for
  100000 cycles reaches that count in the header sector after about 3.2
  million frames pushed and popped. Size `persist_interval` for the part.
- Flash that a ring written by the old erase-on-entry code left with no blank
  slot costs one full read at the first init. That init erases a sector and
  rewrites the header. A stored `write_idx` part way through a sector keeps
  the rest of that sector, which holds the newest frames, and gives up the next
  one.

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
