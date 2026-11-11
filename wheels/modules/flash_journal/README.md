# flash_journal -- append-only flash journal with wear leveling

**Status: candidate**
**Layer: L3 (modules)**

Append-only log journal for flash storage with automatic sector wraparound.
Entries are written sequentially with header (magic + sequence + length),
payload, and CRC32 integrity check. When all sectors are full, the oldest
sector is erased and reused, providing basic wear leveling.

## Origin

Proactive build for embedded data logging use cases (flight logs, sensor
history, error journals) where a simple append-only journal over raw flash
sectors is needed without a full filesystem.

## Why self-written?

Searched GitHub (2026-05-02). Referenced armink/FlashDB TSDB (Apache-2.0). Conceptually similar; our minimal journal adequate. Self-written for platform convention consistency.

## Dependencies

None (self-contained, includes internal CRC32 implementation).

## API

```c
/* Storage callbacks -- application provides these */
typedef struct {
    flash_journal_err_t (*read)(void *ctx, uint32_t sector, uint32_t offset,
                                uint8_t *buf, uint32_t len);
    flash_journal_err_t (*write)(void *ctx, uint32_t sector, uint32_t offset,
                                 const uint8_t *data, uint32_t len);
    flash_journal_err_t (*erase)(void *ctx, uint32_t sector);
    void *ctx;
} flash_journal_storage_t;

/* Initialize journal, scans flash to recover state after power cycle */
flash_journal_err_t flash_journal_init(flash_journal_t *jrnl,
                                       const flash_journal_storage_t *storage,
                                       uint32_t sector_count,
                                       uint32_t sector_size);

/* Append entry (header + payload + CRC32), auto-advances sectors */
flash_journal_err_t flash_journal_append(flash_journal_t *jrnl,
                                         const uint8_t *data, uint16_t len);

/* Read most recent entry */
flash_journal_err_t flash_journal_read_latest(const flash_journal_t *jrnl,
                                              uint8_t *buf, uint32_t *size);

/* Read n-th entry (0 = oldest) */
flash_journal_err_t flash_journal_read_nth(const flash_journal_t *jrnl,
                                           uint32_t n,
                                           uint8_t *buf, uint32_t *size);

/* Get entry count */
uint32_t flash_journal_count(const flash_journal_t *jrnl);

/* Erase all sectors and reset */
flash_journal_err_t flash_journal_erase_all(flash_journal_t *jrnl);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `FLASH_JOURNAL_MAX_SECTORS` | 64 | Maximum number of sectors supported |
| `FLASH_JOURNAL_MAX_ENTRY_SIZE` | 256 | Maximum payload size hint (not enforced, sector_size is the real limit) |

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/modules/flash_journal/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_flash_journal
```

## Test status

- Unit tests (sim): 18 tests, all passing
- HIL: N/A

## API convention deviations

None.

## On-flash format

Each entry:
```
[magic:4 "JRNL"][seq:4][len:2][payload:len][crc32:4]
```
- Header: 10 bytes (magic + sequence number + payload length)
- Footer: 4 bytes (CRC32 over header + payload)
- Overhead per entry: 14 bytes
- All multi-byte fields are little-endian

## API review

- **Date**: 2026-05-01
- **Key decisions**: Storage callbacks for read/write/erase sector; self-contained CRC32 (no cross-wheel include per api-conventions.md rule 0); little-endian on-flash format; sequence numbers for ordering.

## Responsible human

@L-X-Yao
