# boot_counter -- persistent boot count and reset reason tracker

**Status: candidate**
**Layer: L3 (modules)**

Tracks cumulative boot count, last reset reason code, and runtime uptime.
Storage is abstracted via read/write callbacks so it can wire to kvs, flash,
EEPROM, or any persistent backend. Passive design -- no OS calls, no heap.

## Origin

Proactive build for embedded device lifecycle tracking (boot counting,
reset-reason logging, uptime monitoring).


## Why self-written?

Searched GitHub (2026-05-02). No suitable alternative found. ~60-line passive counter; too simple for external dep. Self-written as architecture-specific passive module per platform conventions.

## Dependencies

None. Storage injected via callbacks.

## API

```c
boot_counter_err_t boot_counter_init(boot_counter_t *ctx,
                                     const boot_counter_storage_t *storage);
boot_counter_err_t boot_counter_set_second_copy(boot_counter_t *ctx,
                                                boot_counter_storage_read_fn read2,
                                                boot_counter_storage_write_fn write2);
boot_counter_err_t boot_counter_load(boot_counter_t *ctx);
boot_counter_err_t boot_counter_increment(boot_counter_t *ctx);
uint32_t           boot_counter_get_count(const boot_counter_t *ctx);
uint32_t           boot_counter_get_reset_reason(const boot_counter_t *ctx);
boot_counter_err_t boot_counter_set_reset_reason(boot_counter_t *ctx,
                                                  uint32_t reason);
uint32_t           boot_counter_get_uptime_seconds(const boot_counter_t *ctx);
void               boot_counter_tick_second(boot_counter_t *ctx);
```

## Configuration (compile-time)

None. All configuration via runtime callbacks.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Storage

The record is `sizeof(boot_counter_record_t)` bytes, stored as it is in memory.
It carries a magic and a CRC-32, and anything that fails them, erased flash
included, loads as no record: the count starts from zero.

The read callback has three answers: 0, `BOOT_COUNTER_READ_NO_RECORD` when
nothing has been stored yet, and any other value for a failure. After a failure
`boot_counter_load` returns `BOOT_COUNTER_ERR_STORAGE` and nothing is saved
until a load succeeds, so a transient read error cannot wipe the count.

A flash backend erases before it programs, and power can fail in between.
Give it a second location to survive that:

```c
boot_counter_init(&bc, &storage);                    /* first sector */
boot_counter_set_second_copy(&bc, read_b, write_b);  /* second sector */
boot_counter_load(&bc);
```

Records then alternate between the two and load takes the newest valid one.
The two must not share an erase unit.

## Known issues

- With one copy, power lost between erase and program loses the record, and the
  count starts again from zero. Use `boot_counter_set_second_copy()`.
- With two copies, a power cut loses the save it interrupts: after the reboot
  the count is the one from before that save.

## Migration

### Checked record, second copy, no-record answer (2026-11)

**What changed.**

- `boot_counter_record_t` grew from 8 to 20 bytes: `magic`, `seq` and `crc`
  follow the two old fields.
- `boot_counter_load` treats a record that fails its checks as no record, and
  returns `BOOT_COUNTER_OK` with a count of 0.
- New `BOOT_COUNTER_READ_NO_RECORD` and `boot_counter_set_second_copy()`.
- A failed load now also blocks saves when an earlier load succeeded.
- `uptime_seconds` is `volatile`. `boot_counter_get_count()` and
  `boot_counter_get_reset_reason()` are no longer labelled thread safe, because
  they never were: see the header for who may call what.
- `boot_counter_t` is opaque storage and grew.

**What a caller has to do.**

- Storage must hold `sizeof(boot_counter_record_t)`. A backend with a fixed
  8-byte slot has to grow.
- A backend that fails a read of a key nobody wrote should return
  `BOOT_COUNTER_READ_NO_RECORD`. One that returns anything else stays stuck on
  `BOOT_COUNTER_ERR_STORAGE`, as it always was.
- Call `boot_counter_set_second_copy()` to keep the count across a power cut.
- Serialize `boot_counter_get_count()` and `boot_counter_get_reset_reason()`
  with load, increment and set_reset_reason.

**If you do nothing.** A record in the old 8-byte format fails the checks, so
**the count starts again from zero once**, after the update. From then on
erased flash reads as 0 and not as 4294967295, and the window under "Known
issues" stays open.

## How to test

```bash
cd wheels/modules/boot_counter/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_boot_counter
```

## Test status

- Unit tests (sim): 35 tests, all passing, including a power cut in every erase
  and program of a run, at every byte
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Storage via callback injection (not direct kvs/flash dep).
  Record struct serialized as-is (endian-safe on same MCU).

## Responsible human

@L-X-Yao
