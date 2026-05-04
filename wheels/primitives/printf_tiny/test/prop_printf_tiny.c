/*
 * prop_printf_tiny.c — property-based tests for printf_tiny
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_printf_tiny.c ../printf_tiny.c -o prop_printf_tiny
 */

#include "test_harness.h"
#include "property_test.h"
#include "../printf_tiny.h"
#include <string.h>
#include <stdio.h>

/* Property: snprintf_tiny never writes beyond buffer size */
PROPERTY(no_buffer_overflow, 10000) {
    /* Use a small buffer surrounded by sentinel bytes */
    uint8_t arena[80];
    memset(arena, 0xAA, sizeof(arena));

    uint16_t bufsize = (uint16_t)ARBI_RANGE(seed, 1, 60);
    char *buf = (char *)(arena + 10);

    int val = (int)ARBI_UINT32(seed);
    snprintf_tiny(buf, bufsize, "val=%d end", val);

    /* Check sentinels before and after */
    for (int i = 0; i < 10; i++) {
        PROP_ASSERT_EQ(arena[i], 0xAA);
    }
    for (int i = 10 + bufsize; i < 80; i++) {
        PROP_ASSERT_EQ(arena[i], 0xAA);
    }

    /* Ensure NUL terminated within bounds */
    int found_nul = 0;
    for (uint16_t i = 0; i < bufsize; i++) {
        if (buf[i] == '\0') { found_nul = 1; break; }
    }
    PROP_ASSERT(found_nul);
}

/* Property: %d formatting matches standard snprintf for random ints */
PROPERTY(int_format_matches, 10000) {
    int val = (int)ARBI_UINT32(seed);
    char tiny_buf[32];
    char std_buf[32];

    snprintf_tiny(tiny_buf, sizeof(tiny_buf), "%d", val);
    snprintf(std_buf, sizeof(std_buf), "%d", val);

    PROP_ASSERT(strcmp(tiny_buf, std_buf) == 0);
}

/* Property: %x formatting matches standard snprintf for random uints */
PROPERTY(hex_format_matches, 10000) {
    unsigned val = ARBI_UINT32(seed);
    char tiny_buf[32];
    char std_buf[32];

    snprintf_tiny(tiny_buf, sizeof(tiny_buf), "%x", val);
    snprintf(std_buf, sizeof(std_buf), "%x", val);

    PROP_ASSERT(strcmp(tiny_buf, std_buf) == 0);
}

/* Property: %u formatting matches standard snprintf for random uints */
PROPERTY(uint_format_matches, 10000) {
    unsigned val = ARBI_UINT32(seed);
    char tiny_buf[32];
    char std_buf[32];

    snprintf_tiny(tiny_buf, sizeof(tiny_buf), "%u", val);
    snprintf(std_buf, sizeof(std_buf), "%u", val);

    PROP_ASSERT(strcmp(tiny_buf, std_buf) == 0);
}

/* Property: return value equals number of chars that would be written */
PROPERTY(return_value_correct, 5000) {
    int val = (int)ARBI_UINT32(seed);

    /* Large buffer — no truncation */
    char buf[64];
    int ret = snprintf_tiny(buf, sizeof(buf), "num=%d", val);
    PROP_ASSERT_EQ(ret, (int)strlen(buf));

    /* Tiny buffer — still returns would-be length */
    char small[4];
    int ret2 = snprintf_tiny(small, sizeof(small), "num=%d", val);
    PROP_ASSERT(ret2 == ret);
}

TEST_MAIN("printf_tiny properties") {
    RUN(no_buffer_overflow);
    RUN(int_format_matches);
    RUN(hex_format_matches);
    RUN(uint_format_matches);
    RUN(return_value_correct);
}
TEST_MAIN_END()
