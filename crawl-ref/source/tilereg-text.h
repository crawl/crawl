/*
 *  File:       tilereg_text.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifdef USE_TILE
#ifndef TILEREG_TEXT_H
#define TILEREG_TEXT_H

#include "tilereg.h"

class TextRegion : public Region
{
public:
    TextRegion(FontWrapper *font);
    virtual ~TextRegion();

    virtual void render();
    virtual void clear();

    // STATIC -
    // TODO enne - move these to TilesFramework?

    // where now printing? what color?
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
    //static int get_number_of_lines(void);
    static void _setcursortype(int curstype);
    static void textbackground(int bg);
    static void textcolor(int col);

    // Object's method
    void clear_to_end_of_line(void);
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

protected:
    virtual void on_resize();
    FontWrapper *m_font;
};

#endif
#endif
