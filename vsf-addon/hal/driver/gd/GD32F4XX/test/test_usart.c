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
fsm_rt_t vsf_hw_usart_enable(vsf_hw_usart_t *u);
void vsf_hw_usart_irq_enable(vsf_hw_usart_t *u, vsf_usart_irq_mask_t m);
uint_fast32_t vsf_hw_usart_rxfifo_read(vsf_hw_usart_t *u, void *b, uint_fast16_t n);
uint_fast32_t vsf_hw_usart_txfifo_write(vsf_hw_usart_t *u, void *b, uint_fast16_t n);
void USART0_IRQHandler(void);

static uint32_t regs[0x90 / 4];
#define CTL0            regs[0x0C / 4]
#define CTL1            regs[0x10 / 4]
#define CTL2            regs[0x14 / 4]
#define BAUD            regs[0x08 / 4]

static uint32_t st0, cell, cell_seen, rise_during_write, baud_ignored;
static int open_off = -1, st0_read;
static uint16_t rxdata;
static int callbacks;
static uint32_t last_mask;
static uint16_t tx_frames[8];           /* every bit between start and stop */
static int tx_bits[8], tx_count;

static int ones(uint32_t v)
{
    int n = 0;
    for (; v != 0; v >>= 1) {
        n += (int)(v & 1);
    }
    return n;
}

/* Section 19.3.1 and 19.4.2: with PCEN the last bit of the frame is parity. */
static void tx_frame(uint16_t v)
{
    int bits = (CTL0 & USART_CTL0_WL) ? 9 : 8;
    uint16_t frame = v & (uint16_t)((1U << bits) - 1U);
    if (CTL0 & USART_CTL0_PCEN) {
        uint16_t data = frame & (uint16_t)((1U << (bits - 1)) - 1U);
        int odd = !!(CTL0 & USART_CTL0_PM);
        frame = data | (uint16_t)(((unsigned)(ones(data) & 1) ^ (unsigned)odd) << (bits - 1));
    }
    if (tx_count < 8) {
        tx_frames[tx_count] = frame;
        tx_bits[tx_count++] = bits;
    }
}

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
        } else {
            tx_frame((uint16_t)cell);
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
    } else if ((off == 0x08U) && (CTL0 & USART_CTL0_UEN)) {
        baud_ignored = BAUD;            /* 19.4.3: not writable with UEN=1 */
        return (volatile uint32_t *)&baud_ignored;
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
    tx_count = 0;
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

/* H1-8: 8 data bits plus parity is 9 bits on the wire, so WL has to be 1 */
static void check_parity(uint32_t parity, int odd)
{
    static uint8_t out[2] = { 0xFF, 0x80 };

    printf("H1-8 8 data bits, %s parity\n", odd ? "odd" : "even");
    CHECK(reset_and_init(VSF_USART_TX_ENABLE | VSF_USART_RX_ENABLE
                         | VSF_USART_8_BIT_LENGTH | parity, 115200) == VSF_ERR_NONE);
    CHECK(CTL0 & USART_CTL0_PCEN);
    CHECK(!!(CTL0 & USART_CTL0_PM) == odd);
    CHECK(vsf_hw_usart_txfifo_write(U, out, 2) == 2);
    settle();
    CHECK(tx_count == 2);
    for (int i = 0; i < 2; i++) {
        CHECK(tx_bits[i] == 9);
        CHECK((tx_frames[i] & 0xFF) == out[i]);
        CHECK(((ones(tx_frames[i]) & 1) != 0) == odd);
    }

    /* the frame a peer sends: parity in bit 8, which the caller must not see */
    uint8_t in[3] = { 0, 0x5A, 0x5A };
    rxdata = 0x1A5;
    st0 |= USART_STAT0_RBNE;
    CHECK(vsf_hw_usart_rxfifo_read(U, in, 1) == 1);
    CHECK(in[0] == 0xA5);
    CHECK(in[1] == 0x5A);               /* one byte stored, not two */
}

static void test_parity(void)
{
    static uint16_t out9[1] = { 0x1A5 };

    check_parity(VSF_USART_EVEN_PARITY, 0);
    check_parity(VSF_USART_ODD_PARITY, 1);

    printf("H1-8 9 data bits with parity has no frame format\n");
    CHECK(reset_and_init(VSF_USART_TX_ENABLE | VSF_USART_9_BIT_LENGTH
                         | VSF_USART_EVEN_PARITY, 115200) == VSF_ERR_NOT_SUPPORT);
    CHECK(!(CTL0 & USART_CTL0_PCEN));

    printf("H1-8 no parity keeps 8 and 9 bit frames\n");
    CHECK(reset_and_init(VSF_USART_TX_ENABLE | VSF_USART_8_BIT_LENGTH, 115200) == VSF_ERR_NONE);
    CHECK(!(CTL0 & USART_CTL0_WL));
    CHECK(reset_and_init(VSF_USART_TX_ENABLE | VSF_USART_9_BIT_LENGTH, 115200) == VSF_ERR_NONE);
    CHECK(CTL0 & USART_CTL0_WL);
    CHECK(vsf_hw_usart_txfifo_write(U, out9, 1) == 1);
    settle();
    CHECK(tx_bits[0] == 9);
    CHECK(tx_frames[0] == 0x1A5);
    uint16_t in9 = 0;
    rxdata = 0x1A5;
    st0 |= USART_STAT0_RBNE;
    CHECK(vsf_hw_usart_rxfifo_read(U, &in9, 1) == 1);
    CHECK(in9 == 0x1A5);
}

/* H1-4: BAUD has to match OVSMOD. UCLK 84 MHz, 115200 baud, section 19.3.2:
 * 16x USARTDIV = 45.57 is 0x2D9, 8x USARTDIV = 91.15 is 0x5B1. */
static void test_oversample(void)
{
    vsf_usart_cfg_t cfg = { .baudrate = 115200 };

    printf("H1-4 oversample 8, then 16, with no fini\n");
    reset_and_init(VSF_USART_TX_ENABLE | VSF_USART_OVERSAMPLE_8, 115200);
    CHECK(CTL0 & USART_CTL0_OVSMOD);
    CHECK(BAUD == 0x5B1);
    cfg.mode = VSF_USART_TX_ENABLE | VSF_USART_OVERSAMPLE_16;
    CHECK(vsf_hw_usart_init(U, &cfg) == VSF_ERR_NONE);
    CHECK(!(CTL0 & USART_CTL0_OVSMOD));
    CHECK(BAUD == 0x2D9);

    printf("H1-4 init again while the USART is enabled\n");
    reset_and_init(VSF_USART_TX_ENABLE, 115200);
    vsf_hw_usart_enable(U);
    cfg.mode = VSF_USART_TX_ENABLE | VSF_USART_OVERSAMPLE_8;
    CHECK(vsf_hw_usart_init(U, &cfg) == VSF_ERR_NONE);
    CHECK(CTL0 & USART_CTL0_OVSMOD);
    CHECK(BAUD == 0x5B1);
    CHECK(CTL0 & USART_CTL0_UEN);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    test_cts();
    test_parity();
    test_oversample();
    return host_summary("usart");
}
