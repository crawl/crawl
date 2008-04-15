/*
 *  File:       guic-x11.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xlocale.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xmd.h>

#include "AppHdr.h"
#include "debug.h"
#include "externs.h"
#include "guic.h"
#include "version.h"

static Display *display;
static int screen;
static GC term_gc[MAX_TERM_COL];
static GC map_gc[MAX_MAP_COL];
static unsigned long term_pix[MAX_TERM_COL];
static unsigned long map_pix[MAX_MAP_COL];
static unsigned long pix_transparent;
static unsigned long pix_hilite;
static unsigned long pix_black;
static unsigned long pix_rimcolor;
static int x11_byte_per_pixel_ximage();
static unsigned long create_pixel(unsigned int red, unsigned int green,
                 unsigned int blue);
static XImage *read_png(const char *fname);

/*******************************************************/
void WinClass::SysInit()
{
    win = (Window)NULL;
}

void WinClass::SysDeinit()
{}

void RegionClass::SysInit()
{
    font = NULL;
}

void RegionClass::SysDeinit()
{
    if (font != NULL && !font_copied) XFreeFont(display, font);
}

void TextRegionClass::SysInit(int x, int y, int cx, int cy)
{}

void TextRegionClass::SysDeinit()
{}

void TileRegionClass::SysInit(int mx0, int my0, int dx0, int dy0)
{}

void TileRegionClass::SysDeinit()
{}

void MapRegionClass::SysInit(int x, int y, int o_x, int o_y)
{}

void MapRegionClass::SysDeinit()
{}

void RegionClass::init_font(const char *name){
  /*** Large part of this routine was copied from Hengband ***/
    int ascent, descent, width;

    font = XLoadQueryFont(display, name);
    if (!font)
    {
        fprintf(stderr,"Error! Can't load font %s\n",name);
        exit(1);
    }
    width   = font->max_bounds.width;
    ascent  = font->ascent;
    descent = font->descent;

    int i;
    for (i=0;i<MAX_TERM_COL;i++)
        XSetFont(display, term_gc[i], font->fid);

    fx = dx = width;
    fy = dy = ascent + descent;
    asc =  ascent;
    font_copied = false;
}

void RegionClass::copy_font(RegionClass *r)
{
    fx = r->fx;
    fy = r->fy;
    dx = r->dx;
    dy = r->dy;
    asc = r->asc;
    font = r->font;
    font_copied = true;
}

void RegionClass::sys_flush()
{
    XFlush(display);
}

void RegionClass::init_backbuf()
{
}

void TextRegionClass::init_backbuf()
{
}

void TileRegionClass::init_backbuf()
{
    int x, y;

    backbuf = ImgCreateSimple(mx*dx, my*dy);
    for (x = 0; x < mx*dx; x++)
    for (y = 0; y < my*dy; y++)
        XPutPixel(backbuf, x, y, pix_black);
}

void TileRegionClass::resize_backbuf()
{
    if (backbuf != NULL) ImgDestroy(backbuf);
    init_backbuf();
}

void RegionClass::resize_backbuf()
{
    if (backbuf != NULL) ImgDestroy(backbuf);
    init_backbuf();
}

void MapRegionClass::init_backbuf()
{
    int x, y;
    backbuf = ImgCreateSimple(mx*dx, my*dy);
    for (x = 0; x < mx*dx; x++)
        for (y = 0; y < my*dy; y++)
            XPutPixel(backbuf, x, y, pix_black);
}

void MapRegionClass::resize_backbuf()
{
    if (backbuf != NULL)
        ImgDestroy(backbuf);
    init_backbuf();
}

void TextRegionClass::draw_string(int x, int y, unsigned char *buf,
	int len, int col)
{
    XFillRectangle(display, win->win, term_gc[col>>4],
                      x*dx+ox, y*dy+oy, dx * len, dy);
    XDrawString(display, win->win, term_gc[col&0x0f], x*dx+ox, y*dy+asc+oy,
                (char *)&cbuf[y*mx+x], len);
}

void TextRegionClass::draw_cursor(int x, int y)
{
    if(!flag)return;

    XDrawString(display, win->win, term_gc[0x0f], x*dx+ox, y*dy+asc+oy,
                "_", 1);
    sys_flush();
}

