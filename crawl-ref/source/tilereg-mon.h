/*
 *  File:       tilereg-mon.h
 */

#ifdef USE_TILE
#ifndef TILEREG_MON_H
#define TILEREG_MON_H

#include "tilereg-act.h"

class MonsterRegion : public ActorRegion 
{
public:
    MonsterRegion(const TileRegionInit &init);

    virtual void update();

    virtual const std::string name() const { return "Monsters"; }

protected:
    virtual void activate();
};

#endif
#endif
