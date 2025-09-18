#!/bin/sh
# Протокол выполнения задания по QNX

echo "======================================="
echo "Протокол выполнения задания"
echo "======================================="

echo "1. Определение типов файлов в /dev"
ls -l /dev/hd0 /dev/console /dev/ttyp0 /dev/shmem /dev/mem
echo ""

echo "2. Определение рабочего каталога при входе в систему"
echo "Рабочий каталог: $HOME"
echo "Определяется переменной HOME из /etc/passwd"
echo ""

echo "3. Создание каталога LAB1 и переход в него"
mkdir -p LAB1
cd LAB1
echo "Текущий каталог: $(pwd)"
echo ""

echo "4. Поиск файла services и его содержимое"
find / -name services -type f 2>/dev/null | head -1
echo "Содержимое файла:"
cat $(find / -name services -type f 2>/dev/null | head -1) | head -5
echo "..."
echo ""

echo "5. Количество скрытых файлов в домашнем каталоге"
ls -a ~ | grep "^\." | wc -l
echo ""

echo "6. Дерево подкаталогов в /boot"
find /boot -type d
echo "Количество файлов меньше 1К: $(find /boot -type f -size -1k | wc -l)"
echo "Количество исполняемых файлов: $(find /boot -type f -executable | wc -l)"
echo ""

echo "7. Количество жестких ссылок у /boot"
ls -ld /boot | awk '{print $2}'
echo "У каталогов минимальное количество ссылок = 2 (сам каталог и .)"
echo ""

echo "8. Создание файла в vi и права доступа"
echo "Создаем файл test.txt"
vi -c ":wq" test.txt << EOF
i
Тестовый текст

:wq
EOF
echo "Права доступа: $(ls -l test.txt)"
echo "Права определяются umask (по умолчанию 022)"
echo "Исправление: chmod 755 test.txt"
chmod 755 test.txt
echo ""

echo "9. Работа с копиями файлов"
mkdir subdir
cd subdir
for i in {1..10}; do cp ../test.txt file$i.txt; done
mv file1.txt file2.txt file3.txt ..
echo "Удаляем файлы с подтверждением:"
rm -i file4.txt file5.txt << EOF
y
y
EOF
echo "Проверка влияния флага w:"
chmod -w .
echo "Попытка удаления без права записи:"
rm file6.txt 2>&1
chmod +w .
echo ""

echo "10. Значения переменных среды"
echo "PATH=$PATH"
echo "LOGNAME=$LOGNAME"
echo "HOME=$HOME"
echo "HOSTNAME=$HOSTNAME"
echo "PWD=$PWD"
echo "RANDOM=$RANDOM"
echo "RANDOM меняется при каждом вызове"
echo ""

echo "11. Коды завершения команд"
ls /bin
echo "Код завершения: $?"
ls /pin 2>/dev/null
echo "Код завершения: $?"
echo ""

echo "12. Вывод содержимого /bin и /usr/bin в файл"
ls /bin > list.txt
ls /usr/bin >> list.txt
echo "Файл создан"
echo ""

echo "13. Подсчет файлов для удаления"
echo "Файлов g*: $(ls /usr/bin/g* 2>/dev/null | wc -l)"
echo "Файлов t??: $(ls /usr/bin/t?? 2>/dev/null | wc -l)"
echo ""

echo "14. Количество пользователей"
cat /etc/passwd | wc -l
echo ""

echo "15. Количество групп"
cat /etc/group | wc -l
echo ""

echo "16. Пользователи без пароля"
awk -F: '($2 == "" || $2 == "!") {print $1}' /etc/passwd
echo ""

echo "17-20. Изменение прав доступа к файлу"
echo "Исходные права: $(ls -l test.txt)"
chmod u-r test.txt
echo "После chmod u-r: $(ls -l test.txt)"
echo "Попытка чтения:"
cat test.txt 2>&1
chmod u+r test.txt

chmod o-r test.txt
echo "После chmod o-r: $(ls -l test.txt)"
echo "Попытка чтения другим пользователем:"
su -c "cat test.txt" nobody 2>&1
chmod o+r test.txt

chmod u-w test.txt
echo "После chmod u-w: $(ls -l test.txt)"
echo "Попытка записи:"
echo "test" >> test.txt 2>&1
chmod u+w test.txt

chmod o-w test.txt
echo "После chmod o-w: $(ls -l test.txt)"
echo "Попытка записи другим пользователем:"
su -c "echo 'test' >> test.txt" nobody 2>&1
chmod o+w test.txt
echo ""

echo "21. Доступ к домашнему каталогу"
chmod o+rx ~
echo "Права: $(ls -ld ~)"
chmod o-rwx ~
echo "После запрета: $(ls -ld ~)"
echo ""

echo "22. Доступ без изменения"
chmod o+rx ~
chmod o-w ~
echo "Права: $(ls -ld ~)"
echo "Попытка создания файла другим пользователем:"
su -c "touch ~/testfile" nobody 2>&1
echo ""

echo "23. Доступ только по известным именам"
chmod o-r ~/LAB1
chmod o+x ~/LAB1
echo "Права каталога: $(ls -ld ~/LAB1)"
echo "Попытка просмотра каталога:"
su -c "ls ~/LAB1" nobody 2>&1
echo "Попытка доступа к известному файлу:"
su -c "cat ~/LAB1/test.txt" nobody 2>&1
echo ""

echo "======================================="
echo "Протокол завершен"
echo "======================================="
