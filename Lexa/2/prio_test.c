/*
 * Компиляция:
 *   qcc -o prio_test prio_test.c
 *
 * Запуск:
 *   on -C 0 ./prio_test
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/neutrino.h> // ClockCycles()
#include <time.h>

volatile uint64_t worker_last_run = 0;
volatile int stop_worker = 0;

void msleep(unsigned msec)
{
    struct timespec ts;
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void *worker_func(void *arg)
{
    (void)arg;
    while (!stop_worker)
    {
        worker_last_run = ClockCycles();
        // разумная короткая пауза, чтобы не блокировать систему
        // даёт шанс системе и другим потокам выполниться
        msleep(1); // 1 ms
    }
    return NULL;
}

static int set_thread_priority_safe(pthread_t thr, int policy, int prio)
{
    struct sched_param sp;
    sp.sched_priority = prio;
    int rc = pthread_setschedparam(thr, policy, &sp);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_setschedparam failed: %s (errno=%d)\n", strerror(rc), rc);
        return -1;
    }
    return 0;
}

static int get_thread_priority_safe(pthread_t thr, int *policy, int *prio)
{
    struct sched_param sp;
    int pol;
    int rc = pthread_getschedparam(thr, &pol, &sp);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_getschedparam failed: %s\n", strerror(rc));
        return -1;
    }
    *policy = pol;
    *prio = sp.sched_priority;
    return 0;
}

int main(void)
{
    pthread_t worker;
    pthread_attr_t attr;
    struct sched_param sp_main;

    // Попробуем использовать SCHED_RR — но если права/политика не позволяют,
    // мы вернёмся к SCHED_OTHER или просто сообщим об ошибке.
    int policy = SCHED_RR;
    int prio_min = sched_get_priority_min(policy);
    int prio_max = sched_get_priority_max(policy);
    if (prio_min == -1 || prio_max == -1)
    {
        // Политика SCHED_RR недоступна — переключаемся на SCHED_OTHER
        policy = SCHED_OTHER;
        prio_min = sched_get_priority_min(policy);
        prio_max = sched_get_priority_max(policy);
    }

    printf("=== QNX safe thread priority test ===\n");
    printf("PID=%d\n", getpid());
    printf("Using policy %d, priority range [%d..%d]\n", policy, prio_min, prio_max);

    // Выбираем "безопасные" приоритеты в пределах доступного диапазона
    int worker_prio_initial = prio_min + ((prio_max - prio_min) / 3); // низко-средний
    int controller_prio_low = prio_min;
    int controller_prio_high = worker_prio_initial + 2;
    if (controller_prio_high > prio_max)
        controller_prio_high = prio_max;

    printf("Planned priorities: worker=%d, controller_low=%d, controller_high=%d\n",
           worker_prio_initial, controller_prio_low, controller_prio_high);

    // Инициализация атрибутов нити
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, policy);
    struct sched_param worker_sp;
    worker_sp.sched_priority = worker_prio_initial;
    pthread_attr_setschedparam(&attr, &worker_sp);

    if (pthread_create(&worker, &attr, worker_func, NULL) != 0)
    {
        perror("pthread_create");
        return 1;
    }

    // Поставим main (controller) в безопасно низкий приоритет, чтобы worker получил CPU
    sp_main.sched_priority = controller_prio_low;
    if (pthread_setschedparam(pthread_self(), policy, &sp_main) != 0)
    {
        int e = errno;
        fprintf(stderr, "Warning: cannot lower main priority: %s (errno=%d). Continuing.\n", strerror(e), e);
    }

    printf("Даем worker 1 секунду для запуска (worker должен обновлять метку)...\n");
    sleep(1);
    printf("worker_last_run = %" PRIu64 "\n", worker_last_run);

    // Поднимем main до умеренно высокого, но безопасного приоритета, чтобы продемонстрировать эксперимент
    sp_main.sched_priority = controller_prio_high;
    if (pthread_setschedparam(pthread_self(), policy, &sp_main) != 0)
    {
        int e = errno;
        fprintf(stderr, "Warning: cannot raise main priority: %s (errno=%d). Continuing.\n", strerror(e), e);
    }
    msleep(50);

    int pol_w, pr_w;
    if (get_thread_priority_safe(worker, &pol_w, &pr_w) == 0)
    {
        printf("Current priorities: controller=%d, worker=%d\n", controller_prio_high, pr_w);
    }

    printf("\nЭксперимент A: подтверждение того же приоритета worker (без изменений)...\n");
    uint64_t t_before_A = ClockCycles();
    if (set_thread_priority_safe(worker, policy, pr_w) != 0)
    {
        fprintf(stderr, "Failed to set same priority for worker (A)\n");
    }
    uint64_t t_after_A = ClockCycles();
    msleep(10);
    uint64_t worker_last_after_A = worker_last_run;
    printf("t_before_A=%" PRIu64 ", t_after_A=%" PRIu64 ", worker_last_after_A=%" PRIu64 "\n",
           t_before_A, t_after_A, worker_last_after_A);

    printf("\nЭксперимент B: повышаем приоритет worker немного выше controller (безопасно)...\n");
    int new_worker_prio = pr_w;
    if (new_worker_prio <= controller_prio_high)
        new_worker_prio = controller_prio_high + 1;
    if (new_worker_prio > prio_max)
        new_worker_prio = prio_max;
    printf("Setting worker priority to %d\n", new_worker_prio);
    uint64_t t_before_B = ClockCycles();
    if (set_thread_priority_safe(worker, policy, new_worker_prio) != 0)
    {
        fprintf(stderr, "Failed to raise worker priority (B)\n");
    }
    uint64_t t_after_B = ClockCycles();

    // Дать немного времени, чтобы worker выполнился
    msleep(50);
    uint64_t worker_last_after_B = worker_last_run;
    printf("t_before_B=%" PRIu64 ", t_after_B=%" PRIu64 ", worker_last_after_B=%" PRIu64 "\n",
           t_before_B, t_after_B, worker_last_after_B);

    if (worker_last_after_B >= t_after_B)
    {
        printf("Worker выполнился после повышения приоритета (ожидаемо).\n");
    }
    else
    {
        printf("Worker не выполнился сразу после повышения (возможно, политика/права не позволяют preemption).\n");
    }

    printf("\nОстанавливаем worker\n");
    stop_worker = 1;
    pthread_join(worker, NULL);

    return 0;
}
