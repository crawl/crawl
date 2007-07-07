/*
 *  File:       monsplace.cc
 *  Summary:    Functions used when placing monsters in the dungeon.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef MONPLACE_H
#define MONPLACE_H

#include "enum.h"
#include "dungeon.h"
#include "FixVec.h"

// last updated 13mar2001 {gdl}
/* ***********************************************************************
 * called from: acr - lev-pand - monplace - dungeon
 *
 * Usage:
 * mon_type     WANDERING_MONSTER, RANDOM_MONSTER, or monster type
 * behaviour     standard behaviours (BEH_ENSLAVED, etc)
 * target       MHITYOU, MHITNOT, or monster id
 * extra        various things like skeleton/zombie types, colours, etc
 * summoned     monster is summoned?
 * px           placement x
 * py           placement y
 * level_type   LEVEL_DUNGEON, LEVEL_ABYSS, LEVEL_PANDEMONIUM.
 *              LEVEL_DUNGEON will generate appropriate power monsters
 * proximity    0 = no extra restrictions on monster placement
 *              1 = try to place the monster near the player
 *              2 = don't place the monster near the player
 *              3 = place the monster near stairs (regardless of player pos)
 * *********************************************************************** */
int mons_place( int mon_type, beh_type behaviour, int target, bool summoned,
                int px, int py, int level_type = LEVEL_DUNGEON, 
                proximity_type proximity = PROX_ANYWHERE, int extra = 250,
                int dur = 0, bool permit_bands = false );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - debug - decks - effects - fight - it_use3 - item_use -
 *              items - monstuff - mstuff2 - religion - spell - spells -
 *              spells2 - spells3 - spells4
 * *********************************************************************** */
int create_monster( int cls, int dur, beh_type beha, int cr_x, int cr_y, 
                    int hitting, int zsec, bool permit_bands = false,
                    bool force_place = false, bool force_behaviour = false );

bool player_angers_monster(monsters *mon);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: misc - monplace - spells3
 * *********************************************************************** */
bool empty_surrounds( int emx, int emy, unsigned char spc_wanted, int radius,
                      bool allow_centre, FixedVector<char, 2>& empty );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - items - maps - mstuff2 - spell - spells
 * *********************************************************************** */
int summon_any_demon( demon_class_type demon_class );


// last update 13mar2001 {gdl}
/* ***********************************************************************
 * called from: dungeon monplace
 *
 * This isn't really meant to be a public function.  It is a low level
 * monster placement function used by dungeon building routines and
 * mons_place().  If you need to put a monster somewhere,  use mons_place().
 * Summoned creatures can be created with create_monster().
 * *********************************************************************** */
bool place_monster( int &id, int mon_type, int power, beh_type behaviour,
                    int target, bool summoned, int px, int py, bool allow_bands,
                    proximity_type proximity = PROX_ANYWHERE, int extra = 250,
                    int dur = 0, unsigned mmask = 0 );

monster_type rand_dragon( dragon_class_type type );

/* ***********************************************************************
 * called from: monplace monstuff
 * *********************************************************************** */
void mark_interesting_monst(struct monsters* monster,
                            beh_type behaviour = BEH_SLEEP);

bool grid_compatible(int grid_wanted, int actual_grid, bool generation = false);
bool monster_habitable_grid(int monster_class, int actual_grid, 
                            int flies = -1, bool paralysed = false);
bool monster_habitable_grid(const monsters *m, int actual_grid);
bool monster_can_submerge(int monster_class, int grid);

#endif  // MONPLACE_H
