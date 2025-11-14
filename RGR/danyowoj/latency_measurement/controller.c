/* controller.c
 *
 * Контроллерный процесс:
 *  - Создаёт очередь результатов ("/mq_results")
 *  - Для каждого worker создаёт именованный cmd mq ("/mq_cmd_<name>") и стартовый семафор "/sem_start_<name>"
 *  - Для каждой итерации посылает команду "prepare+setprio" в каждую очередь команд
 *  - Ждёт подтверждений "ready" от всех workers (в виде сообщений в mq_results)
 *  - Делает ClockCycles() -> t_post и семафорные sem_post для всех workers
 *  - Получает от workers сообщения с t_start и считает latency = t_start - t_post
 *  - Собирает статистику и печатает
 *
 * Аргументы:
 *   argv[1] - cpu_freq_hz (integer) — частота CPU в Гц (для перевода тактов в ns)
 *   argv[2+] - имена workers (например "w1" "w2")
 *
 * Запуск:
 *   on -C 0 ./controller 2000000000 w1 w2
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <mqueue.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/neutrino.h>
#include <time.h>
#include <math.h>

#define CMD_MQ_NAME_PREFIX "/mq_cmd_"
#define START_SEM_PREFIX "/sem_start_"
#define RESULT_MQ_NAME "/mq_results"

struct cmd_msg {
    int cmd;
    int priority;
    int iter;
};

struct result_msg {
    pid_t pid;
    uint64_t t_start;
    int iter;
    char name[32];
};

static void perror_exit(const char *s) {
    perror(s);
    exit(1);
}

static double ns_from_cycles(uint64_t cycles, double cpu_hz) {
    return (double)cycles * (1e9 / cpu_hz);
}

/* simple statistics */
typedef struct {
    int count;
    double sum;
    double sumsq;
    double min;
    double max;
} stats_t;

static void stats_add(stats_t *s, double v) {
    if (s->count == 0) {
        s->min = s->max = v;
    } else {
        if (v < s->min) s->min = v;
        if (v > s->max) s->max = v;
    }
    s->count++;
    s->sum += v;
    s->sumsq += v*v;
}

