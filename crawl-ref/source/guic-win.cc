/*
 *  File:       guic-win.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#include "AppHdr.h"
#include "debug.h"
#include "externs.h"
#include "guic.h"

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// WinClass & RegionClass definitions
static HINSTANCE hInst;
static int       nCmdShow;

// colors
static COLORREF term_pix[MAX_TERM_COL];
BYTE pix_transparent;
BYTE pix_black;
BYTE pix_white;
BYTE pix_magenta;
BYTE pix_rimcolor;

RGBQUAD RegionClass::std_palette[256];

bool GuicInit(HINSTANCE h, int nCmd)
{
    int i;
    hInst = h;
    nCmdShow = nCmd;

    for (i = 0; i < MAX_TERM_COL; i++)
    {
    int *c = (int *)&term_colors[i];
        term_pix[i] = PALETTERGB( c[0], c[1], c[2] );
    }
    return true;
}

void GuicDeinit()
{

}

void WinClass::SysInit()
{
    hWnd = NULL;
}

void WinClass::SysDeinit()
{}

void RegionClass::SysInit()
{
    font = NULL;
}

void RegionClass::SysDeinit()
{
    if (font != NULL && !font_copied) DeleteObject(font);
}

void RegionClass::sys_flush()
{}

void TextRegionClass::SysInit(int x, int y, int cx, int cy)
{
    dos_char = false;
}

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

void RegionClass::init_font(const char *name, int height)
{
    int wid, hgt;
    LOGFONT lf;
    HFONT ftmp;

    strcpy(lf.lfFaceName, name);
    lf.lfHeight        = height;
    lf.lfWidth         = 0;
    lf.lfEscapement    = 0;
    lf.lfOrientation   = lf.lfEscapement;
    lf.lfWeight        = FW_NORMAL;
    lf.lfItalic        = FALSE;
    lf.lfUnderline     = FALSE;
    lf.lfStrikeOut     = FALSE;
    lf.lfOutPrecision  = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality       = DEFAULT_QUALITY;
#ifdef JP
    lf.lfCharSet       = (dos_char) ? OEM_CHARSET:SHIFTJIS_CHARSET;
    lf.lfPitchAndFamily= FF_DONTCARE|FIXED_PITCH;
#else
    lf.lfCharSet       = (dos_char) ? OEM_CHARSET:ANSI_CHARSET;
    lf.lfPitchAndFamily= FF_MODERN|FIXED_PITCH;
#endif
    ftmp = CreateFontIndirect( &lf );

    if (!ftmp)
    {
    if (font) return;
    exit(1);
    }
    font = ftmp;

    wid = lf.lfWidth;
    hgt = lf.lfHeight;
    /* This part is taken from angband */
    /* Hack -- Unknown size */
    if (!wid || !hgt)
    {
        HDC hdcDesktop;
        HFONT hfOld;
        TEXTMETRIC tm;

        /* all this trouble to get the cell size */
        hdcDesktop = GetDC(HWND_DESKTOP);
        hfOld = (HFONT)SelectObject(hdcDesktop, font);
        GetTextMetrics(hdcDesktop, &tm);
        SelectObject(hdcDesktop, hfOld);
        ReleaseDC(HWND_DESKTOP, hdcDesktop);

        /* Font size info */
        wid = tm.tmAveCharWidth;
        hgt = tm.tmHeight;
    }

    fx = dx = wid;
    fy = dy = hgt;
}

void RegionClass::change_font(const char *name, int height)
{
    if (font != NULL) DeleteObject(font);
    init_font(name, height);
}

void RegionClass::copy_font(RegionClass *r)
{
    fx = r->fx;
    fy = r->fy;
    dx = r->dx;
    dy = r->dy;
    font = r->font;
}

