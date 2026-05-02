// SPDX-License-Identifier: MIT
/*
 * fixed_point — Q16.16 fixed-point math library
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "fixed_point.h"

/* ── Conversion ───────────────────────────────────────────────── */

fp_t fp_from_int(int16_t val)
{
    return (fp_t)val << FP_FRAC_BITS;
}

fp_t fp_from_float(float val)
{
    /* reject NaN — cast of NaN to integer is undefined behavior */
    if (val != val) return 0;
    return (fp_t)(val * (float)FP_ONE);
}

int16_t fp_to_int(fp_t val)
{
    /* Truncate toward zero */
    if (val >= 0) {
        return (int16_t)(val >> FP_FRAC_BITS);
    }
    return (int16_t)(-(-val >> FP_FRAC_BITS));
}

int16_t fp_to_int_rounded(fp_t val)
{
    if (val >= 0) {
        return (int16_t)((val + FP_HALF) >> FP_FRAC_BITS);
    }
    return (int16_t)(-((-val + FP_HALF) >> FP_FRAC_BITS));
}

float fp_to_float(fp_t val)
{
    return (float)val / (float)FP_ONE;
}

/* ── Arithmetic ───────────────────────────────────────────────── */

fp_t fp_add(fp_t a, fp_t b)
{
    int64_t r = (int64_t)a + (int64_t)b;
    if (r > INT32_MAX) return FP_MAX;
    if (r < INT32_MIN) return FP_MIN;
    return (fp_t)r;
}

fp_t fp_sub(fp_t a, fp_t b)
{
    int64_t r = (int64_t)a - (int64_t)b;
    if (r > INT32_MAX) return FP_MAX;
    if (r < INT32_MIN) return FP_MIN;
    return (fp_t)r;
}

fp_t fp_mul(fp_t a, fp_t b)
{
    int64_t result = (int64_t)a * (int64_t)b;
    return (fp_t)(result >> FP_FRAC_BITS);
}

fp_t fp_div(fp_t a, fp_t b)
{
    if (b == 0) {
        return (a >= 0) ? FP_MAX : FP_MIN;
    }
    int64_t numerator = (int64_t)a << FP_FRAC_BITS;
    return (fp_t)(numerator / (int64_t)b);
}

/* ── Comparison ───────────────────────────────────────────────── */

bool fp_gt(fp_t a, fp_t b) { return a > b; }
bool fp_lt(fp_t a, fp_t b) { return a < b; }
bool fp_eq(fp_t a, fp_t b) { return a == b; }

/* ── Math functions ───────────────────────────────────────────── */

fp_t fp_abs(fp_t val)
{
    /* Guard against INT32_MIN (two's complement asymmetry) */
    if (val == FP_MIN) return FP_MAX;
    return (val < 0) ? -val : val;
}

fp_t fp_min(fp_t a, fp_t b) { return (a < b) ? a : b; }
fp_t fp_max(fp_t a, fp_t b) { return (a > b) ? a : b; }

fp_t fp_clamp(fp_t val, fp_t lo, fp_t hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

fp_t fp_lerp(fp_t a, fp_t b, fp_t t)
{
    /* a + t * (b - a), using int64_t to avoid overflow */
    int64_t diff = (int64_t)b - (int64_t)a;
    int64_t scaled = (diff * (int64_t)t) >> FP_FRAC_BITS;
    return (fp_t)((int64_t)a + scaled);
}

/* ── String formatting ────────────────────────────────────────── */

int fp_to_string(fp_t val, char *buf, uint16_t buf_size, uint8_t decimal_places)
{
    if (!buf || buf_size == 0) return -1;
    if (decimal_places > 5) decimal_places = 5;

    /* Powers of 10 for fractional digit extraction */
    static const uint32_t pow10[6] = {
        1, 10, 100, 1000, 10000, 100000
    };

    int pos = 0;
    uint16_t remaining = buf_size;
    fp_t work = val;

    /* Sign — handle FP_MIN (INT32_MIN) specially to avoid -INT32_MIN UB */
    if (work < 0) {
        if (remaining < 2) { buf[0] = '\0'; return -1; }
        buf[pos++] = '-';
        remaining--;
        if (work == (fp_t)INT32_MIN) {
            work = INT32_MAX; /* off by 1 LSB, acceptable for formatting */
        } else {
            work = -work;
        }
    }

    /* Integer part */
    uint16_t integer_part = (uint16_t)((uint32_t)work >> FP_FRAC_BITS);
    uint16_t frac_raw     = (uint16_t)((uint32_t)work & FP_FRAC_MASK);

    /* Convert integer to string (reverse then flip) */
    char tmp[8];
    int  tlen = 0;

    if (integer_part == 0) {
        tmp[tlen++] = '0';
    } else {
        uint16_t n = integer_part;
        while (n > 0) {
            tmp[tlen++] = (char)('0' + (n % 10));
            n /= 10;
        }
    }

    /* Check space: digits + '.' + decimal_places + NUL */
    uint16_t need = (uint16_t)(tlen + (decimal_places > 0 ? 1 : 0)
                    + decimal_places + 1);
    if (remaining < need) { buf[0] = '\0'; return -1; }

    /* Write integer digits (reversed) */
    for (int i = tlen - 1; i >= 0; i--) {
        buf[pos++] = tmp[i];
    }

    /* Fractional part */
    if (decimal_places > 0) {
        buf[pos++] = '.';

        /* Scale fractional bits to decimal digits:
         * frac_decimal = frac_raw * pow10[decimal_places] / 65536
         * Use uint32_t to avoid overflow. */
        uint32_t frac_scaled = ((uint32_t)frac_raw * pow10[decimal_places]
                                + 32768u) / 65536u;

        /* Clamp in case rounding pushes over (e.g. 0.99999 rounds up) */
        if (frac_scaled >= pow10[decimal_places]) {
            frac_scaled = pow10[decimal_places] - 1u;
        }

        /* Write fractional digits with leading zeros */
        for (int i = decimal_places - 1; i >= 0; i--) {
            buf[pos + i] = (char)('0' + (char)(frac_scaled % 10));
            frac_scaled /= 10;
        }
        pos += decimal_places;
    }

    buf[pos] = '\0';
    return pos;
}
