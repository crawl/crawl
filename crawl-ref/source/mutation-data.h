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
};

static const mutation_def mut_data[] =
{

{ MUT_TOUGH_SKIN, 0, 3, mutflag::GOOD, true,
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

{ MUT_STRONG, 7, 2, mutflag::GOOD, false,
  "strong",

  {"Your muscles are strong. (Str +2)",
   "Your muscles are very strong. (Str +4)", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_CLEVER, 7, 2, mutflag::GOOD, false,
  "clever",

  {"Your mind is acute. (Int +2)",
   "Your mind is very acute. (Int +4)", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_AGILE, 7, 2, mutflag::GOOD, false,
  "agile",

  {"You are agile. (Dex +2)",
   "You are very agile. (Dex +4)", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_POISON_RESISTANCE, 4, 1, mutflag::GOOD, true,
  "poison resistance",

  {"Your system is resistant to poisons. (rPois)", "", ""},
  {"You feel resistant to poisons.", "",  ""},
  {"You feel less resistant to poisons.", "", ""},
},

{ MUT_CARNIVOROUS, 0, 3, mutflag::GOOD, false,
  "carnivore",

  {"Your digestive system is specialised to digest meat.",
   "Your digestive system is highly specialised to digest meat.",
   "You are carnivorous and can eat meat at any time."},

  {"You hunger for flesh.",
   "You hunger for flesh.",
   "You hunger for flesh."},

  {"You feel able to eat a more balanced diet.",
   "You feel able to eat a more balanced diet.",
   "You feel able to eat a more balanced diet."},
},

{ MUT_HERBIVOROUS, 0, 3, mutflag::BAD, false,
  "herbivore",

  {"You digest meat inefficiently.",
   "You digest meat very inefficiently.",
   "You are a herbivore."},

  {"You hunger for vegetation.",
   "You hunger for vegetation.",
   "You hunger for vegetation."},

  {"You feel able to eat a more balanced diet.",
   "You feel able to eat a more balanced diet.",
   "You feel able to eat a more balanced diet."},
},

{ MUT_HEAT_RESISTANCE, 4, 3, mutflag::GOOD, true,
  "fire resistance",

  {"Your flesh is heat resistant. (rF+)",
   "Your flesh is very heat resistant. (rF++)",
   "Your flesh is almost immune to the effects of heat. (rF+++)"},

  {"You feel resistant to heat.",
   "You feel more resistant to heat.",
   "You feel more resistant to heat."},

  {"You no longer feel heat resistant.",
   "You feel less heat resistant.",
   "You feel less heat resistant."},
},

{ MUT_COLD_RESISTANCE, 4, 3, mutflag::GOOD, true,
  "cold resistance",

  {"Your flesh is cold resistant. (rC+)",
   "Your flesh is very cold resistant. (rC++)",
   "Your flesh is almost immune to the effects of cold. (rC+++)"},

  {"You feel resistant to cold.",
   "You feel more resistant to cold.",
   "You feel more resistant to cold."},

  {"You no longer feel cold resistant.",
   "You feel less cold resistant.",
   "You feel less cold resistant."},
},

{ MUT_HEAT_VULNERABILITY, 0, 3,
  mutflag::BAD | mutflag::QAZLAL, true,
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
},

{ MUT_COLD_VULNERABILITY, 0, 3,
  mutflag::BAD | mutflag::QAZLAL, true,

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
},

{ MUT_DEMONIC_GUARDIAN, 0, 3, mutflag::GOOD, false,
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
},

{ MUT_SHOCK_RESISTANCE, 2, 1, mutflag::GOOD, true,
  "electricity resistance",

  {"You are resistant to electric shocks. (rElec)", "", ""},
  {"You feel insulated.", "", ""},
  {"You feel conductive.", "", ""},
},

{ MUT_SHOCK_VULNERABILITY, 0, 1, mutflag::BAD | mutflag::QAZLAL, true,
  "electricity vulnerability",

  {"You are vulnerable to electric shocks.", "", ""},
  {"You feel vulnerable to electricity.", "", ""},
  {"You feel less vulnerable to electricity.", "", ""},
},

{ MUT_REGENERATION, 2, 3, mutflag::GOOD, false,
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
},

{ MUT_INHIBITED_REGENERATION, 3, 1, mutflag::BAD, false,
  "inhibited regeneration",

  {"You do not regenerate when monsters are visible.", "", ""},

  {"Your regeneration stops near monsters.", "", ""},

  {"You begin to regenerate regardless of the presence of monsters.", "", ""},
},

{ MUT_FAST_METABOLISM, 0, 3, mutflag::BAD, false,
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

{ MUT_SLOW_METABOLISM, 0, 2, mutflag::GOOD, false,
  "slow metabolism",

  {"You have a slow metabolism.",
   "You need consume almost no food.",
   ""},

  {"Your metabolism slows.",
   "Your metabolism slows.",
   ""},

  {"You feel a little hungry.",
   "You feel a little hungry.",
   ""},
},

{ MUT_WEAK, 8, 2, mutflag::BAD | mutflag::XOM, false,
  "weak",

  {"You are weak. (Str -2)",
   "You are very weak. (Str -4)", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_DOPEY, 8, 2, mutflag::BAD | mutflag::XOM, false,
  "dopey",

  {"You are dopey. (Int -2)",
   "You are very dopey. (Int -4)", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_CLUMSY, 8, 2, mutflag::BAD | mutflag::XOM, false,
  "clumsy",

  {"You are clumsy. (Dex -2)",
   "You are very clumsy. (Dex -4)", ""},
  {"", "", ""},
  {"", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_TELEPORT_CONTROL, 0, 1, mutflag::GOOD, false,
  "teleport control",

  {"You can control translocations.", "", ""},
  {"You feel controlled.", "", ""},
  {"You feel random.", "", ""},
},
#endif

{ MUT_TELEPORT, 3, 3, mutflag::BAD, false,
  "teleportitis",

  {"You are occasionally teleported next to monsters.",
   "You are sometimes teleported next to monsters.",
   "You are often teleported next to monsters."},

  {"You feel weirdly uncertain.",
   "You feel even more weirdly uncertain.",
   "You feel even more weirdly uncertain."},

  {"You feel stable.",
   "You feel stable.",
   "You feel stable."},
},

{ MUT_MAGIC_RESISTANCE, 5, 3, mutflag::GOOD, false,
  "magic resistance",

  {"You are resistant to hostile enchantments. (MR+)",
   "You are highly resistant to hostile enchantments. (MR++)",
   "You are extremely resistant to the effects of hostile enchantments. (MR+++)"},

  {"You feel resistant to hostile enchantments.",
   "You feel more resistant to hostile enchantments.",
   "You feel almost impervious to the effects of hostile enchantments."},

  {"You feel less resistant to hostile enchantments.",
   "You feel less resistant to hostile enchantments.",
   "You feel vulnerable to hostile enchantments."},
},

{ MUT_FAST, 0, 3, mutflag::GOOD, true,
  "speed",

  {"You cover ground quickly.",
   "You cover ground very quickly.",
   "You cover ground extremely quickly."},

  {"You feel quick.",
   "You feel quick.",
   "You feel quick."},

  {"You feel sluggish.",
   "You feel sluggish.",
   "You feel sluggish."},
},

{ MUT_SLOW, 0, 3, mutflag::BAD, true,
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

{ MUT_ACUTE_VISION, 2, 1, mutflag::GOOD, false,
  "see invisible",

  {"You have supernaturally acute eyesight. (SInv)", "", ""},
  {"Your vision sharpens.", "", ""},
  {"Your vision seems duller.", "", ""},
},

{ MUT_DEFORMED, 8, 1,
  mutflag::BAD | mutflag::XOM,
  true,
  "deformed body",

  {"Armour fits poorly on your strangely shaped body.", "", ""},
  {"Your body twists and deforms.", "", ""},
  {"Your body's shape seems more normal.", "", ""},
},

{ MUT_SPIT_POISON, 8, 2, mutflag::GOOD, false,
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
},

#if TAG_MAJOR_VERSION == 34
{ MUT_BREATHE_FLAMES, 0, 3, mutflag::GOOD, false,
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

{ MUT_JUMP, 0, 3, mutflag::GOOD, false,
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
#endif
{ MUT_BLINK, 3, 1, mutflag::GOOD, false,
  "blink",

  {"You can translocate small distances at will.", "", ""},
  {"You feel jittery.", "", ""},
  {"You no longer feel jittery.", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_STRONG_STIFF, 0, 3, mutflag::GOOD, false,
  "strong stiff",

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

{ MUT_FLEXIBLE_WEAK, 0, 3, mutflag::GOOD, false,
  "flexible weak",

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

{ MUT_SCREAM, 6, 3, mutflag::BAD | mutflag::XOM, false,
  "screaming",

  {"You occasionally shout uncontrollably at your foes.",
   "You sometimes yell uncontrollably at your foes.",
   "You frequently scream uncontrollably at your foes."},

  {"You feel the urge to shout.",
   "You feel a strong urge to yell.",
   "You feel a strong urge to scream."},

  {"Your urge to shout disappears.",
   "Your urge to yell lessens.",
   "Your urge to scream lessens."},
},

{ MUT_CLARITY, 6, 1, mutflag::GOOD, false,
  "clarity",

  {"You possess an exceptional clarity of mind.", "", ""},
  {"Your thoughts seem clearer.", "", ""},
  {"Your thinking seems confused.", "", ""},
},

{ MUT_BERSERK, 7, 3, mutflag::BAD, false,
  "berserk",

  {"You tend to lose your temper in combat.",
   "You often lose your temper in combat.",
   "You have an uncontrollable temper."},

  {"You feel a little pissed off.",
   "You feel angry.",
   "You feel extremely angry at everything!"},

  {"You feel a little more calm.",
   "You feel a little less angry.",
   "You feel a little less angry."},
},

{ MUT_DETERIORATION, 10, 2, mutflag::BAD | mutflag::XOM, false,
  "deterioration",

  {"Your body sometimes deteriorates upon taking damage.",
   "Your body often deteriorates upon taking damage.",
   ""},

  {"You feel yourself wasting away.",
   "You feel your body start to fall apart.",
   ""},

  {"You feel healthier.",
   "You feel a little healthier.",
   ""},
},

{ MUT_BLURRY_VISION, 10, 3, mutflag::BAD | mutflag::XOM, false,
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

{ MUT_MUTATION_RESISTANCE, 4, 3, mutflag::GOOD, false,
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
},

{ MUT_EVOLUTION, 4, 2, mutflag::GOOD, false,
  "evolution",

  {"You evolve.",
   "You rapidly evolve.",
   ""},

  {"You feel nature experimenting on you. Don't worry, failures die fast.",
   "Your genes go into a fast flux.",
   ""},

  {"Nature stops experimenting on you.",
   "Your wild genetic ride slows down.",
   ""},
},

{ MUT_FRAIL, 10, 3,
  mutflag::BAD | mutflag::XOM, false,
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
},

{ MUT_ROBUST, 5, 3, mutflag::GOOD, false,
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
},

{ MUT_UNBREATHING, 0, 1, mutflag::GOOD, true,
  "unbreathing",

  {"You can survive without breathing.", "", ""},
  {"You feel breathless.", "", ""},
  {"", "", ""},
},

{ MUT_TORMENT_RESISTANCE, 0, 1, mutflag::GOOD, false,
  "torment resistance",

  {"You are immune to unholy pain and torment.", "", ""},
  {"You feel a strange anaesthesia.", "", ""},
  {"", "", ""},
},

{ MUT_NEGATIVE_ENERGY_RESISTANCE, 0, 3, mutflag::GOOD, false,
  "negative energy resistance",

  {"You resist negative energy. (rN+)",
   "You are quite resistant to negative energy. (rN++)",
   "You are immune to negative energy. (rN+++)"},

  {"You feel resistant to negative energy.",
   "You feel more resistant to negative energy.",
   "You feel more resistant to negative energy."},

  {"", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_MUMMY_RESTORATION, 0, 1, mutflag::GOOD, false,
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

{ MUT_NECRO_ENHANCER, 0, 2, mutflag::GOOD, false,
  "in touch with death",

  {"You are in touch with the powers of death.",
   "You are strongly in touch with the powers of death.",
   ""},

  {"You feel more in touch with the powers of death.",
   "You feel more in touch with the powers of death.",
   ""},

  {"", "", ""},
},

{ MUT_TENGU_FLIGHT, 0, 2, mutflag::GOOD, false,
  "able to fly",

  {"You can fly.",
   "You can fly continuously.",
   ""},

  {"You have gained the ability to fly.",
   "You can now fly continuously.",
   ""},

  {"", "", ""},
},

{ MUT_HURL_DAMNATION, 0, 1, mutflag::GOOD, false,
  "hurl damnation",

  {"You can hurl damnation.", "", ""},
  {"You smell a hint of brimstone.", "", ""},
  {"", "", ""},
},

// body-slot facets
{ MUT_HORNS, 7, 3, mutflag::GOOD, true,
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
},

{ MUT_BEAK, 1, 1, mutflag::GOOD, true,
  "beak",

  {"You have a beak for a mouth.", "", ""},
  {"Your mouth lengthens and hardens into a beak!", "", ""},
  {"Your beak shortens and softens into a mouth.", "", ""},
},

{ MUT_CLAWS, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_FANGS, 1, 3, mutflag::GOOD, true,
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
},

{ MUT_HOOVES, 5, 3, mutflag::GOOD, true,
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
},

{ MUT_ANTENNAE, 4, 3, mutflag::GOOD, true,
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
},

{ MUT_TALONS, 5, 3, mutflag::GOOD, true,
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
},

// Octopode only
{ MUT_TENTACLE_SPIKE, 10, 3, mutflag::GOOD, true,
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
},
#if TAG_MAJOR_VERSION == 34

{ MUT_BREATHE_POISON, 0, 1, mutflag::GOOD, false,
  "breathe poison",

  {"You can exhale a cloud of poison.", "", ""},
  {"You taste something nasty.", "", ""},
  {"Your breath is less nasty.", "", ""},
},
#endif

{ MUT_CONSTRICTING_TAIL, 0, 1, mutflag::GOOD, true,
  "constrict 1",

  {"You can use your snake-like lower body to constrict enemies.", "", ""},
  {"Your tail grows strong enough to constrict your enemies.", "", ""},
  {"", "", ""},
},

// Naga and Draconian only
{ MUT_STINGER, 8, 3, mutflag::GOOD, true,
  "stinger",

  {"Your tail ends in a poisonous barb.",
   "Your tail ends in a sharp poisonous barb.",
   "Your tail ends in a wickedly sharp and poisonous barb."},

  {"A poisonous barb forms on the end of your tail.",
   "The barb on your tail looks sharper.",
   "The barb on your tail looks very sharp."},

  {"The barb on your tail disappears.",
   "The barb on your tail seems less sharp.",
   "The barb on your tail seems less sharp."},
},

// Draconian only
{ MUT_BIG_WINGS, 4, 1, mutflag::GOOD, true,
  "big wings",

  {"Your large and strong wings let you fly indefinitely.", "", ""},
  {"Your wings grow larger and stronger.", "", ""},
  {"Your wings shrivel and weaken.", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_SAPROVOROUS, 0, 3, mutflag::GOOD, false,
  "saprovore",

  {"You can tolerate rotten meat.",
   "You can eat rotten meat.",
   "You thrive on rotten meat."},

  {"You hunger for rotting flesh.",
   "You hunger for rotting flesh.",
   "You hunger for rotting flesh."},

  {"", "", ""},
},

#endif
// species-dependent innate mutations
{ MUT_ROT_IMMUNITY, 0, 1, mutflag::GOOD, false,
  "rot immunity",

  {"You are immune to rotting.", "", ""},
  {"You feel immune to rotting.", "", ""},
  {"You feel vulnerable to rotting.", "", ""},
},

{ MUT_GOURMAND, 0, 1, mutflag::GOOD, false,
  "gourmand",

  {"You like to eat raw meat.", "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_HOP, 0, 2, mutflag::GOOD, true,
  "strong legs",

  {"You can hop short distances.",
   "You can hop longer distances.",
   ""},

  {"", "Your legs feel stronger.", ""},

  {"", "", ""},
},


{ MUT_SHAGGY_FUR, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_HIGH_MAGIC, 2, 3, mutflag::GOOD, false,
  "high mp",

  {"You have an increased reservoir of magic. (+10% MP)",
   "You have a considerably increased reservoir of magic. (+20% MP)",
   "You have a greatly increased reservoir of magic. (+30% MP)"},

  {"You feel more energetic.",
   "You feel more energetic.",
   "You feel more energetic."},

  {"You feel less energetic.",
   "You feel less energetic.",
   "You feel less energetic."},
},

{ MUT_LOW_MAGIC, 9, 3, mutflag::BAD, false,
  "low mp",

  {"Your magical capacity is low. (-10% MP)",
   "Your magical capacity is very low. (-20% MP)",
   "Your magical capacity is extremely low. (-30% MP)"},

  {"You feel less energetic.",
   "You feel less energetic.",
   "You feel less energetic."},

  {"You feel more energetic.",
   "You feel more energetic.",
   "You feel more energetic."},
},

{ MUT_WILD_MAGIC, 6, 3, mutflag::GOOD, false,
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
},

{ MUT_SUBDUED_MAGIC, 6, 3, mutflag::BAD, false,
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
},

#if TAG_MAJOR_VERSION == 34
{ MUT_FORLORN, 0, 1, mutflag::BAD, false,
  "forlorn",

  {"You have difficulty communicating with the divine.","",""},
  {"You feel forlorn.","",""},
  {"You feel more spiritual.","",""},
},
#endif

{ MUT_STOCHASTIC_TORMENT_RESISTANCE, 0, 1, mutflag::GOOD, false,
  "50% torment resistance",

  {"You are somewhat able to resist unholy torments (1 in 2 success).","",""},
  {"You feel a strange anaesthesia.", "", ""},
  {"", "", ""},
},

{ MUT_PASSIVE_MAPPING, 3, 3, mutflag::GOOD, false,
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
},

{ MUT_ICEMAIL, 0, 1, mutflag::GOOD, false,
  "icemail",

  {"A meltable icy envelope protects you from harm. (AC +", "", ""},
  {"An icy envelope takes form around you.", "", ""},
  {"", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_CONSERVE_SCROLLS, 0, 1, mutflag::GOOD, false,
  "conserve scrolls",

  {"You are very good at protecting items from fire.", "", ""},
  {"You feel less concerned about heat.", "", ""},
  {"", "", ""},
},

{ MUT_CONSERVE_POTIONS, 0, 1, mutflag::GOOD, false,
  "conserve potions",

  {"You are very good at protecting items from cold.", "", ""},
  {"You feel less concerned about cold.", "", ""},
  {"", "", ""},
},
#endif

{ MUT_PASSIVE_FREEZE, 0, 1, mutflag::GOOD, false,
  "passive freeze",

  {"A frigid envelope surrounds you and freezes all who hurt you.", "", ""},
  {"Your skin feels very cold.", "", ""},
  {"", "", ""},
},

{ MUT_NIGHTSTALKER, 0, 3, mutflag::GOOD, false,
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
},

{ MUT_SPINY, 0, 3, mutflag::GOOD, true,
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
},

{ MUT_POWERED_BY_DEATH, 0, 3, mutflag::GOOD, false,
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
},

{ MUT_POWERED_BY_PAIN, 0, 3, mutflag::GOOD, false,
  "powered by pain",

  {"You sometimes gain a little power by taking damage.",
   "You sometimes gain power by taking damage.",
   "You are powered by pain."},

  {"You feel energised by your suffering.",
   "You feel even more energised by your suffering.",
   "You feel completely energised by your suffering."},

  {"", "", ""},
},

{ MUT_AUGMENTATION, 0, 3, mutflag::GOOD, false,
  "augmentation",

  {"Your magical and physical power is slightly enhanced at high health.",
   "Your magical and physical power is enhanced at high health.",
   "Your magical and physical power is greatly enhanced at high health."},

  {"You feel power flowing into your body.",
   "You feel power rushing into your body.",
   "You feel saturated with power."},

  {"", "", ""},
},

{ MUT_MANA_SHIELD, 0, 1, mutflag::GOOD, false,
  "magic shield",

  {"When hurt, damage is shared between your health and your magic reserves.", "", ""},
  {"You feel your magical essence form a protective shroud around your flesh.", "", ""},
  {"", "", ""},
},

{ MUT_MANA_REGENERATION, 0, 1, mutflag::GOOD, false,
  "magic regeneration",

  {"You regenerate magic rapidly.", "", ""},
  {"You feel your magic shroud grow more resilient.", "", ""},
  {"", "", ""},
},

{ MUT_MANA_LINK, 0, 1, mutflag::GOOD, false,
  "magic link",

  {"When low on magic, you restore magic in place of health.", "", ""},
  {"You feel your life force and your magical essence meld.", "", ""},
  {"", "", ""},
},

// Jiyva only mutations
{ MUT_GELATINOUS_BODY, 0, 3, mutflag::GOOD | mutflag::JIYVA, true,
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
},

{ MUT_EYEBALLS, 0, 3, mutflag::GOOD | mutflag::JIYVA, true,
  "eyeballs",

  {"Your body has grown golden eyes which may confuse attackers. (Acc +3)",
   "Your body has grown many golden eyes which may confuse attackers. (Acc +5)",
   "Your body is covered in golden eyes which may confuse attackers. (Acc +7, SInv)"},

  {"Eyeballs grow over part of your body.",
   "Eyeballs cover a large portion of your body.",
   "Eyeballs cover you completely."},

  {"The eyeballs on your body disappear.",
   "The eyeballs on your body recede somewhat.",
   "The eyeballs on your body recede somewhat."},
},

{ MUT_TRANSLUCENT_SKIN, 0, 3, mutflag::GOOD | mutflag::JIYVA, true,
  "translucent skin",

  {"Your translucent skin slightly reduces your foes' accuracy. (Stealth)",
   "Your translucent skin reduces your foes' accuracy. (Stealth)",
   "Your transparent skin significantly reduces your foes' accuracy. (Stealth)"},

  {"Your skin becomes partially translucent.",
   "Your skin becomes more translucent.",
   "Your skin becomes completely transparent."},

  {"Your skin returns to its normal opacity.",
   "Your skin's translucency fades.",
   "Your skin's transparency fades."},
},

{ MUT_PSEUDOPODS, 0, 3, mutflag::GOOD | mutflag::JIYVA, true,
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
},

#if TAG_MAJOR_VERSION == 34
{ MUT_FOOD_JELLY, 0, 1, mutflag::GOOD, false,
  "spawn jellies when eating",

  {"You occasionally spawn a jelly by eating.", "", ""},
  {"You feel more connected to the slimes.", "", ""},
  {"Your connection to the slimes vanishes.", "", ""},
},
#endif

{ MUT_ACIDIC_BITE, 0, 1, mutflag::GOOD | mutflag::JIYVA, true,
  "acidic bite",

  {"You have acidic saliva.", "", ""},
  {"Acid begins to drip from your mouth.", "", ""},
  {"Your mouth feels dry.", "", ""},
},

{ MUT_ANTIMAGIC_BITE, 0, 1, mutflag::GOOD, true,
  "antimagic bite",

  {"Your bite disrupts and absorbs the magic of your enemies.", "", ""},
  {"You feel a sudden thirst for magic.", "", ""},
  {"Your magical appetite wanes.", "", ""},
},

{ MUT_NO_POTION_HEAL, 3, 3, mutflag::BAD, false,
  "no potion heal",

  {"Potions are less effective at restoring your health.",
   "Potions are poor at restoring your health.",
   "Potions cannot restore your health."},

  {"Your system partially rejects the healing effects of potions.",
   "Your system mostly rejects the healing effects of potions.",
   "Your system completely rejects the healing effects of potions."},

  {"Your system completely accepts the healing effects of potions.",
   "Your system mostly accepts the healing effects of potions.",
   "Your system partly accepts the healing effects of potions."},
},

// Scale mutations
{ MUT_DISTORTION_FIELD, 2, 3, mutflag::GOOD, false,
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
},

{ MUT_ICY_BLUE_SCALES, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_IRIDESCENT_SCALES, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_LARGE_BONE_PLATES, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_MOLTEN_SCALES, 2, 3, mutflag::GOOD, true,
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
},

#if TAG_MAJOR_VERSION == 34
{ MUT_ROUGH_BLACK_SCALES, 0, 3, mutflag::GOOD, true,
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

{ MUT_RUGGED_BROWN_SCALES, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_SLIMY_GREEN_SCALES, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_THIN_METALLIC_SCALES, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_THIN_SKELETAL_STRUCTURE, 2, 3, mutflag::GOOD, false,
  "thin skeletal structure",

  {"You have a somewhat thin skeletal structure. (Dex +2, Stealth)",
   "You have a moderately thin skeletal structure. (Dex +4, Stealth+)",
   "You have an unnaturally thin skeletal structure. (Dex +6, Stealth++)"},

  {"Your bones become slightly less dense.",
   "Your bones become somewhat less dense.",
   "Your bones become less dense."},

  {"Your skeletal structure returns to normal.",
   "Your skeletal structure densifies.",
   "Your skeletal structure densifies."},
},

{ MUT_YELLOW_SCALES, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_STURDY_FRAME, 2, 3, mutflag::GOOD, true,
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
},

{ MUT_SANGUINE_ARMOUR, 0, 3, mutflag::GOOD, false,
  "sanguine armour",

  {"When seriously injured, your bleeding wounds create armour. (AC +",
   "When seriously injured, your bleeding wounds create thick armour. (AC +",
   "When seriously injured, your bleeding wounds create very thick armour. (AC +"},

  {"You feel your blood ready itself to protect you.",
   "You feel your blood thicken.",
   "You feel your blood thicken."},

  {"You feel your blood become entirely quiescent.",
   "You feel your blood thin.",
   "You feel your blood thin."},
},

{ MUT_CAMOUFLAGE, 1, 3, mutflag::GOOD, true,
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
},

{ MUT_IGNITE_BLOOD, 0, 1, mutflag::GOOD, false,
  "ignite blood",

  {"Your demonic aura causes spilled blood to erupt in flames.", "", ""},
  {"Your blood runs red-hot!", "", ""},
  {"", "", ""},
},

{ MUT_FOUL_STENCH, 0, 2, mutflag::GOOD, false,
  "foul stench",

  {"You may emit foul miasma when damaged in melee.",
   "You frequently emit foul miasma when damaged in melee.",
   ""},

  {"You begin to emit a foul stench of rot and decay.",
   "You begin to radiate miasma.",
   ""},

  {"", "", ""},
},

{ MUT_TENDRILS, 0, 1, mutflag::GOOD | mutflag::JIYVA, true,
  "tendrils",

  {"You are covered in slimy tendrils that may disarm your opponents.", "", ""},
  {"Thin, slimy tendrils emerge from your body.", "", ""},
  {"Your tendrils retract into your body.", "", ""},
},

{ MUT_JELLY_GROWTH, 0, 1, mutflag::GOOD | mutflag::JIYVA, true,
  "jelly sensing items",

  {"You have a small jelly attached to you that senses nearby items.", "", ""},
  {"Your body partially splits into a small jelly.", "", ""},
  {"The jelly growth is reabsorbed into your body.", "", ""},
},

{ MUT_JELLY_MISSILE, 0, 1, mutflag::GOOD | mutflag::JIYVA, true,
  "jelly absorbing missiles",

  {"You have a small jelly attached to you that may absorb incoming projectiles.", "", ""},
  {"Your body partially splits into a small jelly.", "", ""},
  {"The jelly growth is reabsorbed into your body.", "", ""},
},

{ MUT_PETRIFICATION_RESISTANCE, 0, 1, mutflag::GOOD, false,
  "petrification resistance",

  {"You are immune to petrification.", "", ""},
  {"Your body vibrates.", "", ""},
  {"You briefly stop moving.", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_TRAMPLE_RESISTANCE, 0, 1, mutflag::GOOD, false,
  "trample resistance",

  {"You are resistant to trampling.", "", ""},
  {"You feel steady.", "", ""},
  {"You feel unsteady..", "", ""},
},

{ MUT_CLING, 0, 1, mutflag::GOOD, true,
  "cling",

  {"You can cling to walls.", "", ""},
  {"You feel sticky.", "", ""},
  {"You feel slippery.", "", ""},
},

{ MUT_EXOSKELETON, 0, 2, mutflag::GOOD, true,
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

{ MUT_FUMES, 0, 2, mutflag::GOOD, false,
  "fuming",

  {"You emit clouds of smoke.", "You frequently emit clouds of smoke.", ""},
  {"You fume.", "You fume more.", ""},
  {"You stop fuming.", "You fume less.", ""},
},
#endif

{ MUT_BLACK_MARK, 0, 1, mutflag::GOOD, false,
  "black mark",

  {"Your melee attacks may debilitate your foes.", "", ""},
  {"An ominous black mark forms on your body.", "", ""},
  {"", "", ""},
},

{ MUT_COLD_BLOODED, 0, 1, mutflag::BAD, true,
  "cold-blooded",

  {"You are cold-blooded and may be slowed by cold attacks.", "", ""},
  {"You feel cold-blooded.", "", ""},
  {"You feel warm-blooded.", "", ""},
},

{ MUT_FLAME_CLOUD_IMMUNITY, 0, 1, mutflag::GOOD, false,
  "flame cloud immunity",

  {"You are immune to clouds of flame.", "", ""},
  {"You feel less concerned about heat.", "", ""},
  {"", "", ""},
},

{ MUT_FREEZING_CLOUD_IMMUNITY, 0, 1, mutflag::GOOD, false,
  "freezing cloud immunity",

  {"You are immune to freezing clouds.", "", ""},
  {"You feel less concerned about cold.", "", ""},
  {"", "", ""},
},

#if TAG_MAJOR_VERSION == 34
{ MUT_SUSTAIN_ATTRIBUTES, 0, 1, mutflag::GOOD, false,
    "sustain attributes",

    {"Your attributes are resistant to harm.", "", ""},
    {"", "", ""},
    {"", "", ""},
},
#endif

{ MUT_NO_DRINK, 0, 1, mutflag::BAD, false,
  "inability to drink while threatened",

  {"You cannot drink potions while threatened.", "", ""},
  {"You no longer can drink potions while threatened.", "", ""},
  {"You can once more drink potions while threatened.", "", ""},
},

{ MUT_NO_READ, 0, 1, mutflag::BAD, false,
  "inability to read while threatened",

  {"You cannot read scrolls while threatened.", "", ""},
  {"You can no longer read scrolls while threatened.", "", ""},
  {"You can once more read scrolls while threatened.", "", ""},
},

{ MUT_MISSING_HAND, 0, 1, mutflag::BAD, false,
  "missing a hand",

  {"You are missing a hand.", "", ""},
  {"One of your hands has vanished, leaving only a stump!", "", ""},
  {"Your stump has regrown into a hand!", "", ""},
},

{ MUT_NO_STEALTH, 0, 1, mutflag::BAD, false,
  "no stealth",

  {"You cannot be stealthy.", "", ""},
  {"You can no longer be stealthy.", "", ""},
  {"You can once more be stealthy.", "", ""},
},

{ MUT_NO_ARTIFICE, 0, 1, mutflag::BAD, false,
  "inability to use devices",

  {"You cannot study or use magical devices.", "", ""},
  {"You can no longer study or use magical devices.", "", ""},
  {"You can once more study and use magical devices.", "", ""},
},

{ MUT_NO_LOVE, 0, 1, mutflag::BAD, false,
  "hated by all",

  {"You are hated by all.", "", ""},
  {"You are now hated by all.", "", ""},
  {"You are no longer hated by all.", "", ""},
},

{ MUT_COWARDICE, 0, 1, mutflag::BAD, false,
  "cowardly",

  {"Your cowardice makes you less effective in combat with threatening monsters.", "", ""},
  {"You have lost your courage.", "", ""},
  {"You have regained your courage.", "", ""},
},

{ MUT_NO_DODGING, 0, 1, mutflag::BAD, false,
  "inability to train dodging",

  {"You cannot train Dodging skill.", "", ""},
  {"You can no longer train Dodging skill.", "", ""},
  {"You can once more train Dodging skill.", "", ""},
},

{ MUT_NO_ARMOUR, 0, 1, mutflag::BAD, false,
  "inability to train armour",

  {"You cannot train Armour skill.", "", ""},
  {"You can no longer train Armour skill.", "", ""},
  {"You can once more train Armour skill.", "", ""},
},

{ MUT_NO_AIR_MAGIC, 0, 1, mutflag::BAD, false,
  "no air magic",

  {"You cannot study or cast Air magic.", "", ""},
  {"You can no longer study or cast Air magic.", "", ""},
  {"You can once more study and cast Air magic.", "", ""},
},

{ MUT_NO_CHARM_MAGIC, 0, 1, mutflag::BAD, false,
  "no charms magic",

  {"You cannot study or cast Charms magic.", "", ""},
  {"You can no longer study or cast Charms magic.", "", ""},
  {"You can once more study and cast Charms magic.", "", ""},
},

{ MUT_NO_CONJURATION_MAGIC, 0, 1, mutflag::BAD, false,
  "no conjurations magic",

  {"You cannot study or cast Conjurations magic.", "", ""},
  {"You can no longer study or cast Conjurations magic.", "", ""},
  {"You can once more study and cast Conjurations magic.", "", ""},
},

{ MUT_NO_EARTH_MAGIC, 0, 1, mutflag::BAD, false,
  "no earth magic",

  {"You cannot study or cast Earth magic.", "", ""},
  {"You can no longer study or cast Earth magic.", "", ""},
  {"You can once more study and cast Earth magic.", "", ""},
},

{ MUT_NO_FIRE_MAGIC, 0, 1, mutflag::BAD, false,
  "no fire magic",

  {"You cannot study or cast Fire magic.", "", ""},
  {"You can no longer study or cast Fire magic.", "", ""},
  {"You can once more study and cast Fire magic.", "", ""},
},

{ MUT_NO_HEXES_MAGIC, 0, 1, mutflag::BAD, false,
  "no hexes magic",

  {"You cannot study or cast Hexes magic.", "", ""},
  {"You can no longer study or cast Hexes magic.", "", ""},
  {"You can once more study and cast Hexes magic.", "", ""},
},

{ MUT_NO_ICE_MAGIC, 0, 1, mutflag::BAD, false,
  "no ice magic",

  {"You cannot study or cast Ice magic.", "", ""},
  {"You can no longer study or cast Ice magic.", "", ""},
  {"You can once more study and cast Ice magic.", "", ""},
},

{ MUT_NO_NECROMANCY_MAGIC, 0, 1, mutflag::BAD, false,
  "no necromancy magic",

  {"You cannot study or cast Necromancy magic.", "", ""},
  {"You can no longer study or cast Necromancy magic.", "", ""},
  {"You can once more study and cast Necromancy magic.", "", ""},
},

{ MUT_NO_POISON_MAGIC, 0, 1, mutflag::BAD, false,
  "no poison magic",

  {"You cannot study or cast Poison magic.", "", ""},
  {"You can no longer study or cast Poison magic.", "", ""},
  {"You can once more study and cast Poison magic.", "", ""},
},

{ MUT_NO_SUMMONING_MAGIC, 0, 1, mutflag::BAD, false,
  "no summoning magic",

  {"You cannot study or cast Summoning magic.", "", ""},
  {"You can no longer study or cast Summoning magic.", "", ""},
  {"You can once more study and cast Summoning magic.", "", ""},
},

{ MUT_NO_TRANSLOCATION_MAGIC, 0, 1, mutflag::BAD, false,
  "no translocations magic",

  {"You cannot study or cast Translocations magic.", "", ""},
  {"You can no longer study or cast Translocations magic.", "", ""},
  {"You can once more study and cast Translocations magic.", "", ""},
},

{ MUT_NO_TRANSMUTATION_MAGIC, 0, 1, mutflag::BAD, false,
  "no transmutations magic",

  {"You cannot study or cast Transmutations magic.", "", ""},
  {"You can no longer study or cast Transmutations magic.", "", ""},
  {"You can once more study and cast Transmutations magic.", "", ""},
},

{ MUT_PHYSICAL_VULNERABILITY, 0, 3, mutflag::BAD, false,
  "reduced AC",

  {"You take slightly more damage. (AC -3)",
    "You take more damage. (AC -6)",
    "You take considerably more damage. (AC -9)"},
  {"You feel more vulnerable to harm.",
    "You feel more vulnerable to harm.",
    "You feel more vulnerable to harm."},
  {"You no longer feel extra vulnerable to harm.",
    "You feel less vulnerable to harm.",
    "You feel less vulnerable to harm."},
},

{ MUT_SLOW_REFLEXES, 0, 3, mutflag::BAD, false,
  "reduced EV",

  {"You have somewhat slow reflexes. (EV -3)",
    "You have slow reflexes. (EV -6)",
    "You have very slow reflexes. (EV -9)"},
  {"Your reflexes slow.",
    "Your reflexes slow further.",
    "Your reflexes slow further."},
  {"You reflexes return to normal.",
    "You reflexes speed back up.",
    "You reflexes speed back up."},
},

{ MUT_MAGICAL_VULNERABILITY, 0, 3, mutflag::BAD, false,
  "magic vulnerability",

  {"You are slightly vulnerable to magic. (MR-)",
    "You are vulnerable to magic. (MR--)",
    "You are extremely vulnerable to magic. (MR---)"},
  {"You feel vulnerable to magic.",
    "You feel more vulnerable to magic.",
    "You feel more vulnerable to magic."},
  {"You no longer feel vulnerable to magic.",
    "You feel less vulnerable to magic.",
    "You feel less vulnerable to magic."},
},

{ MUT_ANTI_WIZARDRY, 0, 3, mutflag::BAD, false,
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

{ MUT_MP_WANDS, 7, 1, mutflag::BAD, false,
  "MP-powered wands",

  {"You expend magic power (3 MP) to strengthen your wands.", "", ""},
  {"You feel your magical essence link to your wands.", "", ""},
  {"Your magical essence is no longer linked to your wands.", "", ""},
},

{ MUT_UNSKILLED, 0, 3, mutflag::BAD, false,
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

{ MUT_INEXPERIENCED, 0, 3, mutflag::BAD, false,
    "inexperienced",

    {"You are somewhat inexperienced. (-2 XL)",
     "You are inexperienced. (-4 XL)",
     "You are extremely inexperienced. (-6 XL)"},
    {"You feel less experienced.",
     "You feel less experienced.",
     "You feel less experienced."},
    {"You regain all your potential.",
     "You regain some potential.",
     "You regain some potential."},
},

{ MUT_PAWS, 0, 1, mutflag::GOOD, true,
  "sharp paws",

  {"Your sharp claws are effective at attacking unaware monsters.", "", ""},
  {"", "", ""},
  {"", "", ""},
},

{ MUT_MISSING_EYE, 0, 1, mutflag::BAD, false,
  "missing an eye",

  {"You are missing an eye, making it more difficult to aim.", "", ""},
  {"Your right eye vanishes! The world loses its depth.", "", ""},
  {"Your right eye suddenly reappears! The world regains its depth.", "", ""},
},

{ MUT_TEMPERATURE_SENSITIVITY, 0, 1, mutflag::BAD, false,
  "temperature sensitive",

  {"You are sensitive to extremes of temperature. (rF-, rC-)", "", ""},
  {"You feel sensitive to extremes of temperature.", "", ""},
  {"You no longer feel sensitive to extremes of temperature", "", ""},
},

{ MUT_NO_REGENERATION, 0, 1, mutflag::BAD, false,
  "no regeneration",

  {"You do not regenerate.", "", ""},
  {"You stop regenerating.", "", ""},
  {"You start regenerating.", "", ""},
},

{ MUT_STRONG_NOSE, 0, 1, mutflag::GOOD, false,
  "strong nose",

  {"Your uncanny sense of smell can sniff out nearby items.", "", ""},
  {"Your sense of smell grows stronger.", "", ""},
  {"Your sense of smell gets weaker.", "", ""},
},
};
