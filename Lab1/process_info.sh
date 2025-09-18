#!/bin/sh
# Скрипт выводит информацию об указанном процессе

if [ $# -eq 0 ]; then
    echo "Использование: $0 <имя_процесса>"
    exit 1
fi

PROCESS_NAME=$1
echo "Информация о процессе '$PROCESS_NAME':"
pidin | grep "$PROCESS_NAME"
