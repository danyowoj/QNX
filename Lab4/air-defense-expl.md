# Документация по системе ПВО (Противовоздушной обороны)

## Общее описание

Данная программа представляет собой **симулятор системы противовоздушной обороны**, предназначенной для обнаружения и уничтожения летающих тарелок. Система использует два типа вооружения:
- **Ракеты** - для медленных целей (скорость < 100 единиц)
- **РУС (Радиоуправляемые снаряды)** - для быстрых целей (скорость ≥ 100 единиц)

```c
#define SPEED_THRESHOLD 100  // Порог скорости для выбора типа оружия
#define MAX_DESTROYED 25     // Максимальное количество целей для уничтожения
```

## Архитектура системы

### Структуры данных

**PlateData** - содержит информацию о каждой обнаруженной тарелке:
```c
typedef struct {
    int height;                    // Высота полета (0-570 единиц)
    int speed;                     // Вычисленная скорость
    int direction;                 // Направление: 1 (слева-направо), -1 (справа-налево)
    long long loc1_time, loc2_time; // Временные метки левых локаторов
    long long loc3_time, loc4_time; // Временные метки правых локаторов
    int processed;                 // Флаг обработки цели
    pthread_mutex_t mutex;         // Синхронизация доступа
} PlateData;
```

**RUSInfo** - управление радиоуправляемыми снарядами:
```c
typedef struct {
    int rus_num;                   // Номер РУС (0-14)
    int is_active;                 // Статус: 0 - свободен, 1 - занят
    pthread_mutex_t mutex;         // Синхронизация доступа к РУС
} RUSInfo;
```

## Алгоритм работы

### 1. Обнаружение целей

Система использует **4 локатора** для обнаружения тарелок:
- **LEFT-1** и **LEFT-2** - левая сторона
- **RIGHT-1** и **RIGHT-2** - правая сторона

```c
const char *get_locator_side(int loc_num) {
    switch (loc_num) {
        case 1: return "LEFT-1";
        case 2: return "LEFT-2";
        case 3: return "RIGHT-1";
        case 4: return "RIGHT-2";
        default: return "UNKNOWN";
    }
}
```

### 2. Расчет скорости и направления

Скорость вычисляется по времени прохождения между двумя локаторами:

```c
// Для движения слева-направо
long long t1 = plates[plate_index].loc1_time;
long long t2 = plates[plate_index].loc2_time;
long long dt = abs(t2 - t1);
double speed = (10.0 * 1000000.0) / (double)dt;  // 10м / время в микросекундах
```

### 3. Выбор стратегии перехвата

#### Для быстрых целей (РУС):
```c
// Расчет времени полета РУС до высоты цели
double vertical_distance = 570 - y;
double rus_vertical_time = vertical_distance / 250.0;  // 250 м/с - скорость РУС

// Определение стратегии: навстречу или вдогонку
int meet = (rus_vertical_time < time_to_center) ? 1 : 0;
```

#### Для медленных целей (ракеты):
```c
// Расчет времени перехвата в центре
double rocket_time = (double)rocket_distance / 100.0;  // 100 м/с - скорость ракеты
double sd_sec = time_to_center - rocket_time;
int shoot_delay = (int)round(sd_sec * 1000000.0);  // Задержка выстрела
```

## Ключевые механизмы

### Управление памятью

Система использует **пул слотов** для хранения данных о тарелках:

```c
#define PLATE_SLOTS 30  // Максимальное количество одновременно отслеживаемых целей

int find_or_create_plate(int height) {
    // 1. Поиск существующей тарелки с такой же высотой
    // 2. Поиск свободного слота
    // 3. Создание нового слота при наличии места
    // 4. Возврат -1 при переполнении
}
```

### Очистка устаревших записей

```c
void cleanup_old_plates(void) {
    // Тарелки удаляются если:
    // - Прошло >3 секунд с последнего обнаружения
    // - Или >1 секунда для обработанных тарелок
    if ((max_time > 0 && (current_time - max_time) > 3000000LL) ||
        (plates[i].processed && (current_time - max_time) > 1000000LL)) {
        // Сброс данных слота
    }
}
```

### Многопоточность и синхронизация

Каждая тарелка защищена **индивидуальным мьютексом**:
```c
pthread_mutex_lock(&plates[plate_index].mutex);
// Критическая секция - работа с данными тарелки
pthread_mutex_unlock(&plates[plate_index].mutex);
```

**Глобальные счетчики** защищены отдельными мьютексами:
```c
static pthread_mutex_t plates_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rus_array_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t destroyed_mutex = PTHREAD_MUTEX_INITIALIZER;
```

## Обработка прерываний

Система использует **аппаратные прерывания** для получения данных от локаторов:

```c
const struct sigevent *locator_handler(void *area, int id) {
    // Чтение данных из аппаратных регистров
    int loc_num = getreg(RG_LOCN);  // Номер локатора
    int y = getreg(RG_LOCY);        // Высота
    int w = getreg(RG_LOCW);        // Ширина (3 = тарелка)
    
    // Проверка типа цели
    if (w != 3) return NULL;
    
    // Обновление данных тарелки
    // Запуск обработки при наличии данных от двух локаторов
}
```

## Поток выполнения

### Основной цикл:
```c
void air_defense_system(void) {
    // Инициализация структур данных
    init_rus_array();
    
    // Привязка обработчика прерываний
    interrupt_id = InterruptAttach(LOC_INTR, locator_handler, NULL, 0, 0);
    
    // Главный цикл
    while (destroyed_plates < MAX_DESTROYED && program_running) {
        sleep(1);
        // Периодическая очистка старых записей
        cleanup_old_plates();
    }
}
```

## Особенности реализации

### 1. **Приоритет обработки**
- Быстрые тарелки обрабатываются РУС немедленно
- Медленные тарелки - с расчетом времени перехвата

### 2. **Ограничение ресурсов**
- Максимум 30 одновременно отслеживаемых целей
- Только 15 доступных РУС
- При отсутствии свободных РУС система ожидает освобождения

### 3. **Отказоустойчивость**
- Проверка согласованности данных локаторов
- Защита от некорректных временных интервалов
- Автоматическое освобождение ресурсов

### 4. **Стратегия перехвата РУС**
```c
if (meet) {
    // Стрельба навстречу
    if (direction == 1) send_rus_command(rus_num, RCMC_LEFT);
    else send_rus_command(rus_num, RCMC_RIGHT);
} else {
    // Стрельба вдогонку
    if (direction == 1) send_rus_command(rus_num, RCMC_RIGHT);
    else send_rus_command(rus_num, RCMC_LEFT);
}
```

## Завершение работы

Система завершает работу при:
- Достижении максимального количества уничтоженных целей (25)
- Истечении времени работы (5 минут)
- Принудительной остановке программы

```c
while (destroyed_plates < MAX_DESTROYED && program_running) {
    // ...
    if (time(NULL) - start_time > 300) break;  // 5 минут таймаут
}
```
