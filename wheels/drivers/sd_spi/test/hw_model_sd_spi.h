// SPDX-License-Identifier: MIT
/*
 * hw_model_sd_spi.h -- SD card SPI protocol hardware model
 *
 * Models an SD card's SPI command-response protocol:
 * - Command frame: 0x40|CMD (1 byte) + arg (4 bytes) + CRC (1 byte)
 *   → R1 response (1 byte), optionally followed by extended data
 * - Command table: CMD0 (GO_IDLE, R1), CMD8 (SEND_IF_COND, R7),
 *   CMD55 (APP_CMD, R1), ACMD41 (SD_SEND_OP_COND, R1),
 *   CMD16 (SET_BLOCKLEN, R1), CMD17 (READ_SINGLE_BLOCK, R1 + data),
 *   CMD24 (WRITE_BLOCK, R1 + busy), CMD58 (READ_OCR, R3)
 * - Card state machine: power-on → idle → ready → transfer
 * - Configurable card type: SDv1, SDv2 SC, SDv2 HC (SDHC)
 * - Block read: returns pre-loaded data block + CRC
 * - Block write: captures written data into per-block storage
 * - sd_model_set_block() for pre-loading read data
 *
 * What Physical Layer Spec v3.01 asks of the card, and this model does:
 * - Before the first valid CMD0 the card is in SD mode and stays silent
 *   (7.2.2, 6.4.1). In SPI mode the CRC is off, except for CMD8.
 * - The idle bit is set in every R1 until ACMD41 completes (7.3.2.1), and
 *   only CMD8, ACMD41, CMD58 and CMD59 are valid until then (7.2.7).
 * - An SDHC card stays busy without HCS at the first ACMD41 (4.2.3).
 * - A write holds DataOut low while programming, also after a reselect
 *   (7.2.4). Programming errors show only in CMD13.
 * - A failed read sends a data error token (7.3.3.3).
 *
 * Header-only. Include from test_hw_model_sd_spi.c.
 */

#ifndef HW_MODEL_SD_SPI_H
#define HW_MODEL_SD_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- SD SPI commands --- */

#define SD_CMD0     0    /* GO_IDLE_STATE */
#define SD_CMD8     8    /* SEND_IF_COND */
#define SD_CMD13   13    /* SEND_STATUS */
#define SD_CMD16   16    /* SET_BLOCKLEN */
#define SD_CMD17   17    /* READ_SINGLE_BLOCK */
#define SD_CMD24   24    /* WRITE_BLOCK */
#define SD_CMD55   55    /* APP_CMD prefix */
#define SD_CMD58   58    /* READ_OCR */
#define SD_ACMD41  41    /* SD_SEND_OP_COND (after CMD55) */

#define SD_ACMD41_HCS  0x40000000U   /* Host Capacity Support */

/* --- R1 response flags --- */

#define SD_R1_IDLE         0x01U
#define SD_R1_ERASE_RESET  0x02U
#define SD_R1_ILLEGAL_CMD  0x04U
#define SD_R1_CRC_ERR      0x08U

/* --- Data tokens --- */

#define SD_DATA_TOKEN      0xFEU
#define SD_DATA_RESP_MASK  0x1FU
#define SD_DATA_RESP_OK    0x05U
#define SD_DATA_RESP_CRC   0x0BU
#define SD_DATA_RESP_WRITE 0x0DU

/* --- Block storage --- */

#define SD_MODEL_BLOCK_SIZE    512U
#define SD_MODEL_MAX_BLOCKS    16U    /* enough for testing */

/* --- Card states --- */

typedef enum {
    SD_STATE_POWER_ON = 0,   /* before CMD0 */
    SD_STATE_IDLE,           /* after CMD0 (SPI idle = bit 0 set) */
    SD_STATE_READY,          /* after ACMD41 completes */
    SD_STATE_TRANSFER,       /* after CMD58/CMD16, ready for data */
} sd_model_state_t;

