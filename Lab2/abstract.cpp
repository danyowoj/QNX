#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <vingraph.h>

static int wait_for_any_key()
{
    struct termios oldt, newt;
    int ch;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = std::getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    
    return ch;
}

int main()
{
    ConnectGraph("VinGraph — Abstract Painting");

    const int W = 800, H = 600;

    // фон
    int bg = Rect(0, 0, W, H, 0, RGB(20, 20, 30));
    Show(bg);

    // сетка
    int grid = Grid(20, 20, W-40, H-40, 12, 16, RGB(30, 30, 40));
    Show(grid);

    // три линии разной ширины и цветов
    int l1 = Line(50, 50, 750, 150, RGB(200,30,60)); 
    SetLineWidth(l1, 3); 
    Show(l1);

    int l2 = Line(60, 140, 740, 520, RGB(30,200,140));
    SetLineWidth(l2, 6);
    Show(l2);

    int l3 = Line(400, 10, 400, 590, RGB(120,60,220));
    SetLineWidth(l3, 2);
    Show(l3);

    // точечный шум
    for (int i = 0; i < 600; i++)
    {
        int x = 0 + (std::rand() % 800);
        int y = 0 + (std::rand() % 600);
        Pixel(x, y, RGB(std::rand()%256, std::rand()%256, std::rand()%256));
    }

    // полилиния
    tPoint zig[9];
    for (int i = 0; i < 9; i++)
    {
        zig[i].x = 60 + i*80;
        zig[i].y = 220 + ((i%2)!= 0 ? -60 : 60);
    }
    int pl = Polyline(zig, 9, RGB(255, 200, 40));
    SetLineWidth(pl, 4);
    Show(pl);

    // полигон
    tPoint poly[6] = {
        {200, 300},
        {320, 240},
        {440, 300},
        {520, 420},
        {360, 520},
        {180, 420}};
    int pol = Polygon(poly, 6, RGB(200, 80, 180));
    Show(pol);

    // скругленный прямоугольник
    int r1 = Rect(100, 60, 140, 80, 20, RGB(240,180,60)); 
    Show(r1);
    
    int r2 = Rect(620, 420, 140, 120, 0, RGB(60,180,240)); 
    Show(r2);

    // элипсы
    int e1 = Ellipse(500, 150, 220, 120, RGB(180,240,100)); 
    Show(e1);

    int e2 = Ellipse(260, 420, 180, 120, RGB(240,120,80)); 
    Show(e2);

    // рамка
    int frame = Rect(8, 8, W-16, H-16, 12, RGB(200,200,210)); 
    SetLineWidth(frame, 3); 
    Show(frame);

    std::cout << "\nPress any key to exit..." << std::endl;
    wait_for_any_key();

    Delete(bg);
    Delete(grid);
    Delete(l1); Delete(l2); Delete(l3);
    Delete(pl);

    CloseGraph();
    return 0;
}
