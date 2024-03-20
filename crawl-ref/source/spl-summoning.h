#pragma once

#include "beam.h"
#include "beh-type.h"
#include "enum.h"
#include "god-type.h"
#include "item-prop-enum.h"
#include "spl-cast.h"

// How many aut until the next doom hound pops out of doom howl?
#define NEXT_DOOM_HOUND_KEY "next_doom_hound"

#define DRAGON_CALL_POWER_KEY "dragon_call_power"

#define SIMULACRUM_TYPE_KEY "simulacrum_type"

spret cast_summon_small_mammal(int pow, god_type god, bool fail);

bool canine_familiar_is_alive();
monster *find_canine_familiar();
void check_canid_farewell(const monster &dog, bool deadish);
spret cast_call_canine_familiar(int pow, god_type god, bool fail);

spret cast_summon_armour_spirit(int pow, god_type god, bool fail);
spret cast_summon_ice_beast(int pow, god_type god, bool fail);
spret cast_summon_cactus(int pow, god_type god, bool fail);
spret cast_monstrous_menagerie(actor* caster, int pow, god_type god,
                                 bool fail = false);
spret cast_summon_dragon(actor *caster, int pow,
                              god_type god = GOD_NO_GOD, bool fail = false);
spret cast_summon_hydra(actor *caster, int pow, god_type god = GOD_NO_GOD,
                             bool fail = false);
spret cast_summon_mana_viper(int pow, god_type god, bool fail);
bool summon_berserker(int pow, actor *caster,
                      monster_type override_mons = MONS_PROGRAM_BUG);
bool summon_holy_warrior(int pow, bool punish);

bool tukima_affects(const actor &target);
void cast_tukimas_dance(int pow, actor *target);
spret cast_conjure_ball_lightning(int pow, god_type god, bool fail);
int ball_lightning_hd(int pow, bool random = true);
int mons_ball_lightning_hd(int pow, bool random = true);
int mons_ball_lightning_per_cast(int pow, bool random = true);
spret cast_summon_lightning_spire(int pow, god_type god, bool fail);

spret cast_call_imp(int pow, god_type god, bool fail);
bool summon_demon_type(monster_type mon, int pow, god_type god = GOD_NO_GOD,
                       int spell = 0, bool friendly = true);
spret cast_summon_demon(int pow);
spret summon_shadow_creatures();
spret cast_summon_horrible_things(int pow, god_type god, bool fail);
bool can_cast_malign_gateway();
void create_malign_gateway(coord_def point, beh_type beh, string cause,
                           int pow, god_type god = GOD_NO_GOD,
                           bool is_player = false);
spret cast_malign_gateway(actor* caster, int pow,
                          god_type god = GOD_NO_GOD, bool fail = false,
                          bool test = false);
coord_def find_gateway_location(actor* caster);
spret cast_summon_forest(actor* caster, int pow, god_type god, bool fail, bool test=false);
spret cast_summon_blazeheart_golem(int pow, god_type god, bool fail);

spret cast_dragon_call(int pow, bool fail);
void do_dragon_call(int time);

void doom_howl(int time);

spell_type player_servitor_spell();
bool spell_servitorable(spell_type spell);
void init_servitor(monster* servitor, actor* caster, int pow);
spret cast_spellforged_servitor(int pow, god_type god, bool fail);

monster_type pick_random_wraith();
spret cast_haunt(int pow, const coord_def& where, god_type god, bool fail);

spret cast_martyrs_knell(const actor* caster, int pow, god_type god, bool fail);

monster* find_battlesphere(const actor* agent);
spret cast_battlesphere(actor* agent, int pow, god_type god, bool fail);
void end_battlesphere(monster* mons, bool killed);
bool battlesphere_can_mirror(spell_type spell);
vector<spell_type> player_battlesphere_spells();
bool trigger_battlesphere(actor* agent);
bool fire_battlesphere(monster* mons);
void reset_battlesphere(monster* mons);
dice_def battlesphere_damage(int pow);

spret cast_fulminating_prism(actor* caster, int pow,
                                  const coord_def& where, bool fail);
int prism_hd(int pow, bool random = true);

monster* find_spectral_weapon(const actor* agent);
void end_spectral_weapon(monster* mons, bool killed, bool quiet = false);
void check_spectral_weapon(actor &agent);

spret cast_infestation(int pow, bolt &beam, bool fail);

void summoned_monster(const monster* mons, const actor* caster,
                      spell_type spell);
bool summons_are_capped(spell_type spell);
int summons_limit(spell_type spell, bool player);
int count_summons(const actor *summoner, spell_type spell);

vector<coord_def> find_briar_spaces(bool just_check = false);
void fedhas_wall_of_briars();
spret fedhas_grow_ballistomycete(const coord_def& target, bool fail);
spret fedhas_overgrow(bool fail);
spret fedhas_grow_oklob(const coord_def& target, bool fail);

void kiku_unearth_wretches();

spret cast_foxfire(actor &agent, int pow, god_type god, bool fail,
                   bool marshlight = false);
spret foxfire_swarm();
bool summon_spider(const actor &agent, coord_def pos, god_type god,
                        spell_type spell, int pow);
spret summon_spiders(actor &agent, int pow, god_type god, bool fail = false);

spret summon_butterflies();

spret cast_broms_barrelling_boulder(actor& agent, coord_def pos, int pow, bool fail);

string mons_simulacrum_immune_reason(const monster *mons);
spret cast_simulacrum(coord_def target, int pow, bool fail);
