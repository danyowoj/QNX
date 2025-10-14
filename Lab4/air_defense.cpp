#include "/root/labs/plates.h"
#include <sys/neutrino.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

// Глобальные переменные для отслеживания тарелок
static int target_y = 0;
static int target_speed = 0;
static int target_direction = 0;
static long long loc1_time = 0;
static long long loc2_time = 0;
static int interrupt_id = 0;
static int active_plate_height = 0;

// Функция для получения текущего времени в микросекундах
long long get_time_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

//! Ступень 0: Управление РУСом по заданным траекториям
void step0()
{
    printf("=== СТУПЕНЬ 0: Запуск управления РУСом ===\n");

    int rcm_num = 0;
    putreg(RG_RCMN, rcm_num);
    printf("Установлен РУС номер %d\n", rcm_num);

    // Начальная позиция (400, 570) - середина нижней грани

    // Запускаем снаряд вверх
    putreg(RG_RCMC, RCMC_START);
    printf("РУС запущен вверх\n");
    usleep(400000); // Ждем немного

    //* === Движение по квадрату 200x200 ===
    printf("Начинаем движение по квадрату 200x200\n");

    // Влево на 100 точек
    putreg(RG_RCMC, RCMC_LEFT);
    printf("Движение влево (100 точек)\n");
    usleep(400000); // 100 точек при скорости 250 точек/сек = 0.4 сек

    // Вверх на 200 точек
    putreg(RG_RCMC, RCMC_UP);
    printf("Движение вверх (200 точек)\n");
    usleep(800000);

    // Вправо на 200 точек
    putreg(RG_RCMC, RCMC_RIGHT);
    printf("Движение вправо (200 точек)\n");
    usleep(800000);

    // Вниз на 200 точек
    putreg(RG_RCMC, RCMC_DOWN);
    printf("Движение вниз (200 точек)\n");
    usleep(800000);

    // Влево на 100 точек (возврат к середине)
    putreg(RG_RCMC, RCMC_LEFT);
    printf("Движение влево (100 точек)\n");
    usleep(400000);

    //* === Движение по прямоугольнику 500x200 ===
    printf("Начинаем движение по прямоугольнику 500x200\n");

    // Влево на 250 точек
    putreg(RG_RCMC, RCMC_LEFT);
    printf("Движение влево (250 точек)\n");
    usleep(1000000);

    // Вверх на 200 точек
    putreg(RG_RCMC, RCMC_UP);
    printf("Движение вверх (200 точек)\n");
    usleep(800000);

    // Вправо на 500 точек
    putreg(RG_RCMC, RCMC_RIGHT);
    printf("Движение вправо (500 точек)\n");
    usleep(2000000);

    // Вниз на 200 точек
    putreg(RG_RCMC, RCMC_DOWN);
    printf("Движение вниз (200 точек)\n");
    usleep(800000);

    // Влево на 250 точек (возврат к середине)
    putreg(RG_RCMC, RCMC_LEFT);
    printf("Движение влево (250 точек)\n");
    usleep(1000000);

    printf("=== СТУПЕНЬ 0: Завершено ===\n");
}

