#pragma once

#include "tag-version.h"

struct mutation_def
{
    mutation_type mutation;
    short       weight;     ///< Commonality of the mutation; bigger = appears
                            /// more often.
    short       levels;     ///< The number of levels of the mutation.
    mutflags    flags;      ///< Bitfield holding information about sources of
                            /// this mutation and how it interacts with forms.
                            /// (mutflag::*)
    const char* short_desc; ///< What appears on the '%' screen.
    const char* have[3];    ///< What appears on the 'A' screen.
    const char* gain[3];    ///< Message when you gain the mutation.
    const char* lose[3];    ///< Message when you lose the mutation.
    tileidx_t   tile;       ///< Icon for the mutation.
    const char* will_gain[3]; ///< Message on the 'A' screen future XL gain of the mutation.
};

struct mutation_category_def
{
  mutation_type mutation;
  const char* short_desc;
};

static const mutation_def mut_data[] =
{

{ MUT_IRON_FUSED_SCALES, 0, 1, mutflag::good | mutflag::anatomy | mutflag::substance,
  "iron-fused scales",

  {"Your scales are fused with iron. (AC + 5)", "", ""},
  {"Iron fuses itself to your scales.", "", ""},
  {"The iron flakes away from your scales.", "", ""},
  0, {"Iron will fuse itself to your scales. (AC + 5)"}
},

{ MUT_INVIOLATE_MAGIC, 0, 1, mutflag::good,
  "inviolate magic",

  {"Your magical power and effects resist disruption.", "", ""},
  {"Your magical power grows resistant to disruption.", "", ""},
  {"Your magical power loses its resistance to disruption.", "", ""},
  TILEG_MUT_INVIOLATE_MAGIC,
  {"Your magical power will grow resistant to disruption."}
},

{ MUT_TOUGH_SKIN, 2, 3, mutflag::good | mutflag::anatomy | mutflag::substance,
  "tough skin",

  {"You have tough skin. (AC +1)",
   "You have very tough skin. (AC +2)",
   "You have extremely tough skin. (AC +3)"},

  {"Your skin toughens.",
   "Your skin toughens.",
   "Your skin toughens."},

  {"Your skin feels delicate.",
   "Your skin feels delicate.",
   "Your skin feels delicate."},
},

{ MUT_STRONG, 7, 2, mutflag::good,
  "strong",

  {"You are strong. (Str +4, Int/Dex -1)",
   "You are very strong. (Str +8, Int/Dex -2)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_STRONG,
},

{ MUT_CLEVER, 7, 2, mutflag::good,
  "clever",

  {"Your mind is acute. (Int +4, Str/Dex -1)",
   "Your mind is very acute. (Int +8, Str/Dex -2)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_CLEVER,
},

{ MUT_AGILE, 7, 2, mutflag::good,
  "agile",

  {"You are agile. (Dex +4, Str/Int -1)",
   "You are very agile. (Dex +8, Str/Int -2)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_AGILE,
},

{ MUT_POISON_RESISTANCE, 4, 1, mutflag::good | mutflag::substance,
  "poison resistance",

  {"You are resistant to poisons. (rPois)", "", ""},
  {"You feel resistant to poisons.", "",  ""},
  {"You feel less resistant to poisons.", "", ""},
  TILEG_MUT_POISON_RESISTANCE,
},

{ MUT_HEAT_RESISTANCE, 4, 3, mutflag::good | mutflag::substance,
  "fire resistance",

  {"You are heat resistant. (rF+)",
   "You are very heat resistant. (rF++)",
   "You are almost immune to the effects of heat. (rF+++)"},

  {"You feel resistant to heat.",
   "You feel more resistant to heat.",
   "You feel more resistant to heat."},

  {"You no longer feel heat resistant.",
   "You feel less heat resistant.",
   "You feel less heat resistant."},
  TILEG_MUT_HEAT_RESISTANCE,
},

{ MUT_COLD_RESISTANCE, 4, 3, mutflag::good | mutflag::substance,
  "cold resistance",

  {"You are cold resistant. (rC+)",
   "You are very cold resistant. (rC++)",
   "You are almost immune to the effects of cold. (rC+++)"},

  {"You feel resistant to cold.",
   "You feel more resistant to cold.",
   "You feel more resistant to cold."},

  {"You no longer feel cold resistant.",
   "You feel less cold resistant.",
   "You feel less cold resistant."},
  TILEG_MUT_COLD_RESISTANCE,
},

{ MUT_HEAT_VULNERABILITY, 3, 3, mutflag::bad | mutflag::substance,
  "heat vulnerability",

  {"You are vulnerable to heat. (rF-)",
   "You are very vulnerable to heat. (rF--)",
   "You are extremely vulnerable to heat. (rF---)"},

  {"You feel vulnerable to heat.",
   "You feel vulnerable to heat.",
   "You feel vulnerable to heat."},

  {"You no longer feel vulnerable to heat.",
   "You feel less vulnerable to heat.",
   "You feel less vulnerable to heat."},
  TILEG_MUT_HEAT_VULNERABILITY,
},

{ MUT_COLD_VULNERABILITY, 3, 3, mutflag::bad | mutflag::substance,

  "cold vulnerability",

  {"You are vulnerable to cold. (rC-)",
   "You are very vulnerable to cold. (rC--)",
   "You are extremely vulnerable to cold. (rC---)"},

  {"You feel vulnerable to cold.",
   "You feel vulnerable to cold.",
   "You feel vulnerable to cold."},

  {"You no longer feel vulnerable to cold.",
   "You feel less vulnerable to cold.",
   "You feel less vulnerable to cold."},
  TILEG_MUT_COLD_VULNERABILITY,
},

{ MUT_DEMONIC_GUARDIAN, 0, 3, mutflag::good,
  "demonic guardian",

  {"A weak demonic guardian rushes to your aid.",
   "A demonic guardian rushes to your aid.",
   "A powerful demonic guardian rushes to your aid."},

  {"You feel the presence of a demonic guardian.",
   "Your guardian grows in power.",
   "Your guardian grows in power."},

  {"Your demonic guardian is gone.",
   "Your demonic guardian is weakened.",
   "Your demonic guardian is weakened."},

  TILEG_MUT_DEMONIC_GUARDIAN,
},

{ MUT_SHOCK_RESISTANCE, 2, 1, mutflag::good | mutflag::substance,
  "electricity resistance",

  {"You are resistant to electric shocks. (rElec)", "", ""},
  {"You feel insulated.", "", ""},
  {"You feel conductive.", "", ""},
  TILEG_MUT_SHOCK_RESISTANCE,
},

{ MUT_SHOCK_VULNERABILITY, 2, 1, mutflag::bad | mutflag::substance,
  "electricity vulnerability",

  {"You are vulnerable to electric shocks. (rElec-)", "", ""},
  {"You feel vulnerable to electricity.", "", ""},
  {"You feel less vulnerable to electricity.", "", ""},
  TILEG_MUT_SHOCK_VULNERABILITY,
},

{ MUT_REGENERATION, 2, 3, mutflag::good,
  "regeneration",

  {"Your natural rate of healing is unusually fast.",
   "You heal very quickly.",
   "You regenerate."},

  {"You begin to heal more quickly.",
   "You begin to heal more quickly.",
   "You begin to regenerate."},

  {"Your rate of healing slows.",
   "Your rate of healing slows.",
   "Your rate of healing slows."},

  TILEG_MUT_REGENERATION,
},

{ MUT_INHIBITED_REGENERATION, 3, 1, mutflag::bad,
  "inhibited regeneration",

  {"You do not regenerate when monsters are visible.", "", ""},

  {"Your regeneration stops near monsters.", "", ""},

  {"You begin to regenerate regardless of the presence of monsters.", "", ""},

  TILEG_MUT_INHIBITED_REGENERATION,
},

{ MUT_FEED_OFF_SUFFERING, 4, 2, mutflag::good,
   "feed off suffering",

   {"You can sometimes gain power from killing poisoned or drained foes.",
    "You can regularly gain power from killing poisoned or drained foes.", ""},
   {"Vile energies swirl pleasantly through your soul.",
    "The vile energy whirls more vigorously through your soul.", ""},
   {"The vile energy swirling through your soul dissipates.",
    "The vile energy whirling through your soul weakens.", ""},
    TILEG_MUT_FEED_OFF_SUFFERING,
},

{ MUT_LUCKY, 4, 2, mutflag::good,
   "lucky",

   {"You discover slightly more artefacts.", "You discover more artefacts.", ""},
   {"You feel fortune smile upon you.", "You feel fortune smile more brightly upon you.", ""},
   {"You feel like your luck has run out.", "You feel a little less lucky.", ""},
    TILEG_MUT_LUCKY,
},

{ MUT_WEAK, 8, 2, mutflag::bad,
  "weak",

  {"You are weak. (Str -3)",
   "You are very weak. (Str -6)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_WEAK,
},

{ MUT_DOPEY, 8, 2, mutflag::bad,
  "dopey",

  {"You are dopey. (Int -3)",
   "You are very dopey. (Int -6)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_DOPEY,
},

{ MUT_CLUMSY, 8, 2, mutflag::bad,
  "clumsy",

  {"You are clumsy. (Dex -3)",
   "You are very clumsy. (Dex -6)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_CLUMSY,
},

{ MUT_MEEK, 3, 3, mutflag::bad,
  "meek",

  {"Your attacks are a little less effective. (Slay -3)",
   "Your attacks are less effective. (Slay -5)",
   "Your attacks are much less effective. (Slay -7)"},
  {"You feel meek.",
   "You feel even more meek.",
   "You feel meek as a lamb."},
  {"You feel aggression return to you.",
   "You feel a little less meek.",
   "You feel a little less meek."},
  TILEG_MUT_MEEK,
},

{ MUT_TELEPORTITIS, 3, 2, mutflag::bad,
  "teleportitis",

  {"You are occasionally teleported next to monsters.",
   "You are often teleported next to monsters.",
   ""},

  {"You feel weirdly uncertain.",
   "You feel even more weirdly uncertain.",
   ""},

  {"You feel stable.",
   "You feel stable.",
   ""},
  TILEG_MUT_TELEPORT,
},

{ MUT_PERSISTENT_DRAIN, 5, 1, mutflag::bad,
  "persistent drain",

  {"Your health recovers twice as slowly from being drained.", "", ""},

  {"You begin to recover more slowly from draining effects.", "", ""},

  {"You recover from draining at a normal speed again.", "", ""},
  TILEG_MUT_PERSISTENT_DRAIN,
},

{ MUT_CONTAMINATION_SUSCEPTIBLE, 0, 1, mutflag::bad,
  "contamination susceptible",

  {"You absorb twice as much mutagenic energy from being contaminated.", "", ""},
  {"You feel more susceptible to contamination.", "", ""},
  {"You feel less susceptible to contamination.", "", ""},
  TILEG_MUT_CONTAMINATION_SUSCEPTIBLE,
},

{ MUT_STRONG_WILLED, 5, 3, mutflag::good,
  "strong-willed",

  {"You are strong-willed. (Will+)",
   "You are highly strong-willed. (Will++)",
   "You are extremely strong-willed. (Will+++)"},

  {"You feel strong-willed.",
   "You feel more strong-willed.",
   "You feel your will become almost unbreakable."},

  {"You no longer feel strong-willed.",
   "You feel less strong-willed.",
   "You feel less strong-willed."},
  TILEG_MUT_STRONG_WILLED,
},

{ MUT_FAST, 0, 3, mutflag::good,
  "speed",

  {"You move with supernatural speed. (Speed+)",
   "You move with supernatural speed. (Speed++)",
   "You move with supernatural speed. (Speed+++)"},

  {"You feel quick.",
   "You feel quick.",
   "You feel quick."},

  {"You feel sluggish.",
   "You feel sluggish.",
   "You feel sluggish."},
},

{ MUT_SLOW, 0, 3, mutflag::bad,
  "slowness",

  {"You cover ground slowly.",
   "You cover ground very slowly.",
   "You cover ground extremely slowly."},

  {"You feel sluggish.",
   "You feel sluggish.",
   "You feel sluggish."},

  {"You feel quick.",
   "You feel quick.",
   "You feel quick."},
},

{ MUT_ACUTE_VISION, 2, 1, mutflag::good,
  "see invisible",

  {"You have supernaturally acute vision. (SInv)", "", ""},
  {"Your vision sharpens.", "", ""},
  {"Your vision seems duller.", "", ""},

  TILEG_MUT_ACUTE_VISION,
},

{ MUT_DEFORMED, 8, 1, mutflag::bad | mutflag::anatomy,
  "deformed body",

  {"Armour fits poorly on your strangely shaped body.", "", ""},
  {"Your body twists and deforms.", "", ""},
  {"Your body's shape seems more normal.", "", ""},
  TILEG_MUT_GENERIC_BAD_MUTATION,
},

// Naga only
{ MUT_SPIT_POISON, 8, 2, mutflag::good | mutflag::anatomy,
  "spit poison",

  {"You can spit poison.",
   "You can exhale a cloud of poison.",
   ""},

  {"There is a nasty taste in your mouth for a moment.",
   "There is a nasty taste in your mouth for a moment.",
   ""},

  {"You feel an ache in your throat.",
   "You feel an ache in your throat.",
   ""},
  TILEG_MUT_SPIT_POISON,
},

{ MUT_SCREAM, 6, 2, mutflag::bad,
  "screaming",

  {"You occasionally shout uncontrollably at those who injure you.",
   "You frequently scream uncontrollably at those who injure you.",
   ""},

  {"You feel the urge to shout.",
   "You feel a strong urge to scream.",
   ""},

  {"Your urge to shout disappears.",
   "Your urge to scream lessens.",
   ""},
  TILEG_MUT_SCREAM,
},

{ MUT_CLARITY, 6, 1, mutflag::good,
  "clarity",

  {"You possess an exceptional clarity of mind.", "", ""},
  {"Your thoughts seem clearer.", "", ""},
  {"Your thinking seems confused.", "", ""},
  TILEG_MUT_CLARITY
},

{ MUT_POOR_CONSTITUTION, 10, 2, mutflag::bad,
  "poor constitution",

  {"Your body sometimes grows weak upon taking damage.",
   "Your body sometimes grows weak and slow upon taking damage.",
   ""},

  {"You feel your constitution weaken.",
   "You feel your constitution grow even weaker.",
   ""},

  {"You feel your constitution return to normal.",
   "You feel your constitution improve a little.",
   ""},
  TILEG_MUT_POOR_CONSTITUTION,
},

{ MUT_MUTATION_RESISTANCE, 4, 3, mutflag::good,
  "mutation resistance",

  {"You are somewhat resistant to further mutation.",
   "You are somewhat resistant to both further mutation and mutation removal.",
   "You are almost entirely resistant to further mutation and mutation removal."},

  {"You feel genetically stable.",
   "You feel genetically stable.",
   "You feel genetically immutable."},

  {"You feel genetically unstable.",
   "You feel genetically unstable.",
   "You feel genetically unstable."},
  TILEG_MUT_MUTATION_RESISTANCE,
},

{ MUT_EVOLUTION, 4, 2, mutflag::good,
  "evolution",

  {"You have hidden genetic potential.",
   "You have great hidden genetic potential.",
   ""},

  {"You feel a hidden potential growing inside you.",
   "Your hidden genetic potential grows.",
   ""},

  {"You no longer feel a hidden potential within.",
   "Your hidden genetic potential wanes.",
   ""},
  TILEG_MUT_GENERIC_GOOD_MUTATION,
},

{ MUT_DEVOLUTION, 4, 2, mutflag::bad,
  "devolution",

  {"You have hidden genetic defects.",
   "You have terrible hidden genetic defects.",
   ""},

  {"You feel a hidden malignance growing inside you.",
   "The malignance inside you grows.",
   ""},

  {"You no longer feel a malignance within.",
   "Your genetic malignance weakens.",
   ""},
  TILEG_MUT_GENERIC_BAD_MUTATION,
},

{ MUT_PROTEAN_GRACE, 2, 1, mutflag::good,
   "protean grace",

   {"You are empowered by your non-innate mutations. (+", "", ""},
   {"Mutagenic power surges through your limbs.", "", ""},
   {"The mutagenic power surging through your limbs dissipates.", "", ""},
   TILEG_MUT_PROTEAN_GRACE,
 },

{ MUT_FRAIL, 10, 3, mutflag::bad,
  "frail",

  {"You are frail. (-10% HP)",
   "You are very frail. (-20% HP)",
   "You are extremely frail. (-30% HP)"},

  {"You feel frail.",
   "You feel frail.",
   "You feel frail."},

  {"You feel robust.",
   "You feel robust.",
   "You feel robust."},
  TILEG_MUT_GENERIC_BAD_MUTATION,
},

{ MUT_ROBUST, 5, 3, mutflag::good,
  "robust",

  {"You are robust. (+10% HP)",
   "You are very robust. (+20% HP)",
   "You are extremely robust. (+30% HP)"},

  {"You feel robust.",
   "You feel robust.",
   "You feel robust."},

  {"You feel frail.",
   "You feel frail.",
   "You feel frail."},

    TILEG_MUT_ROBUST,
},

{ MUT_TORMENT_RESISTANCE, 0, 2, mutflag::good | mutflag::substance,
  "torment resistance",

  {"You are resistant to unholy torment.",
   "You are immune to unholy pain and torment.", ""},
  {"You feel a strange anaesthesia.",
   "You feel a very strange anaesthesia.", ""},
  {"", "", ""},
  TILEG_MUT_TORMENT_RES,
},

{ MUT_NEGATIVE_ENERGY_RESISTANCE, 0, 3, mutflag::good | mutflag::substance,
  "negative energy resistance",

  {"You resist negative energy. (rN+)",
   "You are quite resistant to negative energy. (rN++)",
   "You are immune to negative energy. (rN+++)"},

  {"You feel resistant to negative energy.",
   "You feel more resistant to negative energy.",
   "You feel more resistant to negative energy."},

  {"", "", ""},
  TILEG_MUT_NEGATIVE_ENERGY_RESISTANCE,
},

{ MUT_NECRO_ENHANCER, 0, 2, mutflag::good,
  "in touch with death",

  {"You are in touch with the powers of death.",
   "You are strongly in touch with the powers of death.",
   ""},

  {"You feel more in touch with the powers of death.",
   "You feel more in touch with the powers of death.",
   ""},

  {"", "", ""},
  TILEG_MUT_IN_TOUCH_WITH_DEATH,
},

{ MUT_TENGU_FLIGHT, 0, 1, mutflag::good,
  "evasive flight",

  {"Your magical flight helps you evade attacks. (EV +4)",
   "", ""},
  {"Your magical nature develops, letting you fly evasively.",
   "", ""},
  {"", "", ""},
  0,
  {"Your magical flight will help you evade attacks. (EV + 4)"}
},

{ MUT_ACROBATIC, 0, 1, mutflag::good,
  "acrobatic",

  {"You can magically evade attacks while moving or waiting.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_ACROBATIC,
},

{ MUT_WIELD_OFFHAND, 0, 1, mutflag::good,
  "off-hand wielding",

  {"You can wield a second weapon in your off-hand.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_SLOW_WIELD, 0, 1, mutflag::bad,
  "slow wielding",

  {"It takes a long time for you to wield or remove held weapons.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_INITIALLY_ATTRACTIVE, 4, 2, mutflag::bad,
  "initially attractive",

  {"You sometimes pull newly seen creatures towards you.",
   "You frequently pull newly seen creatures towards you.",
   ""},
  {"You feel strangely attractive.",
   "You feel even more attractive.",
    ""},
  {"You feel less attractive.",
   "You feel a little less attractive.",
   ""},
  TILEG_MUT_INITIALLY_ATTRACTIVE,
},

{ MUT_WARMUP_STRIKES, 0, 1, mutflag::bad | mutflag::anatomy,
  "warmup strikes",

  {"Your first few attacks do less damage.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_NO_JEWELLERY, 0, 1, mutflag::bad,
  "no jewellery",

  {"You cannot equip rings or amulets.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_HURL_DAMNATION, 0, 1, mutflag::good,
  "hurl damnation",

  {"You can hurl damnation.", "", ""},
  {"You smell a hint of brimstone.", "", ""},
  {"", "", ""},
  TILEG_MUT_HURL_DAMNATION,
},

// body-slot facets
{ MUT_HORNS, 7, 3, mutflag::good | mutflag::anatomy,
  "horns",

  {"You have a pair of small horns on your head.",
   "You have a pair of horns on your head.",
   "You have a pair of large horns on your head."},

  {"A pair of horns grows on your head!",
   "The horns on your head grow some more.",
   "The horns on your head grow some more."},

  {"The horns on your head shrink away.",
   "The horns on your head shrink a bit.",
   "The horns on your head shrink a bit."},
  TILEG_MUT_HORNS,
},

{ MUT_BEAK, 1, 1, mutflag::good | mutflag::anatomy,
  "beak",

  {"You have a beak for a mouth.", "", ""},
  {"Your mouth lengthens and hardens into a beak!", "", ""},
  {"Your beak shortens and softens into a mouth.", "", ""},
  TILEG_MUT_BEAK,
},

{ MUT_CLAWS, 2, 3, mutflag::good | mutflag::anatomy | mutflag::need_hands,
  "claws",

  {"You have sharp fingernails.",
   "You have very sharp fingernails.",
   "You have claws for hands."},

  {"Your fingernails lengthen.",
   "Your fingernails sharpen.",
   "Your hands twist into claws."},

  {"Your fingernails shrink to normal size.",
   "Your fingernails look duller.",
   "Your hands feel fleshier."},
  TILEG_MUT_CLAWS,
},

{ MUT_FANGS, 1, 3, mutflag::good | mutflag::anatomy,
  "fangs",

  {"You have very sharp teeth.",
   "You have extremely sharp teeth.",
   "You have razor-sharp teeth."},

  {"Your teeth lengthen and sharpen.",
   "Your teeth lengthen and sharpen some more.",
   "Your teeth grow very long and razor-sharp."},

  {"Your teeth shrink to normal size.",
   "Your teeth shrink and become duller.",
   "Your teeth shrink and become duller."},
  TILEG_MUT_FANGS,
},

{ MUT_HOOVES, 5, 3, mutflag::good | mutflag::anatomy | mutflag::need_feet,
  "hooves",

  {"You have large cloven feet.",
   "You have hoof-like feet.",
   "You have hooves in place of feet."},

  {"Your feet thicken and deform.",
   "Your feet thicken and deform.",
   "Your feet have mutated into hooves."},

  {"Your hooves expand and flesh out into feet!",
   "Your hooves look more like feet.",
   "Your hooves look more like feet."},
  TILEG_MUT_HOOVES,
},

{ MUT_ANTENNAE, 4, 3, mutflag::good | mutflag::anatomy,
  "antennae",

  {"You have a pair of small antennae on your head.",
   "You have a pair of antennae on your head.",
   "You have a pair of large antennae on your head. (SInv)"},

  {"A pair of antennae grows on your head!",
   "The antennae on your head grow some more.",
   "The antennae on your head grow some more."},

  {"The antennae on your head shrink away.",
   "The antennae on your head shrink a bit.",
   "The antennae on your head shrink a bit."},

  TILEG_MUT_ANTENNAE,
},

{ MUT_TALONS, 5, 3, mutflag::good | mutflag::anatomy | mutflag::need_feet,
  "talons",

  {"You have sharp toenails.",
   "You have razor-sharp toenails.",
   "You have claws for feet."},

  {"Your toenails lengthen and sharpen.",
   "Your toenails lengthen and sharpen.",
   "Your feet stretch into talons."},

  {"Your talons dull and shrink into feet.",
   "Your talons look more like feet.",
   "Your talons look more like feet."},
  TILEG_MUT_TALONS,
},

// Octopode only
{ MUT_TENTACLE_SPIKE, 10, 3, mutflag::good | mutflag::anatomy,
  "tentacle spike",

  {"One of your tentacles bears a spike.",
   "One of your tentacles bears a nasty spike.",
   "One of your tentacles bears a large vicious spike."},

  {"One of your lower tentacles grows a sharp spike.",
   "Your tentacle spike grows bigger.",
   "Your tentacle spike grows even bigger."},

  {"Your tentacle spike disappears.",
   "Your tentacle spike becomes smaller.",
   "Your tentacle spike recedes somewhat."},
  TILEG_MUT_GENERIC_GOOD_MUTATION,
},

{ MUT_CONSTRICTING_TAIL, 0, 2, mutflag::good | mutflag::anatomy,
  "naga tail",

  {"Your snake-like lower body moves slowly.",
   "Your snake-like lower body moves slowly, but can constrict enemies.", ""},
  {"Your lower body turns into a snake tail.",
   "Your tail grows strong enough to constrict your enemies.", ""},
  {"Your lower body returns to normal.",
   "Your snake tail weakens and can no longer constrict your enemies.", ""},
   TILEG_MUT_NAGA_TAIL,
  {0, "Your tail will grow strong enough to constrict your foes."},
},

{ MUT_STINGER, 8, 3, mutflag::good | mutflag::anatomy,
  "stinger",

  {"Your tail ends in a venomous barb.",
   "Your tail ends in a sharp venomous barb.",
   "Your tail ends in a wickedly sharp and venomous barb."},

  {"A venomous barb forms on the end of your tail.",
   "The barb on your tail looks sharper.",
   "The barb on your tail looks very sharp."},

  {"The barb on your tail disappears.",
   "The barb on your tail seems less sharp.",
   "The barb on your tail seems less sharp."},
  TILEG_MUT_STINGER,
},

// Draconian/gargoyle only
{ MUT_BIG_WINGS, 4, 1, mutflag::good | mutflag::anatomy,
  "big wings",

  {"Your large and strong wings let you fly.", "", ""},
  {"Your wings grow larger and stronger.", "", ""},
  {"Your wings shrivel and weaken.", "", ""},
  TILEG_MUT_BIG_WINGS,
},

{ MUT_FROG_LEGS, 0, 2, mutflag::good | mutflag::anatomy,
  "frog-like legs",

  {"You can hop short distances, but move slowly.",
   "You can hop long distances, but move slowly.",
   ""},

  {"", "Your legs feel stronger.", ""},

  {"", "", ""},

  TILEG_MUT_HOP,
},

{ MUT_ARMOURED_TAIL, 0, 2, mutflag::good | mutflag::anatomy,
  "armoured tail",

  {"You have an armoured tail.", "You have a heavy armoured tail.", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_STAMPEDE, 0, 2, mutflag::good,
  "stampede",

  {"You stampede swiftly towards enemies and can push them backward.",
    "You stampede swiftly towards enemies and can push them backward.", ""},
  {"You become able to stampede towards enemies.",
   "Your stampede becomes supported by the winds.", ""},
  {"You can no longer stampede toward enemies.", "", ""},

  TILEG_MUT_STAMPEDE,

  {0, "One of the Four Winds will empower you as you Stampede.", 0}
},

{ MUT_SHAGGY_FUR, 2, 3, mutflag::good | mutflag::anatomy,
  "shaggy fur",

  {"You are covered in fur. (AC +1)",
   "You are covered in thick fur. (AC +2)",
   "Your thick and shaggy fur keeps you warm. (AC +3, rC+)"},

  {"Fur sprouts all over your body.",
   "Your fur grows into a thick mane.",
   "Your thick fur grows shaggy and warm."},

  {"You shed all your fur.",
   "Your thick fur recedes somewhat.",
   "Your shaggy fur recedes somewhat."},
  TILEG_MUT_SHAGGY_FUR,

  {0, 0, "Your shaggy fur will keep you warm. (AC + 3, rC+)"}
},

{ MUT_HIGH_MAGIC, 2, 3, mutflag::good,
  "high MP",

  {"You have an increased reservoir of magic. (+10% MP)",
   "You have a considerably increased reservoir of magic. (+20% MP)",
   "You have a greatly increased reservoir of magic. (+30% MP)"},

  {"You feel more energetic.",
   "You feel more energetic.",
   "You feel more energetic."},

  {"You feel less energetic.",
   "You feel less energetic.",
   "You feel less energetic."},
  TILEG_MUT_HIGH_MP,
},

{ MUT_LOW_MAGIC, 9, 3, mutflag::bad,
  "low MP",

  {"Your magical capacity is low. (-10% MP)",
   "Your magical capacity is very low. (-20% MP)",
   "Your magical capacity is extremely low. (-30% MP)"},

  {"You feel less energetic.",
   "You feel less energetic.",
   "You feel less energetic."},

  {"You feel more energetic.",
   "You feel more energetic.",
   "You feel more energetic."},
  TILEG_MUT_LOW_MP,
},

{ MUT_WILD_MAGIC, 6, 3, mutflag::good,
  "wild magic",

  {"Your spells are a little harder to cast, but a little more powerful.",
   "Your spells are harder to cast, but more powerful.",
   "Your spells are much harder to cast, but much more powerful."},

  {"You feel less in control of your magic.",
   "You feel less in control of your magic.",
   "You feel your magical power running wild!"},

  {"You regain control of your magic.",
   "You feel more in control of your magic.",
   "You feel more in control of your magic."},
  TILEG_MUT_WILD_MAGIC,
},

{ MUT_SUBDUED_MAGIC, 6, 3, mutflag::bad,
  "subdued magic",

  {"Your spells are a little easier to cast, but a little less powerful.",
   "Your spells are easier to cast, but less powerful.",
   "Your spells are much easier to cast, but much less powerful."},

  {"Your connection to magic feels subdued.",
   "Your connection to magic feels more subdued.",
   "Your connection to magic feels nearly dormant."},

  {"Your magic regains its normal vibrancy.",
   "Your connection to magic feels less subdued.",
   "Your connection to magic feels less subdued."},
  TILEG_MUT_SUBDUED_MAGIC,
},

{ MUT_EFFICIENT_MAGIC, 4, 2, mutflag::good,
  "efficient magic",

  {"Spells you cast cost 1 less MP (to a minimum of 1).",
   "Spells you cast cost 2 less MP (to a minimum of 1).",
   ""},
  {"You gain a new grip on the flow of your magic.",
   "Your grip strengthens on the flow of your magic.",
   ""},
  {"The flow of your magic slips from your grasp.",
   "The flow of your magic starts slipping from your grasp.",
   ""},
  TILEG_MUT_EFFICIENT_MAGIC,
},

{ MUT_EPHEMERAL_SHIELD, 4, 1, mutflag::good,
  "ephemeral shield",

  {"A shield forms around you when casting spells or using Invocations. (SH +7)",
    "", ""},
  {"Ambient excess energy starts collecting around you.", "", ""},
  {"The ambient excess energy around you dissipates.", "", ""},
  TILEG_MUT_EPHEMERAL_SHIELD,
},

{ MUT_TIME_WARPED_BLOOD, 2, 2, mutflag::good | mutflag::need_blood,
  "time-warped blood",

  {"Your blood hastes a few of your allies when you are sufficiently damaged.",
   "Your blood hastes several of your allies when you are sufficiently damaged.",
   ""},
  {"The flow of your blood desyncs from the flow of time itself.",
   "The flow of your blood has become unstuck from time.",
   ""},
  {"Your blood flow once more matches the normal passage of time.",
   "Your blood flow starts aligning itself with the normal passage of time.",
   ""},
  TILEG_MUT_TIME_WARPED_BLOOD,
},

{ MUT_DEMONIC_MAGIC, 0, 3, mutflag::good,
  "demonic magic",

  {"Spells you cast may paralyse adjacent enemies.",
   "Spells you cast may paralyse nearby enemies.",
   "Spells you cast and wands you use may paralyse nearby enemies."},

  {"A menacing aura infuses your magic.",
   "Your magic grows more menacing.",
   "Your wands become infused with your menacing aura."},

  {"","",""},
  TILEG_MUT_DEMONIC_MAGIC,
},

{ MUT_FORLORN, 0, 1, mutflag::bad,
  "forlorn",

  {"You shall have no god before yourself.","",""},
  {"You feel forlorn.","",""},
  {"You feel more spiritual.","",""},
  TILEG_MUT_FORLORN,
},

{ MUT_PASSIVE_MAPPING, 3, 2, mutflag::good,
  "sense surroundings",

  {"You passively map the area around you.",
   "You passively map a large area around you.", ""},

  {"You feel a strange attunement to the structure of the dungeons.",
   "Your attunement to dungeon structure grows further.", ""},

  {"You feel slightly disoriented.",
   "You feel slightly disoriented.", ""},

  TILEG_MUT_PASSIVE_MAPPING,
},

{ MUT_ICEMAIL, 0, 2, mutflag::good,
  "icemail",

  {"A meltable icy envelope protects you from harm. (AC +",
   "A thick, meltable icy envelope protects you from harm. (AC +", ""},
  {"An icy envelope takes form around you.",
   "Your icy envelope grows thicker.", ""},
  {"", "", ""},

  TILEG_MUT_ICEMAIL,
},

{ MUT_CONDENSATION_SHIELD, 0, 1, mutflag::good,
  "condensation shield",

  {"A meltable shield of frost defends you. (SH +", "", ""},
  {"Frost condenses into a shield before you.","", ""},
  {"", "", ""},

  TILEG_MUT_CONDENSATION_SHIELD,
},

{ MUT_PASSIVE_FREEZE, 1, 1, mutflag::good,
  "passive freeze",

  {"A frigid envelope surrounds you and freezes all who hurt you.", "", ""},
  {"Your skin feels very cold.", "", ""},
  {"Your skin warms up.", "", ""},

  TILEG_MUT_PASSIVE_FREEZE,
},

{ MUT_NIGHTSTALKER, 0, 3, mutflag::good,
  "nightstalker",

  {"You are slightly more attuned to the shadows.",
   "You are significantly more attuned to the shadows.",
   "You are completely attuned to the shadows."},

  {"You slip into the darkness of the dungeon.",
   "You slip further into the darkness.",
   "You are surrounded by darkness."},

  {"Your affinity for the darkness vanishes.",
   "Your affinity for the darkness weakens.",
   "Your affinity for the darkness weakens."},

  TILEG_MUT_NIGHTSTALKER,
},

{ MUT_SPINY, 0, 3, mutflag::good | mutflag::anatomy,
  "spiny",

  {"You are partially covered in sharp spines.",
   "You are mostly covered in sharp spines.",
   "You are completely covered in sharp spines."},

  {"Sharp spines emerge from parts of your body.",
   "Sharp spines emerge from more of your body.",
   "Sharp spines emerge from your entire body."},

  {"Your sharp spines disappear entirely.",
   "Your sharp spines retract somewhat.",
   "Your sharp spines retract somewhat."},
  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_POWERED_BY_DEATH, 0, 3, mutflag::good,
  "powered by death",

  {"You regenerate a little health from kills.",
   "You regenerate health from kills.",
   "You regenerate a lot of health from kills."},

  {"A wave of death washes over you.",
   "The wave of death grows in power.",
   "The wave of death grows in power."},

  {"Your control of surrounding life forces is gone.",
   "Your control of surrounding life forces weakens.",
   "Your control of surrounding life forces weakens."},

  TILEG_MUT_POWERED_BY_DEATH,
},

{ MUT_POWERED_BY_PAIN, 0, 3, mutflag::good,
  "powered by pain",

  {"You sometimes gain a little power by taking damage.",
   "You sometimes gain power by taking damage.",
   "You are powered by pain."},

  {"You feel energised by your suffering.",
   "You feel even more energised by your suffering.",
   "You feel completely energised by your suffering."},

  {"", "", ""},

  TILEG_MUT_POWERED_BY_PAIN,
},

{ MUT_AUGMENTATION, 0, 3, mutflag::good,
  "augmentation",

  {"Your magical and physical power is slightly enhanced at high health.",
   "Your magical and physical power is enhanced at high health.",
   "Your magical and physical power is greatly enhanced at high health."},

  {"You feel power flowing into your body.",
   "You feel power rushing into your body.",
   "You feel saturated with power."},

  {"", "", ""},
  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_MANA_SHIELD, 0, 1, mutflag::good,
  "magic shield",

  {"When hurt, damage is shared between your health and your magic reserves.", "", ""},
  {"You feel your magical essence form a protective shroud around your flesh.", "", ""},
  {"", "", ""},

  TILEG_MUT_MANA_SHIELD,
},

{ MUT_MANA_REGENERATION, 0, 1, mutflag::good,
  "magic regeneration",

  {"You regenerate magic rapidly.", "", ""},
  {"Your magic begins to regenerate rapidly.", "", ""},
  {"", "", ""},

  TILEG_MUT_MANA_REGENERATION,
},

{ MUT_MANA_LINK, 0, 1, mutflag::good,
  "magic link",

  {"When low on magic, you restore magic in place of health.", "", ""},
  {"You feel your life force and your magical essence meld.", "", ""},
  {"", "", ""},

  TILEG_MUT_MANA_LINK,
},

// Jiyva only mutations. (MUT_GELATINOUS_BODY is also used by Op)
{ MUT_GELATINOUS_BODY, 0, 3, mutflag::good | mutflag::jiyva | mutflag::substance,
  "gelatinous body",

  {"Your rubbery body absorbs attacks. (AC +1, EV +1)",
   "Your pliable body absorbs attacks. (AC +2, EV +2)",
   "Your gelatinous body deflects attacks. (AC +3, EV +3)"},

  {"Your body becomes stretchy.",
   "Your body becomes more malleable.",
   "Your body becomes viscous."},

  {"Your body returns to its normal consistency.",
   "Your body becomes less malleable.",
   "Your body becomes less viscous."},
  TILEG_MUT_GELATINOUS_BODY,
},

{ MUT_EYEBALLS, 0, 3, mutflag::good | mutflag::jiyva | mutflag::anatomy,
  "eyeballs",

  {"Your body has grown eyes which may confuse attackers. (Acc +3)",
   "Your body has grown many eyes which may confuse attackers. (Acc +5)",
   "Your body is covered in eyes which may confuse attackers. (Acc +7, SInv)"},

  {"Eyeballs grow over part of your body.",
   "Eyeballs cover a large portion of your body.",
   "Eyeballs cover you completely."},

  {"The eyeballs on your body disappear.",
   "The eyeballs on your body recede somewhat.",
   "The eyeballs on your body recede somewhat."},
  TILEG_MUT_EYEBALLS,
},

{ MUT_TRANSLUCENT_SKIN, 0, 3, mutflag::good | mutflag::jiyva | mutflag::substance,
  "translucent skin",

  {"Your translucent skin slightly reduces your foes' accuracy. (Stealth+)",
   "Your translucent skin reduces your foes' accuracy. (Stealth+)",
   "Your transparent skin significantly reduces your foes' accuracy. (Stealth+)"},

  {"Your skin becomes partially translucent.",
   "Your skin becomes more translucent.",
   "Your skin becomes completely transparent."},

  {"Your skin returns to its normal opacity.",
   "Your skin's translucency fades.",
   "Your skin's transparency fades."},
  TILEG_MUT_GENERIC_JIYVA_MUTATION,
},

{ MUT_PSEUDOPODS, 0, 3, mutflag::good | mutflag::jiyva | mutflag::anatomy,
  "pseudopods",

  {"Armour fits poorly on your pseudopods.",
   "Armour fits poorly on your large pseudopods.",
   "Armour fits poorly on your massive pseudopods."},

  {"Pseudopods emerge from your body.",
   "Your pseudopods grow in size.",
   "Your pseudopods grow in size."},

  {"Your pseudopods retract into your body.",
   "Your pseudopods become smaller.",
   "Your pseudopods become smaller."},
  TILEG_MUT_PSEUDOPODS,
},

{ MUT_ACIDIC_BITE, 0, 1, mutflag::good | mutflag::jiyva | mutflag::anatomy,
  "acidic bite",

  {"You have acidic saliva.", "", ""},
  {"Acid begins to drip from your mouth.", "", ""},
  {"Your mouth feels dry.", "", ""},
  TILEG_MUT_ACIDIC_BITE,
},

{ MUT_SLIME_SHROUD, 0, 1, mutflag::good | mutflag::jiyva,
  "slime shroud",

  {"A fragile, slimy shroud covers you, deflecting attacks.", "", ""},
  {"A thin film of slime covers your body.", "", ""},
  {"Your film of slime dries up.", "", ""},
  TILEG_MUT_SLIME_SHROUD,
},

{ MUT_ANTIMAGIC_BITE, 0, 1, mutflag::good | mutflag::anatomy,
  "antimagic bite",

  {"Your bite disrupts and absorbs the magic of your enemies.", "", ""},
  {"You feel a sudden thirst for magic.", "", ""},
  {"Your magical appetite wanes.", "", ""},
  TILEG_MUT_ANTIMAGIC_BITE,
},

{ MUT_EFFICIENT_METABOLISM, 3, 1, mutflag::good,
   "efficient metabolism",

   {"Your metabolism doubles the duration of potion status effects.", "", ""},
   {"Your body churns, leaving you strangely parched.", "", ""},
   {"Your body churns, leaving you strangely quenched.", "", ""},
   TILEG_MUT_EFFICIENT_METABOLISM,
 },

{ MUT_NO_POTION_HEAL, 3, 2, mutflag::bad,
  "no potion heal",

  {"Potions are less effective at restoring your health.",
   "Potions cannot restore your health.",
   ""},

  {"Your system partially rejects the healing effects of potions.",
   "Your system completely rejects the healing effects of potions.",
   ""},

  {"Your system completely accepts the healing effects of potions.",
   "Your system partly accepts the healing effects of potions.",
   ""},

  TILEG_MUT_NO_POTION_HEAL,
},

{ MUT_RECKLESS, 2, 1, mutflag::bad,
   "reckless",

   {"Your SH is halved, but you deal more damage with two-handed weapons.", "", ""},
   {"You feel a sudden disregard for your own safety.", "", ""},
   {"You feel a little less reckless.", "", ""},
   TILEG_MUT_RECKLESS,
},

{ MUT_SPATIAL_ENTANGLEMENT, 2, 1, mutflag::bad,
  "spatial entanglement",

  {"When you translocate, a monster is sometimes pulled along with you.", "", ""},
  {"You feel space start to knot itself around you." "", ""},
  {"You feel space untangle itself from you.", "", ""},
  TILEG_MUT_SPATIAL_ENTANGLEMENT,
},

// Scale mutations
{ MUT_DISTORTION_FIELD, 0, 3, mutflag::good,
  "repulsion field",

  {"You are surrounded by a mild repulsion field. (EV +2)",
   "You are surrounded by a moderate repulsion field. (EV +3)",
   "You are surrounded by a strong repulsion field. (EV +4, RMsl)"},

  {"You begin to radiate repulsive energy.",
   "Your repulsive radiation grows stronger.",
   "Your repulsive radiation grows stronger."},

  {"You feel less repulsive.",
   "You feel less repulsive.",
   "You feel less repulsive."},
  TILEG_MUT_GENERIC_GOOD_MUTATION,
},

{ MUT_ICY_BLUE_SCALES, 0, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "icy blue scales",

  {"You are partially covered in icy blue scales. (AC +2)",
   "You are mostly covered in icy blue scales. (AC +3)",
   "You are completely covered in icy blue scales. (AC +4, rC+)"},

  {"Icy blue scales grow over part of your body.",
   "Icy blue scales spread over more of your body.",
   "Icy blue scales cover your body completely."},

  {"Your icy blue scales disappear.",
   "Your icy blue scales recede somewhat.",
   "Your icy blue scales recede somewhat."},

    TILEG_MUT_ICY_BLUE_SCALES
},

{ MUT_IRIDESCENT_SCALES, 10, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "iridescent scales",

  {"You are partially covered in iridescent scales. (AC +2)",
   "You are mostly covered in iridescent scales. (AC +4)",
   "You are completely covered in iridescent scales. (AC +6)"},

  {"Iridescent scales grow over part of your body.",
   "Iridescent scales spread over more of your body.",
   "Iridescent scales cover your body completely."},

  {"Your iridescent scales disappear.",
   "Your iridescent scales recede somewhat.",
   "Your iridescent scales recede somewhat."},

    TILEG_MUT_IRIDESCENT_SCALES
},

{ MUT_LARGE_BONE_PLATES, 2, 3, mutflag::good | mutflag::substance | mutflag::anatomy,
  "large bone plates",

  {"You are partially covered in large bone plates. (SH +4)",
   "You are mostly covered in large bone plates. (SH +6)",
   "You are completely covered in large bone plates. (SH +8)"},

  {"Large bone plates grow over parts of your arms.",
   "Large bone plates spread over more of your arms.",
   "Large bone plates cover your arms completely."},

  {"Your large bone plates disappear.",
   "Your large bone plates recede somewhat.",
   "Your large bone plates recede somewhat."},

    TILEG_MUT_LARGE_BONE_PLATES,
},

{ MUT_MOLTEN_SCALES, 0, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "molten scales",

  {"You are partially covered in molten scales. (AC +2)",
   "You are mostly covered in molten scales. (AC +3)",
   "You are completely covered in molten scales. (AC +4, rF+)"},

  {"Molten scales grow over part of your body.",
   "Molten scales spread over more of your body.",
   "Molten scales cover your body completely."},

  {"Your molten scales disappear.",
   "Your molten scales recede somewhat.",
   "Your molten scales recede somewhat."},

    TILEG_MUT_MOLTEN_SCALES
},

{ MUT_RUGGED_BROWN_SCALES, 0, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "rugged brown scales",

  {"You are partially covered in rugged brown scales. (AC +1, +3% HP)",
   "You are mostly covered in rugged brown scales. (AC +2, +5% HP)",
   "You are completely covered in rugged brown scales. (AC +3, +7% HP)"},

  {"Rugged brown scales grow over part of your body.",
   "Rugged brown scales spread over more of your body.",
   "Rugged brown scales cover your body completely."},

  {"Your rugged brown scales disappear.",
   "Your rugged brown scales recede somewhat.",
   "Your rugged brown scales recede somewhat."},

    TILEG_MUT_RUGGED_BROWN_SCALES,
},

{ MUT_SLIMY_GREEN_SCALES, 0, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "slimy green scales",

  {"You are partially covered in slimy green scales. (AC +2)",
   "You are mostly covered in slimy green scales. (AC +3)",
   "You are completely covered in slimy green scales. (AC +4, rPois)"},

  {"Slimy green scales grow over part of your body.",
   "Slimy green scales spread over more of your body.",
   "Slimy green scales cover your body completely."},

  {"Your slimy green scales disappear.",
   "Your slimy green scales recede somewhat.",
   "Your slimy green scales recede somewhat."},

    TILEG_MUT_SLIMY_GREEN_SCALES
},

{ MUT_THIN_METALLIC_SCALES, 0, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "thin metallic scales",

  {"You are partially covered in thin metallic scales. (AC +2)",
   "You are mostly covered in thin metallic scales. (AC +3)",
   "You are completely covered in thin metallic scales. (AC +4, rElec)"},

  {"Thin metallic scales grow over part of your body.",
   "Thin metallic scales spread over more of your body.",
   "Thin metallic scales cover your body completely."},

  {"Your thin metallic scales disappear.",
   "Your thin metallic scales recede somewhat.",
   "Your thin metallic scales recede somewhat."},

    TILEG_MUT_THIN_METALLIC_SCALES
},

{ MUT_THIN_SKELETAL_STRUCTURE, 2, 3, mutflag::good | mutflag::need_bones,
  "thin skeletal structure",

  {"You have a somewhat thin skeletal structure. (Dex +2, Stealth+)",
   "You have a moderately thin skeletal structure. (Dex +4, Stealth++)",
   "You have an unnaturally thin skeletal structure. (Dex +6, Stealth+++)"},

  {"Your bones become slightly less dense.",
   "Your bones become somewhat less dense.",
   "Your bones become less dense."},

  {"Your skeletal structure returns to normal.",
   "Your skeletal structure densifies.",
   "Your skeletal structure densifies."},

    TILEG_MUT_THIN_SKELETAL_STRUCTURE,
},

{ MUT_YELLOW_SCALES, 0, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "yellow scales",

  {"You are partially covered in yellow scales. (AC +2)",
   "You are mostly covered in yellow scales. (AC +3)",
   "You are completely covered in yellow scales. (AC +4, rCorr)"},

  {"Yellow scales grow over part of your body.",
   "Yellow scales spread over more of your body.",
   "Yellow scales cover your body completely."},

  {"Your yellow scales disappear.",
   "Your yellow scales recede somewhat.",
   "Your yellow scales recede somewhat."},

    TILEG_MUT_YELLOW_SCALES
},

{ MUT_SHARP_SCALES, 0, 3, mutflag::good | mutflag::substance | mutflag::anatomy | mutflag::scales,
  "sharp scales",

  {"You are partially covered in razor-sharp scales. (AC +1, Slay +1)",
   "You are mostly covered in razor-sharp scales. (AC +2, Slay +2)",
   "You are completely covered in razor-sharp scales. (AC +3, Slay +3)"},

  {"Sharp scales grow over part of your body.",
   "Sharp scales spread over more of your body.",
   "Sharp scales cover your body completely."},

  {"Your sharp scales disappear.",
   "Your sharp scales recede somewhat.",
   "Your sharp scales recede somewhat."},

    TILEG_MUT_SHARP_SCALES,
},

{ MUT_STURDY_FRAME, 2, 3, mutflag::good,
  "sturdy frame",

  {"Your movements are slightly less encumbered by armour. (ER -2)",
   "Your movements are less encumbered by armour. (ER -4)",
   "Your movements are significantly less encumbered by armour. (ER -6)"},

  {"You feel less encumbered by your armour.",
   "You feel less encumbered by your armour.",
   "You feel less encumbered by your armour."},

  {"You feel more encumbered by your armour.",
   "You feel more encumbered by your armour.",
   "You feel more encumbered by your armour."},

  TILEG_MUT_GENERIC_GOOD_MUTATION,
},

{ MUT_SANGUINE_ARMOUR, 0, 3, mutflag::good | mutflag::need_blood,
  "sanguine armour",

  {"When seriously injured, your blood forms armour. (AC +",
   "When seriously injured, your blood forms thick armour. (AC +",
   "When seriously injured, your blood forms very thick armour. (AC +"},

  {"You feel your blood ready itself to protect you.",
   "You feel your blood thicken.",
   "You feel your blood thicken."},

  {"You feel your blood become entirely quiescent.",
   "You feel your blood thin.",
   "You feel your blood thin."},

  TILEG_MUT_SANGUINE_ARMOUR,
},

{ MUT_BIG_BRAIN, 0, 3, mutflag::good,
  "big brain",

  {"You have an unusually large brain. (Int +2)",
   "You have an extremely huge brain. (Int +4)",
   "You have an absolutely massive brain. (Int +6, Wiz)"},

  {"Your brain expands.",
   "Your brain expands.",
   "Your brain expands to incredible size."},

  {"Your brain returns to normal size.",
   "Your brain shrinks.",
   "Your brain shrinks."},

    TILEG_MUT_BIG_BRAIN,
},

{ MUT_CAMOUFLAGE, 1, 3, mutflag::good | mutflag::substance,
  "camouflage",

  {"Your skin changes colour to match your surroundings (Stealth+).",
   "Your skin blends seamlessly with your surroundings (Stealth++).",
   "Your skin perfectly mimics your surroundings (Stealth+++)."},

  {"Your skin functions as natural camouflage.",
   "Your natural camouflage becomes more effective.",
   "Your natural camouflage becomes more effective."},

  {"Your skin no longer functions as natural camouflage.",
   "Your natural camouflage becomes less effective.",
   "Your natural camouflage becomes less effective."},

  TILEG_MUT_CAMOUFLAGE,
},

{ MUT_IGNITE_BLOOD, 0, 3, mutflag::good,
  "ignite blood",

  {"Your demonic aura sometimes causes spilled blood to erupt in flames.",
   "Your demonic aura often causes spilled blood to erupt in flames.",
   "Your demonic aura causes all spilled blood to erupt in flames."},
  {"Your blood heats up.",
   "Your blood runs red-hot!",
   "Your blood burns even hotter!"},
  {"", "", ""},
  TILEG_MUT_IGNITE_BLOOD,
},

{ MUT_FOUL_STENCH, 0, 3, mutflag::good,
  "foul stench",

  {"You may rarely emit foul miasma when damaged in melee.",
   "You sometimes emit foul miasma when damaged in melee.",
   "You frequently emit foul miasma when damaged in melee."},

  {"You begin to emit a foul stench of rot and decay.",
   "Your foul stench grows more powerful.",
   "You begin to radiate miasma.",
  },

  {"", "", ""},
  TILEG_MUT_FOUL_STENCH,
},

{ MUT_TENDRILS, 0, 1, mutflag::good | mutflag::jiyva | mutflag::anatomy,
  "tendrils",

  {"You are covered in slimy tendrils that may disarm your opponents.", "", ""},
  {"Thin, slimy tendrils emerge from your body.", "", ""},
  {"Your tendrils retract into your body.", "", ""},
  TILEG_MUT_TENDRILS,
},

{ MUT_JELLY_GROWTH, 0, 1, mutflag::good | mutflag::jiyva,
  "jelly sensing items",

  {"You have a small jelly attached to you that senses nearby items.", "", ""},
  {"Your body partially splits into a small jelly.", "", ""},
  {"The jelly growth is reabsorbed into your body.", "", ""},
  TILEG_MUT_JELLY_GROWTH,
},

{ MUT_JELLY_MISSILE, 0, 1, mutflag::good | mutflag::jiyva,
  "jelly absorbing missiles",

  {"You have a small jelly attached to you that may absorb projectiles.", "", ""},
  {"Your body partially splits into a small jelly.", "", ""},
  {"The jelly growth is reabsorbed into your body.", "", ""},
  TILEG_MUT_JELLY_ABSORBING_MISSILES,
},

{ MUT_STONE_BODY, 0, 1, mutflag::good | mutflag::substance,
  "stone body",

  {"Your stone body is resiliant and immune to petrification. (AC +", "", ""},
  {"Your body vibrates.", "", ""},
  {"You briefly stop moving.", "", ""},
  TILEG_MUT_PETRIFICATION_RES,
},

{ MUT_BLACK_MARK, 0, 1, mutflag::good,
  "black mark",

  {"Your melee attacks may debilitate your foes.", "", ""},
  {"An ominous black mark forms on your body.", "", ""},
  {"", "", ""},
  TILEG_MUT_BLACK_MARK,
},

{ MUT_SILENCE_AURA, 0, 1, mutflag::good,
  "aura of silence",

  {"You are surrounded by an aura of silence.", "", ""},
  {"An unnatural silence shrouds you.", "", ""},
  {"", "", ""},
  TILEG_MUT_SILENCE_AURA,
},

{ MUT_HEX_ENHANCER, 0, 1, mutflag::good,
  "bedevilling",

  {"Your hexes are more powerful.", "", ""},
  {"You feel devilish.", "", ""},
  {"", "", ""},
  TILEG_MUT_BEDEVILLING,
},

{ MUT_CORRUPTING_PRESENCE, 0, 2, mutflag::good,
  "corrupting presence",

  {"Your presence sometimes corrodes those you injure.",
   "Your presence sometimes corrodes or deforms those you injure.", ""},
  {"You feel corrupt.", "Your corrupting presence intensifies.", ""},
  {"", "", ""},
  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_WORD_OF_CHAOS, 0, 1, mutflag::good,
  "word of chaos",

  {"You can speak a Word of Chaos.", "", ""},
  {"Your tongue twists.", "", ""},
  {"", "", ""},
  TILEG_MUT_WORD_OF_CHAOS,
},

{ MUT_DEMONIC_WILL, 0, 1, mutflag::good,
  "demonic willpower",

  {"You punish those that try to bend your will. (Will+)", "", ""},
  {"You feel wilful.", "", ""},
  {"", "", ""},
  TILEG_MUT_DEMONIC_WILL,
},

{ MUT_WEAKNESS_STINGER, 0, 3, mutflag::good | mutflag::anatomy,
  "weakness stinger",

  {"You have a small tail.",
   "You have a tail ending in a sharp stinger.",
   "You have a sharp stinger which inflicts weakening toxins."},

  {"You grow a small tail.",
   "Your tail grows a sharp stinger.",
   "Your stinger grows larger and begins to produce weakening toxins."},

  {"", "", ""},
  TILEG_MUT_WEAKNESS_STINGER,
},

{ MUT_DEMONIC_TOUCH, 0, 3, mutflag::good | mutflag::anatomy | mutflag::need_hands,
  "demonic touch",

  {"Your touch may inflict minor irresistible damage on your foes.",
   "Your touch may inflict irresistible damage on your foes.",
   "Your touch may irresistibly damage your foes and sap their willpower."},

  {"Your hands begin to faintly glow with unholy energy.",
   "Your hands glow brighter with unholy energy.",
   "Your hands twist and begin to emit a powerful aura of unholy energy."},

  {"", "", ""},
  TILEG_MUT_DEMONIC_TOUCH,
},

{ MUT_COLD_BLOODED, 0, 1, mutflag::bad | mutflag::need_blood,
  "cold-blooded",

  {"You are cold-blooded and may be slowed by cold attacks.", "", ""},
  {"You feel cold-blooded.", "", ""},
  {"You feel warm-blooded.", "", ""},
  TILEG_MUT_COLD_BLOODED,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_FLAME_CLOUD_IMMUNITY, 0, 1, mutflag::good,
  "flame cloud immunity",

  {"You are immune to clouds of flame.", "", ""},
  {"You feel less concerned about heat.", "", ""},
  {"", "", ""},
},

{ MUT_FREEZING_CLOUD_IMMUNITY, 0, 1, mutflag::good,
  "freezing cloud immunity",

  {"You are immune to freezing clouds.", "", ""},
  {"You feel less concerned about cold.", "", ""},
  {"", "", ""},
},
#endif

{ MUT_RENOUNCE_POTIONS, 0, 1, mutflag::bad,
  "renounce potions",

  {"You refuse to drink potions in combat unless seriously injured.", "", ""},
  {"You renounce the hedonism of uninhibited drinking.", "", ""},
  {"You feel willing to use your potions freely once again.", "", ""},
  TILEG_MUT_RENOUNCE_POTIONS,
},

{ MUT_RENOUNCE_SCROLLS, 0, 1, mutflag::bad,
  "renounce scrolls",

  {"You refuse to read scrolls in combat unless seriously injured.", "", ""},
  {"You renounce the frivolity of worldly writings.", "", ""},
  {"You feel willing to use your scrolls freely once again."},
  TILEG_MUT_RENOUNCE_SCROLLS,
},

{ MUT_MISSING_HAND, 0, 1, mutflag::bad,
  "missing a hand",

  {"You are missing a hand.", "", ""},
  {"One of your hands has vanished, leaving only a stump!", "", ""},
  {"Your stump has regrown into a hand!", "", ""},
  TILEG_MUT_MISSING_HAND,
},

{ MUT_BOOMING_VOICE, 3, 1, mutflag::bad,
  "booming voice",

  {"You read scrolls incredibly loudly when enemies are in sight.", "", ""},
  {"You feel your voice swell to a thunderous volume.", "", ""},
  {"You voice quiets to a reasonable volume.", "", ""},
  TILEG_MUT_BOOMING_VOICE,
},

{ MUT_POTION_FUNGUS, 4, 1, mutflag::bad,
  "potion fungus",

  {"You may spread the effect of potions you drink to nearby monsters.", "", ""},
  {"A symbiotic fungus spreads across your body.", "", ""},
  {"The fungus across your body shrivels and falls away.", "", ""},
  TILEG_MUT_POTION_FUNGUS,
},

{ MUT_NO_STEALTH, 0, 1, mutflag::bad,
  "no stealth",

  {"You cannot be stealthy.", "", ""},
  {"You can no longer be stealthy.", "", ""},
  {"You can once more be stealthy.", "", ""},
  TILEG_MUT_NO_STEALTH,
},

{ MUT_NO_ARTIFICE, 0, 1, mutflag::bad,
  "inability to use devices",

  {"You cannot study or use magical devices.", "", ""},
  {"You can no longer study or use magical devices.", "", ""},
  {"You can once more study and use magical devices.", "", ""},
},

{ MUT_NO_LOVE, 0, 1, mutflag::bad,
  "hated by all",

  {"You are hated by all.", "", ""},
  {"You are now hated by all.", "", ""},
  {"You are no longer hated by all.", "", ""},
},

{ MUT_COWARDICE, 0, 1, mutflag::bad,
  "cowardly",

  {"Your cowardice makes you less effective in combat with threatening foes.", "", ""},
  {"You have lost your courage.", "", ""},
  {"You have regained your courage.", "", ""},
},

{ MUT_NO_DODGING, 0, 1, mutflag::bad,
  "inability to train dodging",

  {"You cannot train Dodging skill.", "", ""},
  {"You can no longer train Dodging skill.", "", ""},
  {"You can once more train Dodging skill.", "", ""},
  TILEG_MUT_NO_DODGING,
},

{ MUT_NO_ARMOUR_SKILL, 0, 1, mutflag::bad,
  "inability to train armour",

  {"You cannot train Armour skill.", "", ""},
  {"You can no longer train Armour skill.", "", ""},
  {"You can once more train Armour skill.", "", ""},
  TILEG_MUT_NO_ARMOUR_SKILL,
},

{ MUT_NO_AIR_MAGIC, 0, 1, mutflag::bad,
  "no air magic",

  {"You cannot study or cast Air magic.", "", ""},
  {"You can no longer study or cast Air magic.", "", ""},
  {"You can once more study and cast Air magic.", "", ""},
  TILEG_MUT_NO_AIR_MAGIC,
},

{ MUT_NO_CONJURATION_MAGIC, 0, 1, mutflag::bad,
  "no conjurations magic",

  {"You cannot study or cast Conjurations magic.", "", ""},
  {"You can no longer study or cast Conjurations magic.", "", ""},
  {"You can once more study and cast Conjurations magic.", "", ""},
},

{ MUT_NO_EARTH_MAGIC, 0, 1, mutflag::bad,
  "no earth magic",

  {"You cannot study or cast Earth magic.", "", ""},
  {"You can no longer study or cast Earth magic.", "", ""},
  {"You can once more study and cast Earth magic.", "", ""},
  TILEG_MUT_NO_EARTH_MAGIC,
},

{ MUT_NO_FIRE_MAGIC, 0, 1, mutflag::bad,
  "no fire magic",

  {"You cannot study or cast Fire magic.", "", ""},
  {"You can no longer study or cast Fire magic.", "", ""},
  {"You can once more study and cast Fire magic.", "", ""},
},

{ MUT_NO_HEXES_MAGIC, 0, 1, mutflag::bad,
  "no hexes magic",

  {"You cannot study or cast Hexes magic.", "", ""},
  {"You can no longer study or cast Hexes magic.", "", ""},
  {"You can once more study and cast Hexes magic.", "", ""},
  TILEG_MUT_NO_HEXES_MAGIC,
},

{ MUT_NO_ICE_MAGIC, 0, 1, mutflag::bad,
  "no ice magic",

  {"You cannot study or cast Ice magic.", "", ""},
  {"You can no longer study or cast Ice magic.", "", ""},
  {"You can once more study and cast Ice magic.", "", ""},
  TILEG_MUT_NO_ICE_MAGIC,
},

{ MUT_NO_NECROMANCY_MAGIC, 0, 1, mutflag::bad,
  "no necromancy magic",

  {"You cannot study or cast Necromancy magic.", "", ""},
  {"You can no longer study or cast Necromancy magic.", "", ""},
  {"You can once more study and cast Necromancy magic.", "", ""},
  TILEG_MUT_NO_NECROMANCY_MAGIC,
},

{ MUT_NO_ALCHEMY_MAGIC, 0, 1, mutflag::bad,
  "no alchemy magic",

  {"You cannot study or cast Alchemy magic.", "", ""},
  {"You can no longer study or cast Alchemy magic.", "", ""},
  {"You can once more study and cast Alchemy magic.", "", ""},
  TILEG_MUT_NO_ALCHEMY_MAGIC,
},

{ MUT_NO_FORGECRAFT_MAGIC, 0, 1, mutflag::bad,
  "no forgecraft magic",

  {"You cannot study or cast Forgecraft magic.", "", ""},
  {"You can no longer study or cast Forgecraft magic.", "", ""},
  {"You can once more study and cast Forgecraft magic.", "", ""},
  TILEG_MUT_NO_FORGECRAFT_MAGIC,
},

{ MUT_NO_SUMMONING_MAGIC, 0, 1, mutflag::bad,
  "no summoning magic",

  {"You cannot study or cast Summoning magic.", "", ""},
  {"You can no longer study or cast Summoning magic.", "", ""},
  {"You can once more study and cast Summoning magic.", "", ""},
  TILEG_MUT_NO_SUMMONING_MAGIC,
},

{ MUT_NO_TRANSLOCATION_MAGIC, 0, 1, mutflag::bad,
  "no translocations magic",

  {"You cannot study or cast Translocations magic.", "", ""},
  {"You can no longer study or cast Translocations magic.", "", ""},
  {"You can once more study and cast Translocations magic.", "", ""},
  TILEG_MUT_NO_TRANSLOCATION_MAGIC,
},

{ MUT_PHYSICAL_VULNERABILITY, 0, 3, mutflag::bad,
  "reduced AC",

  {"You take slightly more damage. (AC -5)",
    "You take more damage. (AC -10)",
    "You take considerably more damage. (AC -15)"},
  {"You feel more vulnerable to harm.",
    "You feel more vulnerable to harm.",
    "You feel more vulnerable to harm."},
  {"You no longer feel extra vulnerable to harm.",
    "You feel less vulnerable to harm.",
    "You feel less vulnerable to harm."},
},

{ MUT_SLOW_REFLEXES, 0, 3, mutflag::bad,
  "reduced EV",

  {"You have somewhat slow reflexes. (EV -5)",
    "You have slow reflexes. (EV -10)",
    "You have very slow reflexes. (EV -15)"},
  {"Your reflexes slow.",
    "Your reflexes slow further.",
    "Your reflexes slow further."},
  {"You reflexes return to normal.",
    "You reflexes speed back up.",
    "You reflexes speed back up."},
},

{ MUT_WEAK_WILLED, 0, 3, mutflag::bad,
  "weak-willed",

  {"You are slightly weak-willed. (Will-)",
    "You are weak-willed. (Will--)",
    "You are extremely weak-willed. (Will---)"},
  {"You feel weak-willed.",
    "You feel more weak-willed.",
    "You feel more weak-willed."},
  {"You no longer feel weak-willed.",
    "You feel less weak-willed.",
    "You feel less weak-willed."},
  TILEG_MUT_WEAK_WILLED,
},

{ MUT_ANTI_WIZARDRY, 0, 3, mutflag::bad,
  "disrupted magic",

  {"Your casting is slightly disrupted.",
    "Your casting is disrupted.",
    "Your casting is seriously disrupted."},
  {"Your ability to control magic is disrupted.",
    "Your ability to control magic is more disrupted.",
    "Your ability to control magic is more disrupted."},
  {"Your ability to control magic is no longer disrupted.",
    "Your ability to control magic is less disrupted.",
    "Your ability to control magic is less disrupted."},
},

{ MUT_MP_WANDS, 7, 1, mutflag::good,
  "MP-powered wands",

  {"You expend magic power (3 MP) to strengthen your wands.", "", ""},
  {"You feel your magical essence link to the dungeon's wands.", "", ""},
  {"Your magical essence no longer links to wands of the dungeon.", "", ""},
  TILEG_MUT_MP_WANDS,
},

{ MUT_UNSKILLED, 0, 3, mutflag::bad,
  "unskilled",

  {"You are somewhat unskilled. (-1 Apt)",
    "You are unskilled. (-2 Apt)",
    "You are extremely unskilled. (-3 Apt)"},
  {"You feel less skilled.",
    "You feel less skilled.",
    "You feel less skilled."},
  {"You regain all your skill.",
    "You regain some skill.",
    "You regain some skill."},
},

{ MUT_INEXPERIENCED, 0, 3, mutflag::bad,
    "inexperienced",

    {"You are somewhat inexperienced. (-1 XL)",
     "You are inexperienced. (-2 XL)",
     "You are extremely inexperienced. (-3 XL)"},
    {"You feel less experienced.",
     "You feel less experienced.",
     "You feel less experienced."},
    {"You regain all your potential.",
     "You regain some potential.",
     "You regain some potential."},
},

{ MUT_PAWS, 0, 1, mutflag::good | mutflag::anatomy,
  "stealthy paws",

  {"Your paws help you pounce on unaware monsters.", "", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_PAWS,
},

{ MUT_MISSING_EYE, 0, 1, mutflag::bad,
  "missing an eye",

  {"You are missing an eye, making it more difficult to aim.", "", ""},
  {"Your right eye vanishes! The world loses its depth.", "", ""},
  {"Your right eye suddenly reappears! The world regains its depth.", "", ""},
  TILEG_MUT_MISSING_EYE,
},

{ MUT_TEMPERATURE_SENSITIVITY, 0, 1, mutflag::bad,
  "temperature sensitive",

  {"You are sensitive to extremes of temperature. (rF-, rC-)", "", ""},
  {"You feel sensitive to extremes of temperature.", "", ""},
  {"You no longer feel sensitive to extremes of temperature", "", ""},
  TILEG_MUT_TEMPERATURE_SENSITIVITY,
},


{ MUT_NO_FORMS, 0, 1, mutflag::bad,
  "no forms",

  {"You cannot voluntarily change form.", "", ""},
  {"You can no longer voluntarily change form.", "", ""},
  {"You can once more change form.", "", ""},
  TILEG_MUT_NO_FORMS,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_NO_REGENERATION, 0, 1, mutflag::bad,
  "no regeneration",

  {"You do not regenerate.", "", ""},
  {"You stop regenerating.", "", ""},
  {"You start regenerating.", "", ""},
},
#endif

{ MUT_TREASURE_SENSE, 0, 1, mutflag::good,
  "treasure sense",

  {"You have an uncanny knack for detecting items.", "", ""},
  {"You suddenly feel aware of treasure.", "", ""},
  {"You no longer feel aware of treasure.", "", ""},
},

{ MUT_ACID_RESISTANCE, 0, 1, mutflag::good | mutflag::substance,
  "acid resistance",

  {"You are resistant to acid. (rCorr)", "", ""},
  {"You feel resistant to acid.", "",  ""},
  {"You feel less resistant to acid.", "", ""},
  TILEG_MUT_ACID_RES,
},

{ MUT_QUADRUMANOUS, 0, 1, mutflag::good,
  "four strong arms",

  {"Your four strong arms can wield two-handed weapons with a shield.", "", ""},
  {"Two of your arms shrink away.", "", ""},
  {"You grow two extra arms.", "", ""},
},

{ MUT_NO_DRINK, 0, 1, mutflag::bad,
  "no potions",

  {"You cannot drink.", "", ""},
  {"Your mouth dries to ashes.", "", ""},
  {"You gain the ability to drink.", "", ""},
  TILEG_MUT_NO_DRINK,
},

{ MUT_FAITH, 0, 1, mutflag::good,
  "faith",

  {"You have a special connection with the divine. (Faith)", "", ""},
  {"You feel connected to something greater than you.", "", ""},
  {"You feel rebellious.", "", ""},
  TILEG_MUT_FAITH,
},

{ MUT_REFLEXIVE_HEADBUTT, 0, 1, mutflag::good | mutflag::anatomy,
  "retaliatory headbutt",

  {"You reflexively headbutt those who attack you in melee.", "", ""},
  {"Your retaliatory reflexes feel sharp.", "", ""},
  {"Your retaliatory reflexes feel dull.", "", ""},
},

{ MUT_STEAM_RESISTANCE, 0, 1, mutflag::good | mutflag::substance,
  "steam resistance",

  {"You are immune to the effects of steam.", "", ""},
  {"You are now immune to the effects of steam.", "", ""},
  {"You are no longer immune to the effects of steam.", "", ""},
  TILEG_MUT_STEAM_RES,
},

{ MUT_NO_GRASPING, 0, 1, mutflag::bad,
  "no weapons or thrown items",

  {"You are incapable of wielding weapons or throwing items.", "", ""},
  {"You can no longer grasp objects.", "", ""},
  {"You can now grasp objects.", "", ""},
  TILEG_MUT_NO_GRASPING,
},

{ MUT_NO_ARMOUR, 0, 1, mutflag::bad,
  "no armour",

  {"You cannot wear armour.", "", ""},
  {"You can no longer wear armour.", "", ""},
  {"You can now wear armour.", "", ""},
  TILEG_MUT_NO_ARMOUR,
},

{ MUT_MULTILIVED, 0, 1, mutflag::good,
  "multi-lived",

  {"You gain extra lives every three experience levels.", "", ""},
  {"You are no longer multi-lived.", "", ""},
  {"You can now gain extra lives.", "", ""},
  TILEG_MUT_MULTILIVED,
},

{ MUT_DISTRIBUTED_TRAINING, 0, 1, mutflag::good,
  "distributed training",

  {"Your experience applies equally to all skills.", "", ""},
  {"Your experience now applies equally to all skills.", "", ""},
  {"Your experience no longer applies equally to all skills.", "", ""},
},

{ MUT_NIMBLE_SWIMMER, 0, 2, mutflag::good | mutflag::anatomy,
  "nimble swimmer",

  {"You are camouflaged when in or above water. (Stealth+)",
   "You are very nimble when in or above water. (Stealth+, EV+, Speed+++)", ""},
  {"You feel comfortable near water.",
   "You feel very comfortable near water.", ""},
  {"You feel less comfortable near water.",
   "You feel less comfortable near water.", ""},
},

{ MUT_TENTACLE_ARMS, 0, 1, mutflag::good | mutflag::anatomy,
  "tentacles",

  {"You have tentacles for arms and can constrict enemies.", "", ""},
  {"Your arms feel tentacular.", "", ""},
  {"Your arms no longer feel tentacular.", "", ""},
},

{ MUT_MERTAIL, 0, 1, mutflag::good | mutflag::anatomy,
  "mertail",

  {"Your lower body shifts to a powerful aquatic tail in water.", "", ""},
  {"Your legs feel aquatic.", "", ""},
  {"Your legs no longer feel aquatic."},

  TILEG_MUT_MERTAIL,
},

{ MUT_FLOAT, 0, 1, mutflag::good,
  "float",

  {"You float through the air rather than walking.", "", ""},
  {"You feel both weightless and legless.", "", ""},
  {"You feel dragged down by the weight of the world."},
  TILEG_MUT_FLOAT,
},

{ MUT_INNATE_CASTER, 0, 1, mutflag::good,
  "innate caster",

  {"You learn spells naturally, not from books.", "", ""},
  {"You feel mystic power welling inside you.", "", ""},
  {"You feel a greater respect for book-learning."},
},

{ MUT_HP_CASTING, 0, 1, mutflag::good,
  "HP casting",

  {"Your magical power is your life essence.", "", ""},
  {"Your magical power and health merge together.", "", ""},
  {"Your life and magic unlink."},
  TILEG_MUT_HP_CASTING,
},

// XX why does this have 3 levels, only 1 is used
{ MUT_FLAT_HP, 0, 3, mutflag::good,
  "extra vitality",

    {"You have superior vitality. (+4 MHP)",
     "You have much superior vitality. (+8 MHP)",
     "You have exceptionally superior vitality. (+12 MHP)"},
    {"You feel more vital.",
     "You feel more vital.",
     "You feel more vital."},
    {"You feel less vital.",
     "You feel less vital.",
     "You feel less vital."},

  TILEG_MUT_FLAT_HP,
},

{ MUT_OOZE_FLOOD, 0, 1, mutflag::good | mutflag::jiyva | mutflag::substance,
  "ooze flood",

    {"Your melee attacks may flood your foes with ooze.", "", ""},
    {"You begin exuding ooze.", "", ""},
    {"You stop exuding ooze.", "", ""},
  TILEG_MUT_ENGULF,
},

// Sadly console size restrictions prevent more than one level of this existing
{ MUT_DAYSTALKER, 0, 1, mutflag::good,
  "+LOS",

    {"You have an extended range of vision and can be seen from far away.",
      "", ""},
    {"The darkness flees at your approach.", "", ""},
    {"The shadows grow bolder once more.", "", ""},
},

{ MUT_DIVINE_ATTRS, 0, 1, mutflag::good,
  "divine attributes",
  {"Your divine heritage dramatically boosts your attributes as you level up.", "", ""},
  {"You feel more divine.", "", ""},
  {"You feel more mortal.", "", ""},
  TILEG_MUT_DIVINE_ATTRIBUTES,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_DEVOUR_ON_KILL, 0, 1, mutflag::good,
  "devour on kill",
  {"You thrive by killing the living.", "", ""},
  {"You feel hungry for flesh.", "", ""},
  {"You feel less hungry for flesh.", "", ""},
},
#endif

{ MUT_SHORT_LIFESPAN, 0, 1, mutflag::bad,
  "otherworldly",
  {"You are easily found by Zot.", "", ""},
  {"You feel your time running out.", "", ""},
  {"You feel long-lived.", "", ""},
},

{ MUT_FOUL_SHADOW, 0, 3, mutflag::good,
  "foul shadow",
  {"You are faintly shadowed, very rarely releasing foul flame when damaged in melee.",
   "You are shadowed, sometimes releasing foul flame when damaged in melee.",
   "You are darkly shadowed, frequently releasing foul flame when damaged in melee."},
  {"Your body darkens with foul flame.",
   "Your body becomes darker with foul flame.",
   "Your body becomes darker with foul flame."},
  {"Your body's darkness fades completely.",
   "Your body's darkness fades.",
   "Your body's darkness fades."},

  TILEG_MUT_FOUL_SHADOW,
},

{ MUT_EXPLORE_REGEN, 0, 1, mutflag::good,
  "explore regen",
  {"You regain HP and MP as you explore.", "", ""},
  {"You feel a fierce wanderlust.", "", ""},
  {"You feel like a homebody.", "", ""},
  TILEG_MUT_EXPLORE_REGEN,
},

{ MUT_DOUBLE_POTION_HEAL, 0, 1, mutflag::good,
  "double potion healing",
  {"You gain doubled healing and magic from potions.", "", ""},
  {"You heal twice as much from potions.", "", ""},
  {"You no longer heal twice as much from potions.", "", ""},
  TILEG_MUT_DOUBLE_POTION_HEAL,
},

{ MUT_DRUNKEN_BRAWLING, 0, 1, mutflag::good,
  "drunken brawling",
  {"Whenever you drink a healing potion, you attack all around you.", "", ""},
  {"You brawl whenever you drink a healing potion.", "", ""},
  {"You no longer brawl whenever you drink a healing potion.", "", ""},
},

{ MUT_ARTEFACT_ENCHANTING, 0, 1, mutflag::good,
  "artefact enchanting",
  {"You can use scrolls of enchantment on lesser artefacts.", "", ""},
  {"You can now use scrolls of enchantment on lesser artefacts.", "", ""},
  {"You can no longer use scrolls of enchantment on lesser artefacts.", "", ""},
},

{ MUT_RUNIC_MAGIC, 0, 1, mutflag::good,
  "runic magic",
  {"Your spellcasting is much less encumbered by armour.", "", ""},
  {"Your spellcasting becomes less encumbered by armour.", "", ""},
  {"Your spellcasting no longer less encumbered by armour.", "", ""},
},

{ MUT_FORMLESS, 0, 2, mutflag::good,
  "formless",

  {"You can equip up to 6 pieces of aux armour in any combination.",
   "You can equip up to 6 pieces of aux armour in any combination and unleash them.",
   ""},
  {"", "You feel ready to unleash a true cacophony.", ""},
  {"", "", ""},
  TILEG_MUT_FORMLESS,
  {"", "You will be able to unleash your equipped armour.", ""},
},

{ MUT_TRICKSTER, 0, 1, mutflag::good,
  "trickster",

  {"You gain AC when you inflict magical misfortune on nearby enemies.", "", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_TRICKSTER,
},

{ MUT_MNEMOPHAGE, 0, 1, mutflag::good,
   "mnemnophage",
   {"You can enhance your damage-dealing spells by burning harvested memories.", ""},
   {"Your flames flicker hungrily.", "", ""},
   {"Your flames grow less ravenous.", "", ""},
   TILEG_MUT_MNEMOPHAGE,
   {"You will be able to burn memories to enhance your damage-dealing spells."}
 },

{ MUT_SPELLCLAWS, 0, 1, mutflag::good,
   "spellclaws",
   {"You perform a melee attack whenever you cast damage-dealing spells.", "", ""},
   {"You feel destructive magic coursing through your claws.", "", ""},
   {"You no longer feel destructive magic coursing through your claws.", "", ""},
   TILEG_MUT_SPELLCLAWS,
},

{ MUT_ACCURSED, 0, 1, mutflag::bad,
  "accursed",

  {"You recover more slowly from Doom and Banes.", "", ""},
  {"You feel accursed.", "", ""},
  {"You feel less accursed.", "", ""},
  TILEG_MUT_ACCURSED,
},

{ MUT_SPITEFUL_BLOOD, 2, 2, mutflag::bad | mutflag::need_blood,
  "spiteful blood",

  {"Your shed blood may rise up to attack you.",
   "Your shed blood may rise up to attack you as a group.", ""},
  {"You feel a willful anger gathering in your blood.",
   "You feel your blood grow even more spiteful.", ""},
  {"Your blood calms down again.",
   "Your blood feels a little less spiteful.", ""},
  TILEG_MUT_SPITEFUL_BLOOD,
},

{ MUT_NORTH_WIND, 0, 1, mutflag::good,
  "North Wind's embodiment",

  {"The North Wind lets you shrug off injuries taken while stampeding.", "", ""},
  {"You feel the indomitable North Wind bolstering your fortitude.", "", ""},
  {"The winds are changing....", "", ""},
  TILEG_MUT_STAMPEDE_NORTH,
},

{ MUT_SOUTH_WIND, 0, 1, mutflag::good,
  "South Wind's embodiment",

  {"The South Wind quickens your rampage movements after stabbing.", "", ""},
  {"You feel the capricious South Wind quickening your steps.", "", ""},
  {"The winds are changing....", "", ""},
  TILEG_MUT_STAMPEDE_SOUTH,
},

{ MUT_WEST_WIND, 0, 1, mutflag::good,
  "West Wind's embodiment",

  {"The West Wind empowers your ranged attacks as you Rampage.", "", ""},
  {"You feel the generous West Wind guiding your aim.", "", ""},
  {"The winds are changing....", "", ""},
  TILEG_MUT_STAMPEDE_WEST,
},

{ MUT_EAST_WIND, 0, 1, mutflag::good,
  "East Wind's embodiment",

  {"The East Wind leaves your foes to susceptible to magic after you Rampage.", "", ""},
  {"You feel the spiteful East Wind laying your enemies bare.", "", ""},
  {"The winds are changing....", "", ""},
  TILEG_MUT_STAMPEDE_EAST,
},

// Makhleb-specific mutations

{ MUT_MAKHLEB_DESTRUCTION_GEH, 0, 1, mutflag::good,
  "Gehenna destruction",

  {"You draw destruction from the endless fires of Gehenna.", "", ""},
  {"You feel your soul grow hot with the fiery destruction of Gehenna.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_GEHENNA,
},

{ MUT_MAKHLEB_DESTRUCTION_COC, 0, 1, mutflag::good,
  "Cocytus destruction",

  {"You draw destruction from the frigid wastes of Cocytus.", "", ""},
  {"You feel your soul grow cold with the frigid destruction of Cocytus.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_COCYTUS,
},

{ MUT_MAKHLEB_DESTRUCTION_TAR, 0, 1, mutflag::good,
  "Tartarus destruction",

  {"You draw destruction from the wailing grief of Tartarus.", "", ""},
  {"You feel your soul grow tainted with the deathly destruction of Tartarus.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_TARTARUS,
},

{ MUT_MAKHLEB_DESTRUCTION_DIS, 0, 1, mutflag::good,
  "Dis destruction",

  {"You draw destruction from the ruthless spite of Dis.", "", ""},
  {"You feel your soul grow bitter with the corrosive destruction of Dis.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_DIS,
},

{ MUT_MAKHLEB_MARK_HAEMOCLASM, 0, 1, mutflag::makhleb,
  "Mark of Haemoclasm",

  {"You bear the Mark of Haemoclasm.", "", ""},
  {"Gore shall rain upon your enemies.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_HAEMOCLASM,
},

{ MUT_MAKHLEB_MARK_LEGION, 0, 1, mutflag::makhleb,
  "Mark of the Legion",

  {"You bear the Mark of the Legion.", "", ""},
  {"The armies of chaos are now yours to lead.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_LEGION,
},

{ MUT_MAKHLEB_MARK_CARNAGE, 0, 1, mutflag::makhleb,
  "Mark of Carnage",

  {"You bear the Mark of Carnage.", "", ""},
  {"Your servants grow eager to unleash destruction.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_CARNAGE,
},

{ MUT_MAKHLEB_MARK_ANNIHILATION, 0, 1, mutflag::makhleb,
  "Mark of Annihilation",

  {"You bear the Mark of Annihilation.", "", ""},
  {"You feel the destructive energies within you surge violently.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_ANNIHILATION,
},

{ MUT_MAKHLEB_MARK_TYRANT, 0, 1, mutflag::makhleb,
  "Mark of the Tyrant",

  {"You bear the Mark of the Tyrant.", "", ""},
  {"Even fiends shall kneel before you now.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_TYRANT,
},

{ MUT_MAKHLEB_MARK_CELEBRANT, 0, 1, mutflag::makhleb,
  "Mark of the Celebrant",

  {"You bear the Mark of the Celebrant.", "", ""},
  {"Your suffering will be repaid in blood.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_CELEBRANT,
},

{ MUT_MAKHLEB_MARK_EXECUTION, 0, 1, mutflag::makhleb,
  "Mark of Execution",

  {"You bear the Mark of Execution.", "", ""},
  {"Murder takes root in your soul.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_EXECUTION,
},

{ MUT_MAKHLEB_MARK_ATROCITY, 0, 1, mutflag::makhleb,
  "Mark of Atrocity",

  {"You bear the Mark of Atrocity.", "", ""},
  {"You will scour this world clean.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_ATROCITY,
},

{ MUT_MAKHLEB_MARK_FANATIC, 0, 1, mutflag::makhleb,
  "Mark of the Fanatic",

  {"You bear the Mark of the Fanatic.", "", ""},
  {"You will become an instrument of Makhleb's will.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_FANATIC,
},

// Dummy entry for removed mutations.
// XXX: Must be last!
{ MUT_REMOVED_MUTATION, 0, 0, mutflag::good, "Removed Mutation",
  {"Removed mutation", "Removed mutation", "Removed mutation"},
  {"", "", ""},
  {"", "", ""},
},

};

static const mutation_category_def category_mut_data[] =
{
  { RANDOM_MUTATION, "any"},
  { RANDOM_XOM_MUTATION, "xom"},
  { RANDOM_GOOD_MUTATION, "good"},
  { RANDOM_BAD_MUTATION, "bad"},
  { RANDOM_SLIME_MUTATION, "slime"},
  { RANDOM_CORRUPT_MUTATION, "corrupt"},
};
