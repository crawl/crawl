/*
 *  File:       tilereg.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifdef USE_TILE
#ifndef TILEREG_H
#define TILEREG_H

#include "format.h"
#include "player.h"
#include "tilebuf.h"
#include "tiletex.h"
#include "tiles.h"
#include <vector>

// Forward declare
class PrecisionMenu;
class mcache_entry;

class ImageManager
{
public:
    ImageManager();
    virtual ~ImageManager();

    bool load_textures(bool need_mips);
    bool load_item_texture();
    void unload_textures();

    static const char *filenames[TEX_MAX];
    FixedVector<TilesTexture, TEX_MAX> m_textures;
};

// Windows and internal regions (text, dungeon, map, etc.)
struct MouseEvent;

class Region
{
public:
    Region();
    virtual ~Region();

    void resize(int mx, int my);
    void place(int sx, int sy, int margin);
    void place(int sx, int sy);
    void resize_to_fit(int wx, int wy);

    // Returns true if the mouse position is over the region
    // If true, then cx and cy are set in the range [0..mx-1], [0..my-1]
    virtual bool mouse_pos(int mouse_x, int mouse_y, int &cx, int &cy);

    bool inside(int px, int py);
    virtual bool update_tip_text(std::string &tip) { return false; }
    virtual bool update_alt_text(std::string &alt) { return false; }
    virtual int handle_mouse(MouseEvent &event) = 0;

    virtual void render() = 0;
    virtual void clear() = 0;

    // Geometry
    // <-----------------wx----------------------->
    // sx     ox                                ex
    // |margin| text/tile area            |margin|

    // Offset in pixels
    int ox;
    int oy;

    // Unit size
    int dx;
    int dy;

    // Region size in dx/dy
    int mx;
    int my;

    // Width of the region in pixels
    int wx;
    int wy;

    // Start position in pixels (top left)
    int sx;
    int sy;

    // End position in pixels (bottom right)
    int ex;
    int ey;

    static coord_def NO_CURSOR;

protected:
    void recalculate();
    virtual void on_resize() = 0;
    void set_transform();
};

class FontWrapper;

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

class StatRegion : public TextRegion
{
public:
    StatRegion(FontWrapper *font);

    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
};

class MessageRegion : public TextRegion
{
public:
    MessageRegion(FontWrapper *font);

    void set_overlay(bool is_overlay);

    virtual int handle_mouse(MouseEvent &event);
    virtual void render();
    virtual bool update_tip_text(std::string &tip);

    std::string &alt_text() { return m_alt_text; }
protected:
    std::string m_alt_text;
    bool m_overlay;
};

/**
 * Expanded CRTRegion to support highlightable and clickable entries - felirx
 * The user of this region will have total control over the positioning of
 * objects at all times
 * The base class behaves like the current CRTRegion used commonly
 * It's identity is mapped to the CRT_NOMOUSESELECT value in TilesFramework
 *
 * Menu Entries are handled via pointers and are shared with whatever MenuClass
 * is using them. This is done to keep the keyboard selections in sync with mouse
 */
class CRTRegion : public TextRegion
{
public:

    CRTRegion(FontWrapper *font);
    virtual ~CRTRegion();

    virtual void render();
    virtual void clear();

    virtual int handle_mouse(MouseEvent& event);

    virtual void on_resize();

    void attach_menu(PrecisionMenu* menu);
    void detach_menu();
protected:
    PrecisionMenu* m_attached_menu;
};

class MenuEntry;

class MenuRegion : public Region
{
public:
    MenuRegion(ImageManager *im, FontWrapper *entry);

    virtual int handle_mouse(MouseEvent &event);
    virtual void render();
    virtual void clear();

    int maxpagesize() const;
    void set_entry(int index, const std::string &s, int colour, const MenuEntry *me);
    void set_offset(int lines);
    void set_more(const formatted_string &more);
    void set_num_columns(int columns);
protected:
    virtual void on_resize();
    virtual void place_entries();
    int mouse_entry(int x, int y);

    struct MenuRegionEntry
    {
        formatted_string text;
        int sx, ex, sy, ey;
        bool selected;
        bool heading;
        char key;
        bool valid;
        std::vector<tile_def> tiles;
    };

    ImageManager *m_image;
    FontWrapper *m_font_entry;
    formatted_string m_more;
    int m_mouse_idx;
    int m_max_columns;
    bool m_dirty;

    std::vector<MenuRegionEntry> m_entries;

    ShapeBuffer m_shape_buf;
    LineBuffer m_line_buf;
    FontBuffer m_font_buf;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;
};

class TileRegion : public Region
{
public:
    TileRegion(ImageManager *im, FontWrapper *tag_font,
               int tile_x, int tile_y);
    ~TileRegion();

protected:
    ImageManager *m_image;
    FontWrapper *m_tag_font;
    bool m_dirty;
};

