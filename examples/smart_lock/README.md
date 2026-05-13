# Smart Lock -- Integration Recipe

Demonstrates six wheels working together as a multi-factor smart door
lock. RFID card + numeric PIN unlocks the door; every attempt --
success or failure -- is written to an AES-128 encrypted audit log on
flash. Runs entirely on the host with mocked SPI / I2C / PWM /
storage -- no hardware required.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `aes128`        | AES-128 ECB block encrypt for audit-log records |
| L1 | `rc522`         | MFRC522 RFID reader (driver runs unmodified) |
| L1 | `cst816s`       | Capacitive touch IC (used as numeric keypad) |
| L1 | `buzzer`        | PWM-driven audible feedback |
| L3 | `event_bus`     | Pub/sub decouples reader, keypad, lock controller |
| L3 | `flash_journal` | Append-only encrypted audit log on emulated flash |

## What It Demonstrates

1. **Pub/sub decoupling** -- The reader, keypad, lock controller, and
   pretty-printer all communicate exclusively through `event_bus`.
   The reader doesn't know about the lock controller; the keypad
   doesn't know about the journal.

2. **Multi-factor authentication state machine** -- The lock
   controller subscribes to three event types (`CARD_PRESENT`,
   `PIN_DIGIT`, `PIN_COMMIT`) and walks an IDLE -> AWAITING_PIN ->
   OPENED / LOCKED_OUT state machine.

3. **Anti-brute-force lockout** -- Three wrong PINs while a known
   card is present transition to `LOCK_LOCKED_OUT`, with a dedicated
   audit-log entry.

4. **Master PIN escape hatch** -- A 6-digit master code (`777777`)
   opens the lock without a card, even from any state.

5. **Encrypted audit logging** -- Each attempt is serialized into a
   16-byte plaintext record (event code + UID + PIN + sequence),
   encrypted as a single AES-128 block, and appended to
   `flash_journal`. The journal's wear-leveling and CRC32 footer come
   for free from the wheel.

6. **Driver mocking patterns** -- The RC522 mock implements a small
   SPI register state machine that satisfies the real driver's
   transceive protocol; the CST816S mock plants register values
   directly; the buzzer mock records aggregate on-time. All three
   show different but equally simple HAL stub idioms.

## Build and Run

```bash
cd examples/smart_lock
cmake -B build -S . && cmake --build build && ./build/smart_lock
```

Expected output: 8 integration tests pass. Each scenario prints its
auth-result events; the final test reports the encrypted journal
count and decrypts the latest entry.

## Scenarios (Test Plan)

| Scenario | Expected outcome |
|----------|-----------------|
| 1. Card A + correct PIN     | OPENED, 1 audit "OK" entry |
| 2. Card B + 3 wrong PINs    | LOCKED_OUT, 4 audit entries (3 reject + 1 lockout) |
| 3. Unknown card swipe       | rejected, 1 audit entry |
| 4. PIN-only without card    | rejected, 1 audit entry |
| 5. Master PIN (777777)      | OPENED regardless of state, 1 audit entry |
| Total                       | 8 encrypted journal entries |

## Data Flow

```
   RC522 SPI -----+
                  |  CARD_PRESENT
                  v
   CST816S I2C --> [event_bus] <-- lock controller (subscribes to all)
                  ^                  |
                  |  PIN_DIGIT       |  AUTH_OK / AUTH_FAIL / LOCKOUT
                  |  PIN_COMMIT      v
                  |             pretty-printer (subscriber)
                  |                  |
                  |                  v
                  |        +---------------+
                  |        | flash_journal |
                  +------> | (encrypted    |
                           |  via aes128)  |
                           +---------------+
                                  |
                                  v
                           Buzzer beep pattern
                           via PWM HAL (mock)
```
