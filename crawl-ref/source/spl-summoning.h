#ifndef SPL_SUMMONING
#define SPL_SUMMONING

#include "enum.h"
#include "itemprop-enum.h"

//Bitfield for animate dead messages
#define DEAD_ARE_WALKING 1
#define DEAD_ARE_SWIMMING 2
#define DEAD_ARE_FLYING 4
#define DEAD_ARE_SLITHERING 8
#define DEAD_ARE_HOPPING 16
#define DEAD_ARE_FLOATING 32

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
bool can_cast_malign_gateway();
bool cast_malign_gateway(actor * caster, int pow, god_type god = GOD_NO_GOD);

void equip_undead(const coord_def &a, int corps, int monster, int monnum);
int animate_remains(const coord_def &a, corpse_type class_allowed,
                    beh_type beha, unsigned short hitting,
                    actor *as = NULL, std::string nas = "",
                    god_type god = GOD_NO_GOD, bool actual = true,
                    bool quiet = false, bool force_beh = false,
                    int* mon_index = NULL, int* motions = NULL);

int animate_dead(actor *caster, int pow, beh_type beha, unsigned short hitting,
                 actor *as = NULL, std::string nas = "",
                 god_type god = GOD_NO_GOD, bool actual = true);

bool cast_simulacrum(int pow, god_type god = GOD_NO_GOD);
bool cast_twisted_resurrection(int pow, god_type god = GOD_NO_GOD);
bool cast_haunt(int pow, const coord_def& where, god_type god = GOD_NO_GOD);

void abjuration(int pow);

#endif
