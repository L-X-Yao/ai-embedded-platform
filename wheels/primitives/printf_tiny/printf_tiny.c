/*
 * printf_tiny — minimal printf for resource-constrained embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "printf_tiny.h"
#include <stddef.h>

/* --- internal output abstraction ----------------------------------- */

typedef struct {
    printf_tiny_putc_t putc;    /* callback mode */
    void              *ctx;
    char              *buf;     /* buffer mode */
    uint16_t           size;
    int                pos;     /* characters written (logical) */
} out_t;

static void out_char(out_t *o, char c)
{
    if (o->putc) {
        o->putc(o->ctx, c);
    } else if (o->buf) {
        if (o->pos < (int)(o->size - 1u)) {
            o->buf[o->pos] = c;
        }
    }
    o->pos++;
}

static void out_str(out_t *o, const char *s)
{
    while (*s) {
        out_char(o, *s++);
    }
}

/* --- number formatting --------------------------------------------- */

static void out_uint(out_t *o, unsigned long val, int base, int upper,
                     int width, int zero_pad, int left_align)
{
    char tmp[22];   /* enough for 64-bit decimal on common platforms */
    int len = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0) {
        tmp[len++] = '0';
    } else {
        while (val) {
            tmp[len++] = digits[val % (unsigned long)base];
            val /= (unsigned long)base;
        }
    }

    /* pad calculation */
    int pad = (width > len) ? width - len : 0;

    if (!left_align) {
        char pc = zero_pad ? '0' : ' ';
        for (int i = 0; i < pad; i++) out_char(o, pc);
    }
    /* digits in reverse */
    for (int i = len - 1; i >= 0; i--) out_char(o, tmp[i]);
    if (left_align) {
        for (int i = 0; i < pad; i++) out_char(o, ' ');
    }
}

static void out_int(out_t *o, long val, int width, int zero_pad,
                    int left_align)
{
    unsigned long uval;
    int neg = 0;

    if (val < 0) {
        neg = 1;
        /* handle LONG_MIN safely */
        uval = (unsigned long)(-(val + 1)) + 1u;
    } else {
        uval = (unsigned long)val;
    }

    char tmp[22];
    int len = 0;

    if (uval == 0) {
        tmp[len++] = '0';
    } else {
        while (uval) {
            tmp[len++] = (char)('0' + (int)(uval % 10u));
            uval /= 10u;
        }
    }

    int total = len + neg;
    int pad = (width > total) ? width - total : 0;

    if (!left_align) {
        if (zero_pad) {
            if (neg) out_char(o, '-');
            for (int i = 0; i < pad; i++) out_char(o, '0');
        } else {
            for (int i = 0; i < pad; i++) out_char(o, ' ');
            if (neg) out_char(o, '-');
        }
    } else {
        if (neg) out_char(o, '-');
    }

    for (int i = len - 1; i >= 0; i--) out_char(o, tmp[i]);

    if (left_align) {
        for (int i = 0; i < pad; i++) out_char(o, ' ');
    }
}

/* --- core engine --------------------------------------------------- */

static int format_engine(out_t *o, const char *fmt, va_list ap)
{
    while (*fmt) {
        if (*fmt != '%') {
            out_char(o, *fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        /* flags */
        int left_align = 0;
        int zero_pad = 0;

        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left_align = 1;
            if (*fmt == '0') zero_pad = 1;
            fmt++;
        }
        /* left-align overrides zero-pad */
        if (left_align) zero_pad = 0;

        /* width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* long modifier */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
        }

        /* specifier */
        switch (*fmt) {
        case 'd':
        case 'i': {
            long v = is_long ? va_arg(ap, long) : (long)va_arg(ap, int);
            out_int(o, v, width, zero_pad, left_align);
            break;
        }
        case 'u': {
            unsigned long v = is_long ? va_arg(ap, unsigned long)
                                      : (unsigned long)va_arg(ap, unsigned int);
            out_uint(o, v, 10, 0, width, zero_pad, left_align);
            break;
        }
        case 'x': {
            unsigned long v = is_long ? va_arg(ap, unsigned long)
                                      : (unsigned long)va_arg(ap, unsigned int);
            out_uint(o, v, 16, 0, width, zero_pad, left_align);
            break;
        }
        case 'X': {
            unsigned long v = is_long ? va_arg(ap, unsigned long)
                                      : (unsigned long)va_arg(ap, unsigned int);
            out_uint(o, v, 16, 1, width, zero_pad, left_align);
            break;
        }
        case 'o': {
            unsigned long v = is_long ? va_arg(ap, unsigned long)
                                      : (unsigned long)va_arg(ap, unsigned int);
            out_uint(o, v, 8, 0, width, zero_pad, left_align);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            int pad = (width > 1) ? width - 1 : 0;
            if (!left_align) {
                for (int i = 0; i < pad; i++) out_char(o, ' ');
            }
            out_char(o, c);
            if (left_align) {
                for (int i = 0; i < pad; i++) out_char(o, ' ');
            }
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int slen = 0;
            const char *p = s;
            while (*p++) slen++;
            int pad = (width > slen) ? width - slen : 0;
            if (!left_align) {
                for (int i = 0; i < pad; i++) out_char(o, ' ');
            }
            out_str(o, s);
            if (left_align) {
                for (int i = 0; i < pad; i++) out_char(o, ' ');
            }
            break;
        }
        case 'p': {
            void *ptr = va_arg(ap, void *);
            out_str(o, "0x");
            out_uint(o, (unsigned long)(uintptr_t)ptr, 16, 0,
                     0, 0, 0);
            break;
        }
        case '%':
            out_char(o, '%');
            break;
        case '\0':
            /* trailing '%' at end of format string */
            goto done;
        default:
            /* unknown specifier — emit as-is */
            out_char(o, '%');
            out_char(o, *fmt);
            break;
        }
        fmt++;
    }
done:
    return o->pos;
}

/* --- public API ---------------------------------------------------- */

int vprintf_tiny(printf_tiny_putc_t putc, void *ctx,
                 const char *fmt, va_list ap)
{
    if (!putc || !fmt) return 0;
    out_t o = { putc, ctx, NULL, 0, 0 };
    return format_engine(&o, fmt, ap);
}

int vsnprintf_tiny(char *buf, uint16_t size, const char *fmt, va_list ap)
{
    if (!fmt) return 0;
    out_t o = { NULL, NULL, buf, size, 0 };
    int ret = format_engine(&o, fmt, ap);
    /* NUL-terminate */
    if (buf) {
        if (size > 0) {
            int term = (o.pos < (int)size) ? o.pos : (int)(size - 1u);
            buf[term] = '\0';
        }
    }
    return ret;
}

int printf_tiny(printf_tiny_putc_t putc, void *ctx,
                const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintf_tiny(putc, ctx, fmt, ap);
    va_end(ap);
    return ret;
}

int snprintf_tiny(char *buf, uint16_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf_tiny(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}