/* --- Card type configuration --- */

typedef enum {
    SD_MODEL_TYPE_SDV2_HC = 0,   /* default: SDv2 SDHC */
    SD_MODEL_TYPE_SDV2_SC,       /* SDv2 standard capacity */
    SD_MODEL_TYPE_SDV1,          /* SDv1 */
} sd_model_card_type_t;

/* --- SPI transaction phases --- */

typedef enum {
    SD_PH_IDLE = 0,
    SD_PH_WAIT_CMD,       /* waiting for command byte (0x40|cmd) */
    SD_PH_ARG,            /* collecting 4 arg bytes */
    SD_PH_CRC,            /* next byte is CRC */
    SD_PH_R1,             /* returning R1 response */
    SD_PH_EXT,            /* returning extended response (R7/OCR) */
    SD_PH_READ_TOKEN,     /* about to send 0xFE data start token */
    SD_PH_READ_DATA,      /* sending 512 data bytes */
    SD_PH_READ_CRC,       /* sending 2 CRC bytes */
    SD_PH_WRITE_TOKEN,    /* waiting for 0xFE from host */
    SD_PH_WRITE_DATA,     /* receiving 512 data bytes */
    SD_PH_WRITE_CRC,      /* receiving 2 CRC bytes from host */
    SD_PH_WRITE_RESP,     /* returning data response token */
    SD_PH_WRITE_BUSY,     /* returning busy (0x00) then 0xFF */
} sd_model_phase_t;

/* --- The hardware model --- */

typedef struct {
    /* Card configuration */
    sd_model_card_type_t card_type;
    sd_model_state_t     state;

    /* Block storage */
    uint8_t  blocks[SD_MODEL_MAX_BLOCKS][SD_MODEL_BLOCK_SIZE];
    bool     block_valid[SD_MODEL_MAX_BLOCKS];  /* has data been set? */

    /* Last written block */
    uint8_t  write_buf[SD_MODEL_BLOCK_SIZE];
    uint32_t last_write_block;
    bool     write_captured;

    /* SPI transaction state */
    sd_model_phase_t phase;
    uint8_t  cmd;              /* current command number */
    uint32_t arg;              /* 4-byte argument */
    uint8_t  crc_byte;         /* last byte of the command frame */
    int      byte_idx;         /* multi-purpose counter */

    /* Response state */
    uint8_t  r1;               /* pending R1 response */
    uint8_t  ext[4];           /* extended response bytes */
    int      ext_len;
    int      ext_idx;

    /* Read data state */
    uint32_t read_block;       /* block number for current read */

    /* Configurable behavior */
    int      acmd41_remain;    /* countdown: >0 means still initializing */
    bool     cmd55_seen;       /* flag for APP_CMD prefix */
    bool     cmd8_seen;        /* a valid CMD8 came since the last CMD0 */
    bool     acmd41_seen;      /* the first ACMD41 since CMD0 has come */
    bool     no_hcs;           /* that first ACMD41 had no usable HCS */
    int      busy_polls;       /* polls a write stays busy, -1 = forever */
    int      busy_remain;      /* polls left in the current programming */
    uint8_t  status_r2;        /* second byte of the next CMD13 response */

    /* Error injection */
    bool     fail_cmd0;        /* CMD0 returns no response */
    bool     fail_read;        /* CMD17 data: a data error token */
    uint8_t  read_error_token; /* the token fail_read sends */
    bool     silent_read;      /* CMD17 data: nothing after R1 */
    bool     fail_write;       /* write response = rejected */
    bool     fail_crc;         /* write response = CRC error */
    bool     fail_cmd17;       /* CMD17 R1 = error */
    bool     fail_cmd24;       /* CMD24 R1 = error */
    bool     fail_spi;         /* all xfer return 0xFF */

    /* Counters */
    int      cmd0_count;
    int      cmd8_count;
    int      acmd41_count;
    int      cmd58_count;
    int      delay_count;
    int      busy_seen;        /* polls answered with DataOut held low */
} sd_model_t;

