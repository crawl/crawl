/**
 * @file
 * @brief Functions for spattering blood all over the place.
 **/

#include "AppHdr.h"

#include "bloodspatter.h"

#include "cloud.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "losglobal.h"
#include "mpr.h"
#include "religion.h"
#include "shout.h"
#include "terrain.h"

/**
 * Is it okay to bleed onto the given square?
 *
 * @param where      The square in question.
 * @param to_ignite  Whether the blood will be ignited right
 *                   after.
 * @return           Whether blood is permitted.
 */
static bool _allow_bleeding_on_square(const coord_def& where,
                                      bool to_ignite = false)
{
    // No bleeding onto sanctuary ground, please.
    if (is_sanctuary(where))
        return false;

    // Also not necessary if already covered in blood
    // Unless we're going to set it on fire right after.
    if (is_bloodcovered(where) && !to_ignite)
        return false;

    // No spattering into lava or water.
    if (feat_is_lava(env.grid(where)) || feat_is_water(env.grid(where)))
        return false;

    // No spattering into fountains (other than blood).
    if (env.grid(where) == DNGN_FOUNTAIN_BLUE
        || env.grid(where) == DNGN_FOUNTAIN_SPARKLING)
    {
        return false;
    }

    // The good gods like to keep their altars pristine.
    if (is_good_god(feat_altar_god(env.grid(where))))
        return false;

    return true;
}

bool maybe_bloodify_square(const coord_def& where)
{
    if (!_allow_bleeding_on_square(where))
        return false;

    env.pgrid(where) |= FPROP_BLOODY;
    return true;
}

/**
 * Rotate the wall blood splat tile, so that it is facing the source.
 *
 * Wall blood splat tiles are drawned with the blood dripping down. We need
 * the tile to be facing an orthogonal empty space for the effect to look
 * good. We choose the empty space closest to the source of the blood.
 *
 * @param where Coordinates of the wall where there is a blood splat.
 * @param from Coordinates of the source of the blood.
 * @param old_blood blood splats created at level generation are old and can
 * have some blood inscriptions. Only for south facing splats, so you don't
 * have to turn your head to read the inscriptions.
 */
static void _orient_wall_blood(const coord_def& where, coord_def from,
                               bool old_blood)
{
    if (!feat_is_wall(env.grid(where)))
        return;

    if (from == INVALID_COORD)
        from = where;

    coord_def closer = INVALID_COORD;
    int dist = INT_MAX;
    for (orth_adjacent_iterator ai(where); ai; ++ai)
    {
        if (in_bounds(*ai) && !cell_is_solid(*ai)
            && cell_see_cell(from, *ai, LOS_SOLID)
            && (distance2(*ai, from) < dist
                || distance2(*ai, from) == dist && coinflip()))
        {
            closer = *ai;
            dist = distance2(*ai, from);
        }
    }

    // If we didn't find anything, the wall is in a corner.
    // We don't want blood tile there.
    if (closer == INVALID_COORD)
    {
        env.pgrid(where) &= ~FPROP_BLOODY;
        return;
    }

    const coord_def diff = where - closer;
    if (diff == coord_def(1, 0))
        env.pgrid(where) |= FPROP_BLOOD_WEST;
    else if (diff == coord_def(0, 1))
        env.pgrid(where) |= FPROP_BLOOD_NORTH;
    else if (diff == coord_def(-1, 0))
        env.pgrid(where) |= FPROP_BLOOD_EAST;
    else if (old_blood && one_chance_in(10))
        env.pgrid(where) |= FPROP_OLD_BLOOD;
}

static void _maybe_bloodify_square(const coord_def& where, int amount,
                                   bool spatter = false,
                                   const coord_def& from = INVALID_COORD,
                                   const bool old_blood = false)
{
    if (amount < 1)
        return;

    int ignite_blood = you.get_mutation_level(MUT_IGNITE_BLOOD);

    bool may_bleed = _allow_bleeding_on_square(where,
                        ignite_blood > 0 && you.see_cell(where));

    if (ignite_blood && you.see_cell(where))
        amount *= 2;

    if (!x_chance_in_y(amount, 20))
        return;

    dprf("might bleed now; square: (%d, %d); amount = %d",
         where.x, where.y, amount);

    if (may_bleed)
    {
        env.pgrid(where) |= FPROP_BLOODY;
        _orient_wall_blood(where, from, old_blood);

        if (x_chance_in_y(ignite_blood, 3)
            && you.see_cell(where)
            && !cell_is_solid(where)
            && !cloud_at(where))
        {
            int dur = 2 + ignite_blood + random2(2 * ignite_blood);
            place_cloud(CLOUD_FIRE, where, dur, &you, -1, -1,
                        false); // Don't give penance.
        }
    }

    // Smaller chance of spattering surrounding squares.
    if (spatter)
        for (adjacent_iterator ai(where); ai; ++ai)
            _maybe_bloodify_square(*ai, amount/15, false, from, old_blood);
}

