#include "/root/labs/plates.h"
#include <sys/neutrino.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#define PLATE_SLOTS 30
#define RUS_SLOTS 15
#define MAX_DESTROYED 25
#define SPEED_THRESHOLD 100

typedef struct
{
    int height;
    int speed;
    int direction;
    long long loc1_time;
    long long loc2_time;
    long long loc3_time;
    long long loc4_time;
    int processed;
    pthread_mutex_t mutex;
} PlateData;

typedef struct
{
    int rus_num;
    int is_active;
    pthread_mutex_t mutex;
} RUSInfo;

typedef struct
{
    int plate_index;
    int use_rus;
    int shoot_delay;
    int rus_num;
} ShootThreadData;

long long get_time_us(void);
const char *get_locator_side(int loc_num);
void init_rus_array(void);
int get_available_rus(void);
void release_rus(int rus_num);
void cleanup_old_plates(void);
int find_or_create_plate(int height);
void start_left_to_right_processing(int plate_index);
void start_right_to_left_processing(int plate_index);
void send_rus_command(int rus_num, int command);

static PlateData plates[PLATE_SLOTS];
static int plate_count = 0;
static pthread_mutex_t plates_mutex = PTHREAD_MUTEX_INITIALIZER;

static RUSInfo rus_array[RUS_SLOTS];
static pthread_mutex_t rus_array_mutex = PTHREAD_MUTEX_INITIALIZER;

static int destroyed_plates = 0;
static pthread_mutex_t destroyed_mutex = PTHREAD_MUTEX_INITIALIZER;

long long get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}

const char *get_locator_side(int loc_num)
{
    if (loc_num == 1 || loc_num == 2)
        return "LEFT";
    return "RIGHT";
}

void init_rus_array(void)
{
    pthread_mutex_lock(&rus_array_mutex);
    for (int i = 0; i < RUS_SLOTS; ++i)
    {
        rus_array[i].rus_num = i;
        rus_array[i].is_active = 0;
        pthread_mutex_init(&rus_array[i].mutex, NULL);
    }
    pthread_mutex_unlock(&rus_array_mutex);
}

