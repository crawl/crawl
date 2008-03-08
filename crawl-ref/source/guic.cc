/*
 *  File:       guic-win.cc
 *  Summary:    1) Image manipulation routines
 *              2) WinClass and RegionClass system independent imprementaions
 *              see guic-*.cc for  system dependent implementations
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#include "AppHdr.h"
#include "debug.h"
#include "guic.h"

#ifdef WIN32TILES
#else
#define dos_char false;
#endif

int TextRegionClass::print_x;
int TextRegionClass::print_y;
TextRegionClass *TextRegionClass::text_mode = NULL;
int TextRegionClass::text_col = 0;

TextRegionClass *TextRegionClass::cursor_region= NULL;
int TextRegionClass::cursor_flag = 0;
int TextRegionClass::cursor_x;
int TextRegionClass::cursor_y;

// more logical color naming
const int map_colors[MAX_MAP_COL][3]=
{
    {  0,   0,   0}, // BLACK
    {128, 128, 128}, // DKGREY
    {160, 160, 160}, // MDGREY
    {192, 192, 192}, // LTGREY
    {255, 255, 255}, // WHITE

    {  0,  64, 255}, // BLUE  (actually cyan-blue)
    {128, 128, 255}, // LTBLUE
    {  0,  32, 128}, // DKBLUE (maybe too dark)

    {  0, 255,   0}, // GREEN
    {128, 255, 128}, // LTGREEN
    {  0, 128,   0}, // DKGREEN

    {  0, 255, 255}, // CYAN
    { 64, 255, 255}, // LTCYAN (maybe too pale)
    {  0, 128, 128}, // DKCYAN

    {255,   0,   0}, // RED
    {255, 128, 128}, // LTRED (actually pink)
    {128,   0,   0}, // DKRED

    {192,   0, 255}, // MAGENTA (actually blue-magenta)
    {255, 128, 255}, // LTMAGENTA
    { 96,   0, 128}, // DKMAGENTA

    {255, 255,   0}, // YELLOW
    {255, 255,  64}, // LTYELLOW (maybe too pale)
    {128, 128,   0},  // DKYELLOW

    {165,  91,   0},  // BROWN
};

const int term_colors[MAX_TERM_COL][3]=
{
    {  0,   0,   0}, // BLACK
    {  0,  82, 255}, // BLUE
    {100, 185,  70}, // GREEN
    {  0, 180, 180}, // CYAN
    {255,  48,   0}, // RED
    {238,  92, 238}, // MAGENTA
    {165,  91,   0}, // BROWN
    {162, 162, 162}, // LIGHTGREY
    { 82,  82,  82}, // DARKGREY
    { 82, 102, 255}, // LIGHTBLUE
    { 82, 255,  82}, // LIGHTGREEN
    { 82, 255, 255}, // LIGHTCYAN
    {255,  82,  82}, // LIGHTRED
    {255,  82, 255}, // LIGHTMAGENTA
    {255, 255,  82}, // YELLOW
    {255, 255, 255}  // WHITE
};

WinClass::WinClass()
{
    // Minimum;
    wx = 10;
    wy = 10;

    ox = 0;
    oy = 0;
    SysInit();
}

WinClass::~WinClass()
{
    SysDeinit();
    regions.clear();
    layers.clear();
}

void WinClass::placeRegion(RegionClass *r, int layer0,
     int x, int y,
     int margin_top, int margin_left,
     int margin_bottom, int margin_right)
{
    if (r->win == NULL)
    {
        regions.push_back(r);
        layers.push_back(layer0);
    }

    r->win = this;
    r->layer = layer0;
    r->flag = true;

    r->sx = x;
    r->sy = y;

    r->ox = r->sx + margin_left;
    r->oy = r->sy + margin_top;
    r->wx = r->dx * r->mx + margin_left + margin_right;
    r->wy = r->dy * r->my + margin_top + margin_bottom;
    r->ex = r->sx + r->wx;
    r->ey = r->sy + r->wy;
    if (r->ex > wx) wx = r->ex;
    if (r->ey > wy) wy = r->ey;
}

void WinClass::placeRegion(RegionClass *r, int layer0,
     RegionClass *neighbor, int pflag,
     int margin_top, int margin_left,
     int margin_bottom, int margin_right)
{
    int sx0 =0;
    int sy0 =0;
    int ex0 =0;
    int ey0 =0;

    int x = 0;
    int y = 0;

    if (neighbor!=NULL)
    {
        sx0 = neighbor->sx;
        sy0 = neighbor->sy;
        ex0 = neighbor->ex;
        ey0 = neighbor->ey;
    }

    if (pflag == PLACE_RIGHT)
    {
        x = ex0;
        y = sy0;
    }
    else
    {
        x = sx0;
        y = ey0;
    }
    placeRegion(r, layer0, x, y, margin_top, margin_left,
     margin_bottom, margin_right);

}

void WinClass::removeRegion(RegionClass *r)
{
    for (unsigned int i = 0; i < regions.size(); i++)
    {
        if (regions[i] == r)
        {
            for (unsigned int j = i + 1; j < regions.size(); j++)
            {
                regions[j-1] = regions[j];
                layers[j-1] = layers[j];
            }

            regions.pop_back();
            layers.pop_back();

            return;
        }
    }
}

void WinClass::redraw(int x1, int y1, int x2, int y2)
{
    std::vector <RegionClass *>::iterator r;
    int cx1, cx2, cy1, cy2;
    for (r = regions.begin();r != regions.end();r++) 
    {
        if (!(*r)->is_active()) continue;
        if( (*r)->convert_redraw_rect(x1, y1, x2, y2, &cx1, &cy1, &cx2, &cy2))
        {
            (*r)->redraw(cx1, cy1, cx2, cy2);
        }
    }
}

void WinClass::redraw()
{
    redraw(0, 0, wx-1, wy-1);
}

void WinClass::move(int ox0, int oy0)
{
    ox = ox0;
    oy = oy0;
    move(); // system dependent
}

void WinClass::resize(int wx0, int wy0)
{
    if (wx0>0) wx = wx0;
    if (wy0>0) wy = wy0;
    resize(); // system dependent
}

RegionClass::RegionClass()
{
    flag = false;
    win = NULL;
    backbuf = NULL;
    SysInit();
    ox = oy = 0;
    dx = dy = 1;
    font_copied = false;
    id = 0;
}

RegionClass::~RegionClass()
{
    SysDeinit();
    if (backbuf != NULL)
        ImgDestroy(backbuf);
}

void TextRegionClass::resize(int x, int y)
{
    int i;
    free(cbuf);
    free(abuf);
    cbuf = (unsigned char *)malloc(x*y);
    abuf = (unsigned char *)malloc(x*y);
    for (i=0; i<x*y; i++)
    {
        cbuf[i]=' ';
        abuf[i]=0;
    }
    mx = x;
    my = y;
}

TextRegionClass::TextRegionClass(int x, int y, int cx, int cy)
{
    cbuf = NULL;
    abuf = NULL;
    resize(x, y);

    // Cursor Offset
    cx_ofs = cx;
    cy_ofs = cy;

    SysInit(x, y, cx, cy);
}

TextRegionClass::~TextRegionClass()
{
    SysDeinit();
    free(cbuf);
    free(abuf);
}

TileRegionClass::TileRegionClass(int mx0, int my0, int dx0, int dy0)
{
    // Unit size
    dx = dx0;
    dy = dy0;

    mx = mx0;
    my = my0;
    force_redraw = false;

    SysInit(mx0, my0, dx0, dy0);
}

TileRegionClass::~TileRegionClass()
{
    SysDeinit();
}

void TileRegionClass::resize(int mx0, int my0, int dx0, int dy0)
{
    if (mx0 != 0) mx = mx0;
    if (my0 != 0) my = my0;
    if (dx0 != 0) dx = dx0;
    if (dy0 != 0) dy = dy0;
}

MapRegionClass::MapRegionClass(int x, int y, int o_x, int o_y, bool iso)
{
    int i;

    mx2 = x;
    my2 = y;
    mx = mx2;
    my = my2;

    mbuf = (unsigned char *)malloc(mx2*my2);

    for (i=0; i<mx2*my2; i++)
    {
        mbuf[i]=0;
    }
    x_margin = o_x;
    y_margin = o_y;
    force_redraw = false;

    SysInit(x, y, o_x, o_y);
}

MapRegionClass::~MapRegionClass()
{
    SysDeinit();
    free(mbuf);
}

void MapRegionClass::resize(int mx0, int my0, int dx0, int dy0)
{
    if (mx0 != 0) mx2 = mx0;
    if (my0 != 0) my2 = my0;
    if (dx0 != 0) dx = dx0;
    if (dy0 != 0) dy = dy0;
    if (mx0 != 0 || my0 != 0)
    {
        int i;
        free(mbuf);
        mbuf = (unsigned char *)malloc(mx2*my2);
        for (i=0; i<mx2*my2; i++) mbuf[i]=0;
    }
}

/*------------------------------------------*/

