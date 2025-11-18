#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <inttypes.h>

#define ITERATIONS 1000000

int main()
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    // CHILD PROCESS — SERVER
    if (pid == 0)
    {
        close(pipefd[0]); // child writes only

        int chid = ChannelCreate(0);
        if (chid == -1)
        {
            perror("ChannelCreate");
            exit(1);
        }

        // Send chid to parent
        if (write(pipefd[1], &chid, sizeof(chid)) != sizeof(chid))
        {
            perror("write(pipefd)");
            exit(1);
        }
        close(pipefd[1]);

        char msg, reply = 1;

        for (;;)
        {
            int rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);
            if (rcvid < 0)
            {
                perror("MsgReceive");
                exit(1);
            }
            MsgReply(rcvid, 0, &reply, sizeof(reply));
        }

        return 0;
    }

    // PARENT PROCESS — CLIENT
    close(pipefd[1]); // parent reads only
    int chid;
    if (read(pipefd[0], &chid, sizeof(chid)) != sizeof(chid))
    {
        perror("read(pipefd)");
        return 1;
    }
    close(pipefd[0]);

    // Attach to child's channel
    int coid = ConnectAttach(0, pid, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1)
    {
        perror("ConnectAttach");
        return 1;
    }

    uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    printf("CPU frequency: %" PRIu64 " Hz\n", cps);
    printf("Iterations: %d\n", ITERATIONS);

    uint64_t total_cycles = 0;
    char msg = 1;
    char reply;

    for (int i = 0; i < ITERATIONS; i++)
    {
        uint64_t t1 = ClockCycles();

        if (MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply)) == -1)
        {
            perror("MsgSend");
            return 1;
        }

        uint64_t t2 = ClockCycles();
        total_cycles += (t2 - t1);
    }

    double avg_cycles = (double)total_cycles / ITERATIONS;
    double ns_per_cycle = 1e9 / (double)cps;
    double avg_ns = avg_cycles * ns_per_cycle;

    printf("\nAverage CONTEXT SWITCH parent→child→parent:\n");
    printf("  %.2f cycles\n", avg_cycles);
    printf("  %.2f ns\n", avg_ns);

    printf("\nEstimated single switch:\n");
    printf("  %.2f ns\n", avg_ns / 2.0);

    return 0;
}
