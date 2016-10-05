struct form_entry
{
    // Row 1:
    transformation_type tran;
    monster_type equivalent_mons;
    const char *short_name;
    const char *long_name;
    const char *wiz_name;

    // Row 2:
    const char *description;

    // Row 3:
    int blocked_slots;
    int resists;

    // Row 4:
    FormDuration duration;
    int str_mod;
    int dex_mod;
    size_type size;
    int hp_mod;

    // Row 5
    int flat_ac;
    int power_ac;
    int xl_ac;
    bool can_cast;
    int spellcasting_penalty;
    int unarmed_hit_bonus;
    int base_unarmed_damage;

    // Row 6
    brand_type uc_brand;
    int uc_colour;
    const char *uc_attack;
    FormAttackVerbs uc_attack_verbs;

    // Row 7
    form_capability can_fly;
    form_capability can_swim;
    form_capability can_bleed;
    bool breathes;
    bool keeps_mutations;

    const char *shout_verb;
    int shout_volume_modifier;
    const char *hand_name;
    const char *foot_name;
    const char *prayer_action;
    const char *flesh_equivalent;
};

static const form_entry formdata[] =
{
{
    TRAN_NONE, MONS_PLAYER, "", "", "none",
    "",
    EQF_NONE, MR_NO_FLAGS,
    FormDuration(0, PS_NONE, 0), 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, 0, 3,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true, true,
    "", 0, "", "", "", ""
},
{
    TRAN_SPIDER, MONS_SPIDER, "Spider", "spider-form", "spider",
    "a venomous arachnid creature.",
    EQF_PHYSICAL, MR_VUL_POISON,
    FormDuration(10, PS_DOUBLE, 60), 0, 5, SIZE_TINY, 10,
    2, 0, 0, true, 10, 10, 5,
    SPWPN_VENOM, LIGHTGREEN, "Fangs", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, true, false,
    "hiss", -4, "front leg", "", "crawl onto", "flesh"
},
{
    TRAN_BLADE_HANDS, MONS_PLAYER, "Blade", "", "blade",
    "",
    EQF_HANDS, MR_NO_FLAGS,
    FormDuration(10, PS_SINGLE, 100), 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 20, 12, -1,
    SPWPN_NORMAL, RED, "", { "hit", "slash", "slice", "shred" },
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true, true,
    "", 0, "scythe-like blade", "", "", ""
},
{
    TRAN_STATUE, MONS_STATUE, "Statue", "statue-form", "statue",
    "a stone statue.",
    EQF_STATUE, MR_RES_ELEC | MR_RES_NEG | MR_RES_PETRIFY,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 13,
    20, 12, 0, true, 0, 9, -1,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, false, true,
    "", 0, "", "", "place yourself before", "stone"
},
{
    TRAN_ICE_BEAST, MONS_ICE_BEAST, "Ice", "ice-form", "ice",
    "a creature of crystalline ice.",
    EQF_PHYSICAL, MR_RES_POISON | MR_VUL_FIRE | mrd(MR_RES_COLD, 3),
    FormDuration(30, PS_DOUBLE, 100), 0, 0, SIZE_LARGE, 12,
    5, 7, 0, true, 0, 10, 12,
    SPWPN_FREEZING, WHITE, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_ENABLE, FC_FORBID, true, false,
    "", 0, "front paw", "paw", "bow your head before", "ice"
},

{
    TRAN_DRAGON, MONS_PROGRAM_BUG, "Dragon", "dragon-form", "dragon",
    "a fearsome dragon!",
    EQF_PHYSICAL, MR_RES_POISON,
    DEFAULT_DURATION, 10, 0, SIZE_GIANT, 15,
    16, 0, 0, true, 0, 10, -1,
    SPWPN_NORMAL, GREEN, "Teeth and claws", { "hit", "claw", "bite", "maul" },
    FC_ENABLE, FC_FORBID, FC_ENABLE, true, false,
    "roar", 6, "foreclaw", "", "bow your head before", "flesh"
},

{
    TRAN_LICH, MONS_LICH, "Lich", "lich-form", "lich",
    "a lich.",
    EQF_NONE, MR_RES_COLD | mrd(MR_RES_NEG, 3),
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    6, 0, 0, true, 0, 10, 5,
    SPWPN_DRAINING, MAGENTA, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_FORBID, false, true,
    "", 0, "", "", "", "bone"
},

{
    TRAN_BAT, MONS_PROGRAM_BUG, "Bat", "bat-form", "bat",
    "",
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    DEFAULT_DURATION, 0, 5, SIZE_TINY, 10,
    0, 0, 0, false, 0, 12, -1,
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_ENABLE, FC_FORBID, FC_ENABLE, true, false,
    "squeak", -8, "foreclaw", "", "perch on", "flesh"
},

{
    TRAN_PIG, MONS_HOG, "Pig", "pig-form", "pig",
    "a filthy swine.",
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_SMALL, 10,
    0, 0, 0, false, 0, 0, 3,
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, FC_ENABLE, true, false,
    "squeal", 0, "front trotter", "trotter", "bow your head before", "flesh"
},

{
    TRAN_APPENDAGE, MONS_PLAYER, "App", "appendage", "appendage",
    "",
    EQF_NONE, MR_NO_FLAGS,
    FormDuration(10, PS_DOUBLE, 60), 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, 0, 3,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_DEFAULT, FC_DEFAULT, true, true,
    "", 0, "", "", "", ""
},

{
    TRAN_TREE, MONS_ANIMATED_TREE, "Tree", "tree-form", "tree",
    "a tree.",
    EQF_LEAR | SLOTF(EQ_CLOAK), MR_RES_POISON | mrd(MR_RES_NEG, 3),
    BAD_DURATION, 0, 0, SIZE_CHARACTER, 15,
    20, 0, 50, true, 0, 10, 12,
    SPWPN_NORMAL, BROWN, "Branches", { "hit", "smack", "pummel", "thrash" },
    FC_FORBID, FC_FORBID, FC_FORBID, false, false,
    "creak", 0, "branch", "root", "sway towards", "wood"
},

{
    TRAN_PORCUPINE, MONS_PORCUPINE, "Porc", "porcupine-form", "porcupine",
    "a spiny porcupine.",
    EQF_ALL, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    0, 0, 0, false, 0, 0, 3,
    SPWPN_NORMAL, LIGHTGREY, "Teeth", ANIMAL_VERBS,
    FC_DEFAULT, FC_FORBID, FC_ENABLE, true, false,
    "squeak", -8, "front leg", "", "curl into a sanctuary of spikes before", "flesh"
},

{
    TRAN_WISP, MONS_INSUBSTANTIAL_WISP, "Wisp", "wisp-form", "wisp",
    "an insubstantial wisp.",
    EQF_ALL, mrd(MR_RES_FIRE, 2) | mrd(MR_RES_COLD, 2) | MR_RES_ELEC
             | MR_RES_STICKY_FLAME | mrd(MR_RES_NEG, 3) | MR_RES_ACID
             | MR_RES_PETRIFY,
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    5, 0, 50, false, 0, 10, 5,
    SPWPN_NORMAL, LIGHTGREY, "Misty tendrils", { "touch", "hit",
                                                 "engulf", "engulf" },
    FC_ENABLE, FC_FORBID, FC_FORBID, false, false,
    "whoosh", -8, "misty tendril", "strand", "swirl around", "vapour"
},

#if TAG_MAJOR_VERSION == 34
{
    TRAN_JELLY, MONS_JELLY, "Jelly", "jelly-form", "jelly",
    "a lump of jelly.",
    EQF_PHYSICAL | EQF_RINGS, MR_NO_FLAGS,
    BAD_DURATION, 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, false, 0, 0, 3,
    SPWPN_NORMAL, LIGHTGREY, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, false, false,
    "", 0, "", "", "", ""
},
#endif

{
    TRAN_FUNGUS, MONS_WANDERING_MUSHROOM, "Fungus", "fungus-form", "fungus",
    "a sentient fungus.",
    EQF_PHYSICAL & ~SLOTF(EQ_HELMET), MR_RES_POISON | mrd(MR_RES_NEG, 3),
    BAD_DURATION, 0, 0, SIZE_TINY, 10,
    12, 0, 0, false, 0, 10, 12,
    SPWPN_CONFUSE, BROWN, "Spores", FormAttackVerbs("release spores at"),
    FC_DEFAULT, FC_FORBID, FC_FORBID, false, false,
    "sporulate", -8, "hypha", "", "release spores on", "flesh"
},

{
    TRAN_SHADOW, MONS_PLAYER_SHADOW, "Shadow", "shadow-form", "shadow",
    "a swirling mass of dark shadows.",
    EQF_NONE, mrd(MR_RES_POISON, 3) | mrd(MR_RES_NEG, 3) | MR_RES_ROTTING
                                                         | MR_RES_PETRIFY,
    DEFAULT_DURATION, 0, 0, SIZE_CHARACTER, 10,
    0, 0, 0, true, 0, 0, 3,
    SPWPN_NORMAL, MAGENTA, "", DEFAULT_VERBS,
    FC_DEFAULT, FC_FORBID, FC_FORBID, true, true,
    "", 0, "", "", "", "shadow"
},

{
    TRAN_HYDRA, MONS_HYDRA, "Hydra", "hydra-form", "hydra",
    "",
    EQF_PHYSICAL, MR_RES_POISON,
    FormDuration(10, PS_SINGLE, 100), 0, 0, SIZE_BIG, 13,
    6, 5, 0, true, 0, 10, -1,
    SPWPN_NORMAL, GREEN, "", { "nip at", "bite", "gouge", "chomp" },
    FC_DEFAULT, FC_ENABLE, FC_ENABLE, true, false,
    "roar", 4, "foreclaw", "", "bow your heads before", "flesh"
}
};
COMPILE_CHECK(ARRAYSZ(formdata) == NUM_TRANSFORMS);
