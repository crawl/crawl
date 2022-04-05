struct sacrifice_def
{
    ability_type  sacrifice;        // The ability that executes the sacrifice.
    mutation_type mutation;         // The mutation that will be inflicted.
                                    // See also god-abil.cc:sacrifice_vector_map.
    const char*   sacrifice_text;   // Format: "sacrifice your hand"
                                    // in case of variable sacrifices or sac
                                    // hand, this will be extended later
    const char*   milestone_text;   // Format: "sacrificed <foo>"
                                    // in case of variable sacrifices this will
                                    // be extended later
    int           base_piety;       // The piety that will be gained, modified
                                    // by the skill points in the skill below.
    skill_type    sacrifice_skill;  // This skill will be eliminated.
    const char*   sacrifice_vector; // This is used for sacrifices which give
                                    // multiple mutations. It is a key into
                                    // you.props, yielding a list of mutations
                                    // granted by the sacrifice.
    bool (*valid)();                // Whether the sacrifice is currently an
                                    // valid choice for Ru to offer. Only
                                    // checks factors specific to this
                                    // sacrifice, not general checks.
};

static const sacrifice_def sac_data[] =
{

{ ABIL_RU_SACRIFICE_PURITY, MUT_NON_MUTATION,
  "corrupt yourself such that",
  "sacrificed purity",

  0,
  SK_NONE,
  PURITY_SAC_KEY,
  nullptr,
},

{ ABIL_RU_SACRIFICE_WORDS, MUT_NO_READ,
  "sacrifice your ability to read while threatened",
  "sacrificed words",

  30,
  SK_NONE,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_DRINK, MUT_NO_DRINK,
  "sacrifice your ability to drink while threatened",
  "sacrificed drink",

  30,
  SK_NONE,
  nullptr,
  []() { return you.species != SP_MUMMY; },
},

{ ABIL_RU_SACRIFICE_ESSENCE, MUT_NON_MUTATION,
  "corrupt yourself such that",
  "sacrificed essence",

  0,
  SK_NONE,
  ESSENCE_SAC_KEY,
  nullptr,
},

{ ABIL_RU_SACRIFICE_HEALTH, MUT_NON_MUTATION,
  "corrupt yourself such that",
  "sacrificed health",

  25,
  SK_NONE,
  HEALTH_SAC_KEY,
  nullptr,
},

{ ABIL_RU_SACRIFICE_STEALTH, MUT_NO_STEALTH,
  "sacrifice your ability to go unnoticed",
  "sacrificed stealth",

  15,
  SK_STEALTH,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_ARTIFICE, MUT_NO_ARTIFICE,
  "sacrifice all use of magical tools",
  "sacrificed evocations",

  55,
  SK_EVOCATIONS,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_LOVE, MUT_NO_LOVE,
  "sacrifice your ability to be loved",
  "sacrificed love",

  35,
  SK_NONE,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_COURAGE, MUT_COWARDICE,
  "sacrifice your courage",
  "sacrificed courage",

  25,
  SK_NONE,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_ARCANA, MUT_NON_MUTATION,
  "sacrifice all use of",
  "sacrificed arcana",

  25,
  SK_NONE,
  ARCANA_SAC_KEY,
  []() { return !_player_sacrificed_arcana(); },
},

{ ABIL_RU_SACRIFICE_NIMBLENESS, MUT_NO_DODGING,
  "sacrifice your Dodging skill",
  "sacrificed dodging",

  30,
  SK_DODGING,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_DURABILITY, MUT_NO_ARMOUR,
  "sacrifice your Armour skill",
  "sacrificed armour",

  30,
  SK_ARMOUR,
  nullptr,
  []() { return you_can_wear(EQ_BODY_ARMOUR) != MB_FALSE; },
},

{ ABIL_RU_SACRIFICE_HAND, MUT_MISSING_HAND,
  "sacrifice one of your ",
  "sacrificed a hand",

  65,
  SK_SHIELDS,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_EXPERIENCE, MUT_INEXPERIENCED,
  "sacrifice your experiences",
  "sacrificed experience",

  50,
  SK_NONE,
  nullptr,
  []() { return you.experience_level > RU_SAC_XP_LEVELS; }
},

{ ABIL_RU_SACRIFICE_SKILL, MUT_UNSKILLED,
  "sacrifice your skill",
  "sacrificed skill",

  40,
  SK_NONE,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_EYE, MUT_MISSING_EYE,
  "sacrifice an eye",
  "sacrificed an eye",

  20,
  SK_NONE,
  nullptr,
  nullptr,
},

{ ABIL_RU_SACRIFICE_RESISTANCE, MUT_TEMPERATURE_SENSITIVITY,
  "sacrifice your resistance to extreme temperatures",
  "sacrificed resistance",

  60,
  SK_NONE,
  nullptr,
  nullptr,
},
};
