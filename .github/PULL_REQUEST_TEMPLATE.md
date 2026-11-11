## Summary

<!-- 1-3 bullet points: what changed and why -->

## Type

<!-- Check one -->
- [ ] `wheel` — wheel code or structure change
- [ ] `docs` — documentation
- [ ] `tool` — tooling scripts
- [ ] `ci` — CI/CD configuration
- [ ] `example` — integration example under `examples/`
- [ ] `port` — HAL driver port under `vsf-addon/`
- [ ] `exp` — experiment under `tooling/experiments/`

## Checklist

- [ ] `make audit` (or `python tooling/audit/audit_wheels.py`) passes with 0 failures
- [ ] `make test-wheels` (or `bash tooling/test/test_all.sh --wheel`) passes
- [ ] Commit messages follow CLAUDE.md convention: `<type>(<scope>): <lowercase verb> ...`
- [ ] New/modified wheels have updated README test counts
- [ ] New wheels have: file header, extern "C", @thread_safe, NULL guards, Origin section

## Test plan

<!-- How was this tested? Which wheels or examples are affected? -->
