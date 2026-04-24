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

#ifndef __HAL_DRIVER_GIGADEVICE_GD32F4XX_SPI_H__
#define __HAL_DRIVER_GIGADEVICE_GD32F4XX_SPI_H__

/*============================ INCLUDES ======================================*/

#include "hal/vsf_hal_cfg.h"

#if VSF_HAL_USE_SPI == ENABLED

#include "../../__device.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================ MACROS ========================================*/

#ifndef VSF_HW_SPI_CFG_MULTI_CLASS
#   define VSF_HW_SPI_CFG_MULTI_CLASS               VSF_SPI_CFG_MULTI_CLASS
#endif

#define VSF_SPI_CFG_REIMPLEMENT_TYPE_MODE           ENABLED
#define VSF_SPI_CFG_REIMPLEMENT_TYPE_IRQ_MASK       ENABLED

/*============================ TYPES =========================================*/

/*
 * GD32F4XX SPI mode encoding.
 *
 * SPI_CTL0 bits:
 *   CKPH(0)    clock phase
 *   CKPL(1)    clock polarity
 *   MSTMOD(2)  master mode
 *   PSC(3:5)   prescaler
 *   SPIEN(6)   SPI enable
 *   LSBF(7)    LSB first
 *   SWNSS(8)   software NSS value
 *   SWNSSEN(9) software NSS enable
 *   RXONLY(10)  receive-only mode
 *   FF16(11)   16-bit frame
 *   BDEN(15)   bidirectional enable
 *   BDOEN(14)  bidirectional output enable
 */
typedef enum vsf_spi_mode_t {
    // master / slave
    VSF_SPI_MASTER                      = (1 << 2),
    VSF_SPI_SLAVE                       = (0 << 2),

    // clock polarity + phase — SPI modes 0-3
    VSF_SPI_MODE_0                      = (0 << 1) | (0 << 0),
    VSF_SPI_MODE_1                      = (0 << 1) | (1 << 0),
    VSF_SPI_MODE_2                      = (1 << 1) | (0 << 0),
    VSF_SPI_MODE_3                      = (1 << 1) | (1 << 0),

    VSF_SPI_CPOL_LOW                    = (0 << 1),
    VSF_SPI_CPOL_HIGH                   = (1 << 1),
    VSF_SPI_CPHA_1EDGE                  = (0 << 0),
    VSF_SPI_CPHA_2EDGE                  = (1 << 0),

    // data size
    VSF_SPI_DATASIZE_8                  = (0 << 11),
    VSF_SPI_DATASIZE_16                 = (1 << 11),
    // not supported
    VSF_SPI_DATASIZE_32                 = (1 << 16),

    // bit order
    VSF_SPI_MSB_FIRST                   = (0 << 7),
    VSF_SPI_LSB_FIRST                   = (1 << 7),

    // NSS
    VSF_SPI_CS_SOFTWARE_MODE            = (1 << 9),
    VSF_SPI_CS_HARDWARE_MODE            = (0 << 9),

    __VSF_HW_SPI_CTL0_MASK              = VSF_SPI_MASTER
                                        | VSF_SPI_MODE_3
                                        | VSF_SPI_DATASIZE_16
                                        | VSF_SPI_LSB_FIRST
                                        | VSF_SPI_CS_SOFTWARE_MODE,
    __VSF_HW_SPI_NOT_SUPPORT_MASK       = VSF_SPI_DATASIZE_32,
} vsf_spi_mode_t;

typedef enum vsf_spi_irq_mask_t {
    // 6: RBNEIE(6) in SPI_CTL1
    VSF_SPI_IRQ_MASK_RX                 = (1 << 6),
    // 7: TBEIE(7) in SPI_CTL1
    VSF_SPI_IRQ_MASK_TX                 = (1 << 7),

    VSF_SPI_IRQ_MASK_TX_CPL             = (1 << 16),
    VSF_SPI_IRQ_MASK_RX_CPL             = (1 << 17),
    VSF_SPI_IRQ_MASK_RX_OVERFLOW_ERR    = (1 << 5),

    __VSF_HW_SPI_IRQ_MASK               = VSF_SPI_IRQ_MASK_RX
                                        | VSF_SPI_IRQ_MASK_TX
                                        | VSF_SPI_IRQ_MASK_RX_OVERFLOW_ERR,
} vsf_spi_irq_mask_t;

/*============================ PROTOTYPES ====================================*/

#ifdef __cplusplus
}
#endif

#endif      // VSF_HAL_USE_SPI
#endif      // __HAL_DRIVER_GIGADEVICE_GD32F4XX_SPI_H__
/* EOF */
