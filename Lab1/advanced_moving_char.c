#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

static int x_pos, y_pos;
static int dx = 1, dy = 1;
static int sleep_delay = 100000;
static char symbol = '*';
static char color[10] = "\033[0m";

void restore_terminal()
{
    printf("\033[?25h");
    printf("\033[0m");
    printf("\033[2J");
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
    printf("  -x N      Initial X position (default: 10)\n");
    printf("  -y N      Initial Y position (default: 10)\n");
    printf("  -dx N     X direction (1 or -1, default: 1)\n");
    printf("  -dy N     Y direction (1 or -1, default: 1)\n");
    printf("  -d N      Delay in microseconds (default: 100000)\n");
    printf("  -s CHAR   Symbol to display (default: *)\n");
    printf("  -c COLOR  Color code (30-37, 40-47, default: 0)\n");
    printf("  -h        Show this help message\n");
}

int main(int argc, char *argv[])
{
    int i;
    int show_help = 0;
    
    // Значения по умолчанию
    x_pos = 10;
    y_pos = 10;
    
    // Обработка параметров командной строки
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-x") == 0 && i + 1 < argc)
        {
            x_pos = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-y") == 0 && i + 1 < argc)
        {
            y_pos = atoi(argv[++i]);
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
            sleep_delay = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            symbol = argv[++i][0];
        }
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            snprintf(color, sizeof(color), "\033[%sm", argv[++i]);
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            show_help = 1;
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    // Если запрошена справка или нет параметров
    if (show_help || argc == 1)
    {
        print_usage();
        return 0;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    atexit(restore_terminal);

    int rows = 24, cols = 80;
    int old_x = x_pos, old_y = y_pos;
    
    printf("\033[?25l");
    printf("\033[2J");
    printf("\033[%d;%dH%s%c", y_pos, x_pos, color, symbol);
    fflush(stdout);

    while (1)
    {
        old_x = x_pos;
        old_y = y_pos;
        
        x_pos += dx;
        y_pos += dy;

        if (x_pos <= 1) { x_pos = 1; dx = -dx; }
        if (x_pos >= cols) { x_pos = cols; dx = -dx; }
        if (y_pos <= 1) { y_pos = 1; dy = -dy; }
        if (y_pos >= rows) { y_pos = rows; dy = -dy; }

        printf("\033[%d;%dH ", old_y, old_x);
        printf("\033[%d;%dH%s%c", y_pos, x_pos, color, symbol);
        printf("\033[%d;1H", rows);
        fflush(stdout);

        usleep(sleep_delay);
    }

    return 0;
}
