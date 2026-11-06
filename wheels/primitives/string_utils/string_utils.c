// SPDX-License-Identifier: MIT
/*
 * string_utils — embedded-friendly string utilities (no stdlib beyond string.h)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "string_utils.h"
#include "contract.h"
#include <float.h>
#include <string.h>

/* ── helpers ────────────────────────────────────────────────────────── */

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static size_t safe_copy(const char *src, size_t src_len, char *dst, size_t size)
{
    if (size == 0) return 0;
    size_t n = src_len < size - 1 ? src_len : size - 1;
    if (n > 0) memmove(dst, src, n);  /* memmove for possible overlap */
    dst[n] = '\0';
    ENSURE(dst[n] == '\0');
    return n;
}

/* ── trim ───────────────────────────────────────────────────────────── */

string_utils_err_t str_ltrim(const char *in, char *out, size_t size)
{
    if (!in || !out || size == 0) return STRING_UTILS_ERR_PARAM;
    while (*in && is_space(*in)) in++;
    safe_copy(in, strlen(in), out, size);
    return STRING_UTILS_OK;
}

string_utils_err_t str_rtrim(const char *in, char *out, size_t size)
{
    if (!in || !out || size == 0) return STRING_UTILS_ERR_PARAM;
    size_t len = strlen(in);
    while (len > 0 && is_space(in[len - 1])) len--;
    safe_copy(in, len, out, size);
    return STRING_UTILS_OK;
}

string_utils_err_t str_trim(const char *in, char *out, size_t size)
{
    if (!in || !out || size == 0) return STRING_UTILS_ERR_PARAM;
    while (*in && is_space(*in)) in++;
    size_t len = strlen(in);
    while (len > 0 && is_space(in[len - 1])) len--;
    safe_copy(in, len, out, size);
    return STRING_UTILS_OK;
}

/* ── split ──────────────────────────────────────────────────────────── */

int str_split(char *str, char delim, char **tokens, int max_tokens)
{
    if (!str || !tokens || max_tokens <= 0) return 0;

    int count = 0;
    tokens[count++] = str;

    for (char *p = str; *p; p++) {
        if (*p == delim) {
            *p = '\0';
            if (count < max_tokens) {
                tokens[count++] = p + 1;
            }
        }
    }
    return count;
}

/* ── prefix / suffix ────────────────────────────────────────────────── */

bool str_starts_with(const char *str, const char *prefix)
{
    if (!str || !prefix) return false;
    size_t plen = strlen(prefix);
    return strncmp(str, prefix, plen) == 0;
}

bool str_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) return false;
    size_t slen = strlen(str);
    size_t xlen = strlen(suffix);
    if (xlen > slen) return false;
    return strcmp(str + slen - xlen, suffix) == 0;
}

/* ── parsing ────────────────────────────────────────────────────────── */

string_utils_err_t str_to_int(const char *str, int32_t *val)
{
    if (!str || !val) return STRING_UTILS_ERR_PARAM;
    if (*str == '\0') return STRING_UTILS_ERR_FMT;

    const char *p = str;
    bool neg = false;
    if (*p == '-')      { neg = true; p++; }
    else if (*p == '+') { p++; }

    if (*p == '\0') return STRING_UTILS_ERR_FMT;

    /* Accumulate as negative to handle INT32_MIN without overflow.
     * INT32_MIN (-2147483648) has no positive int32_t counterpart. */
    int32_t result = 0;
    int32_t limit  = neg ? INT32_MIN : -INT32_MAX;
    while (*p) {
        if (*p < '0' || *p > '9') return STRING_UTILS_ERR_FMT;
        int digit = *p - '0';
        /* overflow guard (working in negative domain) */
        if (result < (limit + digit) / 10) return STRING_UTILS_ERR_RANGE;
        result = result * 10 - digit;
        p++;
    }

    *val = neg ? result : -result;
    return STRING_UTILS_OK;
}

string_utils_err_t str_to_float(const char *str, float *val)
{
    if (!str || !val) return STRING_UTILS_ERR_PARAM;
    if (*str == '\0') return STRING_UTILS_ERR_FMT;

    const char *p = str;
    bool neg = false;
    if (*p == '-')      { neg = true; p++; }
    else if (*p == '+') { p++; }

    if (*p == '\0') return STRING_UTILS_ERR_FMT;

    bool has_digit = false;
    float result = 0.0f;

    /* integer part */
    while (*p >= '0' && *p <= '9') {
        result = result * 10.0f + (float)(*p - '0');
        has_digit = true;
        p++;
    }

    /* fractional part */
    if (*p == '.') {
        p++;
        float frac = 0.1f;
        while (*p >= '0' && *p <= '9') {
            result += (float)(*p - '0') * frac;
            frac *= 0.1f;
            has_digit = true;
            p++;
        }
    }

    if (!has_digit || *p != '\0') return STRING_UTILS_ERR_FMT;

    /* Too many digits run result up to +inf */
    if (result > FLT_MAX) return STRING_UTILS_ERR_RANGE;

    *val = neg ? -result : result;
    return STRING_UTILS_OK;
}

/* ── join ───────────────────────────────────────────────────────────── */

string_utils_err_t str_join(const char **tokens, int count, const char *delim,
                            char *buf, size_t size)
{
    if (!tokens || !delim || !buf || size == 0) return STRING_UTILS_ERR_PARAM;
    if (count < 0) return STRING_UTILS_ERR_PARAM;

    buf[0] = '\0';
    size_t pos = 0;
    size_t dlen = strlen(delim);
    bool overflow = false;

    for (int i = 0; i < count; i++) {
        if (!tokens[i]) return STRING_UTILS_ERR_PARAM;
        /* delimiter between tokens */
        if (i > 0 && dlen > 0) {
            size_t avail = pos < size - 1 ? size - 1 - pos : 0;
            size_t n = dlen < avail ? dlen : avail;
            if (n > 0) memcpy(buf + pos, delim, n);
            pos += dlen;
            if (pos >= size) overflow = true;
        }
        size_t tlen = strlen(tokens[i]);
        {
            size_t cap = pos < size - 1 ? size - 1 - pos : 0;
            size_t n = tlen < cap ? tlen : cap;
            if (n > 0) memcpy(buf + pos, tokens[i], n);
        }
        pos += tlen;
        if (pos >= size) overflow = true;
    }
    /* NUL-terminate at actual end or buffer limit */
    buf[pos < size ? pos : size - 1] = '\0';
    ENSURE(buf[pos < size ? pos : size - 1] == '\0');
    return overflow ? STRING_UTILS_ERR_OVERFLOW : STRING_UTILS_OK;
}
