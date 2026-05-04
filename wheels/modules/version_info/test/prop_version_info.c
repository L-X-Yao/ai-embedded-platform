/*
 * prop_version_info.c — property-based tests for version_info
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_version_info.c ../version_info.c -o prop_version_info
 */

#include "test_harness.h"
#include "property_test.h"
#include "../version_info.h"
#include <string.h>
#include <stdio.h>

/* Property: parse then to_string roundtrip */
PROPERTY(parse_tostring_roundtrip, 20000) {
    uint16_t major = (uint16_t)ARBI_RANGE(seed, 0, 999);
    uint16_t minor = (uint16_t)ARBI_RANGE(seed, 0, 999);
    uint16_t patch = (uint16_t)ARBI_RANGE(seed, 0, 999);

    char input[32];
    snprintf(input, sizeof(input), "%u.%u.%u", major, minor, patch);

    version_t v;
    version_info_err_t err = version_parse(input, &v);
    PROP_ASSERT_EQ(err, VERSION_INFO_OK);
    PROP_ASSERT_EQ(v.major, major);
    PROP_ASSERT_EQ(v.minor, minor);
    PROP_ASSERT_EQ(v.patch, patch);

    char output[32];
    int len = version_to_string(&v, output, sizeof(output));
    PROP_ASSERT(len > 0);
    PROP_ASSERT(strcmp(input, output) == 0);
}

/* Property: compare is consistent: a<b implies b>a */
PROPERTY(compare_antisymmetric, 10000) {
    version_t a = {
        .major = (uint16_t)ARBI_RANGE(seed, 0, 100),
        .minor = (uint16_t)ARBI_RANGE(seed, 0, 100),
        .patch = (uint16_t)ARBI_RANGE(seed, 0, 100),
    };
    version_t b = {
        .major = (uint16_t)ARBI_RANGE(seed, 0, 100),
        .minor = (uint16_t)ARBI_RANGE(seed, 0, 100),
        .patch = (uint16_t)ARBI_RANGE(seed, 0, 100),
    };

    int cmp_ab = version_compare(&a, &b);
    int cmp_ba = version_compare(&b, &a);

    if (cmp_ab < 0) { PROP_ASSERT(cmp_ba > 0); }
    else if (cmp_ab > 0) { PROP_ASSERT(cmp_ba < 0); }
    else { PROP_ASSERT_EQ(cmp_ba, 0); }
}

/* Property: compatible is reflexive (a is compatible with a) */
PROPERTY(compatible_reflexive, 10000) {
    version_t v = {
        .major = (uint16_t)ARBI_RANGE(seed, 0, 100),
        .minor = (uint16_t)ARBI_RANGE(seed, 0, 100),
        .patch = (uint16_t)ARBI_RANGE(seed, 0, 100),
    };

    PROP_ASSERT(version_compatible(&v, &v));
}

/* Property: parse of arbitrary strings never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    char garbage[64];
    size_t len = ARBI_SIZE(seed, 63);
    for (size_t i = 0; i < len; i++)
        garbage[i] = (char)ARBI_RANGE(seed, 0x20, 0x7E);
    garbage[len] = '\0';

    version_t v;
    memset(&v, 0, sizeof(v));
    version_parse(garbage, &v);
    version_is_valid(&v);
}

TEST_MAIN("version_info properties") {
    RUN(parse_tostring_roundtrip);
    RUN(compare_antisymmetric);
    RUN(compatible_reflexive);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
