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
    FormScaling ac;
    bool can_cast;
    FormScaling unarmed_bonus_dam;

    // Row 7:
    brand_type uc_brand;
    int uc_colour;
    const char *uc_attack;
    FormAttackVerbs uc_attack_verbs;

    // Row 8:
    form_capability can_fly;
    form_capability can_swim;
    bool keeps_mutations;
    bool changes_physiology;

    // Row 9:
    form_capability has_blood;
    form_capability has_hair;
    form_capability has_bones;
    form_capability has_feet;
    form_capability has_ears;

    // Row 10:
    const char *shout_verb;
    int shout_volume_modifier;
    const char *hand_name;
    const char *foot_name;
    const char *prayer_action;
    const char *flesh_equivalent;

    // pairs of the form {terse_description, full_description} for display in
    // `A` and `%`. Either can be empty.
    // Resistances (except rTorm), flight, amphibious, no grasping,
    // melded equipment are added separately in mutation.cc:_get_form_badmuts
    vector<pair<string,string>> fakemuts;
    vector<pair<string,string>> badmuts;
};

static const form_entry formdata[] =
{
{
    transformation::none, MONS_PLAYER, "", "", "none",
    "",
    0, 0, NUM_TALISMANS,
    EQF_NONE, MR_NO_FLAGS,
    FormDuration(0, PS_NONE, 0), 0, 0, SIZE_CHARACTER, 10,
    {}, true, {},
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, true, false,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "", 0, "", "", "", "",
    {},
    {}
},
#if TAG_MAJOR_VERSION == 34
{
    transformation::spider, MONS_SPIDER, "Spider", "spider-form", "spider",
    "a venomous arachnid creature.",
    0, 27, NUM_TALISMANS,
    EQF_PHYSICAL, mrd(MR_RES_POISON, -1),
    FormDuration(10, PS_DOUBLE, 60), 0, 5, SIZE_TINY, 10,
    {}, true, {},
    SPWPN_VENOM, LIGHTGREEN, "Fangs", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, false, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID,
    "hiss", -4, "front pincers", "", "crawl onto", "flesh",
    { {"venomous fangs", "You have venomous fangs."},
      {"", "You are tiny and dextrous."} // short-form "tiny" is automatically added
    },
    { {"poison vulnerability", "You are susceptible to poisons. (rPois-)"}}
},
#endif
{
    transformation::blade_hands, MONS_PLAYER, "Blade", "", "blade",
    "",
    10, 19, TALISMAN_BLADE,
    EQF_HANDS, MR_NO_FLAGS,
    FormDuration(10, PS_SINGLE, 100), 0, 0, SIZE_CHARACTER, 10,
    {}, true, FormScaling().Base(12).Scaling(8),
    SPWPN_NORMAL, RED, "", { "hit", "slash", "slice", "shred" },
    FC_DEFAULT, FC_DEFAULT, true, false,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "", 0, "", "", "", "",
    {},
    {}
},
{
    transformation::statue, MONS_STATUE, "Statue", "statue-form", "statue",
    "a stone statue.",
    16, 25, TALISMAN_STATUE,
    EQF_STATUE, MR_RES_ELEC | MR_RES_NEG | MR_RES_PETRIFY,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 13,
    FormScaling().Base(27).Scaling(11), true, FormScaling().Base(9),
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, true, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_DEFAULT, FC_ENABLE,
    "", 0, "", "", "place yourself before", "stone",
    { { "powerful", "Your melee attacks are powerful." },
      { "torment resistance 1", "You are resistant to unholy torment." } // same as MUT_TORMENT_RESISTANCE
    }, // rElec, rN+, poison immunity are added separately
    { { "slow", "All of your actions are slow." } }
},
{
    transformation::serpent, MONS_ANACONDA, "Serpent", "snake-form", "snake",
    "an enormous serpent.",
    10, 19, TALISMAN_SERPENT,
    EQF_PHYSICAL, MR_RES_POISON,
    DEFAULT_DURATION, 5, 0, SIZE_LARGE, 12,
    FormScaling().Base(9).Scaling(6), true, FormScaling().Base(7),
    SPWPN_NORMAL, LIGHTGREY, "", { "hit", "lash", "body-slam", "crush" },
    FC_DEFAULT, FC_ENABLE, false, true,
    FC_DEFAULT, FC_FORBID, FC_ENABLE, FC_FORBID, FC_FORBID,
    "hiss", -2, "", "", "coil in front of", "flesh",
    { { "constrict", "You have a powerful constriction melee attack."} },
    // cold-blooded and amphibious are added separately
    {}
},

{
    transformation::dragon, MONS_PROGRAM_BUG, "Dragon", "dragon-form", "dragon",
    "a fearsome dragon!",
    16, 25, TALISMAN_DRAGON,
    EQF_PHYSICAL, MR_RES_POISON,
    DEFAULT_DURATION, 10, 0, SIZE_GIANT, 15,
    FormScaling().Base(12).Scaling(6), true, FormScaling().Base(15).Scaling(9),
    SPWPN_NORMAL, GREEN, "Teeth and claws", { "hit", "claw", "bite", "maul" },
    FC_ENABLE, FC_FORBID, false, true,
    FC_DEFAULT, FC_FORBID, FC_ENABLE, FC_ENABLE, FC_ENABLE,
    "roar", 6, "foreclaw", "", "bow your head before", "flesh",
    { { "dragon claw", "You have a powerful clawing attack." },
      { "dragon scales", "Your giant scaled body is strong and resilient, but less evasive." },
    }, // resistances, breath, flying added separately
    {}
},

{
    transformation::death, MONS_ANCIENT_CHAMPION, "Death", "death-form", "death",
    "an undying horror.",
    23, 27, TALISMAN_DEATH,
    EQF_NONE, MR_RES_COLD | mrd(MR_RES_NEG, 3),
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    {}, true, FormScaling().Base(6),
    SPWPN_DRAINING, MAGENTA, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, true, true,
    FC_FORBID, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "", 0, "", "", "", "fossilised flesh",
    { { "vile attack", "Your melee attacks drain, slow and weaken victims." },
      { "siphon essence", "You can torment nearby foes to heal from their wounds." },
      { "torment immunity", "You are immune to unholy pain and torment." },
    }, // rC+, rN+++, poison immunity added separately
    { { "no potions", "You cannot drink." } }
},

{
    transformation::bat, MONS_PROGRAM_BUG, "Bat", "bat-form", "bat",
    "",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 5, SIZE_TINY, 10,
    {}, false, FormScaling().Base(-2),
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_ENABLE, FC_FORBID, false, true,
    FC_DEFAULT, FC_ENABLE, FC_ENABLE, FC_ENABLE, FC_ENABLE,
    "squeak", -8, "foreclaw", "", "perch on", "flesh",
    { { "extremely fast", "You cover ground extremely quickly." },
      { "", "You are tiny, dextrous, and very evasive." } // short-form "tiny" is automatically added
    },
    { { "weak attacks", "Your unarmed attacks are very weak." },
      { "no casting", "You cannot cast spells." },
    }
},

{
    transformation::pig, MONS_HOG, "Pig", "pig-form", "pig",
    "a filthy swine.",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_SMALL, 10,
    {}, false, FormScaling().XLBased(),
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, false, true,
    FC_DEFAULT, FC_ENABLE, FC_ENABLE, FC_ENABLE, FC_ENABLE,
    "squeal", 0, "front trotter", "trotter", "bow your head before", "flesh",
    { { "very fast", "You cover ground very quickly." } },
    { { "weak attacks", "Your unarmed attacks are very weak." },
      { "no casting", "You cannot cast spells." },
    }
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::appendage, MONS_PLAYER, "App", "appendages", "appendages",
    "",
    0, 0, NUM_TALISMANS,
    SLOTF(EQ_BOOTS) | SLOTF(EQ_HELMET), MR_NO_FLAGS,
    FormDuration(10, PS_DOUBLE, 60), 0, 0, SIZE_CHARACTER, 10,
    {}, true, {},
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, true, false,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "", 0, "", "", "", "",
    {},
    {}
},
#endif

{
    transformation::tree, MONS_ANIMATED_TREE, "Tree", "tree-form", "tree",
    "a tree.",
    0, 0, NUM_TALISMANS,
    EQF_LEAR | SLOTF(EQ_CLOAK), MR_RES_POISON,
    BAD_DURATION, 0, 0, SIZE_CHARACTER, 15,
    FormScaling().Base(20).Scaling(14).XLBased(), true, FormScaling().Base(9),
    SPWPN_NORMAL, BROWN, "Branches", { "hit", "smack", "pummel", "thrash" },
    FC_FORBID, FC_FORBID, false, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID,
    "creak", 0, "branch", "root", "sway towards", "wood",
    { { "resilient", "Your bark is very hard, but your evasion is minimal." },
      { "branches", "Your unarmed attacks smack enemies forcefully with your branches." },
      { "torment immunity", "You are immune to unholy torment." },
    }, // rPois added separately
    // Technically these can have marginal benefits in some situations
    // but they're mostly bad so let's mark them as bad.
    { { "stationary", "Your roots penetrate the ground, keeping you stationary." },
      { "-Tele", "You cannot be teleported." },
    }
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::porcupine, MONS_PORCUPINE, "Porc", "porcupine-form", "porcupine",
    "a spiny porcupine.",
    0, 0, NUM_TALISMANS,
    EQF_ALL, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    {}, false, FormScaling().XLBased(),
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, false, true,
    FC_DEFAULT, FC_ENABLE, FC_ENABLE, FC_ENABLE, FC_ENABLE,
    "squeak", -8, "front leg", "", "curl into a sanctuary of spikes before", "flesh",
    {},
    { { "no casting", "You cannot cast spells." }, }
},
#endif

{
    transformation::wisp, MONS_INSUBSTANTIAL_WISP, "Wisp", "wisp-form", "wisp",
    "an insubstantial wisp.",
    0, 0, NUM_TALISMANS,
    EQF_ALL, mrd(MR_RES_FIRE, 2) | mrd(MR_RES_COLD, 2) | MR_RES_ELEC
             | mrd(MR_RES_NEG, 3) | MR_RES_ACID
             | MR_RES_PETRIFY,
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    FormScaling().Base(5).Scaling(14).XLBased(), false, FormScaling().Base(2).XLBased(),
    SPWPN_NORMAL, LIGHTGREY, "Misty tendrils", { "touch", "touch",
                                                 "engulf", "engulf" },
    FC_ENABLE, FC_FORBID, false, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID, FC_DEFAULT,
    "whoosh", -8, "misty tendril", "strand", "swirl around", "vapour",
    { { "", "You are tiny and evasive." },
      { "insubstantial", "You are insubstantial and cannot be petrified, ensnared, or set on fire." },
      { "torment immunity", "You are immune to unholy pain and torment." },
      { "highly resistant", "You are highly resistant to most damage types. (rF++, rC++, rN+++, rElec, rCorr)" },
    },
    { { "no casting", "You cannot cast spells." }, }
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::jelly, MONS_JELLY, "Jelly", "jelly-form", "jelly",
    "a lump of jelly.",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_CHARACTER, 10,
    {}, false, FormScaling().XLBased(),
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, false, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID, FC_DEFAULT,
    "", 0, "", "", "", "",
    {},
    { { "no casting", "You cannot cast spells." },
    }
},
#endif

{
    transformation::fungus, MONS_WANDERING_MUSHROOM, "Fungus", "fungus-form", "fungus",
    "a sentient fungus.",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL, MR_RES_POISON,
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    FormScaling().Base(12), false, FormScaling().Base(9).XLBased(),
    SPWPN_CONFUSE, BROWN, "Spores", FormAttackVerbs("release spores at"),
    FC_DEFAULT, FC_FORBID, false, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID,
    "sporulate", -8, "hypha", "", "release spores on", "flesh",
    { { "", "You are tiny and evasive." },
      { "spores", "Your melee attacks release spores that confuse breathing creatures." },
      { "torment immunity", "You are immune to unholy pain and torment." },
    }, // rPois, no grasping added separately
    { { "terrified", "You become terrified and freeze when enemies are visible." },
      { "no casting", "You cannot cast spells." },
    }
},

#if TAG_MAJOR_VERSION == 34
{
    transformation::shadow, MONS_PLAYER_SHADOW, "Shadow", "shadow-form", "shadow",
    "a swirling mass of dark shadows.",
    0, 0, NUM_TALISMANS,
    EQF_NONE, mrd(MR_RES_POISON, 3) | mrd(MR_RES_NEG, 3) | MR_RES_MIASMA
                                                         | MR_RES_PETRIFY,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    {}, true, {},
    SPWPN_NORMAL, MAGENTA, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, true, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID, FC_DEFAULT,
    "", 0, "", "", "", "shadow",
    { { "half damage", "Damage taken is halved, but you are drained when taking damage."},
      { "bleed smoke", "You bleed smoke when taking damage." },
      { "invisible", "You are invisible." },
      { "unshakeable will", "Your willpower is unshakeable." },
      { "torment immunity", "You are immune to unholy pain and torment."},
    },
    { { "weak attacks", "Your attacks do half damage." },
      { "weak spells", "Your spells are much less powerful." }, // two de-enhancers
    }
},
#endif

#if TAG_MAJOR_VERSION == 34
{
    transformation::hydra, MONS_HYDRA, "Hydra", "hydra-form", "hydra",
    "",
    0, 0, NUM_TALISMANS,
    EQF_PHYSICAL, MR_RES_POISON,
    DEFAULT_DURATION, 0, 0, SIZE_GIANT, 13,
    {}, true, {},
    SPWPN_NORMAL, GREEN, "", { "nip at", "bite", "gouge", "chomp" },
    FC_DEFAULT, FC_ENABLE, false, true,
    FC_DEFAULT, FC_FORBID, FC_ENABLE, FC_ENABLE, FC_ENABLE,
    "roar", 4, "foreclaw", "", "bow your heads before", "flesh",
    { { "fast swimmer", "You swim very quickly." },
      { "devour", "You can devour living enemies to heal." }
    },
    {}
},
#endif

{
    transformation::storm, MONS_TWISTER, "Storm", "storm-form", "storm",
    "a lightning-filled tempest!",
    23, 27, TALISMAN_STORM,
    EQF_PHYSICAL, MR_RES_ELEC | MR_RES_PETRIFY,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    FormScaling().Base(12).Scaling(3), true, FormScaling().Base(24).Scaling(6),
    SPWPN_ELECTROCUTION, LIGHTCYAN, "", { "hit", "buffet", "batter", "blast" },
    FC_ENABLE, FC_DEFAULT, false, true,
    FC_FORBID, FC_FORBID, FC_FORBID, FC_FORBID, FC_DEFAULT,
    "bellow", 0, "", "", "place yourself before", "air",
    { { "electrical cleaving", "Your electrical attacks strike out in all directions at once." },
      { "evasive", "You are incredibly evasive." },
      { "blinkbolt", "You can turn into living lightning." },
      { "insubstantial", "You are insubstantial and cannot be petrified, ensnared, or set on fire." },
    }, // rElec added separately
    {}
},

{
    transformation::beast, MONS_WOLF, "Beast", "beast-form", "beast",
    "a hulking beast.",
    0, 7, TALISMAN_BEAST,
    EQF_AUXES, MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    {}, true, {},
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, true, false,
    FC_DEFAULT, FC_ENABLE, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "", 0, "", "", "", "",
    {}, // slaying bonus added separately
    {}
},

{
    transformation::maw, MONS_PUTRID_MOUTH, "Maw", "maw-form", "maw",
    "a creature with a mouth for a stomach.",
    10, 19, TALISMAN_MAW,
    SLOTF(EQ_BODY_ARMOUR), MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    {}, true, FormScaling().Base(2),
    SPWPN_NORMAL, GREEN, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, true, false,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "shout twice", 0, "", "", "", "",
    { { "devouring maw", "Your midsection houses a second, enormous mouth." },
      { "", "You can devour living enemies to heal." }
    },
    {}
},

{
    transformation::flux, MONS_SHAPESHIFTER, "Flux", "flux-form", "flux",
    "something dangerously unstable.",
    7, 14, TALISMAN_FLUX,
    SLOTF(EQ_WEAPON) | SLOTF(EQ_OFFHAND) | SLOTF(EQ_BODY_ARMOUR), MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    {}, true, {},
    SPWPN_NORMAL, CYAN, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, true, false,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "", 0, "", "", "", "",
    { { "contaminating", "Foes you strike become dangerously contaminated with magical radiation." }, },
    { { "glow", "You glow with magical radiation, making you easy to see." } }
},

{
    transformation::slaughter, MONS_BRIMSTONE_FIEND, "Slaughter", "slaughter-form", "slaughter",
    "a vessel of demonic slaughter.",
    0, 0, NUM_TALISMANS,
    EQF_NONE, mrd(MR_RES_POISON, 1) | mrd(MR_RES_NEG, 3),
    FormDuration(50, PS_DOUBLE, 100), 0, 0, SIZE_CHARACTER, 10,
    {}, true, {},
    SPWPN_NORMAL, LIGHTRED, "", DEFAULT_VERBS,
    FC_ENABLE, FC_FORBID, true, false,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, FC_DEFAULT,
    "", 0, "", "", "", "",
    { { "damage resistance", "Damage taken is decreased by 1/3."},
      { "doubled heal-on-kills", "Healing from kills is doubled."},
      { "unshakeable will", "Your willpower is unshakeable." },
      { "torment immunity", "You are immune to unholy pain and torment."},
    },
    { { "demonic bargain", "You will endure the Crucible once your slaughter is complete." },
    }
},

};
COMPILE_CHECK(ARRAYSZ(formdata) == NUM_TRANSFORMS);
