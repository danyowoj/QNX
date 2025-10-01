#include "vingraph.h"
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <queue>
#include <termios.h>

// Состояния элементов
enum VehicleState
{
    MOVING_TO_STORAGE,
    LOADING,
    MOVING_TO_BOILER,
    UNLOADING
};
enum BoilerState
{
    WAITING_FOR_FUEL,
    BURNING
};

// Глобальные структуры для синхронизации
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int run_flag = 1;

// Общие данные
std::queue<int> fuel_storage;
VehicleState vehicle_state = MOVING_TO_STORAGE;
BoilerState boiler_states[4] = {WAITING_FOR_FUEL, WAITING_FOR_FUEL, WAITING_FOR_FUEL, WAITING_FOR_FUEL};
int vehicle_fuel = 0;
int vehicle_target_boiler = -1;
int boiler_fuel_level[4] = {0, 0, 0, 0};
int boiler_fuel_marks[4] = {0, 0, 0, 0};

// Позиции для анимации
int vehicle_x = 100, vehicle_y = 235;

// Размеры объектов (высота:ширина = 2:1)
const int STORAGE_W = 80, STORAGE_H = 160;
const int VEHICLE_W = 80, VEHICLE_H = 40;
const int BOILER_W = 80, BOILER_H = 160;

// Расположение объектов (расстояние между ними равно ширине)
const int STORAGE_X = 50, STORAGE_Y = 300;
const int BOILER_Y = 300;
const int BOILER_X[4] = {
    STORAGE_X + STORAGE_W + BOILER_W,     // Первый котел
    STORAGE_X + STORAGE_W + BOILER_W * 2, // Второй котел
    STORAGE_X + STORAGE_W + BOILER_W * 3, // Третий котел
    STORAGE_X + STORAGE_W + BOILER_W * 4  // Четвертый котел
};

// Позиции остановок (грузовик движется выше)
const int STORAGE_STOP_X = STORAGE_X;
const int STORAGE_STOP_Y = 235;
const int BOILER_STOP_Y = 235;

// Идентификаторы графических элементов
int storage_id, vehicle_id, boiler_ids[4], text_ids[10], fuel_bar_ids[4];

