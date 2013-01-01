#ifdef USE_TILE_LOCAL
#ifndef TILEREG_INV_H
#define TILEREG_INV_H

#include "tilereg-grid.h"

class InventoryRegion : public GridRegion
{
public:
    InventoryRegion(const TileRegionInit &init);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(string &tip);
    virtual bool update_tab_tip_text(string &tip, bool active);
    virtual bool update_alt_text(string &alt);

    virtual const string name() const { return "Inventory"; }

protected:
    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();

private:
    bool _is_next_button(int idx);
    bool _is_prev_button(int idx);
};

#endif
#endif
