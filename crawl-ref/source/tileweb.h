/**
 * @file
 * @brief Webtiles implementation of the tiles interface
**/

#pragma once

#ifdef USE_TILE_WEB

#include <bitset>
#include <map>
#include <sys/un.h>

#include "cursor-type.h"
#include "equipment-type.h"
#include "map-cell.h"
#include "map-knowledge.h"
#include "status.h"
#include "text-tag-type.h"
#include "tiledoll.h"
#include "tilemcache.h"
#include "tileweb-text.h"
#include "viewgeom.h"

class Menu;

enum WebtilesUIState
{
    UI_INIT = -1,
    UI_NORMAL,
    UI_VIEW_MAP,
};

struct player_info
{
    player_info();

    string name;
    string job_title;
    bool wizard;
    string species;
    string god;
    bool under_penance;
    uint8_t piety_rank;

    uint8_t form;

    int hp, hp_max, real_hp_max, poison_survival;
    int mp, mp_max, dd_real_mp_max;
    int contam;
    int noise;
    int adjusted_noise;

    int armour_class;
    int evasion;
    int shield_class;

    int8_t strength, strength_max;
    int8_t intel, intel_max;
    int8_t dex, dex_max;

    int experience_level;
    int8_t exp_progress;
    int gold;
    int zot_points;
    int elapsed_time;
    int num_turns;
    int lives, deaths;

    string place;
    int depth;
    coord_def position;

    vector<status_info> status;

    FixedVector<item_info, ENDOFPACK> inv;
    FixedVector<int8_t, NUM_EQUIP> equip;
    int8_t quiver_item;
    string unarmed_attack;
    uint8_t unarmed_attack_colour;
    bool quiver_available;
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
    void layout_reset();

    void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);

    void update_minimap(const coord_def &gc);
    void clear_minimap();
    void update_minimap_bounds();
    void update_tabs();

    void mark_for_redraw(const coord_def& gc);
    void set_need_redraw(unsigned int min_tick_delay = 0);
    bool need_redraw() const;
    void redraw();

    void place_cursor(cursor_type type, const coord_def &gc);
    void clear_text_tags(text_tag_type type);
    void add_text_tag(text_tag_type type, const string &tag,
                      const coord_def &gc);
    void add_text_tag(text_tag_type type, const monster_info& mon);

    const coord_def &get_cursor() const;

    void add_overlay(const coord_def &gc, tileidx_t idx);
    void clear_overlays();

    void draw_doll_edit();

    // Webtiles-specific
    void textcolour(int col);
    void textbackground(int col);
    void put_ucs_string(char32_t *str);
    void clear_to_end_of_line();

    void push_menu(Menu* m);
    void push_crt_menu(string tag);
    bool is_in_crt_menu();
    bool is_in_menu(Menu* m);
    void pop_menu();
    void push_ui_layout(const string& type, unsigned num_state_slots);
    void pop_ui_layout();
    void pop_all_ui_layouts();
    void ui_state_change(const string& type, unsigned state_slot);
    void push_ui_cutoff();
    void pop_ui_cutoff();

    void send_exit_reason(const string& type, const string& message = "");
    void send_dump_info(const string& type, const string& filename);

    string get_message();
    void write_message(PRINTF(1, ));
    void finish_message();
    void send_message(PRINTF(1, ));
    void flush_messages();

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

    // Helper functions for writing JSON
    void write_message_escaped(const string& s);
    void json_open_object(const string& name = "");
    void json_close_object(bool erase_if_empty = false);
    void json_open_array(const string& name = "");
    void json_close_array(bool erase_if_empty = false);
    void json_write_comma();
    void json_write_name(const string& name);
    void json_write_int(int value);
    void json_write_int(const string& name, int value);
    void json_write_bool(bool value);
    void json_write_bool(const string& name, bool value);
    void json_write_null();
    void json_write_null(const string& name);
    void json_write_string(const string& value);
    void json_write_string(const string& name, const string& value);
    /* Causes the current object/array to be erased if it is closed
       with erase_if_empty without writing any other content after
       this call */
    void json_treat_as_empty();
    void json_treat_as_nonempty();
    bool json_is_empty();

    string m_sock_name;
    bool m_await_connection;

    void set_text_cursor(bool enabled);
    void set_ui_state(WebtilesUIState state);
    WebtilesUIState get_ui_state() { return m_ui_state; }

    void dump();
    void update_input_mode(mouse_mode mode);

    void send_mcache(mcache_entry *entry, bool submerged,
                     bool send = true);
    void write_tileidx(tileidx_t t);

    void zoom_dungeon(bool in);

    void send_doll(const dolls_data &doll, bool submerged, bool ghost);

