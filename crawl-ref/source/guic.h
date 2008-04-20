/*
 *  File:       guic-win.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#ifdef USE_X11
#include <X11/Xlib.h>
#include <X11/X.h>
bool GuicInit(Display **d, int *s);
void GuicDeinit();

#elif defined(WIN32TILES)
#include <windows.h>
#include <commdlg.h>
bool GuicInit(HINSTANCE h, int nCmdShow);
void GuicDeinit();

#elif defined(SOME_OS)
#include <some-headers.h>
bool GuicInit(some args);
#endif

#include <vector>

/*
 * Internal Image types
 */

#ifdef USE_X11
/*********** X11 ********/
typedef XImage *img_type;
#define ImgWidth(img) (img->width)
#define ImgHeight(img) (img->height)

#elif defined(WIN32TILES)
/********** Windows *****/
// struct for DIB info
typedef struct dib_pack
{
    LPBITMAPINFO       pDib     ;
    HBITMAP            hDib     ;
    HDC                hDC      ;
    LPBYTE             pDibBits ;
    LPBYTE             pDibZero ;
    int                Width    ;
    int                Height   ;
} dib_pack;
typedef dib_pack *img_type;
#define ImgWidth(img) (img->Width)
#define ImgHeight(img) (img->Height)

#elif defined(SOME_OS)
typedef sometype *img_type;
#define ImgWidth(img) (img->x)
#define ImgHeight(img) (img->y)
#endif

// Image overlay/copy between internal images,
// implemented in winclass-*.cc 
void ImgCopy(img_type src,  int sx, int sy, int wx, int wy,
             img_type dest, int dx, int dy, int copy);
// hilight rim color #010101 to magenta
void ImgCopyH(img_type src,  int sx, int sy, int wx, int wy,
              img_type dest, int dx, int dy, int copy);
// maskout by char array mask
void ImgCopyMasked(img_type src,  int sx, int sy, int wx, int wy,
                   img_type dest, int dx, int dy, char *mask);
// maskout+hilight
void ImgCopyMaskedH(img_type src,  int sx, int sy, int wx, int wy,
                    img_type dest, int dx, int dy, char *mask);

// create internal buffer (not assosiated to any window)
img_type ImgCreateSimple(int wx, int wy);
// create it from file
img_type ImgLoadFile(const char *name);

// destroy
void ImgDestroy(img_type img);
// clear by transparent color
void ImgClear(img_type img);
// if it is pix_transparent #476c6c
bool ImgIsTransparentAt(img_type img, int x, int y);
void ImgSetTransparentPix(img_type img);

/*
 * Windows and internal regions (text, dungeon, map, etc)
 */

class WinClass
{
    public:
    int ox;  //Offset x in dots
    int oy;  //Offset y in dots

    int wx; //width in dots
    int wy; //height in dots

    std::vector<class RegionClass *> regions;
    std::vector<int> layers;

    int active_layer;

    // Pointer to the window
#ifdef USE_X11
    Window win;
#elif defined(WIN32TILES)
    HWND hWnd;
#elif defined(SOME_OS)
    somewindowtype win;
#endif

    WinClass();

    void SysInit();
    void SysDeinit();

    ~WinClass();

    // fillout with black: Sys dep
    void clear();
    // create: Sys dep
#ifdef USE_X11
    void create(char *name);
#elif defined(WIN32TILES)
    BOOL create(const char *name);
#elif defined(SOME_OS)
    void create(some args);
#endif

    void resize(int wx, int wy);
    void resize();
    void move(int ox, int oy);
    void move();

    // place Regions inside it
    void placeRegion(class RegionClass *r, int layer, 
                     class RegionClass *neighbor,    
                     int pflag,
		     int margin_top = 0, int margin_left = 0,
                     int margin_bottom = 0, int margin_right = 0);

    void placeRegion(class RegionClass *r, int layer, 
                      int x, int y,
		      int margin_top = 0, int margin_left = 0,
                      int margin_bottom = 0, int margin_right = 0);

    void removeRegion(class RegionClass *r);

