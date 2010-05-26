// mutation definitions:
// first  number  = probability (0 means it doesn't appear naturally
//                  unless you have some level of it already)
// second number  = maximum levels
// first  boolean = is mutation mostly bad?
// second boolean = is mutation physical, i.e. external only?
// first  strings = what to show in 'A'
// second strings = message given when gaining the mutation
// third  strings = message given when losing the mutation
// fourth string  = wizard-mode name of mutation

#ifndef MUTATION_DATA_H
#define MUTATION_DATA_H

{ MUT_TOUGH_SKIN,                0,  3, false,  true,
  "tough skin",

  {"You have tough skin (AC +1).",
   "You have very tough skin (AC +2).",
   "You have extremely tough skin (AC +3)."},

  {"Your skin toughens.",
   "Your skin toughens.",
   "Your skin toughens."},

  {"Your skin feels delicate.",
   "Your skin feels delicate.",
   "Your skin feels delicate."},

  "tough skin"
},

{ MUT_STRONG,                     8, 14, false,  true,
  NULL,

  {"Your muscles are strong (Str +", "", ""},
  {"", "", ""},
  {"", "", ""},

  "strong"
},
{ MUT_CLEVER,                     8, 14, false,  true,
  NULL,

  {"Your mind is acute (Int +", "", ""},
  {"", "", ""},
  {"", "", ""},

  "clever"
},

{ MUT_AGILE,                      8, 14, false,  true,
  NULL,

  {"You are agile (Dex +", "", ""},
  {"", "", ""},
  {"", "", ""},

  "agile"
},

{ MUT_POISON_RESISTANCE,          4,  1, false, false,
  "poison resistance",

  {"Your system is resistant to poisons.", "", ""},
  {"You feel healthy.", "",  ""},
  {"You feel a little less healthy.", "", ""},

  "poison resistance"
},
{ MUT_CARNIVOROUS,                5,  3, false, false,
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

  "carnivorous"
},
{ MUT_HERBIVOROUS,                5,  3,  true, false,
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

  "herbivorous"
},
{ MUT_HEAT_RESISTANCE,            4,  3, false, false,
  "fire resistance",

  {"Your flesh is heat resistant.",
   "Your flesh is very heat resistant.",
   "Your flesh is almost immune to the effects of heat."},

  {"You feel a sudden chill.",
   "You feel a sudden chill.",
   "You feel a sudden chill."},

  {"You feel hot for a moment.",
   "You feel hot for a moment.",
   "You feel hot for a moment."},

  "heat resistance"
},
{ MUT_COLD_RESISTANCE,            4,  3, false, false,
  "cold resistance",

  {"Your flesh is cold resistant.",
   "Your flesh is very cold resistant.",
   "Your flesh is almost immune to the effects of cold."},

  {"You feel hot for a moment.",
   "You feel hot for a moment.",
   "You feel hot for a moment."},

  {"You feel a sudden chill.",
   "You feel a sudden chill.",
   "You feel a sudden chill."},

  "cold resistance"
},
{ MUT_DEMONIC_GUARDIAN,            0,  3, false, false,
  "demonic guardian",

  {"You are protected by a weak demonic guardian.",
   "You are protected by a demonic guardian.",
   "You are protected by a powerful demonic guardian."},

  {"You feel the presence of a demonic guardian.",
   "Your guardian grows in power.",
   "Your guardian grows in power."},

  {"Your demonic guardian is gone.",
   "Your demonic guardian is weakened.",
   "Your demonic guardian is weakened."},

  "demonic guardian"
},
{ MUT_SHOCK_RESISTANCE,           2,  1, false, false,
  "electricity resistance",

  {"You are resistant to electric shocks.", "", ""},
  {"You feel insulated.", "", ""},
  {"You feel conductive.", "", ""},

  "shock resistance"
},
{ MUT_REGENERATION,               3,  3, false, false,
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

  "regeneration"
},
{ MUT_SLOW_HEALING,               3,  3,  true, false,
  "slow healing",

  {"You heal slowly.",
   "You heal very slowly.",
   "You do not heal naturally."},

  {"You begin to heal more slowly.",
   "You begin to heal more slowly.",
   "You stop healing."},

  {"Your rate of healing increases.",
   "Your rate of healing increases.",
   "Your rate of healing increases."},

  "slow healing"
},
{ MUT_FAST_METABOLISM,           10,  3,  true, false,
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

  "fast metabolism"
},
{ MUT_SLOW_METABOLISM,            7,  3, false, false,
  "slow metabolism",

  {"You have a slow metabolism.",
   "You have a slow metabolism.",
   "You need consume almost no food."},

  {"Your metabolism slows.",
   "Your metabolism slows.",
   "Your metabolism slows."},

  {"You feel a little hungry.",
   "You feel a little hungry.",
   "You feel a little hungry."},

  "slow metabolism"
},

