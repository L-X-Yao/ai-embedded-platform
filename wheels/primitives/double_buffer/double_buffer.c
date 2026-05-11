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
    ctx->ready     = false;

    ENSURE(ctx->write_idx == 0);
    ENSURE(ctx->ready == false);
    return DOUBLE_BUFFER_OK;
}

uint8_t *double_buffer_get_write(const double_buffer_t *ctx)
{
    if (!ctx) { return NULL; }
    return (ctx->write_idx == 0) ? ctx->buf_a : ctx->buf_b;
}

uint8_t *double_buffer_get_read(const double_buffer_t *ctx)
{
    if (!ctx) { return NULL; }
    return (ctx->write_idx == 0) ? ctx->buf_b : ctx->buf_a;
}

size_t double_buffer_get_size(const double_buffer_t *ctx)
{
    if (!ctx) { return 0; }
    return ctx->buf_size;
}

double_buffer_err_t double_buffer_swap(double_buffer_t *ctx)
{
    if (!ctx) { return DOUBLE_BUFFER_ERR_PARAM; }

    ctx->write_idx = (uint8_t)(ctx->write_idx ^ 1U);
    ctx->ready = true;

    ENSURE(ctx->write_idx <= 1);
    ENSURE(ctx->ready == true);
    return DOUBLE_BUFFER_OK;
}

bool double_buffer_is_ready(const double_buffer_t *ctx)
{
    if (!ctx) { return false; }
    return ctx->ready;
}

double_buffer_err_t double_buffer_reset(double_buffer_t *ctx)
{
    if (!ctx) { return DOUBLE_BUFFER_ERR_PARAM; }

    ctx->write_idx = 0;
    ctx->ready     = false;

    ENSURE(ctx->write_idx == 0);
    ENSURE(ctx->ready == false);
    return DOUBLE_BUFFER_OK;
}
