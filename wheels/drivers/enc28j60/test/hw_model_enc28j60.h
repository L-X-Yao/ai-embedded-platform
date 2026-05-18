/*
 * hw_model_enc28j60.h -- ENC28J60 SPI Ethernet controller hardware model
 *
 * Models Microchip ENC28J60 SPI register + buffer + PHY behavior:
 * - SPI command protocol: RCR (0x00), WCR (0x40), BFS (0x80), BFC (0xA0),
 *   RBM (0x3A), WBM (0x7A), SRC (0xFF)
 * - 4-bank register file: bank selected via ECON1[1:0]; registers 0x1B-0x1F
 *   are common across all banks
 * - MAC/MII registers (bank 2/3, addr 0x00-0x19): dummy byte on read
 * - 8KB buffer memory (RXSTART..TXEND) with auto-incrementing R/W pointers
 * - PHY registers accessed indirectly: write MIREGADR, set MICMD.MIIRD,
 *   poll MISTAT.BUSY, read MIRDL/H
 * - Soft reset (SRC) restores all registers + bank to defaults
 * - EREVID (bank 3, 0x12) read-only, default 0x06
 *
 * Header-only. Include from test_hw_model_enc28j60.c.
 */

#ifndef HW_MODEL_ENC28J60_H
#define HW_MODEL_ENC28J60_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- SPI opcodes (matching enc28j60.c) --- */

#define ENC_OP_RCR  0x00U   /* Read Control Register  */
#define ENC_OP_RBM  0x3AU   /* Read Buffer Memory     */
#define ENC_OP_WCR  0x40U   /* Write Control Register */
#define ENC_OP_WBM  0x7AU   /* Write Buffer Memory    */
#define ENC_OP_BFS  0x80U   /* Bit Field Set          */
#define ENC_OP_BFC  0xA0U   /* Bit Field Clear        */
#define ENC_OP_SRC  0xFFU   /* System Reset Command   */

/* --- Bank 0 registers --- */

#define ENC_ERDPTL    0x00U
#define ENC_ERDPTH    0x01U
#define ENC_EWRPTL    0x02U
#define ENC_EWRPTH    0x03U
#define ENC_ETXSTL    0x04U
#define ENC_ETXSTH    0x05U
#define ENC_ETXNDL    0x06U
#define ENC_ETXNDH    0x07U
#define ENC_ERXSTL    0x08U
#define ENC_ERXSTH    0x09U
#define ENC_ERXNDL    0x0AU
#define ENC_ERXNDH    0x0BU
#define ENC_ERXRDPTL  0x0CU
#define ENC_ERXRDPTH  0x0DU

/* --- Bank 1 registers --- */

#define ENC_ERXFCON   0x18U
#define ENC_EPKTCNT   0x19U

/* --- Bank 2 registers --- */

#define ENC_MACON1    0x00U
#define ENC_MACON3    0x02U
#define ENC_MABBIPG   0x04U
#define ENC_MAIPGL    0x06U
#define ENC_MAIPGH    0x07U
#define ENC_MAMXFLL   0x0AU
#define ENC_MAMXFLH   0x0BU
#define ENC_MIREGADR  0x14U
#define ENC_MICMD     0x12U
#define ENC_MISTAT    0x0AU   /* bank 3 */
#define ENC_MIRDL     0x18U
#define ENC_MIRDH     0x19U

/* --- Bank 3 registers --- */

#define ENC_MAADR5    0x00U
#define ENC_MAADR6    0x01U
#define ENC_MAADR3    0x02U
#define ENC_MAADR4    0x03U
#define ENC_MAADR1    0x04U
#define ENC_MAADR2    0x05U
#define ENC_EREVID    0x12U

/* --- Common registers (all banks) --- */

#define ENC_EIE       0x1BU
#define ENC_EIR       0x1CU
#define ENC_ESTAT     0x1DU
#define ENC_ECON2     0x1EU
#define ENC_ECON1     0x1FU

/* --- Bit masks --- */

