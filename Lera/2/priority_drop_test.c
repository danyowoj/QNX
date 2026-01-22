/*
! Компиляция и запуск:
qcc -O2 -Wall priority_drop_test.c -o priority_drop_test
on -C ./priority_drop_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <sys/time.h>

volatile int ready = 0;
volatile int competitor_ran = 0;

// Низкоприоритетная нить
void *competitor_thread(void *arg)
{
    // Ждем сигнала от worker
    while (!ready)
    {
    }

    competitor_ran = 1;

    printf("[competitor] Got control after lowering the priority of worker.\n");
    return NULL;
}

// Высокоприоритетная нить, которая снижает свой приоритет
void *worker_thread(void *arg)
{
    int policy = SCHED_FIFO;
    struct sched_param param;

    printf("[worker] Launched with high priority.\n");

    // Дадим competitor нити создаться
    usleep(10000);

    // Разрешаем competitor работать после нашего снижения приоритета
    ready = 1;

    // Снижаем приоритет до 5
    param.sched_priority = 5;
    ThreadCtl(_NTO_TCTL_IO, 0);
    if (pthread_setschedparam(pthread_self(), policy, &param) != 0)
    {
        perror("pthread_setschedparam");
        exit(1);
    }

    printf("[worker] Lowered the priority to 5.\n");

    // Если планировщик переставил нас в конец очереди — competitor успеет выполнить code
    // Если планировщик оставил нас в начале — competitor не получит процессор
    usleep(20000);

    if (competitor_ran)
        printf("[result] Worker has been placed at the END of the new priority queue.\n");
    else
        printf("[result] The worker stayed at the FRONT of the queue (got the CPU first).\n");

    return NULL;
}

int main()
{
    pthread_t worker, competitor;
    struct sched_param param;

    printf("=== QNX priority decrease test ===\n");

    // Создаем competitor с низким приоритетом: 5
    param.sched_priority = 5;
    pthread_attr_t attr_comp;
    pthread_attr_init(&attr_comp);
    pthread_attr_setschedpolicy(&attr_comp, SCHED_FIFO);
    pthread_attr_setschedparam(&attr_comp, &param);
    pthread_create(&competitor, &attr_comp, competitor_thread, NULL);

    // Создаем worker с высоким приоритетом: 10
    param.sched_priority = 10;
    pthread_attr_t attr_worker;
    pthread_attr_init(&attr_worker);
    pthread_attr_setschedpolicy(&attr_worker, SCHED_FIFO);
    pthread_attr_setschedparam(&attr_worker, &param);
    pthread_create(&worker, &attr_worker, worker_thread, NULL);

    pthread_join(worker, NULL);
    pthread_join(competitor, NULL);

    return 0;
}
