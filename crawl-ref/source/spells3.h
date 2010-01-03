/*
 *  File:       spells3.h
 *  Summary:    Implementations of some additional spells.
 *  Written by: Linley Henzell
 */


#ifndef SPELLS3_H
#define SPELLS3_H

#include "itemprop-enum.h"

class dist;
struct bolt;

bool allow_control_teleport(bool quiet = false);
int airstrike(int power, dist &beam);
bool cast_bone_shards(int power, bolt &);

bool cast_selective_amnesia(bool force);
bool cast_smiting(int power, const coord_def& where);
bool cast_sanctuary(const int power);
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

bool receive_corpses(int pow, coord_def where);

void equip_undead(const coord_def &a, int corps, int monster, int monnum);
int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    actor *as = NULL, std::string nas = "",
                    god_type god = GOD_NO_GOD, bool actual = true,
                    bool quiet = false, bool force_beh = false,
                    int* mon_index = NULL);

int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 actor *as = NULL, std::string nas = "",
                 god_type god = GOD_NO_GOD, bool actual = true);

bool cast_simulacrum(int pow, god_type god = GOD_NO_GOD);
bool cast_twisted_resurrection(int pow, god_type god = GOD_NO_GOD);
bool cast_haunt(int pow, const coord_def& where, god_type god = GOD_NO_GOD);
bool cast_death_channel(int pow, god_type god = GOD_NO_GOD);

void you_teleport();
void you_teleport_now(bool allow_control,
                      bool new_abyss_area = false,
                      bool wizard_tele = false);
bool you_teleport_to(const coord_def where,
                     bool move_monsters = false);
#endif
