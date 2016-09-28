#include "AppHdr.h"

#include "branch.h"
#include "branch-data.h"

#include "itemname.h"
#include "player.h"
#include "stringutil.h"
#include "travel.h"

FixedVector<level_id, NUM_BRANCHES> brentry;
FixedVector<int, NUM_BRANCHES> brdepth;
FixedVector<int, NUM_BRANCHES> branch_bribe;
branch_type root_branch;

/// A save-compat ordering for branches.
static const branch_type logical_branch_order[] = {
    BRANCH_DUNGEON,
    BRANCH_TEMPLE,
    BRANCH_LAIR,
    BRANCH_SWAMP,
    BRANCH_SHOALS,
    BRANCH_SNAKE,
    BRANCH_SPIDER,
    BRANCH_SLIME,
    BRANCH_ORC,
    BRANCH_ELF,
#if TAG_MAJOR_VERSION == 34
    BRANCH_DWARF,
#endif
    BRANCH_VAULTS,
#if TAG_MAJOR_VERSION == 34
    BRANCH_BLADE,
    BRANCH_FOREST,
#endif
    BRANCH_CRYPT,
    BRANCH_TOMB,
    BRANCH_DEPTHS,
    BRANCH_VESTIBULE,
    BRANCH_DIS,
    BRANCH_GEHENNA,
    BRANCH_COCYTUS,
    BRANCH_TARTARUS,
    BRANCH_ZOT,
    BRANCH_ABYSS,
    BRANCH_PANDEMONIUM,
    BRANCH_ZIGGURAT,
    BRANCH_LABYRINTH,
    BRANCH_BAZAAR,
    BRANCH_TROVE,
    BRANCH_SEWER,
    BRANCH_OSSUARY,
    BRANCH_BAILEY,
    BRANCH_ICE_CAVE,
    BRANCH_VOLCANO,
    BRANCH_WIZLAB,
    BRANCH_DESOLATION,
};
COMPILE_CHECK(ARRAYSZ(logical_branch_order) == NUM_BRANCHES);

/// Branches ordered loosely by challenge level.
static const branch_type danger_branch_order[] = {
    BRANCH_TEMPLE,
    BRANCH_BAZAAR,
    BRANCH_TROVE,
    BRANCH_DUNGEON,
    BRANCH_SEWER,
    BRANCH_OSSUARY,
    BRANCH_BAILEY,
    BRANCH_LAIR,
    BRANCH_ICE_CAVE,
    BRANCH_VOLCANO,
    BRANCH_LABYRINTH,
    BRANCH_ORC,
    BRANCH_SWAMP,
    BRANCH_SHOALS,
    BRANCH_SNAKE,
    BRANCH_SPIDER,
    BRANCH_VAULTS,
    BRANCH_ELF,
    BRANCH_CRYPT,
    BRANCH_DESOLATION,
    BRANCH_ABYSS,
    BRANCH_WIZLAB,
    BRANCH_SLIME,
    BRANCH_DEPTHS,
    BRANCH_VESTIBULE,
    BRANCH_ZOT,
    BRANCH_PANDEMONIUM,
    BRANCH_TARTARUS,
    BRANCH_GEHENNA,
    BRANCH_COCYTUS,
    BRANCH_DIS,
    BRANCH_TOMB,
    BRANCH_ZIGGURAT,
#if TAG_MAJOR_VERSION == 34
    BRANCH_DWARF,
    BRANCH_BLADE,
    BRANCH_FOREST,
#endif
};
COMPILE_CHECK(ARRAYSZ(danger_branch_order) == NUM_BRANCHES);

branch_iterator::branch_iterator(branch_iterator_type type) :
    iter_type(type), i(0)
{
}

const branch_type* branch_iterator::branch_order() const
{
    if (iter_type == BRANCH_ITER_DANGER)
        return danger_branch_order;
    return logical_branch_order;
}

branch_iterator::operator bool() const
{
    return i < NUM_BRANCHES;
}

const Branch* branch_iterator::operator*() const
{

    if (i < NUM_BRANCHES)
        return &branches[branch_order()[i]];
    else
        return nullptr;
}

const Branch* branch_iterator::operator->() const
{
    return **this;
}

branch_iterator& branch_iterator::operator++()
{
    i++;
    return *this;
}

branch_iterator branch_iterator::operator++(int)
{
    branch_iterator copy = *this;
    ++(*this);
    return copy;
}

const Branch& your_branch()
{
    return branches[you.where_are_you];
}

