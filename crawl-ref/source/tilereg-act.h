/*
 *  File:       tilereg-act.h
 */

#ifdef USE_TILE
#ifndef TILEREG_ACT_H
#define TILEREG_ACT_H

#include "mon-info.h"
#include "tilereg-grid.h"

class ActorRegion : public GridRegion
{
public:
    ActorRegion(const TileRegionInit &init);

    virtual int handle_mouse(MouseEvent &event);
    virtual bool update_tip_text(std::string &tip);
    virtual bool update_tab_tip_text(std::string &tip, bool active);
    virtual bool update_alt_text(std::string &alt);

protected:
    virtual void pack_buffers();
    virtual void draw_tag();

    const monsters *get_monster(unsigned int idx) const;

    std::vector<monster_info> m_mon_info;
};

#endif
#endif
