// SPDX-License-Identifier: MIT
/*
 * Runs the real spi.c on a model of the SPI data path: one TX buffer, one shift
 * register, one RX buffer, as user manual Rev3.0 (CN) sections 21.5 and 21.6.2
 * describe. On overrun the new frame is lost and the RX buffer keeps its data.
 * Time is in CPU cycles at 168 MHz: an 8-bit frame at 21 MHz SCK is 64 cycles,
 * and each read of SPI_STAT costs 20. An interrupt is a stretch of time in which
 * the driver makes no access. DATA uses a shadow cell, see test_i2c.c.
 */
#include "host.h"
#include "common/vendor/Include/gd32f4xx_spi.h"

#define S               (&vsf_hw_spi0)
#define IRQN            VSF_HW_SPI0_IRQN
#define DATA_TAG        0xA5A50000U
#define OFF_STAT        0x08U
#define OFF_DATA        0x0CU

typedef struct vsf_hw_spi_t vsf_hw_spi_t;
extern vsf_hw_spi_t vsf_hw_spi0;
vsf_err_t vsf_hw_spi_init(vsf_hw_spi_t *s, vsf_spi_cfg_t *c);
fsm_rt_t vsf_hw_spi_enable(vsf_hw_spi_t *s);
void vsf_hw_spi_fifo_transfer(vsf_hw_spi_t *s, void *out, uint_fast32_t *out_off,
                              void *in, uint_fast32_t *in_off, uint_fast32_t count);
vsf_err_t vsf_hw_spi_cs_active(vsf_hw_spi_t *s, uint_fast8_t index);
vsf_err_t vsf_hw_spi_cs_inactive(vsf_hw_spi_t *s, uint_fast8_t index);
vsf_spi_capability_t vsf_hw_spi_capability(vsf_hw_spi_t *s);
void vsf_hw_spi_irq_enable(vsf_hw_spi_t *s, vsf_spi_irq_mask_t m);
void SPI0_IRQHandler(void);

static uint32_t regs[4];
#define CTL0            regs[0]
#define CTL1            regs[1]

static uint32_t cell;
static int open_data, st_read;
static uint32_t st;
static int frame_cycles, poll_cycles = 20;
static long isr_at_poll;
static int isr_cycles;
static uint16_t txreg, shiftreg, rxreg;
static int tx_full, shift_left;
static unsigned long overruns, polls, frames;
static int nss_pin_low, conf_seen;
static uint32_t last_off;
static int callbacks;
static uint32_t last_mask;

static uint16_t reply(uint16_t v)
{
    return (uint16_t)(v ^ 0x5A);
}

/* table 21-4: a master that sees NSS low faults and turns itself into a slave */
static void check_conferr(void)
{
    if (!(CTL0 & SPI_CTL0_MSTMOD) || !(CTL0 & SPI_CTL0_SPIEN)) {
        return;
    }
    int nss_low = (CTL0 & SPI_CTL0_SWNSSEN) ? !(CTL0 & SPI_CTL0_SWNSS)
                                            : (nss_pin_low && !(CTL1 & SPI_CTL1_NSSDRV));
    if (nss_low) {
        st |= SPI_STAT_CONFERR;
        CTL0 &= ~(SPI_CTL0_SPIEN | SPI_CTL0_MSTMOD);
    }
}

static int clocking(void)
{
    check_conferr();
    return (CTL0 & SPI_CTL0_SPIEN) && (CTL0 & SPI_CTL0_MSTMOD);
}

static void run(int cycles)
{
    while (clocking() && (cycles-- > 0)) {
        if ((shift_left == 0) && tx_full) {
            shiftreg = txreg;
            tx_full = 0;
            st |= SPI_STAT_TBE;
            shift_left = frame_cycles;
        }
        if ((shift_left > 0) && (--shift_left == 0)) {
            frames++;
            if (st & SPI_STAT_RBNE) {
                overruns++;
                st |= SPI_STAT_RXORERR;
            } else {
                rxreg = reply(shiftreg);
                st |= SPI_STAT_RBNE;
            }
        }
    }
}

static void settle(void)
{
    if (open_data) {
        if ((cell >> 16) == (DATA_TAG >> 16)) {
            st &= ~SPI_STAT_RBNE;
            st_read = 1;                /* the next STAT read ends the RXORERR sequence */
        } else {
            txreg = (uint16_t)cell;
            tx_full = 1;
            st &= ~SPI_STAT_TBE;
        }
        open_data = 0;
    }
}