/* ================================================================
 * Response preparation
 * ================================================================ */

/* CRC7 of section 4.5, generator x^7 + x^3 + 1. */
__attribute__((unused))
static uint8_t sd_model_crc7(const uint8_t *bytes, int n)
{
    uint8_t crc = 0;

    for (int i = 0; i < n; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t in = (uint8_t)((bytes[i] >> bit) & 1U);
            uint8_t top = (uint8_t)((crc >> 6) & 1U);

            crc = (uint8_t)((crc << 1) & 0x7FU);
            if (in != top)
                crc ^= 0x09U;
        }
    }
    return crc;
}

__attribute__((unused))
static bool sd_model_frame_crc_ok(const sd_model_t *m)
{
    const uint8_t frame[5] = {
        (uint8_t)(0x40U | m->cmd),
        (uint8_t)(m->arg >> 24), (uint8_t)(m->arg >> 16),
        (uint8_t)(m->arg >> 8),  (uint8_t)m->arg,
    };

    /* Seven CRC bits, then the end bit, which is always 1. */
    return m->crc_byte == (uint8_t)((sd_model_crc7(frame, 5) << 1) | 1U);
}

/* In idle state: the card is running the initializing process (7.3.2.1). */
__attribute__((unused))
static uint8_t sd_model_idle_bit(const sd_model_t *m)
{
    return (uint8_t)((m->state == SD_STATE_IDLE) ? SD_R1_IDLE : 0x00U);
}

__attribute__((unused))
static void sd_model_illegal(sd_model_t *m)
{
    m->r1 = (uint8_t)(sd_model_idle_bit(m) | SD_R1_ILLEGAL_CMD);
    m->phase = SD_PH_R1;
}