bool RegionClass::is_active()
{
    if (!flag) return false;
    if (win->active_layer == layer)
        return true;
    else return false;
}

void RegionClass::make_active()
{
    if (!flag) return;
    win->active_layer = layer;
} 

void RegionClass::redraw(int x1, int y1, int x2, int y2)
{
}

void RegionClass::redraw()
{
    redraw(0, 0, mx-1, my-1);
}

void MapRegionClass::redraw()
{
    redraw(0, 0, mx-1, my-1);
}

void TileRegionClass::redraw()
{
    redraw(0, 0, mx*dx-1, my*dy-1);
}

void MapRegionClass::set_col(int col, int x, int y)
{
    mbuf[x + y * mx2] = col;
}

int MapRegionClass::get_col(int x, int y)
{
    return mbuf[x + y * mx2];
}

/*------------------------------------------*/
bool RegionClass::convert_redraw_rect(int x1, int y1, int x2, int y2, 
                  int *rx1, int *ry1, int *rx2, int *ry2)
{
    int cx1 = x1-ox;
    int cy1 = y1-oy;
    int cx2 = x2-ox;
    int cy2 = y2-oy;

    if ( (cx2 < 0) || (cy2 < 0) || (cx1 >= dx*mx) || (cy1 >=dy*my))
        return false;

    cx1 /= dx;
    cy1 /= dy;
    cx2 /= dx;
    cy2 /= dy;

    if(cx2>=mx-1)cx2=mx-1;
    if(cy2>=my-1)cy2=my-1;
    if(cx1<0) cx1=0;
    if(cy1<0) cy1=0;

    *rx1 = cx1;
    *ry1 = cy1;
    *rx2 = cx2;
    *ry2 = cy2;

    return true;
}

