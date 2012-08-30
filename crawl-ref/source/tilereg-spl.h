#ifdef USE_TILE_LOCAL
#ifndef TILEREG_SPL_H
#define TILEREG_SPL_H

#include "tilereg-grid.h"

class SpellRegion : public GridRegion
{
public:
    SpellRegion(const TileRegionInit &init);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(string &tip);
    virtual bool update_tab_tip_text(string &tip, bool active);
    virtual bool update_alt_text(string &alt);

    virtual const string name() const { return "Spells"; }

protected:
    virtual int get_max_slots();

    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();
};

#endif
#endif
