// SPDX-License-Identifier: MIT
/*
 * hw_model_lora_sx1276.h -- SX1276 LoRa transceiver hardware register model
 *
 * Models Semtech SX1276 SPI register behavior:
 * - SPI protocol: address byte bit 7 = write flag, bits [6:0] = register
 * - Register file 0x00-0x7F (128 registers)
 * - Two register pages at 0x0D-0x3F: LoRa, and FSK while LongRangeMode is 0
 *   or AccessSharedReg is 1. LongRangeMode only changes in Sleep
 * - OpMode state machine: SLEEP <-> STANDBY <-> TX / RX_SINGLE; TX and RX
 *   single return to Standby when they end
 * - FIFO simulation: write burst to 0x00 fills FIFO buffer, read from
 *   0x00 reads back; FifoAddrPtr (0x0D) tracks the current position. LoRa
 *   only, closed and cleared in Sleep, filled only in Standby. TX sends
 *   RegPayloadLength bytes from FifoTxBaseAddr
 * - IRQ flags (0x12, LoRa page): TxDone (bit 3) auto-set after TX, RxDone
 *   (bit 6) and RxTimeout (bit 7) injectable for RX; write-1-to-clear
 * - Image calibration through RegImageCal (0x3B, FSK page) in FSK Standby
 * - Version register 0x42 read-only (default 0x12)
 *
 * Uses hw_reg_model_t for the underlying register file, with custom
 * SPI byte-level xfer to model the real chip protocol.
 */

#ifndef HW_MODEL_LORA_SX1276_H
#define HW_MODEL_LORA_SX1276_H

#include "hw_model.h"

/* --- SX1276 register addresses (matching lora_sx1276.c) --- */
#define SX_REG_FIFO              0x00
#define SX_REG_OP_MODE           0x01
#define SX_REG_FRF_MSB           0x06
#define SX_REG_FRF_MID           0x07
#define SX_REG_FRF_LSB           0x08
#define SX_REG_PA_CONFIG         0x09
#define SX_REG_PA_DAC            0x4D
#define SX_REG_FIFO_ADDR_PTR    0x0D
#define SX_REG_FIFO_TX_BASE     0x0E
#define SX_REG_FIFO_RX_BASE     0x0F
#define SX_REG_FIFO_RX_CURRENT  0x10
#define SX_REG_IRQ_FLAGS         0x12
#define SX_REG_RX_NB_BYTES      0x13
#define SX_REG_PKT_SNR           0x19
#define SX_REG_PKT_RSSI          0x1A
#define SX_REG_MODEM_CONFIG_1   0x1D
#define SX_REG_MODEM_CONFIG_2   0x1E
#define SX_REG_PAYLOAD_LEN      0x22
#define SX_REG_MODEM_CONFIG_3   0x26
#define SX_REG_IMAGE_CAL         0x3B   /* FSK page */
#define SX_REG_VERSION           0x42

/* --- RegOpMode bits beside the mode field --- */
#define SX_OP_LONG_RANGE      0x80
#define SX_OP_ACCESS_SHARED   0x40

/* --- OpMode mode bits (low 3 bits of RegOpMode) --- */
#define SX_MODE_SLEEP         0x00
#define SX_MODE_STANDBY       0x01
#define SX_MODE_TX            0x03
#define SX_MODE_RX_SINGLE     0x06

/* --- IRQ flag bits --- */
#define SX_IRQ_TX_DONE        0x08
#define SX_IRQ_PAYLOAD_CRC    0x20
#define SX_IRQ_RX_DONE        0x40
#define SX_IRQ_RX_TIMEOUT     0x80

/* --- RegImageCal bits (p103) --- */
#define SX_IMAGE_CAL_START    0x40
#define SX_IMAGE_CAL_RUNNING  0x20
#define SX_IMAGE_CAL_WRITABLE 0x87
#define SX_IMAGE_CAL_MS       10u   /* p53: "approximately 10ms" */

/* --- Default version for SX1276 --- */
#define SX_VERSION_DEFAULT    0x12

