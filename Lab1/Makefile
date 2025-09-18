QNX_HOST=user@192.168.0.10
QNX_PATH=/home/user/lab1

upload:
	scp *.c $(QNX_HOST):$(QNX_PATH)/

compile:
	ssh $(QNX_HOST) "cd $(QNX_PATH) && qcc -o program program.c"

run:
	ssh $(QNX_HOST) "cd $(QNX_PATH) && ./program"

all: upload compile run
