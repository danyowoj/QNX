## Общая архитектура системы

Система разделена на два независимых сервера, которые взаимодействуют по сети:

1. **Сервер хранилища** - отвечает за генерацию и хранение топлива
2. **Сервер котлов** - управляет транспортными средствами, котлами и визуализацией

## Детальное объяснение сервера хранилища (storage_server.cpp)

### 1. Инициализация и структура данных

```cpp
std::queue<int> fuel_storage;  // Очередь для хранения единиц топлива
pthread_mutex_t mutex;         // Мьютекс для синхронизации доступа к очереди
volatile int run_flag = 1;     // Флаг работы программы
```

Сервер использует стандартную очередь для хранения целочисленных значений, представляющих единицы топлива. Каждая единица - это число от 1 до 10.

### 2. Поток генерации топлива (StorageThread)

```cpp
void* StorageThread(void* arg) {
    while (run_flag) {
        pthread_mutex_lock(&mutex);
        if (fuel_storage.size() < 20) {
            int mark = rand() % 10 + 1;
            fuel_storage.push(mark);
            printf("Generated fuel: %d, Storage size: %zu\n", mark, fuel_storage.size());
        }
        pthread_mutex_unlock(&mutex);
        usleep(1000000);  // 1 секунда между генерацией
    }
    return NULL;
}
```

**Работа потока:**
- Проверяет, не превышен ли максимальный размер хранилища (20 единиц)
- Генерирует топливо случайной марки от 1 до 10
- Добавляет топливо в очередь
- Ждет 1 секунду перед следующей итерацией

### 3. Обработка клиентских запросов (HandleClient)

```cpp
void* HandleClient(void* arg) {
    int client_socket = *((int*)arg);
    
    char buffer[256];
    int n;
    
    while ((n = read(client_socket, buffer, sizeof(buffer)-1)) > 0 && run_flag) {
        buffer[n] = '\0';
        
        if (strncmp(buffer, "POP", 3) == 0) {
            // Обработка запроса на получение топлива
            pthread_mutex_lock(&mutex);
            int fuel = -1;
            if (!fuel_storage.empty()) {
                fuel = fuel_storage.front();
                fuel_storage.pop();
            }
            pthread_mutex_unlock(&mutex);
            
            // Отправка ответа клиенту
            char response[32];
            snprintf(response, sizeof(response), "%d", fuel);
            write(client_socket, response, strlen(response));
        }
        else if (strncmp(buffer, "SIZE", 4) == 0) {
            // Обработка запроса на получение размера хранилища
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
```

**Протокол взаимодействия:**
- **POP** - запрос на получение одной единицы топлива
  - Сервер извлекает топливо из очереди (если есть)
  - Возвращает значение топлива или -1 если очередь пуста
- **SIZE** - запрос на получение текущего размера хранилища

### 4. Сетевой интерфейс

```cpp
// Создание серверного сокета
int server_fd = socket(AF_INET, SOCK_STREAM, 0);

// Настройка адреса сервера
struct sockaddr_in address;
address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY;  // Принимать соединения с любого интерфейса
address.sin_port = htons(8080);        // Порт 8080

bind(server_fd, (struct sockaddr*)&address, sizeof(address));
listen(server_fd, 5);  // Очередь из 5 ожидающих соединений
```

**Основной цикл сервера:**
```cpp
while (run_flag) {
    int* client_socket = (int*)malloc(sizeof(int));
    *client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    
    // Для каждого клиента создается отдельный поток
    pthread_t client_thread;
    pthread_create(&client_thread, NULL, HandleClient, client_socket);
    pthread_detach(client_thread);  // Поток завершится автоматически
}
```

## Детальное объяснение сервера котлов (boiler_server.cpp)

### 1. Состояния системы

**Состояния транспортных средств:**
```cpp
enum VehicleState {
    MOVING_TO_STORAGE,  // Движение к хранилищу
    LOADING,            // Загрузка топлива
    MOVING_TO_BOILER,   // Движение к котлу
    UNLOADING           // Разгрузка в котел
};
```

