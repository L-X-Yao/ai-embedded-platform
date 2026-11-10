// SPDX-License-Identifier: MIT
/*
 * Runs the real usart.c on a model of the USART, from user manual Rev3.0 (CN)
 * sections 19.3 and 19.4. STAT0 and DATA hand out a shadow cell, settled at the
 * next access: see test_i2c.c. A write to STAT0 clears rc_w0 flags written as 0.
 */
#include "host.h"
#include "common/vendor/Include/gd32f4xx_usart.h"

#define U               (&vsf_hw_usart0)
#define IRQN            VSF_HW_USART0_IRQN
#define DATA_TAG        0xA5A50000U
#define OFF_STAT0       0x00U
#define OFF_DATA        0x04U
#define RC_W0_FLAGS     (USART_STAT0_CTSF | USART_STAT0_LBDF | USART_STAT0_TC | USART_STAT0_RBNE)
#define READ_CLEARED    (USART_STAT0_PERR | USART_STAT0_FERR | USART_STAT0_NERR | USART_STAT0_ORERR | USART_STAT0_IDLEF)

typedef struct vsf_hw_usart_t vsf_hw_usart_t;
extern vsf_hw_usart_t vsf_hw_usart0;
vsf_err_t vsf_hw_usart_init(vsf_hw_usart_t *u, vsf_usart_cfg_t *c);
void vsf_hw_usart_irq_enable(vsf_hw_usart_t *u, vsf_usart_irq_mask_t m);
void USART0_IRQHandler(void);

static uint32_t regs[0x90 / 4];
#define CTL0            regs[0x0C / 4]
#define CTL1            regs[0x10 / 4]
#define CTL2            regs[0x14 / 4]

static uint32_t st0, cell, cell_seen, rise_during_write;
static int open_off = -1, st0_read;
static uint16_t rxdata;
static int callbacks;
static uint32_t last_mask;

static void settle(void)
{
    if (open_off == (int)OFF_STAT0) {
        if (cell != cell_seen) {
            st0 |= rise_during_write;   /* hardware set it after the driver's read */
            st0 &= cell | ~RC_W0_FLAGS;
        }
        st0_read = 1;
    } else if (open_off == (int)OFF_DATA) {
        if ((cell >> 16) == (DATA_TAG >> 16)) {
            st0 &= ~USART_STAT0_RBNE;
            if (st0_read) {
                st0 &= ~READ_CLEARED;
            }
        }
        st0_read = 0;
    }
    open_off = -1;
}

volatile uint32_t *sim_usart_reg(uint32_t reg, uint32_t off)
{
    (void)reg;
    settle();
    if (off == OFF_STAT0) {
        cell = cell_seen = st0;
    } else if (off == OFF_DATA) {
        cell = DATA_TAG | rxdata;
    } else {
        return (volatile uint32_t *)&regs[off / 4];
    }
    open_off = (int)off;
    return (volatile uint32_t *)&cell;
}

static int irq_pending(void)
{
    settle();
    if (!host_nvic_on[IRQN]) {
        return 0;
    }
    return ((st0 & USART_STAT0_TBE) && (CTL0 & USART_CTL0_TBEIE))
        || ((st0 & (USART_STAT0_RBNE | USART_STAT0_ORERR)) && (CTL0 & USART_CTL0_RBNEIE))
        || ((st0 & USART_STAT0_PERR) && (CTL0 & USART_CTL0_PERRIE))
        || ((st0 & USART_STAT0_CTSF) && (CTL2 & USART_CTL2_CTSIE));
}

static long pump(long limit)
{
    long n = 0;
    while ((n < limit) && irq_pending()) {
        USART0_IRQHandler();
        n++;
    }
    return n;
}

static void on_irq(void *t, vsf_usart_t *u, vsf_usart_irq_mask_t m)
{
    (void)t; (void)u;
    callbacks++;
    last_mask = (uint32_t)m;
}

static vsf_err_t reset_and_init(uint32_t mode, uint32_t baud)
{
    memset(regs, 0, sizeof(regs));
    st0 = USART_STAT0_TBE | USART_STAT0_TC;
    open_off = -1;
    st0_read = 0;
    rise_during_write = 0;
    callbacks = 0;
    last_mask = 0;
    host_asserts = 0;
    vsf_usart_cfg_t cfg = { .mode = mode, .baudrate = baud };
    cfg.isr.handler_fn = on_irq;
    return vsf_hw_usart_init(U, &cfg);
}

/* H1-2: CTSF is cleared by software only, section 19.4.1 */
static void test_cts(void)
{
    printf("H1-2 one CTS change, one interrupt\n");
    reset_and_init(VSF_USART_TX_ENABLE | VSF_USART_CTS_HWCONTROL, 115200);
    vsf_hw_usart_irq_enable(U, VSF_USART_IRQ_MASK_CTS);
    st0 |= USART_STAT0_CTSF;
    long n = pump(1000);
    printf("  interrupt entries: %ld\n", n);
    CHECK(n == 1);
    CHECK(callbacks == 1);
    CHECK(last_mask == VSF_USART_IRQ_MASK_CTS);
    CHECK(!(st0 & USART_STAT0_CTSF));

    printf("H1-2 clearing CTSF leaves a byte that has just arrived\n");
    reset_and_init(VSF_USART_RX_ENABLE | VSF_USART_CTS_HWCONTROL, 115200);
    vsf_hw_usart_irq_enable(U, VSF_USART_IRQ_MASK_CTS);
    st0 |= USART_STAT0_CTSF;
    rise_during_write = USART_STAT0_RBNE;
    pump(1000);
    CHECK(!(st0 & USART_STAT0_CTSF));
    CHECK(st0 & USART_STAT0_RBNE);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    test_cts();
    return host_summary("usart");
}
