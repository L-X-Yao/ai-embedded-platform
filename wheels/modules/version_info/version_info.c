// SPDX-License-Identifier: MIT
/*
 * version_info — semantic version comparison and compatibility
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "version_info.h"
#include "contract.h"
#include <stdio.h>

/* --------------- helpers --------------- */

/** Parse an unsigned decimal from *p, advance *p past digits.
 *  Returns -1 on error (no digits, overflow). */
static int parse_u16(const char **p, uint16_t *out)
{
    const char *s = *p;
    if (*s < '0' || *s > '9') return -1;

    unsigned long val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (unsigned long)(*s - '0');
        if (val > 65535) return -1;
        s++;
    }
    *out = (uint16_t)val;
    *p = s;
    return 0;
}

/* --------------- public API --------------- */

version_info_err_t version_parse(const char *str, version_t *v)
{
    if (!str || !v) return VERSION_INFO_ERR_PARAM;

    const char *p = str;
    version_t tmp = {0, 0, 0};

    if (parse_u16(&p, &tmp.major) < 0) return VERSION_INFO_ERR_FMT;
    if (*p++ != '.')                    return VERSION_INFO_ERR_FMT;
    if (parse_u16(&p, &tmp.minor) < 0) return VERSION_INFO_ERR_FMT;
    if (*p++ != '.')                    return VERSION_INFO_ERR_FMT;
    if (parse_u16(&p, &tmp.patch) < 0) return VERSION_INFO_ERR_FMT;
    if (*p != '\0')                     return VERSION_INFO_ERR_FMT;

    *v = tmp;
    return VERSION_INFO_OK;
}

int version_compare(const version_t *a, const version_t *b)
{
    if (!a || !b) return 0;

    if (a->major != b->major) return a->major < b->major ? -1 : 1;
    if (a->minor != b->minor) return a->minor < b->minor ? -1 : 1;
    if (a->patch != b->patch) return a->patch < b->patch ? -1 : 1;
    return 0;
}

bool version_compatible(const version_t *required, const version_t *actual)
{
    if (!required || !actual) return false;
    if (actual->major != required->major) return false;
    return version_compare(actual, required) >= 0;
}

int version_to_string(const version_t *v, char *buf, size_t buf_size)
{
    if (!v || !buf || buf_size == 0) return -1;

    int n = snprintf(buf, buf_size, "%u.%u.%u",
                     (unsigned)v->major, (unsigned)v->minor, (unsigned)v->patch);
    if (n < 0 || (size_t)n >= buf_size) return -1;
    return n;
}

bool version_is_valid(const version_t *v)
{
    if (!v) return false;
    return v->major <= 9999 && v->minor <= 9999 && v->patch <= 9999;
}
