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

## What it checks

- Type is from allowed set: `docs`, `wheel`, `exp`, `port`, `project`, `tool`, `ci`
- Summary starts with lowercase imperative verb
- No trailing period on summary
- Summary line <= 70 chars
- No `Co-Authored-By` line (project convention)
