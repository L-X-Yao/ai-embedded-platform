# Device Configuration Shell

Integration recipe combining 5 L3 wheels into a working configuration
terminal -- the pattern found in nearly every embedded device with a
debug/service UART.

## Wheels used

| Wheel | Role |
|-------|------|
| `cli_shell` | Command parsing, tokenization, dispatch |
| `config_store` | Typed configuration with range validation and flash persistence |
| `param_table` | String-based parameter table with CLI-friendly get/set/list |
| `version_info` | Semantic version parsing and formatting |
| `error_handler` | Error logging ring buffer with severity levels |

## What it demonstrates

1. **CLI command dispatch** -- register commands on `cli_shell`, feed
   simulated keystrokes, observe output.
2. **Parameter persistence** -- `config set` writes through both
   `param_table` (string view) and `config_store` (typed persistence).
   Save to mock flash, mutate in-memory, reload from flash and verify
   the original value is restored.
3. **Read-only protection** -- `hw_revision` is defined as read-only in
   `param_table`; attempts to set it are rejected.
4. **Error reporting** -- inject errors via `error_handler_report()`,
   then inspect them through the `errors` shell command.
5. **Device status** -- the `status` command pulls data from all wheels
   to show a unified overview.

## Build (host, WSL / Linux)

```bash
cd examples/device_shell
mkdir -p build && cd build
cmake .. -G Ninja
ninja
./device_shell
```

## Expected output (abridged)

```
=== Device Configuration Shell — Integration Recipe ===

--- Scene 1: Check firmware version ---
device> version
Firmware v1.3.7

--- Scene 4: Set parameter + verify ---
device> config set sensor_interval 500
OK: sensor_interval = 500
device> config get sensor_interval
sensor_interval = 500

--- Scene 6: Error reporting ---
(Injected 3 error records)
device> errors
--- Error Log (3 records) ---
  [1000] code=0x0010 sev=WARN  ctx=0x00000000
  [2500] code=0x0020 sev=ERROR ctx=0x0000DEAD
  [3200] code=0x0030 sev=INFO  ctx=0x00000042

--- Scene 8: Persistence (save → change → load) ---
device> config get device_name
device_name = CHANGED
(Reloaded config from mock flash)
device> config get device_name
device_name = DevShell-01

=== Demo complete ===
```

## Architecture notes

- All wheels are passive libraries (no OS, no threads, no heap).
- `cli_shell` receives bytes via `cli_shell_feed()` -- in production
  this is called from a UART RX ISR or polling loop.
- `config_store` persistence callbacks (`save`/`load`) are injected at
  init -- this recipe uses a RAM buffer; a real device would use flash
  or EEPROM drivers.
- `param_table` provides the human-readable string layer; `config_store`
  provides typed storage with range validation.  The two are kept in
  sync by the `config set` command handler.
