# Git Hooks

Project-specific git hooks for enforcing commit conventions.

## Install

```bash
git config core.hooksPath .githooks
```

## Hooks

| Hook | Purpose |
|------|---------|
| `commit-msg` | Validates commit message format (type/scope/summary per CLAUDE.md) |
| `pre-commit` | Blocks accidental binary commits (rejects files > 100KB non-text) |
| `pre-push` | Runs `make ci-local` gate before push (tests + audit + doc-drift) |

## What it checks

- Type is from allowed set: `docs`, `wheel`, `exp`, `port`, `project`, `tool`, `example`, `ci`
- Summary starts with lowercase imperative verb
- No trailing period on summary
- Summary line <= 70 chars
- No `Co-Authored-By` line (project convention)
