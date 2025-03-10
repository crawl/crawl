#pragma once

#include "cleansing-flame-source-type.h"
#include "enchant-type.h"
#include "holy-word-source-type.h"
#include "random.h"
#include "spell-type.h"
#include "spl-cast.h"
#include "tag-version.h"
#include "torment-source-type.h"

class actor;

spret cast_healing(int pow, bool fail);
bool heal_monster(monster& patient, int amount);

/// List of monster enchantments which can be dispelled.
const enchant_type dispellable_enchantments[] =
{
    ENCH_SLOW,
    ENCH_HASTE,
    ENCH_SWIFT,
    ENCH_MIGHT,
    ENCH_AGILE,
    ENCH_FEAR,
    ENCH_CONFUSION,
    ENCH_CORONA,
    ENCH_STICKY_FLAME,
    ENCH_CHARM,
    ENCH_PARALYSIS,
    ENCH_SICK,
    ENCH_PETRIFYING,
    ENCH_PETRIFIED,
    ENCH_REGENERATION,
    ENCH_STRONG_WILLED,
    ENCH_LOWERED_WL,
    ENCH_SILENCE,
    ENCH_TP,
    ENCH_LIQUEFYING,
    ENCH_INNER_FLAME,
    ENCH_WORD_OF_RECALL,
    ENCH_INJURY_BOND,
    ENCH_FLAYED,
    ENCH_WEAK,
    ENCH_DIMENSION_ANCHOR,
    ENCH_TOXIC_RADIANCE,
    ENCH_FIRE_VULN,
    ENCH_POISON_VULN,
    ENCH_AGILE,
    ENCH_FROZEN,
    ENCH_SIGN_OF_RUIN,
    ENCH_SAP_MAGIC,
    ENCH_CORROSION,
    ENCH_REPEL_MISSILES,
    ENCH_RESISTANCE,
    ENCH_HEXED,
    ENCH_EMPOWERED_SPELLS,
    ENCH_BOUND_SOUL,
    ENCH_INFESTATION,
    ENCH_STILL_WINDS,
    ENCH_CONCENTRATE_VENOM,
    ENCH_MIRROR_DAMAGE,
    ENCH_BLIND,
    ENCH_FRENZIED,
    ENCH_DAZED,
    ENCH_ANTIMAGIC,
    ENCH_ANGUISH,
    ENCH_CONTAM,
    ENCH_BOUND,
    ENCH_BULLSEYE_TARGET,
    ENCH_ARMED,
    ENCH_VITRIFIED,
    ENCH_CURSE_OF_AGONY,
    ENCH_RIMEBLIGHT,
    ENCH_MAGNETISED,
    ENCH_BLINKITIS,
};

bool player_is_debuffable();
bool player_is_cancellable();
string describe_player_cancellation(bool debuffs_only = false);
void debuff_player(bool ignore_resistance = false);
bool monster_is_debuffable(const monster &mon);
bool monster_can_be_unravelled(const monster &mon);
void debuff_monster(monster &mon);

int detect_items(int pow);
int detect_creatures(int pow, bool telepathic = false);

spret cast_tomb(int pow, actor* victim, int source, bool fail);

dice_def beogh_smiting_dice(int pow, bool allow_random = true);
spret cast_smiting(int pow, monster* mons, bool fail);

string unpacifiable_reason(const monster& mon);
string unpacifiable_reason(const monster_info& mi);

struct bolt;

void holy_word(int pow, holy_word_source_type source, const coord_def& where,
               bool silent = false, actor *attacker = nullptr);

void holy_word_monsters(coord_def where, int pow, holy_word_source_type source,
                        actor *attacker = nullptr);
void holy_word_player(holy_word_source_type source);

void torment(actor *attacker, torment_source_type taux, const coord_def& where);
int torment_cell(coord_def where, actor *attacker, torment_source_type taux);
int torment_player(const actor *attacker, torment_source_type taux);

void setup_cleansing_flame_beam(bolt &beam, int pow,
                                cleansing_flame_source caster,
                                coord_def where, actor *attacker = nullptr);
void cleansing_flame(int pow, cleansing_flame_source caster, coord_def where,
                     actor *attacker = nullptr);

spret cast_random_effects(int pow, bolt& beam, bool fail);
void majin_bo_vampirism(monster &mon, int damage);
void dreamshard_shatter();
