/**
 * @file
 * @brief Monster spell casting.
**/

#pragma once

#include "enum.h"
#include "externs.h"
#include "mon-ai-action.h"
#include "spell-type.h"

class actor;
class monster;
struct bolt;
struct dice_def;

#define OLD_ARMS_KEY "old_arms"
#define OLD_ARMS_ALT_KEY "old_arms_alt"

void init_mons_spells();
bool is_valid_mon_spell(spell_type spell);

void prayer_of_brilliance(monster* agent);

bool mons_should_cloud_cone(monster* agent, int power, const coord_def pos);

dice_def waterstrike_damage(int spell_hd);
dice_def resonance_strike_base_damage(int spell_hd);

dice_def eruption_damage();

bool handle_mon_spell(monster* mons);

static const int ENCH_POW_FACTOR = 3;
bool mons_spell_is_spell(spell_type spell);
int mons_power_for_hd(spell_type spell, int hd);
int mons_spellpower(const monster &mons, spell_type spell);
int mons_spell_range(const monster &mons, spell_type spell);
int mons_spell_range_for_hd(spell_type spell, int hd, bool use_veh_bonus = false);
bolt mons_spell_beam(const monster* mons, spell_type spell_cast, int power,
                     bool check_validity = false);
void mons_cast(monster* mons, bolt pbolt, spell_type spell_cast,
               mon_spell_slot_flags slot_flags, bool do_noise = true);
bool is_mons_cast_possible(monster& mons, spell_type spell);
bool try_mons_cast(monster& mons, spell_type spell);
void mons_cast_noise(monster* mons, const bolt &pbolt,
                     spell_type spell_cast, mon_spell_slot_flags slot_flags);
bool setup_mons_cast(const monster* mons, bolt &pbolt, spell_type spell_cast,
                     bool evoke = false,
                     bool check_validity = false);

void mons_cast_flay(monster &caster, mon_spell_slot, bolt&);
bool mons_word_of_recall(monster* mons, int recall_target);
void setup_breath_timeout(monster* mons);
bool mons_can_bind_soul(monster* binder, monster* bound);

bool spell_has_marionette_override(spell_type spell);

int living_spell_count(spell_type spell, bool random);
spell_type living_spell_type_for(monster_type mtyp);

monster* cast_phantom_mirror(monster* mons, monster* targ,
                             int hp_perc = 35,
                             int summ_type = SPELL_PHANTOM_MIRROR,
                             int clone_index = 0);

ai_action::goodness monster_spell_goodness(monster* mon, spell_type spell);
