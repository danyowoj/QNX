#include "/root/labs/plates.h"
#include <sys/neutrino.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#define PLATE_SLOTS 30
#define RUS_SLOTS 15
#define MAX_DESTROYED 25
#define SPEED_THRESHOLD 100

// Структура для хранения данных о тарелке
typedef struct {
    int height;                    // Высота полета тарелки
    int speed;                     // Скорость тарелки (вычисленная)
    int direction;                 // Направление: 1 - слева направо, -1 - справа налево
    long long loc1_time;           // Время обнаружения локатором 1 (LEFT-1)
    long long loc2_time;           // Время обнаружения локатором 2 (LEFT-2)  
    long long loc3_time;           // Время обнаружения локатором 3 (RIGHT-1)
    long long loc4_time;           // Время обнаружения локатором 4 (RIGHT-2)
    int processed;                 // Флаг обработки: 0 - активна, 1 - обработана/свободна
    pthread_mutex_t mutex;         // Мьютекс для синхронизации доступа к данным тарелки
} PlateData;

// Структура для хранения информации о РУС (радиоуправляемом снаряде)
typedef struct {
    int rus_num;                   // Номер РУС
    int is_active;                 // Флаг активности: 1 - занят, 0 - свободен
    pthread_mutex_t mutex;         // Мьютекс для синхронизации доступа к РУС
} RUSInfo;

// Структура для передачи данных в потоки стрельбы
typedef struct {
    int plate_index;               // Индекс тарелки-цели
    int use_rus;                   // Флаг использования РУС (1) или ракеты (0)
    int shoot_delay;               // Задержка выстрела в микросекундах
    int rus_num;                   // Номер РУС (если используется)
} ShootThreadData;