__attribute__((unused))
static void sd_model_prepare_response(sd_model_t *m)
{
    bool crc_ok = sd_model_frame_crc_ok(m);
    bool app_cmd = m->cmd55_seen;   /* APP_CMD covers the next command only */

    m->ext_len = 0;
    m->ext_idx = 0;
    m->cmd55_seen = false;

    /* 7.2.2: the first CMD0 "is received by the card while in SD mode and,
     * therefore, shall have a valid CRC field". 6.4.1: "CMD0 shall be the
     * first command to send the card to SPI mode". */
    if (m->state == SD_STATE_POWER_ON && (m->cmd != SD_CMD0 || !crc_ok)) {
        m->r1 = 0xFFU;
        m->phase = SD_PH_R1;
        return;
    }

    /* 7.2.7: the commands below are not valid in the idle state. */
    if (m->state == SD_STATE_IDLE &&
        (m->cmd == SD_CMD13 || m->cmd == SD_CMD16 ||
         m->cmd == SD_CMD17 || m->cmd == SD_CMD24)) {
        sd_model_illegal(m);
        return;
    }

    switch (m->cmd) {
    case SD_CMD0:
        m->cmd0_count++;
        if (m->fail_cmd0) {
            m->r1 = 0xFFU;  /* no response */
        } else {
            m->r1 = SD_R1_IDLE;
            m->state = SD_STATE_IDLE;
            m->cmd8_seen = false;
            m->acmd41_seen = false;
            m->no_hcs = false;
        }
        m->phase = SD_PH_R1;
        return;

    case SD_CMD8:
        m->cmd8_count++;
        if (m->card_type == SD_MODEL_TYPE_SDV1) {
            /* SDv1 doesn't understand CMD8 */
            sd_model_illegal(m);
            return;
        }
        /* 7.2.2: "The CMD8 CRC verification is always enabled." 7.3.2: with
         * a CRC error the card sends only the first byte. */
        if (!crc_ok) {
            m->r1 = (uint8_t)(sd_model_idle_bit(m) | SD_R1_CRC_ERR);
            m->phase = SD_PH_R1;
            return;
        }
        m->cmd8_seen = true;
        /* SDv2: R1 + 4-byte R7 response */
        m->r1 = sd_model_idle_bit(m);
        m->ext[0] = 0x00;
        m->ext[1] = 0x00;
        m->ext[2] = (uint8_t)((m->arg >> 8) & 0xFFU);   /* echo voltage */
        m->ext[3] = (uint8_t)(m->arg & 0xFFU);           /* echo pattern */
        m->ext_len = 4;
        m->phase = SD_PH_R1;
        return;

    case SD_CMD55:
        m->cmd55_seen = true;
        m->r1 = sd_model_idle_bit(m);
        m->phase = SD_PH_R1;
        return;

    case SD_ACMD41:
        if (!app_cmd) {
            /* ACMD41 without CMD55 prefix: illegal */
            sd_model_illegal(m);
            return;
        }
        m->acmd41_count++;
        /* 7.2.1: "The card checks the HCS bit in the OCR only at the first
         * ACMD41", and HCS means something only after CMD8. 4.2.3: "If HCS
         * is set to 0, SDHC and SDXC Cards never return ready status". */
        if (!m->acmd41_seen) {
            m->acmd41_seen = true;
            m->no_hcs = !m->cmd8_seen || !(m->arg & SD_ACMD41_HCS);
        }
        if (m->card_type == SD_MODEL_TYPE_SDV2_HC && m->no_hcs) {
            m->r1 = SD_R1_IDLE;
        } else if (m->acmd41_remain > 0) {
            m->acmd41_remain--;
            m->r1 = SD_R1_IDLE;  /* still initializing */
        } else {
            m->r1 = 0x00U;  /* ready */
            m->state = SD_STATE_READY;
        }
        m->phase = SD_PH_R1;
        return;

    case SD_CMD58:
        m->cmd58_count++;
        m->r1 = sd_model_idle_bit(m);
        /* OCR register. 5.1: bit 31 is set when the power up procedure has
         * finished, and bit 30 (CCS) is valid only then. */
        if (m->state == SD_STATE_IDLE) {
            m->ext[0] = 0x00U;
        } else if (m->card_type == SD_MODEL_TYPE_SDV2_HC) {
            m->ext[0] = 0xC0U;  /* CCS=1, power up complete */
        } else {
            m->ext[0] = 0x80U;  /* CCS=0, power up complete */
        }
        m->ext[1] = 0xFFU;
        m->ext[2] = 0x80U;
        m->ext[3] = 0x00U;
        m->ext_len = 4;
        if (m->state == SD_STATE_READY)
            m->state = SD_STATE_TRANSFER;
        m->phase = SD_PH_R1;
        return;

    case SD_CMD16:
        /* SET_BLOCKLEN -- always OK in transfer state */
        m->r1 = 0x00U;
        m->phase = SD_PH_R1;
        return;

    case SD_CMD17:
        /* READ_SINGLE_BLOCK */
        if (m->fail_cmd17) {
            sd_model_illegal(m);
            return;
        }
        m->r1 = 0x00U;
        /* Determine block number from argument.
         * SDHC: arg = block number
         * SDSC/SDv1: arg = byte address, so block = arg / 512 */
        if (m->card_type == SD_MODEL_TYPE_SDV2_HC) {
            m->read_block = m->arg;
        } else {
            m->read_block = m->arg / SD_MODEL_BLOCK_SIZE;
        }
        m->phase = SD_PH_R1;
        return;

    case SD_CMD13:
        /* SEND_STATUS: R2. 7.3.4: "error bits are cleared when read by the
         * host, regardless of the response format". */
        m->r1 = 0x00U;
        m->ext[0] = m->status_r2;
        m->status_r2 = 0x00U;
        m->ext_len = 1;
        m->phase = SD_PH_R1;
        return;

    case SD_CMD24:
        /* WRITE_BLOCK */
        if (m->fail_cmd24) {
            sd_model_illegal(m);
            return;
        }
        m->r1 = 0x00U;
        if (m->card_type == SD_MODEL_TYPE_SDV2_HC) {
            m->last_write_block = m->arg;
        } else {
            m->last_write_block = m->arg / SD_MODEL_BLOCK_SIZE;
        }
        m->phase = SD_PH_R1;
        return;

    default:
        /* Unknown command */
        sd_model_illegal(m);
        return;
    }
}