void RegionClass::set_std_palette(RGBQUAD *pPal)
{
    int i;
    for (i = 0; i < 256; i++)
    {
        std_palette[i].rgbRed   = pPal[i].rgbRed  ;
        std_palette[i].rgbGreen = pPal[i].rgbGreen;
        std_palette[i].rgbBlue  = pPal[i].rgbBlue ;

        if (pPal[i].rgbRed      == 0
            && pPal[i].rgbGreen == 0
            && pPal[i].rgbBlue  == 0)
        {
            pix_black = i;
        }

        if (pPal[i].rgbRed      == 255
            && pPal[i].rgbGreen == 255
            && pPal[i].rgbBlue  == 255)
        {
            pix_white = i;
        }

        if (pPal[i].rgbRed      == 255
            && pPal[i].rgbGreen == 0
            && pPal[i].rgbBlue  == 255)
        {
            pix_magenta = i;
        }

        if (pPal[i].rgbRed      == 1
            && pPal[i].rgbGreen == 1
            && pPal[i].rgbBlue  == 1)
        {
            pix_rimcolor = i;
        }
    }

    std_palette[pix_transparent].rgbRed   = 0;
    std_palette[pix_transparent].rgbGreen = 0;
    std_palette[pix_transparent].rgbBlue  = 0;
}

void RegionClass::init_backbuf(RGBQUAD *pPal, int ncolor)
{
    int i;

    // first time
    if (backbuf == NULL)
    {
        // alloc for misc info
        backbuf = (dib_pack *)GlobalAlloc(GPTR, sizeof(dib_pack));

        // alloc for header+palette data
        backbuf->pDib = (LPBITMAPINFO)GlobalAlloc(GPTR,
                (sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD)) );

        // set header data
        backbuf->pDib->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        backbuf->pDib->bmiHeader.biPlanes        = 1;
        backbuf->pDib->bmiHeader.biBitCount      = 8;
        backbuf->pDib->bmiHeader.biCompression   = BI_RGB;
        backbuf->pDib->bmiHeader.biSizeImage     = 0;
        backbuf->pDib->bmiHeader.biXPelsPerMeter = 0;
        backbuf->pDib->bmiHeader.biYPelsPerMeter = 0;
        backbuf->pDib->bmiHeader.biClrUsed       = 0;
        backbuf->pDib->bmiHeader.biClrImportant  = 0;

        // set palette data
        for (i = 0; i < ncolor; i++)
        {
            // copy palette from given palette pPal
            backbuf->pDib->bmiColors[i].rgbRed   = pPal[i].rgbRed  ;
            backbuf->pDib->bmiColors[i].rgbGreen = pPal[i].rgbGreen;
            backbuf->pDib->bmiColors[i].rgbBlue  = pPal[i].rgbBlue ;
        }
    }
    // set dimension
    backbuf->pDib->bmiHeader.biWidth    = mx*dx;
    backbuf->pDib->bmiHeader.biHeight   = my*dy;
    backbuf->Width = mx * dx;
    backbuf->Height= my * dy;

    if (win != NULL)
    {
        // this routine should be called after the window is initialized
        if (win->hWnd != NULL)
        {
            HDC hdc1 = GetDC(0);
            HDC hdc2 = GetDC(win->hWnd);
        // alloc a region of the window
            backbuf->hDib = CreateDIBSection(hdc1, backbuf->pDib,
                DIB_RGB_COLORS, (VOID **)&(backbuf->pDibBits), NULL, 0);
            backbuf->hDC = CreateCompatibleDC(hdc2);
            SelectObject(backbuf->hDC, backbuf->hDib);
            ReleaseDC(win->hWnd, hdc2);
            ReleaseDC(0, hdc1);
        }
    }
    backbuf->pDibZero = (backbuf->pDibBits
                        + (backbuf->Height -1) * backbuf->Width);
}

void RegionClass::resize_backbuf()
{
    int i;

    // discard it for resize
    if (backbuf->hDC != NULL)  DeleteDC(backbuf->hDC);
    if (backbuf->hDib != NULL) DeleteObject(backbuf->hDib);

    // set dimension
    backbuf->pDib->bmiHeader.biWidth  = mx*dx;
    backbuf->pDib->bmiHeader.biHeight = my*dy;
    backbuf->Width  = mx * dx;
    backbuf->Height = my * dy;

    HDC hdc1 = GetDC(0);
    HDC hdc2 = GetDC(win->hWnd);
    // alloc a region of the window
    backbuf->hDib = CreateDIBSection(hdc1, backbuf->pDib,
        DIB_RGB_COLORS, (VOID **)&(backbuf->pDibBits), NULL, 0);
    backbuf->hDC = CreateCompatibleDC(hdc2);
    SelectObject(backbuf->hDC, backbuf->hDib);
    ReleaseDC(win->hWnd, hdc2);
    ReleaseDC(0, hdc1);

    backbuf->pDibZero = (backbuf->pDibBits
                        + (backbuf->Height -1) * backbuf->Width);

    for (i = 0; i< mx*dx*my*dy; i++)
        *(backbuf->pDibBits + i) = pix_black;
}

