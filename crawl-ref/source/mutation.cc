/*
 *  File:       mutation.cc
 *  Summary:    Functions for handling player mutations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <5>      7/29/00        JDJ             Made give_cosmetic_mutation static
 *      <4>      9/25/99        CDL             linuxlib -> liblinux
 *      <3>      9/21/99        LRH             Added many new scales
 *      <2>      5/20/99        BWR             Fixed it so demonspwan should
 *                                              always get a mutation, 3 level
 *                                              perma_mutations now work.
 *      <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "mutation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DOS
#include <conio.h>
#endif

#if defined(UNIX) && !defined(USE_TILE)
#include "libunix.h"
#endif

#include "externs.h"

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


static int body_covered(void);

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
    "You cover ground rather quickly.", //  7*14/10 = 9
    "You cover ground quickly."         //  6*14/10 = 8
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

const char *mutation_descrip[][3] = {
    {"You have tough skin (AC +1).", "You have very tough skin (AC +2).",
     "You have extremely tough skin (AC +3)."},

    {"Your muscles are strong (Str +", "", ""},
    {"Your mind is acute (Int +", "", ""},
    {"You are agile (Dex +", "", ""},

    {"You are partially covered in green scales (AC + 1).",
     "You are mostly covered in green scales (AC + 3).",
     "You are covered in green scales (AC + 5)."},

    {"You are partially covered in thick black scales (AC + 3, dex - 1).",
     "You are mostly covered in thick black scales (AC + 6, dex - 2).",
     "You are completely covered in thick black scales (AC + 9, dex - 3)."},

    {"You are partially covered in supple grey scales (AC + 1).",
     "You are mostly covered in supple grey scales (AC + 2).",
     "You are completely covered in supple grey scales (AC + 3)."},

    {"You are protected by plates of bone (AC + 2, dex - 1).",
     "You are protected by plates of bone (AC + 3, dex - 2).",
     "You are protected by plates of bone (AC + 4, dex - 3)."},

    {"You are surrounded by a mild repulsion field (ev + 1).",
     "You are surrounded by a moderate repulsion field (ev + 3).",
     "You are surrounded by a strong repulsion field (ev + 5; repel missiles)."},

    {"Your system is immune to poisons.", "Your system is immune to poisons.",
     "Your system is immune to poisons."},
// 10

    {"Your digestive system is specialised to digest meat.",
     "Your digestive system is highly specialised to digest meat.",
     "You are carnivorous and can eat meat at any time."},

    {"You digest meat inefficiently.", "You digest meat very inefficiently.",
     "You are primarily a herbivore."},

    {"Your flesh is heat resistant.", "Your flesh is very heat resistant.",
     "Your flesh is almost immune to the effects of heat."},

    {"Your flesh is cold resistant.", "Your flesh is very cold resistant.",
     "Your flesh is almost immune to the effects of cold."},

    {"You are immune to electric shocks.", "You are immune to electric shocks.",
     "You are immune to electric shocks."},

    {"Your natural rate of healing is unusually fast.",
     "You heal very quickly.",
     "You regenerate."},

    {"You have a fast metabolism.", "You have a very fast metabolism.",
     "Your metabolism is lightning-fast."},

    {"You have a slow metabolism.", "You have a slow metabolism.",
     "You need consume almost no food."},

    {"You are weak (Str -", "", ""},
    {"You are dopey (Int -", "", ""},
// 20
    {"You are clumsy (Dex -", "", ""},

    {"You can control translocations.", "You can control translocations.",
     "You can control translocations."},

    {"Space occasionally distorts in your vicinity.",
     "Space sometimes distorts in your vicinity.",
     "Space frequently distorts in your vicinity."},

    {"You are resistant to magic.", "You are highly resistant to magic.",
     "You are extremely resistant to the effects of magic."},

    {"You cover ground quickly.", "You cover ground very quickly.",
     "You cover ground extremely quickly."},

    {"You have supernaturally acute eyesight.",
     "You have supernaturally acute eyesight.",
     "You have supernaturally acute eyesight."},

    {"Armour fits poorly on your deformed body.",
     "Armour fits poorly on your badly deformed body.",
     "Armour fits poorly on your hideously deformed body."},

    {"You can teleport at will.", "You are good at teleporting at will.",
     "You can teleport instantly at will."},

    {"You can spit poison.", "You can spit poison.", "You can spit poison."},

    {"You can sense your immediate surroundings.",
     "You can sense your surroundings.",
     "You can sense a large area of your surroundings."},

// 30
    {"You can breathe flames.", "You can breathe fire.",
     "You can breathe blasts of fire."},

    {"You can translocate small distances instantaneously.",
     "You can translocate small distances instantaneously.",
     "You can translocate small distances instantaneously."},

    {"You have a pair of small horns on your head.",
     "You have a pair of horns on your head.",
     "You have a pair of large horns on your head."},

    {"Your muscles are strong (Str +1), but stiff (Dex -1).",
     "Your muscles are very strong (Str +2), but stiff (Dex -2).",
     "Your muscles are extremely strong (Str +3), but stiff (Dex -3)."},

    {"Your muscles are flexible (Dex +1), but weak (Str -1).",
     "Your muscles are very flexible (Dex +2), but weak (Str -2).",
     "Your muscles are extremely flexible (Dex +3), but weak (Str -3)."},

    {"You occasionally shout uncontrollably.",
     "You sometimes yell uncontrollably.",
     "You frequently scream uncontrollably."},

    {"You possess an exceptional clarity of mind.",
     "You possess an unnatural clarity of mind.",
     "You possess a supernatural clarity of mind."},

    {"You tend to lose your temper in combat.",
     "You often lose your temper in combat.",
     "You have an uncontrollable temper."},

    {"Your body is slowly deteriorating.", "Your body is deteriorating.",
     "Your body is rapidly deteriorating."},

    {"Your vision is a little blurry.", "Your vision is quite blurry.",
     "Your vision is extremely blurry."},

// 40
    {"You are somewhat resistant to further mutation.",
     "You are somewhat resistant to both further mutation and mutation removal.",
     "Your current mutations are irrevocably fixed, and you can mutate no more."},

    {"You are frail (-10 percent hp).",
     "You are very frail (-20 percent hp).",
     "You are extremely frail (-30 percent hp)."},

    {"You are robust (+10 percent hp).",
     "You are very robust (+20 percent hp).",
     "You are extremely robust (+30 percent hp)."},

    {"You are immune to unholy pain and torment.", "", ""},

    {"You resist negative energy.",
     "You are quite resistant to negative energy.",
     "You are immune to negative energy."},

    // Use player_has_spell() to avoid duplication
    {"You can summon minor demons to your aid.", "", ""},
    {"You can summon demons to your aid.", "", ""},
    {"You can hurl blasts of hellfire.", "", ""},
    {"You can call on the torments of Hell.", "", ""},

    // Not summoners/necromancers/worshippers of Yredelemnul
    {"You can raise the dead to walk for you.", "", ""},
// 50
    {"You can control demons.", "", ""},
    {"You can travel to (but not from) Pandemonium at will.", "", ""},
    {"You can draw strength from death and destruction.", "", ""},

    // Not worshippers of Vehumet
    {"You can channel magical energy from Hell.", "", ""},

    {"You can drain life in unarmed combat.", "", ""},

    // Not conjurers/worshippers of Makhleb
    {"You can throw forth the flames of Gehenna.", "", ""},

    {"You can throw forth the frost of Cocytus.", "", ""},

    {"You can invoke the powers of Tartarus to smite your living foes.", "", ""},

    {"You have sharp fingernails.", "You have very sharp fingernails.",
     "You have claws for hands."},

    {"You have very sharp teeth.", "You have extremely sharp teeth.",
     "You have razor-sharp teeth."},

    // 60 - leave some space for more demonic powers
    {"You have hooves in place of feet.", "", ""},
    {"You have talons in place of feet.", "", ""},

    {"You can exhale a cloud of poison.", "", ""},

    {"Your tail ends in a poisonous barb.",
     "Your tail ends in a sharp poisonous barb.",
     "Your tail ends in a wicked poisonous barb."}, //jmf: nagas & dracos

    {"Your wings are large and strong.", "", ""},       //jmf: dracos only

    //jmf: these next two are for evil gods to mark their followers; good gods
    //     will never accept a 'marked' worshipper

    // 65
    {"There is a blue sigil on each of your hands.",
     "There are several blue sigils on your hands and arms.",
     "Your hands, arms and shoulders are covered in intricate, arcane blue writing."},

    {"There is a green sigil on your chest.",
     "There are several green sigils on your chest and abdomen.",
     "Your chest, abdomen and neck are covered in intricate, arcane green writing."},

    {"You can tolerate rotten meat.", "You can eat rotten meat.",
     "You thrive on rotten meat."},

    {"You are covered in fur.",
     "You are covered in thick fur.",
     "Your thick and shaggy fur keeps you warm."},

    {"You have an increased reservoir of magic (+10 percent mp).",
     "You have a strongly increased reservoir of magic (+20 percent mp).",
     "You have an extremely increased reservoir of magic (+30 percent mp)."},

    // 70
    {"Your magical capacity is low (-10 percent mp).",
     "Your magical capacity is very low (-20 percent mp).",
     "Your magical capacity is extremely low (-30 percent mp)."},

    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},

    // 75
    {"You are partially covered in red scales (AC + 1).",
     "You are mostly covered in red scales (AC + 2).",
     "You are covered in red scales (AC + 4)."},

    {"You are partially covered in smooth nacreous scales (AC + 1).",
     "You are mostly covered in smooth nacreous scales (AC + 3).",
     "You are completely covered in smooth nacreous scales (AC + 5)."},

    {"You are partially covered in ridged grey scales (AC + 2, Dex -1).",
     "You are mostly covered in ridged grey scales (AC + 4, Dex -1).",
     "You are completely covered in ridged grey scales (AC + 6, Dex -2)."},

    {"You are partially covered in metallic scales (AC + 3, Dex -2).",
     "You are mostly covered in metallic scales (AC + 7, Dex -3).",
     "You are completely covered in metallic scales (AC + 10, Dex -4)."},

    {"You are partially covered in black scales (AC + 1).",
     "You are mostly covered in black scales (AC + 3).",
     "You are completely covered in black scales (AC + 5)."},

    {"You are partially covered in white scales (AC + 1).",
     "You are mostly covered in white scales (AC + 3).",
     "You are completely covered in white scales (AC + 5)."},

    {"You are partially covered in yellow scales (AC + 2).",
     "You are mostly covered in yellow scales (AC + 4, Dex -1).",
     "You are completely covered in yellow scales (AC + 6, Dex -2)."},

    {"You are partially covered in brown scales (AC + 2).",
     "You are mostly covered in brown scales (AC + 4).",
     "You are completely covered in brown scales (AC + 5)."},

    {"You are partially covered in blue scales (AC + 1).",
     "You are mostly covered in blue scales (AC + 2).",
     "You are completely covered in blue scales (AC + 3)."},

    {"You are partially covered in purple scales (AC + 2).",
     "You are mostly covered in purple scales (AC + 4).",
     "You are completely covered in purple scales (AC + 6)."},

    // 85
    {"You are partially covered in speckled scales (AC + 1).",
     "You are mostly covered in speckled scales (AC + 2).",
     "You are covered in speckled scales (AC + 3)."},

    {"You are partially covered in orange scales (AC + 1).",
     "You are mostly covered in orange scales (AC + 3).",
     "You are completely covered in orange scales (AC + 4)."},

    {"You are partially covered in indigo scales (AC + 2).",
     "You are mostly covered in indigo scales (AC + 3).",
     "You are completely covered in indigo scales (AC + 5)."},

    {"You are partially covered in knobbly red scales (AC + 2).",
     "You are mostly covered in knobbly red scales (AC + 5, Dex -1).",
     "You are completely covered in knobbly red scales (AC + 7, Dex -2)."},

    {"You are partially covered in iridescent scales (AC + 1).",
     "You are mostly covered in iridescent scales (AC + 2).",
     "You are completely covered in iridescent scales (AC + 3)."},

    {"You are partially covered in patterned scales (AC + 1).",
     "You are mostly covered in patterned scales (AC + 2).",
     "You are completely covered in patterned scales (AC + 3)."}
};

// If giving a mutation which must succeed (eg demonspawn), must add
// exception to the "resist mutation" mutation thing.

const char *gain_mutation[][3] = {
    {"Your skin toughens.", "Your skin toughens.", "Your skin toughens."},

    {"", "", ""},  // replaced with player::modify_stat() handling {dlb}
    {"", "", ""},  // replaced with player::modify_stat() handling {dlb}
    {"", "", ""},  // replaced with player::modify_stat() handling {dlb}

    {"Green scales grow over part of your body.",
     "Green scales spread over more of your body.",
     "Green scales cover you completely."},

    {"Thick black scales grow over part of your body.",
     "Thick black scales spread over more of your body.",
     "Thick black scales cover you completely."},

    {"Supple grey scales grow over part of your body.",
     "Supple grey scales spread over more of your body.",
     "Supple grey scales cover you completely."},

    {"You grow protective plates of bone.",
     "You grow more protective plates of bone.",
     "You grow more protective plates of bone."},

    {"You begin to radiate repulsive energy.",
     "Your repulsive radiation grows stronger.",
     "Your repulsive radiation grows stronger."},

    {"You feel healthy.", "You feel healthy.",  "You feel healthy."},

// 10
    {"You hunger for flesh.", "You hunger for flesh.", "You hunger for flesh."},

    {"You hunger for vegetation.", "You hunger for vegetation.",
     "You hunger for vegetation."},

    {"You feel a sudden chill.", "You feel a sudden chill.",
     "You feel a sudden chill."},

    {"You feel hot for a moment.", "You feel hot for a moment.",
     "You feel hot for a moment."},

    {"You feel insulated.", "You feel insulated.", "You feel insulated."},

    {"You begin to heal more quickly.",
     "You begin to heal more quickly.",
     "You begin to regenerate."},

    {"You feel a little hungry.", "You feel a little hungry.",
     "You feel a little hungry."},

    {"Your metabolism slows.", "Your metabolism slows.",
     "Your metabolism slows."},

    {"You feel weaker.", "You feel weaker.", "You feel weaker."},

    {"You feel less intelligent.", "You feel less intelligent.",
     "You feel less intelligent."},

// 20
    {"You feel clumsy.", "You feel clumsy.",
     "You feel clumsy."},

    {"You feel controlled.", "You feel controlled.",
     "You feel controlled."},

    {"You feel weirdly uncertain.",
     "You feel even more weirdly uncertain.",
     "You feel even more weirdly uncertain."},

    {"You feel resistant to magic.",
     "You feel more resistant to magic.",
     "You feel almost impervious to the effects of magic."},

    {"You feel quick.", "You feel quick.", "You feel quick."},

    {"Your vision sharpens.", "Your vision sharpens.", "Your vision sharpens."},

    {"Your body twists and deforms.", "Your body twists and deforms.",
     "Your body twists and deforms."},

    {"You feel jumpy.", "You feel more jumpy.", "You feel even more jumpy."},

    {"There is a nasty taste in your mouth for a moment.",
     "There is a nasty taste in your mouth for a moment.",
     "There is a nasty taste in your mouth for a moment."},

    {"You feel aware of your surroundings.",
     "You feel more aware of your surroundings.",
     "You feel even more aware of your surroundings."},

// 30
    {"Your throat feels hot.", "Your throat feels hot.",
     "Your throat feels hot."},

    {"You feel a little jumpy.", "You feel more jumpy.",
     "You feel even more jumpy."},

    {"A pair of horns grows on your head!",
     "The horns on your head grow some more.",
     "The horns on your head grow some more."},

    {"Your muscles feel sore.", "Your muscles feel sore.",
     "Your muscles feel sore."},

    {"Your muscles feel loose.", "Your muscles feel loose.",
     "Your muscles feel loose."},

    {"You feel the urge to shout.", "You feel a strong urge to yell.",
     "You feel a strong urge to scream."},

    {"Your thoughts seem clearer.", "Your thoughts seem clearer.",
     "Your thoughts seem clearer."},

    {"You feel a little pissed off.", "You feel angry.",
     "You feel extremely angry at everything!"},

    {"You feel yourself wasting away.", "You feel yourself wasting away.",
     "You feel your body start to fall apart."},

    {"Your vision blurs.", "Your vision blurs.", "Your vision blurs."},

// 40
    {"You feel genetically stable.", "You feel genetically stable.",
     "You feel genetically immutable."},

    {"You feel frail.", "You feel frail.",  "You feel frail."},
    {"You feel robust.", "You feel robust.", "You feel robust."},
    {"You feel a strange anaesthesia.", "", ""},
    {"You feel negative.", "You feel negative.", "You feel negative."},
    {"A thousand chattering voices call out to you.", "", ""},
    {"Help is not far away!", "", ""},
    {"You smell fire and brimstone.", "", ""},
    {"You feel a terrifying power at your call.", "", ""},
    {"You feel an affinity for the dead.", "", ""},
// 50
    {"You feel an affinity for all demonkind.", "", ""},
    {"You feel something pulling you to a strange and terrible place.", "", ""},
    {"You feel hungry for death.", "", ""},
    {"You feel a flux of magical energy.", "", ""},
    {"Your skin tingles in a strangely unpleasant way.", "", ""},
    {"You smell the fires of Gehenna.", "", ""},
    {"You feel the icy cold of Cocytus chill your soul.", "", ""},
    {"A shadow passes over the world around you.", "", ""},

    {"Your fingernails lengthen.", "Your fingernails sharpen.",
     "Your hands twist into claws."},

    {"Your teeth lengthen and sharpen.",
     "Your teeth lengthen and sharpen some more.",
     "Your teeth are very long and razor-sharp."},

    // 60
    {"Your feet shrivel into cloven hooves.", "", ""},
    {"Your feet stretch and sharpen into talons.", "", ""},

    {"You taste something nasty.", "You taste something very nasty.",
     "You taste something extremely nasty."},

    {"A poisonous barb forms on the end of your tail.",
     "The barb on your tail looks sharper.",
     "The barb on your tail looks very sharp."},

    {"Your wings grow larger and stronger.", "", ""},

    // 65
    {"Your hands itch.", "Your hands and forearms itch.",
     "Your arms, hands and shoulders itch."},

    {"Your chest itches.", "Your chest and abdomen itch.",
     "Your chest, abdomen and neck itch."},

    // saprovorous: can never be gained or lost, only started with
    {"", "", ""},

    {"Fur sprouts all over your body.",
     "Your fur grows into a thick mane.",
     "Your thick fur grows shaggy and warm."},

    {"You feel more energetic.", "You feel more energetic.",
     "You feel more energetic."},

    // 70
    {"You feel less energetic.", "You feel less energetic.",
     "You feel less energetic."},

    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},

    // 75
    {"Red scales grow over part of your body.",
     "Red scales spread over more of your body.",
     "Red scales cover you completely."},
    {"Smooth nacreous scales grow over part of your body.",
     "Smooth nacreous scales spread over more of your body.",
     "Smooth nacreous scales cover you completely."},
    {"Ridged grey scales grow over part of your body.",
     "Ridged grey scales spread over more of your body.",
     "Ridged grey scales cover you completely."},
    {"Metallic scales grow over part of your body.",
     "Metallic scales spread over more of your body.",
     "Metallic scales cover you completely."},
    {"Black scales grow over part of your body.",
     "Black scales spread over more of your body.",
     "Black scales cover you completely."},
    {"White scales grow over part of your body.",
     "White scales spread over more of your body.",
     "White scales cover you completely."},
    {"Yellow scales grow over part of your body.",
     "Yellow scales spread over more of your body.",
     "Yellow scales cover you completely."},
    {"Brown scales grow over part of your body.",
     "Brown scales spread over more of your body.",
     "Brown scales cover you completely."},
    {"Blue scales grow over part of your body.",
     "Blue scales spread over more of your body.",
     "Blue scales cover you completely."},
    {"Purple scales grow over part of your body.",
     "Purple scales spread over more of your body.",
     "Purple scales cover you completely."},

    // 85
    {"Speckled scales grow over part of your body.",
     "Speckled scales spread over more of your body.",
     "Speckled scales cover you completely."},
    {"Orange scales grow over part of your body.",
     "Orange scales spread over more of your body.",
     "Orange scales cover you completely."},
    {"Indigo scales grow over part of your body.",
     "Indigo scales spread over more of your body.",
     "Indigo scales cover you completely."},
    {"Knobbly red scales grow over part of your body.",
     "Knobbly red scales spread over more of your body.",
     "Knobbly red scales cover you completely."},
    {"Iridescent scales grow over part of your body.",
     "Iridescent scales spread over more of your body.",
     "Iridescent scales cover you completely."},
    {"Patterned scales grow over part of your body.",
     "Patterned scales spread over more of your body.",
     "Patterned scales cover you completely."}
};

const char *lose_mutation[][3] = {

    {"Your skin feels delicate.", "Your skin feels delicate.",
     "Your skin feels delicate."},

    {"You feel weaker.", "You feel weaker.", "You feel weaker."},

    {"You feel less intelligent.", "You feel less intelligent.",
     "You feel less intelligent."},

    {"You feel clumsy.", "You feel clumsy.", "You feel clumsy."},

    {"Your green scales disappear.",
     "Your green scales recede somewhat.",
     "Your green scales recede somewhat."},

    {"Your black scales disappear.", "Your black scales recede somewhat.",
     "Your black scales recede somewhat."},

    {"Your grey scales disappear.", "Your grey scales recede somewhat.",
     "Your grey scales recede somewhat."},

    {"Your bony plates shrink away.", "Your bony plates shrink.",
     "Your bony plates shrink."},

    {"You feel attractive.", "You feel attractive.", "You feel attractive."},

    {"You feel a little less healthy.", "You feel a little less healthy.",
     "You feel a little less healthy."},

    {"You feel able to eat a more balanced diet.",
     "You feel able to eat a more balanced diet.",
     "You feel able to eat a more balanced diet."},

    {"You feel able to eat a more balanced diet.",
     "You feel able to eat a more balanced diet.",
     "You feel able to eat a more balanced diet."},

    {"You feel hot for a moment.", "You feel hot for a moment.",
     "You feel hot for a moment."},

    {"You feel a sudden chill.", "You feel a sudden chill.",
     "You feel a sudden chill."},

    {"You feel conductive.", "You feel conductive.", "You feel conductive."},

    {"Your rate of healing slows.", "Your rate of healing slows.",
     "Your rate of healing slows."},

    {"Your metabolism slows.", "Your metabolism slows.",
     "Your metabolism slows."},

    {"You feel a little hungry.", "You feel a little hungry.",
     "You feel a little hungry."},

    {"", "", ""},  // replaced with player::modify_stat() handling {dlb}
    {"", "", ""},  // replaced with player::modify_stat() handling {dlb}
// 20
    {"", "", ""},  // replaced with player::modify_stat() handling {dlb}

    {"You feel random.", "You feel uncontrolled.", "You feel uncontrolled."},
    {"You feel stable.", "You feel stable.", "You feel stable."},

    {"You feel less resistant to magic.", "You feel less resistant to magic.",
     "You feel vulnerable to magic again."},

    {"You feel sluggish.", "You feel sluggish.", "You feel sluggish."},

    {"Your vision seems duller.", "Your vision seems duller.",
     "Your vision seems duller."},

    {"Your body's shape seems more normal.",
     "Your body's shape seems slightly more normal.",
     "Your body's shape seems slightly more normal."},

    {"You feel static.", "You feel less jumpy.", "You feel less jumpy."},

    {"You feel an ache in your throat.",
     "You feel an ache in your throat.", "You feel an ache in your throat."},

    {"You feel slightly disorientated.", "You feel slightly disorientated.",
     "You feel slightly disorientated."},

// 30
    {"A chill runs up and down your throat.",
     "A chill runs up and down your throat.",
     "A chill runs up and down your throat."},

    {"You feel a little less jumpy.", "You feel less jumpy.",
     "You feel less jumpy."},

    {"The horns on your head shrink away.",
     "The horns on your head shrink a bit.",
     "The horns on your head shrink a bit."},

    {"Your muscles feel loose.", "Your muscles feel loose.",
     "Your muscles feel loose."},

    {"Your muscles feel sore.", "Your muscles feel sore.",
     "Your muscles feel sore."},

    {"Your urge to shout disappears.", "Your urge to yell lessens.",
     "Your urge to scream lessens."},

    {"Your thinking seems confused.", "Your thinking seems confused.",
     "Your thinking seems confused."},

    {"You feel a little more calm.", "You feel a little less angry.",
     "You feel a little less angry."},

    {"You feel healthier.", "You feel a little healthier.",
     "You feel a little healthier."},

    {"Your vision sharpens.", "Your vision sharpens a little.",
     "Your vision sharpens a little."},

// 40
    {"You feel genetically unstable.", "You feel genetically unstable.",
     "You feel genetically unstable."},

    {"You feel robust.", "You feel robust.", "You feel robust."},
    {"You feel frail.", "You feel frail.", "You feel frail."},

// Some demonic powers (which can't be lost) start here...
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
// 50
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},

    {"Your fingernails shrink to normal size.",
     "Your fingernails look duller.", "Your hands feel fleshier."},

    {"Your teeth shrink to normal size.",
     "Your teeth shrink and become duller.",
     "Your teeth shrink and become duller."},

    // 60
    {"Your hooves expand and flesh out into feet!", "", ""},
    {"Your talons dull and shrink into feet.", "", ""},

    {"", "", ""},
    {"", "", ""},
    {"", "", ""},

    // 65
    {"", "", ""},
    {"", "", ""},

    // saprovorous: can never be gained or lost, only started with
    {"", "", ""},

    {"You shed all your fur.",
     "Your thick fur recedes somewhat.",
     "Your shaggy fur recedes somewhat."},

    {"You feel less energetic.", "You feel less energetic.",
     "You feel less energetic."},

    // 70
    {"You feel more energetic.", "You feel more energetic.",
     "You feel more energetic."},

    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},

    // 75
    {"Your red scales disappear.", "Your red scales recede somewhat.",
     "Your red scales recede somewhat."},

    {"Your smooth nacreous scales disappear.",
     "Your smooth nacreous scales recede somewhat.",
     "Your smooth nacreous scales recede somewhat."},

    {"Your ridged grey scales disappear.",
     "Your ridged grey scales recede somewhat.",
     "Your ridged grey scales recede somewhat."},

    {"Your metallic scales disappear.",
     "Your metallic scales recede somewhat.",
     "Your metallic scales recede somewhat."},

    {"Your black scales disappear.", "Your black scales recede somewhat.",
     "Your black scales recede somewhat."},

    {"Your white scales disappear.", "Your white scales recede somewhat.",
     "Your white scales recede somewhat."},

    {"Your yellow scales disappear.", "Your yellow scales recede somewhat.",
     "Your yellow scales recede somewhat."},

    {"Your brown scales disappear.", "Your brown scales recede somewhat.",
     "Your brown scales recede somewhat."},

    {"Your blue scales disappear.", "Your blue scales recede somewhat.",
     "Your blue scales recede somewhat."},

    {"Your purple scales disappear.", "Your purple scales recede somewhat.",
     "Your purple scales recede somewhat."},

    // 85
    {"Your speckled scales disappear.",
     "Your speckled scales recede somewhat.",
     "Your speckled scales recede somewhat."},

    {"Your orange scales disappear.", "Your orange scales recede somewhat.",
     "Your orange scales recede somewhat."},

    {"Your indigo scales disappear.", "Your indigo scales recede somewhat.",
     "Your indigo scales recede somewhat."},

    {"Your knobbly red scales disappear.",
     "Your knobbly red scales recede somewhat.",
     "Your knobbly red scales recede somewhat."},

    {"Your iridescent scales disappear.",
     "Your iridescent scales recede somewhat.",
     "Your iridescent scales recede somewhat."},

    {"Your patterned scales disappear.",
     "Your patterned scales recede somewhat.",
     "Your patterned scales recede somewhat."}
};

// mutation definitions:
// first  number  = probability  (0 means it doesn't appear naturally?)
// second number  = maximum levels
// first  boolean = is mutation mostly bad?
// second boolean = is mutation physical, i.e. external only?

static mutation_def mutation_defs[] = {
    { MUT_TOUGH_SKIN,                10,  3, false,  true },
    { MUT_STRONG,                     8, 14, false,  true },
    { MUT_CLEVER,                     8, 14, false,  true },
    { MUT_AGILE,                      8, 14, false,  true },
    { MUT_GREEN_SCALES,               2,  3, false,  true },
    { MUT_BLACK_SCALES,               1,  3, false,  true },
    { MUT_GREY_SCALES,                2,  3, false,  true },
    { MUT_BONEY_PLATES,               1,  3, false,  true },
    { MUT_REPULSION_FIELD,            1,  3, false, false },
    { MUT_POISON_RESISTANCE,          4,  1, false, false },
// 10
    { MUT_CARNIVOROUS,                5,  3, false, false },
    { MUT_HERBIVOROUS,                5,  3,  true, false },
    { MUT_HEAT_RESISTANCE,            4,  3, false, false },
    { MUT_COLD_RESISTANCE,            4,  3, false, false },
    { MUT_SHOCK_RESISTANCE,           2,  1, false, false },
    { MUT_REGENERATION,               3,  3, false, false },
    { MUT_FAST_METABOLISM,           10,  3,  true, false },
    { MUT_SLOW_METABOLISM,            7,  3, false, false },
    { MUT_WEAK,                      10, 14,  true,  true },
    { MUT_DOPEY,                     10, 14,  true,  true },
// 20
    { MUT_CLUMSY,                    10, 14,  true,  true },
    { MUT_TELEPORT_CONTROL,           2,  1, false, false },
    { MUT_TELEPORT,                   3,  3,  true, false },
    { MUT_MAGIC_RESISTANCE,           5,  3, false, false },
    { MUT_FAST,                       1,  3, false, false },
    { MUT_ACUTE_VISION,               2,  1, false, false },
    { MUT_DEFORMED,                   8,  3,  true,  true },
    { MUT_TELEPORT_AT_WILL,           2,  3, false, false },
    { MUT_SPIT_POISON,                8,  3, false, false },
    { MUT_MAPPING,                    3,  3, false, false },
// 30
    { MUT_BREATHE_FLAMES,             4,  3, false, false },
    { MUT_BLINK,                      3,  3, false, false },
    { MUT_HORNS,                      7,  3, false,  true },
    { MUT_STRONG_STIFF,              10,  3, false,  true },
    { MUT_FLEXIBLE_WEAK,             10,  3, false,  true },
    { MUT_SCREAM,                     6,  3,  true, false },
    { MUT_CLARITY,                    6,  1, false, false },
    { MUT_BERSERK,                    7,  3,  true, false },
    { MUT_DETERIORATION,             10,  3,  true, false },
    { MUT_BLURRY_VISION,             10,  3,  true, false },
// 40
    { MUT_MUTATION_RESISTANCE,        4,  3, false, false },
    { MUT_FRAIL,                     10,  3,  true,  true },
    { MUT_ROBUST,                     5,  3, false,  true },

// Some demonic powers start here:
    { MUT_TORMENT_RESISTANCE,         0,  1, false, false },
    { MUT_NEGATIVE_ENERGY_RESISTANCE, 0,  3, false, false },
    { MUT_SUMMON_MINOR_DEMONS,        0,  1, false, false },
    { MUT_SUMMON_DEMONS,              0,  1, false, false },
    { MUT_HURL_HELLFIRE,              0,  1, false, false },
    { MUT_CALL_TORMENT,               0,  1, false, false },
    { MUT_RAISE_DEAD,                 0,  1, false, false },
// 50
    { MUT_CONTROL_DEMONS,             0,  1, false, false },
    { MUT_PANDEMONIUM,                0,  1, false, false },
    { MUT_DEATH_STRENGTH,             0,  1, false, false },
    { MUT_CHANNEL_HELL,               0,  1, false, false },
    { MUT_DRAIN_LIFE,                 0,  1, false, false },
    { MUT_THROW_FLAMES,               0,  1, false, false },
    { MUT_THROW_FROST,                0,  1, false, false },
    { MUT_SMITE,                      0,  1, false, false },
// end of demonic powers

    { MUT_CLAWS,                      2,  3, false,  true },
    { MUT_FANGS,                      1,  3, false,  true },
// 60
    { MUT_HOOVES,                     1,  1, false,  true },
    { MUT_TALONS,                     1,  1, false,  true },

    // Naga only
    { MUT_BREATHE_POISON,             0,  1, false, false },
    // Naga and Draconian only
    { MUT_STINGER,                    0,  3, false,  true },
    // Draconian only
    { MUT_BIG_WINGS,                  0,  1, false,  true },
// 65
     // used by evil gods to mark followers (currently UNUSED)
    { MUT_BLUE_MARKS,                 0,  3, false,  true },
    { MUT_GREEN_MARKS,                0,  3, false,  true },

    // species-dependent innate mutation
    { MUT_SAPROVOROUS,                0,  3, false, false },

    { MUT_SHAGGY_FUR,                 2,  3, false,  true },

    { MUT_HIGH_MAGIC,                 1,  3, false, false },
// 70
    { MUT_LOW_MAGIC,                  9,  3,  true, false },
    { RANDOM_MUTATION,                0,  3, false, false },
    { RANDOM_MUTATION,                0,  3, false, false },
    { RANDOM_MUTATION,                0,  3, false, false },
    { RANDOM_MUTATION,                0,  3, false, false },

// 75 -- scales of various colours and effects
    { MUT_RED_SCALES,                 2,  3, false,  true },
    { MUT_NACREOUS_SCALES,            1,  3, false,  true },
    { MUT_GREY2_SCALES,               2,  3, false,  true },
    { MUT_METALLIC_SCALES,            1,  3, false,  true },
    { MUT_BLACK2_SCALES,              2,  3, false,  true },
    { MUT_WHITE_SCALES,               2,  3, false,  true },
    { MUT_YELLOW_SCALES,              2,  3, false,  true },
    { MUT_BROWN_SCALES,               2,  3, false,  true },
    { MUT_BLUE_SCALES,                2,  3, false,  true },
    { MUT_PURPLE_SCALES,              2,  3, false,  true },
// 85
    { MUT_SPECKLED_SCALES,            2,  3, false,  true },
    { MUT_ORANGE_SCALES,              2,  3, false,  true },
    { MUT_INDIGO_SCALES,              2,  3, false,  true },
    { MUT_RED2_SCALES,                1,  3, false,  true },
    { MUT_IRIDESCENT_SCALES,          1,  3, false,  true },
    { MUT_PATTERNED_SCALES,           1,  3, false,  true }
};

COMPILE_CHECK(ARRAYSZ(mutation_defs) == NUM_MUTATIONS, c1);

#ifdef DEBUG_DIAGNOSTICS
void sanity_check_mutation_defs()
{

    for (unsigned i = 0; i < ARRAYSZ(mutation_defs); ++i)
    {
        const mutation_def &mdef(mutation_defs[i]);
        ASSERT(mdef.mutation == static_cast<mutation_type>(i)
               || mdef.mutation == RANDOM_MUTATION);
    }
}
#endif

void fixup_mutations()
{
    if (player_genus(GENPC_DRACONIAN))
        mutation_defs[MUT_BIG_WINGS].rarity = 1;
}

bool mutation_is_fully_active(mutation_type mut)
{
    // For all except the semi-undead, mutations always apply.
    if (you.is_undead != US_SEMI_UNDEAD)
        return (true);

    // Innate mutations are always active
    if (you.demon_pow[mut])
        return (true);

    // ... as are physical mutations
    if (mutation_defs[mut].physical)
        return (true);

    // ... as are all mutations for semi-undead who are fully alive.
    if (you.hunger_state == HS_ENGORGED)
        return (true);

    return (false);
}

static bool _mutation_is_fully_inactive(mutation_type mut)
{
    return (you.is_undead == US_SEMI_UNDEAD && you.hunger_state < HS_SATIATED
            && !you.demon_pow[mut] && !mutation_defs[mut].physical);
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

    result += "<lightblue>"; // inborn abilities and weirdness

    switch (you.species)   //mv: following code shows innate abilities - if any
    {
    case SP_MERFOLK:
        result += "You revert to your normal form in water." EOL;
        have_any = true;
        break;

    case SP_NAGA:
        if ( you.mutation[MUT_DEFORMED] > 1)
            result += "</lightblue><cyan>";
        result += naga_deformed_descrip[you.mutation[MUT_DEFORMED] - 1];
        if ( you.mutation[MUT_DEFORMED] > 1)
            result += "</cyan><lightblue>";
        result += EOL;
        result += "You cannot wear boots." EOL;

        // breathe poison replaces spit poison
        if (!you.mutation[MUT_BREATHE_POISON])
            result += "You can spit poison." EOL;
        else
        {
            result += "</lightblue><cyan>You can exhale a cloud of poison."
                      "</cyan><lightblue>" EOL ;
        }

        // slowness can be overridden
        if ( you.mutation[MUT_FAST] )
            result += "</lightblue><cyan>";
        result += naga_speed_descrip[you.mutation[MUT_FAST]];
        if ( you.mutation[MUT_FAST] )
            result += "</cyan>";
        result += EOL;
        have_any = true;
        break;

    case SP_TROLL:
        if ( you.mutation[MUT_CLAWS] )
            result += "</lightblue><cyan>";
        result += troll_claw_descrip[you.mutation[MUT_CLAWS]];
        if ( you.mutation[MUT_CLAWS] )
            result += "</cyan><lightblue>";
        result += EOL;
        have_any = true;
        break;

    case SP_CENTAUR:
        if ( you.mutation[MUT_DEFORMED] > 1)
            result += "</lightblue><cyan>";
        result += centaur_deformed_descrip[you.mutation[MUT_DEFORMED] - 1];
        if ( you.mutation[MUT_DEFORMED] > 1)
            result += "</cyan><lightblue>";
        result += EOL;
        have_any = true;
        break;


    case SP_GHOUL:
        result += "Your body is rotting away." EOL;
        result += troll_claw_descrip[you.mutation[MUT_CLAWS]];
        result += EOL;
        result += "You heal slowly." EOL;
        have_any = true;
        break;

    case SP_KENKU:
        result += "You cannot wear helmets." EOL;
        if (you.experience_level > 4)
        {
            result += "You can fly";
            if (you.experience_level > 14)
                result += " continuously";
            result += "." EOL;
        }
        have_any = true;
        break;

    case SP_MUMMY:
        result += "You are";
        if (you.experience_level > 25)
            result += " very strongly";
        else if (you.experience_level > 12)
            result += " strongly";

        result += " in touch with the powers of death." EOL;
        result += "Your flesh is vulnerable to fire." EOL;

        if (you.experience_level > 12)
            result += "You can restore your body by infusing magical energy." EOL;
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
            result += "You can breathe frost." EOL;
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

    case SP_GOLDEN_DRACONIAN:
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
        if (you.hunger_state < HS_SATIATED)
        {
            if (you.experience_level >= 13)
            {
                result += "<green>";
                result += "You are";
                result += " in touch with the powers of death." EOL;
                result += "</green>";
            }
        }

        if (you.hunger_state == HS_STARVING)
            result += "<green>You do not heal.</green>" EOL;
        else if (you.hunger_state <= HS_HUNGRY)
            result += "<green>You heal slowly.</green>" EOL;
        else if (you.hunger_state >= HS_FULL)
            result += "<green>Your natural rate of healing is unusually fast.</green>" EOL;
        else if (you.hunger_state == HS_ENGORGED)
            result += "<green>Your natural rate of healing is extremely fast.</green>" EOL;
        have_any = true;
        break;

    default:
        break;
    }                           //end switch - innate abilities

    // a bit more stuff
    if ( (you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE) ||
         player_genus(GENPC_DRACONIAN) ||
         you.species == SP_SPRIGGAN )
    {
        result += "Your body does not fit into most forms of armour." EOL;
        have_any = true;
    }

    result += "</lightblue>";

    if ( beogh_water_walk() )
    {
        result += "<green>You can walk on water.</green>" EOL;
        have_any = true;
    }

    textcolor(LIGHTGREY);

    // first add (non-removable) inborn abilities and demon powers
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        // mutation is actually a demonic power
        if (you.mutation[i] != 0 && you.demon_pow[i])
        {
            have_any = true;

            // these are already handled above:
            if (you.species == SP_NAGA &&
                (i == MUT_BREATHE_POISON || i == MUT_FAST || i == MUT_DEFORMED))
            {
                continue;
            }
            if (you.species == SP_TROLL && i == MUT_CLAWS)
                continue;
            if (you.species == SP_CENTAUR && i == MUT_DEFORMED)
                continue;

            const bool fully_active
                        = mutation_is_fully_active((mutation_type) i);
            bool fully_inactive = false;
            if (!fully_active)
                fully_inactive = _mutation_is_fully_inactive((mutation_type) i);

            const char* colourname = "";
            if ( you.species == SP_DEMONSPAWN )
            {
                if (fully_inactive)
                    colourname = "darkgray";
                else if (!fully_active)
                    colourname = "yellow";
                else if ( you.demon_pow[i] < you.mutation[i] )
                    colourname = "lightred";
                else
                    colourname = "red";
            }
            else            // innate ability
            {
                if (fully_inactive)
                    colourname = "darkgray";
                else if (!fully_active)
                    colourname = "blue";
                else if ( you.demon_pow[i] < you.mutation[i] )
                    colourname = "cyan";
                else
                    colourname = "lightblue";
            }

            result += '<';
            result += colourname;
            result += '>';
            if (fully_inactive)
                result += "(";

            result += mutation_name(static_cast<mutation_type>(i));

            if (fully_inactive)
                result += ")";
            result += "</";
            result += colourname;
            result += '>';
            result += EOL;

            // Gourmand is *not* identical to being saprovorous, therefore...
            if (i == MUT_SAPROVOROUS && you.omnivorous())
                result += "<lightblue>You like to eat raw meat.</lightblue>" EOL;
        }
    }

    // now add removable mutations
    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i] != 0 && !you.demon_pow[i])
        {
            // this is already handled above:
            if (you.species == SP_NAGA
                && (i == MUT_BREATHE_POISON || i == MUT_FAST))
            {
                continue;
            }
            if (you.species == SP_TROLL && i == MUT_CLAWS)
                continue;

            have_any = true;

            // not currently active?
            const bool need_grey = !mutation_is_fully_active((mutation_type) i);
            bool inactive = false;
            if (need_grey)
            {
                result += "<darkgrey>";
                if (_mutation_is_fully_inactive((mutation_type) i))
                {
                    inactive = true;
                    result += "(";
                }
            }

            result += mutation_name(static_cast<mutation_type>(i));

            if (need_grey)
            {
                if (inactive)
                    result += ")";
                result += "</darkgrey>";
            }
            result += EOL;
        }
    }

    if (!have_any)
        result +=  "You are rather mundane." EOL;

    if (you.species == SP_VAMPIRE)
    {
        result += EOL EOL;
        result += EOL EOL;
        result += "Press '<w>!</w>' to toggle between mutations and properties depending on your" EOL
                  "hunger status." EOL;
    }

    return formatted_string::parse_string(result);
}

static void _display_vampire_attributes()
{
    clrscr();
    cgotoxy(1,1);

    std::string result;

    std::string column[11][7] =
    {
       {"                     ", "<lightgreen>Alive</lightgreen>      ", "<green>Full</green>    ",
        "Satiated  ", "<yellow>Thirsty</yellow>  ", "<yellow>Near...</yellow>  ",
        "<lightred>Bloodless</lightred>"},
                                //Alive          Full       Satiated       Thirsty   Near...      Bloodless
       {"Metabolism           ", "very fast  ", "fast    ", "fast      ",  "normal   ", "slow     ", "none"},

       {"Regeneration         ", "very fast  ", "fast    ", "normal    ",  "normal   ", "slow     ", "none"},

       {"Poison resistance    ", "           ", "        ", " +        ",  " +       ", " +       ", " +   "},

       {"Cold resistance      ", "           ", "        ", "          ",  " +       ", " +       ", " ++  "},

       {"Negative resistance  ", "           ", "        ", "          ",  " +       ", " ++      ", " +++ "},

       {"Torment resistance   ", "           ", "        ", "          ",  "         ", "         ", " +   "},

       {"Mutation chance      ", "always     ", "often   ", "sometimes ",  "never    ", "never    ", "never"},

       {"Mutation effects     ", "full       ", "capped  ", "capped    ",  "none     ", "none     ", "none "},

       {"Stealth boost        ", "none       ", "none    ", "none      ",  "minor    ", "major    ", "large"},

       {"Bat Form             ", "no         ", "no      ", "yes       ",  "yes      ", "yes      ", "yes  "}
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

    for (int y = 0; y < 11; y++)      // lines   (properties)
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
/*
    result = "                     <lightgreen>Alive</lightgreen>      <green>Full</green>    Satiated  "
                                  "<yellow>Thirsty  Near...</yellow>  <lightred>Bloodless</lightred>" EOL
             "Metabolism           very fast  fast    fast      normal   slow     none " EOL
             "Regeneration         very fast  fast    normal    normal   slow     none " EOL
             "Poison resistance                        +         +        +        +   " EOL
             "Cold resistance                                    +        +        ++  " EOL
             "Negative resistance                                +        ++       +++ " EOL
             "Torment resistance                                                   +   " EOL
             "Mutation effects     full       capped  capped    none     none     none " EOL
             "Stealth boost        none       none    none      minor    major    large" EOL;
*/

    result += EOL EOL;
    result += EOL EOL;
    result += EOL EOL;
    result += "Press '<w>!</w>' to toggle between mutations and properties depending on your " EOL
              "hunger status." EOL;

    const formatted_string vp_props = formatted_string::parse_string(result);
    vp_props.display();

    if (you.species == SP_VAMPIRE)
    {
        const int keyin = getch();
        if (keyin == '!')
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
        const int keyin = getch();
        if (keyin == '!')
            _display_vampire_attributes();
    }
    else
    {
        Menu mutation_menu(mutation_fs);
        mutation_menu.show();
    }
}

