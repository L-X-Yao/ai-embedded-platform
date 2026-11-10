// SPDX-License-Identifier: MIT
/*
 * Runs the real gpio.c on a port whose registers need the RCU clock: with AHB1EN
 * bit 0 clear, writes are lost and reads give 0 (user manual Rev3.0, RCU chapter).
 */
#include "host.h"
#include "common/vendor/Include/gd32f4xx_gpio.h"

#define G               (&vsf_hw_gpio0)
#define AHB1EN          0x0CU           /* word offset, as in common/common.h */
#define EN_GPIOA        (1U << 0)

extern struct vsf_hw_gpio_t vsf_hw_gpio0;
vsf_err_t vsf_hw_gpio_port_config_pins(vsf_hw_gpio_t *g, vsf_gpio_pin_mask_t m, vsf_gpio_cfg_t *c);
void vsf_hw_gpio_set_direction(vsf_hw_gpio_t *g, vsf_gpio_pin_mask_t m, vsf_gpio_pin_mask_t d);

static uint32_t regs[0x30 / 4], gated;

volatile uint32_t *sim_gpio_reg(uint32_t reg, uint32_t off)
{
    (void)reg;
    if (!(host_rcu[AHB1EN] & EN_GPIOA)) {
        gated = 0;
        return (volatile uint32_t *)&gated;
    }
    return (volatile uint32_t *)&regs[off / 4];
}

static void reset_sim(void)
{
    memset(regs, 0, sizeof(regs));
    memset(host_rcu, 0, sizeof(host_rcu));
}

/* H1-3: nothing else in the port turns the clock on */
static void test_clock(void)
{
    vsf_gpio_cfg_t cfg = { .mode = VSF_GPIO_OUTPUT_PUSH_PULL };

    printf("H1-3 configuring a pin clocks its port\n");
    reset_sim();
    CHECK(vsf_hw_gpio_port_config_pins(G, 1U << 5, &cfg) == VSF_ERR_NONE);
    CHECK(host_rcu[AHB1EN] == EN_GPIOA);        /* that port, and no other */
    CHECK(regs[0] == (1U << 10));               /* CTL: pin 5 is output */

    printf("H1-3 setting a direction clocks its port\n");
    reset_sim();
    vsf_hw_gpio_set_direction(G, 1U << 3, 1U << 3);
    CHECK(host_rcu[AHB1EN] == EN_GPIOA);
    CHECK(regs[0] == (1U << 6));

    printf("H1-3 a second call leaves the first pin alone\n");
    CHECK(vsf_hw_gpio_port_config_pins(G, 1U << 5, &cfg) == VSF_ERR_NONE);
    CHECK(regs[0] == ((1U << 6) | (1U << 10)));
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    test_clock();
    return host_summary("gpio");
}
