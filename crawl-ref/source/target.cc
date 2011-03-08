#include "AppHdr.h"

#include "target.h"

#include "beam.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "player.h"
#include "terrain.h"

bool targetter::set_aim(coord_def a)
{
    if (!valid_aim(a))
        return false;

    aim = a;
    return true;
}


targetter_view::targetter_view()
{
    origin = aim = you.pos();
}

bool targetter_view::valid_aim(coord_def a)
{
    return true; // don't reveal map bounds
}

aff_type targetter_view::is_affected(coord_def loc)
{
    if (loc == aim)
        return AFF_YES;

    return AFF_NO;
}


targetter_smite::targetter_smite(const actor* act, int ran,
                                 int exp_min, int exp_max, bool wall_ok):
    exp_range_min(exp_min), exp_range_max(exp_max), affects_walls(wall_ok)
{
    ASSERT(act);
    ASSERT(exp_min >= 0);
    ASSERT(exp_max >= 0);
    ASSERT(exp_min <= exp_max);
    agent = act;
    origin = aim = act->pos();
    range2 = dist_range(ran);
}

bool targetter_smite::valid_aim(coord_def a)
{
    if (!affects_walls && feat_is_solid(grd(a)))
        return false;
    if (a == origin)
        return true;
    if ((origin - a).abs() > range2)
        return false;
    return cell_see_cell(origin, a);
}

bool targetter_smite::set_aim(coord_def a)
{
    if (!targetter::set_aim(a))
        return false;

    if (exp_range_max > 0)
    {
        coord_def centre(9,9);
        bolt beam;
        beam.target = a;
        beam.use_target_as_pos = true;
        exp_map_min.init(INT_MAX);
        beam.determine_affected_cells(exp_map_min, coord_def(), 0,
                                      exp_range_min, true, true);
        exp_map_max.init(INT_MAX);
        beam.determine_affected_cells(exp_map_max, coord_def(), 0,
                                      exp_range_max, true, true);
    }
    return true;
}

aff_type targetter_smite::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    if (exp_range_max <= 0)
        return AFF_NO;

    if ((loc - aim).rdist() > 9)
        return AFF_NO;
    coord_def centre(9,9);
    if (exp_map_min(loc - aim + centre) < INT_MAX)
        return AFF_YES;
    if (exp_map_max(loc - aim + centre) < INT_MAX)
        return AFF_MAYBE;

    return AFF_NO;
}


targetter_reach::targetter_reach(const actor* act, reach_type ran) :
    range(ran)
{
    ASSERT(act);
    agent = act;
    origin = aim = act->pos();
}

bool targetter_reach::valid_aim(coord_def a)
{
    if (!cell_see_cell(origin, a))
        return false;
    if (!agent->see_cell_no_trans(a))
        return false;

    int dist = (origin - a).abs();

    switch(range)
    {
    default:
        return dist <= 2;
    case REACH_KNIGHT:
        return dist <= 5;
    case REACH_TWO:
        return dist <= 8;
    }
}

aff_type targetter_reach::is_affected(coord_def loc)
{
    if (!valid_aim(loc))
        return AFF_NO;

    if (loc == aim)
        return AFF_YES;

    // Knight move reach "slips through" and can't be blocked by either
    // square in the middle.
    if ((loc - origin) * 2 == (aim - origin))
        return AFF_TRACER;

    return AFF_NO;
}


targetter_cloud::targetter_cloud(const actor* act, int range,
                                 int count_min, int count_max) :
    cnt_min(count_min), cnt_max(count_max)
{
    ASSERT(cnt_min > 0);
    ASSERT(cnt_max > 0);
    ASSERT(cnt_min <= cnt_max);
    if (agent = act)
        origin = aim = act->pos();
    range2 = dist_range(range);
}

static bool _cloudable(coord_def loc)
{
    return in_bounds(loc)
           && !feat_is_solid(grd(loc))
           && env.cgrid(loc) == EMPTY_CLOUD;
}

bool targetter_cloud::valid_aim(coord_def a)
{
    if (!agent)
        return _cloudable(a);

    if (a == origin)
        return true;
    if ((origin - a).abs() > range2)
        return false;
    if (!_cloudable(a))
        return false;
    return cell_see_cell(origin, a);
}

bool targetter_cloud::set_aim(coord_def a)
{
    if (!targetter::set_aim(a))
        return false;

    seen.clear();
    queue.clear();
    queue.push_back(std::vector<coord_def>());

    int placed = 0;
    queue[0].push_back(a);
    ASSERT(_cloudable(a));

    for (unsigned int d1 = 0; d1 < queue.size() && placed < cnt_max; d1++)
    {
        unsigned int to_place = queue[d1].size();
        debuglog("At dist %u placed %d + %u\n", d1, placed, to_place);
        placed += to_place;

        for (unsigned int i = 0; i < to_place; i++)
        {
            coord_def c = queue[d1][i];
            debuglog("» (%d,%d)\n", c.x, c.y);
            for(adjacent_iterator ai(c); ai; ++ai)
                if (_cloudable(*ai) && seen.find(*ai) == seen.end())
                {
                    unsigned int d2 = d1 + ((*ai - c).abs() == 1 ? 5 : 7);
                    if (d2 >= queue.size())
                        queue.resize(d2 + 1);
                    debuglog("  + (%d,%d) dist %u\n", ai->x, ai->y, d2);
                    queue[d2].push_back(*ai);
                    seen[*ai] = AFF_TRACER;
                }

            seen[c] = placed <= cnt_min ? AFF_YES : AFF_MAYBE;
        }
    }

    return true;
}

aff_type targetter_cloud::is_affected(coord_def loc)
{
    if (!valid_aim(aim))
        return AFF_NO;

    std::map<coord_def, aff_type>::const_iterator it = seen.find(loc);
    if (it == seen.end() || it->second <= 0) // AFF_TRACER is used privately
        return AFF_NO;

    return it->second;
}
