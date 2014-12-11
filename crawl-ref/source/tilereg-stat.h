#ifdef USE_TILE_LOCAL
#ifndef TILEREG_STAT_H
#define TILEREG_STAT_H

#include "tilebuf.h"
#include "tilereg-text.h"

class StatRegion : public TextRegion
{
public:
    StatRegion(FontWrapper *font);

    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(string &tip);

    virtual void render();
    virtual void clear();

protected:
    ShapeBuffer m_shape_buf;
    void _clear_buffers();
};

#endif
#endif
