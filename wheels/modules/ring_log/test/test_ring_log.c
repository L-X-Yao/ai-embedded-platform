#include "test_harness.h"
#include <string.h>
#include "ring_log.h"

/* ---- 1. init ---- */
TEST(test_init)
{
    ring_log_t rl;
    ASSERT_EQ(ring_log_init(&rl, RING_LOG_INFO), RING_LOG_OK);
    ASSERT_EQ(ring_log_count(&rl), 0);
    ASSERT_EQ(ring_log_total(&rl), 0);
    ASSERT_EQ(ring_log_dropped(&rl), 0);
}

/* ---- 2. write + read ---- */
TEST(test_write_read)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_INFO, 100, "hello"), RING_LOG_OK);
    ASSERT_EQ(ring_log_count(&rl), 1);

    ring_log_entry_t e;
    ASSERT_EQ(ring_log_read(&rl, 0, &e), RING_LOG_OK);
    ASSERT_EQ(e.timestamp, 100);
    ASSERT_EQ(e.level, RING_LOG_INFO);
    ASSERT_EQ(strcmp(e.message, "hello"), 0);
}

/* ---- 3. level filter (below min rejected) ---- */
TEST(test_level_filter)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_WARN);
    /* DEBUG < WARN => rejected */
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_DEBUG, 1, "dbg"), RING_LOG_ERR_FULL);
    /* INFO < WARN => rejected */
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_INFO, 2, "inf"), RING_LOG_ERR_FULL);
    ASSERT_EQ(ring_log_count(&rl), 0);
    ASSERT_EQ(ring_log_dropped(&rl), 2);

    /* WARN >= WARN => accepted */
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_WARN, 3, "wrn"), RING_LOG_OK);
    ASSERT_EQ(ring_log_count(&rl), 1);
}

/* ---- 4. ring wraparound ---- */
TEST(test_wraparound)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);

    /* fill beyond capacity */
    for (uint32_t i = 0; i < RING_LOG_MAX_ENTRIES + 5; i++) {
        ASSERT_EQ(ring_log_write(&rl, RING_LOG_INFO, i, "msg%u", i),
                  RING_LOG_OK);
    }
    /* count capped at max */
    ASSERT_EQ(ring_log_count(&rl), RING_LOG_MAX_ENTRIES);
    /* total includes overwrites */
    ASSERT_EQ(ring_log_total(&rl), RING_LOG_MAX_ENTRIES + 5);

    /* newest (index 0) should be last written */
    ring_log_entry_t e;
    ASSERT_EQ(ring_log_read(&rl, 0, &e), RING_LOG_OK);
    ASSERT_EQ(e.timestamp, RING_LOG_MAX_ENTRIES + 4);
}

/* ---- 5. dropped count ---- */
TEST(test_dropped_count)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_ERROR);
    ring_log_write(&rl, RING_LOG_DEBUG, 1, "a");
    ring_log_write(&rl, RING_LOG_INFO,  2, "b");
    ring_log_write(&rl, RING_LOG_WARN,  3, "c");
    ASSERT_EQ(ring_log_dropped(&rl), 3);
    ASSERT_EQ(ring_log_count(&rl), 0);
}

/* ---- 6. total count ---- */
TEST(test_total_count)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    ring_log_write(&rl, RING_LOG_INFO, 1, "a");
    ring_log_write(&rl, RING_LOG_INFO, 2, "b");
    ring_log_write(&rl, RING_LOG_INFO, 3, "c");
    ASSERT_EQ(ring_log_total(&rl), 3);
}

/* ---- 7. clear ---- */
TEST(test_clear)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_WARN);
    ring_log_write(&rl, RING_LOG_ERROR, 1, "err");
    ring_log_write(&rl, RING_LOG_ERROR, 2, "err2");
    ASSERT_EQ(ring_log_count(&rl), 2);

    ring_log_clear(&rl);
    ASSERT_EQ(ring_log_count(&rl), 0);
    ASSERT_EQ(ring_log_total(&rl), 0);
    ASSERT_EQ(ring_log_dropped(&rl), 0);
    /* min_level preserved after clear */
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_DEBUG, 3, "nope"),
              RING_LOG_ERR_FULL);
}

/* ---- 8. read oldest order ---- */
TEST(test_read_oldest_order)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    ring_log_write(&rl, RING_LOG_INFO, 10, "first");
    ring_log_write(&rl, RING_LOG_INFO, 20, "second");
    ring_log_write(&rl, RING_LOG_INFO, 30, "third");

    ring_log_entry_t e;
    /* oldest = 0 */
    ASSERT_EQ(ring_log_read_oldest(&rl, 0, &e), RING_LOG_OK);
    ASSERT_EQ(e.timestamp, 10);
    ASSERT_EQ(strcmp(e.message, "first"), 0);
    /* newest = count-1 */
    ASSERT_EQ(ring_log_read_oldest(&rl, 2, &e), RING_LOG_OK);
    ASSERT_EQ(e.timestamp, 30);
    ASSERT_EQ(strcmp(e.message, "third"), 0);
}

/* ---- 9. read out of range ---- */
TEST(test_read_out_of_range)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    ring_log_write(&rl, RING_LOG_INFO, 1, "one");

    ring_log_entry_t e;
    ASSERT_EQ(ring_log_read(&rl, 1, &e), RING_LOG_ERR_PARAMS);
    ASSERT_EQ(ring_log_read_oldest(&rl, 1, &e), RING_LOG_ERR_PARAMS);
}

