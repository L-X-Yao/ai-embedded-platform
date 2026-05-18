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
 * - Page program (0x02): up to 256 bytes, NOR semantics (can only clear bits)
 * - Sector erase (0x20): 4KB aligned erase to 0xFF
 * - Chip erase (0xC7): entire flash to 0xFF
 *
 * Uses a state machine to track the SPI transaction phases.
 */

#ifndef HW_MODEL_W25QXX_H
#define HW_MODEL_W25QXX_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define W25QXX_MODEL_SIZE  65536U  /* 64KB simulated flash */

typedef enum {
    W25QXX_PHASE_IDLE = 0,
    W25QXX_PHASE_CMD,
    W25QXX_PHASE_ADDR,
    W25QXX_PHASE_DATA,
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
    int       program_count;    /* number of page programs completed */
    int       erase_count;      /* number of sector erases completed */
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
    m->program_count = 0;
    m->erase_count   = 0;
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
}

__attribute__((unused))
static void w25qxx_model_cs_high(void *ctx)
{
    w25qxx_model_t *m = (w25qxx_model_t *)ctx;

    /* Post-transaction processing */
    if (m->cmd == 0x02 && (m->status & 0x02)) {
        /* Page program complete: clear WEL and BUSY */
        m->status = (uint8_t)(m->status & ~0x03u);
        m->program_count++;
    }
    if (m->cmd == 0x20 && (m->status & 0x02)) {
        /* Sector erase: erase 4KB sector */
        uint32_t base = m->addr & ~0xFFFu;
        uint32_t end = base + 4096;
        if (end > W25QXX_MODEL_SIZE) end = W25QXX_MODEL_SIZE;
        for (uint32_t i = base; i < end; i++) m->mem[i] = 0xFF;
        m->status = (uint8_t)(m->status & ~0x03u);
        m->erase_count++;
    }
    if (m->cmd == 0xC7 && (m->status & 0x02)) {
        /* Chip erase complete: clear WEL and BUSY */
        m->status = (uint8_t)(m->status & ~0x03u);
        m->erase_count++;
    }

    m->phase = W25QXX_PHASE_IDLE;
}

__attribute__((unused))
static uint8_t w25qxx_model_xfer(void *ctx, uint8_t tx)
{
    w25qxx_model_t *m = (w25qxx_model_t *)ctx;

    if (m->phase == W25QXX_PHASE_CMD) {
        m->cmd = tx;
        m->addr = 0;
        m->addr_bytes = 0;
        m->id_byte = 0;

        /* Write Enable: set WEL */
        if (tx == 0x06) {
            m->status |= 0x02;
            return 0xFF;
        }
        /* JEDEC ID or Read Status: go to data phase */
        if (tx == 0x9F || tx == 0x05) {
            m->phase = W25QXX_PHASE_DATA;
            return 0xFF;
        }
        /* Chip erase */
        if (tx == 0xC7) {
            if (m->status & 0x02) {
                memset(m->mem, 0xFF, W25QXX_MODEL_SIZE);
                m->status = (uint8_t)(m->status & ~0x02u);
            }
            return 0xFF;
        }
        /* Commands with address phase */
        m->phase = W25QXX_PHASE_ADDR;
        return 0xFF;
    }

    if (m->phase == W25QXX_PHASE_ADDR) {
        m->addr = (m->addr << 8) | tx;
        m->addr_bytes++;
        if (m->addr_bytes >= 3) m->phase = W25QXX_PHASE_DATA;
        return 0xFF;
    }

    if (m->phase == W25QXX_PHASE_DATA) {
        /* JEDEC ID read */
        if (m->cmd == 0x9F) {
            uint8_t r = (uint8_t)((m->jedec_id >> (16 - 8 * m->id_byte)) & 0xFF);
            m->id_byte = (m->id_byte + 1) % 3;
            return r;
        }
        /* Read Status Register 1 */
        if (m->cmd == 0x05) {
            return m->status;
        }
        /* Read Data */
        if (m->cmd == 0x03) {
            uint8_t r = (m->addr < W25QXX_MODEL_SIZE) ? m->mem[m->addr] : 0xFF;
            m->addr++;
            return r;
        }
        /* Page Program: NOR semantics, can only clear bits */
        if (m->cmd == 0x02) {
            if ((m->status & 0x02) && m->addr < W25QXX_MODEL_SIZE)
                m->mem[m->addr] &= tx;
            m->addr++;
            return 0xFF;
        }
    }

    return 0xFF;
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
