/*
 *  File:       spells2.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */

#ifndef SPELLS2_H
#define SPELLS2_H

#include "enum.h"
#include "itemprop-enum.h"

class dist;

bool brand_weapon(brand_type which_brand, int power);
bool burn_freeze(int pow, beam_type flavour, monsters *monster);

void corpse_rot();

bool vampiric_drain(int pow, monsters *monster);
int detect_creatures(int pow, bool telepathic = false);
int detect_items(int pow);
int detect_traps(int pow);
void cast_refrigeration(int pow, bool non_player = false);
void cast_toxic_radiance(bool non_player = false);

bool summon_animals(int pow);
bool cast_summon_butterflies(int pow, god_type god = GOD_NO_GOD);
bool cast_summon_small_mammals(int pow, god_type god = GOD_NO_GOD);

bool item_is_snakable(const item_def& item);
bool cast_sticks_to_snakes(int pow, god_type god = GOD_NO_GOD);

bool cast_summon_scorpions(int pow, god_type god = GOD_NO_GOD);
bool cast_summon_swarm(int pow, god_type god = GOD_NO_GOD);
bool cast_call_canine_familiar(int pow, god_type god = GOD_NO_GOD);
bool cast_summon_elemental(int pow, god_type god = GOD_NO_GOD,
                           monster_type restricted_type = MONS_NO_MONSTER,
                           int unfriendly = 2, int horde_penalty = 0);
bool cast_summon_ice_beast(int pow, god_type god = GOD_NO_GOD);
bool cast_summon_ugly_thing(int pow, god_type god = GOD_NO_GOD);
bool cast_summon_dragon(int pow, god_type god = GOD_NO_GOD);
bool summon_berserker(int pow, god_type god = GOD_NO_GOD, int spell = 0,
                      bool force_hostile = false);
bool summon_holy_warrior(int pow, god_type god = GOD_NO_GOD, int spell = 0,
                         bool force_hostile = false, bool permanent = false,
                         bool quiet = false);
bool cast_tukimas_dance(int pow, god_type god = GOD_NO_GOD,
                        bool force_hostile = false);
bool cast_conjure_ball_lightning(int pow, god_type god = GOD_NO_GOD);

#endif