volatile uint32_t *sim_spi_reg(uint32_t reg, uint32_t off)
{
    (void)reg;
    settle();
    /* CONFERR clears on STAT then a write of CTL0, table 21-6. Only a write can
     * follow a read of CTL0 at once here, as in x = x. */
    if ((off == 0x00U) && (last_off == 0x00U) && conf_seen) {
        st &= ~SPI_STAT_CONFERR;
        conf_seen = 0;
    }
    last_off = off;
    if (off == OFF_STAT) {
        conf_seen = !!(st & SPI_STAT_CONFERR);
        polls++;
        if ((long)polls == isr_at_poll) {
            run(isr_cycles);
        }
        run(poll_cycles);
        cell = st;
        if (st_read) {
            st &= ~SPI_STAT_RXORERR;    /* manual: read DATA, then read STAT */
            st_read = 0;
        }
        return (volatile uint32_t *)&cell;
    }
    if (off == OFF_DATA) {
        st_read = 0;
        cell = DATA_TAG | rxreg;
        open_data = 1;
        return (volatile uint32_t *)&cell;
    }
    return (volatile uint32_t *)&regs[off / 4];
}

static void reset_spi(uint32_t extra_mode)
{
    memset(regs, 0, sizeof(regs));
    st = SPI_STAT_TBE;
    open_data = st_read = 0;
    isr_at_poll = -1;
    isr_cycles = 0;
    tx_full = shift_left = 0;
    overruns = polls = frames = 0;
    nss_pin_low = conf_seen = 0;
    last_off = 0xFF;
    callbacks = 0;
    last_mask = 0;
    frame_cycles = (extra_mode & VSF_SPI_DATASIZE_16) ? 128 : 64;
    host_asserts = 0;
    vsf_spi_cfg_t cfg = {
        .mode = VSF_SPI_MASTER | VSF_SPI_MODE_0 | VSF_SPI_CS_SOFTWARE_MODE | extra_mode,
        .clock_hz = 21000000,
    };
    vsf_hw_spi_init(S, &cfg);
    vsf_hw_spi_enable(S);
}

/* H1-16: one full-duplex transfer with one interrupt in it */
static int transfer_clean(int count, long at_poll, int cycles)
{
    static uint8_t out[64], in[64];
    uint_fast32_t to = 0, ro = 0;
    int bad = -1;

    reset_spi(0);
    isr_at_poll = at_poll;
    isr_cycles = cycles;
    for (int i = 0; i < count; i++) {
        out[i] = (uint8_t)(i + 1);
    }
    memset(in, 0xEE, sizeof(in));
    vsf_hw_spi_fifo_transfer(S, out, &to, in, &ro, (uint_fast32_t)count);
    settle();
    for (int i = 0; i < count; i++) {
        if (in[i] != (uint8_t)reply(out[i])) {
            bad = i;
            break;
        }
    }
    if (((int)to != count) || ((int)ro != count) || overruns || (bad >= 0) || (polls > 10000)) {
        printf("  interrupt of %d cycles at poll %ld: tx=%u rx=%u overruns=%lu first_bad=%d polls=%lu\n",
               cycles, at_poll, (unsigned)to, (unsigned)ro, overruns, bad, polls);
        return 0;
    }
    return 1;
}

static void test_overrun(void)
{
    static const long at[] = { 1, 5, 10, 20, 30 };
    static const int cycles[] = { 60, 120, 300, 1000, 5000 };

    printf("H1-16 16 frames, no interrupt\n");
    CHECK(transfer_clean(16, -1, 0));
    printf("  %lu polls, about %lu cycles a frame\n", polls, polls * (unsigned long)poll_cycles / 16);
    for (unsigned c = 0; c < sizeof(cycles) / sizeof(cycles[0]); c++) {
        printf("H1-16 16 frames, one interrupt of %d cycles\n", cycles[c]);
        for (unsigned a = 0; a < sizeof(at) / sizeof(at[0]); a++) {
            CHECK(transfer_clean(16, at[a], cycles[c]));
        }
    }
}

