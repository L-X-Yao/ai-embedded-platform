# rc522 -- MFRC522 SPI RFID/NFC reader driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the NXP MFRC522 contactless reader IC.
SPI interface injected via callbacks. Supports ISO/IEC 14443-A card
detection (REQA), UID reading (cascade level 1, 4-byte), self-test,
antenna control, and HALT command.

## Origin

Proactive build. MFRC522 is the most common low-cost 13.56 MHz RFID
reader used in access control, attendance, and inventory systems.

## Why self-written?

No existing open-source component for MFRC522. Self-written for zero-dependency SPI HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

SPI HAL (injected via `rc522_spi_t` callbacks). No other dependencies.

## API

```c
rc522_err_t rc522_init(rc522_t *dev, const rc522_spi_t *spi);
rc522_err_t rc522_self_test(rc522_t *dev);
rc522_err_t rc522_detect_card(rc522_t *dev);
rc522_err_t rc522_read_card_uid(rc522_t *dev, rc522_uid_t *uid);
rc522_err_t rc522_halt(rc522_t *dev);
rc522_err_t rc522_antenna_on(rc522_t *dev);
uint8_t     rc522_get_version(const rc522_t *dev);
```

- `detect_card()` -- sends REQA, returns OK if a PICC responds
- `read_card_uid()` -- reads 4-byte UID (call after detect_card)
- `halt()` -- puts PICC into HALT state

## Usage

```c
rc522_t reader;
rc522_init(&reader, &spi_hal);

if (rc522_detect_card(&reader) == RC522_OK) {
    rc522_uid_t uid;
    rc522_read_card_uid(&reader, &uid);
    // uid.bytes[0..3], uid.len == 4
    rc522_halt(&reader);
}
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected SPI HAL)

## Known issues

- Only cascade level 1 (4-byte UID) is supported; 7-byte and 10-byte
  UIDs require cascade levels 2 and 3 (not implemented).
- MIFARE authentication and block read/write not implemented.

## How to test

```bash
cd wheels/drivers/rc522/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_rc522
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