// Получение текущего времени в микросекундах с использованием CLOCK_MONOTONIC
long long get_time_us(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

// Получение текстового названия локатора по его номеру
const char *get_locator_side(int loc_num) {
    switch (loc_num) {
        case 1: return "LEFT-1";
        case 2: return "LEFT-2";
        case 3: return "RIGHT-1";
        case 4: return "RIGHT-2";
        default: return "UNKNOWN";
    }
}

// Инициализация массива РУС: установка начальных значений и инициализация мьютексов
void init_rus_array(void) {
    pthread_mutex_lock(&rus_array_mutex);
    for (int i = 0; i < RUS_SLOTS; ++i) {
        rus_array[i].rus_num = i;
        rus_array[i].is_active = 0;  // Все РУС изначально свободны
        pthread_mutex_init(&rus_array[i].mutex, NULL);
    }
    pthread_mutex_unlock(&rus_array_mutex);
}

// Поиск свободного РУС и его блокировка для использования
int get_available_rus(void) {
    pthread_mutex_lock(&rus_array_mutex);
    for (int i = 0; i < RUS_SLOTS; ++i) {
        if (!rus_array[i].is_active) {
            rus_array[i].is_active = 1;  // Помечаем РУС как занятый
            pthread_mutex_unlock(&rus_array_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&rus_array_mutex);
    return -1;  // Все РУС заняты
}

// Освобождение РУС: сброс флага активности
void release_rus(int rus_num) {
    if (rus_num < 0 || rus_num >= RUS_SLOTS) return;
    pthread_mutex_lock(&rus_array_mutex);
    rus_array[rus_num].is_active = 0;
    pthread_mutex_unlock(&rus_array_mutex);
}

// Отправка команды на конкретный РУС с проверкой его доступности
void send_rus_command(int rus_num, int command) {
    if (rus_num < 0 || rus_num >= RUS_SLOTS) return;

    pthread_mutex_lock(&rus_array_mutex);
    if (!rus_array[rus_num].is_active) {
        pthread_mutex_unlock(&rus_array_mutex);
        return;  // Не отправляем команду на неактивный РУС
    }
    pthread_mutex_lock(&rus_array[rus_num].mutex);
    pthread_mutex_unlock(&rus_array_mutex);

    // Запись команды в аппаратные регистры
    putreg(RG_RCMN, rus_num);    // Установка номера РУС
    putreg(RG_RCMC, command);    // Отправка команды

    pthread_mutex_unlock(&rus_array[rus_num].mutex);
}

// Очистка устаревших записей о тарелках для освобождения слотов памяти
void cleanup_old_plates(void) {
    long long current_time = get_time_us();
    pthread_mutex_lock(&plates_mutex);
    for (int i = 0; i < plate_count; ++i) {
        // Находим максимальное время обнаружения для тарелки
        long long max_time = 0;
        if (plates[i].loc1_time > max_time) max_time = plates[i].loc1_time;
        if (plates[i].loc2_time > max_time) max_time = plates[i].loc2_time;
        if (plates[i].loc3_time > max_time) max_time = plates[i].loc3_time;
        if (plates[i].loc4_time > max_time) max_time = plates[i].loc4_time;

        // Очищаем тарелку если прошло более 3 секунд с последнего обнаружения
        // или 1 секунда для обработанных тарелок
        if ((max_time > 0 && (current_time - max_time) > 3000000LL) ||
            (plates[i].processed && (current_time - max_time) > 1000000LL)) {
            pthread_mutex_lock(&plates[i].mutex);
            // Сброс всех данных тарелки
            plates[i].height = 0;
            plates[i].speed = 0;
            plates[i].direction = 0;
            plates[i].loc1_time = plates[i].loc2_time = plates[i].loc3_time = plates[i].loc4_time = 0;
            plates[i].processed = 1;  // Помечаем слот как свободный
            pthread_mutex_unlock(&plates[i].mutex);
        }
    }
    pthread_mutex_unlock(&plates_mutex);
}

// Поиск существующей или создание новой записи о тарелке
int find_or_create_plate(int height) {
    pthread_mutex_lock(&plates_mutex);
    // Поиск существующей активной тарелки с такой же высотой
    for (int i = 0; i < plate_count; ++i) {
        pthread_mutex_lock(&plates[i].mutex);
        if (!plates[i].processed && plates[i].height == height) {
            pthread_mutex_unlock(&plates[i].mutex);
            pthread_mutex_unlock(&plates_mutex);
            return i;  // Найдена существующая тарелка
        }
        pthread_mutex_unlock(&plates[i].mutex);
    }
    // Поиск свободного слота для переиспользования
    for (int i = 0; i < plate_count; ++i) {
        pthread_mutex_lock(&plates[i].mutex);
        if (plates[i].processed) {
            // Инициализация свободного слота
            plates[i].height = height;
            plates[i].speed = 0;
            plates[i].direction = 0;
            plates[i].loc1_time = plates[i].loc2_time = plates[i].loc3_time = plates[i].loc4_time = 0;
            plates[i].processed = 0;  // Помечаем как активную
            pthread_mutex_unlock(&plates[i].mutex);
            pthread_mutex_unlock(&plates_mutex);
            return i;
        }
        pthread_mutex_unlock(&plates[i].mutex);
    }
    // Создание нового слота если есть место
    if (plate_count < PLATE_SLOTS) {
        int idx = plate_count++;
        plates[idx].height = height;
        plates[idx].speed = 0;
        plates[idx].direction = 0;
        plates[idx].loc1_time = plates[idx].loc2_time = plates[idx].loc3_time = plates[idx].loc4_time = 0;
        plates[idx].processed = 0;
        pthread_mutex_init(&plates[idx].mutex, NULL);
        pthread_mutex_unlock(&plates_mutex);
        return idx;
    }
    pthread_mutex_unlock(&plates_mutex);
    return -1;  // Нет свободных слотов
}

// Логирование обнаружения тарелки
static void log_detection(int plate_index, double speed) {
    printf("DETECTED: plate=%d speed=%.2f\n", plate_index, speed);
}

// Логирование попадания РУС с увеличением счетчика уничтоженных тарелок
static void log_hit_by_rus(int plate_index, int speed, int rus_num) {
    pthread_mutex_lock(&destroyed_mutex);
    destroyed_plates++;
    int total = destroyed_plates;
    pthread_mutex_unlock(&destroyed_mutex);
    printf("HIT: plate=%d speed=%d by RUS=%d. TOTAL=%d\n", plate_index, speed, rus_num, total);
}

// Логирование попадания ракетой с увеличением счетчика уничтоженных тарелок
static void log_hit_by_rocket(int plate_index, int speed) {
    pthread_mutex_lock(&destroyed_mutex);
    destroyed_plates++;
    int total = destroyed_plates;
    pthread_mutex_unlock(&destroyed_mutex);
    printf("HIT: plate=%d speed=%d by ROCKET. TOTAL=%d\n", plate_index, speed, total);
}

// Поток для стрельбы РУС: рассчитывает траекторию и отправляет команды
void *rus_shoot_thread(void *arg) {
    if (!arg) return NULL;
    ShootThreadData *data = (ShootThreadData *)arg;
    int plate_index = data->plate_index;
    int rus_num = data->rus_num;

    // Чтение параметров тарелки под мьютексом
    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    int direction = plates[plate_index].direction;
    int speed = plates[plate_index].speed;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    // Расчет вертикального времени полета РУС до высоты тарелки
    double vertical_distance = 570 - y;
    if (vertical_distance < 0) vertical_distance = 0;
    double rus_vertical_time = vertical_distance / 250.0;  // 250 м/с - скорость РУС
    
    // Расчет времени до достижения тарелкой центра
    double distance_to_center = 380.0;
    double time_to_center = (speed > 0) ? (distance_to_center / (double)speed) : 1e9;

    // Определение стратегии: навстречу (1) или вдогонку (0)
    int meet = (rus_vertical_time < time_to_center) ? 1 : 0;

    // Запуск РУС вверх
    send_rus_command(rus_num, RCMC_START);
    int vertical_delay = (int)round(rus_vertical_time * 1000000.0);
    if (vertical_delay > 0) usleep((useconds_t)vertical_delay);

    // Выбор направления горизонтального движения РУС
    if (meet) {
        // Стрельба навстречу: противоположное направление движению тарелки
        if (direction == 1) send_rus_command(rus_num, RCMC_LEFT);
        else send_rus_command(rus_num, RCMC_RIGHT);
    } else {
        // Стрельба вдогонку: то же направление что у тарелки
        if (direction == 1) send_rus_command(rus_num, RCMC_RIGHT);
        else send_rus_command(rus_num, RCMC_LEFT);
    }

    // Пауза для выполнения перехвата
    usleep((useconds_t)150000);

    // Помечаем тарелку как обработанную
    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].processed = 1;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    log_hit_by_rus(plate_index, speed, rus_num);

    // Освобождение РУС для повторного использования
    release_rus(rus_num);

    free(data);
    return NULL;
}

// Поток для стрельбы ракетой: ожидание и выстрел с задержкой
void *shoot_thread(void *arg) {
    if (!arg) return NULL;
    ShootThreadData *data = (ShootThreadData *)arg;
    int plate_index = data->plate_index;
    int shoot_delay = data->shoot_delay;

    if (shoot_delay > 0) {
        // Ожидание расчетного времени выстрела
        usleep((useconds_t)shoot_delay);
        // Команда выстрела из орудия
        putreg(RG_GUNS, GUNS_SHOOT);

        pthread_mutex_lock(&plates[plate_index].mutex);
        int speed = plates[plate_index].speed;
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);

        log_hit_by_rocket(plate_index, speed);
    } else {
        // Если задержка отрицательная - просто помечаем тарелку обработанной
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
    }

    free(data);
    return NULL;
}

// Обработка движения тарелки слева направо: расчет скорости и запуск перехвата
void *process_plate_left_to_right_thread(void *arg) {
    if (!arg) return NULL;
    int plate_index = *(int *)arg;
    free(arg);

    // Чтение временных меток локаторов LEFT-1 и LEFT-2
    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    long long t1 = plates[plate_index].loc1_time;
    long long t2 = plates[plate_index].loc2_time;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    // Расчет разницы времени между двумя локаторами
    long long early = (t1 < t2) ? t1 : t2;
    long long late = (t1 < t2) ? t2 : t1;
    long long dt = late - early;
    
    // Проверка корректности временного интервала
    if (dt <= 500 || dt >= 20000000) {
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
        return NULL;
    }

    // Расчет скорости: расстояние между локаторами / время
    double distance_between_locators = 10.0;
    double speed = (distance_between_locators * 1000000.0) / (double)dt;

    // Сохранение вычисленной скорости и направления
    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].speed = (int)round(speed);
    plates[plate_index].direction = 1;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    log_detection(plate_index, speed);

    // Выбор типа оружия в зависимости от скорости
    int use_rus = (speed >= SPEED_THRESHOLD) ? 1 : 0;

    int shoot_delay = 0;
    double distance_to_center = 380.0;
    double time_to_center = (speed > 0) ? (distance_to_center / speed) : 1e9;

    if (use_rus) {
        // Использование РУС для быстрых целей
        int rus_num = get_available_rus();
        // Ожидание освобождения РУС если все заняты
        while (rus_num == -1) {
            cleanup_old_plates();
            usleep(50000);
            rus_num = get_available_rus();
        }

        ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
        if (!sd) { release_rus(rus_num); return NULL; }
        sd->plate_index = plate_index;
        sd->use_rus = 1;
        sd->shoot_delay = 0;
        sd->rus_num = rus_num;

        // Запуск потока стрельбы РУС
        pthread_t t;
        if (pthread_create(&t, NULL, rus_shoot_thread, sd) != 0) {
            release_rus(rus_num);
            free(sd);
            return NULL;
        }
        pthread_detach(t);
        return NULL;
    } else {
        // Использование ракет для медленных целей
        int rocket_distance = 570 - y;
        if (rocket_distance < 0) rocket_distance = 0;
        double rocket_time = (double)rocket_distance / 100.0;  // 100 м/с - скорость ракеты
        // Расчет задержки выстрела для перехвата в центре
        double sd_sec = time_to_center - rocket_time;
        shoot_delay = (int)round(sd_sec * 1000000.0);

        if (shoot_delay > 0) {
            ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
            if (!sd) return NULL;
            sd->plate_index = plate_index;
            sd->use_rus = 0;
            sd->shoot_delay = shoot_delay;
            sd->rus_num = -1;
            // Запуск потока стрельбы ракетой
            pthread_t t;
            if (pthread_create(&t, NULL, shoot_thread, sd) != 0) {
                free(sd);
                return NULL;
            }
            pthread_detach(t);
        } else {
            // Если время перехвата прошло - помечаем тарелку обработанной
            pthread_mutex_lock(&plates[plate_index].mutex);
            plates[plate_index].processed = 1;
            pthread_mutex_unlock(&plates[plate_index].mutex);
        }
        return NULL;
    }
}

