/*
 *  File:       guic-win.cc
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
 */

#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>

// GDT_NONE in commctrl.h conflicts with enum.h
#ifdef GDT_NONE
#undef GDT_NONE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <fcntl.h>

#include "AppHdr.h"
#include "cio.h"
#include "debug.h"
#include "externs.h"
#include "files.h" 
#include "guic.h"
#include "itemprop.h"
#include "state.h"
#include "tiles.h"

extern int old_main(int argc, char *argv[]);

extern WinClass *win_main;
extern TileRegionClass *region_tile;
extern img_type TileImg;
extern BYTE pix_transparent;

typedef struct ev_data
{
    int type;
    int key;
    int x1, y1;
    bool sh, ct;
}ev_data;

#define EV_MAX 1024
struct ev_data ev_cue[EV_MAX];

static int ev_head = 0;
static int ev_tail = 0;
void ev_push(struct ev_data *e);
static bool skip_key = false;

// Tip text
static TOOLINFO tiTip;
static HWND hTool;
static HWND pWin;

#ifdef USE_TILE
#define PAL_STD    0  //standard palette
#define PAL_BER    1  //berserk palette
#define PAL_SHA    2  //shadow palette
static int palette = PAL_STD;
LPBYTE lpPalettes[3] ;
#endif

#define DF_TOOLTIP_WIDTH  300

ATOM   MyRegisterClass( HINSTANCE hInstance );
BOOL   InitInstance( HINSTANCE, int );
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );

bool libgui_init_sys();
void libgui_shutdown_sys();
void update_tip_text(const char *tip);

void GetNextEvent(int *etype, int *key, bool *shift, bool *ctrl,
        int *x1, int *y1, int *x2, int *y2);
void TileInitWin();
void delay(unsigned long ms);
int kbhit();

/***************************/
void TileInitWin()
{
    int i;

    TileImg->pDib->bmiColors[pix_transparent].rgbRed   = 0;
    TileImg->pDib->bmiColors[pix_transparent].rgbGreen = 0;
    TileImg->pDib->bmiColors[pix_transparent].rgbBlue  = 0;

    RegionClass::set_std_palette(&TileImg->pDib->bmiColors[0]);

    WORD chcol;
    lpPalettes[0] = (LPBYTE) (&TileImg->pDib->bmiColors[0]);
    lpPalettes[1] = (LPBYTE)GlobalAlloc(GPTR, 256 * sizeof(RGBQUAD) );
    lpPalettes[2] = (LPBYTE)GlobalAlloc(GPTR, 256 * sizeof(RGBQUAD) );
    for (i = 0; i < 256; i++)
    {
        chcol = (TileImg->pDib->bmiColors[i].rgbRed  * 30
              + TileImg->pDib->bmiColors[i].rgbGreen * 59
              + TileImg->pDib->bmiColors[i].rgbBlue  * 11)/100;
    LPBYTE ptr = lpPalettes[1] + i * sizeof(RGBQUAD);
        ptr[2] = (BYTE)chcol;
        ptr[0] = (BYTE)( (chcol +1)/6 );
        ptr[1] = (BYTE)( (chcol +1)/6 );

    ptr = lpPalettes[2] + i * sizeof(RGBQUAD);
        ptr[2] = ptr[0] = ptr[1] = (BYTE)chcol;
    }
}

void update_tip_text(const char *tip)
{
#define MAXTIP 512
    static char oldtip[MAXTIP+1];
    if (strncmp(oldtip, tip, MAXTIP)==0) return;
    strncpy(oldtip, tip, MAXTIP);
    tiTip.lpszText = (char *)tip;
    SendMessage(hTool, TTM_UPDATETIPTEXT, 0, (LPARAM)&tiTip);
}

bool libgui_init_sys( )
{
    return true;
}

void libgui_shutdown_sys()
{
    GlobalFree(lpPalettes[1]);
    GlobalFree(lpPalettes[2]);
    DestroyWindow( pWin );
}

void GetNextEvent(int *etype, int *key, bool *sh, bool *ct,
        int *x1, int *y1, int *x2, int *y2)
{
    MSG msg;

    while( GetMessage(&msg, NULL, 0, 0) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
        if(ev_tail != ev_head) break;
    }
    struct ev_data *e = &ev_cue[ev_head];
    ev_head++;
    if(ev_head == EV_MAX)
        ev_head = 0;

    *etype = e->type;
    *key = e->key;
    *sh =  e->sh;
    *ct =  e->ct;
    *x1 =  e->x1;
    *y1 =  e->y1;
}

