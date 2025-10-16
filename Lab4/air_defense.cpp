#include "/root/labs/plates.h"
#include <sys/neutrino.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

// Структура для хранения информации о тарелке
typedef struct
{
    int height;
    int speed;
    int direction; // 1 - слева направо, -1 - справа налево
    long long loc1_time;
    long long loc2_time;
    long long loc3_time;
    long long loc4_time;
    int processed;
    pthread_mutex_t mutex;
} PlateData;

// Структура для передачи данных в поток выстрела
typedef struct
{
    int shoot_delay;
    int plate_index;
} ShootThreadData;

// Глобальные переменные
static PlateData plates[10]; // Массив для хранения данных о тарелках
static int plate_count = 0;
static int interrupt_id = 0;
static pthread_mutex_t plates_mutex = PTHREAD_MUTEX_INITIALIZER;

// Прототипы функций
void *process_plate_left_to_right_thread(void *arg);
void *process_plate_right_to_left_thread(void *arg);
void *shoot_thread(void *arg);
long long get_time_us();
int find_or_create_plate(int height);
const char *get_locator_side(int loc_num);
void start_left_to_right_processing(int plate_index);
void start_right_to_left_processing(int plate_index);

// Функция для получения текущего времени в микросекундах
long long get_time_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

// Функция для получения названия стороны локатора
const char *get_locator_side(int loc_num)
{
    switch (loc_num)
    {
    case 1:
        return "ЛЕВЫЙ-1 (x=10)";
    case 2:
        return "ЛЕВЫЙ-2 (x=20)";
    case 3:
        return "ПРАВЫЙ-1 (x=780)";
    case 4:
        return "ПРАВЫЙ-2 (x=790)";
    default:
        return "НЕИЗВЕСТНЫЙ";
    }
}

// Поиск или создание записи о тарелке по высоте
int find_or_create_plate(int height)
{
    pthread_mutex_lock(&plates_mutex);

    // Ищем существующую запись
    for (int i = 0; i < plate_count; i++)
    {
        if (plates[i].height == height && !plates[i].processed)
        {
            pthread_mutex_unlock(&plates_mutex);
            return i;
        }
    }

    // Создаем новую запись
    if (plate_count < 10)
    {
        plates[plate_count].height = height;
        plates[plate_count].speed = 0;
        plates[plate_count].direction = 0;
        plates[plate_count].loc1_time = 0;
        plates[plate_count].loc2_time = 0;
        plates[plate_count].loc3_time = 0;
        plates[plate_count].loc4_time = 0;
        plates[plate_count].processed = 0;
        pthread_mutex_init(&plates[plate_count].mutex, NULL);
        plate_count++;
        pthread_mutex_unlock(&plates_mutex);
        return plate_count - 1;
    }

    pthread_mutex_unlock(&plates_mutex);
    return -1; // Нет свободных слотов
}

// Поток для выполнения выстрела с задержкой
void *shoot_thread(void *arg)
{
    ShootThreadData *data = (ShootThreadData *)arg;
    int shoot_delay = data->shoot_delay;
    int plate_index = data->plate_index;

    printf("=== ПОТОК ВЫСТРЕЛА ===\n");
    printf("Задержка: %d мкс\n", shoot_delay);
    printf("Цель: тарелка %d\n", plate_index);

    if (shoot_delay > 1000)
    {
        printf("Ожидание %d мкс перед выстрелом...\n", shoot_delay);
        usleep(shoot_delay);
    }

    putreg(RG_GUNS, GUNS_SHOOT);
    printf("!!! ВЫСТРЕЛ ВЫПОЛНЕН !!!\n");
    printf("=====================\n");

    // Помечаем тарелку как обработанную
    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].processed = 1;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    free(data);
    return NULL;
}