void MapRegionClass::resize_backbuf()
{
    RegionClass::resize_backbuf();
}

void TileRegionClass::resize_backbuf()
{
    RegionClass::resize_backbuf();
}

void TextRegionClass::init_backbuf()
{
  /* not used */
}

void TileRegionClass::init_backbuf(RGBQUAD *pPal)
{
    int i;
    if (!pPal)
        RegionClass::init_backbuf(std_palette, 256);
    else
        RegionClass::init_backbuf(pPal, 256);

    for (i = 0; i< mx*dx*my*dy; i++)
        *(backbuf->pDibBits + i) = pix_black;
}

void MapRegionClass::init_backbuf()
{
    BYTE black = 0;
    RGBQUAD scol[MAX_MAP_COL];
    int i;

    for (i = 0; i < MAX_MAP_COL; i++)
    {
        scol[i].rgbBlue  = map_colors[i][2];
        scol[i].rgbGreen = map_colors[i][1];
        scol[i].rgbRed   = map_colors[i][0];
        scol[i].rgbReserved = 0;
    }

    // just resize
    if (backbuf != NULL)
        RegionClass::init_backbuf(NULL, 0);
    else
        RegionClass::init_backbuf(scol, MAX_MAP_COL);

    for (i = 0; i < MAX_MAP_COL; i++)
    {
        if (backbuf->pDib->bmiColors[i].rgbRed      == 0
            && backbuf->pDib->bmiColors[i].rgbGreen == 0
            && backbuf->pDib->bmiColors[i].rgbBlue  == 0)
        {
            black = i;
        }
    }

    for (i = 0; i< mx*dx*my*dy; i++)
       *(backbuf->pDibBits + i) = black;
}

// defined to object, not to class
void TextRegionClass::draw_string(int x, int y, unsigned char *buf,
                                  int len, int col)
{
    HDC hdc = GetDC(win->hWnd);
    RECT rc;
    rc.left   = ox + x * dx;
    rc.right  = rc.left + len * dx;
    rc.top    =  oy + y * dy;
    rc.bottom = rc.top + dy;

    SelectObject(hdc, font);
    SetBkColor(hdc, term_pix[col>>4]);
    SetTextColor(hdc, term_pix[col&0x0f]);
    ExtTextOut(hdc, rc.left, rc.top,  ETO_CLIPPED, &rc,
               (char *)buf, len, NULL);
    ReleaseDC(win->hWnd, hdc);
}

void TextRegionClass::draw_cursor(int x, int y)
{
    RECT rc;
    HDC hdc;

    int cx = x - cx_ofs;
    int cy = y - cy_ofs;

    if (!flag)
        return;

    hdc = GetDC(win->hWnd);
    SelectObject(hdc, font);

    rc.left   = ox + cx * dx ;
    rc.right  = rc.left + (2 * dx);
    rc.top    =  oy + cy * dy;
    rc.bottom = rc.top + dy;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, term_pix[0x0f]);
    ExtTextOut(hdc, rc.left, rc.top,  ETO_CLIPPED, &rc,
               "_ ", 2, NULL);
    ReleaseDC(win->hWnd, hdc);
}

void TextRegionClass::erase_cursor()
{
    int x0 = cursor_x;
    int y0 = cursor_y;
    int adrs = y0 * mx + x0;
    int col = abuf[adrs];

    if (!flag)
        return;

    RECT rc;
    HDC hdc = GetDC(win->hWnd);
    SelectObject(hdc, font);

    //restore previous cursor cell
    rc.left   = ox + x0 * dx;
    rc.right  = rc.left + (2 * dx);
    rc.top    = oy + y0 * dy;
    rc.bottom = rc.top + dy;
    unsigned char rchar[3];

    SetBkColor(hdc, term_pix[col>>4]);
    SetTextColor(hdc, term_pix[col&0x0f]);
    rchar[0] = cbuf[adrs];
#ifdef JP
    if ( (rchar[0]&0x80) && !dos_char /*_ismbblead( rchar[0])*/ )
    {
        rchar[1] = cbuf[adrs+1];
        rchar[2] = '\0';
        ExtTextOut(hdc, rc.left, rc.top,  ETO_CLIPPED, &rc,
                   (char *)&rchar, 2, NULL);
    }
    else
#endif
    {
        rchar[1] = '\0';
        ExtTextOut(hdc, rc.left, rc.top,  ETO_CLIPPED, &rc,
                   (char *)&rchar, 1, NULL);
    }
    ReleaseDC(win->hWnd, hdc);
}


