/*
 *  File:       tilereg-mon.h
 */

#ifdef USE_TILE
#ifndef TILEREG_MON_H
#define TILEREG_MON_H

#include "mon-info.h"
#include "tilereg-grid.h"

class MonsterRegion : public GridRegion 
{
public:
    MonsterRegion(const TileRegionInit &init);

    virtual void update();
    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);
    virtual bool update_alt_text(std::string &alt);

    virtual const std::string name() const { return "Monsters"; }

protected:
    const monsters *get_monster(unsigned int idx) const;

    virtual void pack_buffers();
    virtual void draw_tag();
    virtual void activate();

    std::vector<monster_info> m_mon_info;
};

#endif
#endif
