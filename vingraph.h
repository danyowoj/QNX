#ifndef VINGRAPH_H_INCLUDED
#define VINGRAPH_H_INCLUDED

struct tPoint { short int x, y; };

extern int  ConnectGraph (const char *name = 0);
extern void CloseGraph   ();
extern int  GetColor ();

extern int Pixel    (int x, int y, int color = GetColor(), int parent = 0);
extern int Line     (int x1, int y1, int x2, int y2, int color = GetColor(), int parent = 0);
extern int Polyline (const tPoint points [], int npoints, int color = GetColor(), int parent = 0);
extern int Polygon  (const tPoint points [], int npoints, int color = GetColor(), int parent = 0);
extern int Rect     (int x, int y, int w, int h, int roundness = 0, int color = GetColor(), int parent = 0);
extern int Grid     (int x, int y, int w, int h, int nrows, int ncols, int color = GetColor(), int parent = 0);
extern int Ellipse  (int x, int y, int w, int h, int color = GetColor(), int parent = 0);
extern int Arc      (int x, int y, int w, int h, int sangle, int eangle, int arctype = 0, int color = GetColor(), int parent = 0);
extern int Text     (int x, int y, const char *text, int color = GetColor(), int parent = 0);
extern int Picture  (int x = 0, int y = 0);
extern int Image32    (int x, int y, int w, int h, const void *image, int parent = 0);
extern int Image24    (int x, int y, int w, int h, const void *image, int parent = 0);
//extern void* Image32Container (int w, int h);
//extern int Image32    (int x, int y, const void *imcont, int parent = 0);
//extern int Image24    (int x, int y, const void *imcont, int parent = 0);

extern char InputChar ();

extern void Fill      (int ident = 0, int color = GetColor());
extern void SetColor  (int color);
extern void SetColor  (int ident, int color);
extern void MoveTo    (int x, int y, int ident = 0);
extern void Move      (int ident, int dx, int dy);
extern void Enlarge   (int ident, int dx, int dy);
extern void Enlarge   (int ident, int left, int up, int right, int down);
extern void EnlargeTo (const tPoint points [], int npoints, int ident = 0);
extern void EnlargeTo (int x, int y, int w, int h, int ident = 0);
extern void SetText   (int ident, const char *text);
extern void SetLineWidth (int ident, int width);

extern void Show   (int ident);
extern void Hide   (int ident);
extern void Delete (int ident);
extern void Clear  (int ident);

extern int    GetColor (int ident);
extern int    GetFill  (int ident);
extern tPoint GetPos   (int ident);
extern tPoint GetDim   (int ident);

#define RGB(r,g,b) ((((r << 8) + g) << 8) + b)
#endif