void WinClass::clear()
{
    fillrect(0, 0, wx-1, wy-1, PIX_BLACK);
}

void RegionClass::clear()
{
    fillrect(0, 0, wx-1, wy-1, PIX_BLACK);
}

void TileRegionClass::clear()
{
    RegionClass::clear();
}

void MapRegionClass::clear()
{
    for (int i = 0; i < mx2*my2; i++)
        mbuf[i] = PIX_BLACK;

    RegionClass::clear();
}

void TextRegionClass::clear()
{
    int i;

    for (i = 0; i < mx*my; i++)
    {
        cbuf[i] = ' ';
        abuf[i] = 0;
    }
    RegionClass::clear();
}

BOOL WinClass::create(const char *name)
{

    RECT rc;
    rc.left   = 0;
    rc.right  = wx;
    rc.top    = 0;
    rc.bottom = wy;

    //game_state = STAT_NORMAL;

    if ( GetSystemMetrics(SM_CYSCREEN) < (oy + wy) )
        oy = 0;
    if ( GetSystemMetrics(SM_CXSCREEN) < (ox + wx) )
        ox = 0;

    AdjustWindowRectEx(&rc,
                       (WS_OVERLAPPED  | WS_SYSMENU | WS_MINIMIZEBOX
                        | WS_CAPTION | WS_VISIBLE),
                       false, 0);

    if (hWnd == NULL)
    {
        hWnd = CreateWindowEx(0, "CrawlList",name,
                              (WS_OVERLAPPED  | WS_SYSMENU | WS_MINIMIZEBOX
                               | WS_CAPTION | WS_VISIBLE),
                              ox, oy, //pos
                              rc.right - rc.left, rc.bottom - rc.top, //size
                              HWND_DESKTOP, NULL, hInst, NULL);
       ShowWindow( hWnd, nCmdShow );
    }
    if ( !hWnd )
       return FALSE;

   clear();
   return TRUE;
}

void WinClass::move()
{
    SetWindowPos(hWnd, 0, ox, oy, wx, wy, SWP_NOSIZE);
    UpdateWindow( hWnd );
}

void WinClass::resize()
{
    RECT rc;
    rc.left   = 0;
    rc.right  = wx;
    rc.top    = 0;
    rc.bottom = wy;

    AdjustWindowRectEx(&rc,
                       (WS_OVERLAPPED  | WS_SYSMENU | WS_MINIMIZEBOX
                        | WS_CAPTION | WS_VISIBLE),
                       false, 0);

    SetWindowPos(hWnd, 0, ox, oy,
                 rc.right - rc.left,
                 rc.bottom - rc.top,
                 SWP_NOMOVE);

    UpdateWindow( hWnd );
}


void TileRegionClass::redraw(int x1, int y1, int x2, int y2)
{
    if (!flag) return;
    if (!is_active()) return;

    HDC hdc = GetDC(win->hWnd);
    BitBlt(hdc, ox, oy, mx*dx, my*dy,
           backbuf->hDC, 0, 0, SRCCOPY);
    ReleaseDC(win->hWnd, hdc);
}

void MapRegionClass::redraw(int x1, int y1, int x2, int y2)
{
    if (!flag) return;
    if (!is_active()) return;
    HDC hdc = GetDC(win->hWnd);
    BitBlt(hdc, ox, oy, dx*mx, dy*my,
           backbuf->hDC, 0, 0, SRCCOPY);
    ReleaseDC(win->hWnd, hdc);
}