static int calc_mutation_amusement_value(mutation_type which_mutation)
{
    int amusement = 16 * (11 - mutation_defs[which_mutation].rarity);

    switch (which_mutation)
    {
    case MUT_STRONG:
    case MUT_CLEVER:
    case MUT_AGILE:
    case MUT_POISON_RESISTANCE:
    case MUT_SHOCK_RESISTANCE:
    case MUT_SLOW_METABOLISM:
    case MUT_TELEPORT_CONTROL:
    case MUT_MAGIC_RESISTANCE:
    case MUT_TELEPORT_AT_WILL:
    case MUT_MAPPING:
    case MUT_BLINK:
    case MUT_CLARITY:
    case MUT_MUTATION_RESISTANCE:
    case MUT_ROBUST:
    case MUT_HIGH_MAGIC:
        amusement /= 2;  // not funny
        break;

    case MUT_CARNIVOROUS:
    case MUT_HERBIVOROUS:
    case MUT_FAST_METABOLISM:
    case MUT_WEAK:
    case MUT_DOPEY:
    case MUT_CLUMSY:
    case MUT_TELEPORT:
    case MUT_FAST:
    case MUT_DEFORMED:
    case MUT_SPIT_POISON:
    case MUT_BREATHE_FLAMES:
    case MUT_HORNS:
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
    case MUT_BLUE_MARKS:
    case MUT_GREEN_MARKS:
    case MUT_LOW_MAGIC:
        amusement *= 2; // funny!
        break;

    default:
        break;
    }

    return (amusement);
}