/* H1-16: 16-bit frames and the one-way calls the template allows */
static void test_shapes(void)
{
    static uint8_t out[16], in[16];
    static uint16_t out16[8], in16[8];
    uint_fast32_t to, ro;

    for (int i = 0; i < 16; i++) {
        out[i] = (uint8_t)(0x10 + i);
    }
    printf("H1-16 transmit only\n");
    reset_spi(0);
    to = 0;
    vsf_hw_spi_fifo_transfer(S, out, &to, NULL, NULL, 16);
    settle();
    CHECK(to == 16);
    CHECK(frames == 16);
    CHECK(overruns == 0);
    CHECK(!(st & SPI_STAT_RBNE));
    CHECK(host_asserts == 0);

    printf("H1-16 receive only\n");
    reset_spi(0);
    ro = 0;
    memset(in, 0, sizeof(in));
    vsf_hw_spi_fifo_transfer(S, NULL, NULL, in, &ro, 16);
    settle();
    CHECK(ro == 16);
    CHECK(frames == 16);
    CHECK(in[15] == (uint8_t)reply(0xFF));
    CHECK(host_asserts == 0);

    printf("H1-16 16-bit frames\n");
    reset_spi(VSF_SPI_DATASIZE_16);
    for (int i = 0; i < 8; i++) {
        out16[i] = (uint16_t)(0x1234 + i);
    }
    to = ro = 0;
    isr_at_poll = 7;
    isr_cycles = 1000;
    vsf_hw_spi_fifo_transfer(S, out16, &to, in16, &ro, 8);
    settle();
    CHECK(ro == 8);
    CHECK(overruns == 0);
    for (int i = 0; i < 8; i++) {
        CHECK(in16[i] == reply(out16[i]));
    }
}

/* H1-16: with the SPI off the guard fires, and the caller sees it and can resume */
static void test_guard(void)
{
    static uint8_t out[4] = { 1, 2, 3, 4 }, in[4];
    uint_fast32_t to = 0, ro = 0;

    printf("H1-16 SPI off, then enabled and the call repeated\n");
    reset_spi(0);
    CTL0 &= ~SPI_CTL0_SPIEN;
    st |= SPI_STAT_RXORERR;
    vsf_hw_spi_fifo_transfer(S, out, &to, in, &ro, 4);
    settle();
    CHECK(ro < 4);
    CHECK(!(st & SPI_STAT_RXORERR));
    vsf_hw_spi_enable(S);
    vsf_hw_spi_fifo_transfer(S, out, &to, in, &ro, 4);
    settle();
    CHECK(to == 4);
    CHECK(ro == 4);
    CHECK(frames == 4);
    for (int i = 0; i < 4; i++) {
        CHECK(in[i] == (uint8_t)reply(out[i]));
    }

    printf("H1-16 transmit only, resumed\n");
    reset_spi(0);
    CTL0 &= ~SPI_CTL0_SPIEN;
    to = 0;
    vsf_hw_spi_fifo_transfer(S, out, &to, NULL, NULL, 4);
    vsf_hw_spi_enable(S);
    vsf_hw_spi_fifo_transfer(S, out, &to, NULL, NULL, 4);
    settle();
    CHECK(to == 4);
    CHECK(polls < 150000);
}

/* H1-17: master has no CS line to drive. SWNSS low would raise CONFERR (21.5.2). */
static void test_cs_master(void)
{
    printf("H1-17 master: no CS line, and the capability says so\n");
    reset_spi(0);
    CHECK(vsf_hw_spi_capability(S).cs_count == 0);
    CHECK(vsf_hw_spi_capability(S).support_software_cs == 1);
    CHECK(vsf_hw_spi_cs_active(S, 0) == VSF_ERR_NOT_SUPPORT);
    CHECK(CTL0 & SPI_CTL0_SWNSS);
    CHECK(vsf_hw_spi_cs_inactive(S, 0) == VSF_ERR_NOT_SUPPORT);
    CHECK(CTL0 & SPI_CTL0_SWNSS);
}

/* H1-17: a software-NSS slave is selected by SWNSS low only, table 21-3 */
static void test_cs_slave(void)
{
    vsf_spi_cfg_t cfg = {
        .mode = VSF_SPI_SLAVE | VSF_SPI_MODE_0 | VSF_SPI_CS_SOFTWARE_MODE,
        .clock_hz = 1000000,
    };

    printf("H1-17 slave: cs_active drives SWNSS\n");
    memset(regs, 0, sizeof(regs));
    CHECK(vsf_hw_spi_init(S, &cfg) == VSF_ERR_NONE);
    CHECK(CTL0 & SPI_CTL0_SWNSSEN);
    CHECK(CTL0 & SPI_CTL0_SWNSS);       /* deselected until asked */
    CHECK(vsf_hw_spi_capability(S).cs_count == 1);
    CHECK(vsf_hw_spi_cs_active(S, 0) == VSF_ERR_NONE);
    CHECK(!(CTL0 & SPI_CTL0_SWNSS));
    CHECK(vsf_hw_spi_cs_inactive(S, 0) == VSF_ERR_NONE);
    CHECK(CTL0 & SPI_CTL0_SWNSS);
    CHECK(vsf_hw_spi_cs_active(S, 1) == VSF_ERR_NOT_SUPPORT);
    CHECK(CTL0 & SPI_CTL0_SWNSS);
}

