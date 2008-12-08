/*
 *  File:       mstuff2.h
 *  Summary:    Misc monster related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef MSTUFF2_H
#define MSTUFF2_H


#include <string>
#include "externs.h"

struct bolt;

bolt mons_spells(monsters *mons, spell_type spell_cast, int power);
void setup_dragon(monsters *monster, bolt &pbolt);
void mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast);
void setup_mons_cast(monsters *monster, bolt &pbolt, spell_type spell_cast);
bool mons_throw(monsters *monster, bolt &pbolt, int hand_used);
bool mons_thrown_object_destroyed( item_def *item, const coord_def& where,
                                   bool returning, int midx );
void setup_generic_throw(monsters *monster, bolt &pbolt);
void mons_trap(monsters *monster);
bool monster_random_space(const monsters *monster, coord_def& target,
                          bool forbid_sanctuary = false);
bool monster_random_space(monster_type mon, coord_def& target,
                          bool forbid_sanctuary = false);
void monster_teleport(monsters *monster, bool instan, bool silent = false);
bool orc_battle_cry(monsters *chief);
bool orange_statue_effects(monsters *mons);
bool silver_statue_effects(monsters *mons);
bool moth_incite_monsters(const monsters *mon);
void mons_clear_trapping_net(monsters *mon);

bool mons_clonable(const monsters* orig, bool needs_adjacent = true);
int clone_mons(const monsters* orig, bool quiet = false,
               bool* obvious = NULL, coord_def pos = coord_def(0, 0) );

#endif
