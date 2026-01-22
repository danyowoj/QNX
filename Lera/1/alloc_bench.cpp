/*
! Компиляция и запуск:
qcc -O2 -Wall alloc_bench.cpp -o alloc_bench
on -C 0 ./alloc_bench 100000
*/

#include <iostream>
#include <cstdlib>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

using namespace std;

// Получение частоты CPU в Гц
uint64_t get_cpu_freq()
{
    return SYSPAGE_ENTRY(cpuinfo)->speed;
}

// Обёртка над ClockCycles()
inline uint64_t cc()
{
    return ClockCycles();
}

// Измерение частоты процессора
uint64_t measure_cpu_hz()
{
    const uint64_t ms = 50; // 50 ms

    uint64_t t1 = ClockCycles();
    delay(ms); // задержка 50 ms
    uint64_t t2 = ClockCycles();

    return (t2 - t1) * 1000 / ms; // Hz
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Использование: " << argv[0] << " <iter_count>\n";
        return 1;
    }

    const int ITER = atoi(argv[1]);
    const size_t ALLOC_SIZE = 1024; // байт

    uint64_t cpu_hz = measure_cpu_hz();
    cout << "CPU frequency: " << cpu_hz << " Hz\n";
    cout << "Iterations: " << ITER << "\n\n";

    uint64_t t1, t2;
    double sum_new = 0, sum_malloc = 0, sum_mmap = 0;

    //* ИЗМЕРЕНИЕ NEW / DELETE
    for (int i = 0; i < ITER; i++)
    {
        t1 = cc();
        char *p = new char[ALLOC_SIZE];
        t2 = cc();
        delete[] p;

        sum_new += (t2 - t1);
    }

    //* ИЗМЕРЕНИЕ MALLOC / FREE
    for (int i = 0; i < ITER; i++)
    {
        t1 = cc();
        char *p = (char *)malloc(ALLOC_SIZE);
        t2 = cc();
        free(p);

        sum_malloc += (t2 - t1);
    }

    //* ИЗМЕРЕНИЕ MMAP / MUNMAP
    for (int i = 0; i < ITER; i++)
    {

        t1 = cc();
        void *p = mmap(NULL,
                       ALLOC_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANON,
                       -1, 0);
        t2 = cc();

        munmap(p, ALLOC_SIZE);
        sum_mmap += (t2 - t1);
    }

    cout << "=== RESULTS (avg cycles) ===\n";
    cout << "new[]      : " << (sum_new / ITER) << " cycles\n";
    cout << "malloc()   : " << (sum_malloc / ITER) << " cycles\n";
    cout << "mmap()     : " << (sum_mmap / ITER) << " cycles\n\n";

    cout << "=== RESULTS (ns) ===\n";
    cout << "new[]      : " << (sum_new / ITER) * 1e9 / cpu_hz << " ns\n";
    cout << "malloc()   : " << (sum_malloc / ITER) * 1e9 / cpu_hz << " ns\n";
    cout << "mmap()     : " << (sum_mmap / ITER) * 1e9 / cpu_hz << " ns\n";

    return 0;
}