// Обработчик прерывания от локаторов
const struct sigevent *locator_handler(void *area, int id)
{
    int loc_num = getreg(RG_LOCN);
    int y = getreg(RG_LOCY);
    int w = getreg(RG_LOCW);

    printf("[Локатор %d] Y=%d, W=%d\n", loc_num, y, w);

    // Обрабатываем только тарелки (W=3)
    if (w == 3)
    {
        // Игнорируем локаторы 3 и 4 для тарелок, летящих слева направо
        if (loc_num == 3 || loc_num == 4)
        {
            printf("[Локатор %d] Игнорируем для тарелки, летящей слева направо\n", loc_num);
            return NULL;
        }

        long long current_time = get_time_us();

        if (loc_num == 1)
        {
            // Сбрасываем состояние при обнаружении новой тарелки
            if (y != active_plate_height)
            {
                loc1_time = 0;
                loc2_time = 0;
                active_plate_height = y;
                printf("[Локатор 1] Новая тарелка: Y=%d\n", y);
            }

            loc1_time = current_time;
            target_y = y;
            printf("[Локатор 1] Фиксация тарелки: Y=%d, время=%lld\n", y, current_time);
        }
        else if (loc_num == 2)
        {
            if (loc1_time != 0 && y == active_plate_height)
            {
                loc2_time = current_time;
                long long time_diff = loc2_time - loc1_time;

                printf("[Локатор 2] Фиксация тарелки: Y=%d\n", y);
                printf("Время между локаторами 1-2: %lld мкс\n", time_diff);

                // Защита от некорректных значений времени
                if (time_diff > 1000 && time_diff < 10000000)
                {
                    target_direction = 1; // слева направо

                    // Более точный расчет скорости
                    double distance_between_locators = 10.0;
                    double speed = (distance_between_locators * 1000000.0) / time_diff;
                    target_speed = (int)speed;

                    printf("Скорость тарелки: %.2f точек/сек\n", speed);

                    // Уточненный расчет времени и расстояния
                    int distance_to_center = 400 - 20;
                    double time_to_center = (double)distance_to_center / speed;

                    // Уточненный расчет времени полета ракеты
                    int rocket_distance = 570 - y;
                    double rocket_time = (double)rocket_distance / 100.0;

                    // Вычисляем когда нужно выстрелить
                    double shoot_delay_seconds = time_to_center - rocket_time;
                    int shoot_delay = (int)(shoot_delay_seconds * 1000000);

                    printf("Расстояние до центра: %d точек\n", distance_to_center);
                    printf("Время до центра: %.3f сек\n", time_to_center);
                    printf("Время полета ракеты: %.3f сек\n", rocket_time);
                    printf("Задержка выстрела: %d мкс\n", shoot_delay);

                    // Проверка на мминимальную задержку
                    if (shoot_delay > 1000)
                    {
                        printf("Выстрел через %d мкс\n", shoot_delay);
                        usleep(shoot_delay);
                        putreg(RG_GUNS, GUNS_SHOOT);
                        printf("!!! ВЫСТРЕЛ !!!\n");
                    }
                    else if (shoot_delay <= 1000 && shoot_delay >= -1000000)
                    {
                        // Если задержка небольшая или отрицательная, но в разумных пределах, стреляем немедленно
                        printf("Выстрел немедленно (задержка: %d мкс)\n", shoot_delay);
                        putreg(RG_GUNS, GUNS_SHOOT);
                        printf("!!! ВЫСТРЕЛ !!!\n");
                    }
                    else
                    {
                        printf("Некорректная задержка выстрела: %d мкс\n", shoot_delay);
                    }
                }
                else
                {
                    printf("Некорректное время между локаторами: %lld мкс\n", time_diff);
                }

                // Сбрасываем для следующей тарелки
                loc1_time = 0;
                active_plate_height = 0;
            }
            else
            {
                printf("[Локатор 2] Пропуск: нет данных от локатора 1 или другая тарелка\n");
            }
        }
    }
    else if (w == 1)
    {
        printf("[Локатор %d] Обнаружена птица (W=1), игнорируем\n", loc_num);
    }
    else
    {
        printf("[Локатор %d] Неизвестный объект W=%d\n", loc_num, w);
    }

    return NULL;
}

//! Ступень 1: Уничтожение одной тарелки ракетой
void step1()
{
    printf("=== СТУПЕНЬ 1: Запуск системы ПВО ===\n");

    // Инициализируем переменные
    target_y = 0;
    target_speed = 0;
    target_direction = 0;
    loc1_time = 0;
    loc2_time = 0;
    active_plate_height = 0;

    // Подключаем обработчик прерываний от локаторов
    interrupt_id = InterruptAttach(LOC_INTR, locator_handler, NULL, 0, 0);
    printf("Обработчик прерываний подключен, ID=%d\n", interrupt_id);

    printf("Ожидание обнаружения тарелок...\n");

    // Ждем пока тарелка будет обнаружена и сбита
    for (int i = 0; i < 60; i++)
    {
        printf(".");
        fflush(stdout);
        sleep(1);
    }
    printf("\n");

    // Отключаем обработчик
    if (interrupt_id > 0)
    {
        InterruptDetach(interrupt_id);
        printf("Обработчик прерываний отключен\n");
    }

    printf("=== СТУПЕНЬ 1: Завершено ===\n");
}

int main(int argc, char *argv[])
{
    int level = 0;

    // Определяем уровень из аргументов командной строки
    if (argc > 1)
    {
        level = atoi(argv[1]);
    }

    printf("Программа запущена на уровне %d\n", level);

    StartGame(level);
    printf("Игра начата на уровне %d\n", level);

    if (level == 0)
    {
        step0();
    }
    else if (level == 1)
    {
        step1();
    }
    else
    {
        printf("Неизвестный уровень: %d\n", level);
    }

    // Даем время на завершение анимации
    printf("Завершение работы...\n");
    sleep(5);
    EndGame();
    printf("Игра завершена\n");

    return 0;
}
