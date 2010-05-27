/*
 *  File:       tilereg-mon.cc
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-mon.h"

MonsterRegion::MonsterRegion(const TileRegionInit &init) : ActorRegion(init)
{
}

void MonsterRegion::update()
{
    m_items.clear();
    m_mon_info.clear();
    m_dirty = true;

    const size_t max_mons = mx * my;

    if (max_mons == 0)
        return;

    get_monster_info(m_mon_info);
    std::sort(m_mon_info.begin(), m_mon_info.end(),
              monster_info::less_than_wrapper);

    unsigned int num_mons = std::min(max_mons, m_mon_info.size());
    for (size_t i = 0; i < num_mons; ++i)
    {
        InventoryTile desc;
        desc.idx = i;

        m_items.push_back(desc);
    }
}

void MonsterRegion::activate()
{
}

#endif