// Функция для неблокирующего ввода
static void set_raw_mode(int enable)
{
    static struct termios oldt;
    struct termios newt;
    if (enable)
    {
        tcgetattr(0, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(0, TCSANOW, &newt);
    }
    else
    {
        tcsetattr(0, TCSANOW, &oldt);
    }
}

// Функция для плавного перемещения грузовика к целевой позиции
void MoveVehicleTo(int target_x, int target_y)
{
    int steps = 20;
    int start_x = vehicle_x;
    int start_y = vehicle_y;

    for (int i = 0; i <= steps && run_flag; i++)
    {
        float progress = (float)i / steps;
        int new_x = start_x + (target_x - start_x) * progress;
        int new_y = start_y + (target_y - start_y) * progress;

        MoveTo(new_x, new_y, vehicle_id);
        usleep(50000);
    }

    vehicle_x = target_x;
    vehicle_y = target_y;
}

// Функция для обновления цвета грузовика в зависимости от загрузки
void UpdateVehicleColor()
{
    if (vehicle_fuel > 0)
    {
        // Загруженный грузовик - зеленый
        SetColor(vehicle_id, RGB(0, 200, 0));
    }
    else
    {
        // Пустой грузовик - синий
        SetColor(vehicle_id, RGB(100, 100, 200));
    }
}

// Функция для обновления индикатора топлива в котле
void UpdateBoilerFuelIndicator(int boiler_id)
{
    // Удаляем старый индикатор
    if (fuel_bar_ids[boiler_id] != 0)
    {
        Delete(fuel_bar_ids[boiler_id]);
        fuel_bar_ids[boiler_id] = 0;
    }

    // Создаем новый индикатор, если есть топливо
    if (boiler_fuel_level[boiler_id] > 0)
    {
        int max_fuel_height = BOILER_H - 20; // Оставляем отступы
        int fuel_height = (boiler_fuel_level[boiler_id] * max_fuel_height) / 20;
        int fuel_y = BOILER_Y + BOILER_H - fuel_height - 10; // Отступ снизу

        fuel_bar_ids[boiler_id] = Rect(
            BOILER_X[boiler_id] + 10,
            fuel_y,
            BOILER_W - 20,
            fuel_height,
            0,
            boiler_states[boiler_id] == BURNING ? RGB(255, 165, 0) : RGB(0, 200, 0));
    }
}

// Функция для обновления текстовой информации
void UpdateTextInfo()
{
    // Очистка предыдущих текстовых элементов
    for (int i = 0; i < 10; i++)
    {
        if (text_ids[i] != 0)
        {
            Delete(text_ids[i]);
            text_ids[i] = 0;
        }
    }

    // Вывод общей информации В ВЕРХУ ОКНА
    int y_offset = 10;

    char vehicle_fuel_text[50];
    sprintf(vehicle_fuel_text, "Vehicle Fuel: %d", vehicle_fuel);
    text_ids[0] = Text(10, y_offset, vehicle_fuel_text, RGB(255, 255, 255));

    char target_boiler_text[50];
    if (vehicle_target_boiler != -1)
    {
        sprintf(target_boiler_text, "Target Boiler: %d", vehicle_target_boiler + 1);
    }
    else
    {
        sprintf(target_boiler_text, "Target Boiler: None");
    }
    text_ids[1] = Text(10, y_offset + 20, target_boiler_text, RGB(255, 255, 255));

    char storage_count_text[50];
    sprintf(storage_count_text, "Storage: %d units", (int)fuel_storage.size());
    text_ids[2] = Text(10, y_offset + 40, storage_count_text, RGB(255, 255, 255));

    char vehicle_state_text[50];
    sprintf(vehicle_state_text, "Vehicle State: %s",
            vehicle_state == MOVING_TO_STORAGE ? "To Storage" : vehicle_state == LOADING        ? "Loading"
                                                            : vehicle_state == MOVING_TO_BOILER ? "To Boiler"
                                                                                                : "Unloading");
    text_ids[3] = Text(200, y_offset, vehicle_state_text, RGB(255, 255, 255));

    for (int i = 0; i < 4; i++)
    {
        char boiler_mark_text[50];
        sprintf(boiler_mark_text, "Boiler %d Mark: %d", i + 1, boiler_fuel_marks[i]);
        text_ids[4 + i] = Text(200, y_offset + 20 + i * 20, boiler_mark_text, RGB(255, 255, 255));
    }
}

// Функция для отрисовки состояния
void DrawState()
{
    pthread_mutex_lock(&mutex);

    // Обновление цвета грузовика
    UpdateVehicleColor();

    // Обновление хранилища
    char storage_text[50];
    sprintf(storage_text, "Storage: %d units", (int)fuel_storage.size());
    SetText(storage_id, storage_text);
    SetColor(storage_id, RGB(255, 255, 255));

    // Обновление состояния транспорта
    const char *vstate = "";
    switch (vehicle_state)
    {
    case MOVING_TO_STORAGE:
        vstate = "To Storage";
        break;
    case LOADING:
        vstate = "Loading";
        break;
    case MOVING_TO_BOILER:
        vstate = "To Boiler";
        break;
    case UNLOADING:
        vstate = "Unloading";
        break;
    }
    SetText(vehicle_id, vstate);
    SetColor(vehicle_id, RGB(255, 255, 255));

    // Обновление котлов
    for (int i = 0; i < 4; i++)
    {
        char boiler_text[100];
        const char *bstate = boiler_states[i] == BURNING ? "Burning" : "Waiting";
        sprintf(boiler_text, "Boiler %d: %s", i + 1, bstate);
        SetText(boiler_ids[i], boiler_text);
        SetColor(boiler_ids[i], RGB(255, 255, 255));

        // Изменение цвета котла при горении
        if (boiler_states[i] == BURNING)
        {
            SetColor(boiler_ids[i], RGB(255, 50, 50));
        }
        else
        {
            SetColor(boiler_ids[i], RGB(200, 100, 100));
        }
    }

    // Обновление текстовой информации
    UpdateTextInfo();

    pthread_mutex_unlock(&mutex);
}

// Поток для хранилища
void *StorageThread(void *arg)
{
    while (run_flag)
    {
        pthread_mutex_lock(&mutex);
        if (fuel_storage.size() < 20)
        {
            int mark = rand() % 10 + 1;
            fuel_storage.push(mark);
        }
        pthread_mutex_unlock(&mutex);
        usleep(1000000);
    }
    return NULL;
}

// Поток для транспорта
void *VehicleThread(void *arg)
{
    while (run_flag)
    {
        // Загрузка топлива из хранилища
        if (vehicle_state == MOVING_TO_STORAGE)
        {
            // Движение к хранилищу
            MoveVehicleTo(STORAGE_STOP_X, STORAGE_STOP_Y);

            if (!run_flag)
                break;

            // Начинаем загрузку
            pthread_mutex_lock(&mutex);
            vehicle_state = LOADING;
            pthread_mutex_unlock(&mutex);

            // Анимация загрузки
            for (int i = 0; i < 3 && run_flag; i++)
            {
                usleep(300000);
                DrawState();
            }

            pthread_mutex_lock(&mutex);
            if (!fuel_storage.empty())
            {
                vehicle_fuel = fuel_storage.front();
                fuel_storage.pop();

                // Выбор котла после загрузки топлива
                vehicle_target_boiler = -1;
                for (int i = 0; i < 4; i++)
                {
                    if (boiler_states[i] == WAITING_FOR_FUEL)
                    {
                        vehicle_target_boiler = i;
                        break;
                    }
                }
                if (vehicle_target_boiler == -1)
                {
                    vehicle_target_boiler = rand() % 4;
                }

                vehicle_state = MOVING_TO_BOILER;

                // Обновляем информацию о Vehicle Fuel и Target Boiler
                UpdateTextInfo();
            }
            else
            {
                vehicle_state = MOVING_TO_STORAGE;
            }
            pthread_mutex_unlock(&mutex);
        }

        // Разгрузка в котёл
        if (vehicle_state == MOVING_TO_BOILER && vehicle_target_boiler != -1 && run_flag)
        {
            // Движение к котлу
            int target_x = BOILER_X[vehicle_target_boiler];
            MoveVehicleTo(target_x, BOILER_STOP_Y);

            if (!run_flag)
                break;

            // Начинаем разгрузку
            pthread_mutex_lock(&mutex);
            vehicle_state = UNLOADING;
            pthread_mutex_unlock(&mutex);

            // Анимация разгрузки
            for (int i = 0; i < 3 && run_flag; i++)
            {
                usleep(300000);
                DrawState();
            }

            pthread_mutex_lock(&mutex);
            if (vehicle_target_boiler != -1 && vehicle_fuel > 0)
            {
                // Устанавливаем состояние котла и уровень топлива
                boiler_states[vehicle_target_boiler] = BURNING;
                boiler_fuel_level[vehicle_target_boiler] = vehicle_fuel * 2;
                boiler_fuel_marks[vehicle_target_boiler] = vehicle_fuel;

                // Обновляем индикатор топлива в котле
                UpdateBoilerFuelIndicator(vehicle_target_boiler);

                vehicle_fuel = 0;
                vehicle_target_boiler = -1;

                // Обновляем информацию о Vehicle Fuel и Target Boiler (обнуляем)
                UpdateTextInfo();
            }

            // Отрисовываем состояние ДО того, как грузовик начнет движение обратно
            DrawState();

            vehicle_state = MOVING_TO_STORAGE;
            pthread_mutex_unlock(&mutex);
        }

        usleep(100000);
    }
    return NULL;
}

// Поток для котлов
void *BoilerThread(void *arg)
{
    int id = *((int *)arg);
    while (run_flag)
    {
        pthread_mutex_lock(&mutex);
        if (boiler_states[id] == BURNING)
        {
            if (boiler_fuel_level[id] > 0)
            {
                boiler_fuel_level[id]--;

                // Обновляем индикатор уровня топлива
                UpdateBoilerFuelIndicator(id);
            }
            else
            {
                boiler_states[id] = WAITING_FOR_FUEL;
                boiler_fuel_marks[id] = 0;

                // Удаляем индикатор уровня топлива
                if (fuel_bar_ids[id] != 0)
                {
                    Delete(fuel_bar_ids[id]);
                    fuel_bar_ids[id] = 0;
                }
            }
        }
        pthread_mutex_unlock(&mutex);

        usleep(500000);
    }
    return NULL;
}

int main()
{
    ConnectGraph("Power Station Simulation");

    // Инициализация случайного генератора
    srand(time(NULL));

    // Инициализация хранилища
    for (int i = 0; i < 10; i++)
    {
        fuel_storage.push(rand() % 10 + 1);
    }

    // Инициализация ID
    for (int i = 0; i < 10; i++)
    {
        text_ids[i] = 0;
    }
    for (int i = 0; i < 4; i++)
    {
        fuel_bar_ids[i] = 0;
    }

    // Создание графических элементов
    storage_id = Rect(STORAGE_X, STORAGE_Y, STORAGE_W, STORAGE_H, 5, RGB(200, 200, 100));
    SetText(storage_id, "Fuel Storage");
    SetColor(storage_id, RGB(255, 255, 255));

    vehicle_id = Rect(vehicle_x, vehicle_y, VEHICLE_W, VEHICLE_H, 5, RGB(100, 100, 200));
    SetText(vehicle_id, "Truck");
    SetColor(vehicle_id, RGB(255, 255, 255));

    for (int i = 0; i < 4; i++)
    {
        boiler_ids[i] = Rect(BOILER_X[i], BOILER_Y, BOILER_W, BOILER_H, 5, RGB(200, 100, 100));
        char boiler_name[20];
        sprintf(boiler_name, "Boiler %d", i + 1);
        SetText(boiler_ids[i], boiler_name);
        SetColor(boiler_ids[i], RGB(255, 255, 255));
    }

    // Инструкция вверху окна
    Text(10, 120, "Power Station Simulation - Press 'q' to quit", RGB(255, 255, 255));

    // Дорога
    Line(50, 285, 530, 285, RGB(255, 255, 255));
    Line(50, 290, 530, 290, RGB(255, 255, 255));

    // Запуск потоков
    pthread_t storage_thread, vehicle_thread, boiler_threads[4];
    int boiler_ids_arg[4] = {0, 1, 2, 3};

    pthread_create(&storage_thread, NULL, StorageThread, NULL);
    pthread_create(&vehicle_thread, NULL, VehicleThread, NULL);

    for (int i = 0; i < 4; i++)
    {
        pthread_create(&boiler_threads[i], NULL, BoilerThread, &boiler_ids_arg[i]);
    }

    // Установка неблокирующего режима ввода
    set_raw_mode(1);
    printf("Power Station Simulation started. Press 'q' to quit\n");

    // Главный цикл визуализации
    char c;
    while (run_flag)
    {
        DrawState();
        usleep(50000);

        // Проверка ввода
        if (read(0, &c, 1) == 1)
        {
            if (c == 'q' || c == 'Q')
            {
                pthread_mutex_lock(&mutex);
                run_flag = 0;
                pthread_mutex_unlock(&mutex);
                break;
            }
        }
    }

    // Восстановление режима ввода и завершение
    set_raw_mode(0);

    // Ожидание завершения потоков
    pthread_join(storage_thread, NULL);
    pthread_join(vehicle_thread, NULL);
    for (int i = 0; i < 4; i++)
    {
        pthread_join(boiler_threads[i], NULL);
    }

    CloseGraph();
    return 0;
}
