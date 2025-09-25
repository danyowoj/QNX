#include <vingraph.h>
#include <unistd.h>
#include <stdio.h>

// Простая вспомогательная функция для паузы
static void wait_enter()
{
    printf("Press Enter to exit...\n");
    getchar();
}

int main()
{
    ConnectGraph("VinGraph demo");

    Pixel(10, 10, RGB(255, 0, 0));
    Pixel(12, 10, RGB(0, 255, 0));
    Pixel(14, 10, RGB(0, 0, 255));

    Line(20, 20, 200, 20, RGB(255, 128, 0));

    Rect(20, 40, 120, 60, 8, RGB(100, 200, 255));

    Grid(160, 40, 200, 120, 4, 6, RGB(180, 180, 180));

    Ellipse(420, 80, 140, 80, RGB(200, 100, 200));

    tPoint polyline_pts[] = {{20, 140}, {60, 180}, {40, 220}, {100, 200}};
    Polyline(polyline_pts, 4, RGB(0, 200, 100));

    tPoint polygon_pts[] = {{200, 160}, {240, 200}, {220, 240}, {180, 220}};
    Polygon(polygon_pts, 4, RGB(120, 40, 200));

    Rect(420, 180, 60, 30, 4, RGB(255, 255, 100));
    Rect(420, 210, 60, 30, 4, RGB(255, 200, 50));
    Rect(420, 240, 60, 30, 4, RGB(255, 150, 20));

    Line(10, 300, 500, 300, RGB(200, 200, 200));

    wait_enter();
    CloseGraph();
    return 0;
}