**Состояния котлов:**
```cpp
enum BoilerState {
    WAITING_FOR_FUEL,  // Ожидание топлива
    BURNING            // Сжигание топлива
};
```

### 2. Сетевое подключение к хранилищу

```cpp
bool ConnectToStorageServer() {
    storage_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(STORAGE_PORT);
    
    struct hostent* server = gethostbyname(STORAGE_SERVER);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    return connect(storage_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) >= 0;
}

int RequestFuelFromStorage() {
    const char* request = "POP";
    write(storage_socket, request, strlen(request));
    
    char buffer[32];
    int n = read(storage_socket, buffer, sizeof(buffer)-1);
    if (n > 0) {
        buffer[n] = '\0';
        return atoi(buffer);
    }
    return -1;
}
```

### 3. Логика транспортных средств

**Цикл работы грузовика 1:**
```cpp
void* Vehicle1Thread(void* arg) {
    while (run_flag) {
        // Фаза 1: Движение к хранилищу и загрузка
        if (vehicle1_state == MOVING_TO_STORAGE) {
            MoveVehicleTo(&vehicle1_x, &vehicle1_y, STORAGE_STOP_X, STORAGE_STOP_Y1, vehicle1_id);
            vehicle1_state = LOADING;
            
            // Анимация загрузки (3 итерации по 0.3 секунды)
            for (int i = 0; i < 3 && run_flag; i++) {
                usleep(300000);
                DrawState();
            }
            
            // Запрос топлива у сервера хранилища
            int fuel = RequestFuelFromStorage();
            if (fuel > 0) {
                vehicle1_fuel = fuel;
                vehicle1_target_boiler = SelectAvailableBoiler();
                vehicle1_state = MOVING_TO_BOILER;
            }
        }
        
        // Фаза 2: Движение к котлу и разгрузка
        if (vehicle1_state == MOVING_TO_BOILER && vehicle1_target_boiler != -1) {
            int target_x = BOILER_X[vehicle1_target_boiler];
            MoveVehicleTo(&vehicle1_x, &vehicle1_y, target_x, BOILER_STOP_Y1, vehicle1_id);
            vehicle1_state = UNLOADING;
            
            // Анимация разгрузки
            for (int i = 0; i < 3 && run_flag; i++) {
                usleep(300000);
                DrawState();
            }
            
            // Передача топлива котлу
            boiler_states[vehicle1_target_boiler] = BURNING;
            boiler_fuel_level[vehicle1_target_boiler] = vehicle1_fuel;
            boiler_fuel_marks[vehicle1_target_boiler] = vehicle1_fuel;
            boiler_low_fuel[vehicle1_target_boiler] = false;
            
            vehicle1_fuel = 0;
            vehicle1_target_boiler = -1;
            vehicle1_state = MOVING_TO_STORAGE;
        }
        
        usleep(100000);  // 0.1 секунда между проверками состояния
    }
    return NULL;
}
```

### 4. Логика работы котлов

```cpp
void* BoilerThread(void* arg) {
    int id = *((int*)arg);
    while (run_flag) {
        pthread_mutex_lock(&mutex);
        if (boiler_states[id] == BURNING) {
            if (boiler_fuel_level[id] > 0) {
                boiler_fuel_level[id]--;  // Сжигаем одну единицу топлива
                
                // Проверка критического уровня (2 единицы = 2 секунды)
                if (boiler_fuel_level[id] <= 2) {
                    boiler_low_fuel[id] = true;  // Сигнал о низком уровне
                }
                
                UpdateBoilerFuelIndicator(id);
            } else {
                // Топливо закончилось
                boiler_states[id] = WAITING_FOR_FUEL;
                boiler_fuel_marks[id] = 0;
                boiler_low_fuel[id] = false;
                
                // Удаление индикатора уровня топлива
                if (fuel_bar_ids[id] != 0) {
                    Delete(fuel_bar_ids[id]);
                    fuel_bar_ids[id] = 0;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        usleep(1000000);  // 1 секунда между сжиганием единицы топлива
    }
    return NULL;
}
```