{ MUT_WEAK,                      10, 14,  true,  true,
  NULL,
  {"You are weak (Str -", "", ""},
  {"", "", ""},
  {"", "", ""},
  "weak"
},
{ MUT_DOPEY,                     10, 14,  true,  true,
  NULL,
  {"You are dopey (Int -", "", ""},
  {"", "", ""},
  {"", "", ""},
  "dopey",
},
{ MUT_CLUMSY,                    10, 14,  true,  true,
  NULL,
  {"You are clumsy (Dex -", "", ""},
  {"", "", ""},
  {"", "", ""},
  "clumsy"
},
{ MUT_TELEPORT_CONTROL,           2,  1, false, false,
  "teleport control",

  {"You can control translocations.", "", ""},
  {"You feel controlled.", "", ""},
  {"You feel random.", "", ""},

  "teleport control"
},

{ MUT_TELEPORT,                   3,  3,  true, false,
  "teleportitis",

  {"Space occasionally distorts in your vicinity.",
   "Space sometimes distorts in your vicinity.",
   "Space frequently distorts in your vicinity."},

  {"You feel weirdly uncertain.",
   "You feel even more weirdly uncertain.",
   "You feel even more weirdly uncertain."},

  {"You feel stable.",
   "You feel stable.",
   "You feel stable."},

  "teleport"
},
{ MUT_MAGIC_RESISTANCE,           5,  3, false, false,
  "magic resistance",

  {"You are resistant to magic.",
   "You are highly resistant to magic.",
   "You are extremely resistant to the effects of magic."},

  {"You feel resistant to magic.",
   "You feel more resistant to magic.",
   "You feel almost impervious to the effects of magic."},

  {"You feel less resistant to magic.",
   "You feel less resistant to magic.",
   "You feel vulnerable to magic again."},

  "magic resistance"
},

{ MUT_FAST,                       1,  3, false, false,
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

  "fast"
},

{ MUT_SLOW,                       3,  3, true, false,
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

  "slow"
},

{ MUT_ACUTE_VISION,               2,  1, false, false,
  "see invisible",

  {"You have supernaturally acute eyesight.", "", ""},

  {"Your vision sharpens.",
   "Your vision sharpens.",
   "Your vision sharpens."},

  {"Your vision seems duller.",
   "Your vision seems duller.",
   "Your vision seems duller."},

  "acute vision"
},

{ MUT_DEFORMED,                   8,  3,  true,  true,
  "deformed body",

  {"Armour fits poorly on your unusually shaped body.",
   "Armour fits poorly on your deformed body.",
   "Armour fits poorly on your badly deformed body."},

  {"Your body twists unusually.",
   "Your body twists and deforms.",
   "Your body twists and deforms."},

  {"Your body's shape seems more normal.",
   "Your body's shape seems slightly more normal.",
   "Your body's shape seems slightly more normal."},

  "deformed"
},
{ MUT_TELEPORT_AT_WILL,           0,  3, false, false,
  "teleport at will",

  {"You can teleport at will.",
   "You are good at teleporting at will.",
   "You can teleport instantly at will."},

  {"You feel jumpy.",
   "You feel more jumpy.",
   "You feel even more jumpy."},

  {"You feel a little less jumpy.",
   "You feel less jumpy.",
   "You feel less jumpy."},

  "teleport at will"
},
{ MUT_SPIT_POISON,                8,  3, false, false,
  "spit poison",

  {"You can spit poison.",
   "You can spit moderately strong poison.",
   "You can spit strong poison."},

  {"There is a nasty taste in your mouth for a moment.",
   "There is a nasty taste in your mouth for a moment.",
   "There is a nasty taste in your mouth for a moment."},

  {"You feel an ache in your throat.",
   "You feel an ache in your throat.",
   "You feel an ache in your throat."},

  "spit poison"
},

{ MUT_BREATHE_FLAMES,             4,  3, false, false,
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

  "breathe flames"
},

