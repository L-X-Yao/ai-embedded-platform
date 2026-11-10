// SPDX-License-Identifier: MIT
/*
 * Runs the real flash.c on a model of the FMC. Flash is a 1 MB mapping at the
 * chip's address. Word programming and sector erase follow user manual Rev3.0
 * sections 2.3.5 and 2.3.7: END and the interrupt come only with ENDIE set.
 */
#include <sys/mman.h>

#include "host.h"
#include "common/vendor/Include/gd32f4xx_fmc.h"

#define FLASH_IRQN     VSF_HW_FLASH0_IRQN
#define FLASH_SIZE      0x100000U
#define FLASH_MEM       ((uint8_t *)0x08000000UL)
#define F               (&vsf_hw_flash0)

typedef struct vsf_hw_flash_t vsf_hw_flash_t;
extern vsf_hw_flash_t vsf_hw_flash0;
vsf_err_t vsf_hw_flash_init(vsf_hw_flash_t *f, vsf_flash_cfg_t *c);
vsf_err_t vsf_hw_flash_erase_one_sector(vsf_hw_flash_t *f, vsf_flash_size_t off);
vsf_err_t vsf_hw_flash_erase_multi_sector(vsf_hw_flash_t *f, vsf_flash_size_t off, vsf_flash_size_t size);
vsf_err_t vsf_hw_flash_write_multi_sector(vsf_hw_flash_t *f, vsf_flash_size_t off, uint8_t *b, vsf_flash_size_t size);
void vsf_hw_flash_irq_enable(vsf_hw_flash_t *f, vsf_flash_irq_mask_t m);
void vsf_hw_flash_irq_disable(vsf_hw_flash_t *f, vsf_flash_irq_mask_t m);
vsf_err_t vsf_hw_flash_read_multi_sector(vsf_hw_flash_t *f, vsf_flash_size_t off, uint8_t *b, vsf_flash_size_t size);
vsf_flash_status_t vsf_hw_flash_status(vsf_hw_flash_t *f);
void FMC_IRQHandler(void);

volatile uint32_t sim_fmc_ctl = FMC_CTL_LK, sim_fmc_stat, sim_fmc_key;

static uint32_t buf[0x8000 + 8];

/* user manual table 2-1, bank 0 */
static const struct { uint32_t base, size; } sectors[12] = {
    { 0x00000, 0x04000 }, { 0x04000, 0x04000 }, { 0x08000, 0x04000 }, { 0x0C000, 0x04000 },
    { 0x10000, 0x10000 }, { 0x20000, 0x20000 }, { 0x40000, 0x20000 }, { 0x60000, 0x20000 },
    { 0x80000, 0x20000 }, { 0xA0000, 0x20000 }, { 0xC0000, 0x20000 }, { 0xE0000, 0x20000 },
};

static int calls;
static uint32_t last_mask;
static uint32_t words, erased_mask;
/* a write started from the completion callback, as a chained caller does */
static struct { int armed; uint32_t off; vsf_err_t ret; } chain;

static void on_irq(void *t, vsf_flash_t *f, vsf_flash_irq_mask_t m)
{
    (void)t; (void)f;
    calls++;
    last_mask = (uint32_t)m;
    if (chain.armed) {
        chain.armed = 0;
        chain.ret = vsf_hw_flash_write_multi_sector(F, chain.off, (uint8_t *)buf, 8);
    }
}

static void reset_sim(int with_handler, int with_irq_enable)
{
    memset(FLASH_MEM, 0xFF, FLASH_SIZE);
    sim_fmc_ctl = FMC_CTL_LK;
    sim_fmc_stat = 0;
    calls = 0;
    last_mask = 0;
    words = 0;
    erased_mask = 0;
    host_asserts = 0;
    vsf_hw_flash_irq_disable(F, VSF_FLASH_IRQ_ALL_BITS_MASK);
    vsf_flash_cfg_t cfg = { 0 };
    if (with_handler) {
        cfg.isr.handler_fn = on_irq;
    }
    vsf_hw_flash_init(F, &cfg);
    sim_fmc_ctl &= ~FMC_CTL_LK;         /* the key sequence unlocks the chip */
    if (with_irq_enable) {
        vsf_hw_flash_irq_enable(F, VSF_FLASH_IRQ_ERASE_MASK | VSF_FLASH_IRQ_WRITE_MASK);
    }
}

