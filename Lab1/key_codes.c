#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

static struct termios old_termios;

void restore_terminal()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("\033[?25h"); // Восстановление курсора
    printf("\033[0m");   // Сброс цветов
    fflush(stdout);
}

int main()
{
    tcgetattr(STDIN_FILENO, &old_termios);
    atexit(restore_terminal);

    struct termios new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO); // Неканонический режим без эха
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    printf("\033[?25l"); // Скрытие курсора
    printf("\033[2J");   // Очистка экрана
    printf("Press any key to see its code (ESC to exit):\n");

    int c;
    while ((c = getchar()) != 27)
    { // 27 - код ESC
        printf("Key code: %d\n", c);
    }

    return 0;
}