    // fillout a rectangle
    void fillrect(int left, int right, int top, int bottom, int color);

    // redraw for exposure, etc
    void redraw(int x1, int y1, int x2, int y2);
    void redraw();
};

class RegionClass
{
    public:

    WinClass *win;
    int layer;

    // Geometry
    // <-----------------wx----------------------->
    // sx     ox                                ex
    // |margin| text/tile area            |margin|

    int ox;  //Offset x in dots
    int oy;  //Offset y in dots

    int dx;  //unit width
    int dy;  //unit height

    int mx;  //window width in dx
    int my;  //window height in dy

    int wx; //width in dots = dx*mx + margins
    int wy; //height in dots = dy*my + margins

    int sx; //Left edge pos
    int sy; //Top edge pos
    int ex; //right edge pos
    int ey; //bottom edge pos

    bool flag;// use or no

    int id; // for general purpose

    // pointer to internal backup image buffer
    // used for redraw and supplessing flicker
    img_type backbuf;
#ifdef WIN32TILES
    static void set_std_palette(RGBQUAD *pPal);
    static RGBQUAD std_palette[256];
    void init_backbuf(RGBQUAD *pPal = NULL, int ncol = 0);
    bool dos_char;
#else
    void init_backbuf();
#endif
    void resize_backbuf();

    // font-related 
    int fx; // font height and width (can differ from dx, dy)
    int fy;
#ifdef USE_X11
    int asc; //font ascent
  #ifdef JP
    XFontSet font; //fontset
  #else
    XFontStruct *font;
  #endif
    void init_font(const char *name);
#elif defined(WIN32TILES)
    HFONT font;
    void init_font(const char *name, int height);
    void change_font(const char *name, int height);
#elif defined(SOME_OS)
    sometype font;
    void init_font(some args);
#endif

    bool font_copied;

    void copy_font(RegionClass *r);

    // init/deinit
    RegionClass();
    virtual ~RegionClass();

    // system-dependent init/deinit
    void SysInit();
    void SysDeinit();

    // Sys indep
    bool is_active();
    void make_active();


    //Sys indep
    // convert mouse point into logical position
    virtual bool mouse_pos(int mouse_x, int mouse_y, int *cx, int *cy);

    virtual bool convert_redraw_rect(int x1, int y1, int x2, int y2, 
                      int *rx1, int *ry1, int *rx2, int *ry2);

    virtual void redraw(int x1, int y1, int x2, int y2);
    virtual void redraw();
    void sys_flush();

    virtual void fillrect(int left, int right, int top, int bottom, int color);
    virtual void framerect(int left, int right, int top, int bottom, int color);

    virtual void clear();
};


class TextRegionClass :public RegionClass
{
    public:
    // init/deinit
    TextRegionClass(int x, int y , int cx, int cy);
    ~TextRegionClass();

    // os dependent init/deinit
    void SysInit(int x, int y, int cx, int cy);
    void SysDeinit();

    // where now printing? what color?
    static int print_x;
    static int print_y;
    static int text_col;
    // which region now printing?
    static class TextRegionClass *text_mode;
    // display cursor? where is the cursor now?
    static int cursor_flag;
    static class TextRegionClass *cursor_region;
    static int cursor_x;
    static int cursor_y;

    // class methods
    static void cgotoxy(int x, int y);
    static int wherex();
    static int wherey();
    //static int get_number_of_lines(void);
    static void _setcursortype(int curstype);
    static void textbackground(int bg);
    static void textcolor(int col);

    // Object's method
    void clear_to_end_of_line(void);
    void clear_to_end_of_screen(void);
    void putch(unsigned char chr);
    void writeWChar(unsigned char *ch);

    unsigned char *cbuf; //text backup
    unsigned char *abuf; //textcolor backup

    int cx_ofs; //cursor x offset
    int cy_ofs; //cursor y offset

    void addstr(char *buffer);
    void addstr_aux(char *buffer, int len);
    void adjust_region(int *x1, int *x2, int y);
    void scroll();
    //bool mouse_pos(int mouse_x, int mouse_y, int *cx, int *cy);

