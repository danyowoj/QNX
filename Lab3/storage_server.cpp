#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <queue>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Глобальные структуры для синхронизации
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int run_flag = 1;

// Общие данные
std::queue<int> fuel_storage;

// Функция для обработки клиентских запросов
void *HandleClient(void *arg)
{
    int client_socket = *((int *)arg);
    free(arg);

    char buffer[256];
    int n;

    while ((n = read(client_socket, buffer, sizeof(buffer) - 1)) > 0 && run_flag)
    {
        buffer[n] = '\0';

        if (strncmp(buffer, "POP", 3) == 0)
        {
            pthread_mutex_lock(&mutex);
            int fuel = -1;
            if (!fuel_storage.empty())
            {
                fuel = fuel_storage.front();
                fuel_storage.pop();
                printf("Dispensed fuel: %d, Storage size: %zu\n", fuel, fuel_storage.size());
            }
            else
            {
                printf("Storage empty, cannot dispense fuel\n");
            }
            pthread_mutex_unlock(&mutex);

            // Отправляем ответ
            char response[32];
            snprintf(response, sizeof(response), "%d", fuel);
            write(client_socket, response, strlen(response));
        }
        else if (strncmp(buffer, "SIZE", 4) == 0)
        {
            pthread_mutex_lock(&mutex);
            int size = fuel_storage.size();
            pthread_mutex_unlock(&mutex);

            char response[32];
            snprintf(response, sizeof(response), "%d", size);
            write(client_socket, response, strlen(response));
        }
    }

    close(client_socket);
    return NULL;
}

// Поток для генерации топлива
void *StorageThread(void *arg)
{
    while (run_flag)
    {
        pthread_mutex_lock(&mutex);
        if (fuel_storage.size() < 20)
        {
            int mark = rand() % 10 + 1;
            fuel_storage.push(mark);
            printf("Generated fuel: %d, Storage size: %zu\n", mark, fuel_storage.size());
        }
        pthread_mutex_unlock(&mutex);
        usleep(1000000);
    }
    return NULL;
}

int main()
{
    // Инициализация случайного генератора
    srand(time(NULL));

    // Инициализация хранилища
    for (int i = 0; i < 10; i++)
    {
        fuel_storage.push(rand() % 10 + 1);
    }
    printf("Storage initialized with %zu units\n", fuel_storage.size());

    // Создание серверного сокета
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    // Настройка адреса сервера
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Storage server listening on port 8080\n");

    // Запуск потока генерации топлива
    pthread_t storage_thread;
    pthread_create(&storage_thread, NULL, StorageThread, NULL);

    // Основной цикл сервера
    while (run_flag)
    {
        int *client_socket = (int *)malloc(sizeof(int));
        *client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

        if (*client_socket < 0)
        {
            perror("accept");
            free(client_socket);
            continue;
        }

        printf("New client connected\n");

        // Создаем поток для обработки клиента
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, HandleClient, client_socket);
        pthread_detach(client_thread);
    }

    // Очистка
    close(server_fd);
    pthread_join(storage_thread, NULL);

    printf("Storage server stopped\n");
    return 0;
}
