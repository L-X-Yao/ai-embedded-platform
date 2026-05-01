# printf_tiny — minimal printf for resource-constrained embedded systems

**Status: candidate**
**Layer: L0 (primitives)**

Lightweight printf implementation supporting `%d %u %x %X %o %c %s %p %%` with
width, zero-pad, left-align, and long modifier. No float, no malloc, no static
buffers. Output via putc callback (streaming to UART, display, etc.) or to a
sized buffer (snprintf-style).

## Why not VSF native?

VSF does not provide a minimal printf variant. The standard `vsf_trace` relies
on the full libc printf, which pulls in float formatting and adds significant
code size. This wheel targets sub-200-line implementations for MCUs where every
byte of Flash matters.

## Origin

Proactive build for resource-constrained MCU projects needing formatted output
without libc printf overhead.

## Dependencies

None.

## API

```c
typedef void (*printf_tiny_putc_t)(void *ctx, char c);

/* Format to output callback */
int printf_tiny(printf_tiny_putc_t putc, void *ctx, const char *fmt, ...);

/* Format to buffer (like snprintf) */
int snprintf_tiny(char *buf, uint16_t size, const char *fmt, ...);

/* va_list variants */
int vprintf_tiny(printf_tiny_putc_t putc, void *ctx, const char *fmt, va_list ap);
int vsnprintf_tiny(char *buf, uint16_t size, const char *fmt, va_list ap);
```

### Supported specifiers

| Specifier | Description |
|-----------|-------------|
| `%d` / `%i` | Signed decimal int |
| `%u` | Unsigned decimal int |
| `%x` / `%X` | Hex (lower / upper) |
| `%o` | Octal |
| `%c` | Character |
| `%s` | String (NULL prints "(null)") |
| `%p` | Pointer as `0xHEX` |
| `%%` | Literal percent |

### Modifiers

| Modifier | Example | Description |
|----------|---------|-------------|
| Width | `%5d` | Right-aligned, space-padded |
| Left-align | `%-10s` | Left-aligned, space-padded |
| Zero-pad | `%04x` | Zero-padded |
| Long | `%ld`, `%lu` | `long` / `unsigned long` argument |

### Not supported (by design)

- Float (`%f`, `%e`, `%g`) — too much code for constrained MCUs
- `%n` — security risk
- Precision (`%.3s`) — rarely needed on embedded

## Configuration (compile-time)

No compile-time configuration needed. The wheel is always fully functional.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/printf_tiny/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_printf_tiny
```

## Test status

- Unit tests (sim): 29 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: putc callback for streaming output; snprintf for buffer
  mode; no float to keep code tiny; returns character count like standard printf.

## Responsible human

@user
