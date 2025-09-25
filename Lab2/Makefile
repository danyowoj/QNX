# Makefile для сборки примеров VinGraph под QNX
# Имена программ соответствуют вашему скрипту:
# qcc -o draw_all draw_all.cpp -lvg
# qcc -o move_processes move_processes.cpp -lvg
# qcc -o closed_curve_shm closed_curve_shm.cpp -lvg -lm
# qcc -o closed_curve_threads closed_curve_threads.cpp -lvg -lm

# ==================== ПЕРЕМЕННЫЕ ====================
CC = qcc
CFLAGS = -Vgcc_ntox86 -Wall

# Библиотеки
VINGRAPH_LIB = -lvg
LDLIBS = $(VINGRAPH_LIB)
LIBM = -lm

# Целевые бинарники (имена как в скрипте)
TARGETS = draw_all move_processes closed_curve_shm closed_curve_threads

# Пути QNX (при необходимости настройте)
QNX_HOST = /usr/qnx650/host/qnx6/x86
QNX_TARGET = /usr/qnx650/target/qnx6

# Директории
BIN_DIR = bin
SRC_DIR = src

# ==================== НАСТРОЙКА ОКРУЖЕНИЯ ====================
export QNX_HOST := $(QNX_HOST)
export QNX_TARGET := $(QNX_TARGET)
export PATH := $(PATH):$(QNX_HOST)/usr/bin

# ==================== ЦЕЛЬ ПО УМОЛЧАНИЮ ====================
all: $(BIN_DIR) programs

# Создание директории для бинарников
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ==================== ИСТОЧНИКИ ====================
SRC_DRAW        = $(SRC_DIR)/draw_all.cpp
SRC_MOVE        = $(SRC_DIR)/move_processes.cpp
SRC_CLOSED_SHM  = $(SRC_DIR)/closed_curve_shm.cpp
SRC_CLOSED_THR  = $(SRC_DIR)/closed_curve_threads.cpp

# ==================== КОМПИЛЯЦИЯ ПРОГРАММ ====================
programs: $(addprefix $(BIN_DIR)/, $(TARGETS))

# draw_all
$(BIN_DIR)/draw_all: $(SRC_DRAW) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)
	@echo "Скомпилирован draw_all"

# move_processes
$(BIN_DIR)/move_processes: $(SRC_MOVE) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)
	@echo "Скомпилирован move_processes"

# closed_curve_shm (требует math)
$(BIN_DIR)/closed_curve_shm: $(SRC_CLOSED_SHM) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS) $(LIBM)
	@echo "Скомпилирован closed_curve_shm"

# closed_curve_threads (требует math)
$(BIN_DIR)/closed_curve_threads: $(SRC_CLOSED_THR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS) $(LIBM)
	@echo "Скомпилирован closed_curve_threads"

# ==================== ЗАПУСК ПРОГРАММ ====================
run_draw_all: $(BIN_DIR)/draw_all
	@echo "Запуск draw_all"
	$(BIN_DIR)/draw_all

run_move_processes: $(BIN_DIR)/move_processes
	@echo "Запуск move_processes"
	$(BIN_DIR)/move_processes

run_closed_curve_shm: $(BIN_DIR)/closed_curve_shm
	@echo "Запуск closed_curve_shm"
	$(BIN_DIR)/closed_curve_shm

run_closed_curve_threads: $(BIN_DIR)/closed_curve_threads
	@echo "Запуск closed_curve_threads"
	$(BIN_DIR)/closed_curve_threads

run_all: $(addprefix $(BIN_DIR)/, $(TARGETS))
	@echo "=== Последовательный запуск программ ==="
	@echo "Запуск draw_all..."
	$(BIN_DIR)/draw_all
	@echo "Запуск move_processes..."
	$(BIN_DIR)/move_processes
	@echo "Запуск closed_curve_shm..."
	$(BIN_DIR)/closed_curve_shm
	@echo "Запуск closed_curve_threads..."
	$(BIN_DIR)/closed_curve_threads

# ==================== ОТДЕЛЬНЫЕ ЦЕЛИ ====================
draw_all: $(BIN_DIR)/draw_all
move_processes: $(BIN_DIR)/move_processes
closed_curve_shm: $(BIN_DIR)/closed_curve_shm
closed_curve_threads: $(BIN_DIR)/closed_curve_threads

# ==================== ВСПОМОГАТЕЛЬНЫЕ ЦЕЛИ ====================
clean:
	rm -rf $(BIN_DIR)
	rm -f *.o core.* compile_errors.txt
	@echo "Очистка завершена"

help:
	@echo "Makefile для примеров VinGraph"
	@echo "  make all                 - собрать все программы"
	@echo "  make draw_all            - собрать draw_all"
	@echo "  make move_processes      - собрать move_processes"
	@echo "  make closed_curve_shm    - собрать closed_curve_shm"
	@echo "  make closed_curve_threads- собрать closed_curve_threads"
	@echo ""
	@echo "  make run_draw_all"
	@echo "  make run_move_processes"
	@echo "  make run_closed_curve_shm"
	@echo "  make run_closed_curve_threads"
	@echo "  make run_all"
	@echo ""
	@echo "  make clean               - удалить все собранные файлы и каталог bin"

.PHONY: all programs clean help  \
        run_draw_all run_move_processes run_closed_curve_shm run_closed_curve_threads run_all \
        draw_all move_processes closed_curve_shm closed_curve_threads