typedef struct {
    hw_reg_model_t hw;
    /* FIFO: 256-byte circular buffer mirroring real SX1276 */
    uint8_t  fifo[256];
    /* SPI transaction state */
    uint8_t  spi_addr;
    int      spi_byte_idx;
    bool     spi_is_write;
    bool     spi_is_fifo_burst;   /* burst access to REG_FIFO */
    /* Counters */
    int      reset_count;
    int      delay_count;
    int      tx_done_count;       /* times TX completed */
    /* TX behavior: how many delay_ms calls before TxDone fires */
    int      tx_done_after_delays;
    /* RX behavior: how many delay_ms calls before RxDone fires */
    int      rx_done_after_delays;
    bool     rx_inject_crc_error;
    /* RX behavior: how many delay_ms calls before RxTimeout fires */
    int      rx_timeout_after_delays;
    /* What the modem took from the FIFO for the last transmission */
    uint8_t  tx_payload[256];
    int      tx_len;
    /* FSK register page, 0x0D-0x3F */
    uint8_t  fsk_page[0x40];
    /* Image calibrations started, and Frf at the last one */
    int      image_cal_count;
    uint32_t image_cal_frf;
    uint32_t image_cal_left_ms;
} sx1276_model_t;

static bool sx1276_model_is_lora(const sx1276_model_t *m)
{
    return (m->hw.regs[SX_REG_OP_MODE] & SX_OP_LONG_RANGE) != 0;
}

static uint8_t sx1276_model_mode(const sx1276_model_t *m)
{
    return m->hw.regs[SX_REG_OP_MODE] & 0x07;
}

/* p108: "the FSK register are only accessible in FSK mode", and in LoRa mode
 * AccessSharedReg "will grant access to the FSK register 0x0D up to the
 * register 0x3F" */
static bool sx1276_model_on_fsk_page(const sx1276_model_t *m, uint8_t addr)
{
    if (addr < 0x0D || addr > 0x3F) return false;
    return !sx1276_model_is_lora(m) ||
           (m->hw.regs[SX_REG_OP_MODE] & SX_OP_ACCESS_SHARED) != 0;
}

static void sx1276_model_fsk_write(sx1276_model_t *m, uint8_t addr, uint8_t val)
{
    if (addr != SX_REG_IMAGE_CAL) {
        m->fsk_page[addr] = val;
        return;
    }
    m->fsk_page[addr] = (uint8_t)((val & SX_IMAGE_CAL_WRITABLE) |
                                  (m->fsk_page[addr] & (uint8_t)~SX_IMAGE_CAL_WRITABLE));
    /* p103: "Triggers the IQ and RSSI calibration when set in Standby mode."
     * p53: "the radio must be temporarily returned to FSK/OOK mode" */
    if ((val & SX_IMAGE_CAL_START) && !sx1276_model_is_lora(m) &&
        sx1276_model_mode(m) == SX_MODE_STANDBY) {
        m->fsk_page[addr] |= SX_IMAGE_CAL_RUNNING;
        m->image_cal_left_ms = SX_IMAGE_CAL_MS;
        m->image_cal_count++;
        m->image_cal_frf = ((uint32_t)m->hw.regs[SX_REG_FRF_MSB] << 16) |
                           ((uint32_t)m->hw.regs[SX_REG_FRF_MID] << 8) |
                           m->hw.regs[SX_REG_FRF_LSB];
    }
}

/* --- write callback: OpMode transitions + IRQ clear logic --- */

