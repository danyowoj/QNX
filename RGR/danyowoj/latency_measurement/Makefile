CC = qcc
CFLAGS = -O2 -Wall -Wc,-std=c99

all: controller worker

controller: controller.c
	$(CC) $(CFLAGS) -o controller controller.c -lm

worker: worker.c
	$(CC) $(CFLAGS) -o worker worker.c

clean:
	rm -f controller worker
