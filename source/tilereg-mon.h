#ifdef USE_TILE_LOCAL
#ifndef TILEREG_MON_H
#define TILEREG_MON_H

#include "tilereg-grid.h"

struct monster_info;
class MonsterRegion : public GridRegion
{
public:
    MonsterRegion(const TileRegionInit &init);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(string &tip);
    virtual bool update_tab_tip_text(string &tip, bool active);
    virtual bool update_alt_text(string &alt);

    virtual const string name() const { return "Monsters"; }

protected:
    const monster_info* get_monster(unsigned int idx) const;

    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();

    vector<monster_info> m_mon_info;
};

#endif
#endif
