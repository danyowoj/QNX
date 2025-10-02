#include "vingraph.h"
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <queue>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

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

// Сетевые настройки
const char *STORAGE_SERVER = "localhost";
const int STORAGE_PORT = 8080;
int storage_socket = -1;

// Общие данные
BoilerState boiler_states[4] = {WAITING_FOR_FUEL, WAITING_FOR_FUEL, WAITING_FOR_FUEL, WAITING_FOR_FUEL};
int boiler_fuel_level[4] = {0, 0, 0, 0};
int boiler_fuel_marks[4] = {0, 0, 0, 0};
bool boiler_targeted[4] = {false, false, false, false};
bool boiler_low_fuel[4] = {false, false, false, false};

// Данные для первого грузовика
VehicleState vehicle1_state = MOVING_TO_STORAGE;
int vehicle1_fuel = 0;
int vehicle1_target_boiler = -1;
int vehicle1_x = 100, vehicle1_y = 235;

// Данные для второго грузовика
VehicleState vehicle2_state = MOVING_TO_STORAGE;
int vehicle2_fuel = 0;
int vehicle2_target_boiler = -1;
int vehicle2_x = 100, vehicle2_y = 165;

// Размеры объектов
const int STORAGE_W = 80, STORAGE_H = 160;
const int VEHICLE_W = 80, VEHICLE_H = 40;
const int BOILER_W = 80, BOILER_H = 160;

// Расположение объектов
const int STORAGE_X = 50, STORAGE_Y = 300;
const int BOILER_Y = 300;
const int BOILER_X[4] = {
    STORAGE_X + STORAGE_W + BOILER_W,
    STORAGE_X + STORAGE_W + BOILER_W * 2,
    STORAGE_X + STORAGE_W + BOILER_W * 3,
    STORAGE_X + STORAGE_W + BOILER_W * 4};

// Позиции остановок
const int STORAGE_STOP_X = STORAGE_X;
const int STORAGE_STOP_Y1 = 235;
const int STORAGE_STOP_Y2 = 165;
const int BOILER_STOP_Y1 = 235;
const int BOILER_STOP_Y2 = 165;

// Идентификаторы графических элементов
int storage_id, vehicle1_id, vehicle2_id, boiler_ids[4], text_ids[15], fuel_bar_ids[4];

