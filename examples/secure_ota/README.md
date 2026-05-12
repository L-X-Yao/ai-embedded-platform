# Secure OTA Update -- Integration Recipe

Demonstrates seven wheels working together as a complete secure firmware
over-the-air update pipeline. Runs entirely on the host with simulated
flash storage -- no hardware required.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `aes128` | Encrypt/decrypt firmware chunks (AES-128 ECB) |
| L0 | `sha256` | Hash firmware image for integrity |
| L0 | `hmac_sha256` | Authenticate firmware (keyed MAC) |
| L0 | `crc32` | Per-chunk integrity check during download |
| L3 | `boot_counter` | Track boot count and uptime across resets |
| L3 | `ota_manager` | Orchestrate OTA state machine |
| L3 | `flash_journal` | Log boot and update events to flash |

## What It Demonstrates

1. **Mock flash storage** -- A static array simulates flash sectors,
   shared between flash_journal (sectors 0-3) and OTA firmware area.

2. **Crypto pipeline** -- Firmware is encrypted with AES-128, each chunk
   verified with CRC32, then the full image authenticated with SHA-256
   hash and HMAC-SHA256 signature.

3. **OTA state machine** -- ota_manager drives the full lifecycle:
   idle -> check -> download -> verify -> activate.

4. **Persistent boot tracking** -- boot_counter loads, increments, and
   persists boot count via injected storage callbacks.

5. **Event journaling** -- flash_journal appends structured events
   ("boot", "ota_complete") and reads them back for audit.

## Build and Run

```bash
cd examples/secure_ota
cmake -GNinja -B build && ninja -C build && ./build/secure_ota
```

Expected output: all 8 integration tests pass, demonstrating the full
secure OTA pipeline from boot through firmware download and verification.

## Data Flow

```
  prepare_firmware()
       |
       v
  [plaintext] --SHA256--> fw_sha256 (expected hash)
       |       --HMAC---> fw_hmac   (expected MAC)
       |       --AES128-> [encrypted] --CRC32--> fw_chunk_crc[]
       |
       v
  ota_manager
    check_version()  --> update available (1024 bytes)
    download_step()  --> encrypted chunks --> CRC32 verify --> write to flash
    verify()         --> decrypt all --> SHA256 check --> HMAC check
    activate()       --> mark firmware ready
       |
       v
  flash_journal
    append("boot:v1.0.0")
    append("ota_complete:v2.0.0")
    read_back() --> verify event log
       |
       v
  boot_counter
    load --> increment --> persist --> tick uptime
```