    //Sys dep
    void draw_string(int x, int y, unsigned char *buf, int len, int col);
    void draw_cursor(int x, int y, int width);
    void draw_cursor(int x, int y);
    void erase_cursor();
    void clear();
    void init_backbuf();
    void redraw(int x1, int y1, int x2, int y2);
    void resize(int x, int y);
};

class TileRegionClass :public RegionClass
{
    public:
    bool force_redraw;

    void DrawPanel(int left, int top, int width, int height);
    void fillrect(int left, int right, int top, int bottom, int color);
    void framerect(int left, int right, int top, int bottom, int color);

    bool mouse_pos(int mouse_x, int mouse_y, int *cx, int *cy);
    virtual bool convert_redraw_rect(int x1, int y1, int x2, int y2, 
                      int *rx1, int *ry1, int *rx2, int *ry2);
    void redraw(int x1, int y1, int x2, int y2);
    void redraw();
    void clear();

    //Sys dep
    void resize(int x, int y, int dx, int dy);

#ifdef WIN32TILES
    void init_backbuf(RGBQUAD *pPal = NULL);
#else
    void init_backbuf();
#endif
    void resize_backbuf();

    TileRegionClass(int mx0, int my0, int dx0, int dy0);

    void SysInit(int mx0, int my0, int dx0, int dy0);
    void SysDeinit();

    ~TileRegionClass();
};

class MapRegionClass  :public RegionClass
{
    public:
    int mx2;
    int my2;
    int x_margin;
    int y_margin;
    int marker_length;
    unsigned char *mbuf;
    bool force_redraw;
    bool mouse_pos(int mouse_x, int mouse_y, int *cx, int *cy);
    void draw_data(unsigned char *buf);
    void redraw(int x1, int y1, int x2, int y2);
    void redraw();
    void clear();

    //Sys dep
    void init_backbuf();
    void resize_backbuf();
    void resize(int mx0, int my0, int dx0, int dy0);

    void set_col(int col, int x, int y);
    int get_col(int x, int y);

    MapRegionClass(int x, int y, int o_x, int o_y, int marker_length);

    void SysInit(int x, int y, int o_x, int o_y);
    void SysDeinit();

    ~MapRegionClass();
};

#define PLACE_RIGHT 0
#define PLACE_BOTTOM 1
#define PLACE_FORCE 2

// Graphics Colors
#define PIX_BLACK 0
#define PIX_BLUE 1
#define PIX_GREEN 2
#define PIX_CYAN 3
#define PIX_RED 4
#define PIX_MAGENTA 5
#define PIX_BROWN 6
#define PIX_LIGHTGREY 7
#define PIX_DARKGREY 8
#define PIX_LIGHTBLUE 9
#define PIX_LIGHTGREEN 10
#define PIX_LIGHTCYAN 11
#define PIX_LIGHTRED 12
#define PIX_LIGHTMAGENTA 13
#define PIX_YELLOW 14
#define PIX_WHITE 15
#define MAX_TERM_COL 16


#define MAP_BLACK  0
#define MAP_DKGREY 1
#define MAP_MDGREY 2
#define MAP_LTGREY 3
#define MAP_WHITE  4

#define MAP_BLUE   5
#define MAP_LTBLUE 6
#define MAP_DKBLUE 7

#define MAP_GREEN    8
#define MAP_LTGREEN  9
#define MAP_DKGREEN 10

#define MAP_CYAN    11
#define MAP_LTCYAN  12
#define MAP_DKCYAN  13

#define MAP_RED     14
#define MAP_LTRED   15
#define MAP_DKRED   16

#define MAP_MAGENTA   17
#define MAP_LTMAGENTA 18
#define MAP_DKMAGENTA 19

#define MAP_YELLOW   20
#define MAP_LTYELLOW 21
#define MAP_DKYELLOW 22

#define MAP_BROWN    23

#define MAX_MAP_COL 24


extern const int term_colors[MAX_TERM_COL][3];
extern const int map_colors[MAX_MAP_COL][3];