static void on_irq(void *t, vsf_spi_t *p, vsf_spi_irq_mask_t m)
{
    (void)t; (void)p;
    callbacks++;
    last_mask = (uint32_t)m;
}

/* H1-18: the template's zero is software CS, and capability has no hardware CS */
static void test_cs_mode(void)
{
    static uint8_t out[4] = { 1, 2, 3, 4 }, in[4];
    uint_fast32_t to = 0, ro = 0;

    printf("H1-18 master with no CS flag and the NSS pin low\n");
    reset_spi(0);
    vsf_spi_cfg_t cfg = { .mode = VSF_SPI_MASTER | VSF_SPI_MODE_0, .clock_hz = 21000000 };
    nss_pin_low = 1;
    CHECK(vsf_hw_spi_init(S, &cfg) == VSF_ERR_NONE);
    vsf_hw_spi_enable(S);
    CHECK(CTL0 & SPI_CTL0_SWNSSEN);
    CHECK(CTL0 & SPI_CTL0_SWNSS);
    vsf_hw_spi_fifo_transfer(S, out, &to, in, &ro, 4);
    settle();
    CHECK(!(st & SPI_STAT_CONFERR));
    CHECK(CTL0 & SPI_CTL0_MSTMOD);
    CHECK(ro == 4);

    printf("H1-18 hardware CS is refused\n");
    memset(regs, 0, sizeof(regs));
    host_asserts = 0;
    cfg.mode = VSF_SPI_MASTER | VSF_SPI_MODE_0 | VSF_SPI_CS_HARDWARE_MODE;
    CHECK(vsf_hw_spi_init(S, &cfg) == VSF_ERR_NOT_SUPPORT);
    CHECK(CTL0 == 0);
    CHECK(host_asserts == 0);
    CHECK(vsf_hw_spi_capability(S).support_hardware_cs == 0);
    cfg.mode = VSF_SPI_MASTER | VSF_SPI_MODE_0 | VSF_SPI_CS_HARDWARE_INPUT_MODE;
    CHECK(vsf_hw_spi_init(S, &cfg) == VSF_ERR_NOT_SUPPORT);
    CHECK(CTL0 == 0);

    printf("H1-18 a slave can still take select from the NSS pin\n");
    cfg.mode = VSF_SPI_SLAVE | VSF_SPI_MODE_0 | VSF_SPI_CS_HARDWARE_INPUT_MODE;
    CHECK(vsf_hw_spi_init(S, &cfg) == VSF_ERR_NONE);
    CHECK(!(CTL0 & SPI_CTL0_SWNSSEN));
    CHECK(vsf_hw_spi_capability(S).cs_count == 0);
    CHECK(vsf_hw_spi_cs_active(S, 0) == VSF_ERR_NOT_SUPPORT);
}

/* H1-18: CONFERR raises the ERRIE interrupt and only the sequence clears it */
static void test_conferr_irq(void)
{
    long n = 0;

    printf("H1-18 CONFERR is reported and cleared\n");
    reset_spi(0);
    vsf_spi_cfg_t cfg = { .mode = VSF_SPI_MASTER | VSF_SPI_MODE_0, .clock_hz = 21000000 };
    cfg.isr.handler_fn = on_irq;
    vsf_hw_spi_init(S, &cfg);
    vsf_hw_spi_enable(S);
    vsf_hw_spi_irq_enable(S, VSF_SPI_IRQ_MASK_RX_OVERFLOW_ERR);
    st |= SPI_STAT_CONFERR;
    CTL0 &= ~(SPI_CTL0_SPIEN | SPI_CTL0_MSTMOD);
    while ((n < 1000) && (st & SPI_STAT_CONFERR) && (CTL1 & SPI_CTL1_ERRIE) && host_nvic_on[IRQN]) {
        SPI0_IRQHandler();
        settle();
        n++;
    }
    printf("  interrupt entries: %ld\n", n);
    CHECK(n == 1);
    CHECK(callbacks == 1);
    CHECK(last_mask == VSF_SPI_IRQ_MASK_CONFIG_ERR);
    CHECK(vsf_hw_spi_capability(S).irq_mask & VSF_SPI_IRQ_MASK_CONFIG_ERR);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    test_overrun();
    test_shapes();
    test_guard();
    test_cs_master();
    test_cs_slave();
    test_cs_mode();
    test_conferr_irq();
    return host_summary("spi");
}
