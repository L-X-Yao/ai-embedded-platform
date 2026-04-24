#ifndef __VSF_USR_CFG_H__
#define __VSF_USR_CFG_H__

// Size measurement: VSF kernel + linux shell (pthread + sem) on Cortex-M.
// Target: mps2-an500 (QEMU). Goal: measure .text/.data/.bss for the
// POSIX subset needed by flybox application layer migration.

#define VSF_KERNEL_CFG_ENTRY_IS_MAIN                    ENABLED
#define VSF_SYSTIMER_FREQ                               25000000

// Kernel: EDA + thread + sync (mandatory for linux shell)
#define VSF_KERNEL_CFG_EDA_SUPPORT_ON_TERMINATE         ENABLED
#define VSF_KERNEL_CFG_EDA_SUPPORT_SUB_CALL             ENABLED
#define VSF_KERNEL_CFG_EDA_SUPPORT_TIMER                ENABLED
#define VSF_KERNEL_CFG_SUPPORT_THREAD                   ENABLED
#define VSF_KERNEL_CFG_SUPPORT_SYNC                     ENABLED
#define VSF_SYNC_CFG_SUPPORT_ISR                        ENABLED

// OS config
#define VSF_OS_CFG_PRIORITY_NUM                         1
#define VSF_OS_CFG_ADD_EVTQ_TO_IDLE                     ENABLED
#define VSF_OS_CFG_MAIN_MODE                            VSF_OS_CFG_MAIN_MODE_THREAD
#define VSF_OS_CFG_MAIN_STACK_SIZE                      4096

// Heap + stream + FS (mandatory for linux shell)
#define VSF_USE_HEAP                                    ENABLED
#define VSF_HEAP_SIZE                                   (16 * 1024)
#define VSF_USE_SIMPLE_STREAM                           ENABLED
#define VSF_USE_FS                                      ENABLED
#define VSF_USE_MAL                                     ENABLED
#define VSF_USE_INPUT                                   DISABLED

// Trace
#define VSF_USE_TRACE                                   ENABLED

// Linux shell — the component under test
#define VSF_USE_LINUX                                   ENABLED
#define VSF_LINUX_USE_SIMPLE_LIBC                       ENABLED
#define VSF_LINUX_CFG_RELATIVE_PATH                     ENABLED
#define VSF_LINUX_CFG_STACKSIZE                         2048
#define VSF_LINUX_CFG_SUPPORT_SEM                       ENABLED
#define VSF_LINUX_CFG_SUPPORT_SIG                       ENABLED

// Disable heavy subsystems
#define VSF_LINUX_USE_BUSYBOX                           DISABLED
#define VSF_LINUX_USE_SOCKET                            DISABLED
#define VSF_LINUX_USE_DRM                               DISABLED
#define VSF_LINUX_USE_TERMINFO                          DISABLED

// All demo apps OFF
#define APP_USE_KERNEL_TEST                             DISABLED
#define APP_USE_LINUX_DEMO                              DISABLED
#define APP_USE_USBH_DEMO                               DISABLED
#define APP_USE_USBD_DEMO                               DISABLED
#define APP_USE_SCSI_DEMO                               DISABLED
#define APP_USE_AUDIO_DEMO                              DISABLED
#define APP_USE_TGUI_DEMO                               DISABLED
#define APP_USE_SDL2_DEMO                               DISABLED
#define APP_USE_LVGL_DEMO                               DISABLED
#define APP_USE_FREETYPE_DEMO                           DISABLED
#define APP_USE_HAL_DEMO                                DISABLED
#define APP_USE_JSON_DEMO                               DISABLED
#define APP_USE_CPP_DEMO                                DISABLED

#endif
