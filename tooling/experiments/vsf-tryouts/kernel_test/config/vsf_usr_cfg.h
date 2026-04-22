#ifndef __VSF_USR_CFG_H__
#define __VSF_USR_CFG_H__

// CRITICAL: cmake `set(VSF_USE_LINUX 1)` only gates which sources cmake
// compiles. C code guarded by `#if VSF_USE_LINUX == ENABLED` still needs
// this explicit define. Without it, vsf_linux_glibc_ctype.c etc. get
// #if'd out and link fails with __vsf_linux_ctype undefined.
#define VSF_USE_LINUX                                   ENABLED
#define VSF_USE_SIMPLE_STREAM                           ENABLED
// vsf_linux's VFS hard-requires VSF_USE_FS (#error otherwise).
// Minimal enable — memfs is the lightest backend.
#define VSF_USE_FS                                      ENABLED
#   define VSF_FS_USE_MEMFS                             ENABLED
// VSF linux mount.h #error's without MAL. Dependency chain:
// VSF_USE_LINUX → VSF_USE_FS → VSF_USE_MAL.
#define VSF_USE_MAL                                     ENABLED
// vsf_linux_fs.c extern-declares __vsf_trace_output — trace service must be built.
#define VSF_USE_TRACE                                   ENABLED

// linux porting does not support preemption, so need evtq in idle
#define VSF_OS_CFG_ADD_EVTQ_TO_IDLE                     ENABLED
#define VSF_KERNEL_CFG_CPU_USAGE                        DISABLED

// Let VSF own main() and invoke our user_main() from the kernel.
// NOTE: enabling this isn't enough to actually get user_main scheduled on
// linux64 — see playbook entry "VSF linux64 kernel-only mode not supported".
#define VSF_KERNEL_CFG_ENTRY_IS_MAIN                    ENABLED

// simple libc: required to avoid pulling in host glibc headers unexpectedly
#define VSF_LINUX_USE_SIMPLE_LIBC                       ENABLED
#if VSF_LINUX_USE_SIMPLE_LIBC == ENABLED
// NOTE: keep WRAPPER DISABLED on linux64 — headers rewrite opendir→__wrap_opendir
// but VSF_TARGET_WRAPPER_LIST is unset in linux64.cmake, so -Wl,--wrap=* is
// never added by compilers.cmake. Enabling this macro yields undefined
// references to __wrap_opendir/__wrap_stat/etc. at link time.
#   define VSF_LINUX_CFG_WRAPPER                        DISABLED
#   define VSF_USE_SIMPLE_SSCANF                        ENABLED
#   define VSF_USE_SIMPLE_SPRINTF                       ENABLED
#endif

// All app demos OFF — our main.c provides VSF_USER_ENTRY directly.
#define APP_USE_LINUX_DEMO                              DISABLED
#define APP_USE_USBH_DEMO                               DISABLED
#define APP_USE_USBD_DEMO                               DISABLED
#define APP_USE_SCSI_DEMO                               DISABLED
#define APP_USE_AUDIO_DEMO                              DISABLED
#define APP_USE_TGUI_DEMO                               DISABLED
#define APP_USE_SDL2_DEMO                               DISABLED
#define APP_USE_KERNEL_TEST                             DISABLED
#define APP_USE_JSON_DEMO                               DISABLED
#define APP_USE_CPP_DEMO                                DISABLED

#endif