/* ================================================================
 * SPI HAL callbacks
 * ================================================================ */

__attribute__((unused))
static void sd_model_cs_low(void *ctx)
{
    sd_model_t *m = (sd_model_t *)ctx;
    /* 7.2.4: "If the card is reselected before the programming is finished,
     * the DataOut line will be forced back to low and all commands will be
     * rejected." */
    m->phase = (m->busy_remain != 0) ? SD_PH_WRITE_BUSY : SD_PH_WAIT_CMD;
    m->byte_idx = 0;
}

__attribute__((unused))
static void sd_model_cs_high(void *ctx)
{
    sd_model_t *m = (sd_model_t *)ctx;
    /* 7.2.4: programming goes on, and the card releases DataOut. */
    m->phase = SD_PH_IDLE;
}

__attribute__((unused))
static uint8_t sd_model_xfer(void *ctx, uint8_t tx)
{
    sd_model_t *m = (sd_model_t *)ctx;

    if (m->fail_spi) return 0xFFU;

    switch (m->phase) {
    case SD_PH_IDLE:
        return 0xFFU;

    case SD_PH_WAIT_CMD:
        /* A frame starts with bits 01. 0xFF dummies are the bus idling. */
        if ((tx & 0xC0U) != 0x40U) return 0xFFU;
        /* Command byte: 0x40 | cmd_number */
        m->cmd = (uint8_t)(tx & 0x3FU);
        m->arg = 0;
        m->byte_idx = 0;
        m->phase = SD_PH_ARG;
        return 0xFFU;

    case SD_PH_ARG:
        m->arg = (m->arg << 8) | tx;
        m->byte_idx++;
        if (m->byte_idx >= 4)
            m->phase = SD_PH_CRC;
        return 0xFFU;

    case SD_PH_CRC:
        /* CRC byte received, prepare response */
        m->crc_byte = tx;
        sd_model_prepare_response(m);
        return 0xFFU;

    case SD_PH_R1: {
        uint8_t r = m->r1;
        if (m->ext_len > 0) {
            m->phase = SD_PH_EXT;
        } else if (m->cmd == SD_CMD17 && m->r1 == 0x00U) {
            /* Transition to read data phase */
            if (m->silent_read) {
                m->phase = SD_PH_IDLE;  /* no token of any kind */
            } else {
                m->phase = SD_PH_READ_TOKEN;
            }
        } else if (m->cmd == SD_CMD24 && m->r1 == 0x00U) {
            m->phase = SD_PH_WRITE_TOKEN;
            m->byte_idx = 0;
        } else {
            m->phase = SD_PH_IDLE;
        }
        return r;
    }

    case SD_PH_EXT: {
        uint8_t val = m->ext[m->ext_idx++];
        if (m->ext_idx >= m->ext_len)
            m->phase = SD_PH_IDLE;
        return val;
    }

    case SD_PH_READ_TOKEN:
        /* 7.3.3.3: "If a read operation fails and the card cannot provide
         * the required data, it will send a data error token instead." */
        if (m->fail_read) {
            m->phase = SD_PH_IDLE;
            return m->read_error_token;
        }
        /* Send data start token */
        m->phase = SD_PH_READ_DATA;
        m->byte_idx = 0;
        return SD_DATA_TOKEN;

    case SD_PH_READ_DATA: {
        uint8_t val = 0x00U;
        if (m->read_block < SD_MODEL_MAX_BLOCKS &&
            m->block_valid[m->read_block]) {
            val = m->blocks[m->read_block][m->byte_idx];
        }
        m->byte_idx++;
        if (m->byte_idx >= (int)SD_MODEL_BLOCK_SIZE) {
            m->phase = SD_PH_READ_CRC;
            m->byte_idx = 0;
        }
        return val;
    }

    case SD_PH_READ_CRC:
        m->byte_idx++;
        if (m->byte_idx >= 2)
            m->phase = SD_PH_IDLE;
        return 0x00U;  /* dummy CRC bytes */

    case SD_PH_WRITE_TOKEN:
        /* Waiting for host to send 0xFE data start token */
        if (tx == SD_DATA_TOKEN) {
            m->phase = SD_PH_WRITE_DATA;
            m->byte_idx = 0;
        }
        return 0xFFU;

    case SD_PH_WRITE_DATA:
        if (m->byte_idx < (int)SD_MODEL_BLOCK_SIZE) {
            m->write_buf[m->byte_idx] = tx;
        }
        m->byte_idx++;
        if (m->byte_idx >= (int)SD_MODEL_BLOCK_SIZE) {
            m->phase = SD_PH_WRITE_CRC;
            m->byte_idx = 0;
        }
        return 0xFFU;

    case SD_PH_WRITE_CRC:
        m->byte_idx++;
        if (m->byte_idx >= 2)
            m->phase = SD_PH_WRITE_RESP;
        return 0xFFU;

    case SD_PH_WRITE_RESP: {
        m->phase = SD_PH_IDLE;
        m->byte_idx = 0;
        if (m->fail_write) return SD_DATA_RESP_WRITE;  /* rejected */
        if (m->fail_crc)  return SD_DATA_RESP_CRC;     /* CRC error */
        /* Success: store the written data */
        m->phase = SD_PH_WRITE_BUSY;
        m->busy_remain = m->busy_polls;
        m->write_captured = true;
        if (m->last_write_block < SD_MODEL_MAX_BLOCKS) {
            memcpy(m->blocks[m->last_write_block], m->write_buf,
                   SD_MODEL_BLOCK_SIZE);
            m->block_valid[m->last_write_block] = true;
        }
        return SD_DATA_RESP_OK;  /* accepted */
    }

    case SD_PH_WRITE_BUSY:
        /* 7.2.4: "As long as the card is busy programming, a continuous
         * stream of busy tokens will be sent to the host (effectively
         * holding the DataOut line low)." */
        if (m->busy_remain != 0) {
            if (m->busy_remain > 0)
                m->busy_remain--;
            m->busy_seen++;
            return 0x00U;
        }
        m->phase = SD_PH_IDLE;
        return 0xFFU;

    default:
        break;
    } /* switch */

    return 0xFFU;
}