__attribute__((unused))
static void sx1276_on_opmode_write(void *user, uint8_t reg, uint8_t old_val,
                                   uint8_t new_val)
{
    (void)reg;
    (void)old_val;
    sx1276_model_t *m = (sx1276_model_t *)user;

    /*
     * The real SX1276 transitions modes when you write RegOpMode.
     * The mode field is bits [2:0], with bit 7 = LoRa mode.
     * On read-back the chip reflects the actual mode.
     * We model instant transitions (the write already happened via
     * hw_model__write_reg), so the register already holds new_val.
     */
    uint8_t mode = new_val & 0x07;

    if (mode == SX_MODE_TX) {
        /* TX starts: reset the delay counter so we can track
         * when to set TxDone */
        m->delay_count = 0;
    }
    if (mode == SX_MODE_RX_SINGLE) {
        /* RX starts: reset the delay counter */
        m->delay_count = 0;
    }
    if (mode == SX_MODE_SLEEP) {
        /* p35: "The FIFO data buffer is cleared when the device is put in
         * SLEEP mode" */
        memset(m->fifo, 0, sizeof(m->fifo));
    }
}

__attribute__((unused))
static void sx1276_on_irq_write(void *user, uint8_t reg, uint8_t old_val,
                                uint8_t new_val)
{
    (void)reg;
    (void)user;
    (void)new_val;
    (void)old_val;
    /*
     * IRQ flags have write-1-to-clear semantics.
     * The hw_model writable_mask is 0xFF so the normal write path
     * would overwrite. We handle W1C in the xfer callback instead.
     * This callback is here as a placeholder for future extensions.
     */
}

/* --- SPI HAL callbacks --- */

__attribute__((unused))
static void sx1276_model_cs_low(void *ctx)
{
    sx1276_model_t *m = (sx1276_model_t *)ctx;
    m->spi_byte_idx = 0;
    m->spi_addr = 0;
    m->spi_is_write = false;
    m->spi_is_fifo_burst = false;
}

__attribute__((unused))
static void sx1276_model_cs_high(void *ctx)
{
    (void)ctx;
    /* Transaction complete */
}

__attribute__((unused))
static uint8_t sx1276_model_xfer(void *ctx, uint8_t tx)
{
    sx1276_model_t *m = (sx1276_model_t *)ctx;

    if (m->spi_byte_idx == 0) {
        /* First byte: address + R/W flag */
        m->spi_is_write = (tx & 0x80) != 0;
        m->spi_addr = tx & 0x7F;
        m->spi_is_fifo_burst = (m->spi_addr == SX_REG_FIFO);
        m->spi_byte_idx = 1;
        return 0x00;  /* SX1276 returns 0x00 during address byte */
    }

    /* Subsequent bytes: data phase */
    if (m->spi_is_write) {
        /* --- WRITE --- */
        if (m->spi_is_fifo_burst) {
            /* FIFO write: data goes into fifo[] at FifoAddrPtr, which
             * auto-increments (wraps at 256). p38: "The LoRaTM FIFO can only
             * be filled in Standby mode." */
            if (sx1276_model_is_lora(m) &&
                sx1276_model_mode(m) == SX_MODE_STANDBY) {
                uint8_t ptr = m->hw.regs[SX_REG_FIFO_ADDR_PTR];
                m->fifo[ptr] = tx;
                m->hw.regs[SX_REG_FIFO_ADDR_PTR] = (uint8_t)(ptr + 1);
            }
            m->spi_byte_idx++;
            return 0x00;
        }
        if (sx1276_model_on_fsk_page(m, m->spi_addr)) {
            sx1276_model_fsk_write(m, m->spi_addr, tx);
            m->spi_byte_idx++;
            return 0x00;
        }
        if (m->spi_addr == SX_REG_IRQ_FLAGS) {
            /* Write-1-to-clear: any bit written as 1 clears that flag */
            m->hw.regs[SX_REG_IRQ_FLAGS] =
                (uint8_t)(m->hw.regs[SX_REG_IRQ_FLAGS] & ~tx);
            m->spi_byte_idx++;
            return 0x00;
        }
        if (m->spi_addr == SX_REG_OP_MODE &&
            (m->hw.regs[SX_REG_OP_MODE] & 0x07) != SX_MODE_SLEEP) {
            /* LongRangeMode only changes in Sleep */
            tx = (uint8_t)((tx & 0x7F) | (m->hw.regs[SX_REG_OP_MODE] & 0x80));
        }
        /* Normal register write */
        hw_model__write_reg(&m->hw, m->spi_addr, tx);
        m->spi_byte_idx++;
        return 0x00;
    }

    /* --- READ --- */
    if (m->spi_is_fifo_burst) {
        /* FIFO read: data comes from fifo[] at FifoAddrPtr. p34: "uniquely
         * accessible in LoRa mode", "in all operating modes except sleep" */
        uint8_t val = 0x00;
        if (sx1276_model_is_lora(m) && sx1276_model_mode(m) != SX_MODE_SLEEP) {
            uint8_t ptr = m->hw.regs[SX_REG_FIFO_ADDR_PTR];
            val = m->fifo[ptr];
            m->hw.regs[SX_REG_FIFO_ADDR_PTR] = (uint8_t)(ptr + 1);
        }
        m->spi_byte_idx++;
        return val;
    }
    if (sx1276_model_on_fsk_page(m, m->spi_addr)) {
        uint8_t fsk = m->fsk_page[m->spi_addr];
        m->spi_byte_idx++;
        return fsk;
    }
    /* Normal register read */
    uint8_t val = m->hw.regs[m->spi_addr];
    m->spi_byte_idx++;
    return val;
}