// Поток обработки тарелки слева направо
void *process_plate_left_to_right_thread(void *arg)
{
    int plate_index = *(int *)arg;
    free(arg);

    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    long long time1 = plates[plate_index].loc1_time;
    long long time2 = plates[plate_index].loc2_time;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    // Определяем правильный порядок времени (раньше -> позже)
    long long early_time = (time1 < time2) ? time1 : time2;
    long long late_time = (time1 < time2) ? time2 : time1;
    long long time_diff = late_time - early_time;

    printf("\n=== ОБРАБОТКА ТАРЕЛКИ СЛЕВА НАПРАВО ===\n");
    printf("Высота тарелки: Y=%d\n", y);
    printf("Время между локаторами: %lld мкс\n", time_diff);

    // Защита от некорректных значений времени
    if (time_diff <= 1000 || time_diff >= 10000000)
    {
        printf("ОШИБКА: Некорректное время между локаторами\n");
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
        return NULL;
    }

    // Расчет скорости
    double distance_between_locators = 10.0;
    double speed = (distance_between_locators * 1000000.0) / time_diff;

    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].speed = (int)speed;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    printf("Расчетная скорость: %.2f точек/сек\n", speed);

    // Расчет времени и расстояния
    int distance_to_center = 400 - 20; // от локатора 2 до центра
    double time_to_center = (double)distance_to_center / speed;

    // Расчет времени полета ракеты
    int rocket_distance = 570 - y;
    double rocket_time = (double)rocket_distance / 100.0;

    // Вычисляем задержку выстрела
    double shoot_delay_seconds = time_to_center - rocket_time;
    int shoot_delay = (int)(shoot_delay_seconds * 1000000);

    printf("=== РАСЧЕТ ПАРАМЕТРОВ ВЫСТРЕЛА ===\n");
    printf("Расстояние до центра: %d точек\n", distance_to_center);
    printf("Время до центра: %.3f сек\n", time_to_center);
    printf("Расстояние полета ракеты: %d точек\n", rocket_distance);
    printf("Время полета ракеты: %.3f сек\n", rocket_time);
    printf("Расчетная задержка выстрела: %d мкс\n", shoot_delay);

    // Выполняем выстрел только при положительной задержке
    if (shoot_delay > 1000)
    {
        ShootThreadData *data = (ShootThreadData *)malloc(sizeof(ShootThreadData));
        data->shoot_delay = shoot_delay;
        data->plate_index = plate_index;

        pthread_t thread;
        pthread_create(&thread, NULL, shoot_thread, data);
        pthread_detach(thread);

        printf("Создан поток для выстрела\n");
    }
    else if (shoot_delay >= 0)
    {
        // Немедленный выстрел при небольшой положительной задержке
        printf("Немедленный выстрел (небольшая задержка)\n");
        putreg(RG_GUNS, GUNS_SHOOT);
        printf("!!! ВЫСТРЕЛ ВЫПОЛНЕН !!!\n");

        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
    }
    else
    {
        // Отрицательная задержка - не производим выстрел
        printf("ПРОПУСК: Отрицательная задержка - тарелка слишком быстрая\n");

        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
    }

    printf("=================================\n");
    return NULL;
}

// Поток обработки тарелки справа налево
void *process_plate_right_to_left_thread(void *arg)
{
    int plate_index = *(int *)arg;
    free(arg);

    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    long long time3 = plates[plate_index].loc3_time;
    long long time4 = plates[plate_index].loc4_time;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    // Определяем правильный порядок времени (раньше -> позже)
    long long early_time = (time3 < time4) ? time3 : time4;
    long long late_time = (time3 < time4) ? time4 : time3;
    long long time_diff = late_time - early_time;

    printf("\n=== ОБРАБОТКА ТАРЕЛКИ СПРАВА НАЛЕВО ===\n");
    printf("Высота тарелки: Y=%d\n", y);
    printf("Время между локаторами: %lld мкс\n", time_diff);

    // Защита от некорректных значений времени
    if (time_diff <= 1000 || time_diff >= 10000000)
    {
        printf("ОШИБКА: Некорректное время между локаторами\n");
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
        return NULL;
    }

    // Расчет скорости
    double distance_between_locators = 10.0;
    double speed = (distance_between_locators * 1000000.0) / time_diff;

    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].speed = (int)speed;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    printf("Расчетная скорость: %.2f точек/сек\n", speed);

    // Расчет времени и расстояния
    int distance_to_center = 790 - 400; // от локатора 4 до центра
    double time_to_center = (double)distance_to_center / speed;

    // Расчет времени полета ракеты
    int rocket_distance = 570 - y;
    double rocket_time = (double)rocket_distance / 100.0;

    // Вычисляем задержку выстрела
    double shoot_delay_seconds = time_to_center - rocket_time;
    int shoot_delay = (int)(shoot_delay_seconds * 1000000);

    printf("=== РАСЧЕТ ПАРАМЕТРОВ ВЫСТРЕЛА ===\n");
    printf("Расстояние до центра: %d точек\n", distance_to_center);
    printf("Время до центра: %.3f сек\n", time_to_center);
    printf("Расстояние полета ракеты: %d точек\n", rocket_distance);
    printf("Время полета ракеты: %.3f сек\n", rocket_time);
    printf("Расчетная задержка выстрела: %d мкс\n", shoot_delay);

    // Выполняем выстрел только при положительной задержке
    if (shoot_delay > 1000)
    {
        ShootThreadData *data = (ShootThreadData *)malloc(sizeof(ShootThreadData));
        data->shoot_delay = shoot_delay;
        data->plate_index = plate_index;

        pthread_t thread;
        pthread_create(&thread, NULL, shoot_thread, data);
        pthread_detach(thread);

        printf("Создан поток для выстрела\n");
    }
    else if (shoot_delay >= 0)
    {
        // Немедленный выстрел при небольшой положительной задержке
        printf("Немедленный выстрел (небольшая задержка)\n");
        putreg(RG_GUNS, GUNS_SHOOT);
        printf("!!! ВЫСТРЕЛ ВЫПОЛНЕН !!!\n");

        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
    }
    else
    {
        // Отрицательная задержка - не производим выстрел
        printf("ПРОПУСК: Отрицательная задержка - тарелка слишком быстрая\n");

        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
    }

    printf("=================================\n");
    return NULL;
}