bool TileRegionClass::convert_redraw_rect(int x1, int y1, int x2, int y2, 
                  int *rx1, int *ry1, int *rx2, int *ry2)
{
    int cx1 = x1-ox;
    int cy1 = y1-oy;
    int cx2 = x2-ox;
    int cy2 = y2-oy;

    int wwx = dx*mx;
    int wwy = dy*my;

    if ( (cx2 < 0) || (cy2 < 0) || (cx1 >= wwx) || (cy1 >=wwy))
        return false;

    if(cx2>=wwx-1)cx2=wwx-1;
    if(cy2>=wwy-1)cy2=wwy-1;
    if(cx1<0) cx1=0;
    if(cy1<0) cy1=0;

    *rx1 = cx1;
    *ry1 = cy1;
    *rx2 = cx2;
    *ry2 = cy2;

    return true;
}

bool RegionClass::mouse_pos(int mouse_x, int mouse_y, int *cx, int *cy)
{
    int x = mouse_x - ox;
    int y = mouse_y - oy;
    if (!is_active()) return false;
    if ( x < 0 || y < 0 ) return false;
    x /= dx;
    y /= dy;
    if (x >= mx || y >= my) return false;
    *cx = x;
    *cy = y;
    return true;
}

bool MapRegionClass::mouse_pos(int mouse_x, int mouse_y, int *cx, int *cy)
{
    int x = mouse_x - ox;
    int y = mouse_y - oy;
    if ( x < 0 || y < 0 ) return false;
    x /= dx;
    y /= dy;
    if (x >= mx || y >= my) return false;
    if (!is_active()) return false;

    *cx = x;
    *cy = y;
    return true;
}

bool TileRegionClass::mouse_pos(int mouse_x, int mouse_y, int *cx, int *cy)
{
    int x = mouse_x - ox;
    int y = mouse_y - oy;
    if (!is_active()) return false;
    if ( x < 0 || y < 0 ) return false;
    if ( x >= dx*mx || y >= dy*my ) return false;

    x /= dx;
    y /= dy;
    *cx = x;
    *cy = y;

    return true;
}

/*
 * Text related 
 */

void TextRegionClass::scroll()
{
    int idx;

    if(!flag)
        return;

    for(idx=0; idx<mx*(my-1);idx++)
    {
        cbuf[idx] = cbuf[idx + mx];
        abuf[idx] = abuf[idx + mx];
    }

    for(idx=mx*(my-1);idx<mx*my;idx++)
    {
        cbuf[idx] = ' ';
        abuf[idx] = 0;
    }
    redraw(0, 0, mx-1, my-1);

    if (print_y > 0)
        print_y -= 1;
    if (cursor_y > 0)
        cursor_y -= 1;
}

