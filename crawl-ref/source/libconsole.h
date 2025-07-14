#pragma once

#include "defines.h"

void console_startup();
void console_shutdown();

class crawl_view_buffer;

int get_number_of_lines();
int get_number_of_cols();
int num_to_lines(int num);

void clrscr();
void clrscr_sys();
void clear_to_end_of_line();
void gotoxy_sys(int x, int y);
void textcolour(int c);
void textbackground(int c);
COLOURS default_hover_colour();
struct lib_display_info
{
    lib_display_info();
    string type;
    string term;
    int fg_colors;
    int bg_colors;
};
void cprintf(const char *format, ...);

int wherex();
int wherey();
void putwch(char32_t c);
int getch_ck();
bool kbhit();
void delay(unsigned int ms);
void puttext(int x, int y, const crawl_view_buffer &vbuf);
void update_screen();

void set_cursor_enabled(bool enabled);
bool is_cursor_enabled();
void set_mouse_enabled(bool enabled);

static inline void put_colour_ch(int colour, unsigned ch)
{
    textcolour(colour);
    putwch(ch);
}

struct coord_def;
coord_def cgetpos(GotoRegion region = GOTO_CRT);
void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
GotoRegion get_cursor_region();
void set_cursor_region(GotoRegion region);

bool valid_cursor_pos(int x, int y, GotoRegion region);
void assert_valid_cursor_pos();

struct save_cursor_pos
{
    save_cursor_pos()
        : region(get_cursor_region()), pos(cgetpos(region))
    {
        ASSERTM(valid_cursor_pos(pos.x, pos.y, region),
            "invalid cursor position %d,%d in region %d", pos.x, pos.y, region);
    };
    ~save_cursor_pos()
    {
        cgotoxy(pos.x, pos.y, region);
    };

private:
    GotoRegion region;
    coord_def pos;
};
