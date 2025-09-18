#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

static int x, y;
static int dx = 1, dy = 1;
static int delay = 100000; // 100ms по умолчанию
static char symbol = '*';
static char color[10] = "\033[0m"; // Цвет по умолчанию

void restore_terminal()
{
    printf("\033[?25h"); // Восстановление курсора
    printf("\033[0m");   // Сброс цветов
    printf("\033[2J");   // Очистка экрана
    fflush(stdout);
}

void handle_signal(int sig)
{
    restore_terminal();
    exit(0);
}

void print_usage()
{
    printf("Usage: moving_char [options]\n");
    printf("Options:\n");
    printf("  -x N      Initial X position (default: 1)\n");
    printf("  -y N      Initial Y position (default: 1)\n");
    printf("  -dx N     X direction (1 or -1, default: 1)\n");
    printf("  -dy N     Y direction (1 or -1, default: 1)\n");
    printf("  -d N      Delay in microseconds (default: 100000)\n");
    printf("  -s CHAR   Symbol to display (default: *)\n");
    printf("  -c COLOR  Color code (30-37, 40-47, default: 0)\n");
}

int main(int argc, char *argv[])
{
    // Обработка параметров командной строки
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-x") == 0 && i + 1 < argc)
        {
            x = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-y") == 0 && i + 1 < argc)
        {
            y = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-dx") == 0 && i + 1 < argc)
        {
            dx = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-dy") == 0 && i + 1 < argc)
        {
            dy = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
        {
            delay = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            symbol = argv[++i][0];
        }
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            snprintf(color, sizeof(color), "\033[%sm", argv[++i]);
        }
        else
        {
            print_usage();
            return 1;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(restore_terminal);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    printf("\033[?25l"); // Скрытие курсора
    printf("\033[2J");   // Очистка экрана

    while (1)
    {
        printf("\033[%d;%dH%s%c", y, x, color, symbol); // Рисуем символ с цветом
        printf("\033[%d;%dH ", y, x);                   // Стираем символ
        fflush(stdout);

        x += dx;
        y += dy;

        if (x <= 1 || x >= w.ws_col)
            dx = -dx;
        if (y <= 1 || y >= w.ws_row)
            dy = -dy;

        usleep(delay);
    }

    return 0;
}
