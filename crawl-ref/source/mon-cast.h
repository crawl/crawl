/**
 * @file
 * @brief Monster spell casting.
**/

#ifndef MONCAST_H
#define MONCAST_H

#include "enum.h"

class monster;
struct bolt;

void init_mons_spells();
bool is_valid_mon_spell(spell_type spell);

bool handle_mon_spell(monster* mons, bolt &beem);

bolt mons_spells(monster* mons, spell_type spell_cast, int power,
                 bool check_validity = false);
void mons_cast(monster* mons, bolt &pbolt, spell_type spell_cast,
               bool do_noise = true, bool special_ability = false);
void mons_cast_noise(monster* mons, const bolt &pbolt,
                     spell_type spell_cast, bool special_ability = false);
bool setup_mons_cast(monster* mons, bolt &pbolt, spell_type spell_cast,
                     bool check_validity = false);

void mons_cast_haunt(monster* mons);
void mons_cast_mislead(monster* mons);
bool actor_is_illusion_cloneable(actor *target);
void mons_cast_spectral_orcs(monster* mons);

bool ms_direct_nasty(spell_type monspell);
bool ms_useful_fleeing_out_of_sight(const monster* mon, spell_type monspell);
bool ms_quick_get_away(const monster* mon, spell_type monspell);
bool ms_waste_of_time(const monster* mon, spell_type monspell);
bool ms_low_hitpoint_cast(const monster* mon, spell_type monspell);
#endif
