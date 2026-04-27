# project-integration template

Project integration scaffold for bringing an existing embedded project into
ai-embedded-platform. Not a code template — this generates the platform
metadata and directory structure that lets AI collaborate effectively.

## When to use

When the user says "integrate project X" or starts `ai-workflow.md` task type 2.
The project already has working source code; this template provides the
platform-level scaffolding around it.

## How to use

```bash
# 1. Create project directory
mkdir -p projects/<name>/

# 2. Copy CLAUDE.md template and fill in
cp templates/project-integration/CLAUDE.md.template projects/<name>/CLAUDE.md
# Edit: replace all {{placeholders}} with actual values

# 3. Copy project source code into place
# Typical structure:
#   projects/<name>/
#   ├── CLAUDE.md              (from this template)
#   ├── src/
#   │   ├── bsp/               board support (clocks, pins, interrupts)
#   │   ├── drivers/            device drivers (sensors, comms, storage)
#   │   ├── app/                application tasks / main loop
#   │   └── common/             shared utilities
#   ├── docs/                   project-specific docs (optional)
#   ├── third_party/            vendor SDK headers if needed
#   └── test/                   host-side unit tests (optional)

# 4. Sync wheel candidates to WISHLIST
# For each candidate in CLAUDE.md's wheel table, ensure it appears in
# wheels/WISHLIST.md with trigger count incremented.
# See: memory/feedback_project_import_wishlist_sync.md

# 5. Add CI job if project supports cross-compile
# See .github/workflows/ci.yml for flybox-build as reference.
```

## What this template does NOT include

- Source code (project brings its own)
- Build system (varies: Keil / CMake / Make)
- VSF integration code (depends on migration mode chosen)
- Test harnesses (project-specific)

## Reference projects

- `projects/flybox/` — full integration: Cortex-M4, FreeRTOS, VSF compat layer, 5 sunk wheels, CI cross-compile
- `projects/drone_light/` — lightweight integration: Cortex-M0, bare metal, no VSF migration, 2 wheel candidates marked

## Derived from

Common structure extracted from flybox and drone_light CLAUDE.md files (2026-04-27).
