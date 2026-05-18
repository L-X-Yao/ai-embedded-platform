// SPDX-License-Identifier: MIT
/*
 * string_utils — embedded-friendly string utilities (no stdlib beyond string.h)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * All functions operate on caller-owned buffers — no heap allocation.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STRING_UTILS_OK        =  0,
    STRING_UTILS_ERR_PARAM = -1,
    STRING_UTILS_ERR_RANGE = -2,
    STRING_UTILS_ERR_FMT   = -3,
    STRING_UTILS_ERR_OVERFLOW = -4,
} string_utils_err_t;

/**
 * Trim leading and trailing whitespace. Result written to out.
 * @param in   NUL-terminated input string.
 * @param out  Output buffer (may alias in for in-place trim).
 * @param size Size of out buffer including NUL terminator.
 * @return STRING_UTILS_OK on success, STRING_UTILS_ERR_PARAM on NULL.
 * @thread_safe: yes (pure function, caller-owned buffers) @isr_safe: yes
 */
string_utils_err_t str_trim(const char *in, char *out, size_t size);

/**
 * Trim leading whitespace only.
 * @thread_safe: yes @isr_safe: yes
 */
string_utils_err_t str_ltrim(const char *in, char *out, size_t size);

/**
 * Trim trailing whitespace only.
 * @thread_safe: yes @isr_safe: yes
 */
string_utils_err_t str_rtrim(const char *in, char *out, size_t size);

/**
 * Split string by single-character delimiter.
 * Writes pointers into a caller-provided token array. The input string
 * is modified in-place (delimiters replaced with NUL).
 *
 * @param str        Mutable NUL-terminated string (modified in-place).
 * @param delim      Delimiter character.
 * @param tokens     Output array of char* pointers.
 * @param max_tokens Capacity of tokens array.
 * @return Number of tokens found (0 on NULL input, capped at max_tokens).
 * @thread_safe: yes (caller-owned buffers) @isr_safe: yes
 */
int str_split(char *str, char delim, char **tokens, int max_tokens);

/**
 * Check if string starts with the given prefix.
 * @return true if str starts with prefix; false on NULL or mismatch.
 * @thread_safe: yes @isr_safe: yes
 */
bool str_starts_with(const char *str, const char *prefix);

/**
 * Check if string ends with the given suffix.
 * @return true if str ends with suffix; false on NULL or mismatch.
 * @thread_safe: yes @isr_safe: yes
 */
bool str_ends_with(const char *str, const char *suffix);

/**
 * Parse decimal integer from string (handles optional leading +/-).
 * @param str  NUL-terminated decimal string.
 * @param val  Output value.
 * @return STRING_UTILS_OK on success, STRING_UTILS_ERR_FMT if not a valid
 *         integer, STRING_UTILS_ERR_PARAM on NULL.
 * @thread_safe: yes @isr_safe: yes
 */
string_utils_err_t str_to_int(const char *str, int32_t *val);

/**
 * Parse floating-point number from string (handles optional sign, integer
 * part, decimal point, fractional part).
 * @param str  NUL-terminated numeric string.
 * @param val  Output value.
 * @return STRING_UTILS_OK on success, STRING_UTILS_ERR_FMT if not valid,
 *         STRING_UTILS_ERR_PARAM on NULL.
 * @thread_safe: yes @isr_safe: yes
 */
string_utils_err_t str_to_float(const char *str, float *val);

/**
 * Join an array of strings with a delimiter into a buffer.
 * @param tokens Array of NUL-terminated strings.
 * @param count  Number of tokens.
 * @param delim  Delimiter string inserted between tokens.
 * @param buf    Output buffer.
 * @param size   Size of output buffer including NUL.
 * @return STRING_UTILS_OK on success, STRING_UTILS_ERR_OVERFLOW if buffer
 *         too small (output truncated with NUL), STRING_UTILS_ERR_PARAM on NULL.
 * @thread_safe: yes @isr_safe: yes
 */
string_utils_err_t str_join(const char **tokens, int count, const char *delim,
                            char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* STRING_UTILS_H */
