#ifndef VIEWGEOM_H
#define VIEWGEOM_H

template<typename T>
class crawl_view_buffer
{
public:
    crawl_view_buffer() : buffer(NULL) {}
    ~crawl_view_buffer() { delete [] buffer; }

    void size(const coord_def &sz)
    {
        delete [] buffer;
        buffer = new T[sz.x * sz.y * 2];
    }

    operator T * () { return (buffer); }

private:
    T *buffer;
};

#ifdef USE_TILE
typedef unsigned int tile_buffer_t;
#endif

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

    crawl_view_buffer<screen_buffer_t> vbuf;
                                   // Buffer for drawing the main game map.
#ifdef USE_TILE
    crawl_view_buffer<tile_buffer_t> tbuf;
                                   // Tiles buffer.
#endif

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

    coord_def view_centre() const
    {
        return viewp + viewhalfsz;
    }

    coord_def glosc() const
    {
        return (glos1 + glos2) / 2;
    }

    bool in_grid_los(const coord_def &c) const
    {
        return (c.x >= glos1.x && c.x <= glos2.x
                && c.y >= glos1.y && c.y <= glos2.y);
    }

    bool in_view_los(const coord_def &c) const
    {
        return (c.x >= vlos1.x && c.x <= vlos2.x
                && c.y >= vlos1.y && c.y <= vlos2.y);
    }

    bool in_view_viewport(const coord_def &c) const
    {
        return (c.x >= viewp.x && c.y >= viewp.y
                && c.x < viewp.x + viewsz.x
                && c.y < viewp.y + viewsz.y);
    }

    bool in_grid_viewport(const coord_def &c) const
    {
        return in_view_viewport(c - vgrdc + view_centre());
    }
};

extern crawl_view_geometry crawl_view;

inline int view2gridX(int vx)
{
    return (crawl_view.vgrdc.x + vx - crawl_view.view_centre().x);
}

inline int view2gridY(int vy)
{
    return (crawl_view.vgrdc.y + vy - crawl_view.view_centre().y);
}

inline coord_def view2grid(const coord_def &pos)
{
    return pos - crawl_view.view_centre() + crawl_view.vgrdc;
}

inline int grid2viewX(int gx)
{
    return (gx - crawl_view.vgrdc.x + crawl_view.view_centre().x);
}

inline int grid2viewY(int gy)
{
    return (gy - crawl_view.vgrdc.y + crawl_view.view_centre().y);
}

inline coord_def grid2view(const coord_def &pos)
{
    return (pos - crawl_view.vgrdc + crawl_view.view_centre());
}

inline coord_def view2show(const coord_def &pos)
{
    return (pos - crawl_view.vlos1);
}

inline coord_def show2view(const coord_def &pos)
{
    return (pos + crawl_view.vlos1);
}

inline coord_def grid2show(const coord_def &pos)
{
    return (view2show(grid2view(pos)));
}

inline coord_def show2grid(const coord_def &pos)
{
    return (view2grid(show2view(pos)));
}

#endif



