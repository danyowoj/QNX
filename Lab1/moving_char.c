#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

static int x = 1, y = 1;
static int dx = 1, dy = 1;

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

int main()
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(restore_terminal);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    printf("\033[?25l"); // Скрытие курсора
    printf("\033[2J");   // Очистка экрана

    while (1)
    {
        printf("\033[%d;%dH*", y, x); // Рисуем символ
        printf("\033[%d;%dH ", y, x); // Стираем символ (пробелом)
        fflush(stdout);

        x += dx;
        y += dy;

        if (x <= 1 || x >= w.ws_col)
            dx = -dx;
        if (y <= 1 || y >= w.ws_row)
            dy = -dy;

        usleep(100000); // Задержка 100ms
    }

    return 0;
}
