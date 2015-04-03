/**
 * @file
 * @brief Dungeon branch classes
**/

#ifndef BRANCH_H
#define BRANCH_H

#include "enum.h"

enum branch_flag_type
{
    BFLAG_NONE = 0,

    BFLAG_ISLANDED        = (1 << 1), // May have isolated zones with no stairs.
    BFLAG_NO_XLEV_TRAVEL  = (1 << 2), // Can't cross-level travel to or from it.
    BFLAG_NO_ITEMS        = (1 << 3), // Branch gets no random items.
    BFLAG_DANGEROUS_END   = (1 << 4), // bottom level is more dangerous than normal
    BFLAG_SPOTTY          = (1 << 5), // Connect vaults with more open paths, not hallways.
};

struct Branch
{
    branch_type id;
    branch_type parent_branch;

    int mindepth;               // min/max possible parent depth for this branch
    int maxdepth;

    int numlevels;              // depth of the branch
    int absdepth;               // base item generation/etc depth

    uint32_t branch_flags;
    uint32_t default_level_flags;
    dungeon_feature_type entry_stairs;
    dungeon_feature_type exit_stairs;
    const char* shortname;      // "Slime Pits"
    const char* longname;       // "The Pits of Slime"
    const char* abbrevname;     // "Slime"
    const char* entry_message;
    colour_t floor_colour;          // Zot needs special handling.
    colour_t rock_colour;
    int travel_shortcut;         // Which key to press for travel.
    int ambient_noise;           // affects noise loudness and player stealth
};

class branch_iterator
{
public:
    branch_iterator();

    operator bool() const;
    const Branch* operator*() const;
    const Branch* operator->() const;
    branch_iterator& operator++();
    branch_iterator operator++(int);

protected:
    int i;
};

extern const Branch branches[NUM_BRANCHES];
extern FixedVector<level_id, NUM_BRANCHES> brentry;
extern FixedVector<int, NUM_BRANCHES> brdepth;
extern FixedVector<int, NUM_BRANCHES> branch_bribe;
extern branch_type root_branch;

const Branch& your_branch();

bool at_branch_bottom();
bool is_hell_subbranch(branch_type branch);
bool is_random_subbranch(branch_type branch);
bool is_connected_branch(const Branch *branch);
bool is_connected_branch(branch_type branch);
bool is_connected_branch(level_id place);
level_id current_level_parent();

branch_type str_to_branch(const string &branch, branch_type err = NUM_BRANCHES);

int current_level_ambient_noise();

branch_type get_branch_at(const coord_def& pos);
bool branch_is_unfinished(branch_type branch);

branch_type parent_branch(branch_type branch);
#endif
