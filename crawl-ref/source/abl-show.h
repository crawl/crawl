/*
 *  File:       abl-show.h
 *  Summary:    Functions related to special abilities.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>    --/--/--        LRH             Created
 */


#ifndef ABLSHOW_H
#define ABLSHOW_H

#include "enum.h"

#include <string>
#include <vector>

// Structure for representing an ability:
struct ability_def
{
    ability_type        ability;
    const char *        name;
    unsigned int        mp_cost;        // magic cost of ability
    unsigned int        hp_cost;        // hit point cost of ability
    unsigned int        food_cost;      // + rand2avg( food_cost, 2 )
    unsigned int        piety_cost;     // + random2( (piety_cost + 1) / 2 + 1 )
    unsigned int        flags;          // used for additonal cost notices
};

struct talent
{
    ability_type which;
    int hotkey;
    int fail;
    bool is_invocation;
};

const struct ability_def & get_ability_def( ability_type abil );

const char* ability_name(ability_type ability);
const std::string make_cost_description( ability_type ability );
std::vector<const char*> get_ability_names();
int choose_ability_menu(const std::vector<talent>& talents);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool activate_ability();
std::vector<talent> your_talents( bool check_confused );

std::string print_abilities( void );

void set_god_ability_slots( void );


#endif