void TextRegionClass::erase_cursor()
{
    WinClass    *w = win;

    int x0 = cursor_x;
    int y0 = cursor_y;
    int width = 1;
    int adrs = y0 * mx + x0;
    int col = abuf[adrs];
    int x1 = x0;;
    if(!flag)return;

    XFillRectangle(display, w->win, term_gc[col>>4],
                      x1*dx + ox, y0*dy +oy, dx*width, dy);

    XDrawString(display, w->win,
                term_gc[col&0x0f], x0*dx+ ox, y0*dy+asc+ oy,
                (char *)&cbuf[adrs], width );
}


void WinClass::clear()
{
    fillrect(0, 0, wx, wy, PIX_BLACK);
    XFlush(display);
}

void RegionClass::clear()
{
    fillrect(0, 0, wx, wy, PIX_BLACK);
    XFlush(display);
}

void TileRegionClass::clear()
{
    RegionClass::clear();
}

void MapRegionClass::clear()
{
    int i;

    for (i=0; i<mx2*my2; i++)
    {
        mbuf[i]=PIX_BLACK;
    }

    RegionClass::clear();
}

void TextRegionClass::clear()
{
    int i;

    for (i=0; i<mx*my; i++)
    {
        cbuf[i]=' ';
        abuf[i]=0;
    }

    RegionClass::clear();
}

void WinClass::create(char *name)
{

    if (!win)
    {
        win = XCreateSimpleWindow(display, RootWindow(display,screen),
            10,10, wx, wy, 0,
            BlackPixel(display,screen), BlackPixel(display,screen));

        XMapWindow(display, win);
        XSelectInput(display, win, ExposureMask | KeyPressMask
            | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
            LeaveWindowMask | EnterWindowMask | StructureNotifyMask );
        move(ox, oy);

        XStoreName(display, win, CRAWL " " VERSION);
    }
    else
        resize(0,0);

    clear();
}

void WinClass::move()
{
    XMoveWindow(display, win, ox, oy);
}

void WinClass::resize()
{
    XResizeWindow(display, win, wx, wy);
}

void TileRegionClass::redraw(int x1, int y1, int x2, int y2)
{
    int wwx = x2-x1+1;
    int wwy = y2-y1+1;

    if (x1<0)
    {
        wwx += x1;
        x1 = 0;
    }

    if (y1<0)
    {
        wwy += y1;
        y1 = 0;
    }

    if (x2 >= mx*dx)
    {
        wwx -= x2-mx*dx+1;
    }

    if (y2 >= my*dy)
    {
        wwy -= y2-my*dy+1;
    }

    XPutImage(display, win->win, term_gc[0],
              backbuf, x1, y1, x1+ox, y1+oy, wwx, wwy);
}

void MapRegionClass::redraw(int x1, int y1, int x2, int y2)
{
    if (!flag)
        return;

    XPutImage(display, win->win, term_gc[0],
              backbuf, x1*dx, y1*dy,
              x1*dx+ox, y1*dy+oy,
              (x2-x1+1)*dx, (y2-y1+1)*dy);
}

void MapRegionClass::draw_data(unsigned char *buf)
{
    static int px = 0;
    static int py = 0;

    if (!flag)
        return;

    const int marker_length = 2;

    for (int yy = 0; yy < dy * marker_length; yy++)
        XPutPixel(backbuf, px*dx+dx/2 + x_margin, yy, map_pix[MAP_BLACK]);

    for (int xx = 0; xx < dx * marker_length; xx++)
        XPutPixel(backbuf, xx, py*dy+dy/2 + y_margin, map_pix[MAP_BLACK]);

    for (int y = 0; y < my - y_margin; y++)
    {
        unsigned char *ptr = &buf[y * (mx2 - x_margin)];
        for (int x = 0; x < mx - x_margin; x++)
        {
            int col = ptr[x];
            if (col == Options.tile_player_col)
            {
                px = x;
                py = y;
            }
            if (col != get_col(x, y) || force_redraw
                || x < marker_length || y < marker_length)
            {
                for (int xx=0; xx<dx; xx++)
                    for (int yy=0; yy<dy; yy++)
                    {
                        XPutPixel(backbuf, x_margin + x*dx+xx,
                                  y_margin + y*dy+yy, map_pix[col]);
                    }

                set_col(col, x, y);
            }
        }
    }

    for (int yy = 0; yy < dy * marker_length; yy++)
        XPutPixel(backbuf, px*dx+dx/2 + x_margin, yy, map_pix[MAP_WHITE]);

    for (int xx = 0; xx < dx * marker_length; xx++)
        XPutPixel(backbuf, xx, py*dy+dy/2 + y_margin, map_pix[MAP_WHITE]);

    redraw();
    XFlush(display);
    force_redraw = false;
}

