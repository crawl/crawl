#include "AppHdr.h"

#include "place-info.h"

#include "branch.h"
#include "player.h"

PlaceInfo::PlaceInfo()
    : branch(GLOBAL_BRANCH_INFO), num_visits(0),
      levels_seen(0), mon_kill_exp(0), turns_total(0), turns_explore(0),
      turns_travel(0), turns_interlevel(0), turns_resting(0),
      turns_other(0), elapsed_total(0), elapsed_explore(0),
      elapsed_travel(0), elapsed_interlevel(0), elapsed_resting(0),
      elapsed_other(0)
{
    for (int i = 0; i < KC_NCATEGORIES; i++)
        mon_kill_num[i] = 0;
}

bool PlaceInfo::is_global() const
{
    return branch == GLOBAL_BRANCH_INFO;
}

void PlaceInfo::assert_validity() const
{
    // Can't have visited a place without seeing any of its levels, and
    // vice versa.
    ASSERT(num_visits == 0 && levels_seen == 0
           || num_visits > 0 && levels_seen > 0);

    if (!is_global() && brdepth[branch] != -1 && is_connected_branch(branch))
        ASSERT((int)levels_seen <= brdepth[branch]);

    ASSERT(turns_total == (turns_explore + turns_travel + turns_interlevel
                           + turns_resting + turns_other));

    ASSERT(elapsed_total == (elapsed_explore + elapsed_travel
                             + elapsed_interlevel + elapsed_resting
                             + elapsed_other));
}

const string PlaceInfo::short_name() const
{
    return branches[branch].shortname;
}

const PlaceInfo &PlaceInfo::operator += (const PlaceInfo &other)
{
    num_visits  += other.num_visits;
    levels_seen += other.levels_seen;

    mon_kill_exp       += other.mon_kill_exp;

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

    return *this;
}

const PlaceInfo &PlaceInfo::operator -= (const PlaceInfo &other)
{
    num_visits  -= other.num_visits;
    levels_seen -= other.levels_seen;

    mon_kill_exp       -= other.mon_kill_exp;

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

    return *this;
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
    return get_place_info(where_are_you);
}

PlaceInfo& player::get_place_info(branch_type branch) const
{
    ASSERT(branch < NUM_BRANCHES);
    return (PlaceInfo&) branch_info[branch];
}

void player::clear_place_info()
{
    global_info = PlaceInfo();
    for (branch_iterator it; it; ++it)
    {
        branch_info[it->id] = PlaceInfo();
        branch_info[it->id].branch = it->id;
        branch_info[it->id].assert_validity();
    }
}
