#ifndef SPL_SUMMONING_H
#define SPL_SUMMONING_H

#include "beam.h"
#include "enum.h"
#include "itemprop-enum.h"
#include "spl-cast.h"

//Bitfield for animate dead messages
#define DEAD_ARE_WALKING 1
#define DEAD_ARE_SWIMMING 2
#define DEAD_ARE_FLYING 4
#define DEAD_ARE_SLITHERING 8
#define DEAD_ARE_HOPPING 16
#define DEAD_ARE_CRAWLING 32

// Properties set for active summons
#define SW_TARGET_MID "sw_target_mid"
#define SW_READIED "sw_readied"
#define SW_TRACKING "sw_tracking"

// How many aut until the next doom hound pops out of doom howl?
#define NEXT_DOOM_HOUND_KEY "next_doom_hound"

spret_type cast_summon_butterflies(int pow, god_type god = GOD_NO_GOD,
                                   bool fail = false);
spret_type cast_summon_small_mammal(int pow, god_type god, bool fail);

spret_type cast_sticks_to_snakes(int pow, god_type god, bool fail);

spret_type cast_call_canine_familiar(int pow, god_type god, bool fail);
spret_type cast_summon_ice_beast(int pow, god_type god, bool fail);
spret_type cast_monstrous_menagerie(actor* caster, int pow, god_type god,
                                 bool fail = false);
spret_type cast_summon_dragon(actor *caster, int pow,
                              god_type god = GOD_NO_GOD, bool fail = false);
spret_type cast_summon_hydra(actor *caster, int pow, god_type god = GOD_NO_GOD,
                             bool fail = false);
spret_type cast_summon_mana_viper(int pow, god_type god, bool fail);
bool summon_berserker(int pow, actor *caster,
                      monster_type override_mons = MONS_PROGRAM_BUG);
bool summon_holy_warrior(int pow, bool punish);
bool summon_vines(int pow);

bool tukima_affects(const actor &target);
void cast_tukimas_dance(int pow, actor *target);
spret_type cast_conjure_ball_lightning(int pow, god_type god, bool fail);
spret_type cast_summon_lightning_spire(int pow, const coord_def& where, god_type god, bool fail);

spret_type cast_call_imp(int pow, god_type god, bool fail);
bool summon_demon_type(monster_type mon, int pow, god_type god = GOD_NO_GOD,
                       int spell = 0, bool friendly = true);
spret_type cast_summon_demon(int pow, god_type god = GOD_NO_GOD,
                             bool fail = false);
spret_type cast_summon_greater_demon(int pow, god_type god, bool fail);
spret_type cast_shadow_creatures(int st = SPELL_SHADOW_CREATURES,
                                 god_type god = GOD_NO_GOD,
                                 level_id place = level_id::current(),
                                 bool fail = false);
spret_type cast_summon_horrible_things(int pow, god_type god, bool fail);
bool can_cast_malign_gateway();
spret_type cast_malign_gateway(actor* caster, int pow,
                               god_type god = GOD_NO_GOD, bool fail = false);
coord_def find_gateway_location(actor* caster);
spret_type cast_summon_forest(actor* caster, int pow, god_type god, bool fail);
spret_type cast_summon_guardian_golem(int pow, god_type god, bool fail);

spret_type cast_dragon_call(int pow, bool fail);
void do_dragon_call(int time);

void doom_howl(int time);

void init_servitor(monster* servitor, actor* caster);
spret_type cast_spellforged_servitor(int pow, god_type god, bool fail);

int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    actor *as = nullptr, string nas = "",
                    god_type god = GOD_NO_GOD, bool actual = true,
                    bool quiet = false, bool force_beh = false,
                    monster** mon = nullptr, int* motions = nullptr);

spret_type cast_animate_skeleton(god_type god, bool fail);
spret_type cast_animate_dead(int pow, god_type god, bool fail);
int animate_dead(actor *caster, int /*pow*/, beh_type beha,
                 unsigned short hitting, actor *as = nullptr, string nas = "",
                 god_type god = GOD_NO_GOD, bool actual = true);

spret_type cast_simulacrum(int pow, god_type god, bool fail);
bool monster_simulacrum(monster *caster, bool actual);

bool twisted_resurrection(actor *caster, int pow, beh_type beha,
                          unsigned short foe, god_type god, bool actual = true);

monster_type pick_random_wraith();
spret_type cast_haunt(int pow, const coord_def& where, god_type god, bool fail);

spret_type cast_aura_of_abjuration(int pow, bool fail = false);
void do_aura_of_abjuration(int delay);

monster* find_battlesphere(const actor* agent);
spret_type cast_battlesphere(actor* agent, int pow, god_type god, bool fail);
void end_battlesphere(monster* mons, bool killed);
bool battlesphere_can_mirror(spell_type spell);
bool aim_battlesphere(actor* agent, spell_type spell, int powc, bolt& beam);
bool trigger_battlesphere(actor* agent, bolt& beam);
bool fire_battlesphere(monster* mons);
void reset_battlesphere(monster* mons);

spret_type cast_fulminating_prism(actor* caster, int pow,
                                  const coord_def& where, bool fail);

monster* find_spectral_weapon(const actor* agent);
bool weapon_can_be_spectral(const item_def *weapon);
spret_type cast_spectral_weapon(actor *agent, int pow, god_type god, bool fail);
void end_spectral_weapon(monster* mons, bool killed, bool quiet = false);
bool trigger_spectral_weapon(actor* agent, const actor* target);
bool confirm_attack_spectral_weapon(monster* mons, const actor *defender);
void reset_spectral_weapon(monster* mons);

spret_type cast_infestation(int pow, bolt &beam, bool fail);

void summoned_monster(const monster* mons, const actor* caster,
                      spell_type spell);
bool summons_are_capped(spell_type spell);
int summons_limit(spell_type spell);
int count_summons(const actor *summoner, spell_type spell);

#endif