void MapRegionClass::draw_data(unsigned char *buf, bool show_mark,
                               int mark_x, int mark_y)
{
    if (!flag)
        return;

    LPBYTE ppix ,dpix;
    int inc_x, inc_y, inc_x0, inc_y0;
    int bufx = mx * dx;
    int bufy = my * dy;

    bufx  = (bufx+3)/4;
    bufx *= 4;
#define BUF_IDX(x,y, x1, y1) ((x)*dx-(y)*dy*bufx + (x1) - (y1)*bufx)
    // upper left corner
    LPBYTE pDibBit0 = backbuf->pDibBits + bufx*(bufy-1);

    ppix  = pDibBit0;

    inc_x  = dx;
    inc_x0 = 1;

    inc_y  = - mx2 * inc_x + BUF_IDX(0, 1, 0, 0);
    inc_y0 = - dx * inc_x0 + BUF_IDX(0, 0, 0, 1);

    // erase old markers
    for (int j = 0; j < dy * marker_length; j++)
        *(pDibBit0 + BUF_IDX(old_mark_x, 0, dx/2 + x_margin, j)) = MAP_BLACK;

    for (int j = 0; j < dx * marker_length; j++)
        *(pDibBit0 + BUF_IDX(0, old_mark_y, j, dy/2 + y_margin)) = MAP_BLACK;

    force_redraw = true;

    dpix = ppix;
    for (int j = 0; j < my2; j++)
    {
        unsigned char *ptr = &buf[j * (mx2 - x_margin)];
        for (int i = 0; i < mx2; i++)
        {
            int col = (j >= my2 - y_margin || i >= mx2 - x_margin) ?
               MAP_BLACK : ptr[i];
            if ( col != get_col(i,j) || force_redraw
                 || i < marker_length || j < marker_length)
            {
                dpix = ppix;
                for (int y = 0; y < dy; y++)
                {
                    for (int x = 0; x < dx; x++)
                    {
                        *dpix = col;
                        dpix += inc_x0;
                    }
                    dpix += inc_y0;
                }
                set_col(col, i, j);
            }
            ppix += inc_x;
        }
        ppix += inc_y;
    }

    old_mark_x = mark_x;
    old_mark_y = mark_y;

    if (show_mark)
    {
        // draw new markers
        for (int j = 0; j < dy * marker_length; j++)
            *(pDibBit0 + BUF_IDX(mark_x, 0, dx/2 + x_margin, j)) = MAP_WHITE;

        for (int j = 0; j < dx * marker_length; j++)
            *(pDibBit0 + BUF_IDX(0, mark_y, j, dy/2 + y_margin)) = MAP_WHITE;
    }

    redraw();
    force_redraw = false;
}

/* XXXXX
 * img_type related
 */

LPBYTE dib_ref_pixel(dib_pack* dib, int x, int y)
{
    int w = ((3 + dib->Width)/4)*4;
    LPBYTE ref = dib->pDibBits + x + (dib->Height -1 -y) * w;
    return ref;
}

bool ImgIsTransparentAt(img_type img, int x, int y)
{
    if (pix_transparent == *( dib_ref_pixel(img, x, y) ))
        return true;

    return false;
}
void ImgSetTransparentPix(img_type img)
{
    pix_transparent = (BYTE)*(img->pDibZero);
}

img_type ImgCreateSimple(int wx, int wy)
{
    if (wx == 0 || wy == 0)
        return NULL;

    dib_pack *ptr = (dib_pack *)GlobalAlloc(GPTR, sizeof(dib_pack));

    ptr->pDibBits = (LPBYTE)GlobalAlloc(GPTR, wx*wy );
    ptr->pDibZero = ptr->pDibBits + (wy -1)* wx;
    ptr->Width    = wx;
    ptr->Height   = wy;

    ptr->pDib = NULL;
    ptr->hDib = NULL;
    ptr->hDC  = NULL;

    return ptr;
}

void ImgDestroy(img_type img)
{
    if (!img)
        return;

    if (img->pDib) GlobalFree(img->pDib);
    if (img->hDC)  DeleteDC  (img->hDC);
    if (img->hDib) DeleteObject(img->hDib);

    GlobalFree(img);
}