#define ENC_ECON1_BSEL0   0x01U
#define ENC_ECON1_BSEL1   0x02U
#define ENC_ECON1_RXEN    0x04U
#define ENC_ECON1_TXRTS   0x08U
#define ENC_ECON2_PKTDEC  0x40U
#define ENC_ESTAT_CLKRDY  0x01U
#define ENC_MICMD_MIIRD   0x01U
#define ENC_MISTAT_BUSY   0x01U

/* --- Buffer layout --- */

#define ENC_RXSTART  0x0000U
#define ENC_RXEND    0x19FFU
#define ENC_TXSTART  0x1A00U
#define ENC_TXEND    0x1FFFU

/* --- Model buffer size: 8KB to match real chip --- */

#define ENC_BUF_SIZE  8192U

/* --- PHY register count --- */

#define ENC_PHY_REG_COUNT  32U

/* --- SPI transaction phases --- */

typedef enum {
    ENC_PH_IDLE = 0,
    ENC_PH_READ_DATA,       /* RCR: returning register value on each byte */
    ENC_PH_WRITE_DATA,      /* WCR: waiting for data byte */
    ENC_PH_BFS_DATA,        /* BFS: waiting for mask byte */
    ENC_PH_BFC_DATA,        /* BFC: waiting for mask byte */
    ENC_PH_READ_BUF,        /* RBM: streaming buffer reads */
    ENC_PH_WRITE_BUF,       /* WBM: streaming buffer writes */
} enc28j60_phase_t;

/* --- The hardware model --- */

typedef struct {
    /* 4 banks of 32 registers each */
    uint8_t  regs[4][32];
    /* Common registers (0x1B..0x1F) shared across all banks */
    uint8_t  common[32];   /* only indices 0x1B..0x1F used */
    /* Reset values: same layout */
    uint8_t  reset_regs[4][32];
    uint8_t  reset_common[32];
    /* Read-only flags per bank (bit set = read-only) */
    bool     readonly[4][32];
    bool     readonly_common[32];

    /* 8KB buffer memory */
    uint8_t  buffer[ENC_BUF_SIZE];
    /* Auto-incrementing read/write pointer into buffer */
    uint16_t buf_rdptr;
    uint16_t buf_wrptr;

    /* PHY registers (accessed indirectly via MIREGADR/MICMD/MIRDL/H) */
    uint16_t phy_regs[ENC_PHY_REG_COUNT];

    /* Current bank selection */
    uint8_t  current_bank;

    /* SPI transaction state */
    enc28j60_phase_t phase;
    uint8_t  spi_addr;       /* 5-bit register address from opcode byte */

    /* Counters */
    int      reset_count;
    int      delay_count;

    /* TX behavior: how many ECON1 reads before TXRTS auto-clears */
    int      tx_complete_after;
    int      tx_econ1_read_count;

    /* Track buffer writes for verification */
    uint8_t  tx_captured[2048];
    int      tx_captured_len;
} enc28j60_model_t;

/* ================================================================
 * Internal helpers
 * ================================================================ */

#define ENC_IS_COMMON(addr) ((addr) >= 0x1BU)

/* Get pointer to the register value for the current bank */
__attribute__((unused))
static uint8_t *enc_reg_ptr(enc28j60_model_t *m, uint8_t addr)
{
    if (ENC_IS_COMMON(addr))
        return &m->common[addr];
    return &m->regs[m->current_bank][addr];
}

/* Write to a register, respecting read-only flags */
__attribute__((unused))
static void enc_write_reg(enc28j60_model_t *m, uint8_t addr, uint8_t val)
{
    if (ENC_IS_COMMON(addr)) {
        if (!m->readonly_common[addr])
            m->common[addr] = val;
    } else {
        if (!m->readonly[m->current_bank][addr])
            m->regs[m->current_bank][addr] = val;
    }
}

/* Apply BFS (bit field set) */
__attribute__((unused))
static void enc_bfs_reg(enc28j60_model_t *m, uint8_t addr, uint8_t mask)
{
    uint8_t *p = enc_reg_ptr(m, addr);
    if (ENC_IS_COMMON(addr)) {
        if (!m->readonly_common[addr])
            *p |= mask;
    } else {
        if (!m->readonly[m->current_bank][addr])
            *p |= mask;
    }
}

