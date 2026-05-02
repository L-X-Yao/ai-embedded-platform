# string_utils — embedded-friendly string utilities

**Status: candidate**
**Layer: L0 (primitives)**

Common string operations for embedded systems: trim, split, join, prefix/suffix
checks, and numeric parsing. No heap allocation, no stdlib dependency beyond
`string.h`. All functions operate on caller-owned buffers.
## Origin

Proactive build for AT command parsing, CLI input handling, and config
key-value processing across multiple projects.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
/* Trim whitespace */
string_utils_err_t str_trim(const char *in, char *out, size_t size);
string_utils_err_t str_ltrim(const char *in, char *out, size_t size);
string_utils_err_t str_rtrim(const char *in, char *out, size_t size);

/* Split by delimiter (modifies input in-place) */
int str_split(char *str, char delim, char **tokens, int max_tokens);

/* Prefix / suffix checks */
bool str_starts_with(const char *str, const char *prefix);
bool str_ends_with(const char *str, const char *suffix);

/* Numeric parsing (no stdlib atoi/strtol) */
string_utils_err_t str_to_int(const char *str, int32_t *val);
string_utils_err_t str_to_float(const char *str, float *val);

/* Join tokens with delimiter */
string_utils_err_t str_join(const char **tokens, int count, const char *delim,
                            char *buf, size_t size);
```

## Configuration (compile-time)

None. No compile-time defines needed.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified (pure C99, expected portable)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/string_utils/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_string_utils
```

## Test status

- Unit tests (sim): 16 tests, all passing
- HIL: N/A (L0 pure software)

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: No heap allocation; str_split modifies input in-place
  for zero-copy; str_to_float uses iterative parsing (no strtof dependency).

## Responsible human

@user