img_type ImgLoadFile(const char *name)
{
    HANDLE fh;
    DWORD dummy;
    BITMAPFILEHEADER bmHead;
    int BitsSize;
    HDC hdc1;
    dib_pack *img;

    hdc1 = GetDC(0);

    fh = CreateFile(name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);

    if (fh == INVALID_HANDLE_VALUE)
        return NULL;

    SetFilePointer(fh,0,NULL,FILE_BEGIN);

    if (!ReadFile(fh,&bmHead, sizeof(BITMAPFILEHEADER), &dummy, NULL))
        return NULL;

    img = (dib_pack *) GlobalAlloc(GPTR, sizeof(dib_pack));
    if (!img)
        return  NULL;

    img->pDib = (LPBITMAPINFO)GlobalAlloc(GPTR,
                (sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD)) );
    if (img->pDib == NULL)
    {
        GlobalFree(img);
        return NULL;
    }

    SetFilePointer(fh, sizeof(BITMAPFILEHEADER), NULL, FILE_BEGIN);
    if (!ReadFile(fh, img->pDib,
                  sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD),
                  &dummy, NULL))
    {
        GlobalFree(img->pDib);
        GlobalFree(img);
        return NULL;
    }

    img->hDib = CreateDIBSection(hdc1, img->pDib, DIB_RGB_COLORS,
                                (VOID **)&(img->pDibBits), NULL,0);
    if (img->hDib == NULL)
    {
        GlobalFree(img->pDib);
        GlobalFree(img);
        return NULL;
    }


    BitsSize = bmHead.bfSize-bmHead.bfOffBits;
    SetFilePointer(fh, bmHead.bfOffBits, NULL, FILE_BEGIN);

    if (!ReadFile(fh, img->pDibBits, BitsSize, &dummy, NULL))
    {
        GlobalFree(img->hDib);
        GlobalFree(img->pDib);
        GlobalFree(img);
        return NULL;
    }

    CloseHandle(fh);

    img->Width    = img->pDib->bmiHeader.biWidth ;
    img->Height   = img->pDib->bmiHeader.biHeight;
    img->pDibZero = img->pDibBits + (img->Height - 1) * img->Width;

    ReleaseDC(0, hdc1);
    return img;
}

void ImgClear(img_type img)
{
    int i;
    for (i = 0; i< (img->Width * img->Height); i++)
        *(img->pDibBits + i) = pix_transparent;
}

// Copy internal image to another internal image
void ImgCopy(img_type src,  int sx, int sy, int wx, int wy,
             img_type dest, int dx, int dy, int copy)
{
    int x, y;
    BYTE pix;

    if (copy)
    {
        for (x = 0; x < wx; x++)
            for (y = 0; y < wy; y++)
            {
                pix = *( dib_ref_pixel(src, sx+x, sy+y) );
                *( dib_ref_pixel(dest, dx+x, dy+y) ) = pix;
            }
    }
    else
    {
        for (x = 0; x < wx; x++)
            for (y = 0; y < wy; y++)
            {
                pix = *( dib_ref_pixel(src, sx+x, sy+y) );
                if (pix!=pix_transparent)
                    *( dib_ref_pixel(dest, dx+x, dy+y) ) = pix;
            }
    }
}

// Copy internal image to another internal image
void ImgCopyH(img_type src,  int sx, int sy, int wx, int wy,
                    img_type dest, int dx, int dy, int copy)
{
    int x, y;
    BYTE pix;

    if (copy)
    {
        for (x = 0; x < wx; x++)
            for (y = 0; y < wy; y++)
            {
                pix = *( dib_ref_pixel(src, sx+x, sy+y) );
                if (pix == pix_rimcolor) pix = pix_magenta;
                    *( dib_ref_pixel(dest, dx+x, dy+y) ) = pix;
            }
    }
    else
    {
        for (x = 0; x < wx; x++)
            for (y = 0; y < wy; y++)
            {
                pix = *( dib_ref_pixel(src, sx+x, sy+y) );

                if (pix == pix_rimcolor)
                    pix = pix_magenta;
                if (pix != pix_transparent)
                    *( dib_ref_pixel(dest, dx+x, dy+y) ) = pix;
            }
    }
}


// Copy internal image to another internal image
void ImgCopyMasked(img_type src,  int sx, int sy, int wx, int wy,
                    img_type dest, int dx, int dy, char *mask)
{
    int x, y;
    BYTE pix;
    int count = 0;
    for (y = 0; y < wy; y++)
        for (x = 0; x < wx; x++)
        {
            pix = *( dib_ref_pixel(src, sx+x, sy+y) );
            if (mask[count] == 0 && pix != pix_transparent)
                *( dib_ref_pixel(dest, dx+x, dy+y) ) = pix;
            count++;
        }
}