void TextRegionClass::adjust_region(int *x1, int *x2, int y)
{
    *x2 = *x2 + 1;
}

void TextRegionClass::addstr(char *buffer)
{
    int i,j;
    char buf2[1024];
    int len = strlen(buffer);

    if(!flag)return;

    j=0;

    for(i=0;i<len+1;i++)
    {
        char c = buffer[i];
        bool newline=false;
        if (c== '\n' || c== '\r')
        {
            c=0;
            newline = true;
            if (buffer[i+1]=='\n' || buffer[i+1]=='\r')
                i++;
        }
        buf2[j] = c;
        j++;
        if(c==0)
        {
            if (j-1 != 0)
                addstr_aux(buf2, j - 1); // draw it
            if (newline)
            {
                print_x = cx_ofs;
                print_y++;
                j=0;

                if(print_y - cy_ofs == my)
                    scroll();
            }
        }
    }
    if (cursor_flag) cgotoxy(print_x+1, print_y+1);
}

void TextRegionClass::addstr_aux(char *buffer, int len)
{
    int i;
    int x = print_x - cx_ofs;
    int y = print_y - cy_ofs;
    int adrs = y * mx;
    int head = x;
    int tail = x + len - 1;

    if(!flag)
        return;

    adjust_region(&head, &tail, y);

    for (i=0; i < len && x + i < mx;i++)
    {
        cbuf[adrs+x+i]=buffer[i];
        abuf[adrs+x+i]=text_col;
    }
    draw_string(head, y, &cbuf[adrs+head], tail-head, text_col);
    print_x += len;
}

void TextRegionClass::redraw(int x1, int y1, int x2, int y2)
{
    int x, y;
    if(!flag)
        return;

    for(y=y1;y<=y2;y++)
    {
        unsigned char *a = &abuf[y * mx];
        unsigned char *c = &cbuf[y * mx];
        int head = x1;
        int tail = x2;
        adjust_region(&head, &tail, y);

        x=head;
        int col = a[x];

        while (x<=tail)
        {
            int oldcol = col;
            if (x==tail)
                col = -1;
            else
                col = a[x];
            if (oldcol != col)
            {
                draw_string(head, y, &c[head], x-head, oldcol);
                head = x;
            }
            x++;
        }
    }

    if(cursor_region == this && cursor_flag == 1)
        draw_cursor(cursor_x, cursor_y);

    sys_flush();
}

void TextRegionClass::clear_to_end_of_line()
{
    int i;
    int cx = print_x - cx_ofs;
    int cy = print_y - cy_ofs;
    int col = text_col;
    int adrs = cy * mx;

    if(!flag)
        return;

    ASSERT(adrs + mx - 1 < mx * my);
    for(i=cx; i<mx; i++){
        cbuf[adrs+i] = ' ';
        abuf[adrs+i] = col;
    }
    redraw(cx, cy, mx-1, cy);
}

void TextRegionClass::clear_to_end_of_screen()
{
    int i;
    int cy = print_y - cy_ofs;
    int col = text_col;

    if(!flag)return;

    for(i=cy*mx; i<mx*my; i++){
        cbuf[i]=' ';
        abuf[i]=col;
    }
    redraw(0, cy, mx-1, my-1);
}

void TextRegionClass::putch(unsigned char ch)
{
    if (ch==0) ch=32;
    addstr_aux((char *)&ch, 1);
}

void TextRegionClass::writeWChar(unsigned char *ch)
{
    addstr_aux((char *)ch, 2);
}

void TextRegionClass::textcolor(int color)
{
    text_col = color;
}

void TextRegionClass::textbackground(int col)
{
    textcolor(col*16 + (text_col & 0xf));
}

void TextRegionClass::cgotoxy(int x, int y)
{
    print_x = x-1;
    print_y = y-1;

    if (cursor_region != NULL && cursor_flag)
    {
        cursor_region ->erase_cursor();
        cursor_region = NULL;
    }

    if (cursor_flag)
    {
        text_mode->draw_cursor(print_x, print_y);
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}

int  TextRegionClass::wherex()
{
    return print_x + 1;
}

int  TextRegionClass::wherey()
{
    return print_y + 1;
}

void TextRegionClass::_setcursortype(int curstype)
{
    cursor_flag = curstype;
    if (cursor_region != NULL)
        cursor_region ->erase_cursor();

    if (curstype)
    {
        text_mode->draw_cursor(print_x, print_y);
        cursor_x = print_x;
        cursor_y = print_y;
        cursor_region = text_mode;
    }
}