bool at_branch_bottom()
{
    return brdepth[you.where_are_you] == you.depth;
}

level_id current_level_parent()
{
    // Never called from X[], we don't have to support levels you're not on.
    if (!you.level_stack.empty())
        return you.level_stack.back().id;

    return find_up_level(level_id::current());
}

bool is_hell_subbranch(branch_type branch)
{
    return branch >= BRANCH_FIRST_HELL
           && branch <= BRANCH_LAST_HELL
           && branch != BRANCH_VESTIBULE;
}

bool is_random_subbranch(branch_type branch)
{
    return parent_branch(branch) == BRANCH_LAIR
           && branch != BRANCH_SLIME;
}

bool is_connected_branch(const Branch *branch)
{
    return !(branch->branch_flags & BFLAG_NO_XLEV_TRAVEL);
}

bool is_connected_branch(branch_type branch)
{
    ASSERT_RANGE(branch, 0, NUM_BRANCHES);
    return is_connected_branch(&branches[branch]);
}

bool is_connected_branch(level_id place)
{
    return is_connected_branch(place.branch);
}

branch_type branch_by_abbrevname(const string &branch, branch_type err)
{
    for (branch_iterator it; it; ++it)
        if (it->abbrevname && it->abbrevname == branch)
            return it->id;

    return err;
}

branch_type branch_by_shortname(const string &branch)
{
    for (branch_iterator it; it; ++it)
        if (it->shortname && it->shortname == branch)
            return it->id;

    return NUM_BRANCHES;
}

int ambient_noise(branch_type branch)
{
    if (branch == NUM_BRANCHES)
        return branches[you.where_are_you].ambient_noise;
    return branches[branch].ambient_noise;
}

branch_type get_branch_at(const coord_def& pos)
{
    return level_id::current().get_next_level_id(pos).branch;
}

bool branch_is_unfinished(branch_type branch)
{
#if TAG_MAJOR_VERSION == 34
    if (branch == BRANCH_DWARF
        || branch == BRANCH_FOREST
        || branch == BRANCH_BLADE)
    {
        return true;
    }
#endif
    return false;
}

branch_type parent_branch(branch_type branch)
{
    if (brentry[branch].is_valid())
        return brentry[branch].branch;
    // If it's not in the game, use the default parent.
    return branches[branch].parent_branch;
}

int runes_for_branch(branch_type branch)
{
    switch (branch)
    {
    case BRANCH_VAULTS:   return VAULTS_ENTRY_RUNES;
    case BRANCH_ZIGGURAT: return ZIG_ENTRY_RUNES;
    case BRANCH_ZOT:      return ZOT_ENTRY_RUNES;
    default:              return 0;
    }
}

/**
 * Describe the ambient noise level in this branch.
 *
 * @param branch The branch in question.
 * @returns      A string describing how noisy or quiet the branch is.
 */
string branch_noise_desc(branch_type br)
{
    string desc;
    const int noise = ambient_noise(br);
    if (noise != 0)
    {
        desc = "This branch is ";
        if (noise > 0)
        {
            desc += make_stringf("%snoisy, and so sound travels %sless far.",
                                 noise > 5 ? "very " : "",
                                 noise > 5 ? "much " : "");
        }
        else
        {
            desc += make_stringf("%s, and so sound travels %sfurther.",
                                 noise < -5 ? "unnaturally silent"
                                            : "very quiet",
                                 noise < -5 ? "much " : "");
        }
    }

    return desc;
}

/**
 * Write a description of the rune(s), if any, this branch contains.
 *
 * @param br             the branch in question
 * @param remaining_only whether to only mention a rune if the player
 *                       hasn't picked it up yet.
 * @returns a string mentioning all applicable runes.
 */
string branch_rune_desc(branch_type br, bool remaining_only)
{
    string desc;
    vector<string> rune_names;

    for (rune_type rune : branches[br].runes)
        if (!(remaining_only && you.runes[rune]))
            rune_names.push_back(rune_type_name(rune));

    if (!rune_names.empty())
    {
        desc = make_stringf("This branch contains the %s rune%s of Zot.",
                            comma_separated_line(begin(rune_names),
                                                 end(rune_names)).c_str(),
                            rune_names.size() > 1 ? "s" : "");
    }

    return desc;
}

branch_type rune_location(rune_type rune)
{
    for (const auto& br : branches)
        if (find(br.runes.begin(), br.runes.end(), rune) != br.runes.end())
            return br.id;

    return NUM_BRANCHES;
}
