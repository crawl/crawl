#include "AppHdr.h"

#include "branch.h"
#include "branch-data.h"

#include "player.h"
#include "travel.h"

FixedVector<level_id, NUM_BRANCHES> brentry;
FixedVector<int, NUM_BRANCHES> brdepth;
FixedVector<int, NUM_BRANCHES> branch_bribe;
branch_type root_branch;

branch_iterator::branch_iterator() :
    i(BRANCH_DUNGEON)
{
}

branch_iterator::operator bool() const
{
    return i < NUM_BRANCHES;
}

const Branch* branch_iterator::operator*() const
{
    static const branch_type branch_order[] = {
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
        BRANCH_WIZLAB
    };
    COMPILE_CHECK(ARRAYSZ(branch_order) == NUM_BRANCHES);

    if (i < NUM_BRANCHES)
        return &branches[branch_order[i]];
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

branch_type str_to_branch(const string &branch, branch_type err)
{
    for (branch_iterator it; it; ++it)
        if (it->abbrevname && it->abbrevname == branch)
            return it->id;

    return err;
}

int current_level_ambient_noise()
{
    return branches[you.where_are_you].ambient_noise;
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
