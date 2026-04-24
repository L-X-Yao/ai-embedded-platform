/*****************************************************************************
 *   Copyright(C)2009-2022 by VSF Team                                       *
 *                                                                           *
 *  Licensed under the Apache License, Version 2.0 (the "License");          *
 *  you may not use this file except in compliance with the License.         *
 *  You may obtain a copy of the License at                                  *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 *  Unless required by applicable law or agreed to in writing, software      *
 *  distributed under the License is distributed on an "AS IS" BASIS,        *
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *  See the License for the specific language governing permissions and      *
 *  limitations under the License.                                           *
 *                                                                           *
 ****************************************************************************/

#ifndef __HAL_DRIVER_GIGADEVICE_GD32F4XX_USART_H__
#define __HAL_DRIVER_GIGADEVICE_GD32F4XX_USART_H__

/*============================ INCLUDES ======================================*/

#include "hal/vsf_hal_cfg.h"

#if VSF_HAL_USE_USART == ENABLED

#include "../../__device.h"

#include "utilities/ooc_class.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================ MACROS ========================================*/

#ifndef VSF_HW_USART_CFG_MULTI_CLASS
#   define VSF_HW_USART_CFG_MULTI_CLASS             VSF_USART_CFG_MULTI_CLASS
#endif

#define VSF_USART_CFG_REIMPLEMENT_TYPE_MODE         ENABLED
#define VSF_USART_CFG_REIMPLEMENT_TYPE_STATUS       ENABLED
#define VSF_USART_CFG_REIMPLEMENT_TYPE_IRQ_MASK     ENABLED
#define VSF_USART_CFG_REIMPLEMENT_TYPE_CTRL         ENABLED

/*============================ TYPES =========================================*/

/*
 * GD32F4XX USART mode encoding.
 *
 * Bit layout follows the GD32H7XX convention where possible so that
 * application code using the VSF_USART_* constants is source-compatible.
 * Register-level differences (no FIFO, no WL1, OVSMOD at CTL0[15], no
 * SWAP/RINV/TINV in CTL1) are handled in usart.c init/fini.
 *
 * Packed fields:
 *   0..1   STB(13:12) in CTL1, >> 12
 *   2..3   TEN(3)/REN(2) in CTL0, direct
 *   4..5   RTSEN(8)/CTSEN(9) in CTL2, >> 4
 *   6      HDEN(3) in CTL2, << 3
 *   9..10  PCEN(10)/PM(9) in CTL0, direct
 *   12     WL(12) in CTL0, direct  (8/9 bit only)
 *   18     OVSMOD(15) in CTL0, << 3
 *   20..23 CLEN(8)/CPH(9)/CPL(10)/CKEN(11) in CTL1, << 12
 */

