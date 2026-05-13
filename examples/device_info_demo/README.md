# device_info_demo -- Boot Banner with Device Identity

Integration recipe demonstrating the boot banner pattern: at startup
the firmware checks its own version, generates a UUID for this run,
populates a device_info block from build-time constants, and routes
the entire identity record through the structured log subsystem.

## Wheels used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `log` | Leveled, module-tagged log lines on stdout |
| L0 | `uuid` | RFC 4122 UUID v4 generation for per-boot device ID |
| L3 | `version_info` | Parse / compare / format semantic firmware versions |
| L3 | `device_info` | Hold and serialize the identity block (FW/HW/SN/build/uptime) |

## What it demonstrates

1. **Version floor check** -- parses `FIRMWARE_VERSION` and
   `MIN_REQUIRED_VER`, refuses to boot if firmware is too old
   (`version_compatible`).
2. **Per-boot UUID** -- `uuid_v4_generate` pulls 16 bytes from a
   caller-supplied PRNG callback (here a deterministic xorshift32 for
   reproducible output; a real device would use TRNG or a flash-stored
   permanent ID).
3. **Device-info banner** -- `device_info_set_*` populates each field,
   `device_info_format` emits `key=value` lines, the demo forwards each
   line into `LOG_INFO` so the banner shows up with timestamps + level
   tags on whichever sink is wired.
4. **Uptime tick** -- five `device_info_tick()` calls bump uptime,
   and `log_set_time_fn` is wired up before the second banner so the
   timestamps appear on the post-boot logs.
5. **Overflow handling** -- an over-long serial number gets safely
   truncated by `device_info` with `DEVICE_INFO_ERR_OVERFLOW`; the
   demo verifies the truncated string is still NUL-terminated.

## Build (host, WSL or Linux)

```bash
cd examples/device_info_demo
mkdir -p build && cd build
cmake -S .. -B .
cmake --build .
./device_info_demo
```

The program exits with status `0` on success and prints `=== device_info_demo complete ===`.

## Expected output (abridged)

```
=== device_info_demo -- identity banner ===
Wheels: log + uuid + version_info + device_info
==========================================

[I] [boot] system boot
[I] [boot] firmware version: 2.4.1 (required >= 2.0.0)
[I] [boot] version_compare = 1  (negative = older, 0 = equal, positive = newer)
[I] [boot] version_compatible = YES
[I] [boot] ============= DEVICE BANNER ============
[I] [boot]   device_name=FlyboxPro
[I] [boot]   firmware_version=2.4.1
[I] [boot]   hardware_version=rev-C2
[I] [boot]   serial_number=AEP-2026-00347
[I] [boot]   build_date=2026-05-13
[I] [boot]   uptime_seconds=0
[I] [boot]   boot_count=42
[I] [boot]   reset_reason=1
[I] [boot]   device_uuid=37820fbc-d2b4-4c95-9a18-...
[I] [boot] ========================================
[I] [boot] simulating 5 seconds of uptime...
[00005000] [I] [boot] uptime now 5 seconds (expected 5)
...
[I] [boot] device_info_demo complete

=== device_info_demo complete ===
```

## Architecture notes

- The PRNG used by `uuid_v4_generate` is injected via a callback. In
  real firmware swap this for a TRNG peripheral, a flash-stored seed,
  or RNG output from your crypto module.
- `device_info_tick` is meant to be called once per second from an RTC
  or SysTick handler. The demo invokes it synchronously; in a real
  device this is the only stateful timer interaction.
- `device_info_format` returns the entire identity block as one
  multi-line buffer; the demo splits it on `'\n'` so each field can be
  routed through `LOG_INFO` individually. This makes the banner show
  up correctly with module tags and timestamps.
- `version_compatible(required, actual)` requires `actual.major ==
  required.major` and `actual >= required`. Major-version bumps are
  always treated as breaking.
