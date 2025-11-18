## **0. Компиляция и запуск**

```bash
qcc -o sched sched.c 
on -C 0 ./sched
```

## **1. Задание**

Проверить, какая нить получает процессор первой в QNX Neutrino 6 при создании новой нити:

* новая нить, которая только что создана,
  или
* нить, которая её создала.

Измерения времени делались через `ClockCycles()`. Процесс был привязан к одному CPU с помощью `on -C 0`.

---

## **2. Алгоритм работы программы**

1. Основная нить создаёт рабочую нить `A`.
2. Нить `A` создаёт новую нить `B`.
3. Сразу после `pthread_create` нить `A` фиксирует значение `ClockCycles()` → `t_worker_after_create`.
4. Новая нить `B` при старте сразу фиксирует `ClockCycles()` → `t_new_thread_start`.
5. После завершения обеих нитей программа сравнивает времена.

**Принцип:** если `t_new_thread_start < t_worker_after_create`, значит новая нить получила процессор первой.

---

## **3. Программный код**

```c
#include <stdio.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <inttypes.h>

volatile uint64_t t_worker_after_create = 0;
volatile uint64_t t_new_thread_start = 0;

void* new_thread(void* arg) {
    t_new_thread_start = ClockCycles();
    return NULL;
}

void* worker_thread(void* arg) {
    pthread_t tid;
    pthread_create(&tid, NULL, new_thread, NULL);
    t_worker_after_create = ClockCycles();
    pthread_join(tid, NULL);
    return NULL;
}

int main() {
    pthread_t worker;
    printf("cycles_per_sec = %" PRIu64 "\n", SYSPAGE_ENTRY(qtime)->cycles_per_sec);
    pthread_create(&worker, NULL, worker_thread, NULL);
    pthread_join(worker, NULL);

    printf("t_new_thread_start     = %" PRIu64 "\n", t_new_thread_start);
    printf("t_worker_after_create  = %" PRIu64 "\n", t_worker_after_create);

    if (t_new_thread_start < t_worker_after_create)
        printf("\n>>> Новая нить получила процессор первой\n");
    else
        printf("\n>>> Создающая нить продолжила выполняться первой\n");

    return 0;
}
```

---

## **4. Анализ результатов**

Результаты твоего запуска:

```
cycles_per_sec = 2688282000
t_new_thread_start     = 1956911499346
t_worker_after_create  = 1956911666774

>>> Новая нить получила процессор первой
```

* `t_new_thread_start < t_worker_after_create` → новая нить стартовала раньше, чем создавшая нить продолжила выполнение.
* Это показывает, что планировщик QNX **сразу переключается на только что созданную нить**, если её приоритет совпадает с создавшей.

---

## **5. Вывод**

В данном эксперименте новая нить получила процессор первой. Это подтверждает поведение планировщика QNX Neutrino 6: после создания нити он может немедленно передать ей управление, даже если создающая нить ещё не закончила выполнение.

Такое знание важно при разработке реального времени, чтобы правильно планировать выполнение зависимых нитей.
