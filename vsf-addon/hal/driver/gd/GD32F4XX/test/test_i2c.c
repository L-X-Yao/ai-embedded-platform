// SPDX-License-Identifier: MIT
/*
 * Runs the real i2c.c on a model of the GD32F4 I2C master, written from user
 * manual Rev3.0 (CN) sections 20.3.7, 20.3.8 and 20.4.
 *
 * Register accesses reach the model through sim_i2c_reg(). C cannot tell a read
 * from a write there, so DATA and STAT0 hand out a shadow cell and the access is
 * settled at the next register access or bus step: a DATA cell that still holds
 * its tag was read, otherwise it was written. Order is kept.
 *
 * ACK: with POAP=0 ACKEN at the end of a byte decides that byte. With POAP=1
 * ACKEN decides the next byte, so it is latched at the end of the byte before,
 * the address byte counting as one.
 */
#include "host.h"
#include "common/vendor/Include/gd32f4xx_i2c.h"

#define I2C             (&vsf_hw_i2c0)
#define EV_IRQN         VSF_HW_I2C0_EV_IRQN
#define ER_IRQN         VSF_HW_I2C0_ER_IRQN
#define DATA_TAG        0xA5A50000U
#define OFF_DATA        0x10U
#define OFF_STAT0       0x14U
#define OFF_STAT1       0x18U
#define RC_W0_FLAGS     (I2C_STAT0_BERR | I2C_STAT0_LOSTARB | I2C_STAT0_AERR | I2C_STAT0_OUERR)

typedef struct vsf_hw_i2c_t vsf_hw_i2c_t;
extern vsf_hw_i2c_t vsf_hw_i2c0;
vsf_err_t vsf_hw_i2c_init(vsf_hw_i2c_t *p, vsf_i2c_cfg_t *c);
vsf_err_t vsf_hw_i2c_master_request(vsf_hw_i2c_t *p, uint16_t addr, vsf_i2c_cmd_t cmd,
                                    uint_fast16_t count, uint8_t *buf);
uint_fast16_t vsf_hw_i2c_master_get_transferred_count(vsf_hw_i2c_t *p);
void I2C0_EV_IRQHandler(void);
void I2C0_ER_IRQHandler(void);

static uint32_t regs[0x30 / 4];
#define CTL0            regs[0]
#define CTL1            regs[1]

static uint32_t st0, cell, cell_seen, st1_cell;
static int open_off = -1, st0_read;

enum { IDLE, HOLD, ADDR, ADDRLOW, RX, TX } phase;
static uint8_t addrbyte, dreg, shiftr, tx_dreg, next_b;
static int shift_full, tx_shift, tx_dreg_full, is_read, rx_inflight, ack_next, nacked;
static int slave_absent;
static char wire[1024];
static int stops, starts, callbacks;
static uint32_t last_mask;

/* what the driver did, for the N=2 steps of the manual */
static int poap_at_start, acken_at_addclr, btc_at_first_read, stop_at_first_read, data_reads;

static void logw(const char *fmt, int v)
{
    char b[32];
    snprintf(b, sizeof(b), fmt, v);
    strncat(wire, b, sizeof(wire) - strlen(wire) - 1);
}

static void data_read_effect(void)
{
    if (data_reads++ == 0) {
        btc_at_first_read = !!(st0 & I2C_STAT0_BTC);
        stop_at_first_read = !!(CTL0 & I2C_CTL0_STOP);
    }
    if (shift_full) {
        dreg = shiftr;
        shift_full = 0;
        if (st0_read) {
            st0 &= ~I2C_STAT0_BTC;
        }
    } else {
        st0 &= ~I2C_STAT0_RBNE;
    }
    st0_read = 0;
}

static void data_write_effect(uint8_t v)
{
    if ((st0 & I2C_STAT0_SBSEND) && st0_read) {
        st0 &= ~I2C_STAT0_SBSEND;
        addrbyte = v;
        phase = ADDR;
    } else if ((st0 & I2C_STAT0_ADD10SEND) && st0_read) {
        st0 &= ~I2C_STAT0_ADD10SEND;
        addrbyte = v;
        phase = ADDRLOW;
    } else {
        st0 &= ~I2C_STAT0_BTC;
        if (!tx_shift) {
            shiftr = v;
            tx_shift = 1;
        } else {
            tx_dreg = v;
            tx_dreg_full = 1;
            st0 &= ~I2C_STAT0_TBE;
        }
    }
    st0_read = 0;
}

