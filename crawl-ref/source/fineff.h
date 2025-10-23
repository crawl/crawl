/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#pragma once

#include <string>

#include "beh-type.h"
#include "daction-type.h"
#include "god-type.h"
#include "killer-type.h"
#include "monster-type.h"
#include "spell-type.h"

class actor;
struct bolt;
struct coord_def;
struct item_def;
struct mgen_data;
class mon_enchant;
class monster;

enum explosion_fineff_type : int {
    EXPLOSION_FINEFF_GENERIC,
    EXPLOSION_FINEFF_INNER_FLAME,
    EXPLOSION_FINEFF_CONCUSSION,
    EXPLOSION_FINEFF_PYROMANIA,
};

void schedule_mirror_damage_fineff(const actor* attack, const actor* defend,
                                   int dam);
void schedule_anguish_fineff(const actor* attack, int dam);
void schedule_ru_retribution_fineff(const actor* attack, const actor* defend,
                                    int dam);
void schedule_trample_follow_fineff(const actor* attack, const coord_def& pos);
void schedule_blink_fineff(const actor* blinker, const actor* other = nullptr);
void schedule_teleport_fineff(const actor* defend);
void schedule_trj_spawn_fineff(const actor* attack, const actor* defend,
                               const coord_def& pos, int dam);
void schedule_blood_fineff(const actor* defend, const coord_def& pos,
                           int blood_amount);
void schedule_deferred_damage_fineff(const actor* attack, const actor* defend,
                                     int dam, bool attacker_effects,
                                     bool fatal = true);
void schedule_starcursed_merge_fineff(const actor* merger);
void schedule_shock_discharge_fineff(const actor* discharger, actor& oppressor,
                                     coord_def pos, int pow,
                                     string shock_source);
void schedule_explosion_fineff(bolt& beam, string boom, string sanct,
                               explosion_fineff_type typ,
                               const actor* flame_agent,
                               string poof);
void schedule_splinterfrost_fragment_fineff(bolt& beam, string msg);
void schedule_delayed_action_fineff(daction_type action,
                                    const string& final_msg);
void schedule_kirke_death_fineff(const string& final_msg);
void schedule_rakshasa_clone_fineff(const actor* defend, const coord_def& pos);
void schedule_bennu_revive_fineff(coord_def pos, int revives,
                                  beh_type attitude, unsigned short foe,
                                  bool duel, mon_enchant gozag_bribe);
void schedule_avoided_death_fineff(monster* mons);
void schedule_infestation_death_fineff(coord_def pos, const string& name);
void schedule_make_derived_undead_fineff(coord_def pos, mgen_data mg, int xl,
                                         const string& agent,
                                         const string& msg,
                                         bool act_immediately = false);
void schedule_mummy_death_curse_fineff(const actor* attack,
                                       const monster* dead_mummy,
                                       killer_type killer, int pow);
void schedule_summon_dismissal_fineff(const actor* _defender);
void schedule_spectral_weapon_fineff(const actor& attack, const actor& defend,
                                     item_def* weapon);
void schedule_lugonu_meddle_fineff();
void schedule_jinxbite_fineff(const actor* defend);
void schedule_beogh_resurrection_fineff(bool end_ostracism_only = false);
void schedule_dismiss_divine_allies_fineff(const god_type god);
void schedule_death_spawn_fineff(monster_type mon_type, coord_def pos, int dur,
                                 int summon_type = SPELL_NO_SPELL);
void schedule_death_spawn_fineff(mgen_data mg);
void schedule_detonation_fineff(const coord_def& pos, const item_def* wpn);
void schedule_stardust_fineff(actor* agent, int power, int max_stars);
void schedule_pyromania_fineff();
void schedule_celebrant_bloodrite_fineff();


void fire_final_effects();
void clear_final_effects();
