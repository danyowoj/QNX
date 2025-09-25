#include "vingraph.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <termios.h>
#include <sys/wait.h>
#include <time.h>

struct Shm
{
    volatile int run;
};

static void set_raw_mode(int enable)
{
    static struct termios oldt;
    struct termios newt;
    if (enable)
    {
        tcgetattr(0, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(0, TCSANOW, &newt);
    }
    else
    {
        tcsetattr(0, TCSANOW, &oldt);
    }
}

int main()
{
    ConnectGraph("Move processes demo");

    // Создаем общую память
    const char *name = "/vg_shm_demo";
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(Shm));
    Shm *shm = (Shm *)mmap(NULL, sizeof(Shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    shm->run = 1;

    // Создаем несколько дочерних процессов,
    // каждый из которых двигает один элемент
    const int CHILDREN = 4;
    pid_t pids[CHILDREN];
    for (int i = 0; i < CHILDREN; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // Дочерний процесс: анимация одной фигуры
            int x = 50 + i * 120;
            int y = 50 + i * 60;
            int dir = 1;
            int width = 60, height = 30;
            srand(time(NULL) ^ (getpid() << 8));
            while (shm->run)
            {
                // Рисуем, ждем, удаляем, сдвигаем
                int id = Rect(x, y, width, height, 6, RGB(rand() % 256, rand() % 256, rand() % 256));
                usleep(100 * 1000); // 100 ms
                Delete(id);
                x += dir * 8;
                if (x < 10 || x + width > 640)
                    dir = -dir;
            }
            _exit(0);
        }
        else if (pid > 0)
        {
            pids[i] = pid;
        }
        else
        {
            perror("fork");
            shm->run = 0;
            break;
        }
    }

    // Родитель: ждет нажатия любой клавиши (включаем raw режим)
    set_raw_mode(1);
    printf("Press any key to stop the animation...\n");
    char c;
    read(0, &c, 1); // Ждем нажатия на клавишу
    set_raw_mode(0);

    // Завершаем дочерние процессы
    shm->run = 0;
    for (int i = 0; i < CHILDREN; i++)
        waitpid(pids[i], NULL, 0);
    munmap(shm, sizeof(Shm));
    shm_unlink(name);

    CloseGraph();
    return 0;
}
