# ==================== ПЕРЕМЕННЫЕ ====================
# Компилятор и флаги
CC = qcc
CFLAGS = -Vgcc_ntox86

# Целевые файлы
TARGETS = hello_center key_codes moving_char advanced_moving_char
SCRIPTS = show_params.sh process_info.sh compile_and_run.sh protocol.sh

# Пути QNX
QNX_HOST = /usr/qnx650/host/qnx6/x86
QNX_TARGET = /usr/qnx650/target/qnx6

# Директории
BIN_DIR = bin
SRC_DIR = .

# ==================== НАСТРОЙКА ОКРУЖЕНИЯ ====================
export QNX_HOST := $(QNX_HOST)
export QNX_TARGET := $(QNX_TARGET)
export PATH := $(PATH):$(QNX_HOST)/usr/bin

# ==================== ЦЕЛИ ПО УМОЛЧАНИЮ ====================
# Основная цель - создает bin directory и запускает все
all: $(BIN_DIR) scripts programs

# Создание директории для бинарных файлов
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ==================== 1. РАБОТА С КОМАНДНОЙ СТРОКОЙ ====================
protocol: $(BIN_DIR) $(SCRIPTS)
	chmod +x protocol.sh
	./protocol.sh

# ==================== 2. СОЗДАНИЕ ПРОСТЫХ СКРИПТОВ ====================
scripts: $(SCRIPTS)
	chmod +x $(SCRIPTS)
	# Копируем скрипты в bin directory
	cp $(SCRIPTS) $(BIN_DIR)/ && chmod +x $(BIN_DIR)/*.sh

show_params: $(BIN_DIR) scripts
	$(BIN_DIR)/show_params.sh param1 param2 param3

process_info: $(BIN_DIR) scripts
	$(BIN_DIR)/process_info.sh procnto

compile_run: $(BIN_DIR) scripts
	$(BIN_DIR)/compile_and_run.sh my_program.c

# ==================== 3. РАЗРАБОТКА ПРОГРАММ ====================
programs: $(addprefix $(BIN_DIR)/, $(TARGETS))

# Правило для компиляции программ в bin directory
$(BIN_DIR)/%: %.c $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Индивидуальные цели для каждой программы
$(BIN_DIR)/hello_center: hello_center.c $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR)/key_codes: key_codes.c $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR)/moving_char: moving_char.c $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR)/advanced_moving_char: advanced_moving_char.c $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# ==================== ЗАПУСК ПРОГРАММ ====================
run_hello: $(BIN_DIR)/hello_center
	$(BIN_DIR)/hello_center

run_keycodes: $(BIN_DIR)/key_codes
	$(BIN_DIR)/key_codes

run_moving: $(BIN_DIR)/moving_char
	$(BIN_DIR)/moving_char

run_advanced: $(BIN_DIR)/advanced_moving_char
	$(BIN_DIR)/advanced_moving_char -x 10 -y 5 -dx 1 -dy 1 -d 50000 -s @ -c 31

run_advanced_alt: $(BIN_DIR)/advanced_moving_char
	$(BIN_DIR)/advanced_moving_char -x 20 -y 10 -dx -1 -dy 1 -d 200000 -s "#" -c 32

# ==================== ВСПОМОГАТЕЛЬНЫЕ ЦЕЛИ ====================
clean:
	rm -rf $(BIN_DIR)
	rm -f *.o compile_errors.txt

clean_all: clean
	rm -f *~ \#*

# Показать помощь
help:
	@echo "Доступные команды:"
	@echo "  make all              - Создать bin directory, скрипты и программы"
	@echo "  make scripts          - Сделать скрипты исполняемыми и скопировать в bin/"
	@echo "  make programs         - Скомпилировать все программы в bin/"
	@echo "  make clean            - Очистить bin directory и временные файлы"
	@echo ""
	@echo "Отдельные цели:"
	@echo "  make protocol         - Запуск протокола из текущей директории"
	@echo "  make show_params      - Запуск скрипта параметров из bin/"
	@echo "  make process_info     - Запуск скрипта информации о процессе из bin/"
	@echo "  make compile_run      - Запуск скрипта компиляции из bin/"
	@echo ""
	@echo "Запуск программ из bin/:"
	@echo "  make run_hello        - Запуск hello_center"
	@echo "  make run_keycodes     - Запуск key_codes"
	@echo "  make run_moving       - Запуск moving_char"
	@echo "  make run_advanced     - Запуск advanced_moving_char с параметрами"
	@echo "  make run_advanced_alt - Альтернативный запуск"
	@echo ""
	@echo "Структура после выполнения:"
	@echo "  bin/hello_center"
	@echo "  bin/key_codes"
	@echo "  bin/moving_char"
	@echo "  bin/advanced_moving_char"
	@echo "  bin/*.sh"

# Файлы для которых не нужно проверять зависимости
.PHONY: all scripts programs clean clean_all help \
        protocol show_params process_info compile_run \
        run_hello run_keycodes run_moving run_advanced run_advanced_alt
