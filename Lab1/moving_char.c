#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>

static int x = 10, y = 10; // Начальная позиция
static int dx = 1, dy = 1;
static int old_x = 10, old_y = 10; // Предыдущая позиция

void restore_terminal() {
    printf("\033[?25h"); // Восстановление курсора
    printf("\033[0m"); // Сброс цветов
    printf("\033[2J"); // Очистка экрана
    printf("\033[H"); // Курсор в начало
    fflush(stdout);
}

void handle_signal(int sig) {
    restore_terminal();
    exit(0);
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(restore_terminal);
    
    int rows = 24, cols = 80;
    
    // Получение размеров терминала
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        rows = w.ws_row;
        cols = w.ws_col;
    }
    
    printf("\033[?25l"); // Скрытие курсора
    printf("\033[2J"); // Очистка экрана
    fflush(stdout);
    
    // Начальная отрисовка
    printf("\033[%d;%dH*", y, x);
    fflush(stdout);
    
    while (1) {
        // Сохраняем старую позицию для стирания
        old_x = x;
        old_y = y;
        
        // Обновляем позицию
        x += dx;
        y += dy;
        
        // Проверяем границы
        if (x <= 1) { x = 1; dx = -dx; }
        if (x >= cols) { x = cols; dx = -dx; }
        if (y <= 1) { y = 1; dy = -dy; }
        if (y >= rows) { y = rows; dy = -dy; }
        
        // Стираем старый символ
        printf("\033[%d;%dH ", old_y, old_x);
        
        // Рисуем новый символ
        printf("\033[%d;%dH*", y, x);
        
        // Перемещаем курсор в угол чтобы не мешал
        printf("\033[%d;%dH", rows, 1);
        fflush(stdout);
        
        usleep(100000); // Задержка 100ms
    }
    
    return 0;
}
