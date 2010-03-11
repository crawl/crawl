/*
 *  File:       tilesdl.h
 *  Summary:    SDL-related functionality for the tiles port
 *  Written by: Enne Walker
 */

#ifdef USE_TILE
#ifndef TILESDL_H
#define TILESDL_H

#include "externs.h"
#include "tilereg.h"

enum key_mod
{
    MOD_NONE  = 0x0,
    MOD_SHIFT = 0x1,
    MOD_CTRL  = 0x2,
    MOD_ALT   = 0x4
};

struct MouseEvent
{
    enum mouse_event_type
    {
        PRESS,
        RELEASE,
        MOVE
    };

    enum mouse_event_button
    {
        NONE        = 0x00,
        LEFT        = 0x01,
        MIDDLE      = 0x02,
        RIGHT       = 0x04,
        SCROLL_UP   = 0x08,
        SCROLL_DOWN = 0x10
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

class FTFont;

class TilesFramework
{
public:
    TilesFramework();
    virtual ~TilesFramework();

    bool initialise();
    void shutdown();
    void load_dungeon(unsigned int *tileb, const coord_def &gc);
    void load_dungeon(const coord_def &gc);
    int getch_ck();
    void resize();
    void calculate_default_options();
    void clrscr();

    void cgotoxy(int x, int y, GotoRegion region = GOTO_CRT);
    GotoRegion get_cursor_region() const;
    int get_number_of_lines();
    int get_number_of_cols();

    void update_minimap(int gx, int gy);
    void update_minimap(int gx, int gy, map_feature f);
    void clear_minimap();
    void update_minimap_bounds();
    void toggle_inventory_display();
    void update_inventory();

    void set_need_redraw(unsigned int min_tick_delay = 0);
    bool need_redraw() const;
    void redraw();

    void place_cursor(cursor_type type, const coord_def &gc);
    void clear_text_tags(text_tag_type type);
    void add_text_tag(text_tag_type type, const std::string &tag,
                      const coord_def &gc);
    void add_text_tag(text_tag_type type, const monsters* mon);

    bool initialise_items();

    const coord_def &get_cursor() const;

    void add_overlay(const coord_def &gc, int idx);
    void clear_overlays();

    void draw_title();
    void update_title_msg(std::string load_msg);
    void hide_title();

    void draw_doll_edit();

    MenuRegion *get_menu() { return m_region_menu; }
    bool is_fullscreen() { return m_fullscreen; }

    FTFont* get_crt_font() { return m_fonts.at(m_crt_font).font; }
    CRTRegion* get_crt() { return m_region_crt; }
    const ImageManager& get_image_manager() { return m_image; }
protected:
    int load_font(const char *font_file, int font_size,
                  bool default_on_fail, bool outline);
    int handle_mouse(MouseEvent &event);

    void use_control_region(ControlRegion *region);

    // screen pixel dimensions
    coord_def m_windowsz;
    // screen pixels per view cell
    coord_def m_viewsc;

    bool m_fullscreen;
    bool m_need_redraw;

    enum TabID
    {
        TAB_ITEM,
        TAB_SPELL,
        TAB_MEMORISE,
        TAB_MAX
    };

    enum LayerID
    {
        LAYER_NORMAL,
        LAYER_CRT,
        LAYER_TILE_CONTROL,
        LAYER_MAX
    };

    class Layer
    {
    public:
        // Layers don't own these regions
        std::vector<Region*> m_regions;
    };
    Layer m_layers[LAYER_MAX];
    LayerID m_active_layer;

    // Normal layer
    DungeonRegion   *m_region_tile;
    StatRegion      *m_region_stat;
    MessageRegion   *m_region_msg;
    MapRegion       *m_region_map;
    TabbedRegion    *m_region_tab;
    InventoryRegion *m_region_inv;
    SpellRegion     *m_region_spl;
    MemoriseRegion  *m_region_mem;

    // Full-screen CRT layer
    CRTRegion       *m_region_crt;
    MenuRegion      *m_region_menu;

    struct font_info
    {
        std::string name;
        int size;
        bool outline;
        FTFont *font;
    };
    std::vector<font_info> m_fonts;
    int m_crt_font;
    int m_msg_font;
    int m_tip_font;

    void do_layout();
    bool layout_statcol(bool message_overlay, bool show_gold_turns);

    ImageManager *m_image;

    // Mouse state.
    unsigned short m_buttons_held;
    unsigned char m_key_mod;
    coord_def m_mouse;
    unsigned int m_last_tick_moved;
    unsigned int m_last_tick_redraw;

    std::string m_tooltip;

    int m_screen_width;
    int m_screen_height;

    struct cursor_loc
    {
        cursor_loc() { reset(); }
        void reset() { reg = NULL; cx = cy = -1; mode = MOUSE_MODE_MAX; }
        bool operator==(const cursor_loc &rhs) const
        {
            return (rhs.reg == reg
                    && rhs.cx == cx
                    && rhs.cy == cy
                    && rhs.mode == mode);
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

#ifdef TARGET_COMPILER_MINGW
#ifndef alloca
// Srsly, MinGW, wtf?
void *alloca(size_t);
#endif
#endif

#endif
#endif
