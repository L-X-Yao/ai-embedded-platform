#ifndef __VSF_USR_CFG_H__
#define __VSF_USR_CFG_H__

// Minimal kernel-only tryout. Target: VSF_TARGET=an500 (Cortex-M7 / CMSDK_CM7).
// Goal: prove the bare-metal kernel boots + user_main gets scheduled + we can
// observe output via QEMU mps2-an500 serial.

// Let VSF own main() — our main.c provides VSF_USER_ENTRY.
#define VSF_KERNEL_CFG_ENTRY_IS_MAIN                    ENABLED

// mps2-an500 in QEMU: no real clock, pick a plausible value. CMSDK_CM7
// default is 25MHz; systimer feeds SysTick.
#define VSF_SYSTIMER_FREQ                               25000000

// Trace service — needs a stream backend.
#define VSF_USE_TRACE                                   ENABLED
#define VSF_USE_SIMPLE_STREAM                           ENABLED

// No Linux userspace layer on bare-metal. Explicitly OFF.
#define VSF_USE_LINUX                                   DISABLED

// Keep all demo apps OFF — our main.c owns VSF_USER_ENTRY.
#define APP_USE_LINUX_DEMO                              DISABLED
#define APP_USE_USBH_DEMO                               DISABLED
#define APP_USE_USBD_DEMO                               DISABLED
#define APP_USE_SCSI_DEMO                               DISABLED
#define APP_USE_AUDIO_DEMO                              DISABLED
#define APP_USE_TGUI_DEMO                               DISABLED
#define APP_USE_SDL2_DEMO                               DISABLED
#define APP_USE_KERNEL_TEST                             DISABLED
#define APP_USE_LVGL_DEMO                               DISABLED
#define APP_USE_FREETYPE_DEMO                           DISABLED
#define APP_USE_HAL_DEMO                                DISABLED
#define APP_USE_JSON_DEMO                               DISABLED
#define APP_USE_CPP_DEMO                                DISABLED

#endif