// Обработка движения тарелки справа налево (аналогично left_to_right)
void *process_plate_right_to_left_thread(void *arg) {
    if (!arg) return NULL;
    int plate_index = *(int *)arg;
    free(arg);

    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    long long t3 = plates[plate_index].loc3_time;
    long long t4 = plates[plate_index].loc4_time;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    long long early = (t3 < t4) ? t3 : t4;
    long long late = (t3 < t4) ? t4 : t3;
    long long dt = late - early;
    if (dt <= 500 || dt >= 20000000) {
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
        return NULL;
    }

    double distance_between_locators = 10.0;
    double speed = (distance_between_locators * 1000000.0) / (double)dt;

    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].speed = (int)round(speed);
    plates[plate_index].direction = -1;
    pthread_mutex_unlock(&plates[plate_index].mutex);

    log_detection(plate_index, speed);

    int use_rus = (speed >= SPEED_THRESHOLD) ? 1 : 0;

    double distance_to_center = 380.0;
    double time_to_center = (speed > 0) ? (distance_to_center / speed) : 1e9;
    int shoot_delay = 0;

    if (use_rus) {
        int rus_num = get_available_rus();
        while (rus_num == -1) {
            cleanup_old_plates();
            usleep(50000);
            rus_num = get_available_rus();
        }

        ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
        if (!sd) { release_rus(rus_num); return NULL; }
        sd->plate_index = plate_index;
        sd->use_rus = 1;
        sd->shoot_delay = 0;
        sd->rus_num = rus_num;

        pthread_t t;
        if (pthread_create(&t, NULL, rus_shoot_thread, sd) != 0) {
            release_rus(rus_num);
            free(sd);
            return NULL;
        }
        pthread_detach(t);
        return NULL;
    } else {
        int rocket_distance = 570 - y;
        if (rocket_distance < 0) rocket_distance = 0;
        double rocket_time = (double)rocket_distance / 100.0;
        double sd_sec = time_to_center - rocket_time;
        shoot_delay = (int)round(sd_sec * 1000000.0);

        if (shoot_delay > 0) {
            ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
            if (!sd) return NULL;
            sd->plate_index = plate_index;
            sd->use_rus = 0;
            sd->shoot_delay = shoot_delay;
            sd->rus_num = -1;
            pthread_t t;
            if (pthread_create(&t, NULL, shoot_thread, sd) != 0) {
                free(sd);
                return NULL;
            }
            pthread_detach(t);
        } else {
            pthread_mutex_lock(&plates[plate_index].mutex);
            plates[plate_index].processed = 1;
            pthread_mutex_unlock(&plates[plate_index].mutex);
        }
        return NULL;
    }
}

