// SPDX-License-Identifier: MIT
/* Shared by the host models: checks, the RCU stand-in, and the failure count. */
#ifndef HOST_H
#define HOST_H

#include "hal/vsf_hal.h"

extern unsigned host_checks, host_fails;
extern uint32_t host_rcu[0x20];     /* word offsets as in common/common.h */

#define CHECK(c)                                                                \
    do {                                                                        \
        host_checks++;                                                          \
        if (!(c)) {                                                             \
            host_fails++;                                                       \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);               \
        }                                                                       \
    } while (0)

int host_summary(const char *name);

#endif
