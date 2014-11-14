/**
 * @file
 * @brief Monster death functionality.
**/

#ifndef MONDEATH_H
#define MONDEATH_H

#define ORIG_MONSTER_KEY "orig_monster_key"
#define ELVEN_ENERGIZE_KEY "elven_twin_energize"
#define ELVEN_IS_ENERGIZED_KEY "elven_twin_is_energized"
#if TAG_MAJOR_VERSION == 34
#define OLD_DUVESSA_ENERGIZE_KEY "duvessa_berserk"
#define OLD_DOWAN_ENERGIZE_KEY "dowan_upgrade"
#endif

#define MONSTER_DIES_LUA_KEY "monster_dies_lua_key"

#define YOU_KILL(x) ((x) == KILL_YOU || (x) == KILL_YOU_MISSILE \
                     || (x) == KILL_YOU_CONF)
#define MON_KILL(x) ((x) == KILL_MON || (x) == KILL_MON_MISSILE)

#define SAME_ATTITUDE(x) (x->friendly()       ? BEH_FRIENDLY : \
                          x->good_neutral()   ? BEH_GOOD_NEUTRAL : \
                          x->strict_neutral() ? BEH_STRICT_NEUTRAL : \
                          x->neutral()        ? BEH_NEUTRAL \
                                              : BEH_HOSTILE)

struct bolt;

int monster_die(monster* mons, const actor *killer, bool silent = false,
                bool wizard = false, bool fake = false);

int monster_die(monster* mons, killer_type killer,
                int killer_index, bool silent = false, bool wizard = false,
                bool fake = false);

int mounted_kill(monster* daddy, monster_type mc, killer_type killer,
                int killer_index);

monster_type fill_out_corpse(const monster* mons,
                             monster_type mtype,
                             item_def& corpse,
                             bool force_corpse = false);

bool explode_corpse(item_def& corpse, const coord_def& where);

int place_monster_corpse(const monster* mons, bool silent, bool force = false);
void monster_cleanup(monster* mons);
void setup_spore_explosion(bolt & beam, const monster& origin);
void record_monster_defeat(monster* mons, killer_type killer);
void unawaken_vines(const monster* mons, bool quiet);
void fire_monster_death_event(monster* mons, killer_type killer, int i, bool polymorph);
void heal_flayed_effect(actor* act, bool quiet = false, bool blood_only = false);
void end_flayed_effect(monster* ghost);


int exp_rate(int killer);

void mons_check_pool(monster* mons, const coord_def &oldpos,
                     killer_type killer = KILL_NONE, int killnum = -1);

int dismiss_monsters(string pattern);

string summoned_poof_msg(const monster* mons, bool plural = false);
string summoned_poof_msg(const monster* mons, const item_def &item);

bool mons_is_pikel(monster* mons);
void pikel_band_neutralise();

bool mons_is_duvessa(const monster* mons);
bool mons_is_dowan(const monster* mons);
bool mons_is_elven_twin(const monster* mons);
void elven_twin_died(monster* twin, bool in_transit, killer_type killer, int killer_index);
void elven_twin_energize(monster* mons);
void elven_twins_pacify(monster* twin);
void elven_twins_unpacify(monster* twin);

bool mons_is_kirke(monster* mons);
void hogs_to_humans();

bool mons_is_natasha(const monster* mons);
bool mons_felid_can_revive(const monster* mons);
void mons_felid_revive(monster* mons);

bool mons_bennu_can_revive(const monster* mons);
void mons_bennu_revive(monster* mons);
#endif
