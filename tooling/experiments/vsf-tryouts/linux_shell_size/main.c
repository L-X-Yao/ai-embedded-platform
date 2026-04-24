#include "vsf.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

// ARM semihosting output
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

int vsf_stdout_putchar(char ch) { sh_writec(ch); return 0; }
void vsf_stdout_init(void) {}
void vsf_stdin_init(void) {}

// Test: pthread + mutex + semaphore
static pthread_mutex_t  g_mutex;
static sem_t            g_sem;
static int              g_counter = 0;

static void *worker_thread(void *arg)
{
    int id = (int)(intptr_t)arg;

    sem_wait(&g_sem);

    pthread_mutex_lock(&g_mutex);
    g_counter++;
    pthread_mutex_unlock(&g_mutex);

    sh_puts(id == 1 ? "[thread1] done" : "[thread2] done");
    return NULL;
}

int VSF_USER_ENTRY(void)
{
    sh_puts("[main] VSF linux shell size test start");

    pthread_mutex_init(&g_mutex, NULL);
    sem_init(&g_sem, 0, 0);

    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker_thread, (void *)1);
    pthread_create(&t2, NULL, worker_thread, (void *)2);

    // unblock both workers
    sem_post(&g_sem);
    sem_post(&g_sem);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    sh_puts(g_counter == 2 ? "[main] PASS" : "[main] FAIL");
    sh_exit(g_counter == 2 ? 0 : 1);
    return 0;
}