// Copy internal image to another internal image
void ImgCopyMaskedH(img_type src,  int sx, int sy, int wx, int wy,
                    img_type dest, int dx, int dy, char *mask)
{
    int x, y;
    BYTE pix;
    int count = 0;
    for (y = 0; y < wy; y++)
        for (x = 0; x < wx; x++)
        {
            pix = *( dib_ref_pixel(src, sx+x, sy+y) );
            if (pix == pix_rimcolor)
                pix = pix_magenta;
            if (mask[count] == 0 && pix != pix_transparent)
                *( dib_ref_pixel(dest, dx+x, dy+y) ) = pix;
            count++;
        }
}

void WinClass::fillrect(int left, int top, int right, int bottom, int color)
{
    HDC hdc = GetDC(hWnd);

    HBRUSH curbrush;
    HDC   curbrushhdc = NULL;
    RECT  currect;
    curbrush = CreateSolidBrush(term_pix[color]);
    currect.left   = left;
    currect.right  = right;
    currect.top    = top;
    currect.bottom = bottom;
    SelectObject(curbrushhdc, curbrush);
    FillRect(hdc, &currect, curbrush);
    DeleteObject(curbrush);
    DeleteDC(curbrushhdc);

    ReleaseDC(hWnd, hdc);
}


void TileRegionClass::DrawPanel(int left, int top, int width, int height)
{
    framerect(left, top , left + width, top + height, PIX_WHITE);
    framerect(left + 1, top + 1, left + width, top + height, PIX_DARKGREY);
    fillrect (left + 1, top + 1, left + width - 1, top + height -1, PIX_LIGHTGREY);
}

void RegionClass::framerect(int left, int top, int right, int bottom, int color)
{
    HDC hdc = GetDC(win->hWnd);

    HBRUSH curbrush;
    HDC    curbrushhdc = NULL;
    RECT   currect;
    curbrush = CreateSolidBrush(term_pix[color]);
    currect.left   = sx + left;
    currect.right  = sx + right;
    currect.top    = sy + top;
    currect.bottom = sy + bottom;
    SelectObject(curbrushhdc, curbrush);
    FrameRect(hdc, &currect, curbrush);
    DeleteObject(curbrush);
    DeleteDC(curbrushhdc);

    ReleaseDC(win->hWnd, hdc);
}

void TileRegionClass::framerect(int left, int top, int right, int bottom,
                    int color)
{
    HDC dhdc = backbuf->hDC;
    HBRUSH curbrush;
    HDC    curbrushhdc = NULL;
    RECT   currect;
    curbrush = CreateSolidBrush(term_pix[color]);
    currect.left   = left;
    currect.right  = right;
    currect.top    = top;
    currect.bottom = bottom;
    SelectObject(curbrushhdc, curbrush);
    FrameRect(dhdc, &currect, curbrush);
    DeleteObject(curbrush);
    DeleteDC(curbrushhdc);
}

void RegionClass::fillrect(int left, int top, int right, int bottom, int color)
{
    HDC hdc = GetDC(win->hWnd);

    HBRUSH curbrush;
    HDC    curbrushhdc = NULL;
    RECT   currect;
    curbrush = CreateSolidBrush(term_pix[color]);
    currect.left   = sx + left;
    currect.right  = sx + right;
    currect.top    = sy + top;
    currect.bottom = sy + bottom;
    SelectObject(curbrushhdc, curbrush);
    FillRect(hdc, &currect, curbrush);
    DeleteObject(curbrush);
    DeleteDC(curbrushhdc);

    ReleaseDC(win->hWnd, hdc);
}

void  TileRegionClass::fillrect(int left, int top, int right, int bottom,
                    int color)
{
    HDC dhdc = backbuf->hDC;
    HBRUSH curbrush;
    HDC    curbrushhdc = NULL;
    RECT   currect;
    curbrush = CreateSolidBrush(term_pix[color]);
    currect.left   = left;
    currect.right  = right;
    currect.top    = top;
    currect.bottom = bottom;
    SelectObject(curbrushhdc, curbrush);
    FillRect(dhdc, &currect, curbrush);
    DeleteObject(curbrush);
    DeleteDC(curbrushhdc);
}