/* Ends the operation in progress and any that the interrupt starts after it. */
static void fmc_run(uint32_t limit)
{
    while (limit-- > 0) {
        uint32_t ctl = sim_fmc_ctl;
        if (ctl & FMC_CTL_START) {
            uint32_t sn = (ctl & FMC_CTL_SN) >> 3;
            CHECK(sn < 12);
            sim_fmc_ctl &= ~FMC_CTL_START;
            if (sn < 12) {
                memset(FLASH_MEM + sectors[sn].base, 0xFF, sectors[sn].size);
                erased_mask |= 1U << sn;
            }
        } else if (ctl & FMC_CTL_PG) {
            words++;
        } else {
            return;
        }
        if (!(ctl & FMC_CTL_ENDIE) || !host_nvic_on[FLASH_IRQN]) {
            return;
        }
        sim_fmc_stat = FMC_STAT_END;
        FMC_IRQHandler();
        sim_fmc_stat &= ~FMC_STAT_END;
    }
}

static uint32_t word_at(uint32_t off)
{
    return *(uint32_t *)(FLASH_MEM + off);
}

/* H1-9: size 0 or an unaligned request must be refused before anything moves. */
static void test_write_bad_size(void)
{
    static const struct { uint32_t off, size; } bad[] = {
        { 0xE0000, 0 }, { 0xE0000, 6 }, { 0xE0000, 1 }, { 0xE0002, 8 },
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        printf("H1-9 write offset 0x%x size %u\n", (unsigned)bad[i].off, (unsigned)bad[i].size);
        reset_sim(1, 1);
        vsf_err_t e = vsf_hw_flash_write_multi_sector(F, bad[i].off, (uint8_t *)buf, bad[i].size);
        fmc_run(0x4000);                /* stays inside the mapping if it runs away */
        CHECK(e == VSF_ERR_INVALID_PARAMETER);
        CHECK(words == 0);
        CHECK(word_at(0xE0000) == 0xFFFFFFFFU);
        CHECK(!(sim_fmc_ctl & FMC_CTL_PG));
        CHECK(host_asserts == 0);
    }
}

/* H1-9: nothing past the last word of the 1 MB the capability reports. */
static void test_write_range(void)
{
    printf("H1-9 write past the end of flash\n");
    reset_sim(1, 1);
    CHECK(vsf_hw_flash_write_multi_sector(F, FLASH_SIZE - 4, (uint8_t *)buf, 8) == VSF_ERR_INVALID_RANGE);
    CHECK(vsf_hw_flash_write_multi_sector(F, FLASH_SIZE, (uint8_t *)buf, 4) == VSF_ERR_INVALID_RANGE);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0xFFFFFFFCU, (uint8_t *)buf, 8) == VSF_ERR_INVALID_RANGE);
    CHECK(!(sim_fmc_ctl & FMC_CTL_PG));
    CHECK(vsf_hw_flash_write_multi_sector(F, FLASH_SIZE - 4, (uint8_t *)buf, 4) == VSF_ERR_NONE);
    fmc_run(4);
    CHECK(word_at(FLASH_SIZE - 4) == buf[0]);
    CHECK(calls == 1);
}

/* H1-9: a good write still lands word for word and ends in one callback. */
static void test_write_ok(void)
{
    printf("H1-9 write 256 bytes at 0x20000\n");
    reset_sim(1, 1);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x20000, (uint8_t *)buf, 256) == VSF_ERR_NONE);
    fmc_run(0x8000);
    CHECK(words == 64);
    CHECK(memcmp(FLASH_MEM + 0x20000, buf, 256) == 0);
    CHECK(word_at(0x20000 + 256) == 0xFFFFFFFFU);
    CHECK(calls == 1);
    CHECK(last_mask == VSF_FLASH_IRQ_WRITE_MASK);
    CHECK(!(sim_fmc_ctl & FMC_CTL_PG));
    CHECK(host_asserts == 0);
}

