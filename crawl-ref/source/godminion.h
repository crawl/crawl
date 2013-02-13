/**
 * @file
 * @brief Minions used for Demigods "abstract worshippers" mechanic.
**/

#ifndef GODMINION_H
#define GODMINION_H

#include "mgen_enum.h"

#define DEMIGOD_MINION_TIMER_SHORT 8
#define DEMIGOD_MINION_TIMER_LONG 40

// Very loosely-defined backgrounds for the minions
enum demigod_minion_background_type {
    DG_MINION_MAGE,    // Purely casts spells for offense; wears robes or rarely leather / other light armours; sometimes might have staff and/or shield
    DG_MINION_HYBRID,  // Wears fairly light armour and uses light, fast weapons with some spell support
    DG_MINION_FIGHTER, // Mostly fighing based; heavy weapons and armour; might still have some spells (God abilities)
    DG_MINION_ZEALOT,  // ?? - possibly not relevant until we have more god abilities
    DG_MINION_NUM_BACKGROUNDS
};

monster* demigod_build_minion(god_type which_god, int level);
int demigod_weight_race_for_god(god_type god, monster_type mon);
int demigod_weight_spell_for_god(god_type god, monster_type mon, spell_type spell);
// void demigod_fixup_monster_for_god(mgen_data mg, god_type god);
bool demigod_dispatch_minion(god_type which_god, int level);
void demigod_handle_notoriety();
void dec_notoriety(god_type god, int val);
void demigod_minion_timer_expired();
bool demigod_incur_wrath(god_type which_god, int amount);
bool demigod_incur_wrath_all(int amount);
string demigod_random_minion_name(god_type which_god, monster_type chosen_race, int level);

#endif