__attribute__((unused))
static void sd_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ms;
    sd_model_t *m = (sd_model_t *)ctx;
    m->delay_count++;
}

/* ================================================================
 * Model initialization
 * ================================================================ */

__attribute__((unused))
static void sd_model_init(sd_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->card_type = SD_MODEL_TYPE_SDV2_HC;
    m->state = SD_STATE_POWER_ON;
    m->phase = SD_PH_IDLE;
    m->acmd41_remain = 0;  /* ready immediately by default */
    m->read_error_token = 0x01U;  /* Figure 7-13: Error */
}

/* ================================================================
 * Helper: pre-load a block with data for read tests
 * ================================================================ */

__attribute__((unused))
static void sd_model_set_block(sd_model_t *m, uint32_t block_num,
                               const uint8_t *data)
{
    if (block_num < SD_MODEL_MAX_BLOCKS) {
        memcpy(m->blocks[block_num], data, SD_MODEL_BLOCK_SIZE);
        m->block_valid[block_num] = true;
    }
}

/* ================================================================
 * Helper: get pointer to captured write data
 * ================================================================ */

__attribute__((unused))
static const uint8_t *sd_model_get_write_buf(const sd_model_t *m)
{
    return m->write_buf;
}

#endif /* HW_MODEL_SD_SPI_H */
