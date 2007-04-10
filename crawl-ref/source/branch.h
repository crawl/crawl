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

struct Branch
{
    branch_type id;
    branch_type parent_branch;
    int depth;
    int startdepth;             // which level of the parent branch,
                                // 1 for first level
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
};

extern Branch branches[];

Branch& your_branch();

#endif
