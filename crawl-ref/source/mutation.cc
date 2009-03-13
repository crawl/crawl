/*
 *  File:       mutation.cc
 *  Summary:    Functions for handling player mutations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");
#include "mutation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>

#ifdef DOS
#include <conio.h>
#endif

#if defined(UNIX) && !defined(USE_TILE)
#include "libunix.h"
#endif

#include "externs.h"

#include "cio.h"
#include "defines.h"
#include "effects.h"
#include "format.h"
#include "itemprop.h"
#include "macro.h"
#include "menu.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "stuff.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

static int _body_covered();

const char *troll_claw_descrip[4] = {
    "You have claws for hands.",
    "You have sharp claws for hands.",
    "You have very sharp claws for hands.",
    "You have claws sharper than steel for hands."
};

const char *troll_claw_gain[3] = {
    "Your claws sharpen.",
    "Your claws sharpen.",
    "Your claws steel!"
};

const char *troll_claw_lose[3] = {
    "Your claws look duller.",
    "Your claws look duller.",
    "Your claws feel softer."
};

const char *naga_speed_descrip[4] = {
    "You cover ground very slowly.",    // 10*14/10 = 14
    "You cover ground rather slowly.",  //  8*14/10 = 11
    "You cover ground rather quickly.", //  7*14/10 =  9
    "You cover ground quickly."         //  6*14/10 =  8
};

const char *centaur_deformed_descrip[3] = {
    "Armour fits poorly on your equine body.",
    "Armour fits poorly on your deformed equine body.",
    "Armour fits poorly on your badly deformed equine body."
};

const char *naga_deformed_descrip[3] = {
    "Armour fits poorly on your serpentine body.",
    "Armour fits poorly on your deformed serpentine body.",
    "Armour fits poorly on your badly deformed serpentine body."
};

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
mutation_def mutation_defs[] = {
    { MUT_TOUGH_SKIN,                10,  3, false,  true,
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
      {"Your muscles are strong (Str +", "", ""},
      {"", "", ""},
      {"", "", ""},
      "strong"
    },
    { MUT_CLEVER,                     8, 14, false,  true,
      {"Your mind is acute (Int +", "", ""},
      {"", "", ""},
      {"", "", ""},
      "clever"
    },
    { MUT_AGILE,                      8, 14, false,  true,
      {"You are agile (Dex +", "", ""},
      {"", "", ""},
      {"", "", ""},
      "agile"
    },
    { MUT_GREEN_SCALES,               2,  3, false,  true,
      {"You are partially covered in green scales (AC +1).",
       "You are mostly covered in green scales (AC +3).",
       "You are covered in green scales (AC +5)."},

      {"Green scales grow over part of your body.",
       "Green scales spread over more of your body.",
       "Green scales cover you completely."},

      {"Your green scales disappear.",
       "Your green scales recede somewhat.",
       "Your green scales recede somewhat."},

      "green scales"
    },
    { MUT_BLACK_SCALES,               1,  3, false,  true,
      {"You are partially covered in thick black scales (AC +3, Dex -1).",
       "You are mostly covered in thick black scales (AC +6, Dex -2).",
       "You are completely covered in thick black scales (AC +9, Dex -3)."},

      {"Thick black scales grow over part of your body.",
       "Thick black scales spread over more of your body.",
       "Thick black scales cover you completely."},

      {"Your black scales disappear.",
       "Your black scales recede somewhat.",
       "Your black scales recede somewhat."},

      "black scales"
    },
    { MUT_GREY_SCALES,                2,  3, false,  true,
      {"You are partially covered in supple grey scales (AC +1).",
       "You are mostly covered in supple grey scales (AC +2).",
       "You are completely covered in supple grey scales (AC +3)."},

      {"Supple grey scales grow over part of your body.",
       "Supple grey scales spread over more of your body.",
       "Supple grey scales cover you completely."},

      {"Your grey scales disappear.",
       "Your grey scales recede somewhat.",
       "Your grey scales recede somewhat."},

      "grey scales"
    },
    { MUT_BONEY_PLATES,               1,  3, false,  true,
      {"You are protected by plates of bone (AC +2, Dex -1).",
       "You are protected by plates of bone (AC +3, Dex -2).",
       "You are protected by plates of bone (AC +4, Dex -3)."},

      {"You grow protective plates of bone.",
       "You grow more protective plates of bone.",
       "You grow more protective plates of bone."},

      {"Your bony plates shrink away.",
       "Your bony plates shrink.",
       "Your bony plates shrink."},

      "boney plates"
    },
    { MUT_REPULSION_FIELD,            1,  3, false, false,
      {"You are surrounded by a mild repulsion field (EV +1).",
       "You are surrounded by a moderate repulsion field (EV +3).",
       "You are surrounded by a strong repulsion field "
       "(EV +5; repel missiles)."},

      {"You begin to radiate repulsive energy.",
       "Your repulsive radiation grows stronger.",
       "Your repulsive radiation grows stronger."},

      {"You feel attractive.",
       "You feel attractive.",
       "You feel attractive."},

      "repulsion field"
    },

    { MUT_POISON_RESISTANCE,          4,  1, false, false,
      {"Your system is immune to poisons.", "", ""},
      {"You feel healthy.", "",  ""},
      {"You feel a little less healthy.", "", ""},

      "poison resistance"
    },
    { MUT_CARNIVOROUS,                5,  3, false, false,
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
    { MUT_SHOCK_RESISTANCE,           2,  1, false, false,
      {"You are immune to electric shocks.", "", ""},
      {"You feel insulated.", "", ""},
      {"You feel conductive.", "", ""},

      "shock resistance"
    },
    { MUT_REGENERATION,               3,  3, false, false,
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
      {"You are weak (Str -", "", ""},
      {"", "", ""},
      {"", "", ""},
      "weak"
    },
    { MUT_DOPEY,                     10, 14,  true,  true,
      {"You are dopey (Int -", "", ""},
      {"", "", ""},
      {"", "", ""},
      "dopey",
    },
    { MUT_CLUMSY,                    10, 14,  true,  true,
      {"You are clumsy (Dex -", "", ""},
      {"", "", ""},
      {"", "", ""},
      "clumsy"
    },
    { MUT_TELEPORT_CONTROL,           2,  1, false, false,
      {"You can control translocations.", "", ""},
      {"You feel controlled.", "", ""},
      {"You feel random.", "", ""},

      "teleport control"
    },

    { MUT_TELEPORT,                   3,  3,  true, false,
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
    { MUT_ACUTE_VISION,               2,  1, false, false,
      {"You have supernaturally acute eyesight.",
       "You have supernaturally acute eyesight.",
       "You have supernaturally acute eyesight."},

      {"Your vision sharpens.",
       "Your vision sharpens.",
       "Your vision sharpens."},

      {"Your vision seems duller.",
       "Your vision seems duller.",
       "Your vision seems duller."},

      "acute vision"
    },
    { MUT_DEFORMED,                   8,  3,  true,  true,
      {"Armour fits poorly on your deformed body.",
       "Armour fits poorly on your badly deformed body.",
       "Armour fits poorly on your hideously deformed body."},

      {"Your body twists and deforms.",
       "Your body twists and deforms.",
       "Your body twists and deforms."},

      {"Your body's shape seems more normal.",
       "Your body's shape seems slightly more normal.",
       "Your body's shape seems slightly more normal."},

      "deformed"
    },
    { MUT_TELEPORT_AT_WILL,           2,  3, false, false,
      {"You can teleport at will.",
       "You are good at teleporting at will.",
       "You can teleport instantly at will."},

      {"You feel jumpy.",
       "You feel more jumpy.",
       "You feel even more jumpy."},

      {"You feel static.",
       "You feel less jumpy.",
       "You feel less jumpy."},

      "teleport at will"
    },
    { MUT_SPIT_POISON,                8,  3, false, false,
      {"You can spit poison.",
       "You can spit poison.",
       "You can spit poison."},

      {"There is a nasty taste in your mouth for a moment.",
       "There is a nasty taste in your mouth for a moment.",
       "There is a nasty taste in your mouth for a moment."},

      {"You feel an ache in your throat.",
       "You feel an ache in your throat.",
       "You feel an ache in your throat."},

      "spit poison"
    },
    { MUT_MAPPING,                    3,  3, false, false,
      {"You can sense your immediate surroundings.",
       "You can sense your surroundings.",
       "You can sense a large area of your surroundings."},

      {"You feel aware of your surroundings.",
       "You feel more aware of your surroundings.",
       "You feel even more aware of your surroundings."},

      {"You feel slightly disoriented.",
       "You feel slightly disoriented.",
       "You feel slightly disoriented."},

      "mapping"
    },
    { MUT_BREATHE_FLAMES,             4,  3, false, false,
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
      {"You can translocate small distances instantaneously.",
       "You can translocate small distances instantaneously.",
       "You can translocate small distances instantaneously."},

      {"You feel a little jumpy.",
       "You feel more jumpy.",
       "You feel even more jumpy."},

      {"You feel a little less jumpy.",
       "You feel less jumpy.",
       "You feel less jumpy."},

      "blink"
    },
    { MUT_HORNS,                      7,  3, false,  true,
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
      {"You have a beak for a mouth.", "", ""},
      {"Your mouth lengthens and hardens into a beak!", "", ""},
      {"Your beak shortens and softens into a mouth.", "", ""},

      "beak"
    },
    { MUT_STRONG_STIFF,              10,  3, false,  true,
      {"Your muscles are strong (Str +1), but stiff (Dex -1).",
       "Your muscles are very strong (Str +2), but stiff (Dex -2).",
       "Your muscles are extremely strong (Str +3), but stiff (Dex -3)."},

      {"Your muscles feel sore.",
       "Your muscles feel sore.",
       "Your muscles feel sore."},

      {"Your muscles feel loose.",
       "Your muscles feel loose.",
       "Your muscles feel loose."},

      "strong stiff"
    },
    { MUT_FLEXIBLE_WEAK,             10,  3, false,  true,
      {"Your muscles are flexible (Dex +1), but weak (Str -1).",
       "Your muscles are very flexible (Dex +2), but weak (Str -2).",
       "Your muscles are extremely flexible (Dex +3), but weak (Str -3)."},

      {"Your muscles feel loose.",
       "Your muscles feel loose.",
       "Your muscles feel loose."},

      {"Your muscles feel sore.",
       "Your muscles feel sore.",
       "Your muscles feel sore."},

      "flexible weak"
    },
    { MUT_SCREAM,                     6,  3,  true, false,
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
      {"You possess an exceptional clarity of mind.", "", ""},
      {"Your thoughts seem clearer.", "", ""},
      {"Your thinking seems confused.", "", ""},

      "clarity"
    },
    { MUT_BERSERK,                    7,  3,  true, false,
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
      {"You are somewhat resistant to further mutation.",
       "You are somewhat resistant to both further mutation "
       "and mutation removal.",
       "Your current mutations are irrevocably fixed, "
       "and you can mutate no more."},

      {"You feel genetically stable.",
       "You feel genetically stable.",
       "You feel genetically immutable."},

      {"You feel genetically unstable.",
       "You feel genetically unstable.",
       "You feel genetically unstable."},

      "mutation resistance"
    },
    { MUT_FRAIL,                     10,  3,  true,  true,
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

// Some demonic powers start here:
    { MUT_TORMENT_RESISTANCE,         0,  1, false, false,
      {"You are immune to unholy pain and torment.", "", ""},
      {"You feel a strange anaesthesia.", "", ""},
      {"", "", ""},

      "torment resistance"
    },
    { MUT_NEGATIVE_ENERGY_RESISTANCE, 0,  3, false, false,
      {"You resist negative energy.",
       "You are quite resistant to negative energy.",
       "You are immune to negative energy."},

      {"You feel negative.",
       "You feel negative.",
       "You feel negative."},

      {"", "", ""},

      "negative energy resistance"
    },
    { MUT_SUMMON_MINOR_DEMONS,        0,  1, false, false,
      {"You can summon minor demons to your aid.", "", ""},
      {"A thousand chattering voices call out to you.", "", ""},
      {"", "", ""},

      "summon minor demons"
    },
    { MUT_SUMMON_DEMONS,              0,  1, false, false,
      {"You can summon demons to your aid.", "", ""},
      {"Help is not far away!", "", ""},
      {"", "", ""},

      "summon demons"
    },
    { MUT_HURL_HELLFIRE,              0,  1, false, false,
      {"You can hurl blasts of hellfire.", "", ""},
      {"You smell fire and brimstone.", "", ""},
      {"", "", ""},

      "hurl hellfire"
    },
    { MUT_CALL_TORMENT,               0,  1, false, false,
      {"You can call on the torments of Hell.", "", ""},
      {"You feel a terrifying power at your call.", "", ""},
      {"", "", ""},

      "call torment"
    },
    { MUT_RAISE_DEAD,                 0,  1, false, false,
      {"You can raise the dead to walk for you.", "", ""},
      {"You feel an affinity for the dead.", "", ""},
      {"", "", ""},

      "raise dead"
    },
    { MUT_CONTROL_DEMONS,             0,  1, false, false,
      {"You can control demons.", "", ""},
      {"You feel an affinity for all demonkind.", "", ""},
      {"", "", ""},

      "control demons"
    },
    { MUT_DEATH_STRENGTH,             0,  1, false, false,
      {"You can draw strength from death and destruction.", "", ""},
      {"You feel hungry for death.", "", ""},
      {"", "", ""},

      "death strength"
    },
    { MUT_CHANNEL_HELL,               0,  1, false, false,
      {"You can channel magical energy from Hell.", "", ""},
      {"You feel a flux of magical energy.", "", ""},
      {"", "", ""},

      "channel hell"
    },
    { MUT_DRAIN_LIFE,                 0,  1, false, false,
      {"You can drain life in unarmed combat.", "", ""},
      {"Your skin tingles in a strangely unpleasant way.", "", ""},
      {"", "", ""},

      "drain life"
    },
    { MUT_THROW_FLAMES,               0,  1, false, false,
      {"You can throw forth the flames of Gehenna.", "", ""},
      {"You smell the fires of Gehenna.", "", ""},
      {"", "", ""},

      "throw flames"
    },
    { MUT_THROW_FROST,                0,  1, false, false,
      {"You can throw forth the frost of Cocytus.", "", ""},
      {"You feel the icy cold of Cocytus chill your soul.", "", ""},
      {"", "", ""},

      "throw frost"
    },
    { MUT_SMITE,                      0,  1, false, false,
      {"You can invoke the powers of Tartarus to smite your living foes.",
       "", ""},
      {"A shadow passes over the world around you.", "", ""},
      {"", "", ""},

      "smite"
    },
// end of demonic powers

    { MUT_CLAWS,                      2,  3, false,  true,
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
    { MUT_HOOVES,                     1,  1, false,  true,
      {"You have hooves in place of feet.", "", ""},
      {"Your feet shrivel into cloven hooves.", "", ""},
      {"Your hooves expand and flesh out into feet!", "", ""},

      "hooves"
    },
    { MUT_TALONS,                     1,  1, false,  true,
      {"You have talons in place of feet.", "", ""},
      {"Your feet stretch and sharpen into talons.", "", ""},
      {"Your talons dull and shrink into feet!", "", ""},

      "talons"
    },

    // Naga only
    { MUT_BREATHE_POISON,             0,  1, false, false,
      {"You can exhale a cloud of poison.", "", ""},
      {"You taste something nasty.", "", ""},
      {"Your breath is less nasty.", "", ""},

      "breathe poison"
    },
    // Naga and Draconian only
    { MUT_STINGER,                    0,  3, false,  true,
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
      {"Your wings are large and strong.", "", ""},
      {"Your wings grow larger and stronger.", "", ""},
      {"Your wings shrivel and weaken.", "", ""},

      "big wings"
    },

    // species-dependent innate mutations
    { MUT_SAPROVOROUS,                0,  3, false, false,
      {"You can tolerate rotten meat.",
       "You can eat rotten meat.",
       "You thrive on rotten meat."},
      {"", "", ""},
      {"", "", ""},

      "saprovorous"
    },
    { MUT_GOURMAND,                   0,  1, false, false,
      {"You like to eat raw meat.", "", ""},
      {"", "", ""},
      {"", "", ""},

      "gourmand"
    },

    { MUT_SHAGGY_FUR,                 2,  3, false,  true,
      {"You are covered in fur (AC +1).",
       "You are covered in thick fur (AC +2).",
       "Your thick and shaggy fur keeps you warm (AC +3, cold resistant)."},

      {"Fur sprouts all over your body.",
       "Your fur grows into a thick mane.",
       "Your thick fur grows shaggy and warm."},

      {"You shed all your fur.",
       "Your thick fur recedes somewhat.",
       "Your shaggy fur recedes somewhat."},

      "shaggy fur"
    },
    { MUT_HIGH_MAGIC,                 1,  3, false, false,
      {"You have an increased reservoir of magic (+10% MP).",
       "You have a considerably increased reservoir of magic (+20% MP).",
       "You have an greatly increased reservoir of magic (+30% MP)."},

      {"You feel more energetic.",
       "You feel more energetic.",
       "You feel more energetic."},

      {"You feel less energetic.",
       "You feel less energetic.",
       "You feel less energetic."},

      "high mp"
    },
    { MUT_LOW_MAGIC,                  9,  3,  true, false,
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

    { RANDOM_MUTATION,                0,  3, false, false,
      {"", "", ""},
      {"", "", ""},
      {"", "", ""},

      ""
    },

// Scales of various colours and effects
    { MUT_RED_SCALES,                 2,  3, false,  true,
      {"You are partially covered in red scales (AC +1).",
       "You are mostly covered in red scales (AC +2).",
       "You are covered in red scales (AC +4)."},

      {"Red scales grow over part of your body.",
       "Red scales spread over more of your body.",
       "Red scales cover you completely."},

      {"Your red scales disappear.",
       "Your red scales recede somewhat.",
       "Your red scales recede somewhat."},

      "red scales"
    },
    { MUT_NACREOUS_SCALES,            1,  3, false,  true,
      {"You are partially covered in smooth nacreous scales (AC +1).",
       "You are mostly covered in smooth nacreous scales (AC +3).",
       "You are completely covered in smooth nacreous scales (AC +5)."},

      {"Smooth nacreous scales grow over part of your body.",
       "Smooth nacreous scales spread over more of your body.",
       "Smooth nacreous scales cover you completely."},

      {"Your smooth nacreous scales disappear.",
       "Your smooth nacreous scales recede somewhat.",
       "Your smooth nacreous scales recede somewhat."},

      "nacreous scales"
    },
    { MUT_GREY2_SCALES,               2,  3, false,  true,
      {"You are partially covered in ridged grey scales (AC +2, Dex -1).",
       "You are mostly covered in ridged grey scales (AC +4, Dex -1).",
       "You are completely covered in ridged grey scales (AC +6, Dex -2)."},

      {"Ridged grey scales grow over part of your body.",
       "Ridged grey scales spread over more of your body.",
       "Ridged grey scales cover you completely."},

      {"Your ridged grey scales disappear.",
       "Your ridged grey scales recede somewhat.",
       "Your ridged grey scales recede somewhat."},

      "grey2 scales"
    },
    { MUT_METALLIC_SCALES,            1,  3, false,  true,
      {"You are partially covered in metallic scales (AC +3, Dex -2).",
       "You are mostly covered in metallic scales (AC +7, Dex -3).",
       "You are completely covered in metallic scales (AC +10, Dex -4)."},

      {"Metallic scales grow over part of your body.",
       "Metallic scales spread over more of your body.",
       "Metallic scales cover you completely."},

      {"Your metallic scales disappear.",
       "Your metallic scales recede somewhat.",
       "Your metallic scales recede somewhat."},

      "metallic scales"
    },
    { MUT_BLACK2_SCALES,              2,  3, false,  true,
      {"You are partially covered in black scales (AC +1).",
       "You are mostly covered in black scales (AC +3).",
       "You are completely covered in black scales (AC +5)."},

      {"Black scales grow over part of your body.",
       "Black scales spread over more of your body.",
       "Black scales cover you completely."},

      {"Your black scales disappear.",
       "Your black scales recede somewhat.",
       "Your black scales recede somewhat."},

      "black2 scales"
    },
    { MUT_WHITE_SCALES,               2,  3, false,  true,
      {"You are partially covered in white scales (AC +1).",
       "You are mostly covered in white scales (AC +3).",
       "You are completely covered in white scales (AC +5)."},

      {"White scales grow over part of your body.",
       "White scales spread over more of your body.",
       "White scales cover you completely."},

      {"Your white scales disappear.",
       "Your white scales recede somewhat.",
       "Your white scales recede somewhat."},

      "white scales"
    },
    { MUT_YELLOW_SCALES,              2,  3, false,  true,
      {"You are partially covered in yellow scales (AC +2).",
       "You are mostly covered in yellow scales (AC +4, Dex -1).",
       "You are completely covered in yellow scales (AC +6, Dex -2)."},

      {"Yellow scales grow over part of your body.",
       "Yellow scales spread over more of your body.",
       "Yellow scales cover you completely."},

      {"Your yellow scales disappear.",
       "Your yellow scales recede somewhat.",
       "Your yellow scales recede somewhat."},

      "yellow scales"
    },
    { MUT_BROWN_SCALES,               2,  3, false,  true,
      {"You are partially covered in brown scales (AC +2).",
       "You are mostly covered in brown scales (AC +4).",
       "You are completely covered in brown scales (AC +5)."},

      {"Brown scales grow over part of your body.",
       "Brown scales spread over more of your body.",
       "Brown scales cover you completely."},

      {"Your brown scales disappear.",
       "Your brown scales recede somewhat.",
       "Your brown scales recede somewhat."},

      "brown scales"
    },
    { MUT_BLUE_SCALES,                2,  3, false,  true,
      {"You are partially covered in blue scales (AC +1).",
       "You are mostly covered in blue scales (AC +2).",
       "You are completely covered in blue scales (AC +3)."},

      {"Blue scales grow over part of your body.",
       "Blue scales spread over more of your body.",
       "Blue scales cover you completely."},

      {"Your blue scales disappear.",
       "Your blue scales recede somewhat.",
       "Your blue scales recede somewhat."},

      "blue scales"
    },
    { MUT_PURPLE_SCALES,              2,  3, false,  true,
      {"You are partially covered in purple scales (AC +2).",
       "You are mostly covered in purple scales (AC +4).",
       "You are completely covered in purple scales (AC +6)."},

      {"Purple scales grow over part of your body.",
       "Purple scales spread over more of your body.",
       "Purple scales cover you completely."},

      {"Your purple scales disappear.",
       "Your purple scales recede somewhat.",
       "Your purple scales recede somewhat."},

      "purple scales"
    },
    { MUT_SPECKLED_SCALES,            2,  3, false,  true,
      {"You are partially covered in speckled scales (AC +1).",
       "You are mostly covered in speckled scales (AC +2).",
       "You are covered in speckled scales (AC +3)."},

      {"Speckled scales grow over part of your body.",
       "Speckled scales spread over more of your body.",
       "Speckled scales cover you completely."},

      {"Your speckled scales disappear.",
       "Your speckled scales recede somewhat.",
       "Your speckled scales recede somewhat."},

      "speckled scales"
    },
    { MUT_ORANGE_SCALES,              2,  3, false,  true,
      {"You are partially covered in orange scales (AC +1).",
       "You are mostly covered in orange scales (AC +3).",
       "You are completely covered in orange scales (AC +4)."},

      {"Orange scales grow over part of your body.",
       "Orange scales spread over more of your body.",
       "Orange scales cover you completely."},

      {"Your orange scales disappear.",
       "Your orange scales recede somewhat.",
       "Your orange scales recede somewhat."},

      "orange scales"
    },
    { MUT_INDIGO_SCALES,              2,  3, false,  true,
      {"You are partially covered in indigo scales (AC +2).",
       "You are mostly covered in indigo scales (AC +3).",
       "You are completely covered in indigo scales (AC +5)."},

      {"Indigo scales grow over part of your body.",
       "Indigo scales spread over more of your body.",
       "Indigo scales cover you completely."},

      {"Your indigo scales disappear.",
       "Your indigo scales recede somewhat.",
       "Your indigo scales recede somewhat."},

      "indigo scales"
    },
    { MUT_RED2_SCALES,                1,  3, false,  true,
      {"You are partially covered in knobbly red scales (AC +2).",
       "You are mostly covered in knobbly red scales (AC +5, Dex -1).",
       "You are completely covered in knobbly red scales (AC +7, Dex -2)."},

      {"Knobbly red scales grow over part of your body.",
       "Knobbly red scales spread over more of your body.",
       "Knobbly red scales cover you completely."},

      {"Your knobbly red scales disappear.",
       "Your knobbly red scales recede somewhat.",
       "Your knobbly red scales recede somewhat."},

      "red2 scales"
    },
    { MUT_IRIDESCENT_SCALES,          1,  3, false,  true,
      {"You are partially covered in iridescent scales (AC +1).",
       "You are mostly covered in iridescent scales (AC +2).",
       "You are completely covered in iridescent scales (AC +3)."},

      {"Iridescent scales grow over part of your body.",
       "Iridescent scales spread over more of your body.",
       "Iridescent scales cover you completely."},

      {"Your iridescent scales disappear.",
       "Your iridescent scales recede somewhat.",
       "Your iridescent scales recede somewhat."},

      "iridescent scales"
    },
    { MUT_PATTERNED_SCALES,           1,  3, false,  true,
      {"You are partially covered in patterned scales (AC +1).",
       "You are mostly covered in patterned scales (AC +2).",
       "You are completely covered in patterned scales (AC +3)."},

      {"Patterned scales grow over part of your body.",
       "Patterned scales spread over more of your body.",
       "Patterned scales cover you completely."},

      {"Your patterned scales disappear.",
       "Your patterned scales recede somewhat.",
       "Your patterned scales recede somewhat."},

      "patterned scales"
    }
};

const mutation_def& get_mutation_def(mutation_type mut)
{
    for (unsigned i = 0; i < ARRAYSZ(mutation_defs); ++i)
        if (mut == mutation_defs[i].mutation)
            return mutation_defs[i];

    ASSERT(0);
    return mutation_defs[0];
}

void fixup_mutations()
{
    if (player_genus(GENPC_DRACONIAN))
    {
        for (unsigned i = 0; i < ARRAYSZ(mutation_defs); ++i)
        {
            if (mutation_defs[i].mutation == MUT_STINGER
                || mutation_defs[i].mutation == MUT_BIG_WINGS)
            {
                mutation_defs[i].rarity = 1;
            }
        }
    }

    if (you.species == SP_TROLL)
    {
        for (unsigned i = 0; i < ARRAYSZ(mutation_defs); ++i)
        {
            if (mutation_defs[i].mutation == MUT_CLAWS)
            {
                for (int j = 0; j < 3; ++j)
                {
                    mutation_defs[i].gain[j] = troll_claw_gain[j];
                    mutation_defs[i].lose[j] = troll_claw_lose[j];
                }
            }
        }
    }

    if (you.species == SP_NAGA)
    {
        for (unsigned i = 0; i < ARRAYSZ(mutation_defs); ++i)
        {
            if (mutation_defs[i].mutation == MUT_DEFORMED)
            {
                for (int j = 0; j < 3; ++j)
                    mutation_defs[i].have[j] = naga_deformed_descrip[j];
            }
            else if (mutation_defs[i].mutation == MUT_STINGER)
                mutation_defs[i].rarity = 1;
        }
    }

    if (you.species == SP_CENTAUR)
        for (unsigned i = 0; i < ARRAYSZ(mutation_defs); ++i)
            if (mutation_defs[i].mutation == MUT_DEFORMED)
                for (int j = 0; j < 3; ++j)
                    mutation_defs[i].have[j] = centaur_deformed_descrip[j];
}

bool mutation_is_fully_active(mutation_type mut)
{
    // For all except the semi-undead, mutations always apply.
    if (you.is_undead != US_SEMI_UNDEAD)
        return (true);

    // Innate mutations are always active
    if (you.demon_pow[mut])
        return (true);

    // ... as are all mutations for semi-undead who are fully alive
    if (you.hunger_state == HS_ENGORGED)
        return (true);

    // ... as are physical mutations.
    if (get_mutation_def(mut).physical)
        return (true);

    return (false);
}

static bool _mutation_is_fully_inactive(mutation_type mut)
{
    const mutation_def& mdef = get_mutation_def(mut);
    return (you.is_undead == US_SEMI_UNDEAD && you.hunger_state < HS_SATIATED
            && !you.demon_pow[mut] && !mdef.physical);
}

formatted_string describe_mutations()
{
    std::string result;
    bool have_any = false;
    const char *mut_title = "Innate Abilities, Weirdness & Mutations";

    // center title
    int offset = 39 - strlen(mut_title) / 2;
    if ( offset < 0 ) offset = 0;

    result += std::string(offset, ' ');

    result += "<white>";
    result += mut_title;
    result += "</white>" EOL EOL;

    // Innate abilities which don't fit as mutations.
    result += "<lightblue>";
    switch (you.species)
    {
    case SP_MERFOLK:
        result += "You revert to your normal form in water." EOL;
        have_any = true;
        break;

    case SP_NAGA:
        result += "You cannot wear boots." EOL;
        if (!you.mutation[MUT_FAST])
        {
            result += mutation_name(MUT_FAST, -1, true);
            result += EOL;
        }
        // Breathe poison replaces spit poison.
        if (!you.mutation[MUT_BREATHE_POISON])
            result += "You can spit poison." EOL;
        have_any = true;
        break;

    case SP_GHOUL:
        result += "Your body is rotting away." EOL;
        have_any = true;
        break;

    case SP_TROLL:
        if (!you.mutation[MUT_CLAWS])
        {
            result += mutation_name(MUT_CLAWS, -1, true);
            result += EOL;
        }
        have_any = true;
        break;

    case SP_KENKU:
        if (you.experience_level > 4)
        {
            result += "You can fly";
            if (you.experience_level > 14)
                result += " continuously";
            result += "." EOL;
            have_any = true;
        }
        break;

    case SP_MUMMY:
        result += "Your flesh is vulnerable to fire." EOL;
        if (you.experience_level > 12)
        {
            result += "You are";
            if (you.experience_level > 25)
                result += " strongly";

            result += " in touch with the powers of death." EOL;
            result +=
                "You can restore your body by infusing magical energy." EOL;
        }
        have_any = true;
        break;

    case SP_GREY_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "Your tail is studded with spikes." EOL;
            have_any = true;
        }
        break;

    case SP_GREEN_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can breathe poison." EOL;
            have_any = true;
        }
        break;

    case SP_RED_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can breathe fire." EOL;
            have_any = true;
        }
        break;

    case SP_WHITE_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can breathe cold." EOL;
            have_any = true;
        }
        break;

    case SP_BLACK_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can breathe lightning." EOL;
            have_any = true;
        }
        break;

    case SP_YELLOW_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can spit acid." EOL;
            result += "You are resistant to acid." EOL;
            have_any = true;
        }
        break;

    case SP_PURPLE_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can breathe power." EOL;
            have_any = true;
        }
        break;

    case SP_MOTTLED_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can breathe sticky flames." EOL;
            have_any = true;
        }
        break;

    case SP_PALE_DRACONIAN:
        if (you.experience_level > 6)
        {
            result += "You can breathe steam." EOL;
            have_any = true;
        }
        break;

    case SP_KOBOLD:
        result += "You recuperate from illness quickly." EOL;
        have_any = true;
        break;

    case SP_VAMPIRE:
        have_any = true;
        if (you.hunger_state == HS_STARVING)
            result += "<green>You do not heal naturally.</green>" EOL;
        else if (you.hunger_state == HS_ENGORGED)
            result += "<green>Your natural rate of healing is extremely fast.</green>" EOL;
        else if (you.hunger_state < HS_SATIATED)
            result += "<green>You heal slowly.</green>" EOL;
        else if (you.hunger_state >= HS_FULL)
            result += "<green>Your natural rate of healing is unusually fast.</green>" EOL;
        else
            have_any = false;

        if (you.experience_level >= 6)
        {
            result += "You can bottle blood from corpses with 'c'." EOL;
            have_any = true;
        }
        break;

    case SP_DEEP_DWARF:
        result += "You are resistant to damage." EOL;
        result += "You can recharge devices by infusing magical energy." EOL;
        have_any = true;
        break;

    default:
        break;
    }

    // a bit more stuff
    if (you.species == SP_OGRE || you.species == SP_TROLL
        || player_genus(GENPC_DRACONIAN) || you.species == SP_SPRIGGAN)
    {
        result += "Your body does not fit into most forms of armour." EOL;
        have_any = true;
    }

    result += "</lightblue>";

    if (beogh_water_walk())
    {
        result += "<green>You can walk on water.</green>" EOL;
        have_any = true;
    }

    if (you.duration[DUR_FIRE_SHIELD])
    {
        result += "<green>You are immune to clouds of flame.</green>" EOL;
        have_any = true;
    }

    textcolor(LIGHTGREY);

    // First add (non-removable) inborn abilities and demon powers.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i] != 0 && you.demon_pow[i])
        {
            mutation_type mut_type = static_cast<mutation_type>(i);
            result += mutation_name(mut_type, -1, true);
            result += EOL;
            have_any = true;
        }
    }

    // Now add removable mutations.
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i] != 0 && !you.demon_pow[i])
        {
            mutation_type mut_type = static_cast<mutation_type>(i);
            result += mutation_name(mut_type, -1, true);
            result += EOL;
            have_any = true;
        }
    }

    if (!have_any)
        result +=  "You are rather mundane." EOL;

    if (you.species == SP_VAMPIRE)
    {
        result += EOL EOL;
        result += EOL EOL;
        result +=
#ifndef USE_TILE
            "Press '<w>!</w>'"
#else
            "<w>Right-click</w>"
#endif
            " to toggle between mutations and properties depending on your" EOL
            "hunger status." EOL;
    }

    return formatted_string::parse_string(result);
}

static void _display_vampire_attributes()
{
    clrscr();
    cgotoxy(1,1);

    std::string result;

    const int lines = 14;
    std::string column[lines][7] =
    {
       {"                     ", "<lightgreen>Alive</lightgreen>      ", "<green>Full</green>    ",
        "Satiated  ", "<yellow>Thirsty</yellow>  ", "<yellow>Near...</yellow>  ",
        "<lightred>Bloodless</lightred>"},
                                //Alive          Full       Satiated      Thirsty   Near...      Bloodless
       {"Metabolism           ", "very fast  ", "fast    ", "fast      ", "normal   ", "slow     ", "none  "},

       {"Regeneration         ", "very fast  ", "fast    ", "normal    ", "slow     ", "slow     ", "none  "},

       {"Stealth boost        ", "none       ", "none    ", "none      ", "minor    ", "major    ", "large "},

       {"Spell hunger         ", "full       ", "full    ", "full      ", "halved   ", "none     ", "none  "},

       {EOL "<w>Resistances</w>" EOL
        "Poison resistance    ", "           ", "        ", "          ", " +       ", " +       ", " +    "},

       {"Cold resistance      ", "           ", "        ", "          ", " +       ", " +       ", " ++   "},

       {"Negative resistance  ", "           ", "        ", " +        ", " ++      ", " +++     ", " +++  "},

       {"Torment resistance   ", "           ", "        ", "          ", "         ", "         ", " +    "},

       {EOL "<w>Other effects</w>" EOL
        "Mutation chance      ", "always     ", "often   ", "sometimes ", "never    ", "never    ", "never "},

       {"Non-physical " EOL
        "mutation effects     ", "full       ", "capped  ", "capped    ", "none     ", "none     ", "none  "},

       {"Potion effects       ", "full       ", "full    ", "full      ", "halved   ", "halved   ", "halved"},

       {"Bat Form             ", "no         ", "no      ", "yes       ", "yes      ", "yes      ", "yes   "},

       {"Other transformation " EOL
        "or going berserk     ", "yes        ", "yes     ", "no        ", "no       ", "no       ", "no    "}
    };

    int current = 0;
    switch (you.hunger_state)
    {
    case HS_ENGORGED:
        current = 1;
        break;
    case HS_VERY_FULL:
    case HS_FULL:
        current = 2;
        break;
    case HS_SATIATED:
        current = 3;
        break;
    case HS_HUNGRY:
    case HS_VERY_HUNGRY:
        current = 4;
        break;
    case HS_NEAR_STARVING:
        current = 5;
        break;
    case HS_STARVING:
        current = 6;
    }

    for (int y = 0; y < lines; y++)  // lines   (properties)
    {
        for (int x = 0; x < 7; x++)  // columns (hunger states)
        {
             if (y > 0 && x == current)
                 result += "<w>";
             result += column[y][x];
             if (y > 0 && x == current)
                 result += "</w>";
        }
        result += EOL;
    }

    result += EOL;
    result +=
#ifndef USE_TILE
        "Press '<w>!</w>'"
#else
        "<w>Right-click</w>"
#endif
        " to toggle between mutations and properties depending on your" EOL
        "hunger status." EOL;

    const formatted_string vp_props = formatted_string::parse_string(result);
    vp_props.display();

    if (you.species == SP_VAMPIRE)
    {
        mouse_control mc(MOUSE_MODE_MORE);
        const int keyin = getch();
        if (keyin == '!' || keyin == CK_MOUSE_CMD)
            display_mutations();
    }
}

void display_mutations()
{
    clrscr();
    cgotoxy(1,1);

    const formatted_string mutation_fs = describe_mutations();

    if (you.species == SP_VAMPIRE)
    {
        mutation_fs.display();
        mouse_control mc(MOUSE_MODE_MORE);
        const int keyin = getch();
        if (keyin == '!' || keyin == CK_MOUSE_CMD)
            _display_vampire_attributes();
    }
    else
    {
        Menu mutation_menu(mutation_fs);
        mutation_menu.show();
    }
}

static int _calc_mutation_amusement_value(mutation_type which_mutation)
{
    int amusement = 16 * (11 - get_mutation_def(which_mutation).rarity);

    switch (which_mutation)
    {
    case MUT_STRONG:
    case MUT_CLEVER:
    case MUT_AGILE:
    case MUT_POISON_RESISTANCE:
    case MUT_SHOCK_RESISTANCE:
    case MUT_REGENERATION:
    case MUT_SLOW_METABOLISM:
    case MUT_TELEPORT_CONTROL:
    case MUT_MAGIC_RESISTANCE:
    case MUT_TELEPORT_AT_WILL:
    case MUT_MAPPING:
    case MUT_CLARITY:
    case MUT_MUTATION_RESISTANCE:
    case MUT_ROBUST:
    case MUT_HIGH_MAGIC:
        amusement /= 2;  // not funny
        break;

    case MUT_CARNIVOROUS:
    case MUT_HERBIVOROUS:
    case MUT_SLOW_HEALING:
    case MUT_FAST_METABOLISM:
    case MUT_WEAK:
    case MUT_DOPEY:
    case MUT_CLUMSY:
    case MUT_TELEPORT:
    case MUT_FAST:
    case MUT_DEFORMED:
    case MUT_SPIT_POISON:
    case MUT_BREATHE_FLAMES:
    case MUT_BLINK:
    case MUT_HORNS:
    case MUT_BEAK:
    case MUT_SCREAM:
    case MUT_BERSERK:
    case MUT_DETERIORATION:
    case MUT_BLURRY_VISION:
    case MUT_FRAIL:
    case MUT_CLAWS:
    case MUT_FANGS:
    case MUT_HOOVES:
    case MUT_TALONS:
    case MUT_BREATHE_POISON:
    case MUT_STINGER:
    case MUT_BIG_WINGS:
    case MUT_LOW_MAGIC:
        amusement *= 2; // funny!
        break;

    default:
        break;
    }

    return (amusement);
}

static bool _is_deadly(mutation_type mutat, bool delete_mut)
{
    if (delete_mut)
    {
        // First handle non-stat related problems.
        if ( mutat == MUT_HEAT_RESISTANCE && grd(you.pos()) == DNGN_LAVA
             && player_res_fire() == 1 && !you.airborne() )
        {
            // Don't let player instantly fry to a crisp in lava.
            return (true);
        }

        // Swap things around to the same effect, but as if we were
        // gaining a mutation, or return early if deleting the mutation
        // is never a problem.
        switch (mutat)
        {
        case MUT_GREY2_SCALES:
        case MUT_METALLIC_SCALES:
        case MUT_YELLOW_SCALES:
        case MUT_RED2_SCALES:
        case MUT_WEAK:
        case MUT_DOPEY:
        case MUT_CLUMSY:
            return (false);

        case MUT_STRONG_STIFF:
            mutat = MUT_FLEXIBLE_WEAK;
            break;

        case MUT_FLEXIBLE_WEAK:
            mutat = MUT_STRONG_STIFF;
            break;

        case MUT_STRONG:
            mutat = MUT_WEAK;
            break;

        case MUT_CLEVER:
            mutat = MUT_DOPEY;
            break;

        case MUT_AGILE:
            mutat = MUT_CLUMSY;
            break;

        default:
            break;
        }
    }

    unsigned char cur_level = you.mutation[mutat];

    char *stat_ptr = &you.dex; // Default for the scales.
    char  amnt     = 1;
    char  mod      = 0;

    switch(mutat)
    {
    case MUT_GREY2_SCALES:
        if (cur_level == 0 || cur_level == 2)
            amnt = 1;
        else
            amnt = 0;
        break;

    case MUT_METALLIC_SCALES:
        if (cur_level == 0)
            amnt = 2;
        else
            amnt = 1;
        break;

    case MUT_YELLOW_SCALES:
    case MUT_RED2_SCALES:
        if (cur_level == 0)
            amnt = 0;
        else
            amnt = 1;
        break;

    case MUT_FLEXIBLE_WEAK:
    case MUT_WEAK:
        stat_ptr = &you.strength;
        // Take might into account so we don't lower base strength below
        // one.
        if (you.duration[DUR_MIGHT])
            mod = -5;
        break;

    case MUT_DOPEY:
        stat_ptr = &you.intel;
        break;

    case MUT_STRONG_STIFF:
    case MUT_CLUMSY:
        stat_ptr = &you.dex;
        break;

    default:
        return (false);
    }

    return (amnt >= (*stat_ptr + mod));
}

static bool _accept_mutation(mutation_type mutat, bool ignore_rarity = false,
                             bool non_fatal = false, bool delete_mut = false)
{
    if (mutat == RANDOM_MUTATION)
        return (false);

    const mutation_def& mdef = get_mutation_def(mutat);

    if (delete_mut)
        return (!non_fatal || !_is_deadly(mutat, delete_mut));

    if (you.mutation[mutat] >= mdef.levels)
        return (false);

    if (non_fatal && _is_deadly(mutat, delete_mut))
        return (false);

    if (ignore_rarity)
        return (true);

    const int rarity = mdef.rarity + you.demon_pow[mutat];

    // Low rarity means unlikely to choose it.
    return (x_chance_in_y(rarity, 10));
}

static mutation_type _get_random_xom_mutation(bool non_fatal = false)
{
    const mutation_type bad_muts[] = {
        MUT_SLOW_HEALING,  MUT_WEAK,          MUT_DOPEY,
        MUT_CLUMSY,        MUT_DEFORMED,      MUT_SCREAM,
        MUT_DETERIORATION, MUT_BLURRY_VISION, MUT_FRAIL
    };

    mutation_type mutat = NUM_MUTATIONS;

    do
    {
        mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

        if (one_chance_in(1000))
            return (NUM_MUTATIONS);
        else if (one_chance_in(5))
            mutat = RANDOM_ELEMENT(bad_muts);
    }
    while (!_accept_mutation(mutat, false, non_fatal));

    return (mutat);
}

static mutation_type _get_random_mutation(bool prefer_good,
                                          int preferred_multiplier,
                                          bool non_fatal = false)
{
    int cweight = 0;
    mutation_type chosen = NUM_MUTATIONS;
    for (unsigned i = 0; i < ARRAYSZ(mutation_defs); ++i)
    {
        if (!mutation_defs[i].rarity)
            continue;

        const mutation_type curr = mutation_defs[i].mutation;
        if (!_accept_mutation(curr, true, non_fatal))
            continue;

        const bool weighted = mutation_defs[i].bad != prefer_good;
        int weight = mutation_defs[i].rarity;
        if (weighted)
            weight = weight * preferred_multiplier / 100;

        cweight += weight;

        if (x_chance_in_y(weight, cweight))
            chosen = curr;
    }

    return (chosen);
}

static bool _is_random(mutation_type which_mutation)
{
    return (which_mutation == RANDOM_MUTATION
            || which_mutation == RANDOM_XOM_MUTATION
            || which_mutation == RANDOM_GOOD_MUTATION
            || which_mutation == RANDOM_BAD_MUTATION);
}

// Tries to give you the mutation by deleting a conflicting
// one, or clears out conflicting mutations if we should give
// you the mutation anyway.
// Return:
//  1 if we should stop processing (success);
//  0 if we should continue processing;
// -1 if we should stop processing (failure).
static int _handle_conflicting_mutations(mutation_type mutation,
                                         bool override)
{
    if (override)
    {
        // These are mutations which should be cleared away if forced.
        const mutation_type override_conflict[][2] = {
            { MUT_REGENERATION, MUT_SLOW_METABOLISM },
            { MUT_REGENERATION, MUT_SLOW_HEALING    },
            { MUT_ACUTE_VISION, MUT_BLURRY_VISION   }
        };

        // If we have one of the pair, delete all levels of the other,
        // and continue processing.
        for (unsigned i = 0; i < ARRAYSZ(override_conflict); ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                const mutation_type a = override_conflict[i][j];
                const mutation_type b = override_conflict[i][1-j];

                if (mutation == a)
                    while (delete_mutation(b, true, true))
                        ;
            }
        }
    }

    // These are mutations which can't be traded off against each other,
    // so we just fail.
    const mutation_type fail_conflict[][2] = {
        { MUT_REGENERATION, MUT_SLOW_METABOLISM },
        { MUT_FANGS,        MUT_BEAK            },
        { MUT_HOOVES,       MUT_TALONS          }
    };

    for (unsigned i = 0; i < ARRAYSZ(fail_conflict); ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            const mutation_type a = fail_conflict[i][j];
            const mutation_type b = fail_conflict[i][1-j];
            if (mutation == a && you.mutation[b] > 0)
                return (-1);    // Fail.
        }
    }

    // These are mutations which trade off against each other.
    const mutation_type simple_conflict[][2] = {
        { MUT_STRONG,          MUT_WEAK            },
        { MUT_CLEVER,          MUT_DOPEY           },
        { MUT_AGILE,           MUT_CLUMSY          },
        { MUT_STRONG_STIFF,    MUT_FLEXIBLE_WEAK   },
        { MUT_ROBUST,          MUT_FRAIL           },
        { MUT_HIGH_MAGIC,      MUT_LOW_MAGIC       },
        { MUT_CARNIVOROUS,     MUT_HERBIVOROUS     },
        { MUT_SLOW_METABOLISM, MUT_FAST_METABOLISM },
        { MUT_REGENERATION,    MUT_SLOW_HEALING    },
        { MUT_ACUTE_VISION,    MUT_BLURRY_VISION   }
    };

    for (unsigned i = 0; i < ARRAYSZ(simple_conflict); ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            // If we have one of the pair, delete a level of the other,
            // and that's it.
            const mutation_type a = simple_conflict[i][j];
            const mutation_type b = simple_conflict[i][1-j];
            if (mutation == a && you.mutation[b] > 0)
            {
                delete_mutation(b);
                return (1);     // Nothing more to do.
            }
        }
    }

    return (0);
}

static bool _physiology_mutation_conflict(mutation_type mutat)
{
    // Only Nagas and Draconians can get this one.
    if (mutat == MUT_STINGER
        && you.species != SP_NAGA && !player_genus(GENPC_DRACONIAN))
    {
        return (true);
    }

    if ((mutat == MUT_HOOVES || mutat == MUT_TALONS) && !player_has_feet())
        return (true);

    // Already innate.
    if (mutat == MUT_BREATHE_POISON && you.species != SP_NAGA)
        return (true);

    // Red Draconians can already breathe flames.
    if (mutat == MUT_BREATHE_FLAMES && you.species == SP_RED_DRACONIAN)
        return (true);

    // Green Draconians can already breathe poison, so they don't need
    // to spit it.
    if (mutat == MUT_SPIT_POISON && you.species == SP_GREEN_DRACONIAN)
        return (true);

    // Only Draconians can get wings.
    if (mutat == MUT_BIG_WINGS && !player_genus(GENPC_DRACONIAN))
        return (true);

    // Vampires' healing and thirst rates depend on their blood level.
    if (you.species == SP_VAMPIRE
        && (mutat == MUT_CARNIVOROUS || mutat == MUT_HERBIVOROUS
            || mutat == MUT_REGENERATION || mutat == MUT_SLOW_HEALING
            || mutat == MUT_FAST_METABOLISM || mutat == MUT_SLOW_METABOLISM))
    {
        return (true);
    }

    return (false);
}

bool mutate(mutation_type which_mutation, bool failMsg,
            bool force_mutation, bool god_gift, bool stat_gain_potion,
            bool demonspawn, bool non_fatal)
{
    ASSERT(!non_fatal || _is_random(which_mutation));

    if (demonspawn)
        force_mutation = true;

    mutation_type mutat = which_mutation;

    if (!force_mutation)
    {
        // God gifts override all sources of mutation resistance other
        // than the mutation resistance mutation and divine protection,
        // and stat gain potions override all sources of mutation
        // resistance other than the mutation resistance mutation.
        if (wearing_amulet(AMU_RESIST_MUTATION)
                && !one_chance_in(10) && !god_gift && !stat_gain_potion
            || player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
            || player_mutation_level(MUT_MUTATION_RESISTANCE)
               && !one_chance_in(3))
        {
            if (failMsg)
                mpr("You feel odd for a moment.", MSGCH_MUTATION);
            return (false);
        }

        // Zin's protection.
        if (you.religion == GOD_ZIN && x_chance_in_y(you.piety, MAX_PIETY)
            && !stat_gain_potion)
        {
            simple_god_message(" protects your body from chaos!");
            return (false);
        }
    }

    bool rotting = you.is_undead;

    if (you.is_undead == US_SEMI_UNDEAD)
    {
        // The stat gain mutations always come through at Satiated or
        // higher (mostly for convenience), and, for consistency, also
        // their negative counterparts.
        if (which_mutation == MUT_STRONG || which_mutation == MUT_CLEVER
            || which_mutation == MUT_AGILE || which_mutation == MUT_WEAK
            || which_mutation == MUT_DOPEY || which_mutation == MUT_CLUMSY)
        {
            if (you.hunger_state > HS_SATIATED)
                rotting = false;
        }
        else
        {
            // Else, chances depend on hunger state.
            switch (you.hunger_state)
            {
            case HS_SATIATED:  rotting = !one_chance_in(3); break;
            case HS_FULL:      rotting = coinflip();        break;
            case HS_VERY_FULL: rotting = one_chance_in(3);  break;
            case HS_ENGORGED:  rotting = false;             break;
            }
        }
    }

    // Undead bodies don't mutate, they fall apart. -- bwr
    // except for demonspawn (or other permamutations) in lichform -- haranp
    if (rotting && !demonspawn)
    {
        mpr("Your body decomposes!", MSGCH_MUTATION);

        if (coinflip())
            lose_stat(STAT_RANDOM, 1, false, "mutating");
        else
        {
            ouch(3, NON_MONSTER, KILLED_BY_ROTTING);
            rot_hp(roll_dice(1, 3));
        }

        xom_is_stimulated(64);
        return (true);
    }

    if (which_mutation == RANDOM_MUTATION
        || which_mutation == RANDOM_XOM_MUTATION)
    {
        // If already heavily mutated, remove a mutation instead.
        if (x_chance_in_y(how_mutated(false, true), 15))
        {
            // God gifts override mutation loss due to being heavily
            // mutated.
            if (!one_chance_in(3) && !god_gift && !force_mutation)
                return (false);
            else
                return (delete_mutation(RANDOM_MUTATION, failMsg,
                                        force_mutation, non_fatal));
        }
    }

    if (which_mutation == RANDOM_MUTATION)
    {
        do
        {
            mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));
            if (one_chance_in(1000))
                return (false);
        }
        while (!_accept_mutation(mutat, false, non_fatal));
    }
    else if (which_mutation == RANDOM_XOM_MUTATION)
    {
        if ((mutat = _get_random_xom_mutation(non_fatal)) == NUM_MUTATIONS)
            return (false);
    }
    else if (which_mutation == RANDOM_GOOD_MUTATION)
    {
        mutat = _get_random_mutation(true, 500, non_fatal);
        if (mutat == NUM_MUTATIONS)
            return (false);
    }
    else if (which_mutation == RANDOM_BAD_MUTATION)
    {
        mutat = _get_random_mutation(false, 500, non_fatal);
        if (mutat == NUM_MUTATIONS)
            return (false);
    }

    // Saprovorous/gourmand can't be randomly acquired.
    if ((mutat == MUT_SAPROVOROUS || mutat == MUT_GOURMAND) && !force_mutation)
        return (false);

    // Mutation resistance can't be acquired from god gifts.
    if (mutat == MUT_MUTATION_RESISTANCE && god_gift && !force_mutation)
        return (false);

    // These can be forced by demonspawn or god gifts.
    if ((mutat == MUT_SHAGGY_FUR
            || mutat >= MUT_GREEN_SCALES && mutat <= MUT_BONEY_PLATES
            || mutat >= MUT_RED_SCALES && mutat <= MUT_PATTERNED_SCALES)
        && _body_covered() >= 3 && !god_gift && !force_mutation)
    {
        return (false);
    }

    if (you.species == SP_NAGA)
    {
        // gdl: Spit poison 'upgrades' to breathe poison.  Why not...
        if (mutat == MUT_SPIT_POISON)
        {
            if (coinflip())
                return (false);

            mutat = MUT_BREATHE_POISON;

            // Breathe poison replaces spit poison (so it takes the slot).
            for (int i = 0; i < 52; ++i)
            {
                if (you.ability_letter_table[i] == ABIL_SPIT_POISON)
                    you.ability_letter_table[i] = ABIL_BREATHE_POISON;
            }
        }
    }

    if (_physiology_mutation_conflict(mutat))
        return (false);

    const mutation_def& mdef = get_mutation_def(mutat);

    if (you.mutation[mutat] >= mdef.levels)
        return (false);

    // God gifts and forced mutations clear away conflicting mutations.
    int rc =_handle_conflicting_mutations(mutat, god_gift || force_mutation);
    if (rc == 1)
        return (true);
    if (rc == -1)
        return (false);
    ASSERT(rc == 0);

    bool gain_msg = true;
    bool stat_msg = false;

    // Save original stats.
    const stat_type stats[] = {STAT_STRENGTH, STAT_DEXTERITY,
                               STAT_INTELLIGENCE};
    int modifiers[3];

    for (int i = 0; i < 3; ++i)
        modifiers[i] = stat_modifier(stats[i]);

    switch (mutat)
    {
    case MUT_STRONG: case MUT_AGILE:  case MUT_CLEVER:
    case MUT_WEAK:   case MUT_CLUMSY: case MUT_DOPEY:
        stat_msg = true;
        gain_msg = false;
        break;

        // FIXME: these cases should be handled better.
    case MUT_HOOVES:
    case MUT_TALONS:
        mpr(mdef.gain[you.mutation[mutat]], MSGCH_MUTATION);
        gain_msg = false;

        // Hooves and talons force boots off.
        if (you_tran_can_wear(EQ_BOOTS))
            remove_one_equip(EQ_BOOTS, false);
        break;

    case MUT_CLAWS:
        mpr(mdef.gain[you.mutation[mutat]], MSGCH_MUTATION);
        gain_msg = false;

        // Gloves aren't prevented until level 3.  We don't have the
        // mutation yet, so we have to check for level 2 or higher claws
        // here.
        if (you.mutation[mutat] >= 2 && you_tran_can_wear(EQ_GLOVES))
            remove_one_equip(EQ_GLOVES, false);
        break;

    case MUT_HORNS:
    case MUT_BEAK:
        mpr(mdef.gain[you.mutation[mutat]], MSGCH_MUTATION);
        gain_msg = false;

        // Horns and beaks force hard helmets off.
        if (you.equip[EQ_HELMET] != -1
            && is_hard_helmet(you.inv[you.equip[EQ_HELMET]])
            && you_tran_can_wear(EQ_HELMET))
        {
            remove_one_equip(EQ_HELMET, false);
        }
        break;

    default:
        break;
    }

    // For all those scale mutations.
    you.redraw_armour_class = true;

    you.mutation[mutat]++;

    for (int i = 0; i < 3; ++i)
    {
        const int new_modifier = stat_modifier(stats[i]);
        if (new_modifier != modifiers[i])
        {
            modify_stat(stats[i], new_modifier - modifiers[i],
                        !stat_msg, "losing a mutation");
        }
    }

    if (gain_msg)
        mpr(mdef.gain[you.mutation[mutat]-1], MSGCH_MUTATION);

    // Do post-mutation effects.
    if (mutat == MUT_FRAIL || mutat == MUT_ROBUST)
        calc_hp();
    if (mutat == MUT_LOW_MAGIC || mutat == MUT_HIGH_MAGIC)
        calc_mp();

    // Amusement value will be 16 * (11-rarity) * Xom's-sense-of-humor.
    xom_is_stimulated(_calc_mutation_amusement_value(mutat));

    take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
    return (true);
}

bool delete_mutation(mutation_type which_mutation, bool failMsg,
                     bool force_mutation, bool non_fatal)
{
    ASSERT(!non_fatal || _is_random(which_mutation));

    mutation_type mutat = which_mutation;

    if (!force_mutation)
    {
        if (player_mutation_level(MUT_MUTATION_RESISTANCE) > 1
            && (player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
                || coinflip()))
        {
            if (failMsg)
                mpr("You feel rather odd for a moment.", MSGCH_MUTATION);
            return (false);
        }
    }

    if (which_mutation == RANDOM_MUTATION
        || which_mutation == RANDOM_XOM_MUTATION
        || which_mutation == RANDOM_GOOD_MUTATION
        || which_mutation == RANDOM_BAD_MUTATION)
    {
        while (true)
        {
            if (one_chance_in(1000))
                return (false);

            mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

            if (you.mutation[mutat] == 0
                && mutat != MUT_STRONG
                && mutat != MUT_CLEVER
                && mutat != MUT_AGILE
                && mutat != MUT_WEAK
                && mutat != MUT_DOPEY
                && mutat != MUT_CLUMSY)
            {
                continue;
            }

            if (!_accept_mutation(mutat, true, non_fatal, true))
                continue;

            if (you.demon_pow[mutat] >= you.mutation[mutat])
                continue;

            const mutation_def& mdef = get_mutation_def(mutat);

            if (random2(10) >= mdef.rarity)
                continue;

            bool mismatch = (which_mutation == RANDOM_GOOD_MUTATION && mdef.bad)
                || (which_mutation == RANDOM_BAD_MUTATION && !mdef.bad);

            if (mismatch && !one_chance_in(10))
                continue;

            break;
        }
    }

    if (you.mutation[mutat] == 0)
        return (false);

    if (you.demon_pow[mutat] >= you.mutation[mutat])
        return (false);

    const mutation_def& mdef = get_mutation_def(mutat);

    bool lose_msg = true;
    bool stat_msg = false;

    // Save original stats.
    const stat_type stats[] = {STAT_STRENGTH, STAT_DEXTERITY,
                               STAT_INTELLIGENCE};
    int modifiers[3];

    for (int i = 0; i < 3; ++i)
        modifiers[i] = stat_modifier(stats[i]);

    switch (mutat)
    {
    case MUT_STRONG: case MUT_AGILE:  case MUT_CLEVER:
    case MUT_WEAK:   case MUT_CLUMSY: case MUT_DOPEY:
        stat_msg = true;
        lose_msg = false;
        break;

    case MUT_BREATHE_POISON:
        // can't be removed yet, but still covered:
        if (you.species == SP_NAGA)
        {
            // natural ability to spit poison retakes the slot
            for (int i = 0; i < 52; ++i)
            {
                if (you.ability_letter_table[i] == ABIL_BREATHE_POISON)
                    you.ability_letter_table[i] = ABIL_SPIT_POISON;
            }
        }
        break;

    default:
        break;
    }

    // For all those scale mutations.
    you.redraw_armour_class = true;

    you.mutation[mutat]--;

    for (int i = 0; i < 3; ++i)
    {
        const int new_modifier = stat_modifier(stats[i]);
        if (new_modifier != modifiers[i])
        {
            modify_stat(stats[i], new_modifier - modifiers[i],
                        !stat_msg, "losing a mutation");
        }
    }

    if (lose_msg)
        mpr(mdef.lose[you.mutation[mutat]], MSGCH_MUTATION);

    // Do post-mutation effects.
    if (mutat == MUT_FRAIL || mutat == MUT_ROBUST)
        calc_hp();
    if (mutat == MUT_LOW_MAGIC || mutat == MUT_HIGH_MAGIC)
        calc_mp();

    take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat]));
    return (true);
}

static int _body_covered()
{
    // Check how much of your body is covered by scales, etc.
    int covered = 0;

    if (you.species == SP_NAGA)
        covered++;

    if (player_genus(GENPC_DRACONIAN))
        covered += 3;

    const mutation_type scales[] = {
        MUT_SHAGGY_FUR,

        MUT_BONEY_PLATES,      MUT_GREEN_SCALES,    MUT_BLACK_SCALES,
        MUT_GREY_SCALES,       MUT_RED_SCALES,      MUT_NACREOUS_SCALES,
        MUT_GREY2_SCALES,      MUT_METALLIC_SCALES, MUT_BLACK2_SCALES,
        MUT_WHITE_SCALES,      MUT_YELLOW_SCALES,   MUT_BROWN_SCALES,
        MUT_BLUE_SCALES,       MUT_PURPLE_SCALES,   MUT_SPECKLED_SCALES,
        MUT_ORANGE_SCALES,     MUT_INDIGO_SCALES,   MUT_RED2_SCALES,
        MUT_IRIDESCENT_SCALES, MUT_PATTERNED_SCALES
    };

    for (unsigned i = 0; i < ARRAYSZ(scales); ++i)
        covered += you.mutation[scales[i]];

    return (covered);
}

// Return a string describing the mutation.
// If colour is true, also add the colour annotation.
std::string mutation_name(mutation_type mut, int level, bool colour)
{
    const bool fully_active = mutation_is_fully_active(mut);
    const bool fully_inactive =
        (!fully_active) && _mutation_is_fully_inactive(mut);

    // level == -1 means default action of current level
    if (level == -1)
    {
        if (!fully_inactive)
            level = player_mutation_level(mut);
        else // give description of fully active mutation
            level = you.mutation[mut];
    }

    std::string result;
    bool innate = false;

    if (mut == MUT_CLAWS &&
        (you.species == SP_TROLL || you.species == SP_GHOUL))
    {
        innate = true;
        if (you.species == SP_TROLL)
            result = troll_claw_descrip[level];
    }

    if ((mut == MUT_FAST || mut == MUT_BREATHE_POISON)
        && you.species == SP_NAGA)
    {
        innate = true;
        if (mut == MUT_FAST)
            result = naga_speed_descrip[level];
    }

    const mutation_def& mdef = get_mutation_def(mut);

    if (mut == MUT_STRONG || mut == MUT_CLEVER
        || mut == MUT_AGILE || mut == MUT_WEAK
        || mut == MUT_DOPEY || mut == MUT_CLUMSY)
    {
        std::ostringstream ostr;
        ostr << mdef.have[0] << level << ").";
        result = ostr.str();
    }
    else if (result.empty() && level > 0)
        result = mdef.have[level - 1];

    if (fully_inactive)
    {
        result = "(" + result;
        result += ")";
    }

    if (colour)
    {
        const char* colourname = "lightgrey"; // the default
        const bool permanent = (you.demon_pow[mut] > 0);
        if (innate)
            colourname = (level > 0 ? "cyan" : "lightblue");
        else if (permanent)
        {
            const bool demonspawn = (you.species == SP_DEMONSPAWN);
            const bool extra = (you.mutation[mut] > you.demon_pow[mut]);

            if (fully_inactive)
                colourname = "darkgrey";
            else if (!fully_active)
                colourname = demonspawn ? "yellow"   : "blue";
            else if (extra)
                colourname = demonspawn ? "lightred" : "cyan";
            else
                colourname = demonspawn ? "red"      : "lightblue";
        }

        // Build the result
        std::ostringstream ostr;
        ostr << '<' << colourname << '>' << result
             << "</" << colourname << ">";
        result = ostr.str();
    }

    return (result);
}

// Use an attribute counter for how many demonic mutations a demonspawn
// has.
void demonspawn()
{
    mutation_type whichm = NUM_MUTATIONS;
    int howm = 1;
    int counter = 0;

    const int covered = _body_covered();

    you.attribute[ATTR_NUM_DEMONIC_POWERS]++;

    mpr("Your demonic ancestry asserts itself...", MSGCH_INTRINSIC_GAIN);

    // Merged the demonspawn lists into a single loop.  Now a high-level
    // character can potentially get mutations from the low-level list
    // if it's having trouble with the high level list.
    do
    {
        if (you.experience_level >= 10)
        {
            if (you.skills[SK_CONJURATIONS] < 5)
            {                       // good conjurers don't get bolt of draining
                whichm = MUT_SMITE;
                howm = 1;
            }

            if (you.skills[SK_CONJURATIONS] < 10 && one_chance_in(4))
            {                       // good conjurers don't get hellfire
                whichm = MUT_HURL_HELLFIRE;
                howm = 1;
            }

            // Makhlebites have the summonings invocation
            if ((you.religion != GOD_MAKHLEB ||
                you.piety < piety_breakpoint(3)) &&
                you.skills[SK_SUMMONINGS] < 5 && one_chance_in(3))
            {                       // good summoners don't get summon demon
                whichm = MUT_SUMMON_DEMONS;
                howm = 1;
            }

            if (one_chance_in(8))
            {
                whichm = MUT_MAGIC_RESISTANCE;
                howm = (coinflip() ? 2 : 3);
            }

            if (one_chance_in(12))
            {
                whichm = MUT_FAST;
                howm = 1;
            }

            if (one_chance_in(7))
            {
                whichm = MUT_TELEPORT_AT_WILL;
                howm = 2;
            }

            if (one_chance_in(10))
            {
                whichm = MUT_REGENERATION;
                howm = (coinflip() ? 2 : 3);
            }

            if (one_chance_in(12))
            {
                whichm = MUT_SHOCK_RESISTANCE;
                howm = 1;
            }

            if (!you.mutation[MUT_CALL_TORMENT] && one_chance_in(15))
            {
                whichm = MUT_TORMENT_RESISTANCE;
                howm = 1;
            }

            if (one_chance_in(12))
            {
                whichm = MUT_NEGATIVE_ENERGY_RESISTANCE;
                howm = 1 + random2(3);
            }

            if (!you.mutation[MUT_TORMENT_RESISTANCE] && one_chance_in(20))
            {
                whichm = MUT_CALL_TORMENT;
                howm = 1;
            }

            if (you.skills[SK_SUMMONINGS] < 5 && you.skills[SK_NECROMANCY] < 5
                && one_chance_in(12))
            {
                whichm = MUT_CONTROL_DEMONS;
                howm = 1;
            }

            if (you.religion != GOD_VEHUMET && you.religion != GOD_MAKHLEB
                && one_chance_in(11))
            {
                whichm = MUT_DEATH_STRENGTH;
                howm = 1;
            }

            if (you.religion != GOD_SIF_MUNA && one_chance_in(11))
            {
                whichm = MUT_CHANNEL_HELL;
                howm = 1;
            }

            // Yredelemnulites have the raise dead invocation
            if (you.religion != GOD_YREDELEMNUL
                && you.skills[SK_SUMMONINGS] < 3
                && you.skills[SK_NECROMANCY] < 3 && one_chance_in(10))
            {
                whichm = MUT_RAISE_DEAD;
                howm = 1;
            }

            if (you.skills[SK_UNARMED_COMBAT] > 5)
            {
                // Drain Life only works if you're unarmed, so only
                // give it if unarmed is your best attacking skill.
                skill_type wpn_skill = best_skill(SK_SHORT_BLADES, SK_STAVES);
                if ((you.skills[SK_UNARMED_COMBAT] > you.skills[wpn_skill])
                    && one_chance_in(14))
                {
                    whichm = MUT_DRAIN_LIFE;
                    howm = 1;
                }
            }
        }

        // check here so we can see if we need to extend our options
        if (whichm != NUM_MUTATIONS && you.mutation[whichm] != 0)
            whichm = NUM_MUTATIONS;

        if (you.experience_level < 10
            || (counter > 0 && whichm == NUM_MUTATIONS))
        {
            if ((!you.mutation[MUT_THROW_FROST]         // only one of these
                    && !you.mutation[MUT_THROW_FLAMES]
                    && !you.mutation[MUT_BREATHE_FLAMES])
                && (!you.skills[SK_CONJURATIONS]        // conjurers seldomly
                    || one_chance_in(5))
                // Makhlebites seldom
                && (you.religion != GOD_MAKHLEB || one_chance_in(4))
                && (!you.skills[SK_ICE_MAGIC]           // already ice & fire?
                    || !you.skills[SK_FIRE_MAGIC]))
            {
                // try to give the flavour the character doesn't have

                // neither
                if (!you.skills[SK_FIRE_MAGIC] && !you.skills[SK_ICE_MAGIC])
                    whichm = (coinflip() ? MUT_THROW_FLAMES : MUT_THROW_FROST);
                else if (!you.skills[SK_FIRE_MAGIC])
                    whichm = MUT_THROW_FLAMES;
                else if (!you.skills[SK_ICE_MAGIC])
                    whichm = MUT_THROW_FROST;
                // both
                else
                    whichm = (coinflip() ? MUT_THROW_FLAMES : MUT_THROW_FROST);

                howm = 1;
            }

            // summoners and Makhlebites don't get summon imp
            if (!you.skills[SK_SUMMONINGS] && you.religion != GOD_MAKHLEB
                && one_chance_in(3))
            {
                whichm = (you.experience_level < 10) ? MUT_SUMMON_MINOR_DEMONS
                                                     : MUT_SUMMON_DEMONS;
                howm = 1;
            }

            if (one_chance_in(4))
            {
                whichm = MUT_POISON_RESISTANCE;
                howm = 1;
            }

            if (one_chance_in(4))
            {
                whichm = MUT_COLD_RESISTANCE;
                howm = 1;
            }

            if (one_chance_in(4))
            {
                whichm = MUT_HEAT_RESISTANCE;
                howm = 1;
            }

            if (one_chance_in(5))
            {
                whichm = MUT_ACUTE_VISION;
                howm = 1;
            }

            if (!you.skills[SK_POISON_MAGIC] && one_chance_in(7))
            {
                whichm = MUT_SPIT_POISON;
                howm = (you.experience_level < 10) ? 1 : 3;
            }

            if (one_chance_in(10))
            {
                whichm = MUT_MAPPING;
                howm = 3;
            }

            if (one_chance_in(12))
            {
                whichm = MUT_TELEPORT_CONTROL;
                howm = 1;
            }

            if (!you.mutation[MUT_THROW_FROST]         // not with these
                && !you.mutation[MUT_THROW_FLAMES]
                && !you.mutation[MUT_BREATHE_FLAMES]
                && !you.skills[SK_FIRE_MAGIC]          // or with fire already
                && one_chance_in(5))
            {
                whichm = MUT_BREATHE_FLAMES;
                howm = 2;
            }

            if (!you.skills[SK_TRANSLOCATIONS] && one_chance_in(12))
            {
                whichm = (you.experience_level < 10) ? MUT_BLINK
                                                     : MUT_TELEPORT_AT_WILL;
                howm = 2;
            }

            if (covered < 3 && one_chance_in( 1 + covered * 5 ))
            {
                const int bonus = (you.experience_level < 10) ? 0 : 1;
                int levels = 0;

                if (one_chance_in(10))
                {
                    whichm = MUT_TOUGH_SKIN;
                    levels = (coinflip() ? 2 : 3);
                }

                if (one_chance_in(24))
                {
                    whichm = MUT_GREEN_SCALES;
                    levels = (coinflip() ? 2 : 3);
                }

                if (one_chance_in(24))
                {
                    whichm = MUT_BLACK_SCALES;
                    levels = (coinflip() ? 2 : 3);
                }

                if (one_chance_in(24))
                {
                    whichm = MUT_GREY_SCALES;
                    levels = (coinflip() ? 2 : 3);
                }

                if (one_chance_in(12))
                {
                    whichm = static_cast<mutation_type>(MUT_RED_SCALES +
                                                        random2(16));

                    switch (whichm)
                    {
                    case MUT_RED_SCALES:
                    case MUT_NACREOUS_SCALES:
                    case MUT_BLACK2_SCALES:
                    case MUT_WHITE_SCALES:
                    case MUT_BLUE_SCALES:
                    case MUT_SPECKLED_SCALES:
                    case MUT_ORANGE_SCALES:
                    case MUT_IRIDESCENT_SCALES:
                    case MUT_PATTERNED_SCALES:
                        levels = (coinflip() ? 2 : 3);
                        break;

                    default:
                        levels = (coinflip() ? 1 : 2);
                        break;
                    }
                }

                if (one_chance_in(30))
                {
                    whichm = MUT_BONEY_PLATES;
                    levels = (coinflip() ? 1 : 2);
                }

                if (levels)
                    howm = std::min(3 - covered, levels + bonus);
            }

            if (one_chance_in(25))
            {
                whichm = MUT_REPULSION_FIELD;
                howm = (coinflip() ? 2 : 3);
            }

            if (one_chance_in( (you.experience_level < 10) ? 5 : 20 ))
            {
                whichm = MUT_HORNS;
                howm = (coinflip() ? 1 : 2);

                if (you.experience_level > 4 || one_chance_in(5))
                    howm++;
            }
        }

        if (whichm != NUM_MUTATIONS && you.mutation[whichm] != 0)
            whichm = NUM_MUTATIONS;

        counter++;
    }
    while (whichm == NUM_MUTATIONS && counter < 5000);

    if (whichm == NUM_MUTATIONS || !perma_mutate( whichm, howm ))
    {
        // Unlikely but remotely possible; I know this is a cop-out.
        modify_stat(STAT_STRENGTH, 1, true, "demonspawn mutation");
        modify_stat(STAT_INTELLIGENCE, 1, true, "demonspawn mutation");
        modify_stat(STAT_DEXTERITY, 1, true, "demonspawn mutation");
        mpr("You feel much better now.", MSGCH_INTRINSIC_GAIN);
    }
}

bool perma_mutate(mutation_type which_mut, int how_much)
{
    int levels = 0;

    how_much = std::min(static_cast<short>(how_much),
                        mutation_defs[which_mut].levels);

    if (mutate(which_mut, false, true, false, false, true))
        levels++;

    if (how_much >= 2 && mutate(which_mut, false, true, false, false, true))
        levels++;

    if (how_much >= 3 && mutate(which_mut, false, true, false, false, true))
        levels++;

    you.demon_pow[which_mut] = levels;

    return (levels > 0);
}

int how_mutated(bool all, bool levels)
{
    int j = 0;

    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (you.mutation[i])
        {
            if (!all && you.demon_pow[i] >= you.mutation[i])
                continue;

            if (levels)
            {
                // These allow for 14 levels.
                if (i == MUT_STRONG || i == MUT_CLEVER || i == MUT_AGILE
                    || i == MUT_WEAK || i == MUT_DOPEY || i == MUT_CLUMSY)
                {
                    j += (you.mutation[i] / 5 + 1);
                }
                else
                    j += you.mutation[i];
            }
            else
                j++;
        }
    }

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "how_mutated(): all = %u, levels = %u, j = %d",
         all, levels, j);
#endif

    return (j);
}

bool give_bad_mutation(bool failMsg, bool force_mutation, bool non_fatal)
{
    const mutation_type bad_muts[] = {
        MUT_CARNIVOROUS,   MUT_HERBIVOROUS,   MUT_FAST_METABOLISM,
        MUT_SLOW_HEALING,  MUT_WEAK,          MUT_DOPEY,
        MUT_CLUMSY,        MUT_TELEPORT,      MUT_DEFORMED,
        MUT_SCREAM,        MUT_DETERIORATION, MUT_BLURRY_VISION,
        MUT_FRAIL,         MUT_LOW_MAGIC
    };

    mutation_type mutat;

    do
        mutat = RANDOM_ELEMENT(bad_muts);
    while (non_fatal && !_accept_mutation(mutat, true, true));

    const bool result = mutate(mutat, failMsg, force_mutation);
    if (result)
        learned_something_new(TUT_YOU_MUTATED);

    return (result);
}
