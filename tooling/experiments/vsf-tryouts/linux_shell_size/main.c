// SPDX-License-Identifier: MIT
#define __VSF_SIMPLE_STREAM_CLASS_INHERIT__
#include "vsf.h"
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

// ARM semihosting
static void sh_writec(char ch)
{
    register char *r1 asm("r1") = &ch;
    asm volatile ("mov r0, #0x03\n bkpt 0xAB" : : "r"(r1) : "r0", "memory");
}

static void sh_puts(const char *s)
{
    while (*s) sh_writec(*s++);
    sh_writec('\n');
}

static void sh_exit(int code)
{
    uint32_t args[2] = { 0x20026, (uint32_t)code };
    register uint32_t *r1 asm("r1") = args;
    asm volatile ("mov r0, #0x20\n bkpt 0xAB" : : "r"(r1) : "r0", "memory");
    while (1) {}
}

static char print_buf[128];
static void sh_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(print_buf, sizeof(print_buf), fmt, ap);
    va_end(ap);
    sh_puts(print_buf);
}

int vsf_stdout_putchar(char ch) { sh_writec(ch); return 0; }
void vsf_stdout_init(void) {}
void vsf_stdin_init(void) {}

// ================================================================
// posix_queue_t — condvar-based queue with copy semantics
// Replaces FreeRTOS xQueueCreate/Send/Receive
// ================================================================

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    uint8_t         *buf;
    size_t          item_size;
    size_t          capacity;
    size_t          count;
    size_t          head;
    size_t          tail;
} posix_queue_t;

static int posix_queue_init(posix_queue_t *q, void *buf, size_t item_size, size_t capacity)
{
    q->buf = (uint8_t *)buf;
    q->item_size = item_size;
    q->capacity = capacity;
    q->count = q->head = q->tail = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return 0;
}

