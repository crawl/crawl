#pragma once

enum pakellas_blueprint_type
{
    BLUEPRINT_DESTRUCTION_START = 0,
    BLUEPRINT_RANGE = BLUEPRINT_DESTRUCTION_START,
    BLUEPRINT_PENTAN,
    BLUEPRINT_BOME,
    BLUEPRINT_ELEMENTAL_FIRE,
    BLUEPRINT_ELEMENTAL_COLD,
    BLUEPRINT_ELEMENTAL_ELEC,
    BLUEPRINT_ELEMENTAL_EARTH,
    BLUEPRINT_PERFECT_SHOT,
    BLUEPRINT_CLOUD,
    BLUEPRINT_DEBUF_SLOW,
    BLUEPRINT_DEBUF_BLIND,
    BLUEPRINT_KNOCKBACK,
    BLUEPRINT_STICKY_FLAME,
    BLUEPRINT_FROZEN,
    BLUEPRINT_CHAIN_LIGHTNING,
    BLUEPRINT_DEFORM,
    BLUEPRINT_CHAOS,
    BLUEPRINT_DESTRUCTION_END,

    BLUEPRINT_SUMMON_START = 100,
    BLUEPRINT_SIZEUP,
    BLUEPRINT_SUMMON_CAPACITY,
    BLUEPRINT_MAGIC_ENERGY_BOLT,
    BLUEPRINT_FIRE_SUMMON,
    BLUEPRINT_COLD_SUMMON,
    BLUEPRINT_ELEC_SUMMON,
    BLUEPRINT_POISON_SUMMON,
    BLUEPRINT_FLIGHT,
    BLUEPRINT_SUMMON_SHOT,
    BLUEPRINT_CLEAVING,
    BLUEPRINT_HALO,
    BLUEPRINT_ANTIMAGIC_AURA,
    BLUEPRINT_CHOATIC_SUMMON,
    BLUEPRINT_SUMMON_END,


    BLUEPRINT_CHARM_START = 200,
    BLUEPRINT_STATUP_STR,
    BLUEPRINT_STATUP_DEX,
    BLUEPRINT_STATUP_INT,
    BLUEPRINT_SWIFT,
    BLUEPRINT_HASTE,
    BLUEPRINT_BLING,
    BLUEPRINT_DIRECTION_BLINK,
    BLUEPRINT_CONTROL_BLINK,
    BLUEPRINT_TELEPORT,
    BLUEPRINT_EMERGENCY_TELEPORT,
    BLUEPRINT_REGEN,
    BLUEPRINT_SMALL_HEAL,
    BLUEPRINT_LARGE_HEAL,
    BLUEPRINT_CHARM_END,

    BLUEPRINT_PUBLIC_START = 300,
    BLUEPRINT_BATTERY_UP,
    BLUEPRINT_LIGHT,
    BLUEPRINT_BATTLEMAGE,
    BLUEPRINT_MORE_ENCHANT,
    BLUEPRINT_HEAVILY_MODIFIED,
    BLUEPRINT_PUBLIC_END,
};

struct pakellas_blueprint_struct
{
    /// name of blueprint
    const char* name;
    /// describe of blueprint
    const char* desc;
    /// abbreviations of blueprint
    const char* abbr;
    /// max_level of blueprint
    int max_level;
    /// additinal cost when install blueprint
    int additional_mana;
    /// percent
    int percent;
    ///Prerequire blueprint (OR)
    vector<int> prerequire;
    ///exclude blueprint (AND)
    vector<int> exclude;
};

extern map<pakellas_blueprint_type, pakellas_blueprint_struct> blueprint_list;

bool pakellas_prototype();
bool pakellas_upgrade();
void pakellas_offer_new_upgrade();
void pakellas_reset_upgrade_timer(bool clear_timer);
int is_blueprint_exist(pakellas_blueprint_type blueprint);