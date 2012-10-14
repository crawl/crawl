#ifdef USE_TILE_LOCAL
#ifndef TILEREG_MAP_H
#define TILEREG_MAP_H

#include "tilebuf.h"
#include "tilereg.h"

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
    virtual bool update_tip_text(string &tip);

    void init_colours();
    void set(const coord_def &gc, map_feature f);
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

#endif
#endif