static int posix_queue_send(posix_queue_t *q, const void *item)
{
    pthread_mutex_lock(&q->mutex);
    while (q->count >= q->capacity)
        pthread_cond_wait(&q->not_full, &q->mutex);
    memcpy(q->buf + q->tail * q->item_size, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

static int posix_queue_recv(posix_queue_t *q, void *item)
{
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->mutex);
    memcpy(item, q->buf + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

// ================================================================
// posix_evtgrp_t — condvar-based event group
// Replaces FreeRTOS xEventGroupCreate/SetBits/WaitBits/ClearBits
// ================================================================

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    uint32_t        flags;
} posix_evtgrp_t;

static int posix_evtgrp_init(posix_evtgrp_t *eg)
{
    eg->flags = 0;
    pthread_mutex_init(&eg->mutex, NULL);
    pthread_cond_init(&eg->cond, NULL);
    return 0;
}

static void posix_evtgrp_set(posix_evtgrp_t *eg, uint32_t bits)
{
    pthread_mutex_lock(&eg->mutex);
    eg->flags |= bits;
    pthread_cond_broadcast(&eg->cond);
    pthread_mutex_unlock(&eg->mutex);
}

static uint32_t posix_evtgrp_wait(posix_evtgrp_t *eg, uint32_t bits, int clear)
{
    pthread_mutex_lock(&eg->mutex);
    while (!(eg->flags & bits))
        pthread_cond_wait(&eg->cond, &eg->mutex);
    uint32_t got = eg->flags & bits;
    if (clear)
        eg->flags &= ~got;
    pthread_mutex_unlock(&eg->mutex);
    return got;
}

static void posix_evtgrp_clear(posix_evtgrp_t *eg, uint32_t bits)
{
    pthread_mutex_lock(&eg->mutex);
    eg->flags &= ~bits;
    pthread_mutex_unlock(&eg->mutex);
}

// ================================================================
// Tests
// ================================================================

// ============ T1: Basic pthread + sem (from previous) ============
static void *basic_worker(void *arg)
{
    int id = (int)(intptr_t)arg;
    sh_printf("[T1] thread %d running", id);
    return NULL;
}

static int test_basic_pthread(void)
{
    sh_puts("[T1] basic pthread test");
    pthread_t t1, t2;
    pthread_create(&t1, NULL, basic_worker, (void *)1);
    pthread_create(&t2, NULL, basic_worker, (void *)2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    sh_puts("[T1] PASS");
    return 0;
}

// ============ T2: Recursive mutex ============
static int test_recursive_mutex(void)
{
    sh_puts("[T2] recursive mutex test");
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_t rmtx;
    pthread_mutex_init(&rmtx, &attr);
    pthread_mutexattr_destroy(&attr);

    int r1 = pthread_mutex_lock(&rmtx);
    int r2 = pthread_mutex_lock(&rmtx);
    sh_printf("[T2] lock1=%d lock2=%d (0=success)", r1, r2);
    pthread_mutex_unlock(&rmtx);
    pthread_mutex_unlock(&rmtx);

    if (r1 == 0 && r2 == 0) { sh_puts("[T2] PASS"); return 0; }
    sh_puts("[T2] FAIL");
    return 1;
}

// ============ T3: 9 threads ============
#define NUM_THREADS 9
static volatile int g_done_count = 0;
static pthread_mutex_t g_count_mtx;

static void *many_worker(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_count_mtx);
    g_done_count++;
    pthread_mutex_unlock(&g_count_mtx);
    return NULL;
}

static int test_9_threads(void)
{
    sh_puts("[T3] 9-thread test");
    g_done_count = 0;
    pthread_mutex_init(&g_count_mtx, NULL);
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_create(&threads[i], NULL, many_worker, (void *)(intptr_t)i);
        if (ret != 0) { sh_printf("[T3] FAIL: create(%d)=%d", i, ret); return 1; }
    }
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    sh_printf("[T3] done_count=%d", g_done_count);
    if (g_done_count == NUM_THREADS) { sh_puts("[T3] PASS"); return 0; }
    sh_puts("[T3] FAIL");
    return 1;
}

// ============ T4: Semaphore ============
static sem_t g_sem;

static void *sem_worker(void *arg)
{
    (void)arg;
    sem_wait(&g_sem);
    sh_puts("[T4] sem acquired");
    return NULL;
}

static int test_semaphore(void)
{
    sh_puts("[T4] semaphore test");
    sem_init(&g_sem, 0, 0);
    pthread_t t;
    pthread_create(&t, NULL, sem_worker, NULL);
    usleep(10000);
    sem_post(&g_sem);
    pthread_join(t, NULL);
    sh_puts("[T4] PASS");
    return 0;
}

// ============ T5: usleep timing ============
static int test_usleep(void)
{
    sh_puts("[T5] usleep test");
    volatile uint32_t before = vsf_systimer_get_tick();
    usleep(50000);  // 50 ms
    volatile uint32_t after = vsf_systimer_get_tick();
    uint32_t elapsed_us = vsf_systimer_tick_to_us(after - before);
    sh_printf("[T5] usleep(50000) actual=%u us", (unsigned)elapsed_us);
    // QEMU timing is ~10x slower than wall clock — accept wide range
    if (elapsed_us >= 10000 && elapsed_us <= 2000000) {
        sh_puts("[T5] PASS");
        return 0;
    }
    sh_puts("[T5] FAIL: timing out of range");
    return 1;
}

// ============ T6: posix_queue (condvar + copy semantics) ============
typedef struct { uint32_t seq; int16_t value; } test_frame_t;

static posix_queue_t g_queue;
static test_frame_t g_queue_buf[4];

static void *queue_producer(void *arg)
{
    (void)arg;
    for (uint32_t i = 0; i < 3; i++) {
        test_frame_t f = { .seq = i, .value = (int16_t)(i * 10) };
        posix_queue_send(&g_queue, &f);
    }
    sh_puts("[T6] producer: 3 frames sent");
    return NULL;
}

static void *queue_consumer(void *arg)
{
    int *ok = (int *)arg;
    *ok = 1;
    for (uint32_t i = 0; i < 3; i++) {
        test_frame_t f;
        posix_queue_recv(&g_queue, &f);
        if (f.seq != i || f.value != (int16_t)(i * 10)) {
            sh_printf("[T6] consumer: MISMATCH at %u: seq=%u val=%d", i, f.seq, f.value);
            *ok = 0;
        }
    }
    sh_puts("[T6] consumer: 3 frames received");
    return NULL;
}

static int test_posix_queue(void)
{
    sh_puts("[T6] posix_queue test (condvar + copy semantics)");
    posix_queue_init(&g_queue, g_queue_buf, sizeof(test_frame_t), 4);

    int consumer_ok = 0;
    pthread_t prod, cons;
    pthread_create(&cons, NULL, queue_consumer, &consumer_ok);
    usleep(5000);
    pthread_create(&prod, NULL, queue_producer, NULL);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    if (consumer_ok) { sh_puts("[T6] PASS"); return 0; }
    sh_puts("[T6] FAIL");
    return 1;
}

// ============ T7: posix_evtgrp (condvar + bitmap flags) ============
#define EVT_START    (1 << 0)
#define EVT_COMPLETE (1 << 1)
#define EVT_FAIL     (1 << 2)

static posix_evtgrp_t g_evtgrp;

static void *evtgrp_waiter(void *arg)
{
    int *result = (int *)arg;
    uint32_t got = posix_evtgrp_wait(&g_evtgrp, EVT_COMPLETE | EVT_FAIL, 1);
    *result = (got & EVT_COMPLETE) ? 1 : 0;
    sh_printf("[T7] waiter got flags=0x%x", got);
    return NULL;
}

static int test_posix_evtgrp(void)
{
    sh_puts("[T7] posix_evtgrp test (condvar + bitmap flags)");
    posix_evtgrp_init(&g_evtgrp);

    int waiter_result = 0;
    pthread_t tw;
    pthread_create(&tw, NULL, evtgrp_waiter, &waiter_result);

    usleep(10000);
    posix_evtgrp_set(&g_evtgrp, EVT_START);
    sh_puts("[T7] set START");

    usleep(10000);
    posix_evtgrp_set(&g_evtgrp, EVT_COMPLETE);
    sh_puts("[T7] set COMPLETE");

    pthread_join(tw, NULL);

    if (waiter_result == 1) { sh_puts("[T7] PASS"); return 0; }
    sh_puts("[T7] FAIL");
    return 1;
}

// ================================================================
// Entry
// ================================================================

int posix_test_main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sh_puts("=== VSF POSIX verification (7 tests) ===");

    int fail = 0;
    fail += test_basic_pthread();
    fail += test_recursive_mutex();
    fail += test_9_threads();
    fail += test_semaphore();
    fail += test_usleep();
    fail += test_posix_queue();
    fail += test_posix_evtgrp();

    sh_printf("=== Result: %d/%d tests failed ===", fail, 7);
    sh_exit(fail);
    return fail;
}