static void settle(void)
{
    if (open_off == (int)OFF_STAT0) {
        if (cell != cell_seen) {
            st0 &= cell | ~RC_W0_FLAGS;
        }
        st0_read = 1;
    } else if (open_off == (int)OFF_DATA) {
        if ((cell >> 16) == (DATA_TAG >> 16)) {
            data_read_effect();
        } else {
            data_write_effect((uint8_t)cell);
        }
    }
    open_off = -1;
}

volatile uint32_t *sim_i2c_reg(uint32_t reg, uint32_t off)
{
    (void)reg;
    settle();
    switch (off) {
    case OFF_STAT0:
        cell = cell_seen = st0;
        open_off = (int)off;
        return (volatile uint32_t *)&cell;
    case OFF_DATA:
        cell = DATA_TAG | dreg;
        open_off = (int)off;
        return (volatile uint32_t *)&cell;
    case OFF_STAT1:
        if ((st0 & I2C_STAT0_ADDSEND) && st0_read) {
            st0 &= ~I2C_STAT0_ADDSEND;
            st0_read = 0;
            acken_at_addclr = !!(CTL0 & I2C_CTL0_ACKEN);
            phase = is_read ? RX : TX;
            rx_inflight = is_read;      /* clearing ADDSEND frees SCL */
            if (!is_read) {
                st0 |= I2C_STAT0_TBE;
            }
        }
        st1_cell = 0;
        return (volatile uint32_t *)&st1_cell;
    default:
        return (volatile uint32_t *)&regs[off / 4];
    }
}

static void stop_now(void)
{
    CTL0 &= ~I2C_CTL0_STOP;
    st0 &= ~(I2C_STAT0_BTC | I2C_STAT0_TBE);
    logw(" P", 0);
    stops++;
    phase = IDLE;
    rx_inflight = 0;
}

static void start_now(void)
{
    CTL0 &= ~I2C_CTL0_START;
    st0 |= I2C_STAT0_SBSEND;
    st0 &= ~(I2C_STAT0_BTC | I2C_STAT0_TBE);
    poap_at_start = !!(CTL0 & I2C_CTL0_POAP);
    logw(" S", 0);
    starts++;
    phase = HOLD;
    tx_shift = tx_dreg_full = 0;
}

static void rx_end_of_byte(void)
{
    uint8_t b = nacked ? 0xFF : next_b++;
    int acken = !!(CTL0 & I2C_CTL0_ACKEN);
    int ack = (CTL0 & I2C_CTL0_POAP) ? ack_next : acken;

    ack_next = acken;
    logw(" rx=%02X", b);
    logw(ack ? "+ack" : "+NACK", 0);
    if (!ack) {
        nacked = 1;
    }
    if (st0 & I2C_STAT0_RBNE) {
        shiftr = b;
        shift_full = 1;
        st0 |= I2C_STAT0_BTC;
    } else {
        dreg = b;
        st0 |= I2C_STAT0_RBNE;
    }
    rx_inflight = !(st0 & I2C_STAT0_BTC);   /* the next byte follows unless SCL is held */
}

