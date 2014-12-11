/**
 * @file
 * @brief Functions that any display port needs to implement.
**/

#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cio.h"
#include "defines.h"
#include "env.h"
#include "message.h"
#include "state.h"
#include "terrain.h"
#include "tiledef-main.h"
#include "tilereg-text.h"
#include "travel.h"
#include "viewgeom.h"
#include "windowmanager.h"

int m_getch()
{
    return getchk();
}

void set_mouse_enabled(bool enabled)
{
    crawl_state.mouse_enabled = enabled;
}

void gui_init_view_params(crawl_view_geometry &geom)
{
    // The tile version handles its own layout on a pixel-by-pixel basis.
    // Pretend that all of the regions start at character location (1,1).

    geom.termp.x = 1;
    geom.termp.y = 1;

    geom.termsz.x = 80;
    geom.termsz.y = 24;

    geom.viewp.x = 1;
    geom.viewp.y = 1;

    geom.hudp.x = 1;
    geom.hudp.y = 1;

    geom.msgp.x = 1;
    geom.msgp.y = 1;

    geom.mlistp.x  = 1;
    geom.mlistp.y  = 1;
    geom.mlistsz.x = 0;
    geom.mlistsz.y = 0;

    geom.viewsz.x = 17;
    geom.viewsz.y = 17;
}

void putwch(ucs_t chr)
{
    if (!chr)
        chr = ' ';
    TextRegion::text_mode->putwch(chr);
}

void clear_to_end_of_line()
{
    // object's method
    TextRegion::text_mode->clear_to_end_of_line();
}

void cprintf(const char *format,...)
{
    char buffer[2048];          // One full screen if no control seq...
    va_list argp;
    va_start(argp, format);
    vsnprintf(buffer, sizeof(buffer), format, argp);
    va_end(argp);
    // object's method
    TextRegion::text_mode->addstr(buffer);
}

void textcolour(int colour)
{
    TextRegion::textcolour(colour);
}

void textbackground(int bg)
{
    TextRegion::textbackground(bg);
}

void set_cursor_enabled(bool enabled)
{
    if (enabled)
        TextRegion::_setcursortype(1);
    else
        TextRegion::_setcursortype(0);
}

bool is_cursor_enabled()
{
    if (TextRegion::cursor_flag)
        return true;

    return false;
}

bool is_smart_cursor_enabled()
{
    return false;
}

void enable_smart_cursor(bool /*cursor*/)
{
}

int wherex()
{
    return TextRegion::wherex();
}

int wherey()
{
    return TextRegion::wherey();
}

int get_number_of_lines()
{
    return tiles.get_number_of_lines();
}

int get_number_of_cols()
{
    return tiles.get_number_of_cols();
}

int num_to_lines(int num)
{
    return tiles.to_lines(num);
}

int getch_ck()
{
    return tiles.getch_ck();
}

int getchk()
{
    return getch_ck();
}

void clrscr()
{
    tiles.clrscr();
}

void cgotoxy(int x, int y, GotoRegion region)
{
    tiles.cgotoxy(x, y, region);
}

coord_def cgetpos(GotoRegion region)
{
    ASSERT(region == get_cursor_region());
    return coord_def(wherex(), wherey());
}

GotoRegion get_cursor_region()
{
    return tiles.get_cursor_region();
}

void delay(unsigned int ms)
{
    if (crawl_state.disables[DIS_DELAY])
        return;

    tiles.redraw();
    wm->delay(ms);
}

void update_screen()
{
    if (crawl_state.tiles_disabled)
        return;
    tiles.set_need_redraw();
}

bool kbhit()
{
    if (crawl_state.tiles_disabled)
        return false;
    // Look for the presence of any keyboard events in the queue.
    int count = wm->get_event_count(WME_KEYDOWN)
                + wm->get_event_count(WME_KEYPRESS);
    return count > 0;
}

void console_startup()
{
    tiles.resize();
}

void console_shutdown()
{
    tiles.shutdown();
}
#endif // #ifdef USE_TILE_LOCAL