/* ---- 10. NULL params ---- */
TEST(test_null_params)
{
    ring_log_t rl;
    ring_log_entry_t e;
    ASSERT_EQ(ring_log_init(NULL, RING_LOG_DEBUG), RING_LOG_ERR_PARAMS);
    ASSERT_EQ(ring_log_write(NULL, RING_LOG_INFO, 0, "x"), RING_LOG_ERR_PARAMS);
    ring_log_init(&rl, RING_LOG_DEBUG);
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_INFO, 0, NULL), RING_LOG_ERR_PARAMS);
    ASSERT_EQ(ring_log_read(NULL, 0, &e), RING_LOG_ERR_PARAMS);
    ASSERT_EQ(ring_log_read(&rl, 0, NULL), RING_LOG_ERR_PARAMS);
    ASSERT_EQ(ring_log_read_oldest(NULL, 0, &e), RING_LOG_ERR_PARAMS);
    ASSERT_EQ(ring_log_set_level(NULL, RING_LOG_DEBUG), RING_LOG_ERR_PARAMS);
    ASSERT_EQ(ring_log_count(NULL), 0);
    ASSERT_EQ(ring_log_total(NULL), 0);
    ASSERT_EQ(ring_log_dropped(NULL), 0);
}

/* ---- 11. format string with args ---- */
TEST(test_format_string)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    ring_log_write(&rl, RING_LOG_ERROR, 42, "temp=%d psi=%.1f", -5, 101.3);

    ring_log_entry_t e;
    ring_log_read(&rl, 0, &e);
    ASSERT_EQ(strcmp(e.message, "temp=-5 psi=101.3"), 0);
}

/* ---- 12. max entries fill (exactly) ---- */
TEST(test_max_entries_fill)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    for (uint16_t i = 0; i < RING_LOG_MAX_ENTRIES; i++) {
        ring_log_write(&rl, RING_LOG_INFO, (uint32_t)i, "e%u", (unsigned)i);
    }
    ASSERT_EQ(ring_log_count(&rl), RING_LOG_MAX_ENTRIES);
    ASSERT_EQ(ring_log_total(&rl), RING_LOG_MAX_ENTRIES);
    ASSERT_EQ(ring_log_dropped(&rl), 0);

    /* oldest = entry 0, newest = entry MAX-1 */
    ring_log_entry_t e;
    ring_log_read_oldest(&rl, 0, &e);
    ASSERT_EQ(e.timestamp, 0);
    ring_log_read(&rl, 0, &e);
    ASSERT_EQ(e.timestamp, RING_LOG_MAX_ENTRIES - 1);
}

/* ---- 13. set_level ---- */
TEST(test_set_level)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_ERROR);
    /* DEBUG rejected at ERROR level */
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_DEBUG, 1, "no"), RING_LOG_ERR_FULL);

    /* lower threshold to DEBUG */
    ASSERT_EQ(ring_log_set_level(&rl, RING_LOG_DEBUG), RING_LOG_OK);
    ASSERT_EQ(ring_log_write(&rl, RING_LOG_DEBUG, 2, "yes"), RING_LOG_OK);
    ASSERT_EQ(ring_log_count(&rl), 1);
}

/* ---- 14. empty log read ---- */
TEST(test_empty_log_read)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    ring_log_entry_t e;
    ASSERT_EQ(ring_log_read(&rl, 0, &e), RING_LOG_ERR_EMPTY);
    ASSERT_EQ(ring_log_read_oldest(&rl, 0, &e), RING_LOG_ERR_EMPTY);
}

/* ---- 15. zero-init safety ---- */
TEST(test_zero_init_safety)
{
    ring_log_t rl;
    memset(&rl, 0, sizeof(rl));
    /* zero-init struct should behave like init(DEBUG) */
    ASSERT_EQ(ring_log_count(&rl), 0);
    ASSERT_EQ(ring_log_total(&rl), 0);
    ASSERT_EQ(ring_log_dropped(&rl), 0);
}

/* ---- 16. read_oldest after wraparound ---- */
TEST(test_read_oldest_after_wrap)
{
    ring_log_t rl;
    ring_log_init(&rl, RING_LOG_DEBUG);
    /* write MAX+3 entries so oldest is entry 3 */
    for (uint32_t i = 0; i < RING_LOG_MAX_ENTRIES + 3u; i++) {
        ring_log_write(&rl, RING_LOG_INFO, i, "w%u", i);
    }
    ring_log_entry_t e;
    ring_log_read_oldest(&rl, 0, &e);
    ASSERT_EQ(e.timestamp, 3);
    ring_log_read_oldest(&rl, (uint16_t)(RING_LOG_MAX_ENTRIES - 1u), &e);
    ASSERT_EQ(e.timestamp, RING_LOG_MAX_ENTRIES + 2u);
}

TEST_MAIN("ring_log")
    RUN(test_init);
    RUN(test_write_read);
    RUN(test_level_filter);
    RUN(test_wraparound);
    RUN(test_dropped_count);
    RUN(test_total_count);
    RUN(test_clear);
    RUN(test_read_oldest_order);
    RUN(test_read_out_of_range);
    RUN(test_null_params);
    RUN(test_format_string);
    RUN(test_max_entries_fill);
    RUN(test_set_level);
    RUN(test_empty_log_read);
    RUN(test_zero_init_safety);
    RUN(test_read_oldest_after_wrap);
TEST_MAIN_END()