/* Apply BFC (bit field clear) */
__attribute__((unused))
static void enc_bfc_reg(enc28j60_model_t *m, uint8_t addr, uint8_t mask)
{
    uint8_t *p = enc_reg_ptr(m, addr);
    if (ENC_IS_COMMON(addr)) {
        if (!m->readonly_common[addr])
            *p = (uint8_t)(*p & ~mask);
    } else {
        if (!m->readonly[m->current_bank][addr])
            *p = (uint8_t)(*p & ~mask);
    }
}

/* Update bank selection from ECON1[1:0] */
__attribute__((unused))
static void enc_update_bank(enc28j60_model_t *m)
{
    m->current_bank = m->common[ENC_ECON1] & 0x03U;
}

/* Get the 16-bit buffer read pointer from ERDPTL/H */
__attribute__((unused))
static uint16_t enc_get_erdpt(enc28j60_model_t *m)
{
    return (uint16_t)(((uint16_t)m->regs[0][ENC_ERDPTH] << 8) |
           m->regs[0][ENC_ERDPTL]);
}

/* Get the 16-bit buffer write pointer from EWRPTL/H */
__attribute__((unused))
static uint16_t enc_get_ewrpt(enc28j60_model_t *m)
{
    return (uint16_t)(((uint16_t)m->regs[0][ENC_EWRPTH] << 8) |
           m->regs[0][ENC_EWRPTL]);
}

/* ================================================================
 * Soft reset: restore defaults
 * ================================================================ */

__attribute__((unused))
static void enc_soft_reset(enc28j60_model_t *m)
{
    memcpy(m->regs, m->reset_regs, sizeof(m->regs));
    memcpy(m->common, m->reset_common, sizeof(m->common));
    m->current_bank = 0;
    m->buf_rdptr = 0;
    m->buf_wrptr = 0;
    m->tx_econ1_read_count = 0;
    m->tx_captured_len = 0;
    m->reset_count++;
}

/* ================================================================
 * SPI HAL callbacks
 * ================================================================ */

__attribute__((unused))
static void enc28j60_model_cs_low(void *ctx)
{
    enc28j60_model_t *m = (enc28j60_model_t *)ctx;
    m->phase = ENC_PH_IDLE;
    m->spi_addr = 0;
}

__attribute__((unused))
static void enc28j60_model_cs_high(void *ctx)
{
    enc28j60_model_t *m = (enc28j60_model_t *)ctx;
    m->phase = ENC_PH_IDLE;
}

