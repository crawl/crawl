/**
 * @file
 * @brief Monster death functionality.
**/

#pragma once

#include "item-def.h"
#include "killer-type.h"
#include "tag-version.h"

#define ORIG_MONSTER_KEY "orig_monster_key"
#define ELVEN_ENERGIZE_KEY "elven_twin_energize"
#define ELVEN_IS_ENERGIZED_KEY "elven_twin_is_energized"
#if TAG_MAJOR_VERSION == 34
#define OLD_DUVESSA_ENERGIZE_KEY "duvessa_berserk"
#define OLD_DOWAN_ENERGIZE_KEY "dowan_upgrade"
#endif
#define BLORKULA_REVIVAL_TIMER_KEY "blorkula_revival_timer"
#define BLORKULA_NEXT_BAT_TIME "blorkula_next_bat_time"
#define SAVED_BLORKULA_KEY "original_blorkula"
#define BLORKULA_DIE_FOR_REAL_KEY "blorkula_die_for_real"

class actor;
class monster;

#define MONSTER_DIES_LUA_KEY "monster_dies_lua_key"

// Mid of the monster who left this corpse (used to identify apostle corpses)
#define CORPSE_MID_KEY "corpse_mid"

#define YOU_KILL(x) ((x) == KILL_YOU || (x) == KILL_YOU_MISSILE \
                     || (x) == KILL_YOU_CONF)
#define MON_KILL(x) ((x) == KILL_MON || (x) == KILL_MON_MISSILE)

#define SAME_ATTITUDE(x) ((x)->friendly()       ? BEH_FRIENDLY :   \
                          (x)->good_neutral()   ? BEH_GOOD_NEUTRAL : \
                          (x)->neutral()        ? BEH_NEUTRAL           \
                                                : BEH_HOSTILE)

struct bolt;

item_def* monster_die(monster& mons, const actor *killer, bool silent = false,
                      bool wizard = false, bool fake = false);

item_def* monster_die(monster& mons, killer_type killer,
                      int killer_index, bool silent = false,
                      bool wizard = false, bool fake = false);

item_def* mounted_kill(monster* daddy, monster_type mc, killer_type killer,
                       int killer_index);

bool mons_will_goldify(const monster &mons);

void handle_monster_dies_lua(monster& mons, killer_type killer);

item_def* place_monster_corpse(const monster& mons, bool force = false);
void maybe_drop_monster_organ(monster_type mon, monster_type orig,
                              coord_def pos, bool silent = false);

void monster_cleanup(monster* mons);
void record_monster_defeat(const monster* mons, killer_type killer);
void unawaken_vines(const monster* mons, bool quiet);
int mummy_curse_power(monster_type type);
void fire_monster_death_event(monster* mons, killer_type killer, bool polymorph);
void heal_flayed_effect(actor* act, bool quiet = false, bool blood_only = false);
void end_flayed_effect(monster* ghost);


bool damage_contributes_xp(const actor& agent);

void mons_check_pool(monster* mons, const coord_def &oldpos,
                     killer_type killer = KILL_NONE, int killnum = -1);

int dismiss_monsters(string pattern);

string summoned_poof_msg(const monster* mons, bool plural = false);
string summoned_poof_msg(const monster* mons, const item_def &item);

bool mons_is_mons_class(const monster* mons, monster_type type);
void pikel_band_neutralise();

bool mons_is_elven_twin(const monster* mons);
monster* mons_find_elven_twin_of(const monster* mons);
void elven_twin_died(monster* twin, bool in_transit, killer_type killer, int killer_index);
void elven_twin_energize(monster* mons);
void elven_twins_pacify(monster* twin);
void elven_twins_unpacify(monster* twin);

void hogs_to_humans();

bool mons_felid_can_revive(const monster* mons);
void mons_felid_revive(monster* mons);

bool mons_bennu_can_revive(const monster* mons);

void blorkula_bat_merge(monster& bat);