static bool accept_mutation( mutation_type mutat, bool ignore_rarity = false )
{
    if ( you.mutation[mutat] >= mutation_defs[mutat].levels )
        return (false);

    if ( ignore_rarity )
        return (true);

    const int rarity = mutation_defs[mutat].rarity + you.demon_pow[mutat];
    // low rarity means unlikely to choose it
    return (rarity > random2(10));
}

static mutation_type get_random_xom_mutation()
{
    mutation_type mutat = NUM_MUTATIONS;
    do
    {
        mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

        if (one_chance_in(1000))
            return (NUM_MUTATIONS);
        if (one_chance_in(5))
        {
            switch (random2(8))
            {
            case 0: mutat = MUT_WEAK; break;
            case 1: mutat = MUT_DOPEY; break;
            case 2: mutat = MUT_CLUMSY; break;
            case 3: mutat = MUT_DEFORMED; break;
            case 4: mutat = MUT_SCREAM; break;
            case 5: mutat = MUT_DETERIORATION; break;
            case 6: mutat = MUT_BLURRY_VISION; break;
            case 7: mutat = MUT_FRAIL; break;
            }
        }
    }
    while ( !accept_mutation(mutat) );

    return (mutat);
}

static mutation_type get_random_mutation(bool prefer_good,
                                         int preferred_multiplier)
{
    int cweight = 0;
    mutation_type chosen = NUM_MUTATIONS;
    for (int i = 0; i < NUM_MUTATIONS; ++i)
    {
        if (!mutation_defs[i].rarity)
            continue;

        const mutation_type curr = static_cast<mutation_type>(i);
        if (!accept_mutation(curr, true))
            continue;

        const bool weighted = mutation_defs[i].bad != prefer_good;
        int weight = mutation_defs[i].rarity;
        if (weighted)
            weight = weight * preferred_multiplier / 100;

        cweight += weight;

        if (random2(cweight) < weight)
            chosen = curr;
    }
    return (chosen);
}

