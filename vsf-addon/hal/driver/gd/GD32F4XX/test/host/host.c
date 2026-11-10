// SPDX-License-Identifier: MIT
/* Host stand-ins for driver.c and the NVIC, shared by every model. */
#include "host.h"

unsigned host_asserts, host_checks, host_fails;
uint8_t host_nvic_on[256];
uint32_t host_rcu[0x20];

void vsf_hw_clkrst_region_set_bit(uint32_t region)
{
    host_rcu[region >> 16] |= 1U << (region & 0x1F);
}

void vsf_hw_clkrst_region_clear_bit(uint32_t region)
{
    host_rcu[region >> 16] &= ~(1U << (region & 0x1F));
}

int host_summary(const char *name)
{
    printf("%s: %u checks, %u failed\n", name, host_checks, host_fails);
    return host_fails ? 1 : 0;
}