/* One bus event. Returns 0 when the bus cannot move until software acts. */
static int bus_step(void)
{
    settle();
    int between = (phase != RX) || !rx_inflight;
    if (between && (CTL0 & I2C_CTL0_START) && !(st0 & I2C_STAT0_SBSEND)) {
        start_now();
        return 1;
    }
    if (between && (CTL0 & I2C_CTL0_STOP) && (phase != ADDR) && (phase != ADDRLOW)) {
        if ((phase == TX) && tx_shift) {
            /* the byte in the shift register goes first */
        } else {
            stop_now();
            return 1;
        }
    }
    switch (phase) {
    case ADDR:
        logw(" addr=%02X", addrbyte);
        if (slave_absent) {
            st0 |= I2C_STAT0_AERR;
            phase = HOLD;
            return 1;
        }
        ack_next = !!(CTL0 & I2C_CTL0_ACKEN);
        if (((addrbyte & 0xF9) == 0xF0) && !(addrbyte & 1)) {
            st0 |= I2C_STAT0_ADD10SEND;
        } else {
            is_read = addrbyte & 1;
            st0 |= I2C_STAT0_ADDSEND;
        }
        phase = HOLD;
        return 1;
    case ADDRLOW:
        logw(" low=%02X", addrbyte);
        ack_next = !!(CTL0 & I2C_CTL0_ACKEN);
        is_read = 0;
        st0 |= I2C_STAT0_ADDSEND;
        phase = HOLD;
        return 1;
    case RX:
        if (rx_inflight) {
            rx_end_of_byte();
            if (CTL0 & I2C_CTL0_STOP) {
                stop_now();
            }
            return 1;
        }
        if (st0 & I2C_STAT0_BTC) {
            return 0;                   /* SCL held, 20.3.8 */
        }
        rx_inflight = 1;
        return 1;
    case TX:
        if (!tx_shift) {
            return 0;
        }
        logw(" tx=%02X", shiftr);
        tx_shift = 0;
        if (tx_dreg_full) {
            shiftr = tx_dreg;
            tx_shift = 1;
            tx_dreg_full = 0;
            st0 |= I2C_STAT0_TBE;
        } else {
            st0 |= I2C_STAT0_TBE | I2C_STAT0_BTC;
        }
        return 1;
    default:
        return 0;
    }
}

static int ev_pending(void)
{
    settle();
    if (!host_nvic_on[EV_IRQN] || !(CTL1 & I2C_CTL1_EVIE)) {
        return 0;
    }
    if (st0 & (I2C_STAT0_SBSEND | I2C_STAT0_ADDSEND | I2C_STAT0_ADD10SEND | I2C_STAT0_BTC)) {
        return 1;
    }
    return (CTL1 & I2C_CTL1_BUFIE) && (st0 & (I2C_STAT0_TBE | I2C_STAT0_RBNE));
}

static int er_pending(void)
{
    settle();
    return host_nvic_on[ER_IRQN] && (CTL1 & I2C_CTL1_ERRIE) && (st0 & RC_W0_FLAGS);
}

static int storm;
static long isr_calls;

static void run_bus(void)
{
    long stuck = 0, burst = 0;
    storm = 0;
    for (long i = 0; i < 2000000; i++) {
        if (er_pending()) {
            I2C0_ER_IRQHandler();
            isr_calls++;
            continue;
        }
        if (ev_pending() && (burst < 30)) {
            I2C0_EV_IRQHandler();
            isr_calls++;
            burst++;
            continue;
        }
        burst = 0;
        if (bus_step()) {
            stuck = 0;
            continue;
        }
        if (ev_pending()) {
            I2C0_EV_IRQHandler();
            isr_calls++;
            if (++stuck > 20000) {
                storm = 1;
                return;
            }
            continue;
        }
        return;
    }
    storm = 1;
}

/* the next request, sent from the callback as vsf_i2c_regacc does */
static struct { int armed; uint16_t addr; vsf_i2c_cmd_t cmd; uint16_t count; uint8_t *buf; } chain;
static vsf_err_t chain_ret;

static void on_irq(void *t, vsf_i2c_t *p, vsf_i2c_irq_mask_t m)
{
    (void)t; (void)p;
    callbacks++;
    last_mask = (uint32_t)m;
    if (chain.armed) {
        chain.armed = 0;
        chain_ret = vsf_hw_i2c_master_request(I2C, chain.addr, chain.cmd, chain.count, chain.buf);
    }
}

static void bus_reset(int with_handler)
{
    memset(regs, 0, sizeof(regs));
    memset(&chain, 0, sizeof(chain));
    st0 = 0;
    open_off = -1;
    st0_read = 0;
    phase = IDLE;
    shift_full = tx_shift = tx_dreg_full = rx_inflight = nacked = slave_absent = 0;
    next_b = 0xA1;
    wire[0] = 0;
    stops = starts = callbacks = 0;
    last_mask = 0;
    chain_ret = VSF_ERR_NONE;
    poap_at_start = acken_at_addclr = btc_at_first_read = stop_at_first_read = data_reads = 0;
    isr_calls = 0;
    host_asserts = 0;
    vsf_i2c_cfg_t cfg = { .mode = VSF_I2C_MODE_MASTER, .clock_hz = 100000 };
    if (with_handler) {
        cfg.isr.handler_fn = on_irq;
    }
    vsf_hw_i2c_init(I2C, &cfg);
}

