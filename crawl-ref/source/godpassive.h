/**
 * @file
 * @brief Passive god effects.
**/

#ifndef GODPASSIVE_H
#define GODPASSIVE_H

#include "enum.h"
#include "player.h"

class monster;
class dist;

// Passive god abilities.
enum class passive_t
{
    /// Placeholder for absence of a passive ability.
    none = 0,

    /// The god prefers that items be cursed: acquirement grants cursed items,
    /// enchant scrolls and miscasts preserve curse status, and remove curse
    /// allows selecting a subset of items.
    want_curses,

    /// You detect the presence of portals.
    detect_portals,

    /// You identity items on sight, including monster equipment.
    identify_items,

    /// You have (improved) automatic mapping.
    auto_map,

    /// You detect the threat level of monsters.
    detect_montier,

    /// You detect the presence of items.
    detect_items,

    /// You are better at searching for traps.
    search_traps,

    /// You have innate see invisible.
    sinv,

    /// You have innate clarity.
    clarity,

    /// You get a boost to skills from cursed slots.
    bondage_skill_boost,

    /// You convert orcs into followers.
    convert_orcs,

    /// You can walk on water.
    water_walk,

    /// You share experience with followers.
    share_exp,

    /// Your god blesses your followers.
    bless_followers,

    /// Your god blesses your followers when they kill evil or unholy things.
    bless_followers_vs_unholy,

    /// You receive a bonus to armour class.
    bonus_ac,

    /// You cannot be hasted.
    no_haste,

    /// You move slowly.
    slowed,

    /// Fewer creatures spawn on the orb run.
    slow_orb_run,

    /// Fewer creatures spawn in the Abyss, and it morphs less quickly.
    slow_abyss,

    /// Your attributes are boosted.
    stat_boost,

    /// Hunger, poison, and disease affect you more slowly.
    slow_metabolism,

    /// You have an umbra.
    umbra,

    /// You emit clouds when hit.
    hit_smoke,

    /// Your shadow attacks alongside you.
    shadow_attacks,

    /// Your shadow casts attack spells alongside you.
    shadow_spells,

    /// No accuracy penalty in umbra. Gain stealth in umbra.
    nightvision,

    /// No distortion unwield effects.
    safe_distortion,

    /// Less map rot in the abyss.
    map_rot_res_abyss,

    /// Higher chance for spawning the abyssal rune.
    attract_abyssal_rune,

    /// Potentially save allies from death. Holy or natural, in LOS.
    protect_ally,

    /// Chance to nullify a deadly blow, dependant on piety.
    protect_from_harm,

    /// Divine halo around the player, size increases with piety.
    halo,

    /// Protect allies from abjuration by decreasing the power to half.
    abjuration_protection,

    /// Protect allies from abjuration by decreasing the power. Higher HD means
    /// better protection.
    abjuration_protection_hd,

    /// Gain HP when killing monsters.
    restore_hp,

    /// Gain HP and MP when killing evil or unholy monsters.
    restore_hp_mp_vs_unholy,

    /// Mutation resistance increasing with piety.
    resist_mutation,

    /// Polymorph resistance increasing with piety.
    resist_polymorph,

    /// Chance of avoiding hell effects, increasing with piety.
    resist_hell_effects,

    /// Warning about shapeshifters when they come to view.
    warn_shapeshifter,

    /// Torment resistance, piety dependant.
    resist_torment,

    /// You are left with more HP when using Death's Door, piety dependant.
    deaths_door_hp_boost,

    /// Protection against miscasts. Piety dependant.
    miscast_protection,

    /// Protection against necromancy miscasts and mummy death curses.
    miscast_protection_necromancy,

    /// Chance to extend berserk duration and avoid paralysis, piety dependant.
    extend_berserk,

    /// Gold aura that distracts enemies.
    gold_aura,

    /// Corpses turn to gold.
    goldify_corpses,

    /// You detect the presence of gold. Gold is moved on top in stacks.
    detect_gold,
};

enum jiyva_slurp_results
{
    JS_NONE = 0,
    JS_FOOD = 1,
    JS_HP   = 2,
    JS_MP   = 4,
};

enum ru_interference
{
    DO_NOTHING,
    DO_BLOCK_ATTACK,
    DO_REDIRECT_ATTACK
};

bool have_passive(passive_t passive);
bool will_have_passive(passive_t passive);
int rank_for_passive(passive_t passive);
int chei_stat_boost(int piety = you.piety);
void jiyva_eat_offlevel_items();
void jiyva_slurp_bonus(int item_value, int *js);
void jiyva_slurp_message(int js);
void ash_init_bondage(player *y);
void ash_check_bondage(bool msg = true);
string ash_describe_bondage(int flags, bool level);
bool god_id_item(item_def& item, bool silent = true);
void ash_id_monster_equipment(monster* mon);
int ash_detect_portals(bool all);
monster_type ash_monster_tier(const monster *mon);
int ash_skill_boost(skill_type sk, int scale);
map<skill_type, int8_t> ash_get_boosted_skills(eq_type type);
int gozag_gold_in_los(actor* whom);
int qazlal_sh_boost(int piety = you.piety);
int tso_sh_boost();
void qazlal_storm_clouds();
void qazlal_element_adapt(beam_type flavour, int strength);
bool does_ru_wanna_redirect(monster* mon);
ru_interference get_ru_attack_interference_level();
void pakellas_id_device_charges();
monster* shadow_monster(bool equip = true);
void shadow_monster_reset(monster *mon);
void dithmenos_shadow_melee(actor* target);
void dithmenos_shadow_throw(const dist &d, const item_def &item);
void dithmenos_shadow_spell(bolt* orig_beam, spell_type spell);
#endif
