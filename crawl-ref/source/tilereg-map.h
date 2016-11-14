#ifdef USE_TILE_LOCAL
#ifndef TILEREG_MAP_H
#define TILEREG_MAP_H

#include "tilebuf.h"
#include "tilereg.h"

class MapRegion : public Region
{
public:
    MapRegion(int pixsz);
    ~MapRegion();

    virtual void render() override;
    virtual void clear() override;
    virtual int handle_mouse(MouseEvent &event) override;
    virtual bool update_tip_text(string &tip) override;

    void init_colours();
    void set(const coord_def &gc, map_feature f);
    void set_window(const coord_def &start, const coord_def &end);
    void update_bounds();

protected:
    virtual void on_resize() override;
    void recenter();
    void pack_buffers();
    void hide_cell(int x, int y);

    VColour m_colours[MF_MAX];
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