/* H1-9: an END with nothing running must not take size_cur below zero. */
static void test_stray_end(void)
{
    printf("H1-9 stray END after a finished write\n");
    reset_sim(1, 1);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x20000, (uint8_t *)buf, 8) == VSF_ERR_NONE);
    fmc_run(0x8000);
    CHECK(calls == 1);
    sim_fmc_stat = FMC_STAT_END;
    FMC_IRQHandler();
    sim_fmc_stat = 0;
    CHECK(calls == 1);
    CHECK(!(sim_fmc_ctl & FMC_CTL_PG));
    CHECK(word_at(0x20000 + 8) == 0xFFFFFFFFU);
    CHECK(host_asserts == 0);
}

/* H1-10: only the interrupt moves a request on, so no armed interrupt, no request */
static void check_not_armed(int with_handler, int with_irq_enable, const char *name)
{
    printf("H1-10 %s\n", name);
    reset_sim(with_handler, with_irq_enable);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x20000, (uint8_t *)buf, 256) == VSF_ERR_NOT_AVAILABLE);
    CHECK(word_at(0x20000) == 0xFFFFFFFFU);
    CHECK(!(sim_fmc_ctl & FMC_CTL_PG));
    CHECK(vsf_hw_flash_erase_multi_sector(F, 0x20000, 0x20000) == VSF_ERR_NOT_AVAILABLE);
    CHECK(vsf_hw_flash_erase_one_sector(F, 0x20000) == VSF_ERR_NOT_AVAILABLE);
    CHECK(!(sim_fmc_ctl & (FMC_CTL_SER | FMC_CTL_START)));

    /* reading needs no interrupt */
    uint32_t got = 0;
    CHECK(vsf_hw_flash_read_multi_sector(F, 0x20000, (uint8_t *)&got, 4) == VSF_ERR_NONE);
    CHECK(got == 0xFFFFFFFFU);
}

/* H1-10: BUSY is low between words, but the request is not over */
static void test_busy_between_words(void)
{
    printf("H1-10 is_busy holds across a multi-word write\n");
    reset_sim(1, 1);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x20000, (uint8_t *)buf, 8) == VSF_ERR_NONE);
    CHECK(vsf_hw_flash_status(F).is_busy);
    fmc_run(1);
    CHECK(words == 1);
    CHECK(vsf_hw_flash_status(F).is_busy);
    fmc_run(0x8000);
    CHECK(words == 2);
    CHECK(!vsf_hw_flash_status(F).is_busy);
}

/* fills all of flash, so an erase shows up as what it really touched */
static void fill(void)
{
    memset(FLASH_MEM, 0x00, FLASH_SIZE);
}

static int is_erased(uint32_t base, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        if (FLASH_MEM[base + i] != 0xFF) {
            return 0;
        }
    }
    return 1;
}

/* H1-11: the unit is a whole sector, so the range must start and end on one */
static void test_erase_refused(void)
{
    static const struct { uint32_t off, size; vsf_err_t want; const char *why; } t[] = {
        { 0x18000, 0x10000, VSF_ERR_INVALID_PARAMETER, "start inside sector 4" },
        { 0x20000, 0,       VSF_ERR_INVALID_PARAMETER, "size 0" },
        { 0x20000, 0x10000, VSF_ERR_INVALID_PARAMETER, "end inside sector 5" },
        { 0x24000, 0x20000, VSF_ERR_INVALID_PARAMETER, "16 KB aligned, not a sector start" },
        { 0xE0000, 0x40000, VSF_ERR_INVALID_RANGE,     "past the end" },
        { 0x100000, 0x20000, VSF_ERR_INVALID_RANGE,    "offset past the end" },
    };
    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        printf("H1-11 erase 0x%x size 0x%x: %s\n", (unsigned)t[i].off, (unsigned)t[i].size, t[i].why);
        reset_sim(1, 1);
        fill();
        CHECK(vsf_hw_flash_erase_multi_sector(F, t[i].off, t[i].size) == t[i].want);
        fmc_run(16);
        CHECK(erased_mask == 0);
        CHECK(calls == 0);
        CHECK(!(sim_fmc_ctl & FMC_CTL_SER));
    }
}

