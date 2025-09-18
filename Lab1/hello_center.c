#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    int center_row = w.ws_row / 2;
    int center_col = (w.ws_col - 5) / 2; // 5 - длина слова "HELLO"

    printf("\033[2J");                             // Очистка экрана
    printf("\033[%d;%dH", center_row, center_col); // Перемещение курсора в центр
    printf("HELLO");
    printf("\033[%d;%dH", w.ws_row, 0); // Перемещение курсора в нижний левый угол
    fflush(stdout);

    sleep(3); // Задержка для просмотра результата
    return 0;
}
