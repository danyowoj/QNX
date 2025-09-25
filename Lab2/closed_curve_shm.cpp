#include "vingraph.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <termios.h>
#include <math.h>
#include <sys/wait.h>

struct Shm
{
    volatile int run;  // 1 = работать, 0 = выход
    volatile int traj; // текущая траектория: 0=circle,1=ellipse,2=lissajous
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
    ConnectGraph("Closed curve + SHM demo");

    // Рисуем фон (простая «оживлённая» картина)
    for (int i = 0; i < 10; i++)
    {
        Rect(10 + i * 50, 10 + (i % 3) * 40, 40, 30, 4, RGB(20 * i, 255 - 20 * i, 50 * i));
    }
    Line(0, 400, 800, 400, RGB(150, 150, 150));
    Ellipse(600, 200, 120, 80, RGB(100, 150, 255));

    // Создаём shm
    const char *name = "/vg_shm_curve";
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(Shm));
    Shm *shm = (Shm *)mmap(NULL, sizeof(Shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    shm->run = 1;
    shm->traj = 0;

    pid_t pid = fork();
    if (pid == 0)
    {
        // Дочерний процесс: анмимация движущегося объекта
        double t = 0.0;
        int radius = 30;
        while (shm->run)
        {
            // Выбираем траекторию
            int traj = shm->traj;
            double x = 0, y = 0;
            if (traj == 0) // круг
            {
                x = 300 + 150 * cos(t);
                y = 200 + 120 * sin(t);
            }
            else if (traj == 1) // эллипс
            {
                x = 300 + 200 * cos(t * 0.8);
                y = 200 + 80 * sin(t);
            }
            else // другая кривая
            {
                x = 300 + 140 * sin(3 * t + 0.5);
                y = 200 + 140 * sin(2 * t);
            }
            int id = Ellipse((int)x - radius, (int)y - radius, radius * 2, radius * 2, RGB((int)fmod(100 + 50 * t, 256), (int)fmod(50 + 80 * t, 256), (int)fmod(150 + 30 * t, 256)));
            usleep(60 * 1000);
            Delete(id);
            t += 0.06;
        }
        _exit(0);
    }
    else if (pid > 0)
    {
        // Родитель: обрабатывает клавиши:
        // - при любой клавише циклирует traj
        // - Esc или 'q' — выход
        set_raw_mode(1);
        printf("Press any key to change the trajectory, 'q' to quit\n");
        char c;
        while (shm->run)
        {
            if (read(0, &c, 1) == 1)
            {
                if (c == 'q' || c == '\xlb')
                {
                    shm->run = 0;
                    break;
                }
                else
                {
                    shm->traj = (shm->traj + 1) % 3;
                }
            }
            usleep(50 * 1000);
        }
        set_raw_mode(0);
        waitpid(pid, NULL, 0);
    }
    else
    {
        perror("fork");
    }

    munmap(shm, sizeof(Shm));
    shm_unlink(name);
    CloseGraph();

    return 0;
}