bool mutate(mutation_type which_mutation, bool failMsg,
            bool force_mutation, bool god_gift,
            bool demonspawn)
{
    if (demonspawn)
        force_mutation = true;

    mutation_type mutat = which_mutation;

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
            case HS_SATIATED:
                rotting = !one_chance_in(3);
                break;
            case HS_FULL:
                rotting = coinflip();
                break;
            case HS_VERY_FULL:
                rotting = one_chance_in(3);
                break;
            case HS_ENGORGED:
                rotting = false;
            }
        }
    }

    // Undead bodies don't mutate, they fall apart. -- bwr
    // except for demonspawn (or other permamutations) in lichform -- haranp
    if (rotting && !demonspawn)
    {
        // Temporary resistance can be overridden by a god gift.
        if ((wearing_amulet(AMU_RESIST_MUTATION) ? !one_chance_in(10)
                                                 : one_chance_in(3))
            && !god_gift)
        {
            if (failMsg)
                mpr("You feel odd for a moment.", MSGCH_MUTATION);

            return (false);
        }
        else
        {
            mpr( "Your body decomposes!", MSGCH_MUTATION );

            if (coinflip())
                lose_stat( STAT_RANDOM, 1 , false, "mutating");
            else
            {
                ouch( 3, 0, KILLED_BY_ROTTING );
                rot_hp( roll_dice( 1, 3 ) );
            }

            xom_is_stimulated(64);
            return (true);
        }
    }

    if (!force_mutation)
    {
        // Temporary resistance can be overridden by a god gift.
        if (((wearing_amulet(AMU_RESIST_MUTATION) && !one_chance_in(10))
            || (you.religion == GOD_ZIN && you.piety > random2(MAX_PIETY))
                && !god_gift)
            || player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
            || player_mutation_level(MUT_MUTATION_RESISTANCE)
               && !one_chance_in(3))
        {
            if (failMsg)
                mpr("You feel odd for a moment.", MSGCH_MUTATION);

            return (false);
        }
    }

    if (which_mutation == RANDOM_MUTATION
        || which_mutation == RANDOM_XOM_MUTATION)
    {
        if ( random2(15) < how_mutated(false, true) )
        {
            if (!force_mutation && !one_chance_in(3))
                return (false);
            else
                return (delete_mutation(RANDOM_MUTATION));
        }
    }

    if (which_mutation == RANDOM_MUTATION)
    {
        do
        {
            mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));
            if (one_chance_in(1000))
                return false;
        }
        while ( !accept_mutation(mutat) );
    }
    else if (which_mutation == RANDOM_XOM_MUTATION)
    {
        if ((mutat = get_random_xom_mutation()) == NUM_MUTATIONS)
            return false;
    }
    else if (which_mutation == RANDOM_GOOD_MUTATION)
    {
        if ((mutat = get_random_mutation(true, 500)) == NUM_MUTATIONS)
            return false;
    }
    else if (you.mutation[mutat] >= 3
             && mutat != MUT_STRONG && mutat != MUT_CLEVER
             && mutat != MUT_AGILE  && mutat != MUT_WEAK
             && mutat != MUT_DOPEY  && mutat != MUT_CLUMSY)
    {
        return false;
    }

    // Saprovorous can't be randomly acquired
    if (mutat == MUT_SAPROVOROUS && !force_mutation)
        return false;

    if (you.mutation[mutat] > 13 && !force_mutation)
        return false;

    if ((mutat == MUT_TOUGH_SKIN || mutat == MUT_SHAGGY_FUR
            || mutat >= MUT_GREEN_SCALES && mutat <= MUT_BONEY_PLATES
            || mutat >= MUT_RED_SCALES && mutat <= MUT_PATTERNED_SCALES)
         && body_covered() >= 3 && !force_mutation)
    {
        return false;
    }

    if (you.species == SP_NAGA)
    {
        // gdl: spit poison 'upgrades' to breathe poison.  Why not..
        if (mutat == MUT_SPIT_POISON)
        {
            if (coinflip())
                return false;
            {
                mutat = MUT_BREATHE_POISON;

                // breathe poison replaces spit poison (so it takes the slot)
                for (int i = 0; i < 52; i++)
                    if (you.ability_letter_table[i] == ABIL_SPIT_POISON)
                        you.ability_letter_table[i] = ABIL_BREATHE_POISON;
            }
        }
    }

    // This one can be forced by demonspawn or a god gift
    if (mutat == MUT_REGENERATION
        && you.mutation[MUT_SLOW_METABOLISM] > 0 && !god_gift
        && !force_mutation)
    {
        return false;           // if you have a slow metabolism, no regen
    }

    if (mutat == MUT_SLOW_METABOLISM && you.mutation[MUT_REGENERATION] > 0)
        return false;           // if you have regen, no slow metabolism

    // This one can be forced by demonspawn or a god gift
    if (mutat == MUT_ACUTE_VISION
        && you.mutation[MUT_BLURRY_VISION] > 0 && !god_gift
        && !force_mutation)
    {
        return false;
    }

    if (mutat == MUT_BLURRY_VISION && you.mutation[MUT_ACUTE_VISION] > 0)
        return false;           // blurred vision/see invis

    // only Nagas and Draconians can get this one
    if (mutat == MUT_STINGER
        && !(you.species == SP_NAGA || player_genus(GENPC_DRACONIAN)))
    {
        return false;
    }

    // putting boots on after they are forced off. -- bwr
    if ((mutat == MUT_HOOVES || mutat == MUT_TALONS)
         && !player_has_feet())
    {
        return false;
    }

    // no fangs sprouting from Kenkus' beaks
    if (mutat == MUT_FANGS && you.species == SP_KENKU)
        return false;

    // already innate
    if (mutat == MUT_BREATHE_POISON && you.species != SP_NAGA)
        return false;

    // only Draconians can get wings
    if (mutat == MUT_BIG_WINGS && !player_genus(GENPC_DRACONIAN))
        return false;

    // Vampires' thirst rate depends on their blood level.
    if (you.species == SP_VAMPIRE
        && (mutat == MUT_SLOW_METABOLISM || mutat == MUT_FAST_METABOLISM
            || mutat == MUT_CARNIVOROUS || mutat == MUT_HERBIVOROUS))
    {
        return false;
    }

    if (you.mutation[mutat] >= mutation_defs[mutat].levels)
        return false;

    // find where these things are actually changed
    // -- do not globally force redraw {dlb}
    you.redraw_hit_points   = 1;
    you.redraw_magic_points = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion      = 1;
