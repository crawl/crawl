#ifndef SPL_GODITEM_H
#define SPL_GODITEM_H

#include "spl-cast.h"

spret_type cast_healing(int pow, int max_pow, bool divine_ability = false,
                        const coord_def& where = coord_def(0, 0),
                        bool not_self = false,
                        targ_mode_type mode = TARG_NUM_MODES);

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
    ENCH_SILVER_CORONA,
    ENCH_CHARM,
    ENCH_PARALYSIS,
    ENCH_PETRIFYING,
    ENCH_PETRIFIED,
    ENCH_REGENERATION,
    ENCH_TP,
    ENCH_INNER_FLAME,
    ENCH_OZOCUBUS_ARMOUR,
    ENCH_INJURY_BOND,
    ENCH_DIMENSION_ANCHOR,
    ENCH_CONTROL_WINDS,
    ENCH_TOXIC_RADIANCE,
    ENCH_AGILE,
    ENCH_BLACK_MARK,
    ENCH_SHROUD,
    ENCH_SAP_MAGIC,
    ENCH_REPEL_MISSILES,
    ENCH_DEFLECT_MISSILES,
    ENCH_CONDENSATION_SHIELD,
    ENCH_RESISTANCE,
    ENCH_HEXED,
};

bool player_is_debuffable();
void debuff_player();
bool monster_is_debuffable(const monster &mon);
void debuff_monster(monster &mon);

int detect_traps(int pow);
int detect_items(int pow);
int detect_creatures(int pow, bool telepathic = false);
bool remove_curse(bool alreadyknown = true, const string &pre_msg = "");
bool curse_item(bool armour, const string &pre_msg = "");

bool entomb(int pow);
bool cast_imprison(int pow, monster* mons, int source);

bool cast_smiting(int pow, monster* mons);

int is_pacifiable(const monster* mon);

struct bolt;

void holy_word(int pow, holy_word_source_type source, const coord_def& where,
               bool silent = false, actor *attacker = nullptr);

void holy_word_monsters(coord_def where, int pow, holy_word_source_type source,
                        actor *attacker = nullptr);

void torment(actor *attacker, torment_source_type taux, const coord_def& where);
void torment_cell(coord_def where, actor *attacker, torment_source_type taux);
void torment_player(actor *attacker, torment_source_type taux);

void setup_cleansing_flame_beam(bolt &beam, int pow, int caster,
                                coord_def where, actor *attacker = nullptr);
void cleansing_flame(int pow, int caster, coord_def where,
                     actor *attacker = nullptr);

#endif