{ MUT_BLINK,                      3,  3, false, false,
  "blink",

  {"You can translocate small distances at will.",
   "You are good at translocating small distances at will.",
   "You can translocate small distances instantaneously."},

  {"You feel jittery.",
   "You feel more jittery.",
   "You feel even more jittery."},

  {"You feel a little less jittery.",
   "You feel less jittery.",
   "You feel less jittery."},

  "blink"
},
{ MUT_STRONG_STIFF,              10,  3, false,  true,
  NULL,

  {"Your muscles are strong, but stiff (Str +1, Dex -1).",
   "Your muscles are very strong, but stiff (Str +2, Dex -2).",
   "Your muscles are extremely strong, but stiff (Str +3, Dex -3)."},

  {"Your muscles feel sore.",
   "Your muscles feel sore.",
   "Your muscles feel sore."},

  {"Your muscles feel loose.",
   "Your muscles feel loose.",
   "Your muscles feel loose."},

  "strong stiff"
},

{ MUT_FLEXIBLE_WEAK,             10,  3, false,  true,
  NULL,

  {"Your muscles are flexible, but weak (Str -1, Dex +1).",
   "Your muscles are very flexible, but weak (Str -2, Dex +2).",
   "Your muscles are extremely flexible, but weak (Str -3, Dex +3)."},

  {"Your muscles feel loose.",
   "Your muscles feel loose.",
   "Your muscles feel loose."},

  {"Your muscles feel sore.",
   "Your muscles feel sore.",
   "Your muscles feel sore."},

  "flexible weak"
},

{ MUT_SCREAM,                     6,  3,  true, false,
  "screaming",

  {"You occasionally shout uncontrollably.",
   "You sometimes yell uncontrollably.",
   "You frequently scream uncontrollably."},

  {"You feel the urge to shout.",
   "You feel a strong urge to yell.",
   "You feel a strong urge to scream."},

  {"Your urge to shout disappears.",
   "Your urge to yell lessens.",
   "Your urge to scream lessens."},

  "scream"
},

{ MUT_CLARITY,                    6,  1, false, false,
  "clarity",

  {"You possess an exceptional clarity of mind.", "", ""},
  {"Your thoughts seem clearer.", "", ""},
  {"Your thinking seems confused.", "", ""},

  "clarity"
},

{ MUT_BERSERK,                    7,  3,  true, false,
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

  "berserk"
},

{ MUT_DETERIORATION,             10,  3,  true, false,
  "deterioration",

  {"Your body is slowly deteriorating.",
   "Your body is deteriorating.",
   "Your body is rapidly deteriorating."},

  {"You feel yourself wasting away.",
   "You feel yourself wasting away.",
   "You feel your body start to fall apart."},

  {"You feel healthier.",
   "You feel a little healthier.",
   "You feel a little healthier."},

  "deterioration"
},

{ MUT_BLURRY_VISION,             10,  3,  true, false,
  "blurry vision",

  {"Your vision is a little blurry.",
   "Your vision is quite blurry.",
   "Your vision is extremely blurry."},

  {"Your vision blurs.",
   "Your vision blurs.",
   "Your vision blurs."},

  {"Your vision sharpens.",
   "Your vision sharpens a little.",
   "Your vision sharpens a little."},

  "blurry vision"
},

{ MUT_MUTATION_RESISTANCE,        4,  3, false, false,
  "mutation resistance",

  {"You are somewhat resistant to further mutation.",
   "You are somewhat resistant to both further mutation and mutation removal.",
   "Your current mutations are irrevocably fixed, and you can mutate no more."},

  {"You feel genetically stable.",
   "You feel genetically stable.",
   "You feel genetically immutable."},

  {"You feel genetically unstable.",
   "You feel genetically unstable.",
   "You feel genetically unstable."},

  "mutation resistance"
},

{ MUT_FRAIL,                     10,  3,  true,  true,
  NULL,

  {"You are frail (-10% HP).",
   "You are very frail (-20% HP).",
   "You are extremely frail (-30% HP)."},

  {"You feel frail.",
   "You feel frail.",
   "You feel frail."},

  {"You feel robust.",
   "You feel robust.",
   "You feel robust."},

  "frail"
},

{ MUT_ROBUST,                     5,  3, false,  true,
  NULL,

  {"You are robust (+10% HP).",
   "You are very robust (+20% HP).",
   "You are extremely robust (+30% HP)."},

  {"You feel robust.",
   "You feel robust.",
   "You feel robust."},

  {"You feel frail.",
   "You feel frail.",
   "You feel frail."},

  "robust"
},