/* XXXXX
 * img_type related
 */

bool ImgIsTransparentAt(img_type img, int x, int y)
{
    ASSERT(x>=0);
    ASSERT(y>=0);
    ASSERT(x<(img->width));
    ASSERT(y<(img->height));

    return (pix_transparent == XGetPixel(img, x, y)) ? true:false;
}

void ImgSetTransparentPix(img_type img)
{
    pix_transparent = XGetPixel(img, 0, 0);
}

void ImgDestroy(img_type img)
{
    XDestroyImage(img);
}

img_type ImgCreateSimple(int wx, int wy)
{
    if (wx ==0 || wy == 0) return NULL;
    char *buf = (char *)malloc(x11_byte_per_pixel_ximage()* wx * wy);

    img_type res= XCreateImage(display, DefaultVisual(display, screen),
                                  DefaultDepth(display, screen),
                                  ZPixmap, 0, buf, wx, wy, 8, 0);
    return(res);
}

img_type ImgLoadFile(const char *name)
{
    return read_png(name);
}

void ImgClear(img_type img)
{
    int x, y;
    ASSERT(img != NULL);
    for (x = 0; x < img->width; x++)
        for (y = 0; y < img->height; y++)
            XPutPixel(img, x, y, pix_transparent);
}

// Copy internal image to another internal image
void ImgCopy(img_type src,  int sx, int sy, int wx, int wy,
             img_type dest, int dx, int dy, int copy)
{
    int x, y;
    int bpp = src->bytes_per_line / src->width;
    int bpl_s = src->bytes_per_line;
    int bpl_d = dest->bytes_per_line;

    ASSERT(sx >= 0);
    ASSERT(sy >= 0);
    ASSERT(sx + wx <= src->width);
    ASSERT(sy + wy <= src->height);
    ASSERT(dx >= 0);
    ASSERT(dy >= 0);
    ASSERT(dx + wx <= dest->width);
    ASSERT(dy + wy <= dest->height);

    if (copy == 1)
    {
        char *p_src  = (char *)(src->data + bpl_s * sy + sx * bpp);
        char *p_dest = (char *)(dest->data + bpl_d * dy + dx * bpp);

        for (y = 0; y < wy; y++)
        {
            memcpy(p_dest, p_src, wx * bpp);
            p_src  += bpl_s;
            p_dest += bpl_d;
        }
    }
    else if (bpp <= 1)
    {
        CARD8 *p_src  = (CARD8 *)(src->data + bpl_s * sy + sx * bpp);
        CARD8 *p_dest = (CARD8 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] != pix_transparent)
                    p_dest[x] = p_src[x];
            }
            p_src  += bpl_s;
            p_dest += bpl_d;
        }
    }
    else if (bpp <= 2)
    {
        CARD16 *p_src  = (CARD16 *)(src->data + bpl_s * sy + sx * bpp);
        CARD16 *p_dest = (CARD16 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] != pix_transparent)
                    p_dest[x] = p_src[x];
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }
    else if (bpp <= 4)
    {
        CARD32 *p_src  = (CARD32 *)(src->data + bpl_s * sy + sx * bpp);
        CARD32 *p_dest = (CARD32 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] != pix_transparent)
                    p_dest[x] = p_src[x];
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }

}

