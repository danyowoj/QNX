#include "vingraph.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <pthread.h>
#include <math.h>

volatile int run_flag = 1;
volatile int traj = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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

void *animator(void *)
{
    double t = 0.0;
    int radius = 30;
    while (1)
    {
        pthread_mutex_lock(&lock);
        int local_run = run_flag;
        int local_traj = traj;
        pthread_mutex_unlock(&lock);
        if (!local_run)
            break;

        double x = 0, y = 0;
        if (local_traj == 0)
        {
            x = 300 + 150 * cos(t);
            y = 200 + 120 * sin(t);
        }
        else if (local_traj == 1)
        {
            x = 300 + 200 * cos(t * 0.8);
            y = 200 + 80 * sin(t);
        }
        else
        {
            x = 300 + 140 * sin(3 * t + 0.5);
            y = 200 + 140 * sin(2 * t);
        }

        int id = Ellipse((int)x - radius, (int)y - radius, radius * 2, radius * 2, RGB((int)fmod(100 + 50 * t, 256), (int)fmod(50 + 80 * t, 256), (int)fmod(150 + 30 * t, 256)));
        usleep(60 * 1000);
        Delete(id);
        t += 0.06;
    }

    return NULL;
}

int main()
{
    ConnectGraph("Closed curve + threads demo");

    // Рисуем фон
    for (int i = 0; i < 8; i++)
        Rect(20 + i * 70, 20 + (i % 2) * 40, 60, 30, 6, RGB(20 * i, 200 - 10 * i, 50 * i));
    Line(0, 420, 800, 420, RGB(100, 100, 100));

    // Запускаем анимацию
    pthread_t th;
    pthread_create(&th, NULL, animator, NULL);

    // Главный поток обрабатывает клавиши
    set_raw_mode(1);
    printf("Press any key to change trajectory, 'q' to quit\n");
    char c;
    while (1)
    {
        if (read(0, &c, 1) == 1)
        {
            if (c == 'q' || c == '\x1b')
            {
                pthread_mutex_lock(&lock);
                run_flag = 0;
                pthread_mutex_unlock(&lock);
                break;
            }
            else
            {
                pthread_mutex_lock(&lock);
                traj = (traj + 1) % 3;
                pthread_mutex_unlock(&lock);
            }
        }
        usleep(50 * 1000);
    }
    set_raw_mode(0);
    pthread_join(th, NULL);
    CloseGraph();
    return 0;
}