{ MUT_TORMENT_RESISTANCE,         0,  1, false, false,
  "torment resistance",

  {"You are immune to unholy pain and torment.", "", ""},
  {"You feel a strange anaesthesia.", "", ""},
  {"", "", ""},

  "torment resistance"
},

{ MUT_NEGATIVE_ENERGY_RESISTANCE, 0,  3, false, false,
  "life protection",

  {"You resist negative energy.",
   "You are quite resistant to negative energy.",
   "You are immune to negative energy."},

  {"You feel negative.",
   "You feel negative.",
   "You feel negative."},

  {"", "", ""},

  "negative energy resistance"
},

{ MUT_HURL_HELLFIRE,              0,  1, false, false,
  "hurl hellfire",

  {"You can hurl blasts of hellfire.", "", ""},
  {"You smell fire and brimstone.", "", ""},
  {"", "", ""},

  "hurl hellfire"
},

{ MUT_THROW_FLAMES,               0,  1, false, false,
  "throw flames of Gehenna",

  {"You can throw forth the flames of Gehenna.", "", ""},
  {"You smell the fires of Gehenna.", "", ""},
  {"", "", ""},

  "throw flames"
},

{ MUT_THROW_FROST,                0,  1, false, false,
  "throw frost of Cocytus",

  {"You can throw forth the frost of Cocytus.", "", ""},
  {"You feel the icy cold of Cocytus chill your soul.", "", ""},
  {"", "", ""},

  "throw frost"
},

// body-slot facets
{ MUT_HORNS,                      7,  3, false,  true,
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

  "horns"
},
{ MUT_BEAK,                       1,  1, false,  true,
  "beak",

  {"You have a beak for a mouth.", "", ""},
  {"Your mouth lengthens and hardens into a beak!", "", ""},
  {"Your beak shortens and softens into a mouth.", "", ""},

  "beak"
},

{ MUT_CLAWS,                      2,  3, false,  true,
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

  "claws"
},

{ MUT_FANGS,                      1,  3, false,  true,
  "fangs",

  {"You have very sharp teeth.",
   "You have extremely sharp teeth.",
   "You have razor-sharp teeth."},

  {"Your teeth lengthen and sharpen.",
   "Your teeth lengthen and sharpen some more.",
   "Your teeth are very long and razor-sharp."},

  {"Your teeth shrink to normal size.",
   "Your teeth shrink and become duller.",
   "Your teeth shrink and become duller."},

  "fangs"
},

{ MUT_HOOVES,                     5,  3, false,  true,
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

  "hooves"
},

{ MUT_ANTENNAE,                   4,  3, false,  true,
  "antennae",

  {"You have a pair of small antennae on your head.",
   "You have a pair of antennae on your head.",
   "You have a pair of large antennae on your head."},

  {"A pair of antennae grows on your head!",
   "The antennae on your head grow some more.",
   "The antennae on your head grow some more."},

  {"The antennae on your head shrink away.",
   "The antennae on your head shrink a bit.",
   "The antennae on your head shrink a bit."},

  "antennae"
},

{ MUT_TALONS,                     5,  3, false,  true,
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

  "talons"
},

// Naga only
{ MUT_BREATHE_POISON,             0,  1, false, false,
  "breathe poison",

  {"You can exhale a cloud of poison.", "", ""},
  {"You taste something nasty.", "", ""},
  {"Your breath is less nasty.", "", ""},

  "breathe poison"
},

// Naga and Draconian only
{ MUT_STINGER,                    0,  3, false,  true,
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

  "stinger"
},

// Draconian only
{ MUT_BIG_WINGS,                  0,  1, false,  true,
  "large and strong wings",

  {"Your wings are large and strong.", "", ""},
  {"Your wings grow larger and stronger.", "", ""},
  {"Your wings shrivel and weaken.", "", ""},

  "big wings"
},

// species-dependent innate mutations
{ MUT_SAPROVOROUS,                0,  3, false, false,
  "saprovore",

  {"You can tolerate rotten meat.",
   "You can eat rotten meat.",
   "You thrive on rotten meat."},
  {"", "", ""},
  {"", "", ""},

  "saprovorous"
},

{ MUT_GOURMAND,                   0,  1, false, false,
  "gourmand",

  {"You like to eat raw meat.", "", ""},
  {"", "", ""},
  {"", "", ""},

  "gourmand"
},

