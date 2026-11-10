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

static uint16_t reply(uint16_t v)
{
    return (uint16_t)(v ^ 0x5A);
}

static int clocking(void)
{
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
    if (off == OFF_STAT) {
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

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    test_overrun();
    test_shapes();
    test_guard();
    return host_summary("spi");
}
