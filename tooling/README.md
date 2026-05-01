# Tooling

Platform development tools, organized by domain. Run `make help` to see all available targets.

## Directory structure

| Directory | Purpose | Key files |
|-----------|---------|-----------|
| `audit/` | Code quality checks (25 automated) | `audit_wheels.py`, `audit_fix.py`, `doc_drift_check.py`, `arm_size_audit.sh` |
| `test/` | Test infrastructure + runners | `test_harness.h`, `mock_i2c.h`, `mock_spi.h`, `test_all.sh`, `test_downstream.sh` |
| `wheel/` | Wheel lifecycle | `new_wheel.sh`, `wheel_promote.sh`, `find_all_refs.sh`, `release.sh` |
| `session/` | Session management + dispatch | `bootstrap.sh`, `handoff.sh`, `dispatch_agent.sh` |
| `metrics/` | Platform health tracking | `platform_metrics.sh`, `platform_metrics_impl.py` |
| `ci/` | CI and repo maintenance | `sync_vsf.sh`, `update_wheel_inventory.sh`, `commit_suggest.sh` |
| `fix/` | Batch repair tools | `batch_fix.sh` |
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
make audit         # run 25 automated audit checks
```

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
