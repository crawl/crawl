#pragma once

#include "tag-version.h"

struct form_entry
{
    // Row 1:
    transformation tran;
    monster_type equivalent_mons;
    const char *short_name;
    const char *long_name;
    const char *wiz_name;

    // Row 2:
    const char *description;

    // Row 3:
    int min_skill;
    int max_skill;
    talisman_type talisman;

    // Row 4:
    int blocked_slots;
    int resists;

    // Row 5:
    FormDuration duration;
    int str_mod;
    int dex_mod;
    size_type size;
    int hp_mod;

    // Row 6:
    int flat_ac;
    int skill_ac;
    int xl_ac;
    bool can_cast;
    int spellcasting_penalty;
    bool unarmed_hit_bonus;
    int base_unarmed_damage;

    // Row 7:
    brand_type uc_brand;
    int uc_colour;
    const char *uc_attack;
    FormAttackVerbs uc_attack_verbs;

    // Row 8:
    form_capability can_fly;
    form_capability can_swim;
    form_capability can_bleed;
    bool keeps_mutations;

    const char *shout_verb;
    int shout_volume_modifier;
    const char *hand_name;
    const char *foot_name;
    const char *prayer_action;
    const char *flesh_equivalent;

    // pairs of the form {terse_description, full_description} for display in
    // `A` and `%`. Either can be empty.
    vector<pair<string,string>> fakemuts;
};

