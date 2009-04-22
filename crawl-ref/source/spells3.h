/*
 *  File:       spells3.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef SPELLS3_H
#define SPELLS3_H

#include "itemprop.h"  // from _raise_remains()

struct dist;
struct bolt;

bool allow_control_teleport(bool quiet = false);
int airstrike(int power, dist &beam);
bool cast_bone_shards(int power, bolt &);

void cast_poison_ammo();
bool cast_selective_amnesia(bool force);
bool cast_smiting(int power, const coord_def& where);
bool remove_sanctuary(bool did_attack = false);
void decrease_sanctuary_radius();
bool cast_sanctuary(const int power);
int halo_radius();
bool inside_halo(const coord_def& where);
bool project_noise();
bool detect_curse(bool suppress_msg);
bool entomb(int powc);
int portal();
bool recall(char type_recalled);
bool remove_curse(bool suppress_msg);
bool cast_sublimation_of_blood(int pow);

bool cast_call_imp(int pow, god_type god = GOD_NO_GOD);
bool summon_lesser_demon(int pow, god_type god = GOD_NO_GOD, int spell = 0,
                         bool quiet = false);
bool summon_common_demon(int pow, god_type god = GOD_NO_GOD, int spell = 0,
                         bool quiet = false);
bool summon_greater_demon(int pow, god_type god = GOD_NO_GOD, int spell = 0,
                          bool quiet = false);
bool summon_demon_type(monster_type mon, int pow, god_type god = GOD_NO_GOD,
                       int spell = 0);
bool cast_summon_demon(int pow, god_type god = GOD_NO_GOD);
bool cast_demonic_horde(int pow, god_type god = GOD_NO_GOD);
bool cast_summon_greater_demon(int pow, god_type god = GOD_NO_GOD);
bool cast_shadow_creatures(god_type god = GOD_NO_GOD);
bool cast_summon_horrible_things(int pow, god_type god = GOD_NO_GOD);

void equip_undead(const coord_def &a, int corps, int monster, int monnum);
int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    god_type god = GOD_NO_GOD, bool actual = true,
                    bool quiet = false, int* mon_index = NULL);

int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 god_type god = GOD_NO_GOD, bool actual = true);

bool cast_simulacrum(int pow, god_type god = GOD_NO_GOD);
bool cast_twisted_resurrection(int pow, god_type god = GOD_NO_GOD);
bool cast_summon_wraiths(int pow, god_type god = GOD_NO_GOD);
bool cast_death_channel(int pow, god_type god = GOD_NO_GOD);

void you_teleport();
void you_teleport_now( bool allow_control, bool new_abyss_area = false );


#endif
