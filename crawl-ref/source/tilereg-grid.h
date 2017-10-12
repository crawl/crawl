#ifdef USE_TILE_LOCAL
#pragma once

#include "tiledgnbuf.h"
#include "tilereg.h"

class InventoryTile
{
public:
    InventoryTile();

    // tile index
    tileidx_t tile;
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
    GridRegion(const TileRegionInit &init);

    virtual void clear() override;
    virtual void render() override;
    virtual void on_resize() override;

    virtual void update() = 0;
    void place_cursor(const coord_def &cursor);

    virtual const string name() const = 0;
    virtual bool update_tab_tip_text(string &tip, bool active) = 0;
    virtual void activate() = 0;

protected:
    virtual void pack_buffers() = 0;
    virtual void draw_tag() = 0;

    // Place the cursor and set idx with the index into m_items of the click.
    // If click was invalid, return false.
    bool place_cursor(MouseEvent &event, unsigned int &idx);
    unsigned int cursor_index() const;
    int add_quad_char(char c, int x, int y, int ox, int oy);
    void draw_number(int x, int y, int number);
    void draw_desc(const char *desc);

    coord_def m_cursor;
    int m_last_clicked_item;
    int m_grid_page;

    vector<InventoryTile> m_items;

    DungeonCellBuffer m_buf;
};

#endif
