// SPDX-License-Identifier: MIT
#include "vsf.h"
#include <stdio.h>

#define LOG_MODULE "kerneltest"
#include "log.h"

// ------------ stdio retarget via ARM semihosting ------------
// Why semihosting instead of CMSDK_UART0 + vsf_usart_stream? The UART
// path needs a real hw init + stream op table + debug-stream wiring that
// VSF's vsf_start_trace(...) macro expects. For a tryout we take the
// shortest route: put each byte on host stdio via bkpt 0xAB.

static void sh_writec(char ch)
{
    register char *r1 asm("r1") = &ch;
    asm volatile ("mov r0, #0x03\n bkpt 0xAB" : : "r"(r1) : "r0", "memory");
}

static void sh_exit(int code)
{
    uint32_t args[2] = { 0x20026, (uint32_t)code };
    register uint32_t *r1 asm("r1") = args;
    asm volatile ("mov r0, #0x20\n bkpt 0xAB" : : "r"(r1) : "r0", "memory");
    while (1) {}
}

// Override VSF's weak stdout hooks (vsf/source/utilities/compiler/__common/__retarget_io.c).
int vsf_stdout_putchar(char ch) { sh_writec(ch); return 0; }
void vsf_stdout_init(void) {}
void vsf_stdin_init(void) {}

// ------------ log wheel sink: one byte at a time to semihosting ------------
// Pushes each byte of a formatted log line through the same bkpt 0xAB path
// used by sh_writec. Demonstrates log wheel's sink API on a non-host target.
static void log_sink_semihosting(void *ctx, const char *line, size_t len)
{
    (void)ctx;
    for (size_t i = 0; i < len; i++) {
        sh_writec(line[i]);
    }
}

// ------------ log wheel time source: VSF systimer in ms ------------
// The wheel prints no timestamp until it is given a time function.
static uint32_t log_time_ms(void)
{
    return (uint32_t)vsf_systimer_get_ms();
}

// ------------ VSF kernel test: teda + sem round-trip ------------
// Exercises: teda scheduling, vsf_systimer, sem init/post/pend, VSF_EVT
// dispatch (INIT → TIMER → SYNC). Sender posts on a 1 ms teda timer,
// Recver pends with a 100 ms timeout. Exit after 3 sem ping-pongs.

#define TEST_ROUNDS     3

static struct {
    vsf_sem_t    sem;
    vsf_teda_t   sender;
    vsf_teda_t   recver;
    int          posted;
    int          got;
} __t;

static void __sender_handler(vsf_eda_t *eda, vsf_evt_t evt)
{
    (void)eda;
    switch (evt) {
    case VSF_EVT_INIT:
    case VSF_EVT_TIMER:
        __t.posted++;
        LOG_INFO("sender post #%d", __t.posted);
        vsf_eda_sem_post(&__t.sem);
        if (__t.posted < TEST_ROUNDS) {
            vsf_teda_set_timer(vsf_systimer_us_to_tick(1000));
        }
        break;
    }
}

static void __recver_handler(vsf_eda_t *eda, vsf_evt_t evt)
{
    (void)eda;
    switch (evt) {
    case VSF_EVT_INIT:
pend_next:
        if (VSF_ERR_NONE == vsf_eda_sem_pend(&__t.sem,
                                             vsf_systimer_us_to_tick(100 * 1000))) {
got_sem:
            __t.got++;
            LOG_INFO("recver got #%d", __t.got);
            if (__t.got >= TEST_ROUNDS) {
                LOG_INFO("pass: %d sem ping-pongs completed", __t.got);
                sh_exit(0);
            }
            goto pend_next;
        }
        break;
    case VSF_EVT_TIMER:
    case VSF_EVT_SYNC:
    case VSF_EVT_SYNC_CANCEL:
        if (vsf_eda_sync_get_reason(&__t.sem, evt) == VSF_SYNC_GET) {
            goto got_sem;
        }
        break;
    }
}

int VSF_USER_ENTRY(void)
{
    log_add_sink(log_sink_semihosting, NULL);
    log_set_time_fn(log_time_ms);
    LOG_INFO("VSF kernel teda+sem test start");
    vsf_eda_sem_init(&__t.sem, 0);

    vsf_eda_cfg_t cfg = {
        .fn.evthandler = __sender_handler,
        .priority      = vsf_prio_0,
    };
    vsf_teda_start(&__t.sender, &cfg);

    cfg.fn.evthandler = __recver_handler;
    vsf_teda_start(&__t.recver, &cfg);
    return 0;
}
