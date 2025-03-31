/**
 * @file
 * @brief Passive god effects.
**/

#pragma once

#include "enum.h"
#include "wu-jian-attack-type.h"
#include "player.h"

class monster;
class dist;

// Passive god abilities.
enum class passive_t
{
    /// Placeholder for absence of a passive ability.
    none = 0,

    /// The god prefers that items be cursed: acquirement grants cursed items,
    /// enchant scrolls and miscasts preserve curse status, and identify
    /// allows selecting a subset of items to uncurse.
    want_curses,

    /// You detect the presence of portals.
    detect_portals,

    /// You identity items on sight, including monster equipment.
    identify_items,

    /// You reveal the layout of the nearby dungeon.
    scrying,

    /// You detect the threat level of monsters.
    detect_montier,

    /// You detect the presence of items.
    detect_items,

    /// You avoid explore-based traps.
    avoid_traps,

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

    /// Your god blesses your followers when they kill evil things.
    bless_followers_vs_evil,

    /// You cannot be hasted.
    no_haste,

    /// You move slowly.
    slowed,

    /// Fewer creatures spawn on the orb run.
    slow_orb_run,

    /// Fewer creatures spawn in the Abyss, and it morphs less quickly.
    slow_abyss,

    /// The Zot clock runs more slowly.
    slow_zot,

    /// Your attributes are boosted.
    stat_boost,

    /// Poison affects you more slowly.
    slow_poison,

    /// You have an umbra.
    umbra,

    /// Your shadow attacks alongside you.
    shadow_attacks,

    /// Your shadow casts attack spells alongside you.
    shadow_spells,

    /// You dampen all noise in your surroundings.
    dampen_noise,

    /// No accuracy penalty in umbra. Gain stealth in umbra.
    nightvision,

    /// No distortion unwield effects.
    safe_distortion,

    /// After god wrath, enemies nearby are banished.
    wrath_banishment,

    /// Less map rot in the abyss.
    map_rot_res_abyss,

    /// Higher chance for spawning the abyssal rune.
    attract_abyssal_rune,

    /// Potentially save allies from death. Holy or natural, in LOS.
    protect_ally,

    /// Chance to nullify a deadly blow, dependent on piety.
    protect_from_harm,

    /// Strong chance to nullify a deadly blow, with a timeout.
    lifesaving,

    /// Divine halo around the player, size increases with piety.
    halo,

    /// Protect allies from abjuration by decreasing the power to half.
    abjuration_protection,

    /// Protect allies from abjuration by decreasing the power. Higher HD means
    /// better protection.
    abjuration_protection_hd,

    /// Gain HP when killing monsters.
    restore_hp,

    /// Gain HP and MP when killing evil monsters.
    restore_hp_mp_vs_evil,

    /// Mutation resistance increasing with piety.
    resist_mutation,

    /// Polymorph resistance increasing with piety.
    resist_polymorph,

    /// Chance of avoiding hell effects, increasing with piety.
    resist_hell_effects,

    /// Warning about shapeshifters when they come to view.
    warn_shapeshifter,

    /// Cleanse mutation potions (they only delete mutations).
    cleanse_mut_potions,

    /// Torment resistance, piety dependent.
    resist_torment,

    /// Protection against necromancy miscasts and mummy death curses.
    miscast_protection_necromancy,

    /// Chance to extend berserk duration and avoid paralysis, piety dependent.
    extend_berserk,

    /// Gold aura that distracts enemies.
    gold_aura,

    /// Corpses turn to gold.
    goldify_corpses,

    /// Allied plants are friendly towards you
    friendly_plants,

    /// You can walk through plants
    pass_through_plants,

    /// You can fire through allied plants
    shoot_through_plants,

    /// Slimes and eye monsters are neutral towards you
    neutral_slimes,

    /// You are immune to slime covered walls
    slime_wall_immune,

    /// Jiyva protects you from corrosion
    resist_corrosion,

    /// Jiyva summons jellies to protect you
    jellies_army,

    /// Jiyva allows jellies to eats off-level and seen items.
    jelly_eating,

    /// Piety-scaled healing & mpreg
    jelly_regen,

    // You have a chance to spawn slimes when hit hard
    spawn_slimes_on_hit,

