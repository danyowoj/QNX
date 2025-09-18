# TODO
- [x] разобраться с `compile_and_run`
- [x] добавить инструкцию для блока `3. Разработка программ`
- [x] переделать `Makefile`

# Lab1

## 1. Работа с командной строкой 
Для запуска:

```
chmod +x protocol.sh
./protocol.sh
```

## 2. Создание простых скриптов
Сделать скрипты исполняемыми
```
chmod +x show_params.sh process_info.sh compile_and_run.sh
```
Скрипт вывода параметров `./show_params.sh param1 param2 param3`

Скрипт информации о процессе `./process_info.sh procnto # или другое имя процесса`

Скрипт компиляции и запуска `./compile_and_run.sh my_program.c`

## 3. Разработка программ

Компиляция программ:
```
qcc -o hello_center hello_center.c
qcc -o key_codes key_codes.c
qcc -o moving_char moving_char.c
qcc -o advanced_moving_char advanced_moving_char.c
```

Вывода HELLO в центре экрана `./hello_center`

Определение кодов клавиш `./key_codes`

Движущийся символ `./moving_char`

Движущийся символ с параметрами (пример) `./advanced_moving_char -x 10 -y 5 -dx 1 -dy 1 -d 50000 -s @ -c 31`
