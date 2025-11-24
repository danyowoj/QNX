/*
 * Компиляция:
 *   qcc -Wc,-std=c99 -o ctx_switch ctx_switch.c
 *
 * Запуск (обязательно привязать к одному CPU):
 *   on -C 0 ./ctx_switch 1000000
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <unistd.h>
#include <inttypes.h>

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_parent = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_worker = PTHREAD_COND_INITIALIZER;

static volatile int turn = 0;       // 0 = parent, 1 = worker
static unsigned long long *samples; // worker timestamps
static int iterations;

/* Получение значения тактового счетчика */
static inline uint64_t get_cycles()
{
    return ClockCycles();
}

/* Калибровка частоты процессора через ClockCycles() */
uint64_t calibrate_cpu_freq()
{
    uint64_t t0 = get_cycles();
    delay(100); // 100 мс
    uint64_t t1 = get_cycles();

    double hz = (double)(t1 - t0) * 10.0; // 100 ms → *10 = 1 сек

    printf("CPU frequency calibrated via ClockCycles(): %.0f Hz\n", hz);
    return (uint64_t)hz;
}

/* Получение частоты CPU с проверкой */
uint64_t get_cpu_frequency()
{
    uint64_t hz = SYSPAGE_ENTRY(cpuinfo)->speed;

    printf("cpuinfo->speed reported: %" PRIu64 " Hz\n", hz);

    // Проверка на адекватность (100 МГц – 10 ГГц)
    if (hz < 100000000ULL || hz > 10000000000ULL)
    {
        printf("cpuinfo->speed looks invalid → calibrating...\n");
        hz = calibrate_cpu_freq();
    }

    return hz;
}

void *worker_thread(void *arg)
{
    for (int i = 0; i < iterations; i++)
    {
        pthread_mutex_lock(&mtx);

        while (turn != 1)
            pthread_cond_wait(&cond_worker, &mtx);

        uint64_t t1 = get_cycles();
        samples[i] = t1; // время пробуждения worker

        turn = 0;
        pthread_cond_signal(&cond_parent);

        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <iterations>\n", argv[0]);
        return 1;
    }

    iterations = atoi(argv[1]);
    samples = calloc(iterations, sizeof(uint64_t));
    uint64_t *parent_times = calloc(iterations, sizeof(uint64_t));

    // Получение корректной частоты CPU
    uint64_t cpu_hz = get_cpu_frequency();
    double cycle_ns = 1e9 / (double)cpu_hz;

    pthread_t worker;
    pthread_create(&worker, NULL, worker_thread, NULL);

    for (int i = 0; i < iterations; i++)
    {
        pthread_mutex_lock(&mtx);

        uint64_t t0 = get_cycles();
        parent_times[i] = t0;

        turn = 1;
        pthread_cond_signal(&cond_worker);

        while (turn != 0)
            pthread_cond_wait(&cond_parent, &mtx);

        pthread_mutex_unlock(&mtx);
    }

    pthread_join(worker, NULL);

    uint64_t sum_cycles = 0;

    for (int i = 0; i < iterations; i++)
    {
        uint64_t delta = samples[i] - parent_times[i];
        sum_cycles += delta;
    }

    double avg_cycles = (double)sum_cycles / iterations;
    double avg_ns = avg_cycles * cycle_ns;

    printf("\n=== RESULTS ===\n");
    printf("Iterations: %d\n", iterations);
    printf("Average switch time (cycles): %.2f\n", avg_cycles);
    printf("Average switch time (ns): %.2f\n", avg_ns);
    printf("Estimated single context switch (ns): %.2f\n\n", avg_ns / 2.0);

    free(samples);
    free(parent_times);

    return 0;
}
