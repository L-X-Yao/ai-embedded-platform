# Tooling

Platform development tools, organized by domain. Run `make help` to see all available targets.

## Directory structure

| Directory | Purpose | Key files |
|-----------|---------|-----------|
| `audit/` | Code quality checks (55 automated) | `audit_wheels.py`, `audit_fix.py`, `audit_compile.py`, `audit_sanitizer.py`, `audit_coverage.py`, `audit_deep.sh`, `audit_report.sh`, `memory_audit.py`, `arm_size_audit.sh`, `arm_stack_audit.sh`, `run_frama_c.sh`, `_audit_build_base.py`, `doc_drift_check.py` |
| `test/` | Test infrastructure + runners | `test_harness.h`, `mock_i2c.h`, `mock_spi.h`, `mock_gpio.h`, `hw_model.h`, `contract.h`, `property_test.h`, `test_all.sh`, `test_downstream.sh`, `test_scaffold.sh`, `coverage_scan.sh`, `coverage_wheel.sh`, `run_generated_tests.sh`, `test_mutation.sh`, `test_differential.sh`, `gen_null_tests.py`, `gen_sentinel_tests.py`, `gen_boundary_tests.py` |
| `wheel/` | Wheel lifecycle | `new_wheel.sh`, `wheel_promote.sh`, `find_all_refs.sh`, `release.sh` |
| `session/` | Session management + dispatch | `bootstrap.sh`, `handoff.sh`, `dispatch_agent.sh` |
| `metrics/` | Platform health tracking | `platform_metrics.sh`, `platform_metrics_impl.py` |
| `ci/` | CI and repo maintenance | `sync_vsf.sh`, `update_wheel_inventory.sh`, `commit_suggest.sh` |
| `fix/` | Batch repair tools | `batch_fix.sh` (only remaining script after one-time fix cleanup) |
| `debug/` | MCU-specific debug toolkits | `gd32f4xx/` (parse_fault, stack_analyzer, GDB scripts) |
| `hil/` | Hardware-in-loop testing | `hil_runner.py`, `build_flash/` |
| `patches/` | VSF upstream patches | `apply.sh` |
| `experiments/` | Exploration scaffolds | `vsf-tryouts/` (kernel_test, mps2_bare, linux_shell_size) |

## Quick start

```bash
make help          # list all targets
make check         # test + audit + doc-drift (pre-push)
make bootstrap     # session context dump
make test-parallel # run all tests in parallel
make audit         # run 55 automated audit checks
```

## Prerequisites

### Required

| Tool | Used by | Install (Ubuntu/WSL) |
|------|---------|----------------------|
| `gcc` | all tests, audit_compile | `sudo apt install gcc` |
| `cmake` + `ninja` | all test builds | `sudo apt install cmake ninja-build` |
| `python3` | audit scripts, metrics | `sudo apt install python3` |
| `pytest` | log/crc16 wheel tests | `pip3 install pytest` |

### Optional

| Tool | Used by | Install |
|------|---------|---------|
| `arm-none-eabi-gcc` | `arm-size-audit`, HIL builds | `sudo apt install gcc-arm-none-eabi` |
| `clang` + `clang++` | LibFuzzer (`audit-deep --layer 3`) | `sudo apt install clang` |
| `cppcheck` | `audit-deep --layer 6` static analysis | `sudo apt install cppcheck` |
| `rsync` | `sync-vsf` upstream sync | `sudo apt install rsync` |
| `openocd` | HIL flashing (`hil_runner.py`) | `sudo apt install openocd` |

## Dispatch (parallel agent execution)

```bash
# Launch a Claude Code session in an isolated worktree
bash tooling/session/dispatch_agent.sh \
  --name my-task --budget 5 --scope "wheels/" \
  --prompt "do the work"

# Merge all completed branches
bash tooling/session/dispatch_agent.sh --merge-all --push
```

## Adding new tools

Place scripts in the appropriate subdirectory. Each script should support `--help` or `-h`. Add a corresponding `make` target in the root `Makefile`.
