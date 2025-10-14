#include <iostream>
#include <unistd.h>            // usleep
#include "/root/labs/plates.h" // определения регистров и команд

using namespace std;

/*
  send_rcm_command:
  - Устанавливает номер управляемого снаряда (RG_RCMN) и отправляет команду в регистр RG_RCMC.
  - Алгоритм: записать номер (чтобы последующие команды адресовались именно этому снаряду),
    потом записать код команды (начать/вправо/влево/вверх/вниз).
*/
static inline void send_rcm_command(int rus_number, int cmd)
{
    putreg(RG_RCMN, rus_number); // выбрать РУС
    putreg(RG_RCMC, cmd);        // отправить команду выбранному РУСу
}

/*
  fly_straight_seconds:
  - Держит РУС в текущем направлении заданное время (в секундах).
  - Реализация: просто спим на нужное количество микросекунд.
  - Замечание: переключение направления происходит отдельно (через send_rcm_command).
*/
static inline void fly_straight_seconds(double seconds)
{
    if (seconds <= 0.0)
        return;
    // usleep принимает микросекунды
    unsigned int usec = (unsigned int)(seconds * 1e6);
    usleep(usec);
}

/*
  perform_polygon_path:
  - Общая функция, реализующая движение РУСа по замкнутому ломаному пути,
    заданному списком пар (cmd, length_in_pixels).
  - Для каждой стороны: посылаем команду направления и ждём time = length / speed.
  - speed_px_per_s -- скорость РУСа (по условию 250 точек/с).
  - Алгоритм: для каждой стороны выбрать направление -> подождать время пролёта.
*/
void perform_polygon_path(int rus_number, const int *cmds, const double *lengths_px, int sides, double speed_px_per_s)
{
    for (int i = 0; i < sides; ++i)
    {
        send_rcm_command(rus_number, cmds[i]);     // переключаем направление движения
        double t = lengths_px[i] / speed_px_per_s; // время на прохождение стороны
        fly_straight_seconds(t);                   // ждем, пока пройдёт расстояние
    }
}

int main()
{
    // Параметры, взятые из текста задания
    const int RUS_NUMBER = 0;       // используем снаряд номер 0
    const double RUS_SPEED = 250.0; // 250 точек/с (в любом направлении)
    // Время небольшого ожидания после запуска системы, чтобы всё стабилизировалось
    const double START_DELAY = 0.2; // секунды

    // Запустить систему в режиме 0 (нет тарелок)
    StartGame(0);
    // Небольшая пауза перед управлением, чтобы система инициализировалась
    fly_straight_seconds(START_DELAY);

    // Запустить РУС: команда START (по условию — старт и движение вверх)
    send_rcm_command(RUS_NUMBER, RCMC_START);
    // Подождать немного, чтобы ракета начала движение
    fly_straight_seconds(0.05);

    /*
      Частность: начальное направление после RCMC_START — вверх.
      Чтобы пройти по квадрату/прямоугольнику, будем посылать команды переключения
      направления: LEFT, DOWN, RIGHT, UP и т.д., с рассчитанными по длине задержками.
    */

    // --------- Часть A: квадрат 200x200 (4 стороны) ----------
    const int square_cmds[4] = {RCMC_UP, RCMC_RIGHT, RCMC_DOWN, RCMC_LEFT};
    const double square_lengths[4] = {200.0, 200.0, 200.0, 200.0};
    perform_polygon_path(RUS_NUMBER, square_cmds, square_lengths, 4, RUS_SPEED);

    // Небольшая пауза между фигурами
    fly_straight_seconds(0.1);

    // --------- Часть B: прямоугольник 500x200 (4 стороны) ----------
    // Начнём с направления вправо (предполагаем текущее направление — левое/последнее, но
    // send_rcm_command гарантирует установку нужного направления в любом случае).
    const int rect_cmds[4] = {RCMC_RIGHT, RCMC_DOWN, RCMC_LEFT, RCMC_UP};
    const double rect_lengths[4] = {500.0, 200.0, 500.0, 200.0};
    perform_polygon_path(RUS_NUMBER, rect_cmds, rect_lengths, 4, RUS_SPEED);

    // По завершении манёвров можно "остановить" РУС явной командой, если требуется.
    // В условии нет отдельной команды стопа — можно переключить направление вверх и не двигаться дальше.
    // Здесь просто даём небольшую паузу и завершаем.
    fly_straight_seconds(0.1);

    // Завершить работу системы
    EndGame();

    return 0;
}
