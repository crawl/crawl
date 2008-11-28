/*
 *  File:       tilereg.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 *
 *  Modified for Crawl Reference by $Author: j-p-e-g $ on $Date: 2008-03-07 $
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

    void resize(unsigned int mx, unsigned int my);
    void place(unsigned int sx, unsigned int sy, unsigned int margin);
    void resize_to_fit(int wx, int wy);

    // Returns true if the mouse position is over the region
    // If true, then cx and cy are set in the range [0..mx-1], [0..my-1]
    virtual bool mouse_pos(int mouse_x, int mouse_y, int &cx, int &cy);

    bool inside(unsigned int px, unsigned int py);
    virtual bool update_tip_text(std::string &tip) { return false; }
    virtual int handle_mouse(MouseEvent &event) = 0;

    virtual void render() = 0;
    virtual void clear() = 0;

    // Geometry
    // <-----------------wx----------------------->
    // sx     ox                                ex
    // |margin| text/tile area            |margin|

    // Offset in pixels
    unsigned int ox;
    unsigned int oy;

    // Unit size
    unsigned int dx;
    unsigned int dy;

    // Region size in dx/dy
    unsigned int mx;
    unsigned int my;

    // Width of the region in pixels
    unsigned int wx;
    unsigned int wy;

    // Start position in pixels (top left)
    unsigned int sx;
    unsigned int sy;

    // End position in pixels (bottom right)
    unsigned int ex;
    unsigned int ey;

    static coord_def NO_CURSOR;

protected:
    void recalculate();
    virtual void on_resize() = 0;
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
    static unsigned int print_x;
    static unsigned int print_y;
    static int text_col;
    // which region now printing?
    static class TextRegion *text_mode;
    // display cursor? where is the cursor now?
    static int cursor_flag;
    static class TextRegion *cursor_region;
    static unsigned int cursor_x;
    static unsigned int cursor_y;

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
    void addstr_aux(char *buffer, unsigned int len);
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
protected:
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
        int tile;
        TextureID texture;
        int sx, ex, sy, ey;
        bool selected;
        bool heading;
        char key;
        bool valid;
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
    TileBuffer m_tile_buf;
};

class TileRegion : public Region
{
public:
    TileRegion(ImageManager *im, FTFont *tag_font,
               unsigned int tile_x, unsigned int tile_y);
    ~TileRegion();

protected:
    void add_quad(TextureID tex, unsigned int idx, unsigned int x, unsigned int y, int ofs_x = 0, int ofs_y = 0, bool centre = true, int ymax = -1);

    ImageManager *m_image;

    struct tile_vert
    {
        float pos_x;
        float pos_y;
        float tex_x;
        float tex_y;
    };

    std::vector<tile_vert> m_verts;
    FTFont *m_tag_font;
};

struct TextTag
{
    std::string tag;
    coord_def gc;
};

enum cursor_type
{
    CURSOR_MOUSE,
    CURSOR_TUTORIAL,
    CURSOR_MAX
};

enum text_tag_type
{
    TAG_NAMED_MONSTER,
    TAG_TUTORIAL,
    TAG_CELL_DESC,
    TAG_MAX
};

class DungeonRegion : public TileRegion
{
public:
    DungeonRegion(ImageManager *im, FTFont *tag_font,
                  unsigned int tile_x, unsigned int tile_y);
    virtual ~DungeonRegion();

    virtual void render();
    virtual void clear();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
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
    void draw_background(unsigned int bg, unsigned int x, unsigned int y);
    void draw_mcache(mcache_entry *entry, unsigned int x, unsigned int y);
    void draw_player(unsigned int x, unsigned int y);
    void draw_foreground(unsigned int bg, unsigned int fg, unsigned int x, unsigned int y);
    void draw_doll(const dolls_data &doll, unsigned int x, unsigned int y);
    void draw_cursor(cursor_type type, unsigned int tile);

    int get_buffer_index(const coord_def &gc);
    void to_screen_coords(const coord_def &gc, coord_def& pc) const;

    std::vector<unsigned int> m_tileb;
    int m_cx_to_gx;
    int m_cy_to_gy;
    coord_def m_cursor[CURSOR_MAX];
    std::vector<TextTag> m_tags[TAG_MAX];

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
                    unsigned int tile_x, unsigned int tile_y);
    virtual ~InventoryRegion();

    virtual void clear();
    virtual void render();
    virtual void on_resize();
    virtual int handle_mouse(MouseEvent &event);

    void update(unsigned int num, InventoryTile *items);
    void update_slot(unsigned int slot, InventoryTile &item);
    virtual bool update_tip_text(std::string &tip);

protected:
    void pack_tile(unsigned int x, unsigned int y, unsigned int idx);
    void pack_verts();
    void add_quad_char(char c, unsigned int x, unsigned int y, int ox, int oy);
    void place_cursor(const coord_def &cursor);
    unsigned int cursor_index() const;

    unsigned int m_base_verts;
    std::vector<InventoryTile> m_items;
    unsigned char *m_flavour;

    coord_def m_cursor;

    bool m_need_to_pack;
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
    MapRegion(unsigned int pixsz);
    ~MapRegion();

    virtual void render();
    virtual void clear();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);

    void init_colours();
    void set(unsigned int gx, unsigned int gy, map_feature f);
    void set_window(const coord_def &start, const coord_def &end);

protected:
    virtual void on_resize();
    void update_offsets();

    map_colour m_colours[MF_MAX];
    unsigned int m_min_gx, m_max_gx, m_min_gy, m_max_gy;
    coord_def m_win_start;
    coord_def m_win_end;
    unsigned char *m_buf;
    bool m_far_view;
};


#endif
#endif