static const form_entry formdata[] =
{
{
    transformation::none, MONS_PLAYER, "", "", "none",
    "",
    0, 0, NUM_TALISMANS,
    EQF_NONE, MR_NO_FLAGS,
    FormDuration(0, PS_NONE, 0), 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, false, 3,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true,
    "", 0, "", "", "", "",
    {}
},
#if TAG_MAJOR_VERSION == 34
{
    transformation::spider, MONS_SPIDER, "Spider", "spider-form", "spider",
    "a venomous arachnid creature.",
    0, 27, NUM_TALISMANS,
    EQF_PHYSICAL, MR_VUL_POISON,
    FormDuration(10, PS_DOUBLE, 60), 0, 5, SIZE_TINY, 10,
    2, 0, 0, true, 10, true, 5,
    SPWPN_VENOM, LIGHTGREEN, "Fangs", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, false,
    "hiss", -4, "front pincers", "", "crawl onto", "flesh",
    { {"venomous fangs", "You have venomous fangs."},
      {"", "You are tiny and dextrous."} // short-form "tiny" is automatically added
    }
},
#endif
{
    transformation::blade_hands, MONS_PLAYER, "Blade", "", "blade",
    "",
    9, 20, TALISMAN_BLADE,
    EQF_HANDS, MR_NO_FLAGS,
    FormDuration(10, PS_SINGLE, 100), 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 20, true, 22,
    SPWPN_NORMAL, RED, "", { "hit", "slash", "slice", "shred" },
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true,
    "", 0, "", "", "", "",
    {}
},
{
    transformation::statue, MONS_STATUE, "Statue", "statue-form", "statue",
    "a stone statue.",
    15, 23, TALISMAN_STATUE,
    EQF_STATUE, MR_RES_ELEC | MR_RES_NEG | MR_RES_PETRIFY,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 13,
    20, 18, 0, true, 0, true, 12,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, true,
    "", 0, "", "", "place yourself before", "stone",
    { { "slow and powerful", "Your actions are slow, but your melee attacks are powerful." },
      { "torment resistance 1", "You are resistant to unholy torment." } // same as MUT_TORMENT_RESISTANCE
    }
},
{
    transformation::anaconda, MONS_ANACONDA, "Anaconda", "snake-form", "snake",
    "an enormous anaconda.",
    9, 20, TALISMAN_SERPENT,
    EQF_PHYSICAL, MR_NO_FLAGS,
    DEFAULT_DURATION, 5, 0, SIZE_LARGE, 12,
    8, 4, 0, true, 30, true, 12,
    SPWPN_NORMAL, LIGHTGREY, "", { "hit", "lash", "body-slam", "crush" },
    FC_DEFAULT, FC_FORBID, FC_ENABLE, false,
    "", 0, "", "", "coil in front of", "flesh",
    { { "constrict", "You have a powerful constriction melee attack."} }
},

{
    transformation::dragon, MONS_PROGRAM_BUG, "Dragon", "dragon-form", "dragon",
    "a fearsome dragon!",
    17, 27, TALISMAN_DRAGON,
    EQF_PHYSICAL, MR_RES_POISON,
    DEFAULT_DURATION, 10, 0, SIZE_GIANT, 15,
    14, 4, 0, true, 0, true, 32,
    SPWPN_NORMAL, GREEN, "Teeth and claws", { "hit", "claw", "bite", "maul" },
    FC_ENABLE, FC_FORBID, FC_ENABLE, false,
    "roar", 6, "foreclaw", "", "bow your head before", "flesh",
    { { "dragon claw", "You have a powerful clawing attack." },
      { "dragon scales", "Your giant scaled body is strong and resiliant, but less evasive." },
    }
},

{
    transformation::death, MONS_ANCIENT_CHAMPION, "Death", "death-form", "death",
    "an undying horror.",
    21, 27, TALISMAN_DEATH,
    EQF_NONE, MR_RES_COLD | mrd(MR_RES_NEG, 3),
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, true, 9,
    SPWPN_DRAINING, MAGENTA, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_FORBID, true,
    "", 0, "", "", "", "bone",
    { { "vile attack", "Your melee and unarmed attacks drain, slow and weaken victims."},
      { "torment immunity", "You are immune to unholy pain and torment."},
      { "no potions", "<lightred>You cannot drink.</lightred>" },
    }
},

{
    transformation::bat, MONS_PROGRAM_BUG, "Bat", "bat-form", "bat",
    "",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 5, SIZE_TINY, 10,
    0, 0, 0, false, 0, true, 1,
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_ENABLE, FC_FORBID, FC_ENABLE, false,
    "squeak", -8, "foreclaw", "", "perch on", "flesh",
    {
      {"", "You are tiny and dextrous."} // short-form "tiny" is automatically added
    }
},

{
    transformation::pig, MONS_HOG, "Pig", "pig-form", "pig",
    "a filthy swine.",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_SMALL, 10,
    0, 0, 0, false, 0, false, 3,
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, FC_ENABLE, false,
    "squeal", 0, "front trotter", "trotter", "bow your head before", "flesh",
    {} // XX UC penalty?
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::appendage, MONS_PLAYER, "App", "appendages", "appendages",
    "",
    0, 0, NUM_TALISMANS,
    SLOTF(EQ_BOOTS) | SLOTF(EQ_HELMET), MR_NO_FLAGS,
    FormDuration(10, PS_DOUBLE, 60), 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, false, 3,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true,
    "", 0, "", "", "", "",
    {}
},
#endif

{
    transformation::tree, MONS_ANIMATED_TREE, "Tree", "tree-form", "tree",
    "a tree.",
    0, 0, NUM_TALISMANS,
    EQF_LEAR | SLOTF(EQ_CLOAK), MR_RES_POISON,
    BAD_DURATION, 0, 0, SIZE_CHARACTER, 15,
    20, 0, 50, true, 0, true, 12,
    SPWPN_NORMAL, BROWN, "Branches", { "hit", "smack", "pummel", "thrash" },
    FC_FORBID, FC_FORBID, FC_FORBID, false,
    "creak", 0, "branch", "root", "sway towards", "wood",
    {
        { "stationary", "Your roots penetrate the ground, keeping you stationary." },
        { "stasis", "You cannot be teleported."},
        { "torment immunity", "You are immune to unholy pain and torment."},
    }
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::porcupine, MONS_PORCUPINE, "Porc", "porcupine-form", "porcupine",
    "a spiny porcupine.",
    0, 0, NUM_TALISMANS,
    EQF_ALL, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    0, 0, 0, false, 0, false, 3,
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, FC_ENABLE, false,
    "squeak", -8, "front leg", "", "curl into a sanctuary of spikes before", "flesh",
    {}
},
#endif

{
    transformation::wisp, MONS_INSUBSTANTIAL_WISP, "Wisp", "wisp-form", "wisp",
    "an insubstantial wisp.",
    0, 0, NUM_TALISMANS,
    EQF_ALL, mrd(MR_RES_FIRE, 2) | mrd(MR_RES_COLD, 2) | MR_RES_ELEC
             | MR_RES_STICKY_FLAME | mrd(MR_RES_NEG, 3) | MR_RES_ACID
             | MR_RES_PETRIFY,
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    5, 0, 50, false, 0, true, 5,
    SPWPN_NORMAL, LIGHTGREY, "Misty tendrils", { "touch", "touch",
                                                 "engulf", "engulf" },
    FC_ENABLE, FC_FORBID, FC_FORBID, false,
    "whoosh", -8, "misty tendril", "strand", "swirl around", "vapour",
    {
        {"insubstantial", "Your tiny insubstantial body is highly resistant to most damage types." },
    }
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::jelly, MONS_JELLY, "Jelly", "jelly-form", "jelly",
    "a lump of jelly.",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, false, 0, false, 3,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, false,
    "", 0, "", "", "", "",
    {}
},
#endif

{
    transformation::fungus, MONS_WANDERING_MUSHROOM, "Fungus", "fungus-form", "fungus",
    "a sentient fungus.",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL, MR_RES_POISON | mrd(MR_RES_NEG, 3),
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    12, 0, 0, false, 0, true, 12,
    SPWPN_CONFUSE, BROWN, "Spores", FormAttackVerbs("release spores at"),
    FC_DEFAULT, FC_FORBID, FC_FORBID, false,
    "sporulate", -8, "hypha", "", "release spores on", "flesh",
    {
        {"", "You are tiny and evasive." },
        {"melee confuse", "Your melee attack releases spores that confuse breathing creatures."},
        {"terrified", "<lightred>You become terrified and freeze when enemies are visible.</lightred>"} // XX coloring is really hacky here
    }
},

{
    transformation::shadow, MONS_PLAYER_SHADOW, "Shadow", "shadow-form", "shadow",
    "a swirling mass of dark shadows.",
    0, 0, NUM_TALISMANS,
    EQF_NONE, mrd(MR_RES_POISON, 3) | mrd(MR_RES_NEG, 3) | MR_RES_MIASMA
                                                         | MR_RES_PETRIFY,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, false, 3,
    SPWPN_NORMAL, MAGENTA, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, true,
    "", 0, "", "", "", "shadow",
    {
        {"shadow resist", "You are immune to unholy torment and to willpower attacks."},
        {"half damage", "Damage taken is halved, but you are drained when taking damage."},
        {"", "Your attacks are insubstantial and do half damage."}, // XX mark as badmut
        {"bleed smoke", "You bleed smoke when taking damage."},
        {"invisible", "You are invisible."},
    }
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::hydra, MONS_HYDRA, "Hydra", "hydra-form", "hydra",
    "",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL, MR_RES_POISON,
    DEFAULT_DURATION, 0, 0, SIZE_GIANT, 13,
    6, 5, 0, true, 0, true, -1,
    SPWPN_NORMAL, GREEN, "", { "nip at", "bite", "gouge", "chomp" },
    FC_DEFAULT, FC_ENABLE, FC_ENABLE, false,
    "roar", 4, "foreclaw", "", "bow your heads before", "flesh",
    { { "fast swimmer", "You swim very quickly." },
      { "devour", "You can devour living enemies to heal." }
    }
},
#endif

{
    transformation::storm, MONS_TWISTER, "Storm", "storm-form", "storm",
    "a lightning-filled tempest!",
    21, 27, TALISMAN_STORM,
    EQF_PHYSICAL, MR_RES_ELEC | MR_RES_PETRIFY | MR_RES_STICKY_FLAME,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    10, 20, 0, true, 0, true, -1,
    SPWPN_ELECTROCUTION, LIGHTCYAN, "", { "hit", "buffet", "batter", "blast" },
    FC_ENABLE, FC_DEFAULT, FC_FORBID, false,
    "bellow", 0, "", "", "place yourself before", "air",
    { { "cleaving", "Your electrical attacks strike out in all directions at once." },
      { "", "You are incredibly evasive." },
      { "insubstantial", "Your insubstantial body is immune to petrification, constriction, and being set on fire"}
    }
},

{
    transformation::beast, MONS_WOLF, "Beast", "beast-form", "beast",
    "a hulking beast.",
    0, 13, TALISMAN_BEAST,
    EQF_AUXES, MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, true, 5,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true,
    "", 0, "", "", "", "",
    { }
},

{
    transformation::maw, MONS_PUTRID_MOUTH, "Maw", "maw-form", "maw",
    "a creature with a mouth for a stomach.",
    9, 20, TALISMAN_MAW,
    SLOTF(EQ_BODY_ARMOUR), MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, true, 5,
    SPWPN_NORMAL, GREEN, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true,
    "shout twice", 0, "", "", "", "",
    { { "devouring maw", "Your midsection houses a second, enormous mouth." },}
}

};
COMPILE_CHECK(ARRAYSZ(formdata) == NUM_TRANSFORMS);
