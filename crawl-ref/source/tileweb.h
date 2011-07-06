/**
 * @file
 * @brief Webtiles implementation of the tiles interface
**/

#ifndef TILEWEB_H
#define TILEWEB_H

#include "externs.h"
#include "tileweb-text.h"
#include "tiledoll.h"
#include "viewgeom.h"

class TilesFramework
{
public:
    TilesFramework();
    virtual ~TilesFramework();

    bool initialise();
    void shutdown();
    void load_dungeon(const crawl_view_buffer &vbuf, const coord_def &gc);
    void load_dungeon(const coord_def &gc);
    int getch_ck();
    void resize();
    void clrscr();

    void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
    GotoRegion get_cursor_region() const;
    int get_number_of_lines();
    int get_number_of_cols();

    void update_minimap(const coord_def &gc);
    void clear_minimap();
    void update_minimap_bounds();
    void update_tabs();

    void set_need_redraw(unsigned int min_tick_delay = 0);
    bool need_redraw() const;
    void redraw();

    void place_cursor(cursor_type type, const coord_def &gc);
    void clear_text_tags(text_tag_type type);
    void add_text_tag(text_tag_type type, const std::string &tag,
                      const coord_def &gc);
    void add_text_tag(text_tag_type type, const monster* mon);

    const coord_def &get_cursor() const;

    void add_overlay(const coord_def &gc, tileidx_t idx);
    void clear_overlays();

    void draw_doll_edit();

    // Webtiles-specific
    void textcolor(int col);
    void textbackground(int col);
    void put_string(char *str);
    void put_ucs_string(ucs_t *str);
    void clear_to_end_of_line();
    int wherex() { return m_print_x + 1; }
    int wherey() { return m_print_y + 1; }

    void write_message(const char *format, ...);
    void finish_message();
    void send_message(const char *format, ...);
protected:
    enum LayerID
    {
        LAYER_NORMAL,
        LAYER_CRT,
        LAYER_TILE_CONTROL,
        LAYER_MAX,
    };
    LayerID m_active_layer;
    
    unsigned int m_last_tick_redraw;
    bool m_need_redraw;

    coord_def m_origin;

    crawl_view_buffer m_current_view;
    coord_def m_current_gc;

    crawl_view_buffer m_next_view;
    coord_def m_next_gc;
    coord_def m_next_view_tl;
    coord_def m_next_view_br;

    int m_current_flash_colour;
    int m_next_flash_colour;

    coord_def m_cursor[CURSOR_MAX];
    coord_def m_last_clicked_grid;

    bool m_has_overlays;

    WebTextArea m_text_crt;
    WebTextArea m_text_stat;
    WebTextArea m_text_message;

    GotoRegion m_cursor_region;

    WebTextArea *m_print_area;
    int m_print_x, m_print_y;
    int m_print_fg, m_print_bg;

    dolls_data last_player_doll;

    bool _send_cell(int x, int y,
                    const screen_cell_t *screen_cell, screen_cell_t *old_screen_cell);

    void _send_current_view();
};

// Main interface for tiles functions
extern TilesFramework tiles;

#endif
