#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <inttypes.h>
#include <unistd.h>

volatile uint64_t t_worker_after_create = 0;
volatile uint64_t t_new_thread_start = 0;

// Новая нить B
void *new_thread(void *arg)
{

    // фиксируем момент старта
    t_new_thread_start = ClockCycles();

    return NULL;
}

// Рабочая нить A
void *worker_thread(void *arg)
{
    pthread_t tid;

    // создаём новую нить
    pthread_create(&tid, NULL, new_thread, NULL);

    // фиксируем момент сразу после pthread_create
    t_worker_after_create = ClockCycles();

    pthread_join(tid, NULL);
    return NULL;
}

int main()
{
    pthread_t worker;

    uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    printf("cycles_per_sec = %" PRIu64 "\n", cps);

    pthread_create(&worker, NULL, worker_thread, NULL);

    pthread_join(worker, NULL);

    printf("t_new_thread_start     = %" PRIu64 "\n", t_new_thread_start);
    printf("t_worker_after_create  = %" PRIu64 "\n", t_worker_after_create);

    if (t_new_thread_start == 0)
    {
        printf("\n>>> Error: the new thread did not start.\n");
    }
    else if (t_new_thread_start < t_worker_after_create)
    {
        printf("\n>>> The new thread got the processor first\n");
    }
    else
    {
        printf("\n>>> The creating thread continued to execute first\n");
    }

    return 0;
}