static void check_erase(uint32_t off, uint32_t size, uint32_t want_mask)
{
    printf("H1-11 erase 0x%x size 0x%x\n", (unsigned)off, (unsigned)size);
    reset_sim(1, 1);
    fill();
    CHECK(vsf_hw_flash_erase_multi_sector(F, off, size) == VSF_ERR_NONE);
    CHECK(vsf_hw_flash_status(F).is_busy);
    fmc_run(16);
    CHECK(erased_mask == want_mask);
    CHECK(is_erased(off, size));
    if (off > 0) {
        CHECK(FLASH_MEM[off - 1] == 0x00);
    }
    if (off + size < FLASH_SIZE) {
        CHECK(FLASH_MEM[off + size] == 0x00);
    }
    CHECK(calls == 1);
    CHECK(last_mask == VSF_FLASH_IRQ_ERASE_MASK);
    CHECK(!(sim_fmc_ctl & FMC_CTL_SER));
    CHECK(!vsf_hw_flash_status(F).is_busy);
}

static void test_erase_ok(void)
{
    check_erase(0x10000, 0x30000, (1U << 4) | (1U << 5));
    check_erase(0x00000, 0x04000, 1U << 0);
    check_erase(0x0C000, 0x14000, (1U << 3) | (1U << 4));
    check_erase(0xE0000, 0x20000, 1U << 11);
    check_erase(0x00000, FLASH_SIZE, 0xFFF);    /* what the erase_all template asks */

    printf("H1-11 erase_one_sector\n");
    reset_sim(1, 1);
    fill();
    CHECK(vsf_hw_flash_erase_one_sector(F, 0x24000) == VSF_ERR_INVALID_PARAMETER);
    CHECK(vsf_hw_flash_erase_one_sector(F, FLASH_SIZE) == VSF_ERR_INVALID_RANGE);
    CHECK(erased_mask == 0);
    CHECK(vsf_hw_flash_erase_one_sector(F, 0x20000) == VSF_ERR_NONE);
    CHECK(vsf_hw_flash_status(F).is_busy);
    fmc_run(16);
    CHECK(erased_mask == 1U << 5);
    CHECK(calls == 1);
    CHECK(last_mask == VSF_FLASH_IRQ_ERASE_MASK);

    printf("H1-11 stray END after a finished erase\n");
    sim_fmc_stat = FMC_STAT_END;
    FMC_IRQHandler();
    sim_fmc_stat = 0;
    CHECK(calls == 1);
}

/* H1-12: step 2 of both sequences is BUSY=0, sections 2.3.5 and 2.3.7 */
static void test_busy(void)
{
    printf("H1-12 BUSY set by an operation the driver did not start\n");
    reset_sim(1, 1);
    sim_fmc_stat = FMC_STAT_BUSY;
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x40000, (uint8_t *)buf, 8) == VSF_ERR_BUSY);
    CHECK(word_at(0x40000) == 0xFFFFFFFFU);
    CHECK(!(sim_fmc_ctl & FMC_CTL_PG));
    CHECK(vsf_hw_flash_erase_multi_sector(F, 0x40000, 0x20000) == VSF_ERR_BUSY);
    CHECK(vsf_hw_flash_erase_one_sector(F, 0x40000) == VSF_ERR_BUSY);
    CHECK(!(sim_fmc_ctl & (FMC_CTL_SER | FMC_CTL_START)));
    CHECK(vsf_hw_flash_status(F).is_busy);
    sim_fmc_stat = 0;
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x40000, (uint8_t *)buf, 8) == VSF_ERR_NONE);
    fmc_run(16);
    CHECK(word_at(0x40004) == buf[1]);

    printf("H1-12 a second request while one runs, BUSY low between words\n");
    reset_sim(1, 1);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x20000, (uint8_t *)buf, 16) == VSF_ERR_NONE);
    fmc_run(1);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x60000, (uint8_t *)buf, 8) == VSF_ERR_BUSY);
    CHECK(vsf_hw_flash_erase_multi_sector(F, 0x60000, 0x20000) == VSF_ERR_BUSY);
    CHECK(word_at(0x60000) == 0xFFFFFFFFU);
    fmc_run(16);
    CHECK(memcmp(FLASH_MEM + 0x20000, buf, 16) == 0);
    CHECK(calls == 1);

    printf("H1-12 the next write can start from the callback\n");
    reset_sim(1, 1);
    chain.armed = 1;
    chain.off = 0x60000;
    chain.ret = VSF_ERR_FAIL;
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x20000, (uint8_t *)buf, 8) == VSF_ERR_NONE);
    fmc_run(16);
    CHECK(chain.ret == VSF_ERR_NONE);
    CHECK(memcmp(FLASH_MEM + 0x60000, buf, 8) == 0);
    CHECK(calls == 2);
}