struct TextTag
{
    std::string tag;
    coord_def gc;
};

class DungeonRegion : public TileRegion
{
public:
    DungeonRegion(ImageManager *im, FontWrapper *tag_font,
                  int tile_x, int tile_y);
    virtual ~DungeonRegion();

    virtual void render();
    virtual void clear();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_alt_text(std::string &alt);
    virtual void on_resize();

    void load_dungeon(unsigned int* tileb, int cx_to_gx, int cy_to_gy);
    void place_cursor(cursor_type type, const coord_def &gc);
    bool on_screen(const coord_def &gc) const;

    void clear_text_tags(text_tag_type type);
    void add_text_tag(text_tag_type type, const std::string &tag,
                      const coord_def &gc);

    const coord_def &get_cursor() const { return m_cursor[CURSOR_MOUSE]; }

    void add_overlay(const coord_def &gc, int idx);
    void clear_overlays();

protected:
    void pack_background(unsigned int bg, int x, int y);
    void pack_mcache(mcache_entry *entry, int x, int y, bool submerged);
    void pack_player(int x, int y, bool submerged);
    void pack_foreground(unsigned int bg, unsigned int fg, int x, int y);
    void pack_doll(const dolls_data &doll, int x, int y, bool submerged, bool ghost);
    void pack_cursor(cursor_type type, unsigned int tile);
    void pack_buffers();

    void draw_minibars();

    int get_buffer_index(const coord_def &gc);
    void to_screen_coords(const coord_def &gc, coord_def& pc) const;

    std::vector<unsigned int> m_tileb;
    int m_cx_to_gx;
    int m_cy_to_gy;
    coord_def m_cursor[CURSOR_MAX];
    std::vector<TextTag> m_tags[TAG_MAX];

    TileBuffer m_buf_dngn;
    SubmergedTileBuffer m_buf_doll;
    SubmergedTileBuffer m_buf_main_trans;
    TileBuffer m_buf_main;

    struct tile_overlay
    {
        coord_def gc;
        int idx;
    };
    std::vector<tile_overlay> m_overlays;
};

class InventoryTile
{
public:
    InventoryTile();

    // tile index
    int tile;
    // mitm/you.inv idx (depends on flag & TILEI_FLAG_FLOOR)
    int idx;
    // quantity of this item (0-999 valid, >999 shows as 999, <0 shows nothing)
    short quantity;
    // bitwise-or of TILEI_FLAG enumeration
    unsigned short flag;
    // for inventory items, the slot
    char key;
    // a special property, such as for brands
    int special;

    bool empty() const;
};

class GridRegion : public TileRegion
{
public:
    GridRegion(ImageManager *im, FontWrapper *tag_font, int tile_x, int tile_y);
    virtual ~GridRegion();

    virtual void clear();
    virtual void render();
    virtual void on_resize();

    virtual void update() = 0;
    void place_cursor(const coord_def &cursor);

    virtual const std::string name() const = 0;
    virtual bool update_tab_tip_text(std::string &tip, bool active) = 0;
    virtual void activate() = 0;

protected:
    virtual void pack_buffers() = 0;
    virtual void draw_tag() = 0;

    // Place the cursor and set idx with the index into m_items of the click.
    // If click was invalid, return false.
    bool place_cursor(MouseEvent &event, unsigned int &idx);
    unsigned int cursor_index() const;
    void add_quad_char(char c, int x, int y, int ox, int oy);
    void draw_number(int x, int y, int number);
    void draw_desc(const char *desc);

    unsigned char *m_flavour;
    coord_def m_cursor;

    std::vector<InventoryTile> m_items;

    TileBuffer m_buf_dngn;
    TileBuffer m_buf_spells;
    TileBuffer m_buf_main;
};

class InventoryRegion : public GridRegion
{
public:
    InventoryRegion(ImageManager *im, FontWrapper *tag_font,
                    int tile_x, int tile_y);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);
    virtual bool update_alt_text(std::string &alt);

    virtual const std::string name() const { return "Inventory"; }

protected:
    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();
};

class SpellRegion : public GridRegion
{
public:
    SpellRegion(ImageManager *im, FontWrapper *tag_font,
                int tile_x, int tile_y);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);
    virtual bool update_alt_text(std::string &alt);
    virtual bool check_memorise();

    virtual const std::string name() const { return "Spells"; }

protected:
    bool memorise;

    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();
};

class MemoriseRegion : public SpellRegion
{
public:
    MemoriseRegion(ImageManager *im, FontWrapper *tag_font,
                   int tile_x, int tile_y);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);

    virtual const std::string name() const { return "Memorisation"; }

