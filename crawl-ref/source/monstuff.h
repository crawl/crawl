/*
 *  File:       monstuff.cc
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MONSTUFF_H
#define MONSTUFF_H

#include "mon-util.h"

// useful macro
#define SAME_ATTITUDE(x) (mons_friendly(x)?BEH_FRIENDLY:BEH_HOSTILE)

#define MONST_INTERESTING(x) (x->flags & MF_INTERESTING)

// for definition of type monsters {dlb}
#include "externs.h"
// for definition of type monsters {dlb}

void get_mimic_item( const monsters *mimic, item_def & item );
int  get_mimic_colour( const monsters *mimic );

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
   * called from: fight - item_use - items - spell
   * *********************************************************************** */
void alert_nearby_monsters(void);

enum poly_power_type {
    PPT_LESS,
    PPT_MORE,
    PPT_SAME
};
bool monster_polymorph(monsters *monster, monster_type targetc,
                       poly_power_type p = PPT_SAME);

// last updated: 08jun2000 {dlb}
/* ***********************************************************************
   * called from: bang - beam - effects - fight - misc - monstuff - mstuff2 -
   *              spells1 - spells2 - spells3 - spells4
   * *********************************************************************** */
void monster_die(monsters *monster, killer_type killer,
                 int i, bool silent = false);

void mons_check_pool(monsters *monster, killer_type killer = KILL_NONE,
                     int killnum = -1);

// last updated: 17dec2000 {gdl}
/* ***********************************************************************
   * called from: monstuff - fight
   * *********************************************************************** */
void monster_cleanup(struct monsters *monster);


/* ***********************************************************************
 * called from: monstuff beam effects fight view
 * *********************************************************************** */
void behaviour_event( struct monsters *mon, int event_type, 
                      int src = MHITNOT, int src_x = 0, int src_y = 0 ); 

/* ***********************************************************************
 * called from: fight - it_use3 - spells
 * *********************************************************************** */
bool curse_an_item(bool decay_potions);


/* ***********************************************************************
 * called from: fight
 * *********************************************************************** */
bool monster_blink(struct monsters *monster);


/* ***********************************************************************
 * called from: spells1 spells4 monstuff
 * defaults are set up for player blink;  monster blink should call with
 * false, false
 * *********************************************************************** */
bool random_near_space( int ox, int oy, int &tx, int &ty,
    bool allow_adjacent = false, bool restrict_LOS = true);


/* ***********************************************************************
 * called from: beam - effects - fight - monstuff - mstuff2 - spells1 -
 *              spells2 - spells4
 * *********************************************************************** */
bool simple_monster_message(const monsters *monster, const char *event,
                            int channel = MSGCH_PLAIN, int param = 0,
                            description_level_type descrip = DESC_CAP_THE);

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool swap_places(struct monsters *monster);


/* ***********************************************************************
 * called from: bang - beam - direct - fight - spells1 - spells2 - spells3
 * *********************************************************************** */
void print_wounds(struct monsters *monster);


/* ***********************************************************************
 * called from: fight
 * *********************************************************************** */
bool wounded_damaged(int wound_class);


/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void handle_monsters(void);


/* ***********************************************************************
 * called from: acr - bang - beam - direct - dungeon - fight - files -
 *              monplace - mstuff2 - spells3 - view
 * *********************************************************************** */
dungeon_feature_type monster_habitat(int which_class);


/* ***********************************************************************
 * called from: misc
 * *********************************************************************** */
bool monster_descriptor(int which_class, unsigned char which_descriptor);


/* ***********************************************************************
 * called from: direct - item_use - spells1
 * *********************************************************************** */
bool message_current_target(void);


/* ***********************************************************************
 * called from: xxx
 * *********************************************************************** */
unsigned int monster_index(const monsters *monster);


// last updated 08jun2000 {dlb}
/* ***********************************************************************
 * called from: bang - beam - effects - fight - monstuff - mstuff2 -
 *              spells2 - spells3 - spells4
 * *********************************************************************** */
bool hurt_monster(struct monsters *victim, int damage_dealt);


/* ***********************************************************************
 * called from: beam - fight - files - monstuff - spells1
 * *********************************************************************** */
bool heal_monster(struct monsters *patient, int health_boost, bool permit_growth);

/* ***********************************************************************
 * called from: monplace - spells2 - view
 * *********************************************************************** */
void seen_monster(struct monsters *monster);

bool shift_monster( struct monsters *mon, int x = 0, int y = 0 );

int mons_weapon_damage_rating(const item_def &launcher);
int mons_pick_best_missile(monsters *mons, item_def **launcher,
                           bool ignore_melee = false);
int mons_missile_damage(const item_def *launch,
                        const item_def *missile);
int mons_thrown_weapon_damage(const item_def *weap);

#endif