static const vsf_i2c_cmd_t RD = VSF_I2C_CMD_READ | VSF_I2C_CMD_START | VSF_I2C_CMD_STOP;
static const vsf_i2c_cmd_t WR = VSF_I2C_CMD_WRITE | VSF_I2C_CMD_START | VSF_I2C_CMD_STOP;
#define DONE_OK     VSF_I2C_IRQ_MASK_MASTER_TRANSFER_COMPLETE

/* the wire a good N-byte read gives: ACK on all but the last byte, then STOP */
static void expect_read_wire(char *out, size_t cap, const char *prefix, int n, int first)
{
    size_t len = (size_t)snprintf(out, cap, "%s", prefix);
    for (int i = 0; i < n; i++) {
        len += (size_t)snprintf(out + len, cap - len, " rx=%02X+%s", first + i,
                                i == n - 1 ? "NACK" : "ack");
    }
    snprintf(out + len, cap - len, " P");
}

static void check_read(int n)
{
    static uint8_t b[16];
    static char want[256];

    printf("H1-20 read %d bytes\n", n);
    memset(b, 0, sizeof(b));
    bus_reset(1);
    CHECK(vsf_hw_i2c_master_request(I2C, 0x50, RD, (uint_fast16_t)n, b) == VSF_ERR_NONE);
    run_bus();
    expect_read_wire(want, sizeof(want), " S addr=A1", n, 0xA1);
    if (strcmp(wire, want) != 0) {
        printf("  wire:%s\n  want:%s\n", wire, want);
    }
    CHECK(!storm);
    CHECK(strcmp(wire, want) == 0);
    for (int i = 0; i < n; i++) {
        CHECK(b[i] == 0xA1 + i);
    }
    CHECK(callbacks == 1);
    CHECK(last_mask == DONE_OK);
    CHECK(stops == 1);
    CHECK(!(st0 & I2C_STAT0_BTC));
    CHECK(!(st0 & I2C_STAT0_RBNE));
    CHECK(vsf_hw_i2c_master_get_transferred_count(I2C) == (uint_fast16_t)n);
}

/* H1-20: the four steps of the manual's N=2 case, section 20.3.7 scheme B */
static void test_n2_steps(void)
{
    static uint8_t b[2];

    printf("H1-20 N=2 follows the manual's order\n");
    bus_reset(1);
    CHECK(vsf_hw_i2c_master_request(I2C, 0x50, RD, 2, b) == VSF_ERR_NONE);
    run_bus();
    CHECK(poap_at_start == 1);          /* POAP set before START */
    CHECK(acken_at_addclr == 0);        /* ACKEN clear before ADDSEND is cleared */
    CHECK(btc_at_first_read == 1);      /* waited for BTC */
    CHECK(stop_at_first_read == 1);     /* STOP set, then read */
    CHECK(data_reads == 2);             /* DATA read twice */
}

/* H1-20: register read as vsf_i2c_regacc does it: write, then RESTART read */
static void check_regacc_read(int n)
{
    static uint8_t reg = 0x10, b[16];
    static char want[256];

    printf("H1-20 register read of %d bytes after a RESTART\n", n);
    memset(b, 0, sizeof(b));
    bus_reset(1);
    chain.armed = 1;
    chain.addr = 0x50;
    chain.cmd = VSF_I2C_CMD_RESTART | VSF_I2C_CMD_READ | VSF_I2C_CMD_STOP;
    chain.count = (uint16_t)n;
    chain.buf = b;
    CHECK(vsf_hw_i2c_master_request(I2C, 0x50, VSF_I2C_CMD_START | VSF_I2C_CMD_WRITE, 1, &reg) == VSF_ERR_NONE);
    run_bus();
    expect_read_wire(want, sizeof(want), " S addr=A0 tx=10 S addr=A1", n, 0xA1);
    if (strcmp(wire, want) != 0) {
        printf("  wire:%s\n  want:%s\n", wire, want);
    }
    CHECK(chain_ret == VSF_ERR_NONE);
    CHECK(strcmp(wire, want) == 0);
    for (int i = 0; i < n; i++) {
        CHECK(b[i] == 0xA1 + i);
    }
    CHECK(callbacks == 2);
    CHECK(last_mask == DONE_OK);
}