void ev_push(struct ev_data *e)
{
    ev_cue[ev_tail] = *e;
    ev_tail++;
    if(ev_tail == EV_MAX)
        ev_tail = 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow )
{
    MSG msg;

    MyRegisterClass( hInstance );

    GuicInit(hInstance, nCmdShow);

    // Redirect output to the console
    AttachConsole(ATTACH_PARENT_PROCESS);
    freopen("CONOUT$", "wb", stdout);
    freopen("CONOUT$", "wb", stderr);

    // I'll be damned if I have to parse lpCmdLine myself...
    int argc;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char **argv = new char*[argc];
    int args_len = wcslen(GetCommandLineW()) + argc;
    char *args = new char[args_len];

    char *ptr = args;
    for (int i = 0; i < argc; i++)
    {
        wsprintfA(ptr, "%S", wargv[i]);
        argv[i] = ptr;
        ptr += strlen(argv[i]) + 1;
    }
    ASSERT(ptr <= args + args_len);

    old_main(argc, argv);

    delete args;
    delete argv;

    FreeConsole();

    return msg.wParam;
}


ATOM MyRegisterClass( HINSTANCE hInstance )
{
    WNDCLASSEX wcex;

    wcex.cbSize        = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = (WNDPROC)WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = LoadIcon(hInstance, TEXT("CRAWL_ICON"));
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName  = "CRAWL";
    wcex.lpszClassName = "CrawlList";
    wcex.hIconSm       = NULL;//LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

    return RegisterClassEx( &wcex );
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    struct ev_data ev;
    static int clix = 0;
    static int cliy = 0;

    // For keypad
    const unsigned char ck_table[9]=
    {
        CK_END,  CK_DOWN,   CK_PGDN,
        CK_LEFT, CK_INSERT, CK_RIGHT,
        CK_HOME, CK_UP,     CK_PGUP
    };

    switch( message )
    {
        case WM_CREATE:
        {
            pWin = hWnd;

            // Example taken from
            // http://black.sakura.ne.jp/~third/system/winapi/common10.html
            InitCommonControls();
            hTool = CreateWindowEx( 0 , TOOLTIPS_CLASS ,
                    NULL , TTS_ALWAYSTIP ,
                    CW_USEDEFAULT , CW_USEDEFAULT ,
                    CW_USEDEFAULT , CW_USEDEFAULT ,
                    hWnd , NULL , ((LPCREATESTRUCT)(lParam))->hInstance ,
                    NULL
            );
            GetClientRect(hWnd , &tiTip.rect);

            tiTip.cbSize = sizeof (TOOLINFO);
            tiTip.uFlags = TTF_SUBCLASS;
            tiTip.hwnd = hWnd;
            tiTip.lpszText = (char *)
                "This text will tell you" EOL
                " what you are pointing";
            SendMessage(hTool, TTM_ADDTOOL , 0 , (LPARAM)&tiTip);
            // Allow line wrap
            SendMessage(hTool, TTM_SETMAXTIPWIDTH, 0, DF_TOOLTIP_WIDTH);
            return 0;
        }

        case WM_RBUTTONDOWN:
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        {
            ev.type = EV_BUTTON;
            ev.x1 = LOWORD(lParam);
            ev.y1 = HIWORD(lParam);
            ev.key = 1;
            ev.sh = ((GetKeyState(VK_SHIFT) & 0x80)!=0)? true:false;
            ev.ct = ((GetKeyState(VK_CONTROL) & 0x80)!=0)? true:false;
            if (message == WM_RBUTTONDOWN) ev.key = 2;
            if (message == WM_MBUTTONDOWN) ev.key = 3;
            ev_push(&ev);
            return 0;
        }

        case WM_RBUTTONUP:
        {
            ev.type = EV_UNBUTTON;
            ev.x1 = LOWORD(lParam);
            ev.y1 = HIWORD(lParam);
            ev.key = 1;
            if (message == WM_RBUTTONUP) ev.key = 2;
            if (message == WM_MBUTTONUP) ev.key = 3;
            ev_push(&ev);
            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            int z = (short)HIWORD(wParam);
            ev.x1 = LOWORD(lParam) - clix;
            ev.y1 = HIWORD(lParam) - cliy;
        ev.type = EV_BUTTON;
            ev.sh = ((GetKeyState(VK_SHIFT) & 0x80)!=0)? true:false;
            ev.ct = ((GetKeyState(VK_CONTROL) & 0x80)!=0)? true:false;
            ev.key = (z>0)? 4:5;
        ev_push(&ev);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
        ev.type = EV_MOVE;
            ev.x1 = LOWORD(lParam);
            ev.y1 = HIWORD(lParam);
        ev_push(&ev);
            return 0;
        }

        case WM_KEYDOWN:
        {
            int ch=(int)wParam;
            int result = 0;
        int dir = 0;
            bool fs = ((GetKeyState(VK_SHIFT)  & 0x80)!=0)? true:false;
            bool fc = ((GetKeyState(VK_CONTROL)& 0x80)!=0)? true:false;
            bool fa = ((GetKeyState(VK_MENU)   & 0x80)!=0)? true:false;

        if (ch >= VK_NUMPAD1 && ch <= VK_NUMPAD9)
        {
        skip_key = true;
        dir = ch - VK_NUMPAD0;
        }
        else
            if ((VK_PRIOR <= ch && ch <=VK_DOWN) || (ch == VK_CLEAR) )
            {
                switch(ch)
                {
                    case VK_LEFT:  dir = 4; break;
                    case VK_RIGHT: dir = 6; break;
                    case VK_UP:    dir = 8; break;
                    case VK_DOWN:  dir = 2; break;
                    case VK_HOME:  dir = 7; break;
                    case VK_PRIOR: dir = 9; break;
                    case VK_NEXT:  dir = 3; break;
                    case VK_END:   dir = 1; break;
                    case VK_CLEAR: dir = 5; break;
                }
        }

            if (dir != 0)
            {
        ch = ck_table[dir-1];
                if (fc)
            ch += CK_CTRL_UP - CK_UP;
                if (fs)
            ch += CK_SHIFT_UP - CK_UP;

                if (fa) ch |= 2048;
                ev.key = ch;
            ev.type = EV_KEYIN;
            ev_push(&ev);
                return 0;
            }
        else if ( ch >= VK_PAUSE
        && !(ch >= VK_CAPITAL && ch <= VK_SPACE)
        && ch !=VK_PROCESSKEY
        && ch != VK_NUMLOCK
        && !(ch >=VK_LSHIFT && ch<= VK_RMENU)
        && !(ch >= 0x30 && ch<= 0x39)
        && !(ch >= 0x41 && ch<= 0x5a)
        && !(ch >= 0xa6 && ch<= 0xe4)
           )
            {
                result = 300+ch;
            }

            if (result)
            {
                if (fs) result |= 512;
                if (fc) result |= 1024;
                if (fa) result |= 2048;
                ev.key = result;
                ev.type = EV_KEYIN;
                ev_push(&ev);
            }

            return 0;
        }

        case WM_CHAR:
        {
        if (skip_key)
        {
        skip_key = false;
        return 0;
        }
            ev.key = (int)wParam;
        ev.type = EV_KEYIN;
        ev_push(&ev);
            return 0;
        }

        case WM_PAINT:
        {
            hdc = BeginPaint(hWnd, &ps);
            win_main->redraw();
            EndPaint(hWnd, &ps);
            return 0;
        }

    case WM_MOVE:
    {
        clix = LOWORD(lParam);
        cliy = HIWORD(lParam);
        return 0;
    }

        case WM_COMMAND:
        {
            wmId    = LOWORD(wParam);
            wmEvent = HIWORD(wParam);
            switch( wmId )
            {
                default:
                    return DefWindowProc( hWnd, message, wParam, lParam );
            }
            return 0;
        }
        case WM_CLOSE:
            if ( (mouse_get_mode() == MOUSE_MODE_COMMAND)
               ||(!crawl_state.need_save) )
            {
                if (crawl_state.need_save)
                {
                    save_game(true);
                }
        libgui_shutdown();
            }
            break;
        case WM_DESTROY:
        {
            PostQuitMessage( 0 );
            exit(0);
            break;
        }
        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }
    return 0;
}