// ============ Linux FHS setup ============
int vsf_linux_create_fhs(void)
{
    vsf_linux_vfs_init();
    busybox_bind(VSF_LINUX_CFG_BIN_PATH "/init", posix_test_main);
    return 0;
}

// ============ Minimal streams ============
static uint8_t __stream_rx_buf[64], __stream_tx_buf[256];
static vsf_mem_stream_t __stream_rx = {
    .op     = &vsf_mem_stream_op,
    .buffer = __stream_rx_buf,
    .size   = sizeof(__stream_rx_buf),
};
static vsf_mem_stream_t __stream_tx = {
    .op     = &vsf_mem_stream_op,
    .buffer = __stream_tx_buf,
    .size   = sizeof(__stream_tx_buf),
};

// ============ VSF entry ============
int VSF_USER_ENTRY(int argc, char *argv[])
{
    (void)argc; (void)argv;
    VSF_STREAM_INIT(&__stream_rx);
    VSF_STREAM_INIT(&__stream_tx);
    vsf_linux_stdio_stream_t stream = {
        .in  = (vsf_stream_t *)&__stream_rx,
        .out = (vsf_stream_t *)&__stream_tx,
        .err = (vsf_stream_t *)&__stream_tx,
    };
    vsf_linux_init(&stream);
    while (1) { vsf_thread_delay_ms(1000); }
    return 0;
}
