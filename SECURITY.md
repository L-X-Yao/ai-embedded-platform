# Security policy

## Reporting a vulnerability

Please report security problems privately, not in a public issue. On GitHub, open the **Security** tab of this repository and choose **Report a vulnerability**, or go to <https://github.com/L-X-Yao/ai-embedded-platform/security/advisories/new>. Only you and the maintainer can see the report.

A useful report names the component (for example `wheels/protocols/mqtt_packet`), the tag or commit you used, and an input or a test that shows the problem.

## Scope

Code under `wheels/`, `examples/`, `tooling/` and `vsf-addon/`. The parsers in `wheels/protocols/` read untrusted input and matter most. `vsf/` is an unmodified upstream snapshot of [VSF](https://github.com/vsfteam/vsf); problems in it belong upstream.

## What to expect

The project has been in maintenance since 2026-11-10 and has one maintainer, so there is no fixed response time. Fixes go to `main`; older releases get no backports. The components have host tests, sanitizers and fuzzing (see the README), but no external security audit, and most drivers have never run on hardware.
