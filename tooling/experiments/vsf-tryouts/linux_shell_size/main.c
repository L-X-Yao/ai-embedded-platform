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

// ============ Test 1: Basic pthread + sem ============
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

// ============ Test 2: Recursive mutex ============
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

    if (r1 == 0 && r2 == 0) {
        sh_puts("[T2] PASS: no deadlock");
        return 0;
    }
    sh_puts("[T2] FAIL");
    return 1;
}

// ============ Test 3: 9 threads ============
#define NUM_THREADS     9
static volatile int g_done_count = 0;
static pthread_mutex_t g_count_mtx;

static void *many_worker(void *arg)
{
    int id = (int)(intptr_t)arg;
    (void)id;
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
        if (ret != 0) {
            sh_printf("[T3] FAIL: create(%d) returned %d", i, ret);
            return 1;
        }
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    sh_printf("[T3] done_count=%d", g_done_count);
    if (g_done_count == NUM_THREADS) {
        sh_puts("[T3] PASS");
        return 0;
    }
    sh_puts("[T3] FAIL");
    return 1;
}

// ============ Test 4: Semaphore ============
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

// ============ Linux test entry (runs as PID 1 process) ============
int posix_test_main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sh_puts("=== VSF linux shell POSIX verification ===");

    int fail = 0;
    fail += test_basic_pthread();
    fail += test_recursive_mutex();
    fail += test_9_threads();
    fail += test_semaphore();

    sh_printf("=== Result: %d tests failed ===", fail);
    sh_exit(fail);
    return fail;
}

// ============ Linux FHS setup ============
int vsf_linux_create_fhs(void)
{
    sh_puts("[fhs] create_fhs called");
    vsf_linux_vfs_init();
    busybox_bind(VSF_LINUX_CFG_BIN_PATH "/init", posix_test_main);
    sh_puts("[fhs] init bound");
    return 0;
}

// ============ Minimal streams for linux shell stdio ============
static uint8_t __stream_rx_buf[64], __stream_tx_buf[256];
static vsf_mem_stream_t __stream_rx = {
    .op         = &vsf_mem_stream_op,
    .buffer     = __stream_rx_buf,
    .size       = sizeof(__stream_rx_buf),
};
static vsf_mem_stream_t __stream_tx = {
    .op         = &vsf_mem_stream_op,
    .buffer     = __stream_tx_buf,
    .size       = sizeof(__stream_tx_buf),
};

// ============ VSF entry — initialize linux shell ============
int VSF_USER_ENTRY(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sh_puts("[vsf] user_entry reached");

    VSF_STREAM_INIT(&__stream_rx);
    VSF_STREAM_INIT(&__stream_tx);
    sh_puts("[vsf] streams initialized");

    vsf_linux_stdio_stream_t stream = {
        .in     = (vsf_stream_t *)&__stream_rx,
        .out    = (vsf_stream_t *)&__stream_tx,
        .err    = (vsf_stream_t *)&__stream_tx,
    };
    sh_puts("[vsf] calling vsf_linux_init...");
    vsf_err_t err = vsf_linux_init(&stream);
    sh_printf("[vsf] vsf_linux_init returned %d", err);

    sh_puts("[vsf] entering idle loop (yielding to scheduler)...");
    while (1) {
        vsf_thread_delay_ms(1000);
    }
    return 0;
}
