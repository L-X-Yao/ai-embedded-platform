// SPDX-License-Identifier: MIT
/*
 * double_buffer — ping-pong double buffer for DMA / producer-consumer
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "double_buffer.h"
#include "contract.h"

double_buffer_err_t double_buffer_init(double_buffer_t *ctx,
                                       uint8_t *buf_a,
                                       uint8_t *buf_b,
                                       size_t buf_size)
{
    if (!ctx || !buf_a || !buf_b || buf_size == 0) {
        return DOUBLE_BUFFER_ERR_PARAM;
    }
    if (buf_a == buf_b) {
        return DOUBLE_BUFFER_ERR_PARAM;
    }

    ctx->buf_a     = buf_a;
    ctx->buf_b     = buf_b;
    ctx->buf_size  = buf_size;
    ctx->write_idx = 0;
    ctx->ready     = 0U;

    ENSURE(ctx->write_idx == 0);
    ENSURE(ctx->ready == 0U);
    return DOUBLE_BUFFER_OK;
}

uint8_t *double_buffer_get_write(const double_buffer_t *ctx)
{
    if (!ctx) { return NULL; }

    uint8_t *buf = (ctx->write_idx == 0) ? ctx->buf_a : ctx->buf_b;

    DOUBLE_BUFFER_BARRIER(); /* use of the buffer stays after the role read */
    return buf;
}

uint8_t *double_buffer_get_read(const double_buffer_t *ctx)
{
    if (!ctx) { return NULL; }

    uint8_t *buf = (ctx->write_idx == 0) ? ctx->buf_b : ctx->buf_a;

    DOUBLE_BUFFER_BARRIER(); /* use of the buffer stays after the role read */
    return buf;
}

size_t double_buffer_get_size(const double_buffer_t *ctx)
{
    if (!ctx) { return 0; }
    return ctx->buf_size;
}

double_buffer_err_t double_buffer_swap(double_buffer_t *ctx)
{
    if (!ctx) { return DOUBLE_BUFFER_ERR_PARAM; }

    DOUBLE_BUFFER_BARRIER(); /* the frame lands before the roles change */
    ctx->write_idx = (uint8_t)(ctx->write_idx ^ 1U);
    ctx->ready = 1U; /* after write_idx: ready means the roles have changed */

    ENSURE(ctx->write_idx <= 1);
    ENSURE(ctx->ready != 0U);
    return DOUBLE_BUFFER_OK;
}

bool double_buffer_is_ready(const double_buffer_t *ctx)
{
    if (!ctx) { return false; }
    return ctx->ready != 0U;
}

double_buffer_err_t double_buffer_reset(double_buffer_t *ctx)
{
    if (!ctx) { return DOUBLE_BUFFER_ERR_PARAM; }

    ctx->write_idx = 0;
    ctx->ready     = 0U;

    ENSURE(ctx->write_idx == 0);
    ENSURE(ctx->ready == 0U);
    return DOUBLE_BUFFER_OK;
}
