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

spret_type cast_summon_butterflies(int pow, god_type god = GOD_NO_GOD,
                                   bool fail = false);
spret_type cast_summon_small_mammals(int pow, god_type god, bool fail);

bool item_is_snakable(const item_def& item);
spret_type cast_sticks_to_snakes(int pow, god_type god, bool fail);

spret_type cast_summon_scorpions(int pow, god_type god, bool fail);
spret_type cast_summon_swarm(int pow, god_type god, bool fail);
spret_type cast_call_canine_familiar(int pow, god_type god, bool fail);
spret_type cast_summon_elemental(int pow, god_type god = GOD_NO_GOD,
                                 monster_type restricted_type = MONS_NO_MONSTER,
                                 int unfriendly = 2, int horde_penalty = 0,
                                 bool fail = false);
spret_type cast_summon_ice_beast(int pow, god_type god, bool fail);
spret_type cast_summon_ugly_thing(int pow, god_type god, bool fail);
spret_type cast_summon_dragon(actor *caster, int pow,
                              god_type god = GOD_NO_GOD, bool fail = false);
spret_type cast_summon_hydra(actor *caster, int pow, god_type god = GOD_NO_GOD,
                             bool fail = false);
bool summon_berserker(int pow, actor *caster,
                      monster_type override_mons = MONS_PROGRAM_BUG);
bool summon_holy_warrior(int pow, god_type god = GOD_NO_GOD, int spell = 0,
                         bool force_hostile = false, bool permanent = false,
                         bool quiet = false);

spret_type cast_tukimas_dance(int pow, god_type god = GOD_NO_GOD,
                              bool force_hostile = false, bool fail = false);
spret_type cast_conjure_ball_lightning(int pow, god_type god, bool fail);

spret_type cast_call_imp(int pow, god_type god, bool fail);
bool summon_demon_type(monster_type mon, int pow, god_type god = GOD_NO_GOD,
                       int spell = 0);
spret_type cast_summon_demon(int pow, god_type god = GOD_NO_GOD,
                             bool fail = false);
spret_type cast_demonic_horde(int pow, god_type god, bool fail);
spret_type cast_summon_greater_demon(int pow, god_type god, bool fail);
spret_type cast_shadow_creatures(god_type god, bool fail);
spret_type cast_summon_horrible_things(int pow, god_type god, bool fail);
bool can_cast_malign_gateway();
spret_type cast_malign_gateway(actor* caster, int pow,
                               god_type god = GOD_NO_GOD, bool fail = false);
coord_def find_gateway_location(actor* caster);

int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    actor *as = NULL, string nas = "",
                    god_type god = GOD_NO_GOD, bool actual = true,
                    bool quiet = false, bool force_beh = false,
                    monster** mon = NULL, int* motions = NULL);

spret_type cast_animate_skeleton(god_type god, bool fail);
spret_type cast_animate_dead(int pow, god_type god, bool fail);
int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 actor *as = NULL, string nas = "",
                 god_type god = GOD_NO_GOD, bool actual = true);

spret_type cast_simulacrum(int pow, god_type god, bool fail);
bool monster_simulacrum(monster *caster, bool actual);

spret_type cast_twisted_resurrection(int pow, god_type god, bool fail);
bool twisted_resurrection(actor *caster, int pow, beh_type beha,
                          unsigned short foe, god_type god, bool actual = true);

spret_type cast_haunt(int pow, const coord_def& where, god_type god, bool fail);

spret_type cast_abjuration(int pow, const coord_def& where, bool fail = false);
spret_type cast_mass_abjuration(int pow, bool fail = false);

spret_type cast_arcane_familiar(int pow, god_type god, bool fail);
void end_arcane_familiar(bool killed);
bool aim_arcane_familiar(spell_type spell, int powc, bolt& beam);
bool trigger_arcane_familiar(bolt& beam);
bool fire_arcane_familiar(monster* mons);
void reset_arcane_familiar(monster* mons);

spret_type cast_fulminating_prism(int pow, const coord_def& where, bool fail);
#endif
