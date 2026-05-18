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
#define SD_CMD16   16    /* SET_BLOCKLEN */
#define SD_CMD17   17    /* READ_SINGLE_BLOCK */
#define SD_CMD24   24    /* WRITE_BLOCK */
#define SD_CMD55   55    /* APP_CMD prefix */
#define SD_CMD58   58    /* READ_OCR */
#define SD_ACMD41  41    /* SD_SEND_OP_COND (after CMD55) */

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

    /* Error injection */
    bool     fail_cmd0;        /* CMD0 returns no response */
    bool     fail_read;        /* CMD17 data: no 0xFE token */
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
} sd_model_t;

/* ================================================================
 * Response preparation
 * ================================================================ */

__attribute__((unused))
static void sd_model_prepare_response(sd_model_t *m)
{
    m->ext_len = 0;
    m->ext_idx = 0;

    switch (m->cmd) {
    case SD_CMD0:
        m->cmd0_count++;
        if (m->fail_cmd0) {
            m->r1 = 0xFFU;  /* no response */
        } else {
            m->r1 = SD_R1_IDLE;
            m->state = SD_STATE_IDLE;
        }
        m->phase = SD_PH_R1;
        return;

    case SD_CMD8:
        m->cmd8_count++;
        if (m->card_type == SD_MODEL_TYPE_SDV1) {
            /* SDv1 doesn't understand CMD8 */
            m->r1 = SD_R1_IDLE | SD_R1_ILLEGAL_CMD;
            m->phase = SD_PH_R1;
            return;
        }
        /* SDv2: R1 + 4-byte R7 response */
        m->r1 = SD_R1_IDLE;
        m->ext[0] = 0x00;
        m->ext[1] = 0x00;
        m->ext[2] = (uint8_t)((m->arg >> 8) & 0xFFU);   /* echo voltage */
        m->ext[3] = (uint8_t)(m->arg & 0xFFU);           /* echo pattern */
        m->ext_len = 4;
        m->phase = SD_PH_R1;
        return;

    case SD_CMD55:
        m->cmd55_seen = true;
        m->r1 = (m->state == SD_STATE_IDLE) ? SD_R1_IDLE : 0x00U;
        m->phase = SD_PH_R1;
        return;

    case SD_ACMD41:
        if (!m->cmd55_seen) {
            /* ACMD41 without CMD55 prefix: illegal */
            m->r1 = SD_R1_ILLEGAL_CMD;
            m->phase = SD_PH_R1;
            return;
        }
        m->cmd55_seen = false;
        m->acmd41_count++;
        if (m->acmd41_remain > 0) {
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
        m->r1 = 0x00U;
        /* OCR register */
        if (m->card_type == SD_MODEL_TYPE_SDV2_HC) {
            m->ext[0] = 0xC0U;  /* CCS=1, power up complete */
        } else {
            m->ext[0] = 0x80U;  /* CCS=0, power up complete */
        }
        m->ext[1] = 0xFFU;
        m->ext[2] = 0x80U;
        m->ext[3] = 0x00U;
        m->ext_len = 4;
        m->state = SD_STATE_TRANSFER;
        m->phase = SD_PH_R1;
        return;

    case SD_CMD16:
        /* SET_BLOCKLEN -- always OK in transfer state */
        m->r1 = (m->state >= SD_STATE_READY) ? 0x00U : SD_R1_ILLEGAL_CMD;
        m->phase = SD_PH_R1;
        return;

    case SD_CMD17:
        /* READ_SINGLE_BLOCK */
        if (m->fail_cmd17) {
            m->r1 = SD_R1_ILLEGAL_CMD;
            m->phase = SD_PH_R1;
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

    case SD_CMD24:
        /* WRITE_BLOCK */
        if (m->fail_cmd24) {
            m->r1 = SD_R1_ILLEGAL_CMD;
            m->phase = SD_PH_R1;
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
        m->r1 = SD_R1_ILLEGAL_CMD;
        m->phase = SD_PH_R1;
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
    m->phase = SD_PH_WAIT_CMD;
    m->byte_idx = 0;
}

__attribute__((unused))
static void sd_model_cs_high(void *ctx)
{
    sd_model_t *m = (sd_model_t *)ctx;
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
        /* Skip 0xFF dummy bytes (bus idle / MOSI high) */
        if (tx == 0xFFU) return 0xFFU;
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
        sd_model_prepare_response(m);
        return 0xFFU;

    case SD_PH_R1: {
        uint8_t r = m->r1;
        if (m->ext_len > 0) {
            m->phase = SD_PH_EXT;
        } else if (m->cmd == SD_CMD17 && m->r1 == 0x00U) {
            /* Transition to read data phase */
            if (m->fail_read) {
                m->phase = SD_PH_IDLE;  /* no data token sent */
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
        m->phase = SD_PH_WRITE_BUSY;
        m->byte_idx = 0;
        if (m->fail_write) return SD_DATA_RESP_WRITE;  /* rejected */
        if (m->fail_crc)  return SD_DATA_RESP_CRC;     /* CRC error */
        /* Success: store the written data */
        m->write_captured = true;
        if (m->last_write_block < SD_MODEL_MAX_BLOCKS) {
            memcpy(m->blocks[m->last_write_block], m->write_buf,
                   SD_MODEL_BLOCK_SIZE);
            m->block_valid[m->last_write_block] = true;
        }
        return SD_DATA_RESP_OK;  /* accepted */
    }

    case SD_PH_WRITE_BUSY:
        /* Card busy for zero cycles (return 0xFF = ready immediately) */
        m->phase = SD_PH_IDLE;
        return 0xFFU;

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
