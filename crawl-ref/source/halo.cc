#include "AppHdr.h"

#include "halo.h"

#include "actor.h"
#include "coordit.h"
#include "player.h"
#include "monster.h"
#include "religion.h"
#include "terrain.h"

bool actor::haloed() const
{
    return (you.halo_contains(pos()));
}

bool actor::halo_contains(const coord_def &c) const
{
    int r = halo_radius();
    return (r > 0 && (c - pos()).abs() <= r * r && see_cell(c));
}

int player::halo_radius() const
{
    if (you.religion == GOD_SHINING_ONE && you.piety >= piety_breakpoint(0)
        && !you.penance[GOD_SHINING_ONE])
    {
        return (std::min(LOS_RADIUS, you.piety / 20));
    }

    return (0);
}

int monsters::halo_radius() const
{
    // Angels and Daevas are haloed.
    if (holiness() == MH_HOLY)
        return (2);
    else
        return (0);
}

// XXX: This might become too expensive; possibly, keep
//      a mapping of cell -> list of monsters in view of cell
//      and just iterate through that.
std::list<actor*> haloers(const coord_def &c)
{
    std::list<actor*> ret;
    for (radius_iterator ri(c, LOS_RADIUS, false); ri; ++ri)
    {
        actor* a = actor_at(*ri);
        if (a && a->halo_contains(c))
            ret.push_back(a);
    }
    return (ret);
}
