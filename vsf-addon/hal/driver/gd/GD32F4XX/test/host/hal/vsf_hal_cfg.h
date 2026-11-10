// SPDX-License-Identifier: MIT
/* Host stand-in: run.sh picks one peripheral with -DHOST_USE_<NAME>. */
#ifndef HOST_VSF_HAL_CFG_H
#define HOST_VSF_HAL_CFG_H

#define ENABLED     1
#define DISABLED    0

#ifdef HOST_USE_FLASH
#   define VSF_HAL_USE_FLASH    ENABLED
#else
#   define VSF_HAL_USE_FLASH    DISABLED
#endif
#ifdef HOST_USE_I2C
#   define VSF_HAL_USE_I2C      ENABLED
#else
#   define VSF_HAL_USE_I2C      DISABLED
#endif
#ifdef HOST_USE_SPI
#   define VSF_HAL_USE_SPI      ENABLED
#else
#   define VSF_HAL_USE_SPI      DISABLED
#endif
#ifdef HOST_USE_USART
#   define VSF_HAL_USE_USART    ENABLED
#else
#   define VSF_HAL_USE_USART    DISABLED
#endif
#ifdef HOST_USE_GPIO
#   define VSF_HAL_USE_GPIO     ENABLED
#else
#   define VSF_HAL_USE_GPIO     DISABLED
#endif

#endif
