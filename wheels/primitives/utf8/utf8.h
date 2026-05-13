// SPDX-License-Identifier: MIT
/*
 * utf8 — UTF-8 validation, decoding, and length counting
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless utilities for working with UTF-8 encoded strings on MCUs.
 * Validates sequences, decodes codepoints, counts characters.
 * Pure C99, no heap, no OS.
 */

#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UTF8_OK           =  0,
    UTF8_ERR_PARAM    = -1,
    UTF8_ERR_INVALID  = -2,
    UTF8_ERR_SHORT    = -3,
} utf8_err_t;

/** Expected byte count for a UTF-8 lead byte (1-4), or 0 if invalid. @thread_safe: yes @isr_safe: yes */
uint8_t utf8_byte_length(uint8_t lead);

/** Validate a UTF-8 string. Returns OK or ERR_INVALID. @thread_safe: yes @isr_safe: yes */
utf8_err_t utf8_validate(const uint8_t *data, size_t len);

/** Count codepoints in a UTF-8 string. Returns <0 on error. @thread_safe: yes @isr_safe: yes */
int32_t utf8_codepoint_count(const uint8_t *data, size_t len);

/**
 * Decode one codepoint. Returns bytes consumed via out_len.
 * @thread_safe: yes @isr_safe: yes
 */
utf8_err_t utf8_decode(const uint8_t *data, size_t len,
                        uint32_t *codepoint, size_t *out_len);

/**
 * Encode one codepoint. Returns bytes written via out_len.
 * @thread_safe: yes @isr_safe: yes
 */
utf8_err_t utf8_encode(uint32_t codepoint, uint8_t *buf, size_t cap,
                        size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* UTF8_H */