//    you.redraw_experience   = 1;
//    you.redraw_gold         = 1;
//    you.redraw_hunger       = 1;

    switch (mutat)
    {
    case MUT_STRONG:
        if (you.mutation[MUT_WEAK] > 0)
        {
            delete_mutation(MUT_WEAK);
            return true;
        }
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_STRENGTH, 1, false, "gaining a mutation");
        break;

    case MUT_CLEVER:
        if (you.mutation[MUT_DOPEY] > 0)
        {
            delete_mutation(MUT_DOPEY);
            return true;
        }
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_INTELLIGENCE, 1, false, "gaining a mutation");
        break;

    case MUT_AGILE:
        if (you.mutation[MUT_CLUMSY] > 0)
        {
            delete_mutation(MUT_CLUMSY);
            return true;
        }
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_DEXTERITY, 1, false, "gaining a mutation");
        break;

    case MUT_WEAK:
        if (you.mutation[MUT_STRONG] > 0)
        {
            delete_mutation(MUT_STRONG);
            return true;
        }
        modify_stat(STAT_STRENGTH, -1, true, "gaining a mutation");
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_DOPEY:
        if (you.mutation[MUT_CLEVER] > 0)
        {
            delete_mutation(MUT_CLEVER);
            return true;
        }
        modify_stat(STAT_INTELLIGENCE, -1, true, "gaining a mutation");
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_CLUMSY:
        if (you.mutation[MUT_AGILE] > 0)
        {
            delete_mutation(MUT_AGILE);
            return true;
        }
        modify_stat(STAT_DEXTERITY, -1, true, "gaining a mutation");
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_REGENERATION:
        if (you.mutation[MUT_SLOW_METABOLISM] > 0)
        {
            // Should only get here from demonspawn or a god gift, where
            // our innate ability will clear away the counter-mutation.
            while (delete_mutation(MUT_SLOW_METABOLISM))
                ;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_ACUTE_VISION:
        if (you.mutation[MUT_BLURRY_VISION] > 0)
        {
            // Should only get here from demonspawn or a god gift, where
            // our innate ability will clear away the counter-mutation.
            while (delete_mutation(MUT_BLURRY_VISION))
                ;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_CARNIVOROUS:
        if (you.mutation[MUT_HERBIVOROUS] > 0)
        {
            delete_mutation(MUT_HERBIVOROUS);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_HERBIVOROUS:
        if (you.mutation[MUT_CARNIVOROUS] > 0)
        {
            delete_mutation(MUT_CARNIVOROUS);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_FAST_METABOLISM:
        if (you.mutation[MUT_SLOW_METABOLISM] > 0)
        {
            delete_mutation(MUT_SLOW_METABOLISM);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_SLOW_METABOLISM:
        if (you.mutation[MUT_FAST_METABOLISM] > 0)
        {
            delete_mutation(MUT_FAST_METABOLISM);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    //jmf: like horns
    case MUT_HOOVES:
    case MUT_TALONS:
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        remove_one_equip(EQ_BOOTS);
        break;

    case MUT_CLAWS:
        mpr((you.species == SP_TROLL ? troll_claw_gain
             : gain_mutation[mutat])[you.mutation[mutat]],
             MSGCH_MUTATION);

        // gloves aren't prevented until level 3; we don't have the
        // mutation yet, so we have to check for level 2 or higher claws
        // here
        if (you.mutation[mutat] >= 2)
            remove_one_equip(EQ_GLOVES);

        break;

    case MUT_HORNS:
    {
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);

        // horns force hard helmets off
        if (you.equip[EQ_HELMET] != -1
            && !is_hard_helmet( you.inv[you.equip[EQ_HELMET]] ))
        {
            break;
        }
        remove_one_equip(EQ_HELMET);
        break;
    }

    case MUT_STRONG_STIFF:
        if (you.mutation[MUT_FLEXIBLE_WEAK] > 0)
        {
            delete_mutation(MUT_FLEXIBLE_WEAK);
            return true;
        }
        modify_stat(STAT_STRENGTH,   1, true, "gaining a mutation");
        modify_stat(STAT_DEXTERITY, -1, true, "gaining a mutation");
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_FLEXIBLE_WEAK:
        if (you.mutation[MUT_STRONG_STIFF] > 0)
        {
            delete_mutation(MUT_STRONG_STIFF);
            return true;
        }
        modify_stat(STAT_STRENGTH, -1, true, "gaining a mutation");
        modify_stat(STAT_DEXTERITY, 1, true, "gaining a mutation");
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_FRAIL:
        if (you.mutation[MUT_ROBUST] > 0)
        {
            delete_mutation(MUT_ROBUST);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        you.mutation[mutat]++;
        calc_hp();
        // special-case check
        take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_ROBUST:
        if (you.mutation[MUT_FRAIL] > 0)
        {
            delete_mutation(MUT_FRAIL);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        you.mutation[mutat]++;
        calc_hp();
        // special-case check
        take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_LOW_MAGIC:
        if (you.mutation[MUT_HIGH_MAGIC] > 0)
        {
            delete_mutation(MUT_HIGH_MAGIC);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        you.mutation[mutat]++;
        calc_mp();
        // special-case check
        take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_HIGH_MAGIC:
        if (you.mutation[MUT_LOW_MAGIC] > 0)
        {
            delete_mutation(MUT_LOW_MAGIC);
            return true;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        you.mutation[mutat]++;
        calc_mp();
        // special-case check
        take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_BLACK_SCALES:
    case MUT_BONEY_PLATES:
        modify_stat(STAT_DEXTERITY, -1, true, "gaining a mutation");
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_GREY2_SCALES:
        if (you.mutation[mutat] != 1)
            modify_stat(STAT_DEXTERITY, -1, true, "gaining a mutation");

        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_METALLIC_SCALES:
        if (you.mutation[mutat] == 0)
            modify_stat(STAT_DEXTERITY, -2, true, "gaining a mutation");
        else
            modify_stat(STAT_DEXTERITY, -1, true, "gaining a mutation");

        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_RED2_SCALES:
    case MUT_YELLOW_SCALES:
        if (you.mutation[mutat] != 0)
            modify_stat(STAT_DEXTERITY, -1, true, "gaining a mutation");

        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    default:
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;
    }

    you.mutation[mutat]++;

    // amusement value will be 16 * (11-rarity) * Xom's-sense-of-humor
    int amusementvalue = calc_mutation_amusement_value(mutat);
    xom_is_stimulated(amusementvalue);

    take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
    // remember, some mutations don't get this far (e.g. frail)
    return true;
}

int how_mutated(bool all, bool levels)
{
    int j = 0;

    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i])
        {
            if (!all && you.demon_pow[i] < you.mutation[i])
                continue;

            if (levels)
            {
                // these allow for 14 levels:
                if (i == MUT_STRONG  || i == MUT_CLEVER || i == MUT_AGILE
                    || i == MUT_WEAK || i == MUT_DOPEY  || i == MUT_CLUMSY)
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
}                               // end how_mutated()

bool delete_mutation(mutation_type which_mutation,
                     bool force_mutation)
{
    mutation_type mutat = which_mutation;

    if (!force_mutation)
    {
        if (player_mutation_level(MUT_MUTATION_RESISTANCE) > 1
            && (player_mutation_level(MUT_MUTATION_RESISTANCE) == 3
                || coinflip()))
        {
            mpr("You feel rather odd for a moment.", MSGCH_MUTATION);
            return false;
        }
    }

    if (which_mutation == RANDOM_MUTATION
        || which_mutation == RANDOM_XOM_MUTATION
        || which_mutation == RANDOM_GOOD_MUTATION)
    {
        do
        {
            mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));
            if (one_chance_in(1000))
                return false;
        }
        while ((you.mutation[mutat] == 0
                && (mutat != MUT_STRONG && mutat != MUT_CLEVER
                    && mutat != MUT_AGILE)
                && (mutat != MUT_WEAK && mutat != MUT_DOPEY
                    && mutat != MUT_CLUMSY))
               || random2(10) >= mutation_defs[mutat].rarity
               || you.demon_pow[mutat] >= you.mutation[mutat]
               || which_mutation == RANDOM_GOOD_MUTATION
                   && (mutation_defs[mutat].bad || one_chance_in(10)));
    }

    if (you.mutation[mutat] == 0)
        return false;

    if (you.demon_pow[mutat] >= you.mutation[mutat])
        return false;

    switch (mutat)
    {
    case MUT_STRONG:
        modify_stat(STAT_STRENGTH, -1, true, "losing a mutation");
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_CLEVER:
        modify_stat(STAT_INTELLIGENCE, -1, true, "losing a mutation");
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_AGILE:
        modify_stat(STAT_DEXTERITY, -1, true, "losing a mutation");
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_WEAK:
        modify_stat(STAT_STRENGTH, 1, false, "losing a mutation");
        break;

    case MUT_DOPEY:
        modify_stat(STAT_INTELLIGENCE, 1, false, "losing a mutation");
        break;

    case MUT_CLUMSY:
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_DEXTERITY, 1, false, "losing a mutation");
        break;

    case MUT_STRONG_STIFF:
        modify_stat(STAT_STRENGTH, -1, true, "losing a mutation");
        modify_stat(STAT_DEXTERITY, 1, true, "losing a mutation");
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_FLEXIBLE_WEAK:
        modify_stat(STAT_STRENGTH, 1, true, "losing a mutation");
        modify_stat(STAT_DEXTERITY, -1, true, "losing a mutation");
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_FRAIL:
    case MUT_ROBUST:
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        if (you.mutation[mutat] > 0)
            you.mutation[mutat]--;
        calc_hp();
        // special-case check
        take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_LOW_MAGIC:
    case MUT_HIGH_MAGIC:
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        if (you.mutation[mutat] > 0)
            you.mutation[mutat]--;
        calc_mp();
        // special-case check
        take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_BLACK_SCALES:
    case MUT_BONEY_PLATES:
        modify_stat(STAT_DEXTERITY, 1, true, "losing a mutation");
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_CLAWS:
        mpr((you.species == SP_TROLL ? troll_claw_lose
             : lose_mutation[mutat])[you.mutation[mutat] - 1],
             MSGCH_MUTATION);
        break;

    case MUT_GREY2_SCALES:
        if (you.mutation[mutat] != 2)
            modify_stat(STAT_DEXTERITY, 1, true, "losing a mutation");
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_METALLIC_SCALES:
        if (you.mutation[mutat] == 1)
            modify_stat(STAT_DEXTERITY, 2, true, "losing a mutation");
        else
            modify_stat(STAT_DEXTERITY, 1, true, "losing a mutation");
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_RED2_SCALES:
    case MUT_YELLOW_SCALES:
        if (you.mutation[mutat] != 1)
            modify_stat(STAT_DEXTERITY, 1, true, "losing a mutation");
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_BREATHE_POISON:
        // can't be removed yet, but still covered:
        if (you.species == SP_NAGA)
        {
            // natural ability to spit poison retakes the slot
            for (int i = 0; i < 52; i++)
            {
                if (you.ability_letter_table[i] == ABIL_BREATHE_POISON)
                    you.ability_letter_table[i] = ABIL_SPIT_POISON;
            }
        }
        break;

    default:
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;
    }

    // find where these things are actually altered
    // -- do not globally force redraw {dlb}
    you.redraw_hit_points   = 1;
    you.redraw_magic_points = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion      = 1;
//    you.redraw_experience   = 1;
//    you.redraw_gold         = 1;
//    you.redraw_hunger       = 1;

    if (you.mutation[mutat] > 0)
        you.mutation[mutat]--;

    take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat]));
    return true;
}                               // end delete_mutation()

static int body_covered(void)
{
    // checks how much of your body is covered by scales, etc.
    int covered = 0;

    if (you.species == SP_NAGA)
        covered++;

    if (player_genus(GENPC_DRACONIAN))
        covered += 3;

    covered += you.mutation[MUT_TOUGH_SKIN];
    covered += you.mutation[MUT_GREEN_SCALES];
    covered += you.mutation[MUT_BLACK_SCALES];
    covered += you.mutation[MUT_GREY_SCALES];
    covered += you.mutation[MUT_BONEY_PLATES];
    covered += you.mutation[MUT_SHAGGY_FUR];
    covered += you.mutation[MUT_RED_SCALES];
    covered += you.mutation[MUT_NACREOUS_SCALES];
    covered += you.mutation[MUT_GREY2_SCALES];
    covered += you.mutation[MUT_METALLIC_SCALES];
    covered += you.mutation[MUT_BLACK2_SCALES];
    covered += you.mutation[MUT_WHITE_SCALES];
    covered += you.mutation[MUT_YELLOW_SCALES];
    covered += you.mutation[MUT_BROWN_SCALES];
    covered += you.mutation[MUT_BLUE_SCALES];
    covered += you.mutation[MUT_PURPLE_SCALES];
    covered += you.mutation[MUT_SPECKLED_SCALES];
    covered += you.mutation[MUT_ORANGE_SCALES];
    covered += you.mutation[MUT_INDIGO_SCALES];
    covered += you.mutation[MUT_RED2_SCALES];
    covered += you.mutation[MUT_IRIDESCENT_SCALES];
    covered += you.mutation[MUT_PATTERNED_SCALES];

    return covered;
}

const char *mutation_name(mutation_type which_mutat, int level)
{
    static char mut_string[INFO_SIZE];

    // level == -1 means default action of current level
    if (level == -1)
    {
        if (!_mutation_is_fully_inactive(which_mutat))
            level = player_mutation_level(which_mutat);
        else // give description of fully active mutation
            level = you.mutation[ which_mutat ];
    }

    if (which_mutat == MUT_STRONG || which_mutat == MUT_CLEVER
        || which_mutat == MUT_AGILE || which_mutat == MUT_WEAK
        || which_mutat == MUT_DOPEY || which_mutat == MUT_CLUMSY)
    {
        snprintf( mut_string, sizeof( mut_string ), "%s%d).",
                  mutation_descrip[ which_mutat ][0], level );

        return (mut_string);
    }

    return (mutation_descrip[ which_mutat ][ level - 1 ]);
}                               // end mutation_name()

// Use an attribute counter for how many demonic mutations a dspawn has
void demonspawn(void)
{
    mutation_type whichm = NUM_MUTATIONS;
    char howm = 1;
    int counter = 0;

    const int covered = body_covered();

    you.attribute[ATTR_NUM_DEMONIC_POWERS]++;

    mpr("Your demonic ancestry asserts itself...", MSGCH_INTRINSIC_GAIN);

    // Merged the demonspawn lists into a single loop.  Now a high level
    // character can potentially get mutations from the low level list if
    // its having trouble with the high level list.
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
                howm = 3;
            }

            if (!you.mutation[MUT_CALL_TORMENT] && one_chance_in(15))
            {
                // Must keep this at 1. OK since it never happens randomly.
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

            if (you.religion != GOD_VEHUMET && you.religion != GOD_MAKHLEB &&
                one_chance_in(11))
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
            if (you.religion != GOD_YREDELEMNUL &&
                you.skills[SK_SUMMONINGS] < 3 &&
                you.skills[SK_NECROMANCY] < 3 && one_chance_in(10))
            {
                whichm = MUT_RAISE_DEAD;
                howm = 1;
            }

            if (you.skills[SK_UNARMED_COMBAT] > 5 && one_chance_in(14))
            {
                whichm = MUT_DRAIN_LIFE;
                howm = 1;
            }
        }

        // check here so we can see if we need to extend our options:
        if (whichm != NUM_MUTATIONS && you.mutation[whichm] != 0)
            whichm = NUM_MUTATIONS;

        if (you.experience_level < 10 ||
            (counter > 0 && whichm == NUM_MUTATIONS))
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
                // try to give the flavour the character doesn't have:

                // neither
                if (!you.skills[SK_FIRE_MAGIC] && !you.skills[SK_ICE_MAGIC])
                    whichm = (coinflip() ? MUT_THROW_FLAMES : MUT_THROW_FROST);
                else if (!you.skills[SK_FIRE_MAGIC])
                    whichm = MUT_THROW_FLAMES;
                else if (!you.skills[SK_ICE_MAGIC])
                    whichm = MUT_THROW_FROST;
                else            // both
                    whichm = (coinflip() ? MUT_THROW_FLAMES : MUT_THROW_FROST);

                howm = 1;
            }

            // summoners and Makhlebites don't get summon imp
            if (!you.skills[SK_SUMMONINGS] && you.religion != GOD_MAKHLEB &&
                one_chance_in(3))
            {
                whichm = (you.experience_level < 10) ? MUT_SUMMON_MINOR_DEMONS
                                                     : MUT_SUMMON_DEMONS;
                howm = 1;
            }

            if (one_chance_in(4))
            {
                whichm = MUT_POISON_RESISTANCE;
                howm = 3;
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
        // unlikely but remotely possible; I know this is a cop-out
        modify_stat(STAT_STRENGTH, 1, true, "demonspawn mutation");
        modify_stat(STAT_INTELLIGENCE, 1, true, "demonspawn mutation");
        modify_stat(STAT_DEXTERITY, 1, true, "demonspawn mutation");
        mpr("You feel much better now.", MSGCH_INTRINSIC_GAIN);
    }
}                               // end demonspawn()