{ MUT_SHAGGY_FUR,                 2,  3, false,  true,
  NULL,

  {"You are covered in fur (AC +1).",
   "You are covered in thick fur (AC +2).",
   "Your thick and shaggy fur keeps you warm (AC +3, rC+)."},

  {"Fur sprouts all over your body.",
   "Your fur grows into a thick mane.",
   "Your thick fur grows shaggy and warm."},

  {"You shed all your fur.",
   "Your thick fur recedes somewhat.",
   "Your shaggy fur recedes somewhat."},

  "shaggy fur"
},

{ MUT_HIGH_MAGIC,                 2,  3, false, false,
  NULL,

  {"You have an increased reservoir of magic (+10% MP).",
   "You have a considerably increased reservoir of magic (+20% MP).",
   "You have a greatly increased reservoir of magic (+30% MP)."},

  {"You feel more energetic.",
   "You feel more energetic.",
   "You feel more energetic."},

  {"You feel less energetic.",
   "You feel less energetic.",
   "You feel less energetic."},

  "high mp"
},

{ MUT_LOW_MAGIC,                  9,  3,  true, false,
  NULL,

  {"Your magical capacity is low (-10% MP).",
   "Your magical capacity is very low (-20% MP).",
   "Your magical capacity is extremely low (-30% MP)."},

  {"You feel less energetic.",
   "You feel less energetic.",
   "You feel less energetic."},

  {"You feel more energetic.",
   "You feel more energetic.",
   "You feel more energetic."},

  "low mp"
},

{ MUT_STOCHASTIC_TORMENT_RESISTANCE, 0, 3, false, false,
  NULL,

  {"You are somewhat able to resist unholy torments (1 in 5 success).",
   "You are decently able to resist unholy torments (2 in 5 success).",
   "You are rather able to resist unholy torments (3 in 5 success)."},

  {"You feel a slight anaesthesia.",
   "You feel a slight anaesthesia.",
   "You feel a strange anaesthesia."},

  {"","",""},

  "stochastic torment resistance"
},

{ MUT_PASSIVE_MAPPING,            3,  3, false, false,
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

  "passive mapping"
},

{ MUT_ICEMAIL,                    0,  1, false, false,
  NULL,

  {"A meltable icy envelope protects you from harm (AC +", "", ""},
  {"An icy envelope takes form around you.", "", ""},
  {"", "", ""},

  "icemail"
},

{ MUT_CONSERVE_SCROLLS,           0,  1, false, false,
  "conserve scrolls",

  {"You are very good at protecting items from fire.", "", ""},
  {"You feel less concerned about heat.", "", ""},
  {"", "", ""},

  "conserve scrolls",
},

{ MUT_CONSERVE_POTIONS,           0,  1, false, false,
  "conserve potions",

  {"You are very good at protecting items from cold.", "", ""},
  {"You feel less concerned about cold.", "", ""},
  {"", "", ""},
  "conserve potions",
},

{ MUT_PASSIVE_FREEZE,             0,  1, false, false,
  "passive freeze",

  {"A frigid envelope surrounds you and freezes all who hurt you.", "", ""},
  {"Your skin feels very cold.", "", ""},
  {"", "", ""},

  "passive freeze",
},

{ MUT_NIGHTSTALKER,               0,  3, false, true,
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

  "nightstalker"
},

{ MUT_SPINY,                      0,  3, false, true,
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

  "spiny"
},

{ MUT_POWERED_BY_DEATH,           0,  3, false, false,
  "powered by death",

  {"You slowly steal the life force of defeated enemies.",
   "You steal the life force of defeated enemies.",
   "You quickly steal the life force of defeated enemies."},

  {"A wave of death washes over you.",
   "The wave of death grows in power.",
   "The wave of death grows in power."},

  {"Your control of surrounding life forces is gone.",
   "Your control of surrounding life forces weakens.",
   "Your control of surrounding life forces weakens."},

  "pbd"
},

// Jiyva only mutations
{ MUT_GELATINOUS_BODY,            0,  3, true, true,
  NULL,

  {"Your rubbery body absorbs attacks (AC +1).",
   "Your pliable body absorbs attacks (AC +1, EV +1).",
   "Your gelatinous body deflects attacks (AC +2, EV +2)."},

   {"Your body becomes stretchy.",
    "Your body becomes more malleable.",
    "Your body becomes viscous."},

   {"Your body returns to its normal consistency.",
    "Your body becomes less viscous.",
    "Your body becomes less viscous."},

    "gelatinous body"
},

