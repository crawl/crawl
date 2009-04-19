/*
 *  File:       tilereg.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifdef USE_TILE
#ifndef TILEREG_H
#define TILEREG_H

#include "AppHdr.h"
#include "format.h"
#include "tilebuf.h"
#include "tiletex.h"
#include "tiles.h"
#include <vector>

class mcache_entry;

class ImageManager
{
public:
    ImageManager();
    virtual ~ImageManager();

    bool load_textures();
    bool load_item_texture();
    void unload_textures();

    FixedVector<TilesTexture, TEX_MAX> m_textures;
};

// Windows and internal regions (text, dungeon, map, etc)
class MouseEvent;

class Region
{
public:
    Region();
    virtual ~Region();

    void resize(int mx, int my);
    void place(int sx, int sy, int margin);
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

class FTFont;

class TextRegion : public Region
{
public:
    TextRegion(FTFont *font);
    ~TextRegion();

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
    FTFont *m_font;
};

class StatRegion : public TextRegion
{
public:
    StatRegion(FTFont *font);

    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
};

class MessageRegion : public TextRegion
{
public:
    MessageRegion(FTFont *font);

    void set_overlay(bool is_overlay);

    virtual int handle_mouse(MouseEvent &event);
    virtual void render();
    virtual bool update_tip_text(std::string &tip);

    std::string &alt_text() { return m_alt_text; }
protected:
    std::string m_alt_text;
    bool m_overlay;
};

class CRTRegion : public TextRegion
{
public:
    CRTRegion(FTFont *font);

    virtual int handle_mouse(MouseEvent &event);
};

class MenuEntry;

class MenuRegion : public Region
{
public:
    MenuRegion(ImageManager *im, FTFont *entry);

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
    FTFont *m_font_entry;
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
    TileRegion(ImageManager *im, FTFont *tag_font,
               int tile_x, int tile_y);
    ~TileRegion();

protected:
    ImageManager *m_image;
    FTFont *m_tag_font;
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
    DungeonRegion(ImageManager *im, FTFont *tag_font,
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
    void pack_mcache(mcache_entry *entry, int x, int y);
    void pack_player(int x, int y);
    void pack_foreground(unsigned int bg, unsigned int fg, int x, int y);
    void pack_doll(const dolls_data &doll, int x, int y);
    void pack_cursor(cursor_type type, unsigned int tile);
    void pack_buffers();

    int get_buffer_index(const coord_def &gc);
    void to_screen_coords(const coord_def &gc, coord_def& pc) const;

    std::vector<unsigned int> m_tileb;
    int m_cx_to_gx;
    int m_cy_to_gy;
    coord_def m_cursor[CURSOR_MAX];
    std::vector<TextTag> m_tags[TAG_MAX];

    TileBuffer m_buf_dngn;
    TileBuffer m_buf_doll;
    TileBuffer m_buf_main;
    bool m_dirty;

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

class InventoryRegion : public TileRegion
{
public:
    InventoryRegion(ImageManager *im, FTFont *tag_font,
                    int tile_x, int tile_y);
    virtual ~InventoryRegion();

    virtual void clear();
    virtual void render();
    virtual void on_resize();
    virtual int handle_mouse(MouseEvent &event);

    void update(int num, InventoryTile *items);
    void update_slot(int slot, InventoryTile &item);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_alt_text(std::string &alt);

protected:
    void pack_tile(int x, int y, int idx);
    void pack_buffers();
    void add_quad_char(char c, int x, int y, int ox, int oy);
    void place_cursor(const coord_def &cursor);
    unsigned int cursor_index() const;

    std::vector<InventoryTile> m_items;
    unsigned char *m_flavour;

    TileBuffer m_buf_dngn;
    TileBuffer m_buf_main;

    coord_def m_cursor;
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

class TitleRegion : public Region
{
public:
    TitleRegion(int width, int height);

    virtual void render();
    virtual void clear() {};

    virtual int handle_mouse(MouseEvent &event) { return 0; }

protected:
    virtual void on_resize() {}

    GenericTexture m_img;
    VertBuffer<PTVert> m_buf;
};

#endif
#endif
