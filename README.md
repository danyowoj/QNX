# TODO
- [x] разобраться с `compile_and_run`
- [x] добавить инструкцию для блока `3. Разработка программ`
- [x] переделать `Makefile`

# Lab1

## 1. Работа с командной строкой 
Доступные команды:
- make all              - Создать bin directory, скрипты и программы
- make scripts          - Сделать скрипты исполняемыми и скопировать в bin/
- make programs         - Скомпилировать все программы в bin/
- make clean            - Очистить bin directory и временные файлы

Отдельные цели:
- make protocol         - Запуск протокола из текущей директории
- make show_params      - Запуск скрипта параметров из bin/
- make process_info     - Запуск скрипта информации о процессе из bin/
- make compile_run      - Запуск скрипта компиляции из bin/

Запуск программ из bin/:
- make run_hello        - Запуск hello_center
- make run_keycodes     - Запуск key_codes
- make run_moving       - Запуск moving_char
- make run_advanced     - Запуск advanced_moving_char с параметрами
- make run_advanced_alt - Альтернативный запуск

Структура после выполнения:
- bin/hello_center
- bin/key_codes
- bin/moving_char
- bin/advanced_moving_char
- bin/*.sh

# Lab 2
