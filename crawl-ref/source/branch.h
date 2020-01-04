/**
 * @file
 * @brief Dungeon branch classes
**/

#pragma once

#include "enum.h"
#include "branch-type.h"
#include "item-prop-enum.h"
#include "player.h"

#define BRANCH_NOISE_AMOUNT 6

enum class brflag
{
    none                = 0,

    no_map              = (1 << 0), // Can't be magic mapped or remembered.
    islanded            = (1 << 1), // May have isolated zones with no stairs.
    no_x_level_travel   = (1 << 2), // Can't cross-level travel to or from it.
    no_items            = (1 << 3), // Branch gets no random items.
    dangerous_end       = (1 << 4), // bottom level is more dangerous than normal
    spotty              = (1 << 5), // Connect vaults with more open paths, not hallways.
    no_shafts           = (1 << 6), // Don't generate random shafts.
};
DEF_BITFIELD(branch_flags_t, brflag);

enum class branch_noise
{
    normal,
    quiet,
    loud,
};

struct Branch
{
    branch_type id;
    branch_type parent_branch;

    int mindepth;               // min/max possible parent depth for this branch
    int maxdepth;

    int numlevels;              // depth of the branch
    int absdepth;               // base item generation/etc depth

    branch_flags_t branch_flags;

    dungeon_feature_type entry_stairs;
    dungeon_feature_type exit_stairs;
    dungeon_feature_type escape_feature; // for branches with no up hatches allowed
    const char* shortname;      // "Slime Pits"
    const char* longname;       // "The Pits of Slime"
    const char* abbrevname;     // "Slime"
    const char* entry_message;
    colour_t floor_colour;          // Zot needs special handling.
    colour_t rock_colour;
    int travel_shortcut;         // Which key to press for travel.
    vector<rune_type> runes;      // Contained rune(s) (if any).
    branch_noise ambient_noise; // affects noise loudness
};

enum class branch_iterator_type
{
    logical,
    danger,
};

class branch_iterator
{
public:
    branch_iterator(branch_iterator_type type = branch_iterator_type::logical);

    operator bool() const;
    const Branch* operator*() const;
    const Branch* operator->() const;
    branch_iterator& operator++();
    branch_iterator operator++(int);

private:
    const branch_type* branch_order() const;

private:
    branch_iterator_type iter_type;
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
bool is_hell_branch(branch_type branch);
bool is_random_subbranch(branch_type branch);
bool is_connected_branch(const Branch *branch);
bool is_connected_branch(branch_type branch);
bool is_connected_branch(level_id place);
level_id current_level_parent();

branch_type branch_by_abbrevname(const string &branch, branch_type err = NUM_BRANCHES);
branch_type branch_by_shortname(const string &branch);

int ambient_noise(branch_type branch = you.where_are_you);

branch_type get_branch_at(const coord_def& pos);
bool branch_is_unfinished(branch_type branch);

branch_type parent_branch(branch_type branch);
int runes_for_branch(branch_type branch);

string branch_noise_desc(branch_type br);
string branch_rune_desc(branch_type br, bool remaining_only);
branch_type rune_location(rune_type rune);

vector<branch_type> random_choose_disabled_branches();