protected:
    int m_sock;
    int m_max_msg_size;
    string m_msg_buf;
    vector<sockaddr_un> m_dest_addrs;

    bool m_controlled_from_web;
    bool m_need_flush;

    bool _send_lock; // not thread safe

    void _await_connection();
    wint_t _handle_control_message(sockaddr_un addr, string data);
    wint_t _receive_control_message();

    struct JsonFrame
    {
        int start;
        int prefix_end;
        char type; // '}' or ']'
    };
    vector<JsonFrame> m_json_stack;

    void json_open(const string& name, char opener, char type);
    void json_close(bool erase_if_empty, char type);

    struct UIStackFrame
    {
        enum { MENU, CRT, UI, } type;
        Menu* menu;
        string crt_tag;
        vector<string> ui_json;
        bool centred;
    };
    vector<UIStackFrame> m_menu_stack;

    WebtilesUIState m_ui_state;
    WebtilesUIState m_last_ui_state;
    vector<int> m_ui_cutoff_stack;

    unsigned int m_last_tick_redraw;
    bool m_need_redraw;
    bool m_layout_reset;

    coord_def m_origin;

    bool m_view_loaded;
    bool m_player_on_level;

    crawl_view_buffer m_current_view;
    coord_def m_current_gc;

    crawl_view_buffer m_next_view;
    coord_def m_next_gc;
    coord_def m_next_view_tl;
    coord_def m_next_view_br;

    bitset<GXM * GYM> m_dirty_cells;
    bitset<GXM * GYM> m_cells_needing_redraw;
    void mark_dirty(const coord_def& gc);
    void mark_clean(const coord_def& gc);
    bool is_dirty(const coord_def& gc);
    bool cell_needs_redraw(const coord_def& gc);

    int m_current_flash_colour;
    int m_next_flash_colour;

    FixedArray<map_cell, GXM, GYM> m_current_map_knowledge;
    map<uint32_t, coord_def> m_monster_locs;
    bool m_need_full_map;

    coord_def m_cursor[CURSOR_MAX];
    coord_def m_last_clicked_grid;
    bool m_text_cursor;
    bool m_last_text_cursor;

    bool m_has_overlays;

    WebTextArea m_text_menu;

    GotoRegion m_cursor_region;

    WebTextArea *m_print_area;
    int m_print_x, m_print_y;
    int m_print_fg, m_print_bg;

    dolls_data last_player_doll;

    player_info m_current_player_info;

    void _send_version();
    void _send_options();
    void _send_layout();

    void _send_everything();

    bool m_mcache_ref_done;
    void _mcache_ref(bool inc);

    void _send_cursor(cursor_type type);
    void _send_map(bool force_full = false);
    void _send_cell(const coord_def &gc,
                    const screen_cell_t &current_sc, const screen_cell_t &next_sc,
                    const map_cell &current_mc, const map_cell &next_mc,
                    map<uint32_t, coord_def>& new_monster_locs,
                    bool force_full);
    void _send_monster(const coord_def &gc, const monster_info* m,
                       map<uint32_t, coord_def>& new_monster_locs,
                       bool force_full);
    void _send_player(bool force_full = false);
    void _send_item(item_info& current, const item_info& next,
                    bool force_full);
    void _send_messages();
};

// Main interface for tiles functions
extern TilesFramework tiles;

class tiles_crt_popup
{
public:
    tiles_crt_popup(string tag = "")
    {
        tiles.push_crt_menu(tag);
    }

    ~tiles_crt_popup()
    {
        tiles.pop_menu();
    }
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
