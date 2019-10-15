#ifdef USE_TILE_LOCAL
#pragma once

#include "tilereg-spl.h"

class MemoriseRegion : public SpellRegion
{
public:
    MemoriseRegion(const TileRegionInit &init);

    virtual void update() override;
    virtual int handle_mouse(wm_mouse_event &event) override;
    virtual bool update_tip_text(string &tip) override;
    virtual bool update_tab_tip_text(string &tip, bool active) override;

    virtual const string name() const override { return "Memorisation"; }

protected:
    virtual int get_max_slots() override;

    virtual void draw_tag() override;
    virtual void activate() override;
};

#endif
