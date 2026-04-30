/*
 * log_filter — module-based runtime filter sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_filter.h"
#include <string.h>

void log_filter_init(log_filter_t *f, log_filter_output_fn inner_fn,
                     void *inner_ctx, log_filter_mode_t mode) {
    if (!f) return;
    f->inner_fn = inner_fn;
    f->inner_ctx = inner_ctx;
    f->mode = mode;
    f->module_count = 0;
    memset(f->modules, 0, sizeof(f->modules));
}

int log_filter_add(log_filter_t *f, const char *module) {
    if (!f || !module) return -1;
    if (f->module_count >= LOG_FILTER_MAX_MODULES) return -1;
    f->modules[f->module_count++] = module;
    return 0;
}

void log_filter_clear(log_filter_t *f) {
    if (!f) return;
    f->module_count = 0;
    memset(f->modules, 0, sizeof(f->modules));
}

static const char *extract_module(const char *line, size_t len, size_t *mod_len) {
    /* Log format: "[TIMESTAMP] [L] [module] ..." or "[L] [module] ..."
     * Level bracket contains exactly one of E/W/I/D.
     * Module bracket is the first bracket AFTER the level bracket. */
    int found_level = 0;
    for (size_t i = 0; i + 2 < len; i++) {
        if (line[i] != '[') continue;

        if (!found_level && i + 2 < len && line[i + 2] == ']') {
            char c = line[i + 1];
            if (c == 'E' || c == 'W' || c == 'I' || c == 'D') {
                found_level = 1;
                i += 2;
                continue;
            }
        }

        if (found_level) {
            const char *start = line + i + 1;
            for (size_t j = i + 1; j < len; j++) {
                if (line[j] == ']') {
                    *mod_len = j - i - 1;
                    return start;
                }
            }
        }
    }
    *mod_len = 0;
    return NULL;
}

static int module_matches(const log_filter_t *f, const char *mod, size_t mod_len) {
    for (int i = 0; i < f->module_count; i++) {
        if (f->modules[i] && strlen(f->modules[i]) == mod_len &&
            memcmp(f->modules[i], mod, mod_len) == 0) {
            return 1;
        }
    }
    return 0;
}

void log_filter_sink(void *ctx, const char *line, size_t len) {
    log_filter_t *f = (log_filter_t *)ctx;
    if (!f || !f->inner_fn || !line || len == 0) return;

    size_t mod_len;
    const char *mod = extract_module(line, len, &mod_len);

    if (!mod || mod_len == 0) {
        f->inner_fn(f->inner_ctx, line, len);
        return;
    }

    int matched = module_matches(f, mod, mod_len);

    if (f->mode == LOG_FILTER_ALLOW && matched) {
        f->inner_fn(f->inner_ctx, line, len);
    } else if (f->mode == LOG_FILTER_DENY && !matched) {
        f->inner_fn(f->inner_ctx, line, len);
    }
}