__attribute__((unused))
static void sx1276_model_reset(void *ctx)
{
    sx1276_model_t *m = (sx1276_model_t *)ctx;
    m->reset_count++;
    /* Hardware reset: all registers go to POR defaults */
    hw_model_reset(&m->hw);
    memset(m->fifo, 0, sizeof(m->fifo));
    memset(m->fsk_page, 0, sizeof(m->fsk_page));
    m->fsk_page[SX_REG_IMAGE_CAL] = 0x02;  /* TempThreshold 01b, p103 */
    m->image_cal_left_ms = 0;
}

/* The modem takes RegPayloadLength bytes from FifoTxBaseAddr (p35) */
static void sx1276_model_capture_tx(sx1276_model_t *m)
{
    uint8_t base = m->hw.regs[SX_REG_FIFO_TX_BASE];
    m->tx_len = m->hw.regs[SX_REG_PAYLOAD_LEN];
    for (int i = 0; i < m->tx_len; i++) {
        m->tx_payload[i] = m->fifo[(uint8_t)(base + i)];
    }
}

static void sx1276_model_to_standby(sx1276_model_t *m)
{
    m->hw.regs[SX_REG_OP_MODE] =
        (uint8_t)((m->hw.regs[SX_REG_OP_MODE] & 0xF8) | SX_MODE_STANDBY);
}

__attribute__((unused))
static void sx1276_model_delay_ms(void *ctx, uint32_t ms)
{
    sx1276_model_t *m = (sx1276_model_t *)ctx;
    m->delay_count++;

    if (m->image_cal_left_ms > 0) {
        m->image_cal_left_ms -= (ms < m->image_cal_left_ms) ? ms
                                                            : m->image_cal_left_ms;
        if (m->image_cal_left_ms == 0) {
            m->fsk_page[SX_REG_IMAGE_CAL] &= (uint8_t)~SX_IMAGE_CAL_RUNNING;
        }
    }

    /* The flags below belong to the LoRa modem */
    if (!sx1276_model_is_lora(m)) return;

    /* Simulate hardware: set IRQ flags after configured number of delays.
     * Check current mode to decide which flag to set. */
    uint8_t mode = sx1276_model_mode(m);

    if (mode == SX_MODE_TX && m->tx_done_after_delays > 0 &&
        m->delay_count >= m->tx_done_after_delays) {
        sx1276_model_capture_tx(m);
        m->hw.regs[SX_REG_IRQ_FLAGS] |= SX_IRQ_TX_DONE;
        m->tx_done_count++;
        /* TX done: chip auto-returns to standby */
        sx1276_model_to_standby(m);
    }

    if (mode == SX_MODE_RX_SINGLE && m->rx_done_after_delays > 0 &&
        m->delay_count >= m->rx_done_after_delays) {
        m->hw.regs[SX_REG_IRQ_FLAGS] |= SX_IRQ_RX_DONE;
        m->hw.regs[0x1C] |= 0x40;  /* CrcOnPayload */
        if (m->rx_inject_crc_error) {
            m->hw.regs[SX_REG_IRQ_FLAGS] |= SX_IRQ_PAYLOAD_CRC;
        }
        /* p40: "Following the RxDone interrupt the radio goes to Standby" */
        sx1276_model_to_standby(m);
    } else if (mode == SX_MODE_RX_SINGLE && m->rx_timeout_after_delays > 0 &&
               m->delay_count >= m->rx_timeout_after_delays) {
        /* p40: "generates the RxTimeout interrupt and goes back to Standby" */
        m->hw.regs[SX_REG_IRQ_FLAGS] |= SX_IRQ_RX_TIMEOUT;
        sx1276_model_to_standby(m);
    }
}