/* H1-13: enable adds to the mask, disable takes away only what it names */
static void test_irq_mask(void)
{
    uint32_t got;
    const uint32_t both = FMC_CTL_ENDIE | FMC_CTL_ERRIE;

    printf("H1-13 enable ERASE, enable READ, disable READ\n");
    reset_sim(1, 0);
    vsf_hw_flash_irq_enable(F, VSF_FLASH_IRQ_ERASE_MASK);
    vsf_hw_flash_irq_enable(F, VSF_FLASH_IRQ_READ_MASK);
    vsf_hw_flash_irq_disable(F, VSF_FLASH_IRQ_READ_MASK);
    CHECK((sim_fmc_ctl & both) == both);

    printf("H1-13 the same in the middle of a 2-sector erase\n");
    reset_sim(1, 0);
    vsf_hw_flash_irq_enable(F, VSF_FLASH_IRQ_ERASE_MASK);
    CHECK(vsf_hw_flash_erase_multi_sector(F, 0x20000, 0x40000) == VSF_ERR_NONE);
    vsf_hw_flash_irq_enable(F, VSF_FLASH_IRQ_READ_MASK);
    vsf_hw_flash_irq_disable(F, VSF_FLASH_IRQ_READ_MASK);
    fmc_run(16);
    CHECK(erased_mask == ((1U << 5) | (1U << 6)));
    CHECK(calls == 1);
    CHECK(last_mask == VSF_FLASH_IRQ_ERASE_MASK);

    printf("H1-13 ENDIE goes off with the last hardware bit, not before\n");
    reset_sim(1, 0);
    vsf_hw_flash_irq_enable(F, VSF_FLASH_IRQ_ERASE_MASK | VSF_FLASH_IRQ_WRITE_MASK);
    vsf_hw_flash_irq_disable(F, VSF_FLASH_IRQ_ERASE_MASK);
    CHECK((sim_fmc_ctl & both) == both);
    vsf_hw_flash_irq_disable(F, VSF_FLASH_IRQ_WRITE_MASK);
    CHECK((sim_fmc_ctl & both) == 0);

    printf("H1-13 READ is software only: it needs no ENDIE and keeps its callback\n");
    reset_sim(1, 0);
    vsf_hw_flash_irq_enable(F, VSF_FLASH_IRQ_READ_MASK);
    CHECK((sim_fmc_ctl & both) == 0);
    CHECK(vsf_hw_flash_write_multi_sector(F, 0x20000, (uint8_t *)buf, 4) == VSF_ERR_NOT_AVAILABLE);
    CHECK(vsf_hw_flash_read_multi_sector(F, 0x20000, (uint8_t *)&got, 4) == VSF_ERR_NONE);
    CHECK(calls == 1);
    CHECK(last_mask == VSF_FLASH_IRQ_READ_MASK);
    vsf_hw_flash_irq_disable(F, VSF_FLASH_IRQ_READ_MASK);
    CHECK(vsf_hw_flash_read_multi_sector(F, 0x20000, (uint8_t *)&got, 4) == VSF_ERR_NONE);
    CHECK(calls == 1);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);   /* keep earlier lines if a sanitizer aborts */
    if (mmap(FLASH_MEM, FLASH_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED) {
        perror("mmap");
        return 2;
    }
    for (uint32_t i = 0; i < sizeof(buf) / 4; i++) {
        buf[i] = 0xA5000000U | i;
    }
    test_write_bad_size();
    test_write_range();
    test_write_ok();
    test_stray_end();
    check_not_armed(0, 0, "no handler, no irq_enable");
    check_not_armed(1, 0, "handler but no irq_enable");
    check_not_armed(0, 1, "irq_enable but no handler");
    test_busy_between_words();
    test_erase_refused();
    test_erase_ok();
    test_busy();
    test_irq_mask();
    return host_summary("flash");
}