int get_available_rus(void)
{
    pthread_mutex_lock(&rus_array_mutex);
    for (int i = 0; i < RUS_SLOTS; ++i)
    {
        if (!rus_array[i].is_active)
        {
            rus_array[i].is_active = 1;
            pthread_mutex_unlock(&rus_array_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&rus_array_mutex);
    return -1;
}

void release_rus(int rus_num)
{
    if (rus_num < 0 || rus_num >= RUS_SLOTS)
        return;
    pthread_mutex_lock(&rus_array_mutex);
    rus_array[rus_num].is_active = 0;
    pthread_mutex_unlock(&rus_array_mutex);
}

void send_rus_command(int rus_num, int command)
{
    if (rus_num < 0 || rus_num >= RUS_SLOTS)
        return;
    pthread_mutex_lock(&rus_array_mutex);
    if (!rus_array[rus_num].is_active)
    {
        pthread_mutex_unlock(&rus_array_mutex);
        return;
    }
    pthread_mutex_lock(&rus_array[rus_num].mutex);
    pthread_mutex_unlock(&rus_array_mutex);
    putreg(RG_RCMN, rus_num);
    putreg(RG_RCMC, command);
    pthread_mutex_unlock(&rus_array[rus_num].mutex);
}

void cleanup_old_plates(void)
{
    long long current_time = get_time_us();
    pthread_mutex_lock(&plates_mutex);
    for (int i = 0; i < plate_count; ++i)
    {
        long long max_time = 0;
        if (plates[i].loc1_time > max_time)
            max_time = plates[i].loc1_time;
        if (plates[i].loc2_time > max_time)
            max_time = plates[i].loc2_time;
        if (plates[i].loc3_time > max_time)
            max_time = plates[i].loc3_time;
        if (plates[i].loc4_time > max_time)
            max_time = plates[i].loc4_time;
        if ((max_time > 0 && (current_time - max_time) > 3000000LL) ||
            (plates[i].processed && (current_time - max_time) > 1000000LL))
        {
            pthread_mutex_lock(&plates[i].mutex);
            plates[i].height = 0;
            plates[i].speed = 0;
            plates[i].direction = 0;
            plates[i].loc1_time = plates[i].loc2_time = plates[i].loc3_time = plates[i].loc4_time = 0;
            plates[i].processed = 1;
            pthread_mutex_unlock(&plates[i].mutex);
        }
    }
    pthread_mutex_unlock(&plates_mutex);
}

int find_or_create_plate(int height)
{
    pthread_mutex_lock(&plates_mutex);
    for (int i = 0; i < plate_count; ++i)
    {
        if (plates[i].height == height)
        {
            pthread_mutex_unlock(&plates_mutex);
            return i;
        }
    }
    if (plate_count < PLATE_SLOTS)
    {
        int idx = plate_count++;
        plates[idx].height = height;
        plates[idx].speed = 0;
        plates[idx].direction = 0;
        plates[idx].loc1_time = plates[idx].loc2_time = plates[idx].loc3_time = plates[idx].loc4_time = 0;
        plates[idx].processed = 1;
        pthread_mutex_init(&plates[idx].mutex, NULL);
        pthread_mutex_unlock(&plates_mutex);
        return idx;
    }
    pthread_mutex_unlock(&plates_mutex);
    return -1;
}

void log_detection(int plate_index, double speed)
{
    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    pthread_mutex_unlock(&plates[plate_index].mutex);
    printf("DETECT: plate=%d height=%d speed=%.2f\n", plate_index, y, speed);
}

void log_hit_by_rus(int plate_index, int speed, int rus_num)
{
    pthread_mutex_lock(&destroyed_mutex);
    destroyed_plates++;
    int total = destroyed_plates;
    pthread_mutex_unlock(&destroyed_mutex);
    printf("HIT: plate=%d speed=%d by RUS=%d. TOTAL=%d\n", plate_index, speed, rus_num, total);
}

void log_hit_by_rocket(int plate_index, int speed)
{
    pthread_mutex_lock(&destroyed_mutex);
    destroyed_plates++;
    int total = destroyed_plates;
    pthread_mutex_unlock(&destroyed_mutex);
    printf("HIT: plate=%d speed=%d by ROCKET. TOTAL=%d\n", plate_index, speed, total);
}

void *rus_shoot_thread(void *arg)
{
    if (!arg)
        return NULL;
    ShootThreadData *data = (ShootThreadData *)arg;
    int plate_index = data->plate_index;
    int rus_num = data->rus_num;
    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    int direction = plates[plate_index].direction;
    int speed = plates[plate_index].speed;
    pthread_mutex_unlock(&plates[plate_index].mutex);
    double vertical_distance = 570 - y;
    if (vertical_distance < 0)
        vertical_distance = 0;
    double rus_vertical_time = vertical_distance / 250.0;
    double distance_to_center = 380.0;
    double time_to_center = (speed > 0) ? (distance_to_center / (double)speed) : 1e9;
    int meet = (rus_vertical_time < time_to_center) ? 1 : 0;
    send_rus_command(rus_num, RCMC_START);
    int vertical_delay = (int)round(rus_vertical_time * 1000000.0);
    if (vertical_delay > 0)
        usleep((useconds_t)vertical_delay);
    if (meet)
    {
        if (direction == 1)
            send_rus_command(rus_num, RCMC_LEFT);
        else
            send_rus_command(rus_num, RCMC_RIGHT);
    }
    else
    {
        if (direction == 1)
            send_rus_command(rus_num, RCMC_RIGHT);
        else
            send_rus_command(rus_num, RCMC_LEFT);
    }
    int travel_delay = (int)round(200000.0);
    if (travel_delay > 0)
        usleep((useconds_t)travel_delay);
    pthread_mutex_lock(&plates[plate_index].mutex);
    int sp = plates[plate_index].speed;
    plates[plate_index].processed = 1;
    pthread_mutex_unlock(&plates[plate_index].mutex);
    log_hit_by_rus(plate_index, sp, rus_num);
    release_rus(rus_num);
    free(data);
    return NULL;
}

void *shoot_thread(void *arg)
{
    if (!arg)
        return NULL;
    ShootThreadData *data = (ShootThreadData *)arg;
    int plate_index = data->plate_index;
    int shoot_delay = data->shoot_delay;
    if (shoot_delay > 0)
    {
        usleep((useconds_t)shoot_delay);
        putreg(RG_GUNS, GUNS_SHOOT);
        pthread_mutex_lock(&plates[plate_index].mutex);
        int speed = plates[plate_index].speed;
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
        log_hit_by_rocket(plate_index, speed);
    }
    else
    {
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
    }
    free(data);
    return NULL;
}

void *process_plate_left_to_right_thread(void *arg)
{
    if (!arg)
        return NULL;
    int plate_index = *(int *)arg;
    free(arg);
    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    long long t1 = plates[plate_index].loc1_time;
    long long t2 = plates[plate_index].loc2_time;
    pthread_mutex_unlock(&plates[plate_index].mutex);
    long long early = (t1 < t2) ? t1 : t2;
    long long late = (t1 < t2) ? t2 : t1;
    long long dt = late - early;
    if (dt <= 500 || dt >= 20000000)
    {
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
        return NULL;
    }
    double distance_between_locators = 10.0;
    double speed = (distance_between_locators * 1000000.0) / (double)dt;
    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].speed = (int)round(speed);
    plates[plate_index].direction = 1;
    pthread_mutex_unlock(&plates[plate_index].mutex);
    log_detection(plate_index, speed);
    int use_rus = (speed >= SPEED_THRESHOLD) ? 1 : 0;
    int shoot_delay = 0;
    double distance_to_center = 380.0;
    double time_to_center = (speed > 0) ? (distance_to_center / speed) : 1e9;
    if (use_rus)
    {
        int rus_num = get_available_rus();
        long long start_wait = get_time_us();
        long long wait_deadline = start_wait + (long long)(time_to_center * 1000000.0) - 20000LL;
        if (wait_deadline < start_wait)
            wait_deadline = start_wait + 20000LL;
        while (rus_num == -1 && get_time_us() < wait_deadline)
        {
            cleanup_old_plates();
            usleep((useconds_t)10000);
            rus_num = get_available_rus();
        }
        if (rus_num == -1)
        {
            int rocket_distance = 570 - y;
            if (rocket_distance < 0)
                rocket_distance = 0;
            double rocket_time = (double)rocket_distance / 100.0;
            double sd_sec = time_to_center - rocket_time;
            int shoot_delay_local = (int)round(sd_sec * 1000000.0);
            if (shoot_delay_local > 0)
            {
                ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
                if (!sd)
                    return NULL;
                sd->plate_index = plate_index;
                sd->use_rus = 0;
                sd->shoot_delay = shoot_delay_local;
                sd->rus_num = -1;
                pthread_t t;
                if (pthread_create(&t, NULL, shoot_thread, sd) != 0)
                {
                    free(sd);
                    return NULL;
                }
                pthread_detach(t);
                return NULL;
            }
            else
            {
                pthread_mutex_lock(&plates[plate_index].mutex);
                plates[plate_index].processed = 1;
                pthread_mutex_unlock(&plates[plate_index].mutex);
                return NULL;
            }
        }
        ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
        if (!sd)
        {
            release_rus(rus_num);
            return NULL;
        }
        sd->plate_index = plate_index;
        sd->use_rus = 1;
        sd->shoot_delay = 0;
        sd->rus_num = rus_num;
        pthread_t t;
        if (pthread_create(&t, NULL, rus_shoot_thread, sd) != 0)
        {
            release_rus(rus_num);
            free(sd);
            return NULL;
        }
        pthread_detach(t);
        return NULL;
    }
    else
    {
        int rocket_distance = 570 - y;
        if (rocket_distance < 0)
            rocket_distance = 0;
        double rocket_time = (double)rocket_distance / 100.0;
        double sd_sec = time_to_center - rocket_time;
        shoot_delay = (int)round(sd_sec * 1000000.0);
        if (shoot_delay > 0)
        {
            ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
            if (!sd)
                return NULL;
            sd->plate_index = plate_index;
            sd->use_rus = 0;
            sd->shoot_delay = shoot_delay;
            sd->rus_num = -1;
            pthread_t t;
            if (pthread_create(&t, NULL, shoot_thread, sd) != 0)
            {
                free(sd);
                return NULL;
            }
            pthread_detach(t);
        }
        else
        {
            pthread_mutex_lock(&plates[plate_index].mutex);
            plates[plate_index].processed = 1;
            pthread_mutex_unlock(&plates[plate_index].mutex);
        }
        return NULL;
    }
}

void *process_plate_right_to_left_thread(void *arg)
{
    if (!arg)
        return NULL;
    int plate_index = *(int *)arg;
    free(arg);
    pthread_mutex_lock(&plates[plate_index].mutex);
    int y = plates[plate_index].height;
    long long t3 = plates[plate_index].loc3_time;
    long long t4 = plates[plate_index].loc4_time;
    pthread_mutex_unlock(&plates[plate_index].mutex);
    long long early = (t3 < t4) ? t3 : t4;
    long long late = (t3 < t4) ? t4 : t3;
    long long dt = late - early;
    if (dt <= 500 || dt >= 20000000)
    {
        pthread_mutex_lock(&plates[plate_index].mutex);
        plates[plate_index].processed = 1;
        pthread_mutex_unlock(&plates[plate_index].mutex);
        return NULL;
    }
    double distance_between_locators = 10.0;
    double speed = (distance_between_locators * 1000000.0) / (double)dt;
    pthread_mutex_lock(&plates[plate_index].mutex);
    plates[plate_index].speed = (int)round(speed);
    plates[plate_index].direction = -1;
    pthread_mutex_unlock(&plates[plate_index].mutex);
    log_detection(plate_index, speed);
    int use_rus = (speed >= SPEED_THRESHOLD) ? 1 : 0;
    double distance_to_center = 380.0;
    double time_to_center = (speed > 0) ? (distance_to_center / speed) : 1e9;
    int shoot_delay = 0;
    if (use_rus)
    {
        int rus_num = get_available_rus();
        long long start_wait = get_time_us();
        long long wait_deadline = start_wait + (long long)(time_to_center * 1000000.0) - 20000LL;
        if (wait_deadline < start_wait)
            wait_deadline = start_wait + 20000LL;
        while (rus_num == -1 && get_time_us() < wait_deadline)
        {
            cleanup_old_plates();
            usleep((useconds_t)10000);
            rus_num = get_available_rus();
        }
        if (rus_num == -1)
        {
            int rocket_distance = 570 - y;
            if (rocket_distance < 0)
                rocket_distance = 0;
            double rocket_time = (double)rocket_distance / 100.0;
            double sd_sec = time_to_center - rocket_time;
            int shoot_delay_local = (int)round(sd_sec * 1000000.0);
            if (shoot_delay_local > 0)
            {
                ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
                if (!sd)
                    return NULL;
                sd->plate_index = plate_index;
                sd->use_rus = 0;
                sd->shoot_delay = shoot_delay_local;
                sd->rus_num = -1;
                pthread_t t;
                if (pthread_create(&t, NULL, shoot_thread, sd) != 0)
                {
                    free(sd);
                    return NULL;
                }
                pthread_detach(t);
                return NULL;
            }
            else
            {
                pthread_mutex_lock(&plates[plate_index].mutex);
                plates[plate_index].processed = 1;
                pthread_mutex_unlock(&plates[plate_index].mutex);
                return NULL;
            }
        }
        ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
        if (!sd)
        {
            release_rus(rus_num);
            return NULL;
        }
        sd->plate_index = plate_index;
        sd->use_rus = 1;
        sd->shoot_delay = 0;
        sd->rus_num = rus_num;
        pthread_t t;
        if (pthread_create(&t, NULL, rus_shoot_thread, sd) != 0)
        {
            release_rus(rus_num);
            free(sd);
            return NULL;
        }
        pthread_detach(t);
        return NULL;
    }
    else
    {
        int rocket_distance = 570 - y;
        if (rocket_distance < 0)
            rocket_distance = 0;
        double rocket_time = (double)rocket_distance / 100.0;
        double sd_sec = time_to_center - rocket_time;
        shoot_delay = (int)round(sd_sec * 1000000.0);
        if (shoot_delay > 0)
        {
            ShootThreadData *sd = (ShootThreadData *)malloc(sizeof(ShootThreadData));
            if (!sd)
                return NULL;
            sd->plate_index = plate_index;
            sd->use_rus = 0;
            sd->shoot_delay = shoot_delay;
            sd->rus_num = -1;
            pthread_t t;
            if (pthread_create(&t, NULL, shoot_thread, sd) != 0)
            {
                free(sd);
                return NULL;
            }
            pthread_detach(t);
        }
        else
        {
            pthread_mutex_lock(&plates[plate_index].mutex);
            plates[plate_index].processed = 1;
            pthread_mutex_unlock(&plates[plate_index].mutex);
        }
        return NULL;
    }
}

void start_left_to_right_processing(int plate_index)
{
    int *p = (int *)malloc(sizeof(int));
    if (!p)
        return;
    *p = plate_index;
    pthread_t t;
    if (pthread_create(&t, NULL, process_plate_left_to_right_thread, p) != 0)
    {
        free(p);
        return;
    }
    pthread_detach(t);
}

void start_right_to_left_processing(int plate_index)
{
    int *p = (int *)malloc(sizeof(int));
    if (!p)
        return;
    *p = plate_index;
    pthread_t t;
    if (pthread_create(&t, NULL, process_plate_right_to_left_thread, p) != 0)
    {
        free(p);
        return;
    }
    pthread_detach(t);
}

const struct sigevent *locator_handler(void *area, int id)
{
    (void)area;
    (void)id;
    if (destroyed_plates >= MAX_DESTROYED)
        return NULL;
    int loc = getreg(RG_LOC);
    if (!loc)
        return NULL;
    int side = loc & 0xff;
    int code = (loc >> 8) & 0xff;
    int height = (loc >> 16) & 0xff;
    int idx = find_or_create_plate(height);
    if (idx < 0)
        return NULL;
    pthread_mutex_lock(&plates[idx].mutex);
    if (code == LOC1)
        plates[idx].loc1_time = get_time_us();
    else if (code == LOC2)
        plates[idx].loc2_time = get_time_us();
    else if (code == LOC3)
        plates[idx].loc3_time = get_time_us();
    else if (code == LOC4)
        plates[idx].loc4_time = get_time_us();
    if (plates[idx].loc1_time && plates[idx].loc2_time && plates[idx].loc3_time && plates[idx].loc4_time && !plates[idx].processed)
    {
        int dir = 0;
        if (plates[idx].loc1_time && plates[idx].loc2_time && !(plates[idx].loc3_time || plates[idx].loc4_time))
            dir = 1;
        if (plates[idx].loc3_time && plates[idx].loc4_time && !(plates[idx].loc1_time || plates[idx].loc2_time))
            dir = -1;
        if (dir == 1)
        {
            plates[idx].processed = 1;
            pthread_mutex_unlock(&plates[idx].mutex);
            start_left_to_right_processing(idx);
            return NULL;
        }
        else if (dir == -1)
        {
            plates[idx].processed = 1;
            pthread_mutex_unlock(&plates[idx].mutex);
            start_right_to_left_processing(idx);
            return NULL;
        }
    }
    pthread_mutex_unlock(&plates[idx].mutex);
    return NULL;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    plate_count = 0;
    pthread_mutex_init(&plates_mutex, NULL);
    for (int i = 0; i < PLATE_SLOTS; ++i)
    {
        plates[i].height = 0;
        plates[i].speed = 0;
        plates[i].direction = 0;
        plates[i].loc1_time = plates[i].loc2_time = plates[i].loc3_time = plates[i].loc4_time = 0;
        plates[i].processed = 1;
        pthread_mutex_init(&plates[i].mutex, NULL);
    }
    init_rus_array();
    int interrupt_id = InterruptAttach(LOC_INTR, locator_handler, NULL, 0, 0);
    if (interrupt_id < 0)
    {
        fprintf(stderr, "Warning: InterruptAttach returned %d\n", interrupt_id);
    }
    time_t start_time = time(NULL);
    int cleanup_counter = 0;
    while (destroyed_plates < MAX_DESTROYED && 1)
    {
        usleep(100000);
        cleanup_counter++;
        if (cleanup_counter >= 10)
        {
            cleanup_old_plates();
            cleanup_counter = 0;
        }
    }
    EndGame();
    return 0;
}