__attribute__((unused))
static uint8_t enc28j60_model_xfer(void *ctx, uint8_t tx)
{
    enc28j60_model_t *m = (enc28j60_model_t *)ctx;

    switch (m->phase) {
    case ENC_PH_IDLE: {
        /* First byte: opcode (bits 7:5) + register address (bits 4:0) */

        /* System Reset Command */
        if (tx == ENC_OP_SRC) {
            enc_soft_reset(m);
            return 0xFFU;
        }

        /* Read Buffer Memory */
        if (tx == ENC_OP_RBM) {
            /* Buffer reads use ERDPT as starting address */
            m->buf_rdptr = enc_get_erdpt(m);
            m->phase = ENC_PH_READ_BUF;
            return 0xFFU;
        }

        /* Write Buffer Memory */
        if (tx == ENC_OP_WBM) {
            /* Buffer writes use EWRPT as starting address */
            m->buf_wrptr = enc_get_ewrpt(m);
            m->phase = ENC_PH_WRITE_BUF;
            return 0xFFU;
        }

        uint8_t op   = tx & 0xE0U;
        uint8_t addr = tx & 0x1FU;
        m->spi_addr = addr;

        if (op == ENC_OP_RCR) {
            /* Read Control Register: return register value on every
             * subsequent byte. For MAC/MII registers, the driver sends
             * an extra dummy clock (ignoring the returned value), so
             * we simply return the register value each time. */
            m->phase = ENC_PH_READ_DATA;
        } else if (op == ENC_OP_WCR) {
            m->phase = ENC_PH_WRITE_DATA;
        } else if (op == ENC_OP_BFS) {
            m->phase = ENC_PH_BFS_DATA;
        } else if (op == ENC_OP_BFC) {
            m->phase = ENC_PH_BFC_DATA;
        }
        return 0xFFU;
    }

    case ENC_PH_READ_DATA: {
        /* Return register value. The driver handles the dummy byte for
         * MAC/MII registers by calling xfer an extra time and discarding
         * the result (which is also the register value -- harmless).
         * This means the model returns the register value on every byte
         * and the driver picks up the correct one. */
        uint8_t val = *enc_reg_ptr(m, m->spi_addr);
        /* Special handling: reading ECON1 may auto-clear TXRTS */
        if (m->spi_addr == ENC_ECON1) {
            if (m->common[ENC_ECON1] & ENC_ECON1_TXRTS) {
                m->tx_econ1_read_count++;
                if (m->tx_complete_after > 0 &&
                    m->tx_econ1_read_count >= m->tx_complete_after) {
                    m->common[ENC_ECON1] &= (uint8_t)~ENC_ECON1_TXRTS;
                    val = m->common[ENC_ECON1];
                }
            }
        }
        return val;
    }

    case ENC_PH_WRITE_DATA: {
        enc_write_reg(m, m->spi_addr, tx);
        /* If writing ECON1, update bank selection */
        if (m->spi_addr == ENC_ECON1) {
            enc_update_bank(m);
        }
        /* If writing MICMD with MIIRD set, simulate PHY read */
        if (m->current_bank == 2 && m->spi_addr == ENC_MICMD) {
            if (tx & ENC_MICMD_MIIRD) {
                uint8_t phy_addr = m->regs[2][ENC_MIREGADR] & 0x1FU;
                uint16_t phy_val = m->phy_regs[phy_addr];
                /* Store result in MIRDL/H (bank 2) */
                m->regs[2][ENC_MIRDL] = (uint8_t)(phy_val & 0xFFU);
                m->regs[2][ENC_MIRDH] = (uint8_t)(phy_val >> 8);
                /* Clear MISTAT.BUSY (bank 3, addr 0x0A) */
                m->regs[3][ENC_MISTAT] &= (uint8_t)~ENC_MISTAT_BUSY;
            }
        }
        m->phase = ENC_PH_IDLE;
        return 0xFFU;
    }

    case ENC_PH_BFS_DATA: {
        enc_bfs_reg(m, m->spi_addr, tx);
        if (m->spi_addr == ENC_ECON1) {
            enc_update_bank(m);
        }
        m->phase = ENC_PH_IDLE;
        return 0xFFU;
    }

    case ENC_PH_BFC_DATA: {
        enc_bfc_reg(m, m->spi_addr, tx);
        if (m->spi_addr == ENC_ECON1) {
            enc_update_bank(m);
        }
        m->phase = ENC_PH_IDLE;
        return 0xFFU;
    }

    case ENC_PH_READ_BUF: {
        /* Read from buffer memory at buf_rdptr, auto-increment with wrap */
        uint8_t val = 0x00U;
        if (m->buf_rdptr < ENC_BUF_SIZE) {
            val = m->buffer[m->buf_rdptr];
        }
        m->buf_rdptr++;
        if (m->buf_rdptr > ENC_RXEND && m->buf_rdptr < ENC_TXSTART) {
            /* Wrap within RX buffer space (simplified) */
        }
        /* Update ERDPT registers to track auto-increment */
        m->regs[0][ENC_ERDPTL] = (uint8_t)(m->buf_rdptr & 0xFFU);
        m->regs[0][ENC_ERDPTH] = (uint8_t)(m->buf_rdptr >> 8);
        return val;
    }

    case ENC_PH_WRITE_BUF: {
        /* Write to buffer memory at buf_wrptr, auto-increment */
        if (m->buf_wrptr < ENC_BUF_SIZE) {
            m->buffer[m->buf_wrptr] = tx;
        }
        /* Also capture into tx_captured for test verification */
        if (m->tx_captured_len < (int)sizeof(m->tx_captured)) {
            m->tx_captured[m->tx_captured_len++] = tx;
        }
        m->buf_wrptr++;
        /* Update EWRPT registers */
        m->regs[0][ENC_EWRPTL] = (uint8_t)(m->buf_wrptr & 0xFFU);
        m->regs[0][ENC_EWRPTH] = (uint8_t)(m->buf_wrptr >> 8);
        return 0xFFU;
    }

    } /* switch */

    return 0xFFU;
}

