// SPDX-License-Identifier: MIT
/*
 * hw_model_w25qxx.h -- W25QXX SPI NOR flash hardware model
 *
 * Models W25Q series SPI flash behavior:
 * - Command-based SPI protocol (not register-based)
 * - JEDEC ID read (0x9F): returns 3-byte manufacturer/type/capacity
 * - Status register 1 (0x05): BUSY bit 0, WEL bit 1
 * - Write enable (0x06): sets WEL bit
 * - Read data (0x03): 24-bit address + continuous read
 * - Page program (0x02): up to 256 bytes, NOR semantics (can only clear bits),
 *   wrapping inside the page
 * - Sector erase (0x20): 4KB aligned erase to 0xFF
 * - Chip erase (0xC7): entire flash to 0xFF
 * - Program and erase start when /CS rises, need WEL, and run with BUSY set.
 *   Meanwhile only Read Status gets an answer. WEL clears when they end.
 *
 * Uses a state machine to track the SPI transaction phases. Time passes only
 * through status reads: a cycle lasts busy_reads of them, the same for every
 * operation. It is not the manual's time: at any SPI clock the real tCE of 20 to
 * 100 s is far more reads than the driver's poll limit, which is an open driver
 * question and so is not asserted here.
 */

#ifndef HW_MODEL_W25QXX_H
#define HW_MODEL_W25QXX_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define W25QXX_MODEL_SIZE  65536U  /* 64KB simulated flash */
#define W25QXX_MODEL_PAGE  256U
#define W25QXX_MODEL_BUSY_READS 3U

typedef enum {
    W25QXX_PHASE_IDLE = 0,
    W25QXX_PHASE_CMD,
    W25QXX_PHASE_ADDR,
    W25QXX_PHASE_DATA,
    W25QXX_PHASE_END,      /* instruction complete: /CS must rise now */
} w25qxx_phase_t;

typedef struct {
    uint8_t   mem[W25QXX_MODEL_SIZE];
    uint32_t  jedec_id;
    uint8_t   status;           /* SR1: bit 0 = BUSY, bit 1 = WEL */
    w25qxx_phase_t phase;
    uint8_t   cmd;
    uint32_t  addr;
    int       addr_bytes;
    int       id_byte;
    bool      overrun;          /* clocks came after the instruction ended */
    uint8_t   page_buf[W25QXX_MODEL_PAGE];
    uint32_t  page_sent;        /* data bytes of this Page Program */
    uint8_t   running;          /* the instruction of the running cycle */
    uint32_t  running_addr;
    uint32_t  busy_reads;       /* status reads a cycle lasts */
    uint32_t  busy_left;
    int       program_count;    /* number of page programs completed */
    int       erase_count;      /* number of sector and chip erases completed */
} w25qxx_model_t;

static void w25qxx_model_init(w25qxx_model_t *m)
{
    memset(m->mem, 0xFF, W25QXX_MODEL_SIZE);
    m->jedec_id      = 0xEF4017;  /* W25Q64 */
    m->status        = 0x00;
    m->phase         = W25QXX_PHASE_IDLE;
    m->cmd           = 0;
    m->addr          = 0;
    m->addr_bytes    = 0;
    m->id_byte       = 0;
    m->overrun       = false;
    m->page_sent     = 0;
    m->running       = 0;
    m->running_addr  = 0;
    m->busy_reads    = W25QXX_MODEL_BUSY_READS;
    m->busy_left     = 0;
    m->program_count = 0;
    m->erase_count   = 0;
}

/* /CS has risen on a complete instruction: the cycle starts. */
static void w25qxx_model_start_cycle(w25qxx_model_t *m)
{
    m->running = m->cmd;
    m->running_addr = m->addr;
    m->busy_left = m->busy_reads;
    m->status |= 0x01;
}

/* The cycle ends: the array changes, and BUSY and WEL clear (p.36). */
static void w25qxx_model_end_cycle(w25qxx_model_t *m)
{
    if (m->running == 0x02) {
        uint32_t page = m->running_addr & ~(W25QXX_MODEL_PAGE - 1U);
        for (uint32_t i = 0; i < W25QXX_MODEL_PAGE; i++)
            if (page + i < W25QXX_MODEL_SIZE)
                m->mem[page + i] &= m->page_buf[i];
        m->program_count++;
    } else if (m->running == 0x20) {
        uint32_t base = m->running_addr & ~0xFFFu;
        uint32_t end = base + 4096;
        if (end > W25QXX_MODEL_SIZE) end = W25QXX_MODEL_SIZE;
        for (uint32_t i = base; i < end; i++) m->mem[i] = 0xFF;
        m->erase_count++;
    } else {
        memset(m->mem, 0xFF, W25QXX_MODEL_SIZE);
        m->erase_count++;
    }
    m->running = 0;
    m->status = (uint8_t)(m->status & ~0x03u);
}

/* --- SPI HAL callbacks for w25qxx_spi_t --- */

