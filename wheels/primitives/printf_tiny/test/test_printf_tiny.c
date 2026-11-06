// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "printf_tiny.h"

/* --- helpers ------------------------------------------------------- */

static char g_buf[256];

/* Callback that appends to g_buf via a position counter stored in ctx */
static void test_putc(void *ctx, char c)
{
    int *pos = (int *)ctx;
    if (*pos < (int)(sizeof(g_buf) - 1)) {
        g_buf[*pos] = c;
    }
    (*pos)++;
}

/* Format via putc callback, NUL-terminate, return length */
static int fmt_putc(const char *fmt, ...)
{
    int pos = 0;
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintf_tiny(test_putc, &pos, fmt, ap);
    va_end(ap);
    if (pos < (int)sizeof(g_buf)) g_buf[pos] = '\0';
    else g_buf[sizeof(g_buf) - 1] = '\0';
    return ret;
}

/* --- %d tests ------------------------------------------------------ */

TEST(test_d_positive)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%d", 42);
    ASSERT_EQ(n, 2);
    ASSERT_TRUE(strcmp(buf, "42") == 0);
}

TEST(test_d_negative)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%d", -7);
    ASSERT_EQ(n, 2);
    ASSERT_TRUE(strcmp(buf, "-7") == 0);
}

TEST(test_d_zero)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%d", 0);
    ASSERT_EQ(n, 1);
    ASSERT_TRUE(strcmp(buf, "0") == 0);
}

TEST(test_d_int_min)
{
    char buf[32];
    snprintf_tiny(buf, sizeof(buf), "%d", INT_MIN);
    /* verify it starts with '-' and is a valid number */
    ASSERT_TRUE(buf[0] == '-');
    ASSERT_TRUE(strlen(buf) > 1);
    /* verify round-trip: INT_MIN = -2147483648 on 32-bit int */
    char ref[32];
    snprintf(ref, sizeof(ref), "%d", INT_MIN);
    ASSERT_TRUE(strcmp(buf, ref) == 0);
}

TEST(test_d_int_max)
{
    char buf[32];
    snprintf_tiny(buf, sizeof(buf), "%d", INT_MAX);
    char ref[32];
    snprintf(ref, sizeof(ref), "%d", INT_MAX);
    ASSERT_TRUE(strcmp(buf, ref) == 0);
}

/* --- %u tests ------------------------------------------------------ */

TEST(test_u_zero)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%u", 0u);
    ASSERT_EQ(n, 1);
    ASSERT_TRUE(strcmp(buf, "0") == 0);
}

TEST(test_u_max)
{
    char buf[32];
    snprintf_tiny(buf, sizeof(buf), "%u", UINT_MAX);
    char ref[32];
    snprintf(ref, sizeof(ref), "%u", UINT_MAX);
    ASSERT_TRUE(strcmp(buf, ref) == 0);
}

/* --- %x / %X tests ------------------------------------------------- */

TEST(test_x_lowercase)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%x", 0xDEAD);
    ASSERT_EQ(n, 4);
    ASSERT_TRUE(strcmp(buf, "dead") == 0);
}

TEST(test_x_uppercase)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%X", 0xBEEF);
    ASSERT_EQ(n, 4);
    ASSERT_TRUE(strcmp(buf, "BEEF") == 0);
}

TEST(test_x_zero_padded)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%08x", 0xFF);
    ASSERT_EQ(n, 8);
    ASSERT_TRUE(strcmp(buf, "000000ff") == 0);
}

/* --- %o test ------------------------------------------------------- */

TEST(test_o_octal)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%o", 255);
    ASSERT_EQ(n, 3);
    ASSERT_TRUE(strcmp(buf, "377") == 0);
}

/* --- %c test ------------------------------------------------------- */

TEST(test_c_char)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%c", 'A');
    ASSERT_EQ(n, 1);
    ASSERT_TRUE(strcmp(buf, "A") == 0);
}

/* --- %s tests ------------------------------------------------------ */

TEST(test_s_string)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%s", "hello");
    ASSERT_EQ(n, 5);
    ASSERT_TRUE(strcmp(buf, "hello") == 0);
}

TEST(test_s_null)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%s", (const char *)NULL);
    ASSERT_EQ(n, 6);
    ASSERT_TRUE(strcmp(buf, "(null)") == 0);
}

/* --- %p test ------------------------------------------------------- */

TEST(test_p_pointer)
{
    char buf[32];
    /* use a known value so we can check output */
    uintptr_t addr = 0x1234;
    int n = snprintf_tiny(buf, sizeof(buf), "%p", (void *)addr);
    ASSERT_TRUE(n > 2);
    ASSERT_TRUE(buf[0] == '0' && buf[1] == 'x');
    ASSERT_TRUE(strcmp(buf, "0x1234") == 0);
}

/* --- %% test ------------------------------------------------------- */

TEST(test_percent_literal)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "100%%");
    ASSERT_EQ(n, 4);
    ASSERT_TRUE(strcmp(buf, "100%") == 0);
}

/* --- width / alignment tests --------------------------------------- */

TEST(test_width_right_align)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%10d", 42);
    ASSERT_EQ(n, 10);
    ASSERT_TRUE(strcmp(buf, "        42") == 0);
}

TEST(test_width_left_align)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%-10s!", "hi");
    ASSERT_EQ(n, 11);
    ASSERT_TRUE(strcmp(buf, "hi        !") == 0);
}

/* --- zero-pad tests ------------------------------------------------ */

TEST(test_zero_pad_d)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%04d", 7);
    ASSERT_EQ(n, 4);
    ASSERT_TRUE(strcmp(buf, "0007") == 0);
}

