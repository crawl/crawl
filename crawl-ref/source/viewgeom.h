#pragma once

#include "tilecell.h"

struct screen_cell_t
{
    char32_t glyph;
    unsigned short colour; // TODO: check if this is real colour (8 bit)
    unsigned short flash_colour;
#ifdef USE_TILE
    packed_cell tile;
#endif
};

class crawl_view_buffer
{
public:
    crawl_view_buffer();
    crawl_view_buffer(const coord_def &sz);
    ~crawl_view_buffer();

    coord_def size() const { return m_size; }
    void resize(const coord_def &sz);
    bool empty() const;

    operator screen_cell_t * () { return m_buffer; }
    operator const screen_cell_t * () const { return m_buffer; }
    const crawl_view_buffer & operator = (const crawl_view_buffer &rhs);

    void clear();
    void draw();
private:
    coord_def m_size;
    screen_cell_t *m_buffer;
};

struct crawl_view_geometry
{
public:
    coord_def termp;               // Left-top pos of terminal.
    coord_def termsz;              // Size of the terminal.
    coord_def viewp;               // Left-top pos of viewport.
    coord_def viewsz;              // Size of the viewport (play area).
    coord_def hudp;                // Left-top pos of status area.
    coord_def hudsz;               // Size of the status area.
    coord_def msgp;                // Left-top pos of the message pane.
    coord_def msgsz;               // Size of the message pane.
    coord_def mlistp;              // Left-top pos of the monster list.
    coord_def mlistsz;             // Size of the monster list.

    crawl_view_buffer vbuf;        // Buffer for drawing the main game map.

    coord_def vgrdc;               // What grid pos is at the centre of the view
                                   // usually you.pos().

    coord_def viewhalfsz;

    coord_def glos1, glos2;        // LOS limit grid coords (inclusive)
    coord_def vlos1, vlos2;        // LOS limit viewport coords (inclusive)

    coord_def mousep;              // Where the mouse is.

private:
    coord_def last_player_pos;

public:
    crawl_view_geometry();
    void init_geometry();

    void init_view();
    void set_player_at(const coord_def &c, bool force_centre = false);
    // Set new location, but preserve scrolling as if the player didn't move.
    void shift_player_to(const coord_def &c);
    // Recalculate vlos1 and vlos2.
    void calc_vlos();

    inline coord_def view2grid(const coord_def &pos) const
    {
        return pos - viewhalfsz + vgrdc - coord_def(1, 1);
    }

    inline coord_def grid2view(const coord_def &pos) const
    {
        return pos - vgrdc + viewhalfsz + coord_def(1, 1);
    }

    inline coord_def view2show(const coord_def &pos) const
    {
        return pos - vlos1;
    }

    inline coord_def show2view(const coord_def &pos) const
    {
        return pos + vlos1;
    }

    inline coord_def grid2show(const coord_def &pos) const
    {
        return view2show(grid2view(pos));
    }

    inline coord_def show2grid(const coord_def &pos) const
    {
        return view2grid(show2view(pos));
    }

    inline coord_def screen2view(const coord_def& pos) const
    {
        return pos - viewp + termp;
    }

    inline coord_def view2screen(const coord_def& pos) const
    {
        return pos + viewp - termp;
    }

    inline coord_def screen2grid(const coord_def& pos) const
    {
        return view2grid(screen2view(pos));
    }

    inline coord_def grid2screen(const coord_def& pos) const
    {
        return view2screen(grid2view(pos));
    }

    coord_def glosc() const
    {
        return (glos1 + glos2) / 2;
    }

    bool in_los_bounds_g(const coord_def &c) const
    {
        return c.x >= glos1.x && c.x <= glos2.x
               && c.y >= glos1.y && c.y <= glos2.y;
    }

    bool in_los_bounds_v(const coord_def &c) const
    {
        return in_los_bounds_g(view2grid(c));
    }

    bool in_viewport_v(const coord_def &c) const
    {
        return c.x > 0 && c.y > 0
               && c.x <= viewsz.x && c.y <= viewsz.y;
    }

    bool in_viewport_s(const coord_def &c) const
    {
        return in_viewport_v(screen2view(c));
    }

    bool in_viewport_g(const coord_def &c) const
    {
        return in_viewport_v(grid2view(c));
    }
};

extern crawl_view_geometry crawl_view;

static inline coord_def view2grid(const coord_def &pos)
{
    return crawl_view.view2grid(pos);
}

static inline coord_def grid2view(const coord_def &pos)
{
    return crawl_view.grid2view(pos);
}

static inline coord_def view2show(const coord_def &pos)
{
    return crawl_view.view2show(pos);
}

static inline coord_def show2view(const coord_def &pos)
{
    return crawl_view.show2view(pos);
}

static inline coord_def grid2show(const coord_def &pos)
{
    return crawl_view.grid2show(pos);
}

static inline coord_def show2grid(const coord_def &pos)
{
    return crawl_view.show2grid(pos);
}

static inline bool in_los_bounds_v(const coord_def& pos)
{
    return crawl_view.in_los_bounds_v(pos);
}

static inline bool in_los_bounds_g(const coord_def& pos)
{
    return crawl_view.in_los_bounds_g(pos);
}

