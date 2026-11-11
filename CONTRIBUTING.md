# Contributing

The project has been in maintenance since 2026-11-10. Bug fixes, hardware test results and corrections to the docs are welcome; new wheels and API changes are not being taken for now. Hardware results have their own issue form, and security problems are reported privately, see [SECURITY.md](SECURITY.md). The wheel sections below describe the process for when development resumes.

## Quick start

```bash
make bootstrap    # see project state
make check        # tests + audit + doc-drift
```

## Adding a wheel

1. Check for existing open-source implementations first (see `docs/adr/010-adopt-opensource-before-reinvent.md`)
2. Scaffold: `bash tooling/wheel/new_wheel.sh --layer <L> --name <name> [--hal i2c|spi|gpio|onewire|pwm]`
3. Implement + write tests until `make check` passes
4. Ensure coverage >= 80%: `bash tooling/test/coverage_wheel.sh wheels/<layer>/<name>`

## Wheel contract

Every wheel must be a passive library: no OS calls, no heap allocation, no task creation. Dependencies are injected via function pointers. See `docs/wheel-contract.md` for the full specification.

## Commit format

```
<type>(<scope>): <summary>
```

Types: `wheel`, `tool`, `docs`, `ci`, `project`, `port`, `exp`. See `CLAUDE.md` for the complete reference.

## Code style

- C99, `<stdint.h>` exact-width types
- `-Wall -Wextra -Werror`
- No C++ exceptions, no heap-heavy STL
- SPDX-License-Identifier: MIT in every source header

## Testing

- All wheels must have host-simulation tests (no hardware required)
- Property tests (`property_test.h`), null-safety tests, fuzz harnesses where applicable
- Run individual wheel: `bash tooling/test/test_all.sh wheels/<layer>/<name>`
- Run everything: `make test-parallel`

## License

MIT. See `LICENSE`.