#ifdef USE_TILE
void TileDrawDungeonAux(){
    dib_pack *pBuf = region_tile->backbuf;

    int new_palette = PAL_STD;
    if (you.duration[DUR_BERSERKER]) new_palette = PAL_BER;
    if (you.special_wield == SPWLD_SHADOW) new_palette = PAL_SHA;

    if (new_palette != palette)
    {
    palette = new_palette;
        SetDIBColorTable(pBuf->hDC, 0, 256, (RGBQUAD *)lpPalettes[palette]);
    }
}
#endif

bool windows_change_font(char *font_name, int *font_size, bool dos)
{
    CHOOSEFONT cf;

    memset(&cf, 0, sizeof(cf));
    cf.lStructSize = sizeof(cf);
    cf.iPointSize = *font_size;
    cf.nSizeMin = 8;
    cf.nSizeMax = 24;
    cf.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_NOVERTFONTS
        | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE | CF_FORCEFONTEXIST;

    LOGFONT lf;
    strcpy(lf.lfFaceName, font_name);
    lf.lfHeight        = 16;
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
    lf.lfPitchAndFamily= FF_MODERN|FIXED_PITCH;
    lf.lfCharSet       = (dos)? OEM_CHARSET:ANSI_CHARSET;
    cf.lpLogFont = &lf;

    if (ChooseFont(&cf))
    {
        *font_size = (cf.iPointSize / 10);
        strcpy(font_name, lf.lfFaceName);
    return true;
    }
    return false;
}

void windows_get_winpos(int *x, int *y)
{
    WINDOWPLACEMENT wndpl;

    // set length
    wndpl.length = sizeof( WINDOWPLACEMENT );

    if( GetWindowPlacement( win_main->hWnd, &wndpl ) != 0 )
    {
        *x = wndpl.rcNormalPosition.top;
        *y = wndpl.rcNormalPosition.left;
    }
}

void delay(unsigned long ms)
{
   Sleep((DWORD)ms);
}

int kbhit()
{
    MSG msg;

    if (PeekMessage(&msg, NULL, WM_CHAR, WM_CHAR, PM_NOREMOVE)
        || PeekMessage(&msg, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE))
        return 1;
    else
        return 0;
}

void update_screen()
{
}