    /// Jiyva grants you access to the hidden treasures of the Slime Pits
    unlock_slime_vaults,

    /// You have a chance to gain mana when you kill
    mp_on_kill,

    /// You are less likely to miscast destructive spells
    spells_success,

    /// You can cast destructive spells farther
    spells_range,

    /// Clouds don't harm you
    cloud_immunity,

    /// You generate elemental clouds to protect you
    storm_shield,

    /// Chances to be struck by projectiles are reduced
    upgraded_storm_shield,

    /// Elemental attacks leaves you somewhat more resistant to them
    elemental_adaptation,

    /// Your enemies will sometime fail their attack or even hit themselves
    aura_of_power,

    /// Enemies that inflict damage upon you will sometime receive a detrimental status effect
    upgraded_aura_of_power,

    /// Hep's transfer drains enemies adjacent to the effect's target
    transfer_drain,

    /// Hep reduces your max HP
    frail,

    // TSO prevents you from stabbing
    no_stabbing,

    /// Martial attacks
    wu_jian_lunge,
    wu_jian_whirlwind,
    wu_jian_wall_jump,

    /// Okawaru prevents gaining any allies
    no_allies,

    // rF+
    resist_fire,

    // Yredelemnul reaping
    reaping,

    // Immunity to spectral mist clouds
    r_spectral_mist,
};

enum ru_interference
{
    DO_NOTHING,
    DO_BLOCK_ATTACK,
    DO_REDIRECT_ATTACK
};

#define DITH_SHADOW_ATTACK_KEY "dith_shadow_attack"
#define DITH_SHADOW_SPELLPOWER_KEY "dith_shadow_spellpower"
#define DITH_SHADOW_MID_KEY "dith_shadow_mid"
#define DITH_SHADOW_LAST_TARGET_KEY "dith_shadow_last_target"

bool god_gives_passive(god_type god, passive_t passive);
bool have_passive(passive_t passive);
bool will_have_passive(passive_t passive);
int rank_for_passive(passive_t passive);

int chei_stat_boost(int piety = you.piety);

void jiyva_eat_offlevel_items();

void ash_check_bondage();
void ash_id_inventory();
void ash_id_item(item_def& item, bool silent = true);
int ash_detect_portals(bool all);
monster_type ash_monster_tier(const monster *mon);
unsigned int ash_skill_point_boost(skill_type sk, int scaled_skill);
int ash_skill_boost(skill_type sk, int scale);
bool ash_has_skill_boost(skill_type sk);
void ash_scrying();

void gozag_move_level_gold_to_top();
void gozag_move_gold_to_top(const coord_def p);
void gozag_count_level_gold();

int qazlal_sh_boost(int piety = you.piety);
void qazlal_storm_clouds();
void qazlal_element_adapt(beam_type flavour, int strength);

bool does_ru_wanna_redirect(const monster &mon);
ru_interference get_ru_attack_interference_level();
void dithmenos_cleanup_player_shadow(monster* shadow);
monster* dithmenos_get_player_shadow();

monster* create_player_shadow(coord_def pos, bool friendly = true,
                              spell_type spell_known = SPELL_NO_SPELL);
void dithmenos_shadow_melee(actor* target);
void dithmenos_shadow_shoot(const dist &d, const item_def &item);
void dithmenos_shadow_spell(spell_type spell);

void uskayaw_prepares_audience();
void uskayaw_bonds_audience();

void wu_jian_trigger_serpents_lash(bool wall_jump, const coord_def& old_pos);
void wu_jian_heaven_tick();
void wu_jian_decrement_heavenly_storm();
void wu_jian_end_heavenly_storm();
monster *wu_jian_wall_jump_monster_at(const coord_def &pos);
bool wu_jian_wall_jump_triggers_attacks(const coord_def &pos);
void wu_jian_wall_jump_effects();
bool wu_jian_has_momentum(wu_jian_attack_type);
bool wu_jian_post_move_effects(bool did_wall_jump,
                               const coord_def& initial_position);
bool wu_jian_move_triggers_attacks(coord_def new_pos);

void makhleb_tyrant_buff();
void makhleb_celebrant_bloodrite();
void makhleb_execution_activate();
bool makhleb_haemoclasm_trigger_check(const monster& victim);