{ MUT_EYEBALLS,                   0,  3, true, true,
  NULL,

  {"Your body is partially covered in golden eyeballs (Acc +3)",
   "Your body is mostly covered in golden eyeballs (Acc +5)",
   "Your body is completely covered in golden eyeballs (Acc +7, SInv)"},

   {"Eyeballs grow over part of your body.",
    "Eyeballs cover a large portion of your body.",
    "Eyeballs cover you completely."},

   {"The eyeballs on your body disappear.",
    "The eyeballs on your body recede somewhat.",
    "The eyeballs on your body recede somewhat."},

    "eyeballs"
},

{ MUT_TRANSLUCENT_SKIN,           0,   3, true, true,
  "translucent skin",

  {"Your skin is partially translucent.",
   "Your skin is mostly translucent (Stlth).",
   "Your transparent skin reduces the accuracy of your foes (Stlth)."},

  {"Your skin becomes partially translucent.",
   "Your skin becomes more translucent.",
   "Your skin becomes completely transparent."},

  {"Your skin returns to its normal opacity.",
   "Your skin's translucency fades.",
   "Your skin's transparency fades."},

   "translucent skin"
},

{ MUT_PSEUDOPODS,                 0,    3, true, true,
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

   "pseudopods"
},

{ MUT_FOOD_JELLY,                 0,    1, true, true,
  "spawn jellies when eating",

  {"You occasionally spawn a jelly by eating.", "", ""},

  {"You feel more connected to the slimes.", "", ""},

  {"Your connection to the slimes vanishes.", "", ""},

  "jelly spawner"
},

{ MUT_ACIDIC_BITE,                0,     1, true, true,
  "acidic bite",

  {"You have acidic saliva.", "", ""},

  {"Acid begins to drip from your mouth.", "", ""},

  {"Your mouth feels dry.", "", ""},

  "acidic bite"
},

// Scale mutations
{ MUT_DISTORTION_FIELD,                 2,  3, false, false,
  NULL,

  {"You are surrounded by a mild repulsion field (EV +2).",
   "You are surrounded by a moderate repulsion field (EV +3).",
   "You are surrounded by a strong repulsion field (EV +4, rMsl)."},

  {"You begin to radiate repulsive energy.",
   "Your repulsive radiation grows stronger.",
   "Your repulsive radiation grows stronger."},

  {"You feel less repulsive.",
   "You feel less repulsive.",
   "You feel less repulsive."},

  "repulsion field"
},

{ MUT_ICY_BLUE_SCALES,                  2,  3, false, true,
  NULL,

  {"You are partially covered in colorless scales (AC +1).",
   "You are mostly covered in icy blue scales (AC +2, EV -1).",
   "You are completely covered in icy blue scales (AC +3, EV -1, rC+)."},

  {"Colorless scales grow over part of your body.",
   "Your colorless scales turn blue and spread over more of your body.",
   "Icy blue scales cover your body completely."},

  {"Your colorless scales disappear.",
   "Your icy blue scales recede somewhat.",
   "Your icy blue scales recede somewhat."},

  "icy blue scales"
},

{ MUT_IRIDESCENT_SCALES,                2,  3, false,  true,
  NULL,

  {"You are partially covered in iridescent scales (AC +3).",
   "You are mostly covered in iridescent scales (AC +6).",
   "You are completely covered in iridescent scales (AC +9)."},

  {"Iridescent scales grow over part of your body.",
   "Iridescent scales spread over more of your body.",
   "Iridescent scales cover you completely."},

  {"Your iridescent scales disappear.",
   "Your iridescent scales recede somewhat.",
   "Your iridescent scales recede somewhat."},

  "iridescent scales"
},

{ MUT_LARGE_BONE_PLATES,                2,  3, false,  true,
  NULL,

  {"You are partially covered in large bone plates (AC +2, SH +2).",
   "You are mostly covered in large bone plates (AC +3, SH +3).",
   "You are completely covered in large bone plates (AC +4, SH +4)."},

  {"Large bone plates grow over parts of your arms.",
   "Large bone plates spread over more of your arms.",
   "Large bone plates cover your arms completely."},

  {"Your large bone plates disappear.",
   "Your large bone plates recede somewhat.",
   "Your large bone plates recede somewhat."},

  "large bone plates"
},