/* --- Model initialization --- */

static void sx1276_model_init(sx1276_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR reset values from SX1276 datasheet */
    hw_model_set_reset_value(&m->hw, SX_REG_OP_MODE, 0x09);  /* FSK+Standby */
    hw_model_set_reset_value(&m->hw, SX_REG_VERSION, SX_VERSION_DEFAULT);
    hw_model_set_reset_value(&m->hw, SX_REG_FRF_MSB, 0x6C);  /* 434 MHz */
    hw_model_set_reset_value(&m->hw, SX_REG_FRF_MID, 0x80);
    hw_model_set_reset_value(&m->hw, SX_REG_FIFO_TX_BASE, 0x80);
    hw_model_set_reset_value(&m->hw, SX_REG_PAYLOAD_LEN, 0x01);
    hw_model_set_reset_value(&m->hw, SX_REG_PA_CONFIG, 0x4F);
    hw_model_set_reset_value(&m->hw, SX_REG_PA_DAC, 0x84);
    hw_model_set_reset_value(&m->hw, SX_REG_MODEM_CONFIG_1, 0x72);
    hw_model_set_reset_value(&m->hw, SX_REG_MODEM_CONFIG_2, 0x70);
    hw_model_set_reset_value(&m->hw, SX_REG_MODEM_CONFIG_3, 0x00);

    /* Version register is read-only */
    hw_model_set_readonly(&m->hw, SX_REG_VERSION);

    /* So are the LoRa status registers, mode "r" on p110-112 */
    hw_model_set_readonly(&m->hw, SX_REG_FIFO_RX_CURRENT);
    for (uint8_t r = SX_REG_RX_NB_BYTES; r <= 0x1C; r++) {
        hw_model_set_readonly(&m->hw, r);
    }

    /* IRQ flags: handled specially in xfer (W1C), mark writable to
     * allow hw_model__write_reg to pass through for the on_write cb */
    hw_model_set_on_write(&m->hw, SX_REG_OP_MODE, sx1276_on_opmode_write);

    sx1276_model_reset(m);
    m->reset_count = 0;

    /* Default TX/RX done timing: fire after 1 delay call */
    m->tx_done_after_delays = 1;
    m->rx_done_after_delays = 1;
}

/* --- Helper: inject RX packet into FIFO for receive tests --- */

__attribute__((unused))
static void sx1276_model_inject_rx(sx1276_model_t *m, const uint8_t *data,
                                   uint8_t len)
{
    /* Place data at RX base address in FIFO */
    uint8_t rx_base = m->hw.regs[SX_REG_FIFO_RX_BASE];
    for (uint8_t i = 0; i < len; i++) {
        m->fifo[(uint8_t)(rx_base + i)] = data[i];
    }
    /* Set the number of received bytes */
    m->hw.regs[SX_REG_RX_NB_BYTES] = len;
    /* Set the current RX address to the base */
    m->hw.regs[SX_REG_FIFO_RX_CURRENT] = rx_base;
}

#endif /* HW_MODEL_LORA_SX1276_H */
