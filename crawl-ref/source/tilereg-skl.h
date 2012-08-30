#ifdef USE_TILE_LOCAL
#ifndef TILEREG_SKL_H
#define TILEREG_SKL_H

#include "tilereg-grid.h"

class SkillRegion : public GridRegion
{
public:
    SkillRegion(const TileRegionInit &init);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(string &tip);
    virtual bool update_tab_tip_text(string &tip, bool active);
    virtual bool update_alt_text(string &alt);

    virtual const string name() const { return "Skills"; }

protected:
    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();
};

#endif
#endif