// Copy internal image to another internal image
void ImgCopyH(img_type src,  int sx, int sy, int wx, int wy,
              img_type dest, int dx, int dy, int copy)
{
    int x, y;
    int bpp = src->bytes_per_line / src->width;
    int bpl_s = src->bytes_per_line;
    int bpl_d = dest->bytes_per_line;

    ASSERT(sx >= 0);
    ASSERT(sy >= 0);
    ASSERT(sx + wx <= src->width);
    ASSERT(sy + wy <= src->height);
    ASSERT(dx >= 0);
    ASSERT(dy >= 0);
    ASSERT(dx + wx <= dest->width);
    ASSERT(dy + wy <= dest->height);

    if (copy == 1)
    {
        char *p_src  = (char *)(src->data + bpl_s * sy + sx * bpp);
        char *p_dest = (char *)(dest->data + bpl_d * dy + dx * bpp);

        for (y = 0; y < wy; y++)
        {
            memcpy(p_dest, p_src, wx * bpp);
            p_src  += bpl_s;
            p_dest += bpl_d;
        }
    }
    else if (bpp <= 1)
    {
        CARD8 *p_src  = (CARD8 *)(src->data + bpl_s * sy + sx * bpp);
        CARD8 *p_dest = (CARD8 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] == pix_rimcolor)
                    p_dest[x] = pix_hilite;
                else if (p_src[x] != pix_transparent)
                    p_dest[x] = p_src[x];
            }
            p_src  += bpl_s;
            p_dest += bpl_d;
        }
    }
    else if (bpp <= 2)
    {
        CARD16 *p_src  = (CARD16 *)(src->data + bpl_s * sy + sx * bpp);
        CARD16 *p_dest = (CARD16 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] == pix_rimcolor)
                    p_dest[x] = pix_hilite;
                else if (p_src[x] != pix_transparent)
                    p_dest[x] = p_src[x];
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }
    else if (bpp <= 4)
    {
        CARD32 *p_src  = (CARD32 *)(src->data + bpl_s * sy + sx * bpp);
        CARD32 *p_dest = (CARD32 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] == pix_rimcolor)
                    p_dest[x] = pix_hilite;
                else if (p_src[x] != pix_transparent)
                    p_dest[x] = p_src[x];
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }

}


// Copy internal image to another internal image
void ImgCopyMasked(img_type src,  int sx, int sy, int wx, int wy,
                   img_type dest, int dx, int dy, char *mask)
{
    int x, y, count;
    int bpp = src->bytes_per_line / src->width;
    int bpl_s = src->bytes_per_line;
    int bpl_d = dest->bytes_per_line;
    ASSERT(sx >= 0);
    ASSERT(sy >= 0);
    ASSERT(sx + wx <= src->width);
    ASSERT(sy + wy <= src->height);
    ASSERT(dx >= 0);
    ASSERT(dy >= 0);
    ASSERT(dx + wx <= dest->width);
    ASSERT(dy + wy <= dest->height);

    count = 0;

    if (bpp <= 1)
    {
        CARD8 *p_src  = (CARD8 *)(src->data + bpl_s * sy + sx * bpp);
        CARD8 *p_dest = (CARD8 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] != pix_transparent && mask[count] == 0)
                    p_dest[x] = p_src[x];
                count++;
            }
            p_src  += bpl_s;
            p_dest += bpl_d;
        }
    }
    else if (bpp <= 2)
    {
        CARD16 *p_src  = (CARD16 *)(src->data + bpl_s * sy + sx * bpp);
        CARD16 *p_dest = (CARD16 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if(p_src[x] != pix_transparent && mask[count] == 0)
                    p_dest[x] = p_src[x];
                count++;
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }
    else if (bpp <= 4)
    {
        CARD32 *p_src  = (CARD32 *)(src->data + bpl_s * sy + sx * bpp);
        CARD32 *p_dest = (CARD32 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] != pix_transparent && mask[count] == 0)
                    p_dest[x] = p_src[x];
                count++;
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }
}

