/*
 *  File:       branch.h
 *  Summary:    Dungeon branch classes
 *  Written by: Haran Pilpel
 *
 *  Modified for Crawl Reference by $Author: haranp $ on $Date: 2006-11-29 13:12:52 -0500 (Wed, 29 Nov 2006) $
 *
 */

#ifndef BRANCH_H
#define BRANCH_H

#include "enum.h"

enum branch_flag_type
{
    BFLAG_NONE = 0,

    BFLAG_NO_TELE_CONTROL = (1 << 0), // Teleport control not allowed.
    BFLAG_NOT_MAPPABLE    = (1 << 1), // Branch levels not mappable.
    BFLAG_NO_MAGIC_MAP    = (1 << 2)  // Branch levels can't be magic mapped.
};

struct Branch
{
    branch_type id;
    branch_type parent_branch;
    int depth;
    int startdepth;             // which level of the parent branch,
                                // 1 for first level
    unsigned long branch_flags;
    unsigned long default_level_flags;
    dungeon_feature_type entry_stairs;
    dungeon_feature_type exit_stairs;
    const char* shortname;      // "Slime Pits"
    const char* longname;       // "The Pits of Slime"
    const char* abbrevname;     // "Slime"
    const char* entry_message;
    bool has_shops;
    bool has_uniques;
    char floor_colour;          // Zot needs special handling
    char rock_colour;
    int (*mons_rarity_function)(int);
    int (*mons_level_function)(int);
    int altar_chance;           // in percent
    int travel_shortcut;        // which key to press for travel
    bool any_upstair_exits;     // any upstair exits the branch (Hell branches)
};

extern Branch branches[];

Branch& your_branch();
branch_type str_to_branch(const std::string &branch,
                          branch_type err = NUM_BRANCHES);

bool set_branch_flags(unsigned long flags, bool silent = false,
                      branch_type branch = NUM_BRANCHES);
bool unset_branch_flags(unsigned long flags, bool silent = false,
                        branch_type branch = NUM_BRANCHES);
unsigned long get_branch_flags(branch_type branch = NUM_BRANCHES);

#endif