static void stats_print(stats_t *s) {
    if (s->count == 0) {
        printf("  no samples\n");
        return;
    }
    double mean = s->sum / s->count;
    double var = (s->sumsq / s->count) - mean*mean;
    double sd = var > 0 ? sqrt(var) : 0.0;
    printf("  samples=%d mean=%.2f ns std=%.2f ns min=%.2f ns max=%.2f ns\n",
           s->count, mean, sd, s->min, s->max);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <cpu_freq_hz> <worker1> <worker2> [...]\n", argv[0]);
        fprintf(stderr, "Example: %s 2000000000 w1 w2\n", argv[0]);
        return 1;
    }

    double cpu_hz = atof(argv[1]);
    int nworkers = argc - 2;
    char **workers = &argv[2];

    printf("Controller: cpu_hz=%.0f, workers=%d\n", cpu_hz, nworkers);

    /* create result mq */
    struct mq_attr attr_res;
    attr_res.mq_flags = 0;
    attr_res.mq_maxmsg = 1000;
    attr_res.mq_msgsize = sizeof(struct result_msg);
    attr_res.mq_curmsgs = 0;

    mqd_t mq_res = mq_open(RESULT_MQ_NAME, O_RDONLY | O_CREAT, 0666, &attr_res);
    if (mq_res == (mqd_t)-1) perror_exit("mq_open result");

    /* For each worker create the cmd mq and start semaphore (empty) */
    mqd_t *mq_cmds = calloc(nworkers, sizeof(mqd_t));
    sem_t **sems = calloc(nworkers, sizeof(sem_t*));
    char namebuf[64];

    for (int i=0;i<nworkers;i++) {
        snprintf(namebuf, sizeof(namebuf), "%s%s", CMD_MQ_NAME_PREFIX, workers[i]);
        struct mq_attr attr_cmd;
        attr_cmd.mq_flags = 0;
        attr_cmd.mq_maxmsg = 10;
        attr_cmd.mq_msgsize = sizeof(struct cmd_msg);
        attr_cmd.mq_curmsgs = 0;
        /* create command queue */
        mq_unlink(namebuf); /* remove if exists */
        mq_cmds[i] = mq_open(namebuf, O_WRONLY | O_CREAT, 0666, &attr_cmd);
        if (mq_cmds[i] == (mqd_t)-1) perror_exit("mq_open cmd");

        snprintf(namebuf, sizeof(namebuf), "%s%s", START_SEM_PREFIX, workers[i]);
        sem_unlink(namebuf);
        sems[i] = sem_open(namebuf, O_CREAT, 0666, 0);
        if (sems[i] == SEM_FAILED) perror_exit("sem_open start");
    }

    /* Parameters of experiment */
    const int iterations = 1000;
    /* Example priority matrix: we will test several priority settings pairs.
       For simplicity here we do: worker0 priority = p0, worker1 priority = p1 */
    int test_pairs[][2] = {
        {10, 5},   /* w0 higher than w1 */
        {5, 10},   /* w1 higher than w0 */
        {10, 10},  /* equal high */
        {5, 5}     /* equal low */
    };
    int n_pairs = sizeof(test_pairs) / (2 * sizeof(int)); /* intentionally wrong? fix next line */

    n_pairs = 4; /* just 4 combos as above */

    /* For results accumulation per worker per pair */
    stats_t *stats = calloc(nworkers * n_pairs, sizeof(stats_t));

    /* Run experiments */
    for (int pair = 0; pair < n_pairs; pair++) {
        printf("\n=== Test pair %d: priorities:", pair);
        for (int w=0; w<nworkers; w++) {
            int p = test_pairs[pair][w];
            printf(" %s=%d", workers[w], p);
        }
        printf("\n");

        /* iterate */
        for (int iter = 0; iter < iterations; iter++) {
            /* 1) send prepare command to each worker with desired priority */
            for (int i=0;i<nworkers;i++) {
                struct cmd_msg cmd;
                cmd.cmd = 1;
                cmd.priority = test_pairs[pair][i];
                cmd.iter = iter;
                if (mq_send(mq_cmds[i], (char*)&cmd, sizeof(cmd), 0) == -1) {
                    perror_exit("mq_send cmd");
                }
            }

            /* 2) wait for 'ready' from each worker (via result mq) */
            int ready_count = 0;
            while (ready_count < nworkers) {
                struct result_msg rm;
                ssize_t r = mq_receive(mq_res, (char*)&rm, sizeof(rm), NULL);
                if (r == -1) {
                    if (errno == EINTR) continue;
                    perror_exit("mq_receive ready");
                }
                /* treat any incoming message before start as 'ready' */
                /* Could validate rm.iter but simple approach ok */
                ready_count++;
                /* Optionally log */
                /* printf("Ready from %s (pid=%d)\n", rm.name, rm.pid); */
            }

            /* 3) record t_post and release all semaphores */
            uint64_t t_post = ClockCycles();

            for (int i=0;i<nworkers;i++) {
                if (sem_post(sems[i]) == -1) perror_exit("sem_post");
            }

            /* 4) receive t_start from each worker and compute latency */
            int got = 0;
            while (got < nworkers) {
                struct result_msg rm;
                ssize_t r = mq_receive(mq_res, (char*)&rm, sizeof(rm), NULL);
                if (r == -1) {
                    if (errno == EINTR) continue;
                    perror_exit("mq_receive result");
                }
                /* compute latency */
                uint64_t latency_cycles = 0;
                if (rm.t_start >= t_post) latency_cycles = rm.t_start - t_post;
                else latency_cycles = 0; /* shouldn't happen on same CPU */

                double latency_ns = ns_from_cycles(latency_cycles, cpu_hz);

                /* find worker index */
                int wi = -1;
                for (int k=0;k<nworkers;k++) if (strcmp(workers[k], rm.name)==0) { wi=k; break; }
                if (wi == -1) wi = 0; /* fallback */
                stats_add(&stats[wi + pair*nworkers], latency_ns);
                got++;
            }
        } /* iterations loop */

        /* print intermediate stats for this pair */
        for (int w=0; w<nworkers; w++) {
            printf("Worker %s statistics:\n", workers[w]);
            stats_print(&stats[w + pair*nworkers]);
        }
    } /* pair loop */

    /* cleanup: send stop cmd to workers */
    for (int i=0;i<nworkers;i++) {
        struct cmd_msg cmd;
        cmd.cmd = 0;
        cmd.priority = 0;
        cmd.iter = -1;
        mq_send(mq_cmds[i], (char*)&cmd, sizeof(cmd), 0);
    }

    /* close and unlink objects */
    for (int i=0;i<nworkers;i++) {
        mq_close(mq_cmds[i]);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s", CMD_MQ_NAME_PREFIX, workers[i]);
        mq_unlink(buf);

        char sbuf[64];
        snprintf(sbuf, sizeof(sbuf), "%s%s", START_SEM_PREFIX, workers[i]);
        sem_close(sems[i]);
        sem_unlink(sbuf);
    }

    mq_close(mq_res);
    mq_unlink(RESULT_MQ_NAME);

    printf("\nExperiment finished. Cleaned up.\n");
    return 0;
}