// Copy internal image to another internal image
void ImgCopyMaskedH(img_type src,  int sx, int sy, int wx, int wy,
                   img_type dest, int dx, int dy, char *mask)
{
    int x, y, count;
    int bpp = src->bytes_per_line / src->width;
    int bpl_s = src->bytes_per_line;
    int bpl_d = dest->bytes_per_line;
    ASSERT(sx >= 0);
    ASSERT(sy >= 0);
    ASSERT(sx + wx <= src->width);
    ASSERT(sy + wy <= src->height);
    ASSERT(dx >= 0);
    ASSERT(dy >= 0);
    ASSERT(dx + wx <= dest->width);
    ASSERT(dy + wy <= dest->height);

    count = 0;

    if (bpp <= 1)
    {
        CARD8 *p_src  = (CARD8 *)(src->data + bpl_s * sy + sx * bpp);
        CARD8 *p_dest = (CARD8 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] == pix_rimcolor)
                    p_dest[x] = pix_hilite;
                else if(p_src[x] != pix_transparent && mask[count] == 0)
                    p_dest[x] = p_src[x];

                count++;
            }
            p_src  += bpl_s;
            p_dest += bpl_d;
        }
    }
    else if (bpp <= 2)
    {
        CARD16 *p_src  = (CARD16 *)(src->data + bpl_s * sy + sx * bpp);
        CARD16 *p_dest = (CARD16 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] == pix_rimcolor)
                    p_dest[x] = pix_hilite;
                else if (p_src[x] != pix_transparent && mask[count] == 0)
                    p_dest[x] = p_src[x];
                count++;
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }
    else if (bpp <= 4)
    {
        CARD32 *p_src  = (CARD32 *)(src->data + bpl_s * sy + sx * bpp);
        CARD32 *p_dest = (CARD32 *)(dest->data + bpl_d * dy + dx * bpp);
        for (y = 0; y < wy; y++)
        {
            for (x = 0; x < wx; x++)
            {
                //X11 specific
                if (p_src[x] == pix_rimcolor)
                    p_dest[x] = pix_hilite;
                else if (p_src[x] != pix_transparent && mask[count] == 0)
                    p_dest[x] = p_src[x];
                count++;
            }
            p_src  += bpl_s/bpp;
            p_dest += bpl_d/bpp;
        }
    }
}

void TileRegionClass::DrawPanel(int left, int top, int width, int height)
{
    framerect(left, top , left + width, top + height, PIX_WHITE);
    framerect(left + 1, top + 1, left + width, top + height, PIX_DARKGREY);
    fillrect (left + 1, top + 1, left + width - 1, top + height -1, PIX_LIGHTGREY);
}

void RegionClass::framerect(int left, int top, int right, int bottom,
                            int color)
{
    XDrawRectangle(display, win->win, term_gc[color&0xf],
                   ox+left, oy+top, right-left, bottom-top);
}

void TileRegionClass::framerect(int left, int top, int right, int bottom,
                    int color)
{
    int x,y;
    int pix = term_pix[color];

    for (x = left; x <= right; x++)
    {
        XPutPixel(backbuf, x, top, pix);
        XPutPixel(backbuf, x, bottom, pix);
    }

    for (y = top+1; y < bottom; y++)
    {
        XPutPixel(backbuf, left, y, pix);
        XPutPixel(backbuf, right, y, pix);
    }
}

void WinClass::fillrect(int left, int top, int right, int bottom,
                            int color)
{
    XFillRectangle(display, win, term_gc[color&0xf],
                   top, left, right-left+1, bottom-top+1);
}

void RegionClass::fillrect(int left, int top, int right, int bottom,
                            int color)
{
    XFillRectangle(display, win->win, term_gc[color&0xf],
                   ox+left, oy+top, right-left, bottom-top);
}

void  TileRegionClass::fillrect(int left, int top, int right, int bottom,
                    int color)
{
    int x,y;
    int pix = term_pix[color];

    ASSERT(left >= 0);
    ASSERT(top  >= 0);
    ASSERT(right  < mx*dx);
    ASSERT(bottom < my*dy);

    for (x = left; x <= right; x++)
        for (y = top; y <= bottom; y++)
            XPutPixel(backbuf, x, y, pix);
}

/********************************************/

