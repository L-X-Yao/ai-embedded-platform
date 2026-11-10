// SPDX-License-Identifier: MIT
/*
 * hw_model_enc28j60.h -- ENC28J60 SPI Ethernet controller hardware model
 *
 * Models Microchip ENC28J60 SPI register + buffer + PHY behavior:
 * - SPI command protocol: RCR (0x00), WCR (0x40), BFS (0x80), BFC (0xA0),
 *   RBM (0x3A), WBM (0x7A), SRC (0xFF)
 * - 4-bank register file: bank selected via ECON1[1:0]; registers 0x1B-0x1F
 *   are common across all banks
 * - MAC/MII registers (all of bank 2, MAADR and MISTAT in bank 3): dummy byte
 *   on read
 * - 8KB buffer memory (RXSTART..TXEND) with auto-incrementing R/W pointers
 * - PHY registers accessed indirectly: write MIREGADR, set MICMD.MIIRD,
 *   poll MISTAT.BUSY, read MIRDL/H; or write MIREGADR, MIWRL, MIWRH, poll BUSY
 * - Soft reset (SRC) restores the Table 3-2 values, except CLKRDY and ECOCON
 * - EREVID (bank 3, 0x12) read-only, default 0x06
 * - BFS and BFC reach ETH registers only, and leave MAC and MII ones alone
 * - RBM wraps at ERXND, RBM and WBM wrap at 1FFFh, both need ECON2.AUTOINC
 * - PKTDEC decrements EPKTCNT and clears itself
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
#define ENC_ERXWRPTL  0x0EU
#define ENC_ERXWRPTH  0x0FU

/* --- Bank 1 registers --- */

#define ENC_ERXFCON   0x18U
#define ENC_EPKTCNT   0x19U

/* --- Bank 2 registers --- */

#define ENC_MACON1    0x00U
#define ENC_MACON3    0x02U
#define ENC_MACON4    0x03U
#define ENC_MABBIPG   0x04U
#define ENC_MAIPGL    0x06U
#define ENC_MAIPGH    0x07U
#define ENC_MACLCON1  0x08U
#define ENC_MACLCON2  0x09U
#define ENC_MAMXFLL   0x0AU
#define ENC_MAMXFLH   0x0BU
#define ENC_MIREGADR  0x14U
#define ENC_MIWRL     0x16U
#define ENC_MIWRH     0x17U
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
#define ENC_ECOCON    0x15U
#define ENC_EPAUSH    0x19U

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
#define ENC_ECON1_TXRST   0x80U
#define ENC_ECON2_PKTDEC  0x40U
#define ENC_ECON2_AUTOINC 0x80U
/* Register 3-2: bits 2-0 are unimplemented and read as 0 */
#define ENC_ECON2_MASK    0xF8U
#define ENC_EIR_RXERIF    0x01U
#define ENC_EIR_TXERIF    0x02U
#define ENC_EIR_TXIF      0x08U
#define ENC_EIR_DMAIF     0x20U
#define ENC_EIR_PKTIF     0x40U
/* Register 12-3: the R/C bits. PKTIF and LINKIF are R and follow the chip. */
#define ENC_EIR_RC_MASK \
    (ENC_EIR_DMAIF | ENC_EIR_TXIF | ENC_EIR_TXERIF | ENC_EIR_RXERIF)
#define ENC_ESTAT_CLKRDY  0x01U
#define ENC_ESTAT_TXABRT  0x02U
#define ENC_ESTAT_RXBUSY  0x04U
#define ENC_ESTAT_LATECOL 0x10U
#define ENC_ESTAT_BUFER   0x40U
/* Register 12-1: the R/C bits, which only the host clears */
#define ENC_ESTAT_RC_MASK \
    (ENC_ESTAT_TXABRT | ENC_ESTAT_LATECOL | ENC_ESTAT_BUFER)
#define ENC_MICMD_MIIRD   0x01U
#define ENC_MISTAT_BUSY   0x01U