// Запуск потока обработки движения слева направо
void start_left_to_right_processing(int plate_index) {
    int *p = (int *)malloc(sizeof(int));
    if (!p) return;
    *p = plate_index;
    pthread_t t;
    if (pthread_create(&t, NULL, process_plate_left_to_right_thread, p) != 0) {
        free(p);
        return;
    }
    pthread_detach(t);  // Отсоединяем поток для автоочистки ресурсов
}

// Запуск потока обработки движения справа налево
void start_right_to_left_processing(int plate_index) {
    int *p = (int *)malloc(sizeof(int));
    if (!p) return;
    *p = plate_index;
    pthread_t t;
    if (pthread_create(&t, NULL, process_plate_right_to_left_thread, p) != 0) {
        free(p);
        return;
    }
    pthread_detach(t);
}

// Обработчик прерываний от локаторов - вызывается при обнаружении цели
const struct sigevent *locator_handler(void *area, int id) {
    (void)area; (void)id;

    // Проверка достижения максимального количества уничтоженных тарелок
    if (destroyed_plates >= MAX_DESTROYED) return NULL;

    // Чтение данных из аппаратных регистров
    int loc_num = getreg(RG_LOCN);  // Номер локатора
    int y = getreg(RG_LOCY);        // Высота обнаружения
    int w = getreg(RG_LOCW);        // Ширина цели

    // Проверка что цель - тарелка (ширина = 3)
    if (w != 3) return NULL;

    long long current_time = get_time_us();

    // Поиск или создание записи о тарелке
    int plate_index = find_or_create_plate(y);
    if (plate_index == -1) {
        // Попытка очистки и повторного поиска при нехватке слотов
        cleanup_old_plates();
        plate_index = find_or_create_plate(y);
        if (plate_index == -1) return NULL;
    }

    // Обработка события в зависимости от номера локатора
    pthread_mutex_lock(&plates[plate_index].mutex);
    switch (loc_num) {
        case 1:
            // Проверка согласованности направления (не должно быть правых меток)
            if (plates[plate_index].loc3_time != 0 || plates[plate_index].loc4_time != 0) { 
                pthread_mutex_unlock(&plates[plate_index].mutex); 
                return NULL; 
            }
            plates[plate_index].loc1_time = current_time;
            plates[plate_index].direction = 1;
            // Запуск обработки если есть данные от второго левого локатора
            if (plates[plate_index].loc2_time != 0) start_left_to_right_processing(plate_index);
            break;
        case 2:
            if (plates[plate_index].loc3_time != 0 || plates[plate_index].loc4_time != 0) { 
                pthread_mutex_unlock(&plates[plate_index].mutex); 
                return NULL; 
            }
            plates[plate_index].loc2_time = current_time;
            plates[plate_index].direction = 1;
            if (plates[plate_index].loc1_time != 0) start_left_to_right_processing(plate_index);
            break;
        case 3:
            // Проверка согласованности направления (не должно быть левых меток)
            if (plates[plate_index].loc1_time != 0 || plates[plate_index].loc2_time != 0) { 
                pthread_mutex_unlock(&plates[plate_index].mutex); 
                return NULL; 
            }
            plates[plate_index].loc3_time = current_time;
            plates[plate_index].direction = -1;
            // Запуск обработки если есть данные от второго правого локатора
            if (plates[plate_index].loc4_time != 0) start_right_to_left_processing(plate_index);
            break;
        case 4:
            if (plates[plate_index].loc1_time != 0 || plates[plate_index].loc2_time != 0) { 
                pthread_mutex_unlock(&plates[plate_index].mutex); 
                return NULL; 
            }
            plates[plate_index].loc4_time = current_time;
            plates[plate_index].direction = -1;
            if (plates[plate_index].loc3_time != 0) start_right_to_left_processing(plate_index);
            break;
        default:
            break;
    }
    pthread_mutex_unlock(&plates[plate_index].mutex);
    return NULL;
}