// Запуск обработки тарелки слева направо (общая функция)
void start_left_to_right_processing(int plate_index)
{
    printf("Запуск обработки тарелки слева направо...\n");

    int *plate_ptr = (int *)malloc(sizeof(int));
    *plate_ptr = plate_index;

    pthread_t thread;
    pthread_create(&thread, NULL, process_plate_left_to_right_thread, plate_ptr);
    pthread_detach(thread);
}

// Запуск обработки тарелки справа налево (общая функция)
void start_right_to_left_processing(int plate_index)
{
    printf("Запуск обработки тарелки справа налево...\n");

    int *plate_ptr = (int *)malloc(sizeof(int));
    *plate_ptr = plate_index;

    pthread_t thread;
    pthread_create(&thread, NULL, process_plate_right_to_left_thread, plate_ptr);
    pthread_detach(thread);
}

// Ступень 0: Управление РУСом по заданным траекториям
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
    usleep(400000);

    // Движение по квадрату 200x200
    printf("Начинаем движение по квадрату 200x200\n");

    // Влево на 100 точек
    putreg(RG_RCMC, RCMC_LEFT);
    printf("Движение влево (100 точек)\n");
    usleep(400000);

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

    // Движение по прямоугольнику 500x200
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

// Обработчик прерывания от локаторов для ступени 1
const struct sigevent *locator_handler_step1(void *area, int id)
{
    int loc_num = getreg(RG_LOCN);
    int y = getreg(RG_LOCY);
    int w = getreg(RG_LOCW);

    // Обрабатываем только тарелки (W=3)
    if (w == 3)
    {
        printf("\n=== ФИКСАЦИЯ ТАРЕЛКИ ===\n");
        printf("Локатор: %s\n", get_locator_side(loc_num));
        printf("Координаты: Y=%d\n", y);
        printf("Время фиксации: %lld мкс\n", get_time_us());

        // Игнорируем локаторы 3 и 4 для тарелок, летящих слева направо
        if (loc_num == 3 || loc_num == 4)
        {
            printf("ИГНОРИРУЕМ: локатор с противоположной стороны\n");
            printf("====================\n");
            return NULL;
        }

        long long current_time = get_time_us();
        int plate_index = find_or_create_plate(y);

        if (plate_index == -1)
        {
            printf("ОШИБКА: Нет свободных слотов для тарелки\n");
            printf("====================\n");
            return NULL;
        }

        pthread_mutex_lock(&plates[plate_index].mutex);

        if (loc_num == 1)
        {
            plates[plate_index].loc1_time = current_time;
            plates[plate_index].direction = 1;
            printf("Зафиксирована новая тарелка слева\n");

            // Проверяем, есть ли уже данные от локатора 2
            if (plates[plate_index].loc2_time != 0)
            {
                start_left_to_right_processing(plate_index);
            }
        }
        else if (loc_num == 2)
        {
            plates[plate_index].loc2_time = current_time;
            plates[plate_index].direction = 1;
            printf("Зафиксирован второй локатор для тарелки слева\n");

            // Проверяем, есть ли уже данные от локатора 1
            if (plates[plate_index].loc1_time != 0)
            {
                start_left_to_right_processing(plate_index);
            }
        }

        pthread_mutex_unlock(&plates[plate_index].mutex);
        printf("====================\n");
    }

    return NULL;
}

