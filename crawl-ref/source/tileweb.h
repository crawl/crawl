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
#include "map_knowledge.h"
#include <map>
#include <bitset>
#include <sys/un.h>

class Menu;

enum WebtilesCRTMode
{
    CRT_DISABLED,
    CRT_NORMAL,
    CRT_MENU
};

enum WebtilesUIState
{
    UI_NORMAL,
    UI_CRT,
    UI_VIEW_MAP,
};

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
    void add_text_tag(text_tag_type type, const monster_info& mon);

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

    void push_menu(Menu* m);
    void push_crt_menu(std::string tag);
    void pop_menu();
    void close_all_menus();

    void write_message(const char *format, ...);
    void finish_message();
    void send_message(const char *format = "", ...);

    bool has_receivers() { return !m_dest_addrs.empty(); }
    bool is_controlled_from_web() { return m_controlled_from_web; }

    /* Webtiles can receive input both via stdin, and on the
       socket. Also, while waiting for input, it should be
       able to handle other control messages (for example,
       requests to re-send data when a new spectator joins).

       This function waits until input is available either via
       stdin or from a control message. If the input came from
       a control message, it will be written into c; otherwise,
       it still has to be read from stdin.

       If block is false, await_input will immediately return,
       even if no input is available. The return value indicates
       whether input can be read from stdin; c will be non-zero
       if input came via a control message.
     */
    bool await_input(wint_t& c, bool block);

    void check_for_control_messages();

    /* Adds a prefix that will be written before any other
       data that is sent after this call, unless no other
       data is sent until pop_prefix is called. The suffix
       passed to pop_prefix will only be sent if the prefix
       was sent. */
    void push_prefix(const std::string& prefix);
    void pop_prefix(const std::string& suffix);
    bool prefix_popped();

    // Helper functions for writing JSON
    void write_message_escaped(const std::string& s);
    void json_open_object(const std::string& name = "");
    void json_close_object();
    void json_open_array(const std::string& name = "");
    void json_close_array();
    void json_write_comma();
    void json_write_name(const std::string& name);
    void json_write_int(int value);
    void json_write_int(const std::string& name, int value);
    void json_write_string(const std::string& value);
    void json_write_string(const std::string& name, const std::string& value);

    std::string m_sock_name;
    bool m_await_connection;

    WebtilesCRTMode m_crt_mode;

    void clear_crt_menu() { m_text_menu.clear(); }

    void set_ui_state(WebtilesUIState state);
    WebtilesUIState get_ui_state() { return m_ui_state; }

protected:
    int m_sock;
    int m_max_msg_size;
    std::string m_msg_buf;
    std::vector<sockaddr_un> m_dest_addrs;

    bool m_controlled_from_web;

    void _await_connection();
    wint_t _handle_control_message(sockaddr_un addr, std::string data);
    wint_t _receive_control_message();

    std::vector<std::string> m_prefixes;
    int json_object_level;
    bool need_comma;

    struct MenuInfo
    {
        std::string tag;
        Menu* menu;
    };
    std::vector<MenuInfo> m_menu_stack;

    WebtilesUIState m_ui_state;

    unsigned int m_last_tick_redraw;
    bool m_need_redraw;

    coord_def m_origin;

    bool m_view_loaded;

    FixedArray<screen_cell_t, GXM, GYM> m_current_view;
    coord_def m_current_gc;

    FixedArray<screen_cell_t, GXM, GYM> m_next_view;
    coord_def m_next_gc;
    coord_def m_next_view_tl;
    coord_def m_next_view_br;

    std::bitset<GXM * GYM> m_dirty_cells;
    void mark_dirty(const coord_def& gc) { m_dirty_cells[gc.y * GXM + gc.x] = true; }
    void mark_clean(const coord_def& gc) { m_dirty_cells[gc.y * GXM + gc.x] = false; }
    bool is_dirty(const coord_def& gc) { return m_dirty_cells[gc.y * GXM + gc.x]; }

    int m_current_flash_colour;
    int m_next_flash_colour;

    FixedArray<map_cell, GXM, GYM> m_current_map_knowledge;
    std::map<uint32_t, coord_def> m_monster_locs;
    bool m_need_full_map;

    coord_def m_cursor[CURSOR_MAX];
    coord_def m_last_clicked_grid;

    bool m_has_overlays;

    WebTextArea m_text_crt;
    WebTextArea m_text_menu;
    WebTextArea m_text_stat;
    WebTextArea m_text_message;

    GotoRegion m_cursor_region;

    WebTextArea *m_print_area;
    int m_print_x, m_print_y;
    int m_print_fg, m_print_bg;

    dolls_data last_player_doll;

    void _send_version();

    void _send_everything();

    void _send_map(bool force_full = false);
    void _send_cell(const coord_def &gc,
                    const screen_cell_t &current_sc, const screen_cell_t &next_sc,
                    const map_cell &current_mc, const map_cell &next_mc,
                    std::map<uint32_t, coord_def>& new_monster_locs,
                    bool force_full);
    void _send_monster(const coord_def &gc, const monster_info* m,
                       std::map<uint32_t, coord_def>& new_monster_locs,
                       bool force_full);
};

// Main interface for tiles functions
extern TilesFramework tiles;

class tiles_crt_control
{
public:
    tiles_crt_control(bool crt_enabled)
        : m_old_mode(tiles.m_crt_mode)
    {
        tiles.m_crt_mode = crt_enabled ? CRT_NORMAL : CRT_DISABLED;
    }

    tiles_crt_control(WebtilesCRTMode mode,
                      std::string tag = "")
        : m_old_mode(tiles.m_crt_mode)
    {
        tiles.m_crt_mode = mode;
        if (mode == CRT_MENU)
        {
            tiles.push_crt_menu(tag);
        }
    }

    ~tiles_crt_control()
    {
        if (tiles.m_crt_mode == CRT_MENU)
        {
            tiles.pop_menu();
            tiles.clear_crt_menu();
        }
        tiles.m_crt_mode = m_old_mode;
    }

private:
    WebtilesCRTMode m_old_mode;
};

class tiles_ui_control
{
public:
    tiles_ui_control(WebtilesUIState state)
        : m_new_state(state), m_old_state(tiles.get_ui_state())
    {
        tiles.set_ui_state(state);
    }

    ~tiles_ui_control()
    {
        if (tiles.get_ui_state() == m_new_state)
            tiles.set_ui_state(m_old_state);
    }

private:
    WebtilesUIState m_new_state;
    WebtilesUIState m_old_state;
};

#endif
