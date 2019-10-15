#ifdef USE_TILE_LOCAL
#pragma once

#include "tilebuf.h"
#include "tiledoll.h"
#include "tilereg.h"

class DollEditRegion : public ControlRegion
{
public:
    DollEditRegion(ImageManager *im, FontWrapper *font);

    virtual void render() override;
    virtual void clear() override;
    virtual void run() override;

    virtual int handle_mouse(wm_mouse_event &event) override;
protected:
    virtual void on_resize() override {}

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