/* H1-20: POAP from a 2-byte read must be gone before the next START */
static void test_after_n2(void)
{
    static const int next[] = { 1, 3 };
    static uint8_t b[4];
    static char want[256];

    for (unsigned i = 0; i < 2; i++) {
        printf("H1-20 read of %d bytes after a 2-byte read\n", next[i]);
        bus_reset(1);
        CHECK(vsf_hw_i2c_master_request(I2C, 0x50, RD, 2, b) == VSF_ERR_NONE);
        run_bus();
        CHECK(poap_at_start == 1);
        wire[0] = 0;
        next_b = 0xB1;
        nacked = 0;
        CHECK(vsf_hw_i2c_master_request(I2C, 0x50, RD, (uint_fast16_t)next[i], b) == VSF_ERR_NONE);
        run_bus();
        expect_read_wire(want, sizeof(want), " S addr=A1", next[i], 0xB1);
        CHECK(poap_at_start == 0);
        CHECK(strcmp(wire, want) == 0);
        CHECK(b[0] == 0xB1);
    }
}

/* H1-21: header is 11110 A9 A8 R/W, then the low address byte, section 20.3.7 */
static void test_write_10bit(void)
{
    static const struct { uint16_t addr; const char *want; } t[] = {
        { 0x3FF, " S addr=F6 low=FF tx=33 P" },
        { 0x155, " S addr=F2 low=55 tx=33 P" },
        { 0x2AA, " S addr=F4 low=AA tx=33 P" },
        { 0x000, " S addr=F0 low=00 tx=33 P" },
    };
    static uint8_t b[1] = { 0x33 };

    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        printf("H1-21 write to 10-bit address 0x%03X\n", t[i].addr);
        bus_reset(1);
        CHECK(vsf_hw_i2c_master_request(I2C, t[i].addr, WR | VSF_I2C_CMD_10_BITS, 1, b) == VSF_ERR_NONE);
        run_bus();
        if (strcmp(wire, t[i].want) != 0) {
            printf("  wire:%s\n  want:%s\n", wire, t[i].want);
        }
        CHECK(!storm);
        CHECK(strcmp(wire, t[i].want) == 0);
        CHECK(callbacks == 1);
        CHECK(last_mask == DONE_OK);
    }
}

/* H1-21: a 10-bit read addresses for write, then repeats START, header with R */
static void check_read_10bit(int n)
{
    static uint8_t b[8];
    static char want[256];

    printf("H1-21 read %d bytes from 10-bit address 0x3FF\n", n);
    memset(b, 0, sizeof(b));
    bus_reset(1);
    CHECK(vsf_hw_i2c_master_request(I2C, 0x3FF, RD | VSF_I2C_CMD_10_BITS, (uint_fast16_t)n, b) == VSF_ERR_NONE);
    run_bus();
    expect_read_wire(want, sizeof(want), " S addr=F6 low=FF S addr=F7", n, 0xA1);
    if (strcmp(wire, want) != 0) {
        printf("  wire:%s\n  want:%s\n", wire, want);
    }
    CHECK(!storm);
    CHECK(strcmp(wire, want) == 0);
    for (int i = 0; i < n; i++) {
        CHECK(b[i] == 0xA1 + i);
    }
    CHECK(callbacks == 1);
    CHECK(last_mask == DONE_OK);
    CHECK(stops == 1);
}

/* writes were right before this work: they must stay right */
static void test_write(void)
{
    static uint8_t b[3] = { 0x11, 0x22, 0x33 };

    printf("write 3 bytes\n");
    bus_reset(1);
    CHECK(vsf_hw_i2c_master_request(I2C, 0x50, WR, 3, b) == VSF_ERR_NONE);
    run_bus();
    CHECK(strcmp(wire, " S addr=A0 tx=11 tx=22 tx=33 P") == 0);
    CHECK(callbacks == 1);
    CHECK(last_mask == DONE_OK);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (int n = 1; n <= 5; n++) {
        check_read(n);
    }
    test_n2_steps();
    for (int n = 1; n <= 3; n++) {
        check_regacc_read(n);
    }
    test_after_n2();
    test_write();
    test_write_10bit();
    for (int n = 1; n <= 4; n++) {
        check_read_10bit(n);
    }
    return host_summary("i2c");
}
