# HIL soak-test runner

Wraps `build_flash_check.sh` with soak profiles, result archival, and run comparison.

## Prerequisites

- Python 3.8+
- Hardware connected: debug probe (CMSIS-DAP) + debug UART (COM port)
- OpenOCD in PATH
- Project `.conf` in `tooling/scripts/build_flash/`

## Quick start

```bash
# Run a quick boot check (30s)
python tooling/hil/hil_runner.py -p flybox --soak quick

# 10-minute standard soak, skip rebuild
python tooling/hil/hil_runner.py -p flybox --soak standard --skip-build

# 30-minute stress soak
python tooling/hil/hil_runner.py -p flybox --soak stress

# List past results
python tooling/hil/hil_runner.py -p flybox --list

# Show latest result
python tooling/hil/hil_runner.py -p flybox --latest

# Compare latest vs previous
python tooling/hil/hil_runner.py -p flybox --compare latest previous
```

## Soak tiers

| Tier | Default | Purpose |
|------|---------|---------|
| `quick` | 30s | Boot check — sensors init, tasks start |
| `standard` | 600s | Normal verification — network, MQTT cycle |
| `stress` | 1800s | Extended soak — memory leaks, watchdog stability |

Override defaults per-project in the `.conf`:

```bash
SOAK_QUICK="30"
SOAK_STANDARD="600"
SOAK_STRESS="1800"
```

## Results

Each run saves to `tooling/hil/results/<project>/<YYYYMMDD_HHMMSS>/`:

```
20260427_143022/
  serial.log      # full serial capture
  verdict.json    # pass/fail + counters (from build_flash_check.sh)
  meta.json       # timestamp, git commit, soak tier, duration
```

Results are gitignored — they are local hardware test artifacts.

## Adding a new project

1. Create `tooling/scripts/build_flash/<project>.conf` (copy `example.conf`)
2. Add `SOAK_QUICK` / `SOAK_STANDARD` / `SOAK_STRESS` lines
3. Run: `python tooling/hil/hil_runner.py -p <project> --soak quick`