__attribute__((unused))
static void enc28j60_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ms;
    enc28j60_model_t *m = (enc28j60_model_t *)ctx;
    m->delay_count++;
}

/* ================================================================
 * Model initialization
 * ================================================================ */

__attribute__((unused))
static void enc28j60_model_init(enc28j60_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Set reset defaults */

    /* EREVID (bank 3, 0x12) = 0x06 (silicon rev 6) */
    m->reset_regs[3][ENC_EREVID] = 0x06U;
    /* ESTAT.CLKRDY = 1 after reset (clock is ready) */
    m->reset_common[ENC_ESTAT] = ENC_ESTAT_CLKRDY;

    /* Mark EREVID as read-only */
    m->readonly[3][ENC_EREVID] = true;
    /* Mark ESTAT as read-only */
    m->readonly_common[ENC_ESTAT] = true;
    /* Mark EIR as read-only (interrupt flags, cleared by hardware) */
    m->readonly_common[ENC_EIR] = true;
    /* EPKTCNT (bank 1) is read-only from SPI perspective; only
     * PKTDEC in ECON2 decrements it */
    m->readonly[1][ENC_EPKTCNT] = true;

    /* Apply reset values */
    memcpy(m->regs, m->reset_regs, sizeof(m->regs));
    memcpy(m->common, m->reset_common, sizeof(m->common));

    /* Clear buffer */
    memset(m->buffer, 0, sizeof(m->buffer));

    /* Default PHY register values */
    memset(m->phy_regs, 0, sizeof(m->phy_regs));

    /* Initial state */
    m->current_bank = 0;
    m->phase = ENC_PH_IDLE;
    m->reset_count = 0;
    m->delay_count = 0;
    m->tx_complete_after = 1;   /* TXRTS clears after 1 read by default */
    m->tx_econ1_read_count = 0;
    m->tx_captured_len = 0;
}

/* ================================================================
 * Helper: inject an RX packet into the buffer at a given address
 *
 * Writes the ENC28J60 receive status vector (6 bytes) plus data.
 * Format: [next_pkt_lo, next_pkt_hi, len_lo, len_hi, status_lo, status_hi]
 * ================================================================ */

__attribute__((unused))
static void enc28j60_model_inject_rx(enc28j60_model_t *m, uint16_t addr,
                                     uint16_t next_pkt,
                                     const uint8_t *frame, uint16_t frame_len)
{
    uint16_t off = addr;
    /* Next packet pointer (little-endian) */
    m->buffer[off++] = (uint8_t)(next_pkt & 0xFFU);
    m->buffer[off++] = (uint8_t)(next_pkt >> 8);
    /* Byte count (little-endian) */
    m->buffer[off++] = (uint8_t)(frame_len & 0xFFU);
    m->buffer[off++] = (uint8_t)(frame_len >> 8);
    /* Status (OK) */
    m->buffer[off++] = 0x00U;
    m->buffer[off++] = 0x00U;
    /* Frame data */
    for (uint16_t i = 0; i < frame_len; i++) {
        m->buffer[off++] = frame[i];
    }
}

/* ================================================================
 * Helper: set PHY register value (for indirect read tests)
 * ================================================================ */

__attribute__((unused))
static void enc28j60_model_set_phy(enc28j60_model_t *m, uint8_t phy_addr,
                                   uint16_t val)
{
    if (phy_addr < ENC_PHY_REG_COUNT) {
        m->phy_regs[phy_addr] = val;
    }
}

/* ================================================================
 * Helper: set EPKTCNT (bypasses read-only check for test setup)
 * ================================================================ */

__attribute__((unused))
static void enc28j60_model_set_pktcnt(enc28j60_model_t *m, uint8_t count)
{
    m->regs[1][ENC_EPKTCNT] = count;
}

#endif /* HW_MODEL_ENC28J60_H */
