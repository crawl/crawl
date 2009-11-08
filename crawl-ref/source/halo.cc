#include "AppHdr.h"

#include "halo.h"

#include "actor.h"
#include "player.h"
#include "monster.h"
#include "religion.h"
#include "terrain.h"

// TODO: generalize.
bool actor::haloed() const
{
    return (you.inside_halo(pos()));
}

bool actor::inside_halo(const coord_def &c) const
{
    int r = halo_radius();
    return ((c - pos()).abs() <= r * r && see_cell(c));
}

int player::halo_radius() const
{
    if (you.religion == GOD_SHINING_ONE && you.piety >= piety_breakpoint(0)
        && !you.penance[GOD_SHINING_ONE])
    {
        return std::min(LOS_RADIUS, you.piety / 20);
    }

    return 0;
}

int monsters::halo_radius() const
{
    return 0;
}

std::list<actor*> inside_halo(const coord_def &c)
{
    std::list<actor*> haloers;
    for (radius_iterator ri(c, LOS_RADIUS, false); ri; ++ri)
    {
        actor* a = actor_at(*ri);
        if (a && a->inside_halo(c))
            haloers.push_back(a);
    }
    return (haloers);
}

