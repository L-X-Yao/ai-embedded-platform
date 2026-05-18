// SPDX-License-Identifier: MIT
/*
 * prop_string_utils.c — property-based tests for string_utils
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_string_utils.c ../string_utils.c -o prop_string_utils
 */

#include "test_harness.h"
#include "property_test.h"
#include "../string_utils.h"
#include <string.h>

/* Property: no-crash on arbitrary byte strings for trim functions */
PROPERTY(no_crash_trim, 50000) {
    char input[64];
    size_t len = ARBI_SIZE(seed, 62) + 1;
    ARBI_FILL(seed, (uint8_t *)input, len);
    input[len] = '\0';

    char out[64];
    (void)str_trim(input, out, sizeof(out));
    (void)str_ltrim(input, out, sizeof(out));
    (void)str_rtrim(input, out, sizeof(out));

    PROP_ASSERT(1); /* reached without crash */
}

/* Property: trim output is always null-terminated */
PROPERTY(trim_null_terminated, 10000) {
    char input[32];
    size_t len = ARBI_SIZE(seed, 30) + 1;
    ARBI_FILL(seed, (uint8_t *)input, len);
    input[len] = '\0';

    char out[32];
    memset(out, 0xFF, sizeof(out));
    string_utils_err_t err = str_trim(input, out, sizeof(out));

    if (err == STRING_UTILS_OK) {
        /* Output must be null-terminated somewhere within buffer */
        bool found_nul = false;
        for (size_t i = 0; i < sizeof(out); i++) {
            if (out[i] == '\0') { found_nul = true; break; }
        }
        PROP_ASSERT(found_nul);
    }
}

/* Property: str_starts_with is prefix-consistent */
PROPERTY(starts_with_prefix, 10000) {
    char str[32];
    size_t slen = ARBI_SIZE(seed, 30) + 1;
    ARBI_FILL(seed, (uint8_t *)str, slen);
    str[slen] = '\0';

    /* Use first N bytes as prefix — should always return true */
    size_t plen = ARBI_SIZE(seed, slen);
    char prefix[32];
    memcpy(prefix, str, plen);
    prefix[plen] = '\0';

    PROP_ASSERT(str_starts_with(str, prefix));
}

/* Property: str_split token count <= max_tokens */
PROPERTY(split_bounded_tokens, 10000) {
    char input[64];
    size_t len = ARBI_SIZE(seed, 62) + 1;
    ARBI_FILL(seed, (uint8_t *)input, len);
    input[len] = '\0';

    /* Ensure no embedded NULs before the end */
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\0') input[i] = 'a';
    }
    input[len] = '\0';

    int max_tokens = ARBI_RANGE(seed, 1, 10);
    char *tokens[10];
    char delim = (char)ARBI_RANGE(seed, 32, 126);

    int count = str_split(input, delim, tokens, max_tokens);
    PROP_ASSERT(count >= 0);
    PROP_ASSERT(count <= max_tokens);
}

/* Property: str_to_int roundtrip for valid integers */
PROPERTY(to_int_deterministic, 5000) {
    int32_t val1 = 0, val2 = 0;
    /* Build a simple integer string */
    int num = ARBI_RANGE(seed, -99999, 99999);
    char buf[16];
    int pos = 0;
    if (num < 0) { buf[pos++] = '-'; num = -num; }
    if (num == 0) {
        buf[pos++] = '0';
    } else {
        char tmp[12];
        int tpos = 0;
        while (num > 0) { tmp[tpos++] = (char)('0' + num % 10); num /= 10; }
        while (tpos > 0) buf[pos++] = tmp[--tpos];
    }
    buf[pos] = '\0';

    string_utils_err_t e1 = str_to_int(buf, &val1);
    string_utils_err_t e2 = str_to_int(buf, &val2);
    PROP_ASSERT_EQ(e1, e2);
    if (e1 == STRING_UTILS_OK) {
        PROP_ASSERT_EQ(val1, val2);
    }
}

TEST_MAIN("string_utils properties") {
    RUN(no_crash_trim);
    RUN(trim_null_terminated);
    RUN(starts_with_prefix);
    RUN(split_bounded_tokens);
    RUN(to_int_deterministic);
}
TEST_MAIN_END()
