/**
 * @file
 * @brief SDL-related functionality for the tiles port
**/

#pragma once

#ifdef USE_TILE_LOCAL

#include "cursor-type.h"
#include "text-tag-type.h"
#include "tilereg.h"

#ifndef PROPORTIONAL_FONT
# error PROPORTIONAL_FONT not defined
#endif
#ifndef MONOSPACED_FONT
# error MONOSPACED_FONT not defined
#endif

class Popup;
class Region;
class CRTRegion;
class CRTRegionSingleSelect;
class MenuRegion;
class PopupRegion;
class TileRegion;
class DungeonRegion;
class GridRegion;
class InventoryRegion;
class AbilityRegion;
class SpellRegion;
class MemoriseRegion;
class ActorRegion;
class MonsterRegion;
class SkillRegion;
class CommandRegion;
class ActorRegion;
class TabbedRegion;
class MapRegion;
class ControlRegion;
class TitleRegion;
class DollEditRegion;
class StatRegion;
class MessageRegion;

struct map_cell;
struct crawl_view_geometry;

void gui_init_view_params(crawl_view_geometry &geom);

// If mouse on dungeon map, returns true and sets gc.
// Otherwise, it just returns false.
bool gui_get_mouse_grid_pos(coord_def &gc);

typedef map<int, TabbedRegion*>::iterator tab_iterator;

enum tiles_key_mod
{
    TILES_MOD_NONE  = 0x0,
    TILES_MOD_SHIFT = 0x1,
    TILES_MOD_CTRL  = 0x2,
    TILES_MOD_ALT   = 0x4,
};

struct MouseEvent
{
    enum mouse_event_type
    {
        PRESS,
        RELEASE,
        MOVE,
    };

    enum mouse_event_button
    {
        NONE        = 0x00,
        LEFT        = 0x01,
        MIDDLE      = 0x02,
        RIGHT       = 0x04,
        SCROLL_UP   = 0x08,
        SCROLL_DOWN = 0x10,
    };

    // Padding for ui_event
    unsigned char type;

    // kind of event
    mouse_event_type event;
    // if PRESS or RELEASE, the button pressed
    mouse_event_button button;
    // bitwise-or of buttons currently pressed
    unsigned short held;
    // bitwise-or of key mods currently pressed
    unsigned char mod;
    // location of events in pixels and in window coordinate space
    unsigned int px;
    unsigned int py;
};

class FontWrapper;
class crawl_view_buffer;

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
    void layout_statcol();
    void calculate_default_options();
    void clrscr();

    void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
    GotoRegion get_cursor_region() const;
    int get_number_of_lines();
    int get_number_of_cols();
    bool is_using_small_layout();
    void zoom_dungeon(bool in);
    bool zoom_to_minimap();
    bool zoom_from_minimap();

    void deactivate_tab();

    void update_minimap(const coord_def &gc);
    void clear_minimap();
    void update_minimap_bounds();
    void toggle_inventory_display();
    void update_tabs();

    void set_need_redraw(unsigned int min_tick_delay = 0);
    bool need_redraw() const;
    void redraw();

    void place_cursor(cursor_type type, const coord_def &gc);
    void clear_text_tags(text_tag_type type);
    void add_text_tag(text_tag_type type, const string &tag,
                      const coord_def &gc);
    void add_text_tag(text_tag_type type, const monster_info& mon);

    bool initialise_items();

    const coord_def &get_cursor() const;

    void add_overlay(const coord_def &gc, tileidx_t idx);
    void clear_overlays();

    void draw_title();
    void update_title_msg(string load_msg);
    void hide_title();

    void draw_doll_edit();

    int draw_popup(Popup *popup);
    void set_map_display(const bool display);
    bool get_map_display();
    void do_map_display();

    MenuRegion *get_menu() { return m_region_menu; }
    bool is_fullscreen() { return m_fullscreen; }

    FontWrapper* get_crt_font() { return m_fonts.at(m_crt_font).font; }
    CRTRegion* get_crt() { return m_region_crt; }
    const ImageManager* get_image_manager() { return m_image; }
    int to_lines(int num_tiles, int tile_height = TILE_Y);
protected:
    int load_font(const char *font_file, int font_size,
                  bool default_on_fail, bool outline);
    int handle_mouse(MouseEvent &event);

    void use_control_region(ControlRegion *region, bool use_control_layer = true);
    bool m_map_mode_enabled;

    // screen pixel dimensions
    coord_def m_windowsz;
    // screen pixel density ratio
    int densityNum, densityDen;
    // screen pixels per view cell
    coord_def m_viewsc;

    bool m_fullscreen;
    bool m_need_redraw;

    int TAB_ABILITY;
    int TAB_COMMAND;
    int TAB_COMMAND2;
    int TAB_ITEM;
    int TAB_MONSTER;
    int TAB_NAVIGATION;
    int TAB_SKILL;
    int TAB_SPELL;

    enum LayerID
    {
        LAYER_NORMAL,
        LAYER_CRT,
        LAYER_TILE_CONTROL,
        LAYER_MAX,
    };

    class Layer
    {
    public:
        // Layers don't own these regions
        vector<Region*> m_regions;
    };
    Layer m_layers[LAYER_MAX];
    LayerID m_active_layer;

    // Normal layer
    TileRegionInit  m_init;
    DungeonRegion   *m_region_tile;
    StatRegion      *m_region_stat;
    MessageRegion   *m_region_msg;
    MapRegion       *m_region_map;
    TabbedRegion    *m_region_tab;
    InventoryRegion *m_region_inv;
    AbilityRegion   *m_region_abl;
    SpellRegion     *m_region_spl;
    MemoriseRegion  *m_region_mem;
    MonsterRegion   *m_region_mon;
    SkillRegion     *m_region_skl;
    CommandRegion   *m_region_cmd;
    CommandRegion   *m_region_cmd_meta;
    CommandRegion   *m_region_cmd_map;

    map<int, TabbedRegion*> m_tabs;

    // Full-screen CRT layer
    CRTRegion       *m_region_crt;
    MenuRegion      *m_region_menu;

    struct font_info
    {
        string name;
        int size;
        bool outline;
        FontWrapper *font;
    };
    vector<font_info> m_fonts;
    int m_crt_font;
    int m_msg_font;
    int m_tip_font;
    int m_lbl_font;

    int m_tab_margin;
    int m_stat_col;
    int m_stat_x_divider;
    int m_statcol_top;
    int m_statcol_bottom;
    int m_map_pixels;

    void do_layout();
    int calc_tab_lines(const int num_elements);
    void place_tab(int idx);
    void autosize_minimap();
    void place_minimap();
    void resize_inventory();
    void place_gold_turns();

    ImageManager *m_image;

    // Mouse state.
    unsigned short m_buttons_held;
    unsigned char m_key_mod;
    coord_def m_mouse;
    unsigned int m_last_tick_moved;
    unsigned int m_last_tick_redraw;

    string m_tooltip;

    int m_screen_width;
    int m_screen_height;

    struct cursor_loc
    {
        cursor_loc() { reset(); }
        void reset() { reg = nullptr; cx = cy = -1; mode = MOUSE_MODE_MAX; }
        bool operator==(const cursor_loc &rhs) const
        {
            return rhs.reg == reg
                   && rhs.cx == cx
                   && rhs.cy == cy
                   && rhs.mode == mode;
        }
        bool operator!=(const cursor_loc &rhs) const
        {
            return !(*this == rhs);
        }

        Region *reg;
        int cx, cy;
        mouse_mode mode;
    };
    cursor_loc m_cur_loc;
};

// Main interface for tiles functions
extern TilesFramework tiles;

#endif