// Обработчик прерывания от локаторов для ступени 2
const struct sigevent *locator_handler_step2(void *area, int id)
{
    int loc_num = getreg(RG_LOCN);
    int y = getreg(RG_LOCY);
    int w = getreg(RG_LOCW);

    // Обрабатываем только тарелки (W=3)
    if (w == 3)
    {
        printf("\n=== ФИКСАЦИЯ ТАРЕЛКИ ===\n");
        printf("Локатор: %s\n", get_locator_side(loc_num));
        printf("Координаты: Y=%d\n", y);
        printf("Время фиксации: %lld мкс\n", get_time_us());

        long long current_time = get_time_us();
        int plate_index = find_or_create_plate(y);

        if (plate_index == -1)
        {
            printf("ОШИБКА: Нет свободных слотов для тарелки\n");
            printf("====================\n");
            return NULL;
        }

        pthread_mutex_lock(&plates[plate_index].mutex);

        // Обработка для разных направлений движения
        switch (loc_num)
        {
        case 1: // Локатор 1 (x=10) - слева направо
            // Игнорируем локатор 1, если тарелка уже зафиксирована справа
            if (plates[plate_index].loc3_time != 0 || plates[plate_index].loc4_time != 0)
            {
                printf("ИГНОРИРУЕМ: тарелка уже зафиксирована с правой стороны\n");
                pthread_mutex_unlock(&plates[plate_index].mutex);
                printf("====================\n");
                return NULL;
            }
            plates[plate_index].loc1_time = current_time;
            plates[plate_index].direction = 1;
            printf("Зафиксирована новая тарелка слева\n");

            // Проверяем, есть ли уже данные от локатора 2
            if (plates[plate_index].loc2_time != 0)
            {
                start_left_to_right_processing(plate_index);
            }
            break;

        case 2: // Локатор 2 (x=20) - слева направо
            // Игнорируем локатор 2, если тарелка уже зафиксирована справа
            if (plates[plate_index].loc3_time != 0 || plates[plate_index].loc4_time != 0)
            {
                printf("ИГНОРИРУЕМ: тарелка уже зафиксирована с правой стороны\n");
                pthread_mutex_unlock(&plates[plate_index].mutex);
                printf("====================\n");
                return NULL;
            }
            plates[plate_index].loc2_time = current_time;
            plates[plate_index].direction = 1;
            printf("Зафиксирован второй локатор для тарелки слева\n");

            // Проверяем, есть ли уже данные от локатора 1
            if (plates[plate_index].loc1_time != 0)
            {
                start_left_to_right_processing(plate_index);
            }
            break;

        case 3: // Локатор 3 (x=780) - справа налево
            // Игнорируем локатор 3, если тарелка уже зафиксирована слева
            if (plates[plate_index].loc1_time != 0 || plates[plate_index].loc2_time != 0)
            {
                printf("ИГНОРИРУЕМ: тарелка уже зафиксирована с левой стороны\n");
                pthread_mutex_unlock(&plates[plate_index].mutex);
                printf("====================\n");
                return NULL;
            }
            plates[plate_index].loc3_time = current_time;
            plates[plate_index].direction = -1;
            printf("Зафиксирована новая тарелка справа\n");

            // Проверяем, есть ли уже данные от локатора 4
            if (plates[plate_index].loc4_time != 0)
            {
                start_right_to_left_processing(plate_index);
            }
            break;

        case 4: // Локатор 4 (x=790) - справа налево
            // Игнорируем локатор 4, если тарелка уже зафиксирована слева
            if (plates[plate_index].loc1_time != 0 || plates[plate_index].loc2_time != 0)
            {
                printf("ИГНОРИРУЕМ: тарелка уже зафиксирована с левой стороны\n");
                pthread_mutex_unlock(&plates[plate_index].mutex);
                printf("====================\n");
                return NULL;
            }
            plates[plate_index].loc4_time = current_time;
            plates[plate_index].direction = -1;
            printf("Зафиксирован второй локатор для тарелки справа\n");

            // Проверяем, есть ли уже данные от локатора 3
            if (plates[plate_index].loc3_time != 0)
            {
                start_right_to_left_processing(plate_index);
            }
            break;
        }

        pthread_mutex_unlock(&plates[plate_index].mutex);
        printf("====================\n");
    }

    return NULL;
}

// Ступень 1: Уничтожение одной тарелки ракетой
void step1()
{
    printf("=== СТУПЕНЬ 1: Запуск системы ПВО ===\n");

    // Инициализируем переменные
    plate_count = 0;
    for (int i = 0; i < 10; i++)
    {
        plates[i].processed = 1; // Помечаем все как обработанные
        pthread_mutex_init(&plates[i].mutex, NULL);
    }

    // Подключаем обработчик прерываний от локаторов
    interrupt_id = InterruptAttach(LOC_INTR, locator_handler_step1, NULL, 0, 0);
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

// Ступень 2: Уничтожение нескольких тарелок ракетами
void step2()
{
    printf("=== СТУПЕНЬ 2: Запуск системы ПВО для нескольких тарелок ===\n");

    // Инициализируем переменные
    plate_count = 0;
    for (int i = 0; i < 10; i++)
    {
        plates[i].processed = 1; // Помечаем все как обработанные
        pthread_mutex_init(&plates[i].mutex, NULL);
    }

    // Подключаем обработчик прерываний от локаторов
    interrupt_id = InterruptAttach(LOC_INTR, locator_handler_step2, NULL, 0, 0);
    printf("Обработчик прерываний подключен, ID=%d\n", interrupt_id);

    printf("Ожидание обнаружения тарелок...\n");

    // Ждем обнаружения и уничтожения тарелок
    for (int i = 0; i < 90; i++)
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

    printf("=== СТУПЕНЬ 2: Завершено ===\n");
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
    else if (level == 2)
    {
        step2();
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
