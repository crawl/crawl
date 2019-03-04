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
    global_xp_info = LevelXPInfo();

    // can't use branch_iterator here, because it depends on a global that
    // may not be inited when you is first constructed.
    for (int i = BRANCH_DUNGEON; i < NUM_BRANCHES; i++)
    {
        branch_info[i] = PlaceInfo();
        branch_info[i].branch = static_cast<branch_type>(i);
        branch_info[i].assert_validity();
    }
}

void player::set_place_info(PlaceInfo place_info)
{
    place_info.assert_validity();

    if (place_info.is_global())
        global_info = place_info;
    else
        branch_info[place_info.branch] = place_info;
}

vector<PlaceInfo> player::get_all_place_info(bool visited_only,
                                             bool dungeon_only) const
{
    vector<PlaceInfo> list;

    for (branch_iterator it; it; ++it)
    {
        if (visited_only && branch_info[it->id].num_visits == 0
            || dungeon_only && !is_connected_branch(*it))
        {
            continue;
        }
        list.push_back(branch_info[it->id]);
    }

    return list;
}

LevelXPInfo::LevelXPInfo()
    : level(level_id(GLOBAL_BRANCH_INFO, -1)), non_vault_xp(0),
      non_vault_count(0), vault_xp(0), vault_count(0)
{
}

LevelXPInfo::LevelXPInfo(const level_id &lev)
    : level(lev), non_vault_xp(0), non_vault_count(0), vault_xp(0),
      vault_count(0)
{
}

bool LevelXPInfo::is_global() const
{
    return level.branch == GLOBAL_BRANCH_INFO;
}

void LevelXPInfo::assert_validity() const
{
    if (level.branch == GLOBAL_BRANCH_INFO)
    {
        ASSERT(level.depth == -1);
        return;
    }

    ASSERT(level.is_valid());
}

const LevelXPInfo &LevelXPInfo::operator += (const LevelXPInfo &other)
{
    vault_xp += other.vault_xp;
    vault_count += other.vault_count;
    non_vault_xp += other.non_vault_xp;
    non_vault_count += other.non_vault_count;

    return *this;
}

const LevelXPInfo &LevelXPInfo::operator -= (const LevelXPInfo &other)
{
    vault_xp -= other.vault_xp;
    vault_count -= other.vault_count;
    non_vault_xp -= other.non_vault_xp;
    non_vault_count -= other.non_vault_count;

    return *this;
}

LevelXPInfo LevelXPInfo::operator + (const LevelXPInfo &other) const
{
    LevelXPInfo copy = *this;
    copy += other;
    return copy;
}

LevelXPInfo LevelXPInfo::operator - (const LevelXPInfo &other) const
{
    LevelXPInfo copy = *this;
    copy -= other;
    return copy;
}

LevelXPInfo& player::get_level_xp_info()
{
    return get_level_xp_info(level_id::current());
}

LevelXPInfo& player::get_level_xp_info(const level_id &lev)
{
    ASSERT(lev.is_valid());
    ASSERT(lev.depth <= brdepth[lev.branch]);

    if (!you.level_xp_info.count(lev))
    {
        LevelXPInfo xp_info;
        xp_info.level = lev;
        level_xp_info[lev] = xp_info;
    }
    return (LevelXPInfo &) level_xp_info[lev];
}

vector<LevelXPInfo> player::get_all_xp_info(bool must_have_kills) const
{
    vector<LevelXPInfo> list;

    for (branch_iterator it; it; ++it)
    {

        for (int d = 1; d <= brdepth[it->id]; ++d)
        {
            const auto lev = level_id(it->id, d);
            map<level_id, LevelXPInfo>::iterator mi
                = you.level_xp_info.find(lev);

            if (mi == you.level_xp_info.end())
                continue;

            if (must_have_kills
                && mi->second.vault_count == 0
                && mi->second.non_vault_count == 0)
            {
                continue;
            }

            list.push_back(mi->second);
        }
    }

    return list;
}

void player::set_level_xp_info(LevelXPInfo &xp_info)
{
    xp_info.assert_validity();

    if (xp_info.is_global())
        global_xp_info = xp_info;
    else
        level_xp_info[xp_info.level] = xp_info;
}