{ MUT_MOLTEN_SCALES,                    2,  3, false, true,
  NULL,

  {"You are partially covered in colorless scales (AC +1).",
   "You are mostly covered in molten scales (AC +2, EV -1).",
   "You are completely covered in molten scales (AC +3, EV -1, rF+)."},

  {"Colorless scales grow over part of your body.",
   "Your colorless scales turn molten and spread over more of your body.",
   "Molten scales cover your body completely."},

  {"Your colorless scales disappear.",
   "Your molten scales recede somewhat.",
   "Your molten scales recede somewhat."},

  "molten scales"
},

{ MUT_ROUGH_BLACK_SCALES,              2,  3, false,  true,
  NULL,

  {"You are partially covered in rough black scales (AC +4, Dex -1).",
   "You are mostly covered in rough black scales (AC +7, Dex -2).",
   "You are completely covered in rough black scales (AC +10, Dex -3)."},

  {"Rough black scales grow over part of your body.",
   "Rough black scales spread over more of your body.",
   "Rough black scales cover you completely."},

  {"Your rough black scales disappear.",
   "Your rough black scales recede somewhat.",
   "Your rough black scales recede somewhat."},

  "rough black scales"
},

{ MUT_RUGGED_BROWN_SCALES,              2,  3, false,  true,
  NULL,

  {"You are partially covered in rugged brown scales (AC +2, +3% HP).",
   "You are mostly covered in rugged brown scales (AC +2, +5% HP).",
   "You are completely covered in rugged brown scales (AC +2, +7% HP)."},

  {"Rugged brown scales grow over part of your body.",
   "Rugged brown scales spread over more of your body.",
   "Rugged brown scales cover you completely."},

  {"Your rugged brown scales disappear.",
   "Your rugged brown scales recede somewhat.",
   "Your rugged brown scales recede somewhat."},

  "rugged brown scales"
},

{ MUT_SLIMY_GREEN_SCALES,            2,  3, false, true,
  NULL,

  {"You are partially covered in colorless scales (AC +1).",
   "You are mostly covered in slimy green scales (AC +2, EV -1).",
   "You are completely covered in slimy green scales (AC +3, EV -2, rPois)."},

  {"Colorless scales grow over part of your body.",
   "Your colorless scales turn green and spread over more of your body.",
   "Slimy green scales cover your body completely."},

  {"Your colorless scales disappear.",
   "Your slimy green scales recede somewhat.",
   "Your slimy green scales recede somewhat."},

  "slimy green scales"
},

{ MUT_THIN_METALLIC_SCALES,            2,  3, false, true,
  NULL,

  {"You are partially covered in colorless scales (AC +1).",
   "You are mostly covered in thin metallic scales (AC +2).",
   "You are completely covered in thin metallic scales (AC +3, rElec)."},

  {"Colorless scales grow over part of your body.",
   "Your colorless scales are metallic and spread over more of your body.",
   "Thin metallic scales cover your body completely."},

  {"Your colorless scales disappear.",
   "Your thin metallic scales recede somewhat.",
   "Your thin metallic scales recede somewhat."},

  "thin metallic scales"
},

{ MUT_THIN_SKELETAL_STRUCTURE,          2,  3, false,  true,
  NULL,

  {"You have a somewhat thin skeletal structure (Dex +1, Str -1, Stlth).",
   "You have a moderately thin skeletal structure (Dex +2, Str -2, Stlth).",
   "You have an unnaturally thin skeletal structure (Dex +3, Str -3, Stlth)."},

  {"Your bones become slightly less dense.",
   "Your bones become somewhat less dense.",
   "Your bones become less dense."},

  {"Your skeletal structure returns to normal.",
   "Your skeletal structure densifies.",
   "Your skeletal structure densifies."},

  "thin skeletal structure"
},

{ MUT_YELLOW_SCALES,                    2,  3, false,  true,
  NULL,

  {"You are partially covered in colorless scales (AC +1).",
   "You are mostly covered in yellow scales (AC +2).",
   "You are completely covered in yellow scales (AC +3, rCorr)."},

  {"Colorless scales grow over part of your body",
   "Your colorless scales turn yellow and spread over more of your body.",
   "Yellow scales cover you completely"},

  {"Your colorless scales disappear.",
   "Your yellow scales recede somewhat.",
   "Your yellow scales recede somewhat."},

  "yellow scales"
}

#endif