typedef enum vsf_usart_mode_t {
    // 0..1: STB(13:12) in USART_CTL1, >> 12
    VSF_USART_0_5_STOPBIT               = (1 << 12) >> 12,
    VSF_USART_1_STOPBIT                 = (0 << 12) >> 12,
    VSF_USART_1_5_STOPBIT               = (3 << 12) >> 12,
    VSF_USART_2_STOPBIT                 = (2 << 12) >> 12,

    // 2..3: TEN(3)/REN(2) in USART_CTL0
    VSF_USART_TX_ENABLE                 = (1 << 3),
    VSF_USART_TX_DISABLE                = (0 << 3),
    VSF_USART_RX_ENABLE                 = (1 << 2),
    VSF_USART_RX_DISABLE                = (0 << 2),

    // 4..5: CTSEN(9)/RTSEN(8) in USART_CTL2, >> 4
    VSF_USART_NO_HWCONTROL              = (0),
    VSF_USART_RTS_HWCONTROL             = (1 << 8) >> 4,
    VSF_USART_CTS_HWCONTROL             = (1 << 9) >> 4,
    VSF_USART_RTS_CTS_HWCONTROL         = VSF_USART_RTS_HWCONTROL
                                        | VSF_USART_CTS_HWCONTROL,

    // 6: HDEN(3) in USART_CTL2, << 3
    VSF_USART_HALF_DUPLEX_ENABLE        = (1 << 3) << 3,
    VSF_USART_HALF_DUPLEX_DISABLE       = 0,

    // 9..10: PCEN(10)/PM(9) in USART_CTL0
    VSF_USART_NO_PARITY                 = (0 << 10),
    VSF_USART_ODD_PARITY                = (1 << 10) | (1 << 9),
    VSF_USART_EVEN_PARITY               = (1 << 10),

    // 23: CKEN(11) in USART_CTL1, << 12
    VSF_USART_SYNC_CLOCK_ENABLE         = (1 << 11) << 12,
    VSF_USART_SYNC_CLOCK_DISABLE        = (0 << 11) << 12,
    // 22: CPL(10) in USART_CTL1, << 12
    VSF_USART_SYNC_CLOCK_POLARITY_LOW   = (0 << 10) << 12,
    VSF_USART_SYNC_CLOCK_POLARITY_HIGH  = (1 << 10) << 12,
    // 21: CPH(9) in USART_CTL1, << 12
    VSF_USART_SYNC_CLOCK_PHASE_1_EDGE   = (0 << 9) << 12,
    VSF_USART_SYNC_CLOCK_PHASE_2_EDGE   = (1 << 9) << 12,
    // 20: CLEN(8) in USART_CTL1, << 12
    VSF_USART_SYNC_CLOCK_LAST_BIT_ENABLE        = (1 << 8) << 12,
    VSF_USART_SYNC_CLOCK_LAST_BIT_DISABLE       = (0 << 8) << 12,
#define VSF_USART_SYNC_CLOCK_LAST_BIT_ENABLE    VSF_USART_SYNC_CLOCK_LAST_BIT_ENABLE
#define VSF_USART_SYNC_CLOCK_LAST_BIT_DISABLE   VSF_USART_SYNC_CLOCK_LAST_BIT_DISABLE
#define VSF_USART_SYNC_CLOCK_LAST_BIT_MASK      VSF_USART_SYNC_CLOCK_LAST_BIT_ENABLE | VSF_USART_SYNC_CLOCK_LAST_BIT_DISABLE

    // 12: WL(12) in USART_CTL0 — F4XX has 8/9 bit only
    VSF_USART_9_BIT_LENGTH              = (1 << 12),
    VSF_USART_8_BIT_LENGTH              = (0),

    // 18: OVSMOD(15) in USART_CTL0, encoded at bit 18 (>> 3 to get actual pos)
    VSF_USART_OVERSAMPLE_8              = (1 << 18),
    VSF_USART_OVERSAMPLE_16             = (0 << 18),
    VSF_USART_OVERSAMPLE_MASK           = (1 << 18),


    __VSF_HW_USART_CTL0_MASK            = VSF_USART_9_BIT_LENGTH
                                        | VSF_USART_NO_PARITY
                                        | VSF_USART_ODD_PARITY
                                        | VSF_USART_EVEN_PARITY
                                        | VSF_USART_TX_ENABLE
                                        | VSF_USART_RX_ENABLE,
    __VSF_HW_USART_CTL1_MASK            = VSF_USART_SYNC_CLOCK_ENABLE
                                        | VSF_USART_SYNC_CLOCK_POLARITY_HIGH
                                        | VSF_USART_SYNC_CLOCK_PHASE_2_EDGE
                                        | VSF_USART_SYNC_CLOCK_LAST_BIT_ENABLE
                                        | VSF_USART_0_5_STOPBIT
                                        | VSF_USART_1_STOPBIT
                                        | VSF_USART_1_5_STOPBIT
                                        | VSF_USART_2_STOPBIT,
    __VSF_HW_USART_CTL2_MASK            = VSF_USART_RTS_CTS_HWCONTROL
                                        | VSF_USART_HALF_DUPLEX_ENABLE,

    // not supported on F4XX, allocate unused bits
    VSF_USART_10_BIT_LENGTH             = (1 << 12) | (1 << 28),
    VSF_USART_7_BIT_LENGTH              = (1 << 28),
    VSF_USART_6_BIT_LENGTH              = (1 << 7),
    VSF_USART_5_BIT_LENGTH              = (2 << 7),
    VSF_USART_FORCE_0_PARITY            = (0 << 11),
    VSF_USART_FORCE_1_PARITY            = (1 << 11),

    VSF_USART_TX_FIFO_THRESHOLD_NOT_FULL
                                        = (0 << 24),
    VSF_USART_TX_FIFO_THRESHOLD_HALF_EMPTY
                                        = (1 << 24),
    VSF_USART_TX_FIFO_THRESHOLD_EMPTY   = (2 << 24),

    VSF_USART_RX_FIFO_THRESHOLD_NOT_EMPTY
                                        = (0 << 26),
    VSF_USART_RX_FIFO_THRESHOLD_HALF_FULL
                                        = (1 << 26),
    VSF_USART_RX_FIFO_THRESHOLD_FULL    = (2 << 26),

    __VSF_HW_USART_NOT_SUPPORT_MASK     = VSF_USART_5_BIT_LENGTH
                                        | VSF_USART_6_BIT_LENGTH
                                        | VSF_USART_7_BIT_LENGTH
                                        | VSF_USART_FORCE_1_PARITY,
} vsf_usart_mode_t;