### 5. Алгоритм выбора котла

```cpp
int SelectAvailableBoiler() {
    // Приоритет 1: Котлы с низким уровнем топлива
    for (int i = 0; i < 4; i++) {
        if (boiler_low_fuel[i] && !boiler_targeted[i]) {
            boiler_targeted[i] = true;
            return i;
        }
    }
    
    // Приоритет 2: Котлы, ожидающие топливо
    for (int i = 0; i < 4; i++) {
        if (boiler_states[i] == WAITING_FOR_FUEL && !boiler_targeted[i]) {
            boiler_targeted[i] = true;
            return i;
        }
    }
    
    return -1;  // Нет доступных котлов
}
```

### 6. Визуализация

**Обновление индикатора топлива:**
```cpp
void UpdateBoilerFuelIndicator(int boiler_id) {
    if (fuel_bar_ids[boiler_id] != 0) {
        Delete(fuel_bar_ids[boiler_id]);
    }
    
    if (boiler_fuel_level[boiler_id] > 0) {
        int max_fuel_height = BOILER_H - 20;
        int fuel_height = (boiler_fuel_level[boiler_id] * max_fuel_height) / 20;
        int fuel_y = BOILER_Y + BOILER_H - fuel_height - 10;
        
        // Цвет зависит от состояния:
        // - Зеленый: ожидание топлива
        // - Оранжевый: нормальное горение
        // - Красный: низкий уровень топлива
        int color;
        if (boiler_states[boiler_id] == BURNING) {
            color = boiler_low_fuel[boiler_id] ? RGB(255, 0, 0) : RGB(255, 165, 0);
        } else {
            color = RGB(0, 200, 0);
        }
        
        fuel_bar_ids[boiler_id] = Rect(
            BOILER_X[boiler_id] + 10, fuel_y,
            BOILER_W - 20, fuel_height,
            0, color);
    }
}
```

## Последовательность работы системы

### 1. Запуск системы
```
Сервер хранилища → Инициализация → Прослушивание порта 8080
Сервер котлов → Подключение к хранилищу → Запуск потоков → Графический интерфейс
```

### 2. Цикл работы транспортного средства
```
MOVING_TO_STORAGE → LOADING → [Сетевой запрос POP] → MOVING_TO_BOILER → UNLOADING → MOVING_TO_STORAGE
```

### 3. Цикл работы котла
```
WAITING_FOR_FUEL → [Получение топлива] → BURNING → [Топливо > 0] → BURNING → [Топливо ≤ 2] → LOW_FUEL_FLAG → [Топливо = 0] → WAITING_FOR_FUEL
```

### 4. Сетевой протокол
```
Клиент (сервер котлов)          Сервер (хранилище)
     |                             |
     |--------- "POP" ------------>|
     |                             | ● Проверка очереди
     |                             | ● Если не пусто: извлечь топливо
     |<-------- "[значение]" ------| ● Если пусто: вернуть "-1"
     |                             |
```

## Критические секции и синхронизация

### 1. Мьютексы
- **Сервер хранилища**: защита очереди `fuel_storage` от одновременного доступа
- **Сервер котлов**: защита общих данных (состояния котлов, уровни топлива)

### 2. Сетевые блокировки
- Один сокет для всех запросов от обоих грузовиков
- Запросы выполняются последовательно благодаря мьютексу

## Особенности временных параметров

- **Генерация топлива**: 1 единица в секунду (макс. 20)
- **Сжигание топлива**: 1 единица в секунду
- **Критический уровень**: 2 единицы (2 секунды работы)
- **Анимация движения**: 20 шагов по 0.05 секунды = 1 секунда
- **Анимация загрузки/разгрузки**: 3 шага по 0.3 секунды = 0.9 секунды