protected:
    virtual void draw_tag();
    virtual void activate();
};

// A region that contains multiple region, selectable by tabs.
class TabbedRegion : public GridRegion
{
public:
    TabbedRegion(ImageManager *im, FontWrapper *tag_font,
                 int tile_x, int tile_y);

    virtual ~TabbedRegion();

    enum
    {
        TAB_OFS_UNSELECTED,
        TAB_OFS_MOUSEOVER,
        TAB_OFS_SELECTED,
        TAB_OFS_MAX
    };

    void set_tab_region(int idx, GridRegion *reg, int tile_tab);
    GridRegion *get_tab_region(int idx);
    void activate_tab(int idx);
    int active_tab() const;
    int num_tabs() const;

    virtual void update();
    virtual void clear();
    virtual void render();
    virtual void on_resize();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);
    virtual bool update_alt_text(std::string &alt);

    virtual const std::string name() const { return ""; }

protected:
    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate() {}

    bool active_is_valid() const;
    // Returns the tab the mouse is over, -1 if none.
    int get_mouseover_tab(MouseEvent &event) const;

    int m_active;
    int m_mouse_tab;
    TileBuffer m_buf_gui;

    struct TabInfo
    {
        GridRegion *reg;
        int tile_tab;
        int ofs_y;
        int min_y;
        int max_y;
    };
    std::vector<TabInfo> m_tabs;
};

enum map_colour
{
    MAP_BLACK,
    MAP_DKGREY,
    MAP_MDGREY,
    MAP_LTGREY,
    MAP_WHITE,
    MAP_BLUE,
    MAP_LTBLUE,
    MAP_DKBLUE,
    MAP_GREEN,
    MAP_LTGREEN,
    MAP_DKGREEN,
    MAP_CYAN,
    MAP_LTCYAN,
    MAP_DKCYAN,
    MAP_RED,
    MAP_LTRED,
    MAP_DKRED,
    MAP_MAGENTA,
    MAP_LTMAGENTA,
    MAP_DKMAGENTA,
    MAP_YELLOW,
    MAP_LTYELLOW,
    MAP_DKYELLOW,
    MAP_BROWN,
    MAX_MAP_COL
};

class MapRegion : public Region
{
public:
    MapRegion(int pixsz);
    ~MapRegion();

    virtual void render();
    virtual void clear();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);

    void init_colours();
    void set(int gx, int gy, map_feature f);
    void set_window(const coord_def &start, const coord_def &end);
    void update_bounds();

protected:
    virtual void on_resize();
    void recenter();
    void pack_buffers();

    map_colour m_colours[MF_MAX];
    int m_min_gx, m_max_gx, m_min_gy, m_max_gy;
    coord_def m_win_start;
    coord_def m_win_end;
    unsigned char *m_buf;

    ShapeBuffer m_buf_map;
    LineBuffer m_buf_lines;
    bool m_dirty;
    bool m_far_view;
};

// An abstract tiles-only region that takes control over all input.
class ControlRegion : public Region
{
public:
    virtual void run() = 0;
};

class TitleRegion : public ControlRegion
{
public:
    TitleRegion(int width, int height, FontWrapper* font);

    virtual void render();
    virtual void clear() {};
    virtual void run();

    virtual int handle_mouse(MouseEvent &event) { return 0; }

    void update_message(std::string message);

protected:
    virtual void on_resize() {}

    GenericTexture m_img;
    VertBuffer<PTVert> m_buf;
    FontBuffer m_font_buf;
};

enum tile_doll_mode
{
    TILEP_MODE_EQUIP   = 0,  // draw doll based on equipment
    TILEP_MODE_LOADING = 1,  // draw doll based on dolls.txt definitions
    TILEP_MODE_DEFAULT = 2,  // draw doll based on job specific definitions
    TILEP_MODE_MAX
};

class DollEditRegion : public ControlRegion
{
public:
    DollEditRegion(ImageManager *im, FontWrapper *font);

    virtual void render();
    virtual void clear();
    virtual void run();

    virtual int handle_mouse(MouseEvent &event);
protected:
    virtual void on_resize() {}

    // Currently edited doll index.
    int m_doll_idx;
    // Currently edited category of parts.
    int m_cat_idx;
    // Current part in current category.
    int m_part_idx;

    // Set of loaded dolls.
    dolls_data m_dolls[NUM_MAX_DOLLS];

    dolls_data m_player;
    dolls_data m_job_default;
    dolls_data m_doll_copy;
    bool m_copy_valid;

    tile_doll_mode m_mode;

    FontWrapper *m_font;

    ShapeBuffer m_shape_buf;
    FontBuffer m_font_buf;
    SubmergedTileBuffer m_tile_buf;
    SubmergedTileBuffer m_cur_buf;
};

#endif
#endif