bool GuicInit(Display **d, int *s)
{
    int  i;
    setlocale(LC_ALL, "");
    display = XOpenDisplay("");
    if (!display)
    {
        fprintf(stderr,"Cannot open display\n");
        return false;
    }
    screen=DefaultScreen(display);

    *d = display;
    *s = screen;

    // for text display
    for (i = 0; i < MAX_TERM_COL; i++)
    {
        const int *c = term_colors[i];
        term_pix[i] = create_pixel(c[0], c[1], c[2]);
        term_gc[i]  = XCreateGC(display, RootWindow(display,screen), 0, 0);
        XSetForeground(display,term_gc[i], term_pix[i]);
    }
    // for text display
    for (i = 0; i < MAX_MAP_COL; i++)
    {
        const int *c = map_colors[i];
        map_pix[i] = create_pixel(c[0], c[1], c[2]);
        map_gc[i]  = XCreateGC(display, RootWindow(display,screen), 0, 0);
        XSetForeground(display, map_gc[i], map_pix[i]);
    }

    // for Image manipulation
    pix_black    = term_pix[PIX_BLACK] ;
    pix_hilite   = term_pix[PIX_LIGHTMAGENTA] ;
    pix_rimcolor = create_pixel(1,1,1);

    return true;
}

void GuicDeinit()
{
    int i;

    for (i = 0; i < MAX_TERM_COL; i++)
        XFreeGC(display,term_gc[i]);

    for (i = 0; i < MAX_MAP_COL; i++)
        XFreeGC(display,map_gc[i]);

    XCloseDisplay(display);
}

static int x11_byte_per_pixel_ximage()
{
    int i = 1;
    int j = (DefaultDepth(display, screen) - 1) >> 2;
    while (j >>= 1)
    {
        i <<= 1;
    }
    return i;
}

unsigned long create_pixel(unsigned int red, unsigned int green,
                 unsigned int blue)
{
    Colormap cmap = DefaultColormapOfScreen(DefaultScreenOfDisplay(display));
    XColor xcolour;

    xcolour.red   = red * 256;
    xcolour.green = green * 256;
    xcolour.blue  = blue * 256;
    xcolour.flags = DoRed | DoGreen | DoBlue;

    XAllocColor(display, cmap, &xcolour);
    return (xcolour.pixel);
}

/*
 Copied from pngtopnm.c and modified by M.Itakura
 (mostly omitted and added a few lines)
 only color paletted image is handled
*/

/*
** pngtopnm.c -
** read a Portable Network Graphics file and produce a portable anymap
**
** Copyright (C) 1995,1998 by Alexander Lehmann <alex@hal.rhein-main.de>
**                        and Willem van Schaik <willem@schaik.com>
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
**
** modeled after giftopnm by David Koblas and
** with lots of bits pasted from libpng.txt by Guy Eric Schalnat
*/

#include "png.h"

#define pm_message printf
#define pm_error(x) {fprintf(stderr,x);return NULL;}

#  define TRUE 1
#  define FALSE 0
#  define NONE 0

#define SIG_CHECK_SIZE 4

