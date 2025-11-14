/* worker.c
 *
 * Простой worker-процесс для эксперимента планирования.
 * Аргументы:
 *   argv[1] - уникальное имя worker, например "w1"
 *   argv[2] - имя очереди результатов (например "/mq_results")
 *
 * worker открывает:
 *   - очередь команд: "/mq_cmd_<name>"
 *   - семафор старта: "/sem_start_<name>"
 *   - очередь результатов: argv[2]
 *
 * Команда от контроллера: struct cmd_msg { int cmd; int priority; int iter; }
 *  cmd == 1 : set priority and prepare; then wait for sem_wait; when woken, send timestamp
 *
 * Использует ClockCycles() из <sys/neutrino.h>.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/neutrino.h>   /* ClockCycles() */
#include <sched.h>
#include <signal.h>

#define CMD_MQ_NAME_PREFIX "/mq_cmd_"
#define START_SEM_PREFIX "/sem_start_"
#define RESULT_MQ_DEFAULT "/mq_results"

struct cmd_msg {
    int cmd;       /* 1 == prepare+wait */
    int priority;  /* desired priority for the worker thread */
    int iter;      /* iteration number (for correlation) */
};

/* result message: send back to controller */
struct result_msg {
    pid_t pid;
    uint64_t t_start;   /* ClockCycles at wake */
    int iter;
    char name[32];
};

static volatile int keep_running = 1;

static void sigint_handler(int s) {
    (void)s;
    keep_running = 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <worker_name> <result_mq_name>\n", argv[0]);
        fprintf(stderr, "Example: %s w1 /mq_results\n", argv[0]);
        return 1;
    }

    char *name = argv[1];
    char result_mq_name[64];
    strncpy(result_mq_name, argv[2], sizeof(result_mq_name)-1);
    result_mq_name[sizeof(result_mq_name)-1] = '\0';

    char cmd_mq_name[64];
    char start_sem_name[64];
    snprintf(cmd_mq_name, sizeof(cmd_mq_name), "%s%s", CMD_MQ_NAME_PREFIX, name);
    snprintf(start_sem_name, sizeof(start_sem_name), "%s%s", START_SEM_PREFIX, name);

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    /* Open/create command mq (controller creates it before sending) */
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct cmd_msg);
    attr.mq_curmsgs = 0;

    mqd_t mq_cmd = mq_open(cmd_mq_name, O_RDONLY | O_CREAT, 0666, &attr);
    if (mq_cmd == (mqd_t)-1) {
        perror("mq_open cmd");
        return 1;
    }

    /* Open/create result mq to send timestamps */
    struct mq_attr attr_res;
    attr_res.mq_flags = 0;
    attr_res.mq_maxmsg = 100;
    attr_res.mq_msgsize = sizeof(struct result_msg);
    attr_res.mq_curmsgs = 0;

    mqd_t mq_res = mq_open(result_mq_name, O_WRONLY | O_CREAT, 0666, &attr_res);
    if (mq_res == (mqd_t)-1) {
        perror("mq_open result");
        mq_close(mq_cmd);
        return 1;
    }

    /* Open/create start semaphore */
    sem_t *sem_start = sem_open(start_sem_name, O_CREAT, 0666, 0);
    if (sem_start == SEM_FAILED) {
        perror("sem_open start");
        mq_close(mq_cmd);
        mq_close(mq_res);
        return 1;
    }

    printf("worker %s started (pid=%d). Waiting for commands on %s\n", name, getpid(), cmd_mq_name);
    fflush(stdout);

    while (keep_running) {
        struct cmd_msg cmd;
        ssize_t r = mq_receive(mq_cmd, (char*)&cmd, sizeof(cmd), NULL);
        if (r == -1) {
            if (errno == EINTR) continue;
            perror("mq_receive");
            break;
        }

        if (cmd.cmd == 0) {
            /* stop/exit */
            break;
        } else if (cmd.cmd == 1) {
            /* set priority for this thread (main thread) */
            int prio = cmd.priority;
            struct sched_param sp;
            sp.sched_priority = prio;

            /* Try to set SCHED_RR; fallback to SCHED_FIFO if needed */
            if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) != 0) {
                if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
                    /* fallback: try to set via setpriority (nice) */
                    /* Not fatal; continue */
                }
            }

            /* notify controller: ready */
            struct result_msg ready_msg;
            ready_msg.pid = getpid();
            ready_msg.t_start = ClockCycles(); /* timestamp of readiness (optional) */
            ready_msg.iter = cmd.iter;
            strncpy(ready_msg.name, name, sizeof(ready_msg.name)-1);
            ready_msg.name[sizeof(ready_msg.name)-1] = '\0';

            if (mq_send(mq_res, (char*)&ready_msg, sizeof(ready_msg), 0) == -1) {
                perror("mq_send ready");
            }

            /* Wait for start semaphore (controller will post) */
            if (sem_wait(sem_start) == -1) {
                if (errno == EINTR) continue;
                perror("sem_wait");
                break;
            }

            /* Immediately record ClockCycles and send it back */
            struct result_msg res;
            res.pid = getpid();
            res.t_start = ClockCycles();
            res.iter = cmd.iter;
            strncpy(res.name, name, sizeof(res.name)-1);
            res.name[sizeof(res.name)-1] = '\0';

            if (mq_send(mq_res, (char*)&res, sizeof(res), 0) == -1) {
                perror("mq_send result");
            }
        }
    }

    /* cleanup */
    sem_close(sem_start);
    mq_close(mq_cmd);
    mq_close(mq_res);

    /* do not unlink named objects here; controller may do cleanup */
    printf("worker %s exiting\n", name);
    return 0;
}