/* --- PHY registers --- */

#define ENC_PHCON1         0x00U
#define ENC_PHCON1_PDPXMD  0x0100U
#define ENC_PHCON2         0x10U
#define ENC_PHCON2_HDLDIS  0x0100U

/* --- Buffer layout --- */

#define ENC_RXSTART  0x0000U
#define ENC_RXEND    0x19FFU
#define ENC_TXSTART  0x1A00U
#define ENC_TXEND    0x1FFFU

/* --- Model buffer size: 8KB to match real chip --- */

#define ENC_BUF_SIZE  8192U
#define ENC_BUF_LAST  0x1FFFU

/* --- PHY register count --- */

#define ENC_PHY_REG_COUNT  32U

/* --- SPI transaction phases --- */

typedef enum {
    ENC_PH_IDLE = 0,
    ENC_PH_READ_DUMMY,      /* RCR of a MAC/MII register: dummy byte first */
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
    /* MISTAT reads that still see BUSY after a PHY write, 0 = stays busy */
    int      phy_busy_after;
    int      phy_busy_reads;
    /* PHY writes started while BUSY was set, or with RXEN set */
    int      phy_write_while_busy;
    int      phy_write_rx_on;

    /* Current bank selection */
    uint8_t  current_bank;

    /* SPI transaction state */
    enc28j60_phase_t phase;
    uint8_t  spi_addr;       /* 5-bit register address from opcode byte */

    /* Counters */
    int      reset_count;
    int      delay_count;

    /* A frame arriving: ESTAT reads until RXBUSY clears, 0 = never */
    int      rxbusy_after;
    int      rxbusy_reads;
    /* MAADR writes made with RXEN or RXBUSY set */
    int      mac_write_rx_active;

    /* Oscillator Start-up Timer: ESTAT reads until CLKRDY sets, 0 = never */
    int      clkrdy_after;
    int      estat_read_count;

    /* ms the driver waited since the last SRC, -1 = no SRC seen yet */
    int      ms_since_src;
    /* MAC/MII accesses and RXEN/TXRTS sets made before the chip was ready */
    int      early_access_count;

    /* TX behavior: how many ECON1 reads before TXRTS auto-clears */
    int      tx_complete_after;
    int      tx_econ1_read_count;
    /* ESTAT bits the next transmission sets as it ends, to model an abort */
    uint8_t  tx_end_estat;
    /* Buffer bytes written while a transmission was pending */
    int      tx_write_while_pending;
    /* Errata DS80349C issue 12: an abort left the transmit logic stalled, so
     * TXRTS stays set until a TXRST. Off to model silicon that recovers. */
    bool     tx_abort_stalls;
    bool     tx_stalled;
    /* TXRTS set with TXRST still held, and EIR as the last TXRTS found it */
    int      tx_start_in_reset;
    uint8_t  eir_at_tx_start;

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

/* MAC and MII registers (Table 3-1): all of bank 2, MAADR and MISTAT in bank 3.
 * Everything else, EREVID included, is an ETH register. */
__attribute__((unused))
static bool enc_is_mac_mii(uint8_t bank, uint8_t addr)
{
    if (ENC_IS_COMMON(addr)) return false;
    if (bank == 2U) return true;
    return bank == 3U && (addr <= ENC_MAADR2 || addr == ENC_MISTAT);
}

/* DS39662C p.7: until CLKRDY is set, no MAC, MII or PHY access, no RXEN and no
 * TXRTS. Errata DS80349C issue 2: an SRC does not clear CLKRDY, so after one
 * the wait of at least 1 ms is what counts. */
__attribute__((unused))
static void enc_note_ready_needed(enc28j60_model_t *m)
{
    if (!(m->common[ENC_ESTAT] & ENC_ESTAT_CLKRDY) ||
        (m->ms_since_src >= 0 && m->ms_since_src < 1))
        m->early_access_count++;
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
    /* p.31: "this command cannot be used on the MAC registers, MII registers,
     * PHY registers or buffer memory" */
    if (enc_is_mac_mii(m->current_bank, addr))
        return;
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
    /* p.31: BFC has the same limit as BFS */
    if (enc_is_mac_mii(m->current_bank, addr))
        return;
    if (addr == ENC_ESTAT) {
        *p = (uint8_t)(*p & ~(mask & ENC_ESTAT_RC_MASK));
    } else if (addr == ENC_EIR) {
        *p = (uint8_t)(*p & ~(mask & ENC_EIR_RC_MASK));
    } else if (ENC_IS_COMMON(addr)) {
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

/* Side effects of any write to ECON2. Register 3-2 note 1: PKTDEC "is
 * automatically cleared once it is set". p.47: it decrements EPKTCNT, "if
 * EPKTCNT is '0', the EIR.PKTIF flag will automatically be cleared", and
 * "Attempts to decrement EPKTCNT below 0 are ignored." */
__attribute__((unused))
static void enc_econ2_written(enc28j60_model_t *m)
{
    if (m->common[ENC_ECON2] & ENC_ECON2_PKTDEC) {
        if (m->regs[1][ENC_EPKTCNT] > 0U)
            m->regs[1][ENC_EPKTCNT]--;
        if (m->regs[1][ENC_EPKTCNT] == 0U)
            m->common[ENC_EIR] &= (uint8_t)~ENC_EIR_PKTIF;
    }
    m->common[ENC_ECON2] &= (uint8_t)(ENC_ECON2_MASK & ~ENC_ECON2_PKTDEC);
}

/* DS39662C p.42: the end of a transmission. TXRTS clears and TXIF sets, and
 * p.70: an abort sets TXERIF with it and TXABRT. */
__attribute__((unused))
static void enc_tx_end(enc28j60_model_t *m)
{
    m->common[ENC_ECON1] &= (uint8_t)~ENC_ECON1_TXRTS;
    m->common[ENC_ESTAT] |= m->tx_end_estat;
    m->common[ENC_EIR] |= ENC_EIR_TXIF;
    if (m->tx_end_estat & ENC_ESTAT_TXABRT) {
        m->common[ENC_EIR] |= ENC_EIR_TXERIF;
        m->tx_stalled = m->tx_abort_stalls;
    }
    m->tx_end_estat = 0;
}

/* Side effects of any write to ECON1 */
__attribute__((unused))
static void enc_econ1_written(enc28j60_model_t *m, uint8_t old)
{
    uint8_t now  = m->common[ENC_ECON1];
    uint8_t rose = (uint8_t)(now & ~old);
    uint8_t fell = (uint8_t)(old & ~now);

    enc_update_bank(m);

    if (rose & ENC_ECON1_TXRST) {
        /* p.62: "the hardware will automatically clear the TXRTS bit and
         * abort the transmission" */
        if (now & ENC_ECON1_TXRTS) {
            m->common[ENC_ECON1] &= (uint8_t)~ENC_ECON1_TXRTS;
            m->common[ENC_EIR] |= ENC_EIR_TXIF;
        }
        m->tx_stalled = false;
    }
    /* Errata issue 12: "Clearing ECON1.TXRST may cause a new transmit error
     * interrupt event (EIR.TXERIF will become set)." */
    if (fell & ENC_ECON1_TXRST)
        m->common[ENC_EIR] |= ENC_EIR_TXERIF;
    if (rose & ENC_ECON1_TXRTS) {
        m->tx_econ1_read_count = 0;
        if (now & ENC_ECON1_TXRST)
            m->tx_start_in_reset++;
        m->eir_at_tx_start = m->common[ENC_EIR];
    }
    /* p.70: TXIF also sets on "transmission cancellation by the host" */
    if (fell & ENC_ECON1_TXRTS)
        m->common[ENC_EIR] |= ENC_EIR_TXIF;
}

/* Get the 16-bit buffer read pointer from ERDPTL/H */
__attribute__((unused))
static uint16_t enc_get_erdpt(enc28j60_model_t *m)
{
    return (uint16_t)((((uint16_t)m->regs[0][ENC_ERDPTH] << 8) |
           m->regs[0][ENC_ERDPTL]) & ENC_BUF_LAST);
}

/* Get the 16-bit buffer write pointer from EWRPTL/H */
__attribute__((unused))
static uint16_t enc_get_ewrpt(enc28j60_model_t *m)
{
    return (uint16_t)((((uint16_t)m->regs[0][ENC_EWRPTH] << 8) |
           m->regs[0][ENC_EWRPTL]) & ENC_BUF_LAST);
}

/* The programmed ends of the receive buffer */
__attribute__((unused))
static uint16_t enc_get_erxst(enc28j60_model_t *m)
{
    return (uint16_t)((((uint16_t)m->regs[0][ENC_ERXSTH] << 8) |
           m->regs[0][ENC_ERXSTL]) & ENC_BUF_LAST);
}

__attribute__((unused))
static uint16_t enc_get_erxnd(enc28j60_model_t *m)
{
    return (uint16_t)((((uint16_t)m->regs[0][ENC_ERXNDH] << 8) |
           m->regs[0][ENC_ERXNDL]) & ENC_BUF_LAST);
}

/* p.30: "if the last byte in the receive buffer is read (ERDPT = ERXND), the
 * ERDPT Pointer will change to the beginning of the receive buffer (ERXST)",
 * and past 1FFFh it wraps to 0000h */
__attribute__((unused))
static uint16_t enc_next_rd(enc28j60_model_t *m, uint16_t ptr)
{
    if (ptr == enc_get_erxnd(m))
        return enc_get_erxst(m);
    return (uint16_t)((ptr + 1U) & ENC_BUF_LAST);
}

/* ================================================================
 * Soft reset: restore defaults
 * ================================================================ */

__attribute__((unused))
static void enc_soft_reset(enc28j60_model_t *m)
{
    /* CLKRDY resets to 0 on Power-on Reset, unaffected on all other Resets */
    uint8_t clkrdy = m->common[ENC_ESTAT] & ENC_ESTAT_CLKRDY;
    /* Table 3-2 note 3: ECOCON is '---- -uuu' on all but Power-on Reset */
    uint8_t ecocon = m->regs[3][ENC_ECOCON];
    memcpy(m->regs, m->reset_regs, sizeof(m->regs));
    memcpy(m->common, m->reset_common, sizeof(m->common));
    m->common[ENC_ESTAT] |= clkrdy;
    m->regs[3][ENC_ECOCON] = ecocon;
    m->ms_since_src = 0;
    m->current_bank = 0;
    m->buf_rdptr = 0;
    m->buf_wrptr = 0;
    m->tx_econ1_read_count = 0;
    m->tx_captured_len = 0;
    m->tx_stalled = false;  /* p.62: a System Reset includes the TX reset */
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
            /* Read Control Register. DS39662C p.29: an ETH register
             * shifts out at once, a MAC or MII register after a dummy
             * byte. */
            m->phase = enc_is_mac_mii(m->current_bank, addr)
                     ? ENC_PH_READ_DUMMY : ENC_PH_READ_DATA;
        } else if (op == ENC_OP_WCR) {
            m->phase = ENC_PH_WRITE_DATA;
        } else if (op == ENC_OP_BFS) {
            m->phase = ENC_PH_BFS_DATA;
        } else if (op == ENC_OP_BFC) {
            m->phase = ENC_PH_BFC_DATA;
        }
        return 0xFFU;
    }

    case ENC_PH_READ_DUMMY:
        /* Not the register: a driver that skips this byte reads junk */
        m->phase = ENC_PH_READ_DATA;
        return 0xFFU;

    case ENC_PH_READ_DATA: {
        if (enc_is_mac_mii(m->current_bank, m->spi_addr))
            enc_note_ready_needed(m);
        /* The Oscillator Start-up Timer runs out after clkrdy_after reads */
        if (m->spi_addr == ENC_ESTAT) {
            m->estat_read_count++;
            if (m->clkrdy_after > 0 &&
                m->estat_read_count >= m->clkrdy_after)
                m->common[ENC_ESTAT] |= ENC_ESTAT_CLKRDY;
            if ((m->common[ENC_ESTAT] & ENC_ESTAT_RXBUSY) &&
                m->rxbusy_after > 0 &&
                ++m->rxbusy_reads >= m->rxbusy_after)
                m->common[ENC_ESTAT] &= (uint8_t)~ENC_ESTAT_RXBUSY;
        }
        uint8_t val = *enc_reg_ptr(m, m->spi_addr);
        /* A PHY write ends after phy_busy_after reads that saw BUSY */
        if (m->current_bank == 3U && m->spi_addr == ENC_MISTAT &&
            (val & ENC_MISTAT_BUSY)) {
            m->phy_busy_reads++;
            if (m->phy_busy_after > 0 &&
                m->phy_busy_reads >= m->phy_busy_after)
                m->regs[3][ENC_MISTAT] &= (uint8_t)~ENC_MISTAT_BUSY;
        }
        /* Special handling: reading ECON1 may auto-clear TXRTS */
        if (m->spi_addr == ENC_ECON1) {
            if (m->common[ENC_ECON1] & ENC_ECON1_TXRTS) {
                m->tx_econ1_read_count++;
                if (m->tx_complete_after > 0 && !m->tx_stalled &&
                    !(m->common[ENC_ECON1] & ENC_ECON1_TXRST) &&
                    m->tx_econ1_read_count >= m->tx_complete_after) {
                    enc_tx_end(m);
                    val = m->common[ENC_ECON1];
                }
            }
        }
        return val;
    }

    case ENC_PH_WRITE_DATA: {
        if (enc_is_mac_mii(m->current_bank, m->spi_addr))
            enc_note_ready_needed(m);
        /* DS39662C p.45: clear RXEN before altering the MAC address */
        if (m->current_bank == 3U && m->spi_addr <= ENC_MAADR2 &&
            ((m->common[ENC_ECON1] & ENC_ECON1_RXEN) ||
             (m->common[ENC_ESTAT] & ENC_ESTAT_RXBUSY)))
            m->mac_write_rx_active++;
        uint8_t econ1 = m->common[ENC_ECON1];
        enc_write_reg(m, m->spi_addr, tx);
        if (m->spi_addr == ENC_ECON1) {
            enc_econ1_written(m, econ1);
        }
        if (m->spi_addr == ENC_ECON2) {
            enc_econ2_written(m);
        }
        /* DS39662C p.21: writing MIWRH starts the PHY write */
        if (m->current_bank == 2 && m->spi_addr == ENC_MIWRH) {
            uint8_t phy_addr = m->regs[2][ENC_MIREGADR] & 0x1FU;
            if (m->regs[3][ENC_MISTAT] & ENC_MISTAT_BUSY)
                m->phy_write_while_busy++;
            if (m->common[ENC_ECON1] & ENC_ECON1_RXEN)
                m->phy_write_rx_on++;
            m->phy_regs[phy_addr] = (uint16_t)(((uint16_t)tx << 8) |
                                               m->regs[2][ENC_MIWRL]);
            m->regs[3][ENC_MISTAT] |= ENC_MISTAT_BUSY;
            m->phy_busy_reads = 0;
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
        if (m->spi_addr == ENC_ECON1 &&
            (tx & (ENC_ECON1_RXEN | ENC_ECON1_TXRTS)))
            enc_note_ready_needed(m);
        uint8_t econ1 = m->common[ENC_ECON1];
        enc_bfs_reg(m, m->spi_addr, tx);
        if (m->spi_addr == ENC_ECON1) {
            enc_econ1_written(m, econ1);
        }
        if (m->spi_addr == ENC_ECON2) {
            enc_econ2_written(m);
        }
        m->phase = ENC_PH_IDLE;
        return 0xFFU;
    }

    case ENC_PH_BFC_DATA: {
        uint8_t econ1 = m->common[ENC_ECON1];
        enc_bfc_reg(m, m->spi_addr, tx);
        if (m->spi_addr == ENC_ECON1) {
            enc_econ1_written(m, econ1);
        }
        m->phase = ENC_PH_IDLE;
        return 0xFFU;
    }

    case ENC_PH_READ_BUF: {
        /* Read from buffer memory at buf_rdptr. p.30: with AUTOINC clear the
         * same byte comes out again. */
        uint8_t val = m->buffer[m->buf_rdptr];
        if (m->common[ENC_ECON2] & ENC_ECON2_AUTOINC) {
            m->buf_rdptr = enc_next_rd(m, m->buf_rdptr);
        }
        /* Update ERDPT registers to track auto-increment */
        m->regs[0][ENC_ERDPTL] = (uint8_t)(m->buf_rdptr & 0xFFU);
        m->regs[0][ENC_ERDPTH] = (uint8_t)(m->buf_rdptr >> 8);
        return val;
    }

    case ENC_PH_WRITE_BUF: {
        /* DS39662C p.42: no touching the bytes of a pending transmission */
        if (m->common[ENC_ECON1] & ENC_ECON1_TXRTS)
            m->tx_write_while_pending++;
        /* Write to buffer memory at buf_wrptr */
        m->buffer[m->buf_wrptr] = tx;
        /* Also capture into tx_captured for test verification */
        if (m->tx_captured_len < (int)sizeof(m->tx_captured)) {
            m->tx_captured[m->tx_captured_len++] = tx;
        }
        /* p.31: "If address, 1FFFh, is written with AUTOINC set, the Write
         * Pointer will increment to 0000h." */
        if (m->common[ENC_ECON2] & ENC_ECON2_AUTOINC) {
            m->buf_wrptr = (uint16_t)((m->buf_wrptr + 1U) & ENC_BUF_LAST);
        }
        /* Update EWRPT registers */
        m->regs[0][ENC_EWRPTL] = (uint8_t)(m->buf_wrptr & 0xFFU);
        m->regs[0][ENC_EWRPTH] = (uint8_t)(m->buf_wrptr >> 8);
        return 0xFFU;
    }

    default:
        break;
    } /* switch */

    return 0xFFU;
}

__attribute__((unused))
static void enc28j60_model_delay_ms(void *ctx, uint32_t ms)
{
    enc28j60_model_t *m = (enc28j60_model_t *)ctx;
    m->delay_count++;
    if (m->ms_since_src >= 0)
        m->ms_since_src += (int)ms;
}

/* ================================================================
 * Model initialization
 * ================================================================ */

__attribute__((unused))
static void enc28j60_model_init(enc28j60_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* Set reset defaults */

    /* Table 3-2: every Value on Reset that is not 0 */
    m->reset_regs[0][ENC_ERDPTL]   = 0xFAU;
    m->reset_regs[0][ENC_ERDPTH]   = 0x05U;
    m->reset_regs[0][ENC_ERXSTL]   = 0xFAU;
    m->reset_regs[0][ENC_ERXSTH]   = 0x05U;
    m->reset_regs[0][ENC_ERXNDL]   = 0xFFU;
    m->reset_regs[0][ENC_ERXNDH]   = 0x1FU;
    m->reset_regs[0][ENC_ERXRDPTL] = 0xFAU;
    m->reset_regs[0][ENC_ERXRDPTH] = 0x05U;
    m->reset_regs[1][ENC_ERXFCON]  = 0xA1U;
    m->reset_regs[2][ENC_MACLCON1] = 0x0FU;
    m->reset_regs[2][ENC_MACLCON2] = 0x37U;
    m->reset_regs[2][ENC_MAMXFLH]  = 0x06U;
    m->reset_regs[3][ENC_ECOCON]   = 0x04U;
    m->reset_regs[3][ENC_EPAUSH]   = 0x10U;
    m->reset_common[ENC_ECON2]     = ENC_ECON2_AUTOINC;

    /* EREVID (bank 3, 0x12) = 0x06 (silicon rev 6) */
    m->reset_regs[3][ENC_EREVID] = 0x06U;

    /* Mark EREVID as read-only */
    m->readonly[3][ENC_EREVID] = true;
    /* Mark ESTAT as read-only */
    m->readonly_common[ENC_ESTAT] = true;
    /* EIR: WCR leaves it alone, BFC clears its R/C bits */
    m->readonly_common[ENC_EIR] = true;
    /* p.19: ERXWRPT "is read-only" */
    m->readonly[0][ENC_ERXWRPTL] = true;
    m->readonly[0][ENC_ERXWRPTH] = true;
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
    m->clkrdy_after = 3;        /* CLKRDY is 0 at POR, sets on the 3rd read */
    m->ms_since_src = -1;
    m->phy_busy_after = 2;      /* the 10.24 us PHY write outlasts a read */
    m->tx_complete_after = 1;   /* TXRTS clears after 1 read by default */
    m->tx_econ1_read_count = 0;
    m->tx_captured_len = 0;
    m->tx_abort_stalls = true;  /* the errata says "may": assume the worst */
}

/* ================================================================
 * Helper: inject an RX packet into the buffer at a given address
 *
 * Writes the ENC28J60 receive status vector (6 bytes) plus data.
 * Format: [next_pkt_lo, next_pkt_hi, len_lo, len_hi, status_lo, status_hi]
 * ================================================================ */

#define ENC_RX_CRC_LEN  4U
#define ENC_RX_CRC_MARK 0xC5U

/* The receive hardware stores bytes round the buffer as the read pointer
 * reads them back */
__attribute__((unused))
static void enc_rx_put(enc28j60_model_t *m, uint16_t *off, uint8_t val)
{
    m->buffer[*off] = val;
    *off = enc_next_rd(m, *off);
}

__attribute__((unused))
static void enc28j60_model_inject_rx(enc28j60_model_t *m, uint16_t addr,
                                     uint16_t next_pkt,
                                     const uint8_t *frame, uint16_t frame_len)
{
    uint16_t off = addr;
    /* Next packet pointer (little-endian) */
    enc_rx_put(m, &off, (uint8_t)(next_pkt & 0xFFU));
    enc_rx_put(m, &off, (uint8_t)(next_pkt >> 8));
    /* Byte count (little-endian): p.46 Table 7-3 counts the 4 CRC bytes too */
    uint16_t byte_count = (uint16_t)(frame_len + ENC_RX_CRC_LEN);
    enc_rx_put(m, &off, (uint8_t)(byte_count & 0xFFU));
    enc_rx_put(m, &off, (uint8_t)(byte_count >> 8));
    /* Status: Table 7-3 bit 23, Received Ok, is bit 7 of this first byte */
    enc_rx_put(m, &off, 0x80U);
    enc_rx_put(m, &off, 0x00U);
    /* Frame data */
    for (uint16_t i = 0; i < frame_len; i++) {
        enc_rx_put(m, &off, frame[i]);
    }
    /* The CRC follows the data. Nothing checks these bytes, so a marker will
     * do: a test can see that they stay out of the caller's frame. */
    for (uint16_t i = 0; i < ENC_RX_CRC_LEN; i++) {
        enc_rx_put(m, &off, ENC_RX_CRC_MARK);
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
    /* p.45: a packet in the buffer also sets EIR.PKTIF */
    if (count > 0U)
        m->common[ENC_EIR] |= ENC_EIR_PKTIF;
}

#endif /* HW_MODEL_ENC28J60_H */