XImage *read_png (const char *fname)
{
    char sig_buf [SIG_CHECK_SIZE];
    png_struct *png_ptr;
    png_info *info_ptr;
    png_byte **png_image;
    png_byte *png_pixel;
    unsigned int x, y;
    int linesize;
    png_uint_16 c;
    unsigned int i;

    //X11
    XImage *res;
    unsigned long pix_table[256];

    FILE *ifp = fopen(fname,"r");

    if (!ifp)
    {
        fprintf(stderr, "File not found: %s", fname);
        return NULL;
    }

    if (fread (sig_buf, 1, SIG_CHECK_SIZE, ifp) != SIG_CHECK_SIZE)
        pm_error ("input file empty or too short");
    if (png_sig_cmp ((unsigned char *)sig_buf, (png_size_t) 0,
                     (png_size_t) SIG_CHECK_SIZE) != 0)
    {
        pm_error ("input file not a PNG file");
    }

    png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL)
        pm_error ("cannot allocate LIBPNG structure");

    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL)
    {
        png_destroy_read_struct (&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        pm_error ("cannot allocate LIBPNG structures");
    }

    if (setjmp (png_ptr->jmpbuf))
    {
        png_destroy_read_struct (&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        free (png_ptr);
        free (info_ptr);
        pm_error ("setjmp returns error condition");
    }

    png_init_io (png_ptr, ifp);
    png_set_sig_bytes (png_ptr, SIG_CHECK_SIZE);
    png_read_info (png_ptr, info_ptr);

    png_image = (png_byte **)malloc (info_ptr->height * sizeof (png_byte*));
    if (png_image == NULL)
    {
        free (png_ptr);
        free (info_ptr);
        pm_error ("couldn't alloc space for image");
    }

    if (info_ptr->bit_depth == 16)
        linesize = 2 * info_ptr->width;
    else
        linesize = info_ptr->width;

    if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        linesize *= 2;
    else if (info_ptr->color_type == PNG_COLOR_TYPE_RGB)
        linesize *= 3;
    else if (info_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        linesize *= 4;

    for (y = 0 ; y < info_ptr->height ; y++)
    {
        png_image[y] = (png_byte *)malloc (linesize);
        if (png_image[y] == NULL)
        {
            for (x = 0; x < y; x++)
                free (png_image[x]);

            free (png_image);
            free (png_ptr);
            free (info_ptr);
            pm_error ("couldn't alloc space for image");
        }
    }

    if (info_ptr->bit_depth < 8)
        png_set_packing (png_ptr);

  /* sBIT handling is very tricky. If we are extracting only the image, we
     can use the sBIT info for grayscale and color images, if the three
     values agree. If we extract the transparency/alpha mask, sBIT is
     irrelevant for trans and valid for alpha. If we mix both, the
     multiplication may result in values that require the normal bit depth,
     so we will use the sBIT info only for transparency, if we know that only
     solid and fully transparent is used */

    if (info_ptr->valid & PNG_INFO_sBIT)
    {
        if ((info_ptr->color_type == PNG_COLOR_TYPE_PALETTE ||
             info_ptr->color_type == PNG_COLOR_TYPE_RGB ||
             info_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA) &&
            (info_ptr->sig_bit.red != info_ptr->sig_bit.green ||
             info_ptr->sig_bit.red != info_ptr->sig_bit.blue) )
        {
	        pm_message ("different bit depths for color channels not "
                        "supported");
	        pm_message ("writing file with %d bit resolution",
                        info_ptr->bit_depth);
        }
        else if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE
                 && info_ptr->sig_bit.red < 255)
        {
            for (i = 0 ; i < info_ptr->num_palette ; i++)
            {
	            info_ptr->palette[i].red   >>= (8 - info_ptr->sig_bit.red);
	            info_ptr->palette[i].green >>= (8 - info_ptr->sig_bit.green);
	            info_ptr->palette[i].blue  >>= (8 - info_ptr->sig_bit.blue);
	        }
        }
        else if ((info_ptr->color_type == PNG_COLOR_TYPE_GRAY
                    || info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
                 && info_ptr->sig_bit.gray < info_ptr->bit_depth)
        {
	        png_set_shift (png_ptr, &(info_ptr->sig_bit));
        }
    }

    if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
    {
        //X11
        for (i = 0 ; i < info_ptr->num_palette ; i++)
            pix_table[i] = create_pixel(info_ptr->palette[i].red,

        info_ptr->palette[i].green, info_ptr->palette[i].blue);
    }
    else if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY)
    {
        //X11
        for (i = 0 ; i < 256 ; i++)
            pix_table[i] = create_pixel(i, i, i);
    }

    png_read_image (png_ptr, png_image);
    png_read_end (png_ptr, info_ptr);

    res = ImgCreateSimple(info_ptr->width, info_ptr->height);

    for (y = 0; y < info_ptr->height; y++)
    {
        png_pixel = png_image[y];
        for (x = 0; x < info_ptr->width; x++)
        {
            c = *png_pixel;
            png_pixel++;
            XPutPixel(res, x, y, pix_table[c]);
        }
    }

    for (y = 0 ; y < info_ptr->height ; y++)
        free (png_image[y]);

    free (png_image);
    free (png_ptr);
    free (info_ptr);

    fclose(ifp);
    return res;
}
