#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg.h"

class TextRegion : public Region
{
public:
    TextRegion(FontWrapper *font);
    virtual ~TextRegion();

    virtual void render() override;
    virtual void clear() override;

    // STATIC -
    // TODO enne - move these to TilesFramework?

    // where now printing? what colour?
    static int print_x;
    static int print_y;
    static int text_col;
    // which region now printing?
    static class TextRegion *text_mode;
    // display cursor? where is the cursor now?
    static int cursor_flag;
    static class TextRegion *cursor_region;
    static int cursor_x;
    static int cursor_y;

    // class methods
    static void cgotoxy(int x, int y);
    static int wherex();
    static int wherey();
    //static int get_number_of_lines();
    static void _setcursortype(int curstype);
    static void textbackground(int bg);
    static void textcolour(int col);

    // Object's method
    void clear_to_end_of_line();
    void putwch(char32_t chr);

    char32_t *cbuf; //text backup
    uint8_t *abuf; //textcolour backup

    int cx_ofs; //cursor x offset
    int cy_ofs; //cursor y offset

    void addstr(const char *buffer);
    void addstr_aux(const char32_t *buffer, int len);
    void adjust_region(int *x1, int *x2, int y);
    void scroll();

protected:
    virtual void on_resize() override;
    FontWrapper *m_font;
};

#endif