__attribute__((unused))
static void w25qxx_model_cs_low(void *ctx)
{
    w25qxx_model_t *m = (w25qxx_model_t *)ctx;
    m->phase = W25QXX_PHASE_CMD;
    m->addr = 0;
    m->addr_bytes = 0;
    m->id_byte = 0;
    m->overrun = false;
    m->page_sent = 0;
}

__attribute__((unused))
static void w25qxx_model_cs_high(void *ctx)
{
    w25qxx_model_t *m = (w25qxx_model_t *)ctx;
    bool busy = (m->status & 0x01) != 0;
    bool wel = (m->status & 0x02) != 0;
    bool complete = m->phase == W25QXX_PHASE_END && !m->overrun;

    /* p.41: "The /CS pin must be driven high after the eighth bit has been
     * latched. If this is not done the Chip Erase instruction will not be
     * executed." Sector Erase has the same rule for its last byte. */
    if (!busy) {
        if (complete && m->cmd == 0x06)
            m->status |= 0x02;
        else if (complete && wel && (m->cmd == 0x20 || m->cmd == 0xC7))
            w25qxx_model_start_cycle(m);
        else if (m->cmd == 0x02 && m->phase == W25QXX_PHASE_DATA && wel &&
                 m->page_sent != 0)
            w25qxx_model_start_cycle(m);
    }
    m->phase = W25QXX_PHASE_IDLE;
}

__attribute__((unused))
static uint8_t w25qxx_model_xfer(void *ctx, uint8_t tx)
{
    w25qxx_model_t *m = (w25qxx_model_t *)ctx;

    if (m->phase == W25QXX_PHASE_CMD) {
        m->addr = 0;
        m->addr_bytes = 0;
        m->id_byte = 0;

        /* p.22: "all instructions except for Read Status Register will be
         * ignored" until the cycle has finished */
        if ((m->status & 0x01) && tx != 0x05) {
            m->phase = W25QXX_PHASE_IDLE;
            return 0xFF;
        }
        m->cmd = tx;

        /* Write Enable and Chip Erase end with their code */
        if (tx == 0x06 || tx == 0xC7) {
            m->phase = W25QXX_PHASE_END;
            return 0xFF;
        }
        /* JEDEC ID or Read Status: go to data phase */
        if (tx == 0x9F || tx == 0x05) {
            m->phase = W25QXX_PHASE_DATA;
            return 0xFF;
        }
        /* Commands with address phase */
        if (tx == 0x02)
            memset(m->page_buf, 0xFF, sizeof(m->page_buf));
        m->phase = W25QXX_PHASE_ADDR;
        return 0xFF;
    }

    if (m->phase == W25QXX_PHASE_ADDR) {
        m->addr = (m->addr << 8) | tx;
        m->addr_bytes++;
        if (m->addr_bytes >= 3)
            m->phase = (m->cmd == 0x20) ? W25QXX_PHASE_END : W25QXX_PHASE_DATA;
        return 0xFF;
    }

    if (m->phase == W25QXX_PHASE_END) {
        m->overrun = true;
        return 0xFF;
    }

    if (m->phase == W25QXX_PHASE_DATA) {
        /* JEDEC ID read */
        if (m->cmd == 0x9F) {
            uint8_t r = (uint8_t)((m->jedec_id >> (16 - 8 * m->id_byte)) & 0xFF);
            m->id_byte = (m->id_byte + 1) % 3;
            return r;
        }
        /* Read Status Register 1: the only clock a cycle has */
        if (m->cmd == 0x05) {
            uint8_t r = m->status;
            if ((m->status & 0x01) && --m->busy_left == 0)
                w25qxx_model_end_cycle(m);
            return r;
        }
        /* Read Data */
        if (m->cmd == 0x03) {
            uint8_t r = (m->addr < W25QXX_MODEL_SIZE) ? m->mem[m->addr] : 0xFF;
            m->addr++;
            return r;
        }
        /* Page Program: kept until /CS rises. p.36: past the page end "the
         * addressing will wrap to the beginning of the page", and past 256
         * bytes it will "overwrite previously sent data". */
        if (m->cmd == 0x02) {
            m->page_buf[(m->addr + m->page_sent) & (W25QXX_MODEL_PAGE - 1U)] = tx;
            m->page_sent++;
            return 0xFF;
        }
    }

    return 0xFF;
}

/* Helper: how many status reads a program or erase cycle lasts */
__attribute__((unused))
static void w25qxx_model_set_busy_reads(w25qxx_model_t *m, uint32_t reads)
{
    m->busy_reads = reads;
}

/* Helper: set JEDEC ID */
static void w25qxx_model_set_jedec(w25qxx_model_t *m, uint32_t id)
{
    m->jedec_id = id;
}

/* Helper: pre-fill flash region */
static void w25qxx_model_fill(w25qxx_model_t *m, uint32_t addr,
                               const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len && (addr + i) < W25QXX_MODEL_SIZE; i++) {
        m->mem[addr + i] = data[i];
    }
}

#endif /* HW_MODEL_W25QXX_H */
