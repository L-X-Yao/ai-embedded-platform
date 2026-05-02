// SPDX-License-Identifier: MIT
/*
 * version_info — semantic version comparison and compatibility
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef VERSION_INFO_H
#define VERSION_INFO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VERSION_INFO_OK        =  0,
    VERSION_INFO_ERR_PARAM = -1,
    VERSION_INFO_ERR_FMT   = -2,
} version_info_err_t;

/** Semantic version triplet. */
typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} version_t;

/** Parse "major.minor.patch" into *v.  @thread_safe: yes (pure) */
version_info_err_t version_parse(const char *str, version_t *v);

/** Compare a and b: return -1 if a<b, 0 if equal, 1 if a>b.
 *  @thread_safe: yes (pure) */
int version_compare(const version_t *a, const version_t *b);

/** Check if actual is compatible with required (same major, actual >= required).
 *  @thread_safe: yes (pure) */
bool version_compatible(const version_t *required, const version_t *actual);

/** Write "major.minor.patch" into buf.  Returns bytes written (excl NUL), or -1
 *  on error.  @thread_safe: yes (pure) */
int version_to_string(const version_t *v, char *buf, size_t buf_size);

/** Return true if the version fields are non-degenerate (major <= 9999 etc).
 *  A zero-init version (0.0.0) is considered valid.
 *  @thread_safe: yes (pure) */
bool version_is_valid(const version_t *v);

#ifdef __cplusplus
}
#endif

#endif /* VERSION_INFO_H */
