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

static int calls;
static uint32_t last_mask;
static uint32_t words;
static uint32_t buf[0x8000 + 8];

static void on_irq(void *t, vsf_flash_t *f, vsf_flash_irq_mask_t m)
{
    (void)t; (void)f;
    calls++;
    last_mask = (uint32_t)m;
}

static void reset_sim(int with_handler, int with_irq_enable)
{
    memset(FLASH_MEM, 0xFF, FLASH_SIZE);
    sim_fmc_ctl = FMC_CTL_LK;
    sim_fmc_stat = 0;
    calls = 0;
    last_mask = 0;
    words = 0;
    host_asserts = 0;
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
            sim_fmc_ctl &= ~FMC_CTL_START;
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
    return host_summary("flash");
}
