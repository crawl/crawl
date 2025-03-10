#pragma once

#include "tag-version.h"

struct mutation_def
{
    mutation_type mutation;
    short       weight;     ///< Commonality of the mutation; bigger = appears
                            /// more often.
    short       levels;     ///< The number of levels of the mutation.
    mutflags    uses;       ///< Bitfield holding types of effects that grant
                            /// this mutation (mutflag::*)
    bool        form_based; ///< Mutation is suppressed when shapechanged.
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

{ MUT_IRON_FUSED_SCALES, 0, 1, mutflag::good, true,
  "iron-fused scales",

  {"Your scales are fused with iron. (AC + 5)", "", ""},
  {"Iron fuses itself to your scales.", "", ""},
  {"The iron flakes away from your scales.", "", ""},
  0, {"Iron will fuse itself to your scales. (AC + 5)"}
},

{ MUT_INVIOLATE_MAGIC, 0, 1, mutflag::good, false,
  "inviolate magic",

  {"Your magical power and effects resist disruption.", "", ""},
  {"Your magical power grows resistant to disruption.", "", ""},
  {"Your magical power loses its resistance to disruption.", "", ""},
  TILEG_MUT_INVIOLATE_MAGIC,
  {"Your magical power will grow resistant to disruption."}
},

{ MUT_TOUGH_SKIN, 0, 3, mutflag::good, true,
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

{ MUT_STRONG, 7, 2, mutflag::good, false,
  "strong",

  {"Your muscles are strong. (Str +4, Int/Dex -1)",
   "Your muscles are very strong. (Str +8, Int/Dex -2)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_STRONG,
},

{ MUT_CLEVER, 7, 2, mutflag::good, false,
  "clever",

  {"Your mind is acute. (Int +4, Str/Dex -1)",
   "Your mind is very acute. (Int +8, Str/Dex -2)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_CLEVER,
},

{ MUT_AGILE, 7, 2, mutflag::good, false,
  "agile",

  {"You are agile. (Dex +4, Str/Int -1)",
   "You are very agile. (Dex +8, Str/Int -2)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_AGILE,
},

{ MUT_POISON_RESISTANCE, 4, 1, mutflag::good, true,
  "poison resistance",

  {"Your system is resistant to poisons. (rPois)", "", ""},
  {"You feel resistant to poisons.", "",  ""},
  {"You feel less resistant to poisons.", "", ""},
  TILEG_MUT_POISON_RESISTANCE,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_CARNIVOROUS, 0, 1, mutflag::good, false,
  "carnivore",

  {"You are carnivorous and can eat meat at any time.", "", ""},

  {"You hunger for flesh.", "", ""},

  {"You feel able to eat a more balanced diet.", "", ""},
},
{ MUT_HERBIVOROUS, 0, 1, mutflag::bad, false,
  "herbivore",

  {"You are a herbivore.", "", ""},

  {"", "", ""},

  {""},
},
#endif

{ MUT_HEAT_RESISTANCE, 4, 3, mutflag::good, true,
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

{ MUT_COLD_RESISTANCE, 4, 3, mutflag::good, true,
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

{ MUT_HEAT_VULNERABILITY, 3, 3, mutflag::bad | mutflag::qazlal, true,
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

{ MUT_COLD_VULNERABILITY, 3, 3, mutflag::bad | mutflag::qazlal, true,

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

{ MUT_DEMONIC_GUARDIAN, 0, 3, mutflag::good, false,
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

{ MUT_SHOCK_RESISTANCE, 2, 1, mutflag::good, true,
  "electricity resistance",

  {"You are resistant to electric shocks. (rElec)", "", ""},
  {"You feel insulated.", "", ""},
  {"You feel conductive.", "", ""},
  TILEG_MUT_SHOCK_RESISTANCE,
},

{ MUT_SHOCK_VULNERABILITY, 0, 1, mutflag::bad | mutflag::qazlal, true,
  "electricity vulnerability",

  {"You are vulnerable to electric shocks.", "", ""},
  {"You feel vulnerable to electricity.", "", ""},
  {"You feel less vulnerable to electricity.", "", ""},
  TILEG_MUT_SHOCK_VULNERABILITY,
},

{ MUT_REGENERATION, 2, 3, mutflag::good, false,
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

{ MUT_INHIBITED_REGENERATION, 3, 1, mutflag::bad, false,
  "inhibited regeneration",

  {"You do not regenerate when monsters are visible.", "", ""},

  {"Your regeneration stops near monsters.", "", ""},

  {"You begin to regenerate regardless of the presence of monsters.", "", ""},

  TILEG_MUT_INHIBITED_REGENERATION,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_FAST_METABOLISM, 0, 3, mutflag::bad, false,
  "fast metabolism",

  {"You have a fast metabolism.",
   "You have a very fast metabolism.",
   "Your metabolism is lightning-fast."},

  {"You feel a little hungry.",
   "You feel a little hungry.",
   "You feel a little hungry."},

  {"Your metabolism slows.",
   "Your metabolism slows.",
   "Your metabolism slows."},
},

{ MUT_SLOW_METABOLISM, 0, 2, mutflag::good, false,
  "slow metabolism",

  {"You have a slow metabolism.",
   "You need to consume almost no food.",
   ""},

  {"Your metabolism slows.",
   "Your metabolism slows.",
   ""},

  {"You feel a little hungry.",
   "You feel a little hungry.",
   ""},
},
#endif

{ MUT_WEAK, 8, 2, mutflag::bad, false,
  "weak",

  {"You are weak. (Str -2)",
   "You are very weak. (Str -4)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_WEAK,
},

{ MUT_DOPEY, 8, 2, mutflag::bad, false,
  "dopey",

  {"You are dopey. (Int -2)",
   "You are very dopey. (Int -4)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_DOPEY,
},

{ MUT_CLUMSY, 8, 2, mutflag::bad, false,
  "clumsy",

  {"You are clumsy. (Dex -2)",
   "You are very clumsy. (Dex -4)", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_CLUMSY,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_TELEPORT_CONTROL, 0, 1, mutflag::good, false,
  "teleport control",

  {"You can control translocations.", "", ""},
  {"You feel controlled.", "", ""},
  {"You feel random.", "", ""},
},
#endif

{ MUT_TELEPORT, 3, 2, mutflag::bad, false,
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

{ MUT_PERSISTENT_DRAIN, 5, 1, mutflag::bad, false,
  "persistent drain",

  {"Your health recovers twice as slowly from being drained.", "", ""},

  {"You begin to recover more slowly from draining effects.", "", ""},

  {"You recover from draining at a normal speed again.", "", ""},
  TILEG_MUT_PERSISTENT_DRAIN,
},

{ MUT_STRONG_WILLED, 5, 3, mutflag::good, false,
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

{ MUT_FAST, 0, 3, mutflag::good, true,
  "speed",

  {"You cover ground quickly. (Speed+)",
   "You cover ground very quickly. (Speed++)",
   "You cover ground extremely quickly. (Speed+++)"},

  {"You feel quick.",
   "You feel quick.",
   "You feel quick."},

  {"You feel sluggish.",
   "You feel sluggish.",
   "You feel sluggish."},
},

{ MUT_SLOW, 0, 3, mutflag::bad, true,
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

{ MUT_ACUTE_VISION, 2, 1, mutflag::good, false,
  "see invisible",

  {"You have supernaturally acute vision. (SInv)", "", ""},
  {"Your vision sharpens.", "", ""},
  {"Your vision seems duller.", "", ""},

  TILEG_MUT_ACUTE_VISION,
},

{ MUT_DEFORMED, 8, 1, mutflag::bad, true,
  "deformed body",

  {"Armour fits poorly on your strangely shaped body.", "", ""},
  {"Your body twists and deforms.", "", ""},
  {"Your body's shape seems more normal.", "", ""},
  TILEG_MUT_GENERIC_BAD_MUTATION,
},

// Naga only
{ MUT_SPIT_POISON, 8, 2, mutflag::good, false,
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
#if TAG_MAJOR_VERSION == 34

{ MUT_BREATHE_FLAMES, 0, 3, mutflag::good, false,
  "breathe flames",

  {"You can breathe flames.",
   "You can breathe fire.",
   "You can breathe blasts of fire."},

  {"Your throat feels hot.",
   "Your throat feels hot.",
   "Your throat feels hot."},

  {"A chill runs up and down your throat.",
   "A chill runs up and down your throat.",
   "A chill runs up and down your throat."},
},

{ MUT_JUMP, 0, 3, mutflag::good, false,
  "jump",

  {"You can jump attack at a short distance.",
   "You can jump attack at a medium distance.",
   "You can jump attack at a long distance."},

  {"You feel more sure on your feet.",
   "You feel more sure on your feet.",
   "You feel more sure on your feet."},

  {"You feel less sure on your feet.",
   "You feel less sure on your feet.",
   "You feel less sure on your feet."},
},

{ MUT_BLINK, 0, 1, mutflag::good, false,
  "blink",

  {"You can translocate small distances at will.", "", ""},
  {"You feel jittery.", "", ""},
  {"You no longer feel jittery.", "", ""},
},

{ MUT_STRONG_STIFF, 0, 3, mutflag::good, false,
  "stiff muscles",

  {"Your muscles are strong, but stiff. (Str +1, Dex -1)",
   "Your muscles are very strong, but stiff. (Str +2, Dex -2)",
   "Your muscles are extremely strong, but stiff. (Str +3, Dex -3)"},

  {"Your muscles feel sore.",
   "Your muscles feel sore.",
   "Your muscles feel sore."},

  {"Your muscles feel loose.",
   "Your muscles feel loose.",
   "Your muscles feel loose."},
},

{ MUT_FLEXIBLE_WEAK, 0, 3, mutflag::good, false,
  "flexible muscles",

  {"Your muscles are flexible, but weak (Str -1, Dex +1).",
   "Your muscles are very flexible, but weak (Str -2, Dex +2).",
   "Your muscles are extremely flexible, but weak (Str -3, Dex +3)."},

  {"Your muscles feel loose.",
   "Your muscles feel loose.",
   "Your muscles feel loose."},

  {"Your muscles feel sore.",
   "Your muscles feel sore.",
   "Your muscles feel sore."},
},
#endif

{ MUT_SCREAM, 6, 2, mutflag::bad, false,
  "screaming",

  {"You occasionally shout uncontrollably at your foes.",
   "You frequently scream uncontrollably at your foes.",
   ""},

  {"You feel the urge to shout.",
   "You feel a strong urge to scream.",
   ""},

  {"Your urge to shout disappears.",
   "Your urge to scream lessens.",
   ""},
  TILEG_MUT_SCREAM,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_NOISE_DAMPENING, 0, 1, mutflag::good, false,
  "noise suppression",

  {"You passively dampen the noise of your surroundings.", "", ""},

  {"You feel your surroundings grow quieter.", "", ""},

  {"You feel your surroundings grow louder.", "", ""},
},
#endif

{ MUT_CLARITY, 6, 1, mutflag::good, false,
  "clarity",

  {"You possess an exceptional clarity of mind.", "", ""},
  {"Your thoughts seem clearer.", "", ""},
  {"Your thinking seems confused.", "", ""},
  TILEG_MUT_CLARITY
},

{ MUT_BERSERK, 7, 2, mutflag::bad, false,
  "berserk",

  {"You sometimes lose your temper in combat.",
   "You have an uncontrollable temper.",
   ""},

  {"You feel a little pissed off.",
   "You feel extremely angry at everything!",
   ""},

  {"You feel a little more calm.",
   "You feel a little less angry.",
   ""},
  TILEG_MUT_BERSERK,
},

{ MUT_POOR_CONSTITUTION, 10, 2, mutflag::bad, false,
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

#if TAG_MAJOR_VERSION == 34
{ MUT_BLURRY_VISION, 0, 3, mutflag::bad, false,
  "blurry vision",

  {"Scrolls take you a little longer to read.",
   "Scrolls take you longer to read.",
   "Scrolls take you much longer to read."},

  {"Your vision blurs.",
   "Your vision blurs.",
   "Your vision blurs."},

  {"Your vision sharpens.",
   "Your vision sharpens a little.",
   "Your vision sharpens a little."},
},
#endif

{ MUT_MUTATION_RESISTANCE, 4, 3, mutflag::good, false,
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

{ MUT_EVOLUTION, 4, 2, mutflag::good, false,
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

{ MUT_DEVOLUTION, 4, 2, mutflag::bad, false,
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

{ MUT_FRAIL, 10, 3, mutflag::bad, false,
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

{ MUT_ROBUST, 5, 3, mutflag::good, false,
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

#if TAG_MAJOR_VERSION == 34
{ MUT_UNBREATHING, 0, 2, mutflag::good, true,
  "unbreathing",

  {"You can survive without breathing.",
   "You can survive without breathing, even under water.", ""},
  {"You feel breathless.", "", ""},
  {"", "", ""},
},
#endif

{ MUT_TORMENT_RESISTANCE, 0, 2, mutflag::good, false,
  "torment resistance",

  {"You are resistant to unholy torment.",
   "You are immune to unholy pain and torment.", ""},
  {"You feel a strange anaesthesia.",
   "You feel a very strange anaesthesia.", ""},
  {"", "", ""},
  TILEG_MUT_TORMENT_RES,
},

{ MUT_NEGATIVE_ENERGY_RESISTANCE, 0, 3, mutflag::good, false,
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
#if TAG_MAJOR_VERSION == 34

{ MUT_MUMMY_RESTORATION, 0, 1, mutflag::good, false,
  "restore body",

  {"You can restore your body by infusing magical energy.",
   "",
   ""},

  {"You can now infuse your body with magic to restore decomposition.",
   "",
   ""},

  {"", "", ""},
},
#endif

{ MUT_NECRO_ENHANCER, 0, 2, mutflag::good, false,
  "in touch with death",

  {"You are in touch with the powers of death.",
   "You are strongly in touch with the powers of death.",
   ""},

  {"You feel more in touch with the powers of death.",
   "You feel more in touch with the powers of death.",
   ""},

  {"", "", ""},
},

{ MUT_TENGU_FLIGHT, 0, 1, mutflag::good, false,
  "evasive flight",

  {"Your magical flight helps you evade attacks. (EV +4)",
   "", ""},
  {"Your magical nature develops, letting you fly evasively.",
   "", ""},
  {"", "", ""},
  0,
  {"Your magical flight will help you evade attacks. (EV + 4)"}
},

{ MUT_ACROBATIC, 0, 1, mutflag::good, false,
  "acrobatic",

  {"You can magically evade attacks while moving or waiting.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_ACROBATIC,
},

{ MUT_WIELD_OFFHAND, 0, 1, mutflag::good, true,
  "off-hand wielding",

  {"You can wield a second weapon in your off-hand.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_SLOW_WIELD, 0, 1, mutflag::bad, true,
  "slow wielding",

  {"It takes a long time for you to wield or remove held weapons.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_INITIALLY_ATTRACTIVE, 0, 1, mutflag::bad, false,
  "initially attractive",

  {"You sometimes attract newly seen creatures.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_GENERIC_BAD_MUTATION,
},

{ MUT_WARMUP_STRIKES, 0, 1, mutflag::bad, true,
  "warmup strikes",

  {"Your first few attacks do less damage.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_NO_JEWELLERY, 0, 1, mutflag::bad, false,
  "no jewellery",

  {"You cannot equip rings or amulets.",
   "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_HURL_DAMNATION, 0, 1, mutflag::good, false,
  "hurl damnation",

  {"You can hurl damnation.", "", ""},
  {"You smell a hint of brimstone.", "", ""},
  {"", "", ""},
  TILEG_MUT_HURL_DAMNATION,
},

// body-slot facets
{ MUT_HORNS, 7, 3, mutflag::good, true,
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

{ MUT_BEAK, 1, 1, mutflag::good, true,
  "beak",

  {"You have a beak for a mouth.", "", ""},
  {"Your mouth lengthens and hardens into a beak!", "", ""},
  {"Your beak shortens and softens into a mouth.", "", ""},
  TILEG_MUT_BEAK,
},

{ MUT_CLAWS, 2, 3, mutflag::good, true,
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

{ MUT_FANGS, 1, 3, mutflag::good, true,
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

{ MUT_HOOVES, 5, 3, mutflag::good, true,
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

{ MUT_ANTENNAE, 4, 3, mutflag::good, true,
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

{ MUT_TALONS, 5, 3, mutflag::good, true,
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
{ MUT_TENTACLE_SPIKE, 10, 3, mutflag::good, true,
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
#if TAG_MAJOR_VERSION == 34

{ MUT_BREATHE_POISON, 0, 1, mutflag::good, false,
  "breathe poison",

  {"You can exhale a cloud of poison.", "", ""},
  {"You taste something nasty.", "", ""},
  {"Your breath is less nasty.", "", ""},
},
#endif

{ MUT_CONSTRICTING_TAIL, 0, 2, mutflag::good, true,
  "naga tail",

  {"You have a snake-like lower body.",
   "You can use your snake-like lower body to constrict enemies.", ""},
  {"Your lower body turns into a snake tail.",
   "Your tail grows strong enough to constrict your enemies.", ""},
  {"Your lower body returns to normal.",
   "Your snake tail weakens and can no longer constrict your enemies.", ""},
   0,
   {0, "Your tail will grow strong enough to constrict your foes."}
},

// Naga and Draconian only
{ MUT_STINGER, 8, 3, mutflag::good, true,
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
  TILEG_MUT_GENERIC_GOOD_MUTATION,
},

// Draconian/gargoyle only
{ MUT_BIG_WINGS, 4, 1, mutflag::good, true,
  "big wings",

  {"Your large and strong wings let you fly.", "", ""},
  {"Your wings grow larger and stronger.", "", ""},
  {"Your wings shrivel and weaken.", "", ""},
  TILEG_MUT_BIG_WINGS,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_SAPROVOROUS, 0, 3, mutflag::good, false,
  "saprovore",

  {"You can tolerate rotten meat.",
   "You can eat rotten meat.",
   "You thrive on rotten meat."},

  {"You hunger for rotting flesh.",
   "You hunger for rotting flesh.",
   "You hunger for rotting flesh."},

  {"", "", ""},
},

{ MUT_MIASMA_IMMUNITY, 0, 1, mutflag::good, false,
  "miasma immunity",

  {"You are immune to miasma.", "", ""},
  {"You feel immune to miasma.", "", ""},
  {"You feel vulnerable to miasma.", "", ""},
},

// species-dependent innate mutations
{ MUT_GOURMAND, 0, 1, mutflag::good, false,
  "gourmand",

  {"You like to eat raw meat.", "", ""},
  {"", "", ""},
  {"", "", ""},
},
#endif

{ MUT_HOP, 0, 2, mutflag::good, true,
  "strong legs",

  {"You can hop short distances.",
   "You can hop long distances.",
   ""},

  {"", "Your legs feel stronger.", ""},

  {"", "", ""},

  TILEG_MUT_HOP,
},
#if TAG_MAJOR_VERSION == 34

// Armataur only
{ MUT_ROLL, 0, 3, mutflag::good, true,
  "roll",

  {"You can roll at nearby foes to attack.",
   "You can roll at foes to attack.",
   "You can roll a great distance at foes to attack."},

  {"",
   "You feel you can roll further.",
   "You feel you can roll even further."},

  {"",
   "You can no longer roll as far.",
   "You can no longer roll as far."},
},

{ MUT_CURL, 0, 1, mutflag::good, true,
  "reflexive curl",

  {"You curl defensively after being hit. (AC +7*)", "", ""},
  {"You now curl defensively after being hit.", "", ""},
  {"", "", ""},
},

{ MUT_AWKWARD_TONGUE, 0, 1, mutflag::bad, false,
  "awkward tongue",
  {"Your tongue gives you trouble enunciating. (1.5x scroll delay)", "", ""},
  {"Your tongue begins to flop around amusingly.", "", ""},
  {"Your tongue regains its customary placidity.", "", ""},
},
#endif

{ MUT_ARMOURED_TAIL, 0, 1, mutflag::good, true,
  "armoured tail",

  {"You have a long armoured tail.", "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_ROLLPAGE, 0, 2, mutflag::good, false,
  "rollpage",

  {"You regenerate magic when rolling toward enemies. (Rampage MPRegen)",
   "You regenerate magic and health when rolling toward enemies. (Rampage Regen)",
   ""},

  {"You begin to regenerate magic when rolling toward enemies.",
   "You begin to regenerate health when rolling toward enemies.",
   ""},

  {"You can no longer roll toward enemies.",
   "You can no longer regenerate health when rolling toward enemies.",
   ""},
},

{ MUT_SHAGGY_FUR, 2, 3, mutflag::good, true,
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

{ MUT_HIGH_MAGIC, 2, 3, mutflag::good, false,
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

{ MUT_LOW_MAGIC, 9, 3, mutflag::bad, false,
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

{ MUT_WILD_MAGIC, 6, 3, mutflag::good, false,
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

{ MUT_SUBDUED_MAGIC, 6, 3, mutflag::bad, false,
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

{ MUT_EFFICIENT_MAGIC, 4, 2, mutflag::good, false,
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

{ MUT_EPHEMERAL_SHIELD, 4, 1, mutflag::good, false,
  "ephemeral shield",

  {"A shield forms around you when casting spells or using Invocations. (SH +7)",
    "", ""},
  {"Ambient excess energy start collecting around you.", "", ""},
  {"The ambient excess energy around you dissipates.", "", ""},
  TILEG_MUT_EPHEMERAL_SHIELD,
},

{ MUT_TIME_WARPED_BLOOD, 2, 2, mutflag::good, false,
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

{ MUT_DEMONIC_MAGIC, 0, 3, mutflag::good, false,
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

{ MUT_FORLORN, 0, 1, mutflag::bad, false,
  "forlorn",

  {"You shall have no god before yourself.","",""},
  {"You feel forlorn.","",""},
  {"You feel more spiritual.","",""},
  TILEG_MUT_FORLORN,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_STOCHASTIC_TORMENT_RESISTANCE, 0, 1, mutflag::good, false,
  "removed torment resistance",

  {"You are somewhat able to resist unholy torments.","",""},
  {"You feel a strange anaesthesia.", "", ""},
  {"", "", ""},
},
#endif

{ MUT_PASSIVE_MAPPING, 3, 3, mutflag::good, false,
  "sense surroundings",

  {"You passively map a small area around you.",
   "You passively map the area around you.",
   "You passively map a large area around you."},

  {"You feel a strange attunement to the structure of the dungeons.",
   "Your attunement to dungeon structure grows.",
   "Your attunement to dungeon structure grows further."},

  {"You feel slightly disoriented.",
   "You feel slightly disoriented.",
   "You feel slightly disoriented."},

  TILEG_MUT_PASSIVE_MAPPING,
},

{ MUT_ICEMAIL, 0, 2, mutflag::good, false,
  "icemail",

  {"A meltable icy envelope protects you from harm. (AC +",
   "A thick, meltable icy envelope protects you from harm. (AC +", ""},
  {"An icy envelope takes form around you.",
   "Your icy envelope grows thicker.", ""},
  {"", "", ""},

  TILEG_MUT_ICEMAIL,
},

{ MUT_CONDENSATION_SHIELD, 0, 1, mutflag::good, false,
  "condensation shield",

  {"A meltable shield of frost defends you. (SH +", "", ""},
  {"Frost condenses into a shield before you.","", ""},
  {"", "", ""},

  TILEG_MUT_CONDENSATION_SHIELD,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_CONSERVE_SCROLLS, 0, 1, mutflag::good, false,
  "conserve scrolls",

  {"You are very good at protecting items from fire.", "", ""},
  {"You feel less concerned about heat.", "", ""},
  {"", "", ""},
},

{ MUT_CONSERVE_POTIONS, 0, 1, mutflag::good, false,
  "conserve potions",

  {"You are very good at protecting items from cold.", "", ""},
  {"You feel less concerned about cold.", "", ""},
  {"", "", ""},
},
#endif

{ MUT_PASSIVE_FREEZE, 1, 1, mutflag::good, false,
  "passive freeze",

  {"A frigid envelope surrounds you and freezes all who hurt you.", "", ""},
  {"Your skin feels very cold.", "", ""},
  {"Your skin warms up.", "", ""},

  TILEG_MUT_PASSIVE_FREEZE,
},

{ MUT_NIGHTSTALKER, 0, 3, mutflag::good, false,
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

{ MUT_SPINY, 0, 3, mutflag::good, true,
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

{ MUT_POWERED_BY_DEATH, 0, 3, mutflag::good, false,
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

{ MUT_POWERED_BY_PAIN, 0, 3, mutflag::good, false,
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

{ MUT_AUGMENTATION, 0, 3, mutflag::good, false,
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

{ MUT_MANA_SHIELD, 0, 1, mutflag::good, false,
  "magic shield",

  {"When hurt, damage is shared between your health and your magic reserves.", "", ""},
  {"You feel your magical essence form a protective shroud around your flesh.", "", ""},
  {"", "", ""},

  TILEG_MUT_MANA_SHIELD,
},

{ MUT_MANA_REGENERATION, 0, 1, mutflag::good, false,
  "magic regeneration",

  {"You regenerate magic rapidly.", "", ""},
  {"Your magic begins to regenerate rapidly.", "", ""},
  {"", "", ""},

  TILEG_MUT_MANA_REGENERATION,
},

{ MUT_MANA_LINK, 0, 1, mutflag::good, false,
  "magic link",

  {"When low on magic, you restore magic in place of health.", "", ""},
  {"You feel your life force and your magical essence meld.", "", ""},
  {"", "", ""},

  TILEG_MUT_MANA_LINK,
},

// Jiyva only mutations. (MUT_GELATINOUS_BODY is also used by Op)
{ MUT_GELATINOUS_BODY, 0, 3, mutflag::good | mutflag::jiyva, true,
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

{ MUT_EYEBALLS, 0, 3, mutflag::good | mutflag::jiyva, true,
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

{ MUT_TRANSLUCENT_SKIN, 0, 3, mutflag::good | mutflag::jiyva, true,
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

{ MUT_PSEUDOPODS, 0, 3, mutflag::good | mutflag::jiyva, true,
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
  TILEG_MUT_GENERIC_JIYVA_MUTATION,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_FOOD_JELLY, 0, 1, mutflag::good, false,
  "spawn jellies when eating",

  {"You occasionally spawn a jelly by eating.", "", ""},
  {"You feel more connected to the slimes.", "", ""},
  {"Your connection to the slimes vanishes.", "", ""},
},
#endif

{ MUT_ACIDIC_BITE, 0, 1, mutflag::good | mutflag::jiyva, true,
  "acidic bite",

  {"You have acidic saliva.", "", ""},
  {"Acid begins to drip from your mouth.", "", ""},
  {"Your mouth feels dry.", "", ""},
  TILEG_MUT_ACIDIC_BITE,
},

{ MUT_ANTIMAGIC_BITE, 0, 1, mutflag::good, true,
  "antimagic bite",

  {"Your bite disrupts and absorbs the magic of your enemies.", "", ""},
  {"You feel a sudden thirst for magic.", "", ""},
  {"Your magical appetite wanes.", "", ""},
  TILEG_MUT_ANTIMAGIC_BITE,
},

{ MUT_NO_POTION_HEAL, 3, 2, mutflag::bad, false,
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

// Scale mutations
{ MUT_DISTORTION_FIELD, 0, 3, mutflag::good, false,
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

{ MUT_ICY_BLUE_SCALES, 0, 3, mutflag::good, true,
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

{ MUT_IRIDESCENT_SCALES, 10, 3, mutflag::good, true,
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

{ MUT_LARGE_BONE_PLATES, 2, 3, mutflag::good, true,
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

{ MUT_MOLTEN_SCALES, 0, 3, mutflag::good, true,
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
#if TAG_MAJOR_VERSION == 34

{ MUT_ROUGH_BLACK_SCALES, 0, 3, mutflag::good, true,
  "rough black scales",

  {"You are partially covered in rough black scales. (AC +2, Dex -1)",
   "You are mostly covered in rough black scales. (AC +5, Dex -2)",
   "You are completely covered in rough black scales. (AC +8, Dex -3)"},

  {"Rough black scales grow over part of your body.",
   "Rough black scales spread over more of your body.",
   "Rough black scales cover your body completely."},

  {"Your rough black scales disappear.",
   "Your rough black scales recede somewhat.",
   "Your rough black scales recede somewhat."},
},
#endif

{ MUT_RUGGED_BROWN_SCALES, 0, 3, mutflag::good, true,
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

{ MUT_SLIMY_GREEN_SCALES, 0, 3, mutflag::good, true,
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

{ MUT_THIN_METALLIC_SCALES, 0, 3, mutflag::good, true,
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

{ MUT_THIN_SKELETAL_STRUCTURE, 2, 3, mutflag::good, false,
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

{ MUT_YELLOW_SCALES, 0, 3, mutflag::good, true,
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

{ MUT_SHARP_SCALES, 0, 3, mutflag::good, true,
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

{ MUT_STURDY_FRAME, 2, 3, mutflag::good, true,
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

{ MUT_SANGUINE_ARMOUR, 0, 3, mutflag::good, false,
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

  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_BIG_BRAIN, 0, 3, mutflag::good, false,
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

{ MUT_CAMOUFLAGE, 1, 3, mutflag::good, true,
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

{ MUT_IGNITE_BLOOD, 0, 3, mutflag::good, false,
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

{ MUT_FOUL_STENCH, 0, 3, mutflag::good, false,
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

{ MUT_TENDRILS, 0, 1, mutflag::good | mutflag::jiyva, true,
  "tendrils",

  {"You are covered in slimy tendrils that may disarm your opponents.", "", ""},
  {"Thin, slimy tendrils emerge from your body.", "", ""},
  {"Your tendrils retract into your body.", "", ""},
  TILEG_MUT_TENDRILS,
},

{ MUT_JELLY_GROWTH, 0, 1, mutflag::good | mutflag::jiyva, true,
  "jelly sensing items",

  {"You have a small jelly attached to you that senses nearby items.", "", ""},
  {"Your body partially splits into a small jelly.", "", ""},
  {"The jelly growth is reabsorbed into your body.", "", ""},
  TILEG_MUT_JELLY_GROWTH,
},

{ MUT_JELLY_MISSILE, 0, 1, mutflag::good | mutflag::jiyva, true,
  "jelly absorbing missiles",

  {"You have a small jelly attached to you that may absorb projectiles.", "", ""},
  {"Your body partially splits into a small jelly.", "", ""},
  {"The jelly growth is reabsorbed into your body.", "", ""},
  TILEG_MUT_JELLY_ABSORBING_MISSILES,
},

{ MUT_PETRIFICATION_RESISTANCE, 0, 1, mutflag::good, false,
  "petrification resistance",

  {"You are immune to petrification.", "", ""},
  {"Your body vibrates.", "", ""},
  {"You briefly stop moving.", "", ""},
  TILEG_MUT_PETRIFICATION_RES,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_TRAMPLE_RESISTANCE, 0, 1, mutflag::good, false,
  "trample resistance",

  {"You are resistant to trampling.", "", ""},
  {"You feel steady.", "", ""},
  {"You feel unsteady.", "", ""},
},

{ MUT_CLING, 0, 1, mutflag::good, true,
  "cling",

  {"You can cling to walls.", "", ""},
  {"You feel sticky.", "", ""},
  {"You feel slippery.", "", ""},
},

{ MUT_EXOSKELETON, 0, 2, mutflag::good, true,
  "exoskeleton",

  {"Your body is surrounded by an exoskeleton. (buggy)",
   "Your body is surrounded by a tough exoskeleton. (buggy)",
   ""},

  {"Your exoskeleton hardens.",
   "Your exoskeleton becomes even harder.",
   ""},

  {"Your exoskeleton softens.",
   "Your exoskeleton softens.",
   ""},
},

{ MUT_FUMES, 0, 2, mutflag::good, false,
  "fuming",

  {"You emit clouds of smoke.", "You frequently emit clouds of smoke.", ""},
  {"You fume.", "You fume more.", ""},
  {"You stop fuming.", "You fume less.", ""},
},
#endif

{ MUT_BLACK_MARK, 0, 1, mutflag::good, false,
  "black mark",

  {"Your melee attacks may debilitate your foes.", "", ""},
  {"An ominous black mark forms on your body.", "", ""},
  {"", "", ""},
  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_SILENCE_AURA, 0, 1, mutflag::good, false,
  "aura of silence",

  {"You are surrounded by an aura of silence.", "", ""},
  {"An unnatural silence shrouds you.", "", ""},
  {"", "", ""},
  TILEG_MUT_SILENCE_AURA,
},

{ MUT_HEX_ENHANCER, 0, 1, mutflag::good, false,
  "bedevilling",

  {"Your hexes are more powerful.", "", ""},
  {"You feel devilish.", "", ""},
  {"", "", ""},
  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_CORRUPTING_PRESENCE, 0, 2, mutflag::good, false,
  "corrupting presence",

  {"Your presence sometimes corrodes those you injure.",
   "Your presence sometimes corrodes or deforms those you injure.", ""},
  {"You feel corrupt.", "Your corrupting presence intensifies.", ""},
  {"", "", ""},
  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_WORD_OF_CHAOS, 0, 1, mutflag::good, false,
  "word of chaos",

  {"You can speak a Word of Chaos.", "", ""},
  {"Your tongue twists.", "", ""},
  {"", "", ""},
  TILEG_MUT_WORD_OF_CHAOS,
},

{ MUT_DEMONIC_WILL, 0, 1, mutflag::good, false,
  "demonic willpower",

  {"You punish those that try to bend your will. (Will+)", "", ""},
  {"You feel wilful.", "", ""},
  {"", "", ""},
  TILEG_MUT_DEMONIC_WILL,
},

{ MUT_WEAKNESS_STINGER, 0, 3, mutflag::good, true,
  "weakness stinger",

  {"You have a small tail.",
   "You have a tail ending in a sharp stinger.",
   "You have a sharp stinger which inflicts weakening toxins."},

  {"You grow a small tail.",
   "Your tail grows a sharp stinger.",
   "Your stinger grows larger and begins to produce weakening toxins."},

  {"", "", ""},
  TILEG_MUT_GENERIC_DEMONSPAWN_MUTATION,
},

{ MUT_DEMONIC_TOUCH, 0, 3, mutflag::good, true,
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

{ MUT_COLD_BLOODED, 0, 1, mutflag::bad, true,
  "cold-blooded",

  {"You are cold-blooded and may be slowed by cold attacks.", "", ""},
  {"You feel cold-blooded.", "", ""},
  {"You feel warm-blooded.", "", ""},
  TILEG_MUT_COLD_BLOODED,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_FLAME_CLOUD_IMMUNITY, 0, 1, mutflag::good, false,
  "flame cloud immunity",

  {"You are immune to clouds of flame.", "", ""},
  {"You feel less concerned about heat.", "", ""},
  {"", "", ""},
},

{ MUT_FREEZING_CLOUD_IMMUNITY, 0, 1, mutflag::good, false,
  "freezing cloud immunity",

  {"You are immune to freezing clouds.", "", ""},
  {"You feel less concerned about cold.", "", ""},
  {"", "", ""},
},

{ MUT_SUSTAIN_ATTRIBUTES, 0, 1, mutflag::good, false,
    "sustain attributes",

    {"Your attributes are resistant to harm.", "", ""},
    {"", "", ""},
    {"", "", ""},
},
#endif

{ MUT_DRINK_SAFETY, 7, 2, mutflag::bad, false,
  "inability to drink after injury",

  {"You occasionally lose the ability to drink potions when taking damage.",
   "You sometimes lose the ability to drink potions when taking damage.",
   ""},
  {"You occasionally lose the ability to drink potions when taking damage.",
   "You lose the ability to drink potions when taking damage more often.",
   ""},
  {"You no longer become unable to drink potions after taking damage.",
   "You lose the ability to drink potions when taking damage less often.",
   ""},
  TILEG_MUT_GENERIC_BAD_MUTATION,
},

{ MUT_READ_SAFETY, 7, 2, mutflag::bad, false,
  "inability to read after injury",

  {"You occasionally lose the ability to read scrolls when taking damage.",
   "You sometimes lose the ability to read scrolls when taking damage.",
   ""},
  {"You occasionally lose the ability to read scrolls when taking damage.",
   "You lose the ability to read scrolls when taking damage more often.",
   ""},
  {"You no longer become unable to read scrolls after taking damage.",
   "You lose the ability to read scrolls when taking damage less often.",
   ""},
  TILEG_MUT_GENERIC_BAD_MUTATION,
},

{ MUT_MISSING_HAND, 0, 1, mutflag::bad, false,
  "missing a hand",

  {"You are missing a hand.", "", ""},
  {"One of your hands has vanished, leaving only a stump!", "", ""},
  {"Your stump has regrown into a hand!", "", ""},
  TILEG_MUT_MISSING_HAND,
},

{ MUT_NO_STEALTH, 0, 1, mutflag::bad, false,
  "no stealth",

  {"You cannot be stealthy.", "", ""},
  {"You can no longer be stealthy.", "", ""},
  {"You can once more be stealthy.", "", ""},
  TILEG_MUT_NO_STEALTH,
},

{ MUT_NO_ARTIFICE, 0, 1, mutflag::bad, false,
  "inability to use devices",

  {"You cannot study or use magical devices.", "", ""},
  {"You can no longer study or use magical devices.", "", ""},
  {"You can once more study and use magical devices.", "", ""},
},

{ MUT_NO_LOVE, 0, 1, mutflag::bad, false,
  "hated by all",

  {"You are hated by all.", "", ""},
  {"You are now hated by all.", "", ""},
  {"You are no longer hated by all.", "", ""},
},

{ MUT_COWARDICE, 0, 1, mutflag::bad, false,
  "cowardly",

  {"Your cowardice makes you less effective in combat with threatening foes.", "", ""},
  {"You have lost your courage.", "", ""},
  {"You have regained your courage.", "", ""},
},

{ MUT_NO_DODGING, 0, 1, mutflag::bad, false,
  "inability to train dodging",

  {"You cannot train Dodging skill.", "", ""},
  {"You can no longer train Dodging skill.", "", ""},
  {"You can once more train Dodging skill.", "", ""},
  TILEG_MUT_NO_DODGING,
},

{ MUT_NO_ARMOUR_SKILL, 0, 1, mutflag::bad, false,
  "inability to train armour",

  {"You cannot train Armour skill.", "", ""},
  {"You can no longer train Armour skill.", "", ""},
  {"You can once more train Armour skill.", "", ""},
  TILEG_MUT_NO_ARMOUR_SKILL,
},

{ MUT_NO_AIR_MAGIC, 0, 1, mutflag::bad, false,
  "no air magic",

  {"You cannot study or cast Air magic.", "", ""},
  {"You can no longer study or cast Air magic.", "", ""},
  {"You can once more study and cast Air magic.", "", ""},
  TILEG_MUT_NO_AIR_MAGIC,
},
#if TAG_MAJOR_VERSION == 34

{ MUT_NO_CHARM_MAGIC, 0, 1, mutflag::bad, false,
  "no charms magic",

  {"You cannot study or cast removed Charms magic.", "", ""},
  {"You can no longer study or cast Charms magic.", "", ""},
  {"You can once more study and cast Charms magic.", "", ""},
},
#endif

{ MUT_NO_CONJURATION_MAGIC, 0, 1, mutflag::bad, false,
  "no conjurations magic",

  {"You cannot study or cast Conjurations magic.", "", ""},
  {"You can no longer study or cast Conjurations magic.", "", ""},
  {"You can once more study and cast Conjurations magic.", "", ""},
},

{ MUT_NO_EARTH_MAGIC, 0, 1, mutflag::bad, false,
  "no earth magic",

  {"You cannot study or cast Earth magic.", "", ""},
  {"You can no longer study or cast Earth magic.", "", ""},
  {"You can once more study and cast Earth magic.", "", ""},
  TILEG_MUT_NO_EARTH_MAGIC,
},

{ MUT_NO_FIRE_MAGIC, 0, 1, mutflag::bad, false,
  "no fire magic",

  {"You cannot study or cast Fire magic.", "", ""},
  {"You can no longer study or cast Fire magic.", "", ""},
  {"You can once more study and cast Fire magic.", "", ""},
},

{ MUT_NO_HEXES_MAGIC, 0, 1, mutflag::bad, false,
  "no hexes magic",

  {"You cannot study or cast Hexes magic.", "", ""},
  {"You can no longer study or cast Hexes magic.", "", ""},
  {"You can once more study and cast Hexes magic.", "", ""},
  TILEG_MUT_NO_HEXES_MAGIC,
},

{ MUT_NO_ICE_MAGIC, 0, 1, mutflag::bad, false,
  "no ice magic",

  {"You cannot study or cast Ice magic.", "", ""},
  {"You can no longer study or cast Ice magic.", "", ""},
  {"You can once more study and cast Ice magic.", "", ""},
  TILEG_MUT_NO_ICE_MAGIC,
},

{ MUT_NO_NECROMANCY_MAGIC, 0, 1, mutflag::bad, false,
  "no necromancy magic",

  {"You cannot study or cast Necromancy magic.", "", ""},
  {"You can no longer study or cast Necromancy magic.", "", ""},
  {"You can once more study and cast Necromancy magic.", "", ""},
  TILEG_MUT_NO_NECROMANCY_MAGIC,
},

{ MUT_NO_ALCHEMY_MAGIC, 0, 1, mutflag::bad, false,
  "no alchemy magic",

  {"You cannot study or cast Alchemy magic.", "", ""},
  {"You can no longer study or cast Alchemy magic.", "", ""},
  {"You can once more study and cast Alchemy magic.", "", ""},
  TILEG_MUT_NO_ALCHEMY_MAGIC,
},

{ MUT_NO_FORGECRAFT_MAGIC, 0, 1, mutflag::bad, false,
  "no forgecraft magic",

  {"You cannot study or cast Forgecraft magic.", "", ""},
  {"You can no longer study or cast Forgecraft magic.", "", ""},
  {"You can once more study and cast Forgecraft magic.", "", ""},
  TILEG_MUT_NO_FORGECRAFT_MAGIC,
},

{ MUT_NO_SUMMONING_MAGIC, 0, 1, mutflag::bad, false,
  "no summoning magic",

  {"You cannot study or cast Summoning magic.", "", ""},
  {"You can no longer study or cast Summoning magic.", "", ""},
  {"You can once more study and cast Summoning magic.", "", ""},
  TILEG_MUT_NO_SUMMONING_MAGIC,
},

{ MUT_NO_TRANSLOCATION_MAGIC, 0, 1, mutflag::bad, false,
  "no translocations magic",

  {"You cannot study or cast Translocations magic.", "", ""},
  {"You can no longer study or cast Translocations magic.", "", ""},
  {"You can once more study and cast Translocations magic.", "", ""},
  TILEG_MUT_NO_TRANSLOCATION_MAGIC,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_NO_TRANSMUTATION_MAGIC, 0, 1, mutflag::bad, false,
  "no transmutations magic",

  {"You cannot study or cast Transmutations magic.", "", ""},
  {"You can no longer study or cast Transmutations magic.", "", ""},
  {"You can once more study and cast Transmutations magic.", "", ""},
},
#endif

{ MUT_PHYSICAL_VULNERABILITY, 0, 3, mutflag::bad, false,
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

{ MUT_SLOW_REFLEXES, 0, 3, mutflag::bad, false,
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

{ MUT_WEAK_WILLED, 0, 3, mutflag::bad, false,
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

{ MUT_ANTI_WIZARDRY, 0, 3, mutflag::bad, false,
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

{ MUT_MP_WANDS, 7, 1, mutflag::good, false,
  "MP-powered wands",

  {"You expend magic power (3 MP) to strengthen your wands.", "", ""},
  {"You feel your magical essence link to the dungeon's wands.", "", ""},
  {"Your magical essence no longer links to wands of the dungeon.", "", ""},
  TILEG_MUT_MP_WANDS,
},

{ MUT_UNSKILLED, 0, 3, mutflag::bad, false,
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

{ MUT_INEXPERIENCED, 0, 3, mutflag::bad, false,
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

{ MUT_PAWS, 0, 1, mutflag::good, true,
  "stealthy paws",

  {"Your paws help you pounce on unaware monsters.", "", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_PAWS,
},

{ MUT_MISSING_EYE, 0, 1, mutflag::bad, false,
  "missing an eye",

  {"You are missing an eye, making it more difficult to aim.", "", ""},
  {"Your right eye vanishes! The world loses its depth.", "", ""},
  {"Your right eye suddenly reappears! The world regains its depth.", "", ""},
  TILEG_MUT_MISSING_EYE,
},

{ MUT_TEMPERATURE_SENSITIVITY, 0, 1, mutflag::bad, false,
  "temperature sensitive",

  {"You are sensitive to extremes of temperature. (rF-, rC-)", "", ""},
  {"You feel sensitive to extremes of temperature.", "", ""},
  {"You no longer feel sensitive to extremes of temperature", "", ""},
  TILEG_MUT_TEMPERATURE_SENSITIVITY,
},


{ MUT_NO_FORMS, 0, 1, mutflag::bad, false,
  "no forms",

  {"You cannot voluntarily change form.", "", ""},
  {"You can no longer voluntarily change form.", "", ""},
  {"You can once more change form.", "", ""},
  TILEG_MUT_NO_FORMS,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_NO_REGENERATION, 0, 1, mutflag::bad, false,
  "no regeneration",

  {"You do not regenerate.", "", ""},
  {"You stop regenerating.", "", ""},
  {"You start regenerating.", "", ""},
},
#endif

{ MUT_STRONG_NOSE, 0, 1, mutflag::good, false,
  "strong nose",

  {"Your uncanny sense of smell can sniff out nearby items.", "", ""},
  {"Your sense of smell grows stronger.", "", ""},
  {"Your sense of smell gets weaker.", "", ""},
},

{ MUT_ACID_RESISTANCE, 0, 1, mutflag::good, true,
  "acid resistance",

  {"You are resistant to acid. (rCorr)", "", ""},
  {"You feel resistant to acid.", "",  ""},
  {"You feel less resistant to acid.", "", ""},
  TILEG_MUT_ACID_RES,
},

{ MUT_QUADRUMANOUS, 0, 1, mutflag::good, false,
  "four strong arms",

  {"Your four strong arms can wield two-handed weapons with a shield.", "", ""},
  {"Two of your arms shrink away.", "", ""},
  {"You grow two extra arms.", "", ""},
},

{ MUT_NO_DRINK, 0, 1, mutflag::bad, false,
  "no potions",

  {"You do not drink.", "", ""},
  {"Your mouth dries to ashes.", "", ""},
  {"You gain the ability to drink.", "", ""},
  TILEG_MUT_NO_DRINK,
},

{ MUT_FAITH, 0, 1, mutflag::good, false,
  "faith",

  {"You have a special connection with the divine. (Faith)", "", ""},
  {"You feel connected to something greater than you.", "", ""},
  {"You feel rebellious.", "", ""},
  TILEG_MUT_FAITH,
},

{ MUT_REFLEXIVE_HEADBUTT, 0, 1, mutflag::good, true,
  "retaliatory headbutt",

  {"You reflexively headbutt those who attack you in melee.", "", ""},
  {"Your retaliatory reflexes feel sharp.", "", ""},
  {"Your retaliatory reflexes feel dull.", "", ""},
},

{ MUT_STEAM_RESISTANCE, 0, 1, mutflag::good, true,
  "steam resistance",

  {"You are immune to the effects of steam.", "", ""},
  {"You are now immune to the effects of steam.", "", ""},
  {"You are no longer immune to the effects of steam.", "", ""},
  TILEG_MUT_STEAM_RES,
},

{ MUT_NO_GRASPING, 0, 1, mutflag::bad, false,
  "no weapons or thrown items",

  {"You are incapable of wielding weapons or throwing items.", "", ""},
  {"You can no longer grasp objects.", "", ""},
  {"You can now grasp objects.", "", ""},
  TILEG_MUT_NO_GRASPING,
},

{ MUT_NO_ARMOUR, 0, 1, mutflag::bad, false,
  "no armour",

  {"You cannot wear armour.", "", ""},
  {"You can no longer wear armour.", "", ""},
  {"You can now wear armour.", "", ""},
  TILEG_MUT_NO_ARMOUR,
},

{ MUT_MULTILIVED, 0, 1, mutflag::good, false,
  "multi-lived",

  {"You gain extra lives every three experience levels.", "", ""},
  {"You are no longer multi-lived.", "", ""},
  {"You can now gain extra lives.", "", ""},
  TILEG_MUT_MULTILIVED,
},

{ MUT_DISTRIBUTED_TRAINING, 0, 1, mutflag::good, false,
  "distributed training",

  {"Your experience applies equally to all skills.", "", ""},
  {"Your experience now applies equally to all skills.", "", ""},
  {"Your experience no longer applies equally to all skills.", "", ""},
},

{ MUT_NIMBLE_SWIMMER, 0, 2, mutflag::good, true,
  "nimble swimmer",

  {"You are camouflaged when in or above water. (Stealth+)",
   "You are very nimble when in or above water. (Stealth+, EV+, Speed+++)", ""},
  {"You feel comfortable near water.",
   "You feel very comfortable near water.", ""},
  {"You feel less comfortable near water.",
   "You feel less comfortable near water.", ""},
},

{ MUT_TENTACLE_ARMS, 0, 1, mutflag::good, true,
  "tentacles",

  {"You have tentacles for arms and can constrict enemies.", "", ""},
  {"Your arms feel tentacular.", "", ""},
  {"Your arms no longer feel tentacular.", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_VAMPIRISM, 0, 2, mutflag::good, false,
  "vampiric",

  {"You are afflicted with vampirism.", "You are afflicted with vampirism and can become a bat while bloodless.", ""},
  {"You feel a craving for blood.", "You can now turn into a vampire bat when bloodless.", ""},
  {"Your craving for blood subsides.", "You can no longer turn into a bat.", ""},
  0,
  {"", "You will be able to turn into a vampire bat when bloodless.", ""}
},
#endif

{ MUT_MERTAIL, 0, 1, mutflag::good, true,
  "mertail",

  {"Your lower body shifts to a powerful aquatic tail in water.", "", ""},
  {"Your legs feel aquatic.", "", ""},
  {"Your legs no longer feel aquatic."},

  TILEG_MUT_MERTAIL,
},

{ MUT_FLOAT, 0, 1, mutflag::good, false,
  "float",

  {"You float through the air rather than walking.", "", ""},
  {"You feel both weightless and legless.", "", ""},
  {"You feel dragged down by the weight of the world."},
  TILEG_MUT_FLOAT,
},

{ MUT_INNATE_CASTER, 0, 1, mutflag::good, false,
  "innate caster",

  {"You learn spells naturally, not from books.", "", ""},
  {"You feel mystic power welling inside you.", "", ""},
  {"You feel a greater respect for book-learning."},
},

{ MUT_HP_CASTING, 0, 1, mutflag::good, false,
  "HP casting",

  {"Your magical power is your life essence.", "", ""},
  {"Your magical power and health merge together.", "", ""},
  {"Your life and magic unlink."},
  TILEG_MUT_HP_CASTING,
},

// XX why does this have 3 levels, only 1 is used
{ MUT_FLAT_HP, 0, 3, mutflag::good, false,
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

{ MUT_ENGULF, 0, 1, mutflag::good | mutflag::jiyva, true,
  "engulf",

    {"Your melee attacks may engulf your foes in ooze.", "", ""},
    {"You begin exuding ooze.", "", ""},
    {"You stop exuding ooze.", "", ""},
  TILEG_MUT_ENGULF,
},

// Sadly console size restrictions prevent more than one level of this existing
{ MUT_DAYSTALKER, 0, 1, mutflag::good, false,
  "+LOS",

    {"You have an extended range of vision and can be seen from far away.",
      "", ""},
    {"The darkness flees at your approach.", "", ""},
    {"The shadows grow bolder once more.", "", ""},
},

{ MUT_DIVINE_ATTRS, 0, 1, mutflag::good, false,
  "divine attributes",
  {"Your divine heritage dramatically boosts your attributes as you level up.", "", ""},
  {"You feel more divine.", "", ""},
  {"You feel more mortal.", "", ""},
  TILEG_MUT_DIVINE_ATTRIBUTES,
},

#if TAG_MAJOR_VERSION == 34
{ MUT_DEVOUR_ON_KILL, 0, 1, mutflag::good, true,
  "devour on kill",
  {"You thrive by killing the living.", "", ""},
  {"You feel hungry for flesh.", "", ""},
  {"You feel less hungry for flesh.", "", ""},
},
#endif

{ MUT_SHORT_LIFESPAN, 0, 1, mutflag::bad, false,
  "otherworldly",
  {"You are easily found by Zot.", "", ""},
  {"You feel your time running out.", "", ""},
  {"You feel long-lived.", "", ""},
},

{ MUT_FOUL_SHADOW, 0, 3, mutflag::good, false,
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

{ MUT_EXPLORE_REGEN, 0, 1, mutflag::good, false,
  "explore regen",
  {"You regain HP and MP as you explore.", "", ""},
  {"You feel a fierce wanderlust.", "", ""},
  {"You feel like a homebody.", "", ""},
  TILEG_MUT_EXPLORE_REGEN,
},

{ MUT_DOUBLE_POTION_HEAL, 0, 1, mutflag::good, false,
  "double potion healing",
  {"You gain doubled healing and magic from potions.", "", ""},
  {"You heal twice as much from potions.", "", ""},
  {"You no longer heal twice as much from potions.", "", ""},
},

{ MUT_DRUNKEN_BRAWLING, 0, 1, mutflag::good, false,
  "drunken brawling",
  {"Whenever you drink a healing potion, you attack all around you.", "", ""},
  {"You brawl whenever you drink a healing potion.", "", ""},
  {"You no longer brawl whenever you drink a healing potion.", "", ""},
},

{ MUT_ARTEFACT_ENCHANTING, 0, 1, mutflag::good, false,
  "artefact enchanting",
  {"You can use scrolls of enchantment on lesser artefacts.", "", ""},
  {"You can now use scrolls of enchantment on lesser artefacts.", "", ""},
  {"You can no longer use scrolls of enchantment on lesser artefacts.", "", ""},
},

{ MUT_RUNIC_MAGIC, 0, 1, mutflag::good, false,
  "runic magic",
  {"Your spellcasting is much less encumbered by armour.", "", ""},
  {"Your spellcasting becomes less encumbered by armour.", "", ""},
  {"Your spellcasting no longer less encumbered by armour.", "", ""},
},

{ MUT_FORMLESS, 0, 2, mutflag::good, true,
  "formless",

  {"You can equip up to 6 pieces of aux armour in any combination.",
   "You can equip up to 6 pieces of aux armour in any combination and unleash them.",
   ""},
  {"", "You feel ready to unleash a true cacophony.", ""},
  {"", "", ""},
  TILEG_MUT_FORMLESS,
  {"", "You will be able to unleash your equipped armour.", ""},
},

{ MUT_TRICKSTER, 0, 1, mutflag::good, false,
  "trickster",

  {"You gain AC when you inflict magical misfortune on nearby enemies.", "", ""},
  {"", "", ""},
  {"", "", ""},
  TILEG_MUT_TRICKSTER,
},

{ MUT_MNEMOPHAGE, 0, 1, mutflag::good, false,
   "mnemnophage",
   {"You can enhance your damage-dealing spells by burning harvested memories.", ""},
   {"Your flames flicker hungrily.", "", ""},
   {"Your flames grow less ravenous.", "", ""},
   TILEG_MUT_MNEMOPHAGE,
   {"You will be able to burn memories to enhance your damage-dealing spells."}
 },

{ MUT_SPELLCLAWS, 0, 1, mutflag::good, false,
   "spellclaws",
   {"You perform a melee attack whenever you cast damage-dealing spells.", "", ""},
   {"You feel destructive magic coursing through your claws.", "", ""},
   {"You no longer feel destructive magic coursing through your claws.", "", ""},
   TILEG_MUT_SPELLCLAWS,
},

// Makhleb-specific mutations

{ MUT_MAKHLEB_DESTRUCTION_GEH, 0, 1, mutflag::good, false,
  "Gehenna destruction",

  {"You draw destruction from the endless fires of Gehenna.", "", ""},
  {"You feel your soul grow hot with the fiery destruction of Gehenna.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_GEHENNA,
},

{ MUT_MAKHLEB_DESTRUCTION_COC, 0, 1, mutflag::good, false,
  "Cocytus destruction",

  {"You draw destruction from the frigid wastes of Cocytus.", "", ""},
  {"You feel your soul grow cold with the frigid destruction of Cocytus.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_COCYTUS,
},

{ MUT_MAKHLEB_DESTRUCTION_TAR, 0, 1, mutflag::good, false,
  "Tartarus destruction",

  {"You draw destruction from the wailing grief of Tartarus.", "", ""},
  {"You feel your soul grow tainted with the deathly destruction of Tartarus.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_TARTARUS,
},

{ MUT_MAKHLEB_DESTRUCTION_DIS, 0, 1, mutflag::good, false,
  "Dis destruction",

  {"You draw destruction from the ruthless spite of Dis.", "", ""},
  {"You feel your soul grow bitter with the corrosive destruction of Dis.", "", ""},
  {"", "", ""},

  TILEG_MUT_MAJOR_DESTRUCTION_OF_DIS,
},

{ MUT_MAKHLEB_MARK_HAEMOCLASM, 0, 1, mutflag::makhleb, false,
  "Mark of Haemoclasm",

  {"You bear the Mark of Haemoclasm.", "", ""},
  {"Gore shall rain upon your enemies.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_HAEMOCLASM,
},

{ MUT_MAKHLEB_MARK_LEGION, 0, 1, mutflag::makhleb, false,
  "Mark of the Legion",

  {"You bear the Mark of the Legion.", "", ""},
  {"The armies of chaos are now yours to lead.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_LEGION,
},

{ MUT_MAKHLEB_MARK_CARNAGE, 0, 1, mutflag::makhleb, false,
  "Mark of Carnage",

  {"You bear the Mark of Carnage.", "", ""},
  {"Your servants grow eager to unleash destruction.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_CARNAGE,
},

{ MUT_MAKHLEB_MARK_ANNIHILATION, 0, 1, mutflag::makhleb, false,
  "Mark of Annihilation",

  {"You bear the Mark of Annihilation.", "", ""},
  {"You feel the destructive energies within you surge violently.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_ANNIHILATION,
},

{ MUT_MAKHLEB_MARK_TYRANT, 0, 1, mutflag::makhleb, false,
  "Mark of the Tyrant",

  {"You bear the Mark of the Tyrant.", "", ""},
  {"Even fiends shall kneel before you now.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_TYRANT,
},

{ MUT_MAKHLEB_MARK_CELEBRANT, 0, 1, mutflag::makhleb, false,
  "Mark of the Celebrant",

  {"You bear the Mark of the Celebrant.", "", ""},
  {"Your suffering will be repaid in blood.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_CELEBRANT,
},

{ MUT_MAKHLEB_MARK_EXECUTION, 0, 1, mutflag::makhleb, false,
  "Mark of Execution",

  {"You bear the Mark of Execution.", "", ""},
  {"Murder takes root in your soul.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_EXECUTION,
},

{ MUT_MAKHLEB_MARK_ATROCITY, 0, 1, mutflag::makhleb, false,
  "Mark of Atrocity",

  {"You bear the Mark of Atrocity.", "", ""},
  {"You will scour this world clean.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_ATROCITY,
},

{ MUT_MAKHLEB_MARK_FANATIC, 0, 1, mutflag::makhleb, false,
  "Mark of the Fanatic",

  {"You bear the Mark of the Fanatic.", "", ""},
  {"You will become an instrument of Makhleb's will.", "", ""},
  {"", "", ""},

  TILEG_MUT_MARK_OF_THE_FANATIC,
},

};

static const mutation_category_def category_mut_data[] =
{
  { RANDOM_MUTATION, "any"},
  { RANDOM_XOM_MUTATION, "xom"},
  { RANDOM_GOOD_MUTATION, "good"},
  { RANDOM_BAD_MUTATION, "bad"},
  { RANDOM_SLIME_MUTATION, "slime"},
  { RANDOM_NON_SLIME_MUTATION, "nonslime"},
  { RANDOM_CORRUPT_MUTATION, "corrupt"},
  { RANDOM_QAZLAL_MUTATION, "qazlal"},
};