typedef enum vsf_usart_irq_mask_t {
    // 5: RBNEIE(5) in USART_CTL0
    VSF_USART_IRQ_MASK_RX               = (1 << 5),
    // 7: TBEIE(7) in USART_CTL0
    VSF_USART_IRQ_MASK_TX               = (1 << 7),
    // 8: PERRIE(8) in USART_CTL0
    VSF_USART_IRQ_MASK_PARITY_ERR       = (1 << 8),

    // 10: CTSIE(10) in USART_CTL2
    VSF_USART_IRQ_MASK_CTS              = (1 << 10),

    // F4XX receiver timeout is in CTL3 — not supported in initial skeleton
    VSF_USART_IRQ_MASK_RX_TIMEOUT       = (0),

    // DMA completion — not implemented
    VSF_USART_IRQ_MASK_RX_CPL           = (0),
    VSF_USART_IRQ_MASK_TX_CPL           = (0),

    __VSF_HW_USART_IRQ_MASK             = VSF_USART_IRQ_MASK_RX
                                        | VSF_USART_IRQ_MASK_TX
                                        | VSF_USART_IRQ_MASK_PARITY_ERR
                                        | VSF_USART_IRQ_MASK_CTS,
    __VSF_HW_USART_CTL0_IRQ_MASK        = VSF_USART_IRQ_MASK_RX
                                        | VSF_USART_IRQ_MASK_TX
                                        | VSF_USART_IRQ_MASK_PARITY_ERR,
    __VSF_HW_USART_CTL2_IRQ_MASK        = VSF_USART_IRQ_MASK_CTS,

    // not supported
    VSF_USART_IRQ_MASK_FRAME_ERR        = (1 << 0),
    VSF_USART_IRQ_MASK_BREAK_ERR        = (1 << 1),
    VSF_USART_IRQ_MASK_RX_OVERFLOW_ERR  = (1 << 2),

    __VSF_HW_USART_NOT_SUPPORT_IRQ_MASK = VSF_USART_IRQ_MASK_FRAME_ERR
                                        | VSF_USART_IRQ_MASK_BREAK_ERR
                                        | VSF_USART_IRQ_MASK_RX_OVERFLOW_ERR,
} vsf_usart_irq_mask_t;

typedef enum vsf_usart_ctrl_t {
    // SBKCMD(0) in USART_CTL0
    VSF_USART_CTRL_SEND_BREAK           = (1 << 0),

    VSF_USART_CTRL_SET_BREAK            = (1 << 8),
    VSF_USART_CTRL_CLEAR_BREAK          = (1 << 9),

    __VSF_HW_USART_SUPPORT_CMD_MASK     = VSF_USART_CTRL_SEND_BREAK,
} vsf_usart_ctrl_t;

/*
 * GD32F4XX status: STAT0(bits 0-9) | STAT1(bits 11,12,16) combined.
 * Bit positions match the GD32H7XX layout for source compatibility.
 */
typedef struct vsf_usart_status_t {
    union {
        struct {
            uint32_t parity_err         : 1;    // PERR(0) in STAT0
            uint32_t frame_err          : 1;    // FERR(1) in STAT0
            uint32_t noise_err          : 1;    // NERR(2) in STAT0
            uint32_t overrun_err        : 1;    // ORERR(3) in STAT0
            uint32_t idle               : 1;    // IDLEF(4) in STAT0
            uint32_t rx_not_empty       : 1;    // RBNE(5) in STAT0
            uint32_t trans_complete     : 1;    // TC(6) in STAT0
            uint32_t tx_empty           : 1;    // TBE(7) in STAT0
            uint32_t __dummy0           : 1;    // LBDF(8) in STAT0
            uint32_t cts_changed        : 1;    // CTSF(9) in STAT0
            uint32_t __dummy1           : 1;
            uint32_t rx_timeouted       : 1;    // RTF(11) in STAT1
            uint32_t __dummy2           : 4;
            uint32_t is_busy            : 1;    // BSY(16) in STAT1
            uint32_t __dummy3           : 15;
        };
        uint32_t value;
    };
} vsf_usart_status_t;

/*============================ GLOBAL VARIABLES ==============================*/
/*============================ PROTOTYPES ====================================*/

#ifdef __cplusplus
}
#endif

#endif      // VSF_HAL_USE_USART
#endif      // __HAL_DRIVER_GIGADEVICE_GD32F4XX_USART_H__
/* EOF */
