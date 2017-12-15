/**
 * @file
 * @brief Monster spell casting.
**/

#pragma once

#include "enum.h"

class monster;
struct bolt;
struct dice_def;

void init_mons_spells();
bool is_valid_mon_spell(spell_type spell);

void aura_of_brilliance(monster* agent);

bool mons_should_cloud_cone(monster* agent, int power, const coord_def pos);
bool scattershot_tracer(monster *caster, int pow, coord_def aim);

dice_def waterstrike_damage(const monster &caster);
dice_def resonance_strike_base_damage(const monster &caster);

void flay(const monster &caster, actor &defender, int damage);

bool handle_mon_spell(monster* mons);

static const int ENCH_POW_FACTOR = 3;
int mons_power_for_hd(spell_type spell, int hd, bool random = true);
int mons_spell_range(spell_type spell, int hd);
bolt mons_spell_beam(const monster* mons, spell_type spell_cast, int power,
                     bool check_validity = false);
void mons_cast(monster* mons, bolt pbolt, spell_type spell_cast,
               mon_spell_slot_flags slot_flags, bool do_noise = true);
void mons_cast_noise(monster* mons, const bolt &pbolt,
                     spell_type spell_cast, mon_spell_slot_flags slot_flags);
bool setup_mons_cast(const monster* mons, bolt &pbolt, spell_type spell_cast,
                     bool check_validity = false);

void mons_cast_haunt(monster* mons);
bool mons_word_of_recall(monster* mons, int recall_target);
void mons_cast_spectral_orcs(monster* mons);
void setup_breath_timeout(monster* mons);

monster* cast_phantom_mirror(monster* mons, monster* targ,
                             int hp_perc = 35,
                             int summ_type = SPELL_PHANTOM_MIRROR);