TEST(test_zero_pad_negative)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "%06d", -42);
    ASSERT_EQ(n, 6);
    ASSERT_TRUE(strcmp(buf, "-00042") == 0);
}

/* --- long modifier tests ------------------------------------------- */

TEST(test_ld_long)
{
    char buf[32];
    long v = 123456789L;
    int n = snprintf_tiny(buf, sizeof(buf), "%ld", v);
    ASSERT_EQ(n, 9);
    ASSERT_TRUE(strcmp(buf, "123456789") == 0);
}

TEST(test_lu_unsigned_long)
{
    char buf[32];
    unsigned long v = 4000000000UL;
    snprintf_tiny(buf, sizeof(buf), "%lu", v);
    char ref[32];
    snprintf(ref, sizeof(ref), "%lu", v);
    ASSERT_TRUE(strcmp(buf, ref) == 0);
}

/* --- snprintf_tiny truncation tests -------------------------------- */

TEST(test_snprintf_truncation)
{
    char buf[6]; /* 5 chars + NUL */
    int n = snprintf_tiny(buf, sizeof(buf), "hello world");
    /* returns total that would have been written */
    ASSERT_EQ(n, 11);
    /* buffer contains truncated + NUL */
    ASSERT_TRUE(strcmp(buf, "hello") == 0);
}

TEST(test_snprintf_exact_fit)
{
    char buf[4];
    int n = snprintf_tiny(buf, sizeof(buf), "abc");
    ASSERT_EQ(n, 3);
    ASSERT_TRUE(strcmp(buf, "abc") == 0);
}

/* --- multiple specifiers ------------------------------------------- */

TEST(test_multiple_specifiers)
{
    char buf[64];
    int n = snprintf_tiny(buf, sizeof(buf), "%s=%d, 0x%04X", "val", 42, 255);
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strcmp(buf, "val=42, 0x00FF") == 0);
}

/* --- plain string (no specifiers) ---------------------------------- */

TEST(test_plain_string)
{
    char buf[32];
    int n = snprintf_tiny(buf, sizeof(buf), "no format");
    ASSERT_EQ(n, 9);
    ASSERT_TRUE(strcmp(buf, "no format") == 0);
}

/* --- NULL fmt ------------------------------------------------------- */

TEST(test_null_fmt)
{
    int n = snprintf_tiny(NULL, 0, NULL);
    ASSERT_EQ(n, 0);
}

/* --- putc callback mode -------------------------------------------- */

TEST(test_putc_callback)
{
    int n = fmt_putc("A%d%s", 99, "z");
    ASSERT_EQ(n, 4);
    ASSERT_TRUE(strcmp(g_buf, "A99z") == 0);
}

TEST(test_putc_null_returns_zero)
{
    int n = printf_tiny(NULL, NULL, "test");
    ASSERT_EQ(n, 0);
}

/* --- width limit --------------------------------------------------- */

/* A width past the limit saturates: no signed overflow, no 2^31 callbacks. */
TEST(test_huge_width_saturates)
{
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%99999999999d", 1), PRINTF_TINY_MAX_WIDTH);
    /* 2^32 + 1 would wrap round to a width of 1 */
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%4294967297d", 1), PRINTF_TINY_MAX_WIDTH);
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%-99999999999d", 1), PRINTF_TINY_MAX_WIDTH);
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%099999999999d", 1), PRINTF_TINY_MAX_WIDTH);
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%99999999999u", 1u), PRINTF_TINY_MAX_WIDTH);
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%99999999999s", "ab"), PRINTF_TINY_MAX_WIDTH);
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%99999999999c", 'a'), PRINTF_TINY_MAX_WIDTH);
}

TEST(test_width_at_the_limit)
{
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%4094d", 1), 4094);
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%4095d", 1), 4095);
    ASSERT_EQ(snprintf_tiny(NULL, 0, "%4096d", 1), 4095);
}

/* All the digits are consumed, so what follows still formats. */
TEST(test_format_continues_after_huge_width)
{
    static char big[PRINTF_TINY_MAX_WIDTH + 8];
    int n = snprintf_tiny(big, (uint16_t)sizeof(big), "%-99999999999d|%d", 1, 22);

    ASSERT_EQ(n, PRINTF_TINY_MAX_WIDTH + 3);
    ASSERT_EQ(big[0], '1');
    ASSERT_EQ(big[PRINTF_TINY_MAX_WIDTH - 1], ' ');
    ASSERT_STR_EQ(big + PRINTF_TINY_MAX_WIDTH, "|22");
}

/* ------------------------------------------------------------------- */

TEST_MAIN("printf_tiny")
    RUN(test_huge_width_saturates);
    RUN(test_width_at_the_limit);
    RUN(test_format_continues_after_huge_width);
    RUN(test_d_positive);
    RUN(test_d_negative);
    RUN(test_d_zero);
    RUN(test_d_int_min);
    RUN(test_d_int_max);
    RUN(test_u_zero);
    RUN(test_u_max);
    RUN(test_x_lowercase);
    RUN(test_x_uppercase);
    RUN(test_x_zero_padded);
    RUN(test_o_octal);
    RUN(test_c_char);
    RUN(test_s_string);
    RUN(test_s_null);
    RUN(test_p_pointer);
    RUN(test_percent_literal);
    RUN(test_width_right_align);
    RUN(test_width_left_align);
    RUN(test_zero_pad_d);
    RUN(test_zero_pad_negative);
    RUN(test_ld_long);
    RUN(test_lu_unsigned_long);
    RUN(test_snprintf_truncation);
    RUN(test_snprintf_exact_fit);
    RUN(test_multiple_specifiers);
    RUN(test_plain_string);
    RUN(test_null_fmt);
    RUN(test_putc_callback);
    RUN(test_putc_null_returns_zero);
TEST_MAIN_END()
