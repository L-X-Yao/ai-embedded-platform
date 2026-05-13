/*
 * mock_spi.h — shared SPI mock infrastructure for L1 driver tests
 *
 * Header-only. Provides CS tracking, fail injection, and xfer byte
 * logging. Unlike the I2C mock, SPI has no universal register model,
 * so each device must supply its own xfer callback that interprets
 * the protocol. This header provides the common scaffolding.
 *
 * Usage:
 *   #include "test_harness.h"
 *   #include "mock_spi.h"
 *   #include "my_spi_device.h"
 *
 *   static uint8_t my_xfer(void *ctx, uint8_t tx) {
 *       if (mock_spi_should_fail()) return 0xFF;
 *       mock_spi_log_byte(tx);
 *       // device-specific protocol handling ...
 *       return response;
 *   }
 *
 *   static const my_device_spi_t mock_spi = {
 *       .cs_low  = mock_spi_cs_low,
 *       .cs_high = mock_spi_cs_high,
 *       .xfer    = my_xfer,
 *       .ctx     = NULL,
 *   };
 */

#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef MOCK_SPI_LOG_SIZE
#define MOCK_SPI_LOG_SIZE 256
#endif

static int     mock_spi_cs_state;
static int     mock_spi_fail_next;
static int     mock_spi_op_count;
static int     mock_spi_fail_at;
static uint8_t mock_spi_tx_log[MOCK_SPI_LOG_SIZE];
static int     mock_spi_tx_log_count;
static int     mock_spi_cs_toggles;

static void mock_spi_reset(void)
{
    mock_spi_cs_state    = 1;
    mock_spi_fail_next   = 0;
    mock_spi_op_count    = 0;
    mock_spi_fail_at     = -1;
    mock_spi_tx_log_count = 0;
    mock_spi_cs_toggles  = 0;
    memset(mock_spi_tx_log, 0, sizeof(mock_spi_tx_log));
}

__attribute__((unused))
static bool mock_spi_should_fail(void)
{
    int n = mock_spi_op_count++;
    if (mock_spi_fail_next) { mock_spi_fail_next = 0; return true; }
    if (mock_spi_fail_at >= 0 && n == mock_spi_fail_at) return true;
    return false;
}

__attribute__((unused))
static void mock_spi_cs_low(void *ctx)
{
    (void)ctx;
    mock_spi_cs_state = 0;
    mock_spi_cs_toggles++;
}

__attribute__((unused))
static void mock_spi_cs_high(void *ctx)
{
    (void)ctx;
    mock_spi_cs_state = 1;
    mock_spi_cs_toggles++;
}

__attribute__((unused))
static void mock_spi_log_byte(uint8_t tx)
{
    if (mock_spi_tx_log_count < MOCK_SPI_LOG_SIZE)
        mock_spi_tx_log[mock_spi_tx_log_count++] = tx;
}

/* --- xfer sequence matchers --------------------------------------------- */
/*
 * Assert the captured TX log matches an expected byte sequence.
 * Returns 1 on match, 0 on mismatch (caller chooses how to fail).
 *
 * mock_spi_log_starts_with: prefix match (extra trailing bytes ok)
 * mock_spi_log_equals:      exact length + bytes
 * mock_spi_log_contains:    expected sequence appears anywhere in log
 */

__attribute__((unused))
static int mock_spi_log_starts_with(const uint8_t *expect, int len)
{
    if (mock_spi_tx_log_count < len) return 0;
    int i;
    for (i = 0; i < len; i++)
        if (mock_spi_tx_log[i] != expect[i]) return 0;
    return 1;
}

__attribute__((unused))
static int mock_spi_log_equals(const uint8_t *expect, int len)
{
    if (mock_spi_tx_log_count != len) return 0;
    return mock_spi_log_starts_with(expect, len);
}

__attribute__((unused))
static int mock_spi_log_contains(const uint8_t *expect, int len)
{
    if (len <= 0 || mock_spi_tx_log_count < len) return 0;
    int max = mock_spi_tx_log_count - len;
    int i, j;
    for (i = 0; i <= max; i++) {
        for (j = 0; j < len; j++)
            if (mock_spi_tx_log[i + j] != expect[j]) break;
        if (j == len) return 1;
    }
    return 0;
}

#endif /* MOCK_SPI_H */