bool perma_mutate(mutation_type which_mut, int how_much)
{
    int levels = 0;

    how_much = std::min(static_cast<short>(how_much),
                        mutation_defs[which_mut].levels);

    if (mutate(which_mut, false, true, false, true))
        levels++;

    if (how_much >= 2 && mutate(which_mut, false, true, false, true))
        levels++;

    if (how_much >= 3 && mutate(which_mut, false, true, false, true))
        levels++;

    you.demon_pow[which_mut] = levels;

    return (levels > 0);
}                               // end perma_mutate()

bool give_bad_mutation(bool failMsg, bool force_mutation)
{
    mutation_type mutat;

    switch (random2(12))
    {
    case 0: mutat = MUT_CARNIVOROUS; break;
    case 1: mutat = MUT_HERBIVOROUS; break;
    case 2: mutat = MUT_FAST; break;
    case 3: mutat = MUT_WEAK; break;
    case 4: mutat = MUT_DOPEY; break;
    case 5: mutat = MUT_CLUMSY; break;
    case 6: mutat = MUT_TELEPORT; break;
    case 7: mutat = MUT_DEFORMED; break;
    case 8: mutat = MUT_SCREAM; break;
    case 9: mutat = MUT_DETERIORATION; break;
    case 10: mutat = MUT_BLURRY_VISION; break;
    case 11: mutat = MUT_FRAIL; break;
    }

    const bool result = mutate(mutat, failMsg, force_mutation);
    if (result)
        learned_something_new(TUT_YOU_MUTATED);

    return (result);
}                               // end give_bad_mutation()
