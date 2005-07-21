/*
 *  File:       abl-show.h
 *  Summary:    Functions related to special abilities.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>    --/--/--        LRH             Created
 */


#ifndef ABLSHOW_H
#define ABLSHOW_H

#include <string>

// Structure for representing an ability:
struct ability_def
{
    int                 ability;
    const char *        name;
    unsigned int        mp_cost;        // magic cost of ability
    unsigned int        hp_cost;        // hit point cost of ability
    unsigned int        food_cost;      // + rand2avg( food_cost, 2 )
    unsigned int        piety_cost;     // + random2( (piety_cost + 1) / 2 + 1 )
    unsigned int        flags;          // used for additonal cost notices
};

const struct ability_def & get_ability_def( int abil );

const char * get_ability_name_by_index( char index );

const std::string   make_cost_description( const struct ability_def &abil );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool activate_ability( void );     // handles all special abilities now
char show_abilities( void );
bool generate_abilities( void );

void set_god_ability_slots( void );


#endif