// Основная функция системы ПВО - инициализация и главный цикл
void air_defense_system(void) {
    // Инициализация массива тарелок
    pthread_mutex_lock(&plates_mutex);
    plate_count = 0;
    pthread_mutex_unlock(&plates_mutex);
    for (int i = 0; i < PLATE_SLOTS; ++i) {
        plates[i].height = 0;
        plates[i].speed = 0;
        plates[i].direction = 0;
        plates[i].loc1_time = plates[i].loc2_time = plates[i].loc3_time = plates[i].loc4_time = 0;
        plates[i].processed = 1;  // Все слоты изначально свободны
        pthread_mutex_init(&plates[i].mutex, NULL);
    }

    init_rus_array();

    // Привязка обработчика прерываний к локаторам
    interrupt_id = InterruptAttach(LOC_INTR, locator_handler, NULL, 0, 0);
    if (interrupt_id < 0) {
        fprintf(stderr, "Warning: InterruptAttach returned %d\n", interrupt_id);
    }

    time_t start_time = time(NULL);
    int cleanup_counter = 0;

    // Главный цикл работы системы
    while (destroyed_plates < MAX_DESTROYED && program_running) {
        sleep(1);
        cleanup_counter++;
        // Периодическая очистка устаревших записей каждые 2 секунды
        if (cleanup_counter >= 2) {
            cleanup_old_plates();
            cleanup_counter = 0;
        }
        // Автоматическое завершение через 5 минут
        if (time(NULL) - start_time > 300) break;
    }

    // Отсоединение обработчика прерываний при завершении
    if (interrupt_id >= 0) InterruptDetach(interrupt_id);
}

// Главная функция программы - запуск игры и системы ПВО
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    StartGame(2);           // Запуск игрового окружения
    air_defense_system();   // Запуск основной логики ПВО
    sleep(2);               // Пауза перед завершением
    EndGame();              // Завершение игры
    return 0;
}