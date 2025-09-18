#!/bin/sh
# Скрипт компилирует и запускает программу

if [ $# -eq 0 ]; then
    echo "Использование: $0 <имя_файла.c>"
    exit 1
fi

SOURCE_FILE=$1
EXECUTABLE=$(basename "$SOURCE_FILE" .c)

# Компиляция программы
echo "Компиляция $SOURCE_FILE..."
cc -o "$EXECUTABLE" "$SOURCE_FILE" 2> compile_errors.txt

# Проверка на ошибки компиляции
if [ $? -eq 0 ]; then
    echo "Компиляция успешна. Запуск программы..."
    rm -f compile_errors.txt
    ./"$EXECUTABLE"
else
    echo "Обнаружены ошибки компиляции:"
    cat compile_errors.txt
    echo "Запуск редактора для исправления ошибок..."
    vi "$SOURCE_FILE"
fi
