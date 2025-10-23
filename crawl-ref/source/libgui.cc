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
#include "rltiles/tiledef-main.h"
#include "tilereg-text.h"
#include "tiles-build-specific.h"
#include "travel.h"
#include "viewgeom.h"
#include "windowmanager.h"

void enter_headless_mode()
{
    crawl_state.tiles_disabled = true;
}

bool in_headless_mode()
{
    return crawl_state.tiles_disabled;
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

void putwch(char32_t chr)
{
    if (in_headless_mode())
        return;
    if (!chr)
        chr = ' ';
    TextRegion::text_mode->putwch(chr);
}

void clear_to_end_of_line()
{
    if (in_headless_mode())
        return;
    // object's method
    TextRegion::text_mode->clear_to_end_of_line();
}

void cprintf(const char *format,...)
{
    if (in_headless_mode())
        return;
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
    if (in_headless_mode())
        return;
    TextRegion::textcolour(colour);
}

void textbackground(int bg)
{
    if (in_headless_mode())
        return;
    TextRegion::textbackground(bg);
}

COLOURS default_hover_colour()
{
    return DARKGREY;
}

lib_display_info::lib_display_info()
    : type("SDL Tiles"),
    term("N/A"),
    // this means that the lib supports the 16 ansi colors, not that it can't
    // do more
    fg_colors(16),
    bg_colors(16)
{
}

void set_cursor_enabled(bool enabled)
{
    if (in_headless_mode())
        return;
    if (enabled)
        TextRegion::_setcursortype(1);
    else
        TextRegion::_setcursortype(0);
}

bool is_cursor_enabled()
{
    if (in_headless_mode())
        return false;
    if (TextRegion::cursor_flag)
        return true;

    return false;
}

int wherex()
{
    return TextRegion::wherex();
}

int wherey()
{
    return TextRegion::wherey();
}

#define HEADLESS_LINES 24
#define HEADLESS_COLS 80

int get_number_of_lines()
{
    if (crawl_state.tiles_disabled)
        return HEADLESS_LINES;
    else
        return tiles.get_number_of_lines();
}

int get_number_of_cols()
{
    if (crawl_state.tiles_disabled)
        return HEADLESS_COLS;
    else
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

void set_cursor_region(GotoRegion region)
{
    tiles.set_cursor_region(region);
}

void delay(unsigned int ms)
{
    if (crawl_state.disables[DIS_DELAY])
        return;

    update_screen();

    if (wm)
        wm->delay(ms);
}

void update_screen(unsigned int min_delay_ms)
{
    if (tiles.need_redraw(min_delay_ms))
        tiles.redraw();
}

bool kbhit()
{
    if (crawl_state.tiles_disabled || crawl_state.seen_hups)
        return false;
    // Look for the presence of any keyboard events in the queue.
    return wm->next_event_is(WME_KEYDOWN);
}

void console_startup()
{
    if (!crawl_state.tiles_disabled)
        tiles.resize();
}

void console_shutdown()
{
    if (!crawl_state.tiles_disabled)
        tiles.shutdown();
}
#endif // #ifdef USE_TILE_LOCAL
