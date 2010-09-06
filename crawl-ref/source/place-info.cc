#include "AppHdr.h"

#include "place-info.h"

#include "branch.h"
#include "player.h"

PlaceInfo::PlaceInfo()
    : level_type(-2), branch(-2), num_visits(0),
      levels_seen(0), mon_kill_exp(0), mon_kill_exp_avail(0),
      turns_total(0), turns_explore(0), turns_travel(0), turns_interlevel(0),
      turns_resting(0), turns_other(0), elapsed_total(0),
      elapsed_explore(0), elapsed_travel(0), elapsed_interlevel(0),
      elapsed_resting(0), elapsed_other(0)
{
    for (int i = 0; i < KC_NCATEGORIES; i++)
        mon_kill_num[i] = 0;
}

bool PlaceInfo::is_global() const
{
    return (level_type == -1 && branch == -1);
}

void PlaceInfo::make_global()
{
    level_type = -1;
    branch     = -1;
}

void PlaceInfo::assert_validity() const
{
    // Check that level_type and branch match up.
    ASSERT(is_global()
           || level_type == LEVEL_DUNGEON && branch >= BRANCH_MAIN_DUNGEON
              && branch < NUM_BRANCHES
           || level_type > LEVEL_DUNGEON && level_type < NUM_LEVEL_AREA_TYPES
              && branch == -1);

    // Can't have visited a place without seeing any of its levels, and
    // vice versa.
    ASSERT(num_visits == 0 && levels_seen == 0
           || num_visits > 0 && levels_seen > 0);

    if (level_type == LEVEL_LABYRINTH || level_type == LEVEL_ABYSS)
        ASSERT(num_visits == levels_seen);
    else if (level_type == LEVEL_PANDEMONIUM)
        // Ziggurats can allow a player to return to the same
        // Pandemonium level.
        // ASSERT(num_visits <= levels_seen);
        ;
    // Commented out to allow games with broken place_info to continue.
    // Please uncomment at a later point in 0.8 development. (jpeg)
    else if (level_type == LEVEL_DUNGEON && branches[branch].depth > 0)
        ASSERT(levels_seen <= (unsigned long) branches[branch].depth);

    ASSERT(turns_total == (turns_explore + turns_travel + turns_interlevel
                           + turns_resting + turns_other));

    ASSERT(elapsed_total == (elapsed_explore + elapsed_travel
                             + elapsed_interlevel + elapsed_resting
                             + elapsed_other));
}

const std::string PlaceInfo::short_name() const
{
    if (level_type == LEVEL_DUNGEON)
        return branches[branch].shortname;
    else
    {
        switch (level_type)
        {
        case LEVEL_ABYSS:
            return "Abyss";

        case LEVEL_PANDEMONIUM:
            return "Pandemonium";

        case LEVEL_LABYRINTH:
            return "Labyrinth";

        case LEVEL_PORTAL_VAULT:
            return "Portal Chamber";

        default:
            return "Bug";
        }
    }
}

const PlaceInfo &PlaceInfo::operator += (const PlaceInfo &other)
{
    num_visits  += other.num_visits;
    levels_seen += other.levels_seen;

    mon_kill_exp       += other.mon_kill_exp;
    mon_kill_exp_avail += other.mon_kill_exp_avail;

    for (int i = 0; i < KC_NCATEGORIES; i++)
        mon_kill_num[i] += other.mon_kill_num[i];

    turns_total      += other.turns_total;
    turns_explore    += other.turns_explore;
    turns_travel     += other.turns_travel;
    turns_interlevel += other.turns_interlevel;
    turns_resting    += other.turns_resting;
    turns_other      += other.turns_other;

    elapsed_total      += other.elapsed_total;
    elapsed_explore    += other.elapsed_explore;
    elapsed_travel     += other.elapsed_travel;
    elapsed_interlevel += other.elapsed_interlevel;
    elapsed_resting    += other.elapsed_resting;
    elapsed_other      += other.elapsed_other;

    return (*this);
}

const PlaceInfo &PlaceInfo::operator -= (const PlaceInfo &other)
{
    num_visits  -= other.num_visits;
    levels_seen -= other.levels_seen;

    mon_kill_exp       -= other.mon_kill_exp;
    mon_kill_exp_avail -= other.mon_kill_exp_avail;

    for (int i = 0; i < KC_NCATEGORIES; i++)
        mon_kill_num[i] -= other.mon_kill_num[i];

    turns_total      -= other.turns_total;
    turns_explore    -= other.turns_explore;
    turns_travel     -= other.turns_travel;
    turns_interlevel -= other.turns_interlevel;
    turns_resting    -= other.turns_resting;
    turns_other      -= other.turns_other;

    elapsed_total      -= other.elapsed_total;
    elapsed_explore    -= other.elapsed_explore;
    elapsed_travel     -= other.elapsed_travel;
    elapsed_interlevel -= other.elapsed_interlevel;
    elapsed_resting    -= other.elapsed_resting;
    elapsed_other      -= other.elapsed_other;

    return (*this);
}

PlaceInfo PlaceInfo::operator + (const PlaceInfo &other) const
{
    PlaceInfo copy = *this;
    copy += other;
    return copy;
}

PlaceInfo PlaceInfo::operator - (const PlaceInfo &other) const
{
    PlaceInfo copy = *this;
    copy -= other;
    return copy;
}

PlaceInfo& player::get_place_info() const
{
    return get_place_info(where_are_you, level_type);
}

PlaceInfo& player::get_place_info(branch_type branch) const
{
    return get_place_info(branch, LEVEL_DUNGEON);
}

PlaceInfo& player::get_place_info(level_area_type level_type2) const
{
    return get_place_info(NUM_BRANCHES, level_type2);
}

PlaceInfo& player::get_place_info(branch_type branch,
                                  level_area_type level_type2) const
{
    ASSERT(level_type2 == LEVEL_DUNGEON && branch >= BRANCH_MAIN_DUNGEON
                && branch < NUM_BRANCHES
           || level_type2 > LEVEL_DUNGEON && level_type < NUM_LEVEL_AREA_TYPES);

    if (level_type2 == LEVEL_DUNGEON)
        return (PlaceInfo&) branch_info[branch];
    else
        return (PlaceInfo&) non_branch_info[level_type2 - 1];
}

void player::clear_place_info()
{
    you.global_info = PlaceInfo();
    for (unsigned int i = 0; i < NUM_BRANCHES; ++i)
        branch_info[i] = PlaceInfo();
    for (unsigned int i = 0; i < NUM_LEVEL_AREA_TYPES - 1; ++i)
        non_branch_info[i] = PlaceInfo();
}