// Функции для работы с сетью
bool ConnectToStorageServer()
{
    storage_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (storage_socket < 0)
    {
        perror("socket");
        return false;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(STORAGE_PORT);

    struct hostent *server = gethostbyname(STORAGE_SERVER);
    if (server == NULL)
    {
        fprintf(stderr, "Error: no such host\n");
        return false;
    }

    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(storage_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        return false;
    }

    printf("Connected to storage server at %s:%d\n", STORAGE_SERVER, STORAGE_PORT);
    return true;
}

int RequestFuelFromStorage()
{
    if (storage_socket < 0)
        return -1;

    const char *request = "POP";
    if (write(storage_socket, request, strlen(request)) < 0)
    {
        perror("write");
        return -1;
    }

    char buffer[32];
    int n = read(storage_socket, buffer, sizeof(buffer) - 1);
    if (n > 0)
    {
        buffer[n] = '\0';
        return atoi(buffer);
    }

    return -1;
}

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

// Функция для плавного перемещения грузовика
void MoveVehicleTo(int *vehicle_x, int *vehicle_y, int target_x, int target_y, int vehicle_id)
{
    int steps = 20;
    int start_x = *vehicle_x;
    int start_y = *vehicle_y;

    for (int i = 0; i <= steps && run_flag; i++)
    {
        float progress = (float)i / steps;
        int new_x = start_x + (target_x - start_x) * progress;
        int new_y = start_y + (target_y - start_y) * progress;

        MoveTo(new_x, new_y, vehicle_id);
        usleep(50000);
    }

    *vehicle_x = target_x;
    *vehicle_y = target_y;
}

// Функция для обновления индикатора топлива в котле
void UpdateBoilerFuelIndicator(int boiler_id)
{
    if (fuel_bar_ids[boiler_id] != 0)
    {
        Delete(fuel_bar_ids[boiler_id]);
        fuel_bar_ids[boiler_id] = 0;
    }

    if (boiler_fuel_level[boiler_id] > 0)
    {
        int max_fuel_height = BOILER_H - 20;
        int fuel_height = (boiler_fuel_level[boiler_id] * max_fuel_height) / 20;
        int fuel_y = BOILER_Y + BOILER_H - fuel_height - 10;

        int color;
        if (boiler_states[boiler_id] == BURNING)
        {
            if (boiler_low_fuel[boiler_id])
                color = RGB(255, 0, 0);
            else
                color = RGB(255, 165, 0);
        }
        else
        {
            color = RGB(0, 200, 0);
        }

        fuel_bar_ids[boiler_id] = Rect(
            BOILER_X[boiler_id] + 10,
            fuel_y,
            BOILER_W - 20,
            fuel_height,
            0,
            color);
    }
}

// Функция для выбора доступного котла
int SelectAvailableBoiler()
{
    for (int i = 0; i < 4; i++)
    {
        if (boiler_low_fuel[i] && !boiler_targeted[i])
        {
            boiler_targeted[i] = true;
            return i;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        if (boiler_states[i] == WAITING_FOR_FUEL && !boiler_targeted[i])
        {
            boiler_targeted[i] = true;
            return i;
        }
    }

    return -1;
}

// Функция для обновления текстовой информации
void UpdateTextInfo()
{
    for (int i = 0; i < 15; i++)
    {
        if (text_ids[i] != 0)
        {
            Delete(text_ids[i]);
            text_ids[i] = 0;
        }
    }

    int y_offset = 10;

    char vehicle1_fuel_text[50];
    sprintf(vehicle1_fuel_text, "Truck1 Fuel: %d", vehicle1_fuel);
    text_ids[0] = Text(10, y_offset, vehicle1_fuel_text, RGB(255, 255, 255));

    char vehicle1_target_text[50];
    if (vehicle1_target_boiler != -1)
    {
        sprintf(vehicle1_target_text, "Truck1 Target: %d", vehicle1_target_boiler + 1);
    }
    else
    {
        sprintf(vehicle1_target_text, "Truck1 Target: None");
    }
    text_ids[1] = Text(10, y_offset + 20, vehicle1_target_text, RGB(255, 255, 255));

    char vehicle2_fuel_text[50];
    sprintf(vehicle2_fuel_text, "Truck2 Fuel: %d", vehicle2_fuel);
    text_ids[2] = Text(10, y_offset + 40, vehicle2_fuel_text, RGB(255, 255, 255));

    char vehicle2_target_text[50];
    if (vehicle2_target_boiler != -1)
    {
        sprintf(vehicle2_target_text, "Truck2 Target: %d", vehicle2_target_boiler + 1);
    }
    else
    {
        sprintf(vehicle2_target_text, "Truck2 Target: None");
    }
    text_ids[3] = Text(10, y_offset + 60, vehicle2_target_text, RGB(255, 255, 255));

    for (int i = 0; i < 4; i++)
    {
        char boiler_mark_text[50];
        sprintf(boiler_mark_text, "Boiler %d: Mark %d, %s", i + 1, boiler_fuel_marks[i],
                boiler_low_fuel[i] ? "LOW FUEL!" : (boiler_targeted[i] ? "Targeted" : "Free"));
        text_ids[7 + i] = Text(375, y_offset + i * 20, boiler_mark_text,
                               boiler_low_fuel[i] ? RGB(255, 0, 0) : RGB(255, 255, 255));
    }
}

// Функция для отрисовки состояния
void DrawState()
{
    pthread_mutex_lock(&mutex);

    const char *v1state = "";
    switch (vehicle1_state)
    {
    case MOVING_TO_STORAGE:
        v1state = "To Storage";
        break;
    case LOADING:
        v1state = "Loading";
        break;
    case MOVING_TO_BOILER:
        v1state = "To Boiler";
        break;
    case UNLOADING:
        v1state = "Unloading";
        break;
    }
    SetText(vehicle1_id, v1state);

    const char *v2state = "";
    switch (vehicle2_state)
    {
    case MOVING_TO_STORAGE:
        v2state = "To Storage";
        break;
    case LOADING:
        v2state = "Loading";
        break;
    case MOVING_TO_BOILER:
        v2state = "To Boiler";
        break;
    case UNLOADING:
        v2state = "Unloading";
        break;
    }
    SetText(vehicle2_id, v2state);

    for (int i = 0; i < 4; i++)
    {
        char boiler_text[100];
        const char *bstate = boiler_states[i] == BURNING ? "Burning" : "Waiting";
        sprintf(boiler_text, "Boiler %d: %s", i + 1, bstate);
        SetText(boiler_ids[i], boiler_text);

        if (boiler_states[i] == BURNING)
        {
            if (boiler_low_fuel[i])
                SetColor(boiler_ids[i], RGB(255, 0, 0));
            else
                SetColor(boiler_ids[i], RGB(255, 50, 50));
        }
        else
        {
            SetColor(boiler_ids[i], RGB(200, 100, 100));
        }
    }

    UpdateTextInfo();
    pthread_mutex_unlock(&mutex);
}

// Поток для первого грузовика
void *Vehicle1Thread(void *arg)
{
    while (run_flag)
    {
        if (vehicle1_state == MOVING_TO_STORAGE)
        {
            MoveVehicleTo(&vehicle1_x, &vehicle1_y, STORAGE_STOP_X, STORAGE_STOP_Y1, vehicle1_id);

            if (!run_flag)
                break;

            pthread_mutex_lock(&mutex);
            vehicle1_state = LOADING;
            pthread_mutex_unlock(&mutex);

            for (int i = 0; i < 3 && run_flag; i++)
            {
                usleep(300000);
                DrawState();
            }

            pthread_mutex_lock(&mutex);
            int fuel = RequestFuelFromStorage();
            if (fuel > 0)
            {
                vehicle1_fuel = fuel;
                vehicle1_target_boiler = SelectAvailableBoiler();

                if (vehicle1_target_boiler != -1)
                {
                    vehicle1_state = MOVING_TO_BOILER;
                }
                else
                {
                    vehicle1_state = MOVING_TO_STORAGE;
                }
                UpdateTextInfo();
            }
            else
            {
                vehicle1_state = MOVING_TO_STORAGE;
            }
            pthread_mutex_unlock(&mutex);
        }

        if (vehicle1_state == MOVING_TO_BOILER && vehicle1_target_boiler != -1 && run_flag)
        {
            int target_x = BOILER_X[vehicle1_target_boiler];
            MoveVehicleTo(&vehicle1_x, &vehicle1_y, target_x, BOILER_STOP_Y1, vehicle1_id);

            if (!run_flag)
                break;

            pthread_mutex_lock(&mutex);
            vehicle1_state = UNLOADING;
            pthread_mutex_unlock(&mutex);

            for (int i = 0; i < 3 && run_flag; i++)
            {
                usleep(300000);
                DrawState();
            }

            pthread_mutex_lock(&mutex);
            if (vehicle1_target_boiler != -1 && vehicle1_fuel > 0)
            {
                boiler_states[vehicle1_target_boiler] = BURNING;
                boiler_fuel_level[vehicle1_target_boiler] = vehicle1_fuel;
                boiler_fuel_marks[vehicle1_target_boiler] = vehicle1_fuel;
                boiler_low_fuel[vehicle1_target_boiler] = false;
                boiler_targeted[vehicle1_target_boiler] = false;
                UpdateBoilerFuelIndicator(vehicle1_target_boiler);

                vehicle1_fuel = 0;
                vehicle1_target_boiler = -1;
                UpdateTextInfo();
            }

            DrawState();
            vehicle1_state = MOVING_TO_STORAGE;
            pthread_mutex_unlock(&mutex);
        }

        usleep(100000);
    }
    return NULL;
}

// Поток для второго грузовика
void *Vehicle2Thread(void *arg)
{
    while (run_flag)
    {
        if (vehicle2_state == MOVING_TO_STORAGE)
        {
            MoveVehicleTo(&vehicle2_x, &vehicle2_y, STORAGE_STOP_X, STORAGE_STOP_Y2, vehicle2_id);

            if (!run_flag)
                break;

            pthread_mutex_lock(&mutex);
            vehicle2_state = LOADING;
            pthread_mutex_unlock(&mutex);

            for (int i = 0; i < 3 && run_flag; i++)
            {
                usleep(300000);
                DrawState();
            }

            pthread_mutex_lock(&mutex);
            int fuel = RequestFuelFromStorage();
            if (fuel > 0)
            {
                vehicle2_fuel = fuel;
                vehicle2_target_boiler = SelectAvailableBoiler();

                if (vehicle2_target_boiler != -1)
                {
                    vehicle2_state = MOVING_TO_BOILER;
                }
                else
                {
                    vehicle2_state = MOVING_TO_STORAGE;
                }
                UpdateTextInfo();
            }
            else
            {
                vehicle2_state = MOVING_TO_STORAGE;
            }
            pthread_mutex_unlock(&mutex);
        }

        if (vehicle2_state == MOVING_TO_BOILER && vehicle2_target_boiler != -1 && run_flag)
        {
            int target_x = BOILER_X[vehicle2_target_boiler];
            MoveVehicleTo(&vehicle2_x, &vehicle2_y, target_x, BOILER_STOP_Y2, vehicle2_id);

            if (!run_flag)
                break;

            pthread_mutex_lock(&mutex);
            vehicle2_state = UNLOADING;
            pthread_mutex_unlock(&mutex);

            for (int i = 0; i < 3 && run_flag; i++)
            {
                usleep(300000);
                DrawState();
            }

            pthread_mutex_lock(&mutex);
            if (vehicle2_target_boiler != -1 && vehicle2_fuel > 0)
            {
                boiler_states[vehicle2_target_boiler] = BURNING;
                boiler_fuel_level[vehicle2_target_boiler] = vehicle2_fuel;
                boiler_fuel_marks[vehicle2_target_boiler] = vehicle2_fuel;
                boiler_low_fuel[vehicle2_target_boiler] = false;
                boiler_targeted[vehicle2_target_boiler] = false;
                UpdateBoilerFuelIndicator(vehicle2_target_boiler);

                vehicle2_fuel = 0;
                vehicle2_target_boiler = -1;
                UpdateTextInfo();
            }

            DrawState();
            vehicle2_state = MOVING_TO_STORAGE;
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

                if (boiler_fuel_level[id] <= 2)
                {
                    boiler_low_fuel[id] = true;
                }
                UpdateBoilerFuelIndicator(id);
            }
            else
            {
                boiler_states[id] = WAITING_FOR_FUEL;
                boiler_fuel_marks[id] = 0;
                boiler_low_fuel[id] = false;

                if (fuel_bar_ids[id] != 0)
                {
                    Delete(fuel_bar_ids[id]);
                    fuel_bar_ids[id] = 0;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        usleep(1000000);
    }
    return NULL;
}

int main()
{
    // Подключение к серверу хранилища
    if (!ConnectToStorageServer())
    {
        fprintf(stderr, "Failed to connect to storage server\n");
        return 1;
    }

    ConnectGraph("Power Station Simulation - Boiler Server");

    // Инициализация случайного генератора
    srand(time(NULL));

    // Инициализация ID
    for (int i = 0; i < 15; i++)
    {
        text_ids[i] = 0;
    }
    for (int i = 0; i < 4; i++)
    {
        fuel_bar_ids[i] = 0;
    }

    // Создание графических элементов
    storage_id = Rect(STORAGE_X, STORAGE_Y, STORAGE_W, STORAGE_H, 5, RGB(200, 200, 100));
    SetText(storage_id, "Remote Storage");
    SetColor(storage_id, RGB(255, 255, 255));

    vehicle1_id = Rect(vehicle1_x, vehicle1_y, VEHICLE_W, VEHICLE_H, 5, RGB(0, 0, 255));
    SetText(vehicle1_id, "Truck1");

    vehicle2_id = Rect(vehicle2_x, vehicle2_y, VEHICLE_W, VEHICLE_H, 5, RGB(0, 128, 0));
    SetText(vehicle2_id, "Truck2");

    for (int i = 0; i < 4; i++)
    {
        boiler_ids[i] = Rect(BOILER_X[i], BOILER_Y, BOILER_W, BOILER_H, 5, RGB(200, 100, 100));
        char boiler_name[20];
        sprintf(boiler_name, "Boiler %d", i + 1);
        SetText(boiler_ids[i], boiler_name);
    }

    Text(10, 120, "Boiler Server - Press 'q' to quit", RGB(255, 255, 255));

    // Дороги
    Line(50, 285, 530, 285, RGB(255, 255, 255));
    Line(50, 290, 530, 290, RGB(255, 255, 255));
    Line(50, 215, 530, 215, RGB(255, 255, 255));
    Line(50, 220, 530, 220, RGB(255, 255, 255));

    // Запуск потоков
    pthread_t vehicle1_thread, vehicle2_thread, boiler_threads[4];
    int boiler_ids_arg[4] = {0, 1, 2, 3};

    pthread_create(&vehicle1_thread, NULL, Vehicle1Thread, NULL);
    pthread_create(&vehicle2_thread, NULL, Vehicle2Thread, NULL);

    for (int i = 0; i < 4; i++)
    {
        pthread_create(&boiler_threads[i], NULL, BoilerThread, &boiler_ids_arg[i]);
    }

    // Установка неблокирующего режима ввода
    set_raw_mode(1);
    printf("Boiler server started. Press 'q' to quit\n");

    // Главный цикл визуализации
    char c;
    while (run_flag)
    {
        DrawState();
        usleep(50000);

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
    pthread_join(vehicle1_thread, NULL);
    pthread_join(vehicle2_thread, NULL);
    for (int i = 0; i < 4; i++)
    {
        pthread_join(boiler_threads[i], NULL);
    }

    if (storage_socket >= 0)
    {
        close(storage_socket);
    }

    CloseGraph();
    return 0;
}