// Colour ground (and possibly adjacent squares) red. "damage" depends on damage
// taken (or hitpoints, if damage higher).
void bleed_onto_floor(const coord_def& where, monster_type montype,
                      int damage, bool spatter, const coord_def& from,
                      const bool old_blood)
{
    ASSERT_IN_BOUNDS(where);

    if (montype == MONS_PLAYER && !you.has_blood())
        return;

    if (montype != NUM_MONSTERS && montype != MONS_PLAYER)
    {
        monster m;
        m.type = montype;
        if (!m.has_blood())
            return;
    }

    _maybe_bloodify_square(where, damage, spatter, from, old_blood);
}

void blood_spray(const coord_def& origin, monster_type montype, int level)
{
    int tries = 0;
    for (int i = 0; i < level; ++i)
    {
        // Blood drops are small and light and suffer a lot of wind
        // resistance.
        int range = random2(8) + 1;

        while (tries < 5000)
        {
            ++tries;

            coord_def bloody = origin;
            bloody.x += random_range(-range, range);
            bloody.y += random_range(-range, range);

            if (in_bounds(bloody) && cell_see_cell(origin, bloody, LOS_SOLID))
            {
                bleed_onto_floor(bloody, montype, 99, false, origin);
                break;
            }
        }
    }
}

static void _spatter_neighbours(const coord_def& where, int chance,
                                const coord_def& from = INVALID_COORD)
{
    for (adjacent_iterator ai(where, false); ai; ++ai)
    {
        if (!_allow_bleeding_on_square(*ai))
            continue;

        if (one_chance_in(chance))
        {
            env.pgrid(*ai) |= FPROP_BLOODY;
            _orient_wall_blood(where, from, true);
            _spatter_neighbours(*ai, chance+1, from);
        }
    }
}

void generate_random_blood_spatter_on_level(const map_bitmask *susceptible_area)
{
    const int max_cluster = 7 + random2(9);

    // Lower chances for large bloodshed areas if we have many clusters,
    // but increase chances if we have few.
    // Chances for startprob are [1..3] for 7-9 clusters,
    //                       ... [1..4] for 10-12 clusters, and
    //                       ... [2..5] for 13-15 clusters.

    int min_prob = 1;
    int max_prob = 4;

    if (max_cluster < 10)
        max_prob--;
    else if (max_cluster > 12)
        min_prob++;

    for (int i = 0; i < max_cluster; ++i)
    {
        const coord_def c = random_in_bounds();

        if (susceptible_area && !(*susceptible_area)(c))
            continue;

        // startprob is used to initialise the chance for neighbours
        // being spattered, which will be decreased by 1 per recursion
        // round. We then use one_chance_in(chance) to determine
        // whether to spatter a given grid or not. Thus, startprob = 1
        // means that initially all surrounding grids will be
        // spattered (3x3), and the _higher_ startprob the _lower_ the
        // overall chance for spattering and the _smaller_ the
        // bloodshed area.
        const int startprob = min_prob + random2(max_prob);

        maybe_bloodify_square(c);

        _spatter_neighbours(c, startprob);
    }
}

// Always produce one puddle of blood, at an increasingly large radius as nearby
// tiles become filled.
void bleed_for_makhleb(const actor& actor)
{
    for (distance_iterator di(actor.pos(), true, false, 2); di; ++di)
    {
        if (!is_bloodcovered(*di) && _allow_bleeding_on_square(*di)
            && cell_see_cell(actor.pos(), *di, LOS_NO_TRANS))
        {
            bleed_onto_floor(*di, actor.type, 20);
            return;
        }
    }
}
