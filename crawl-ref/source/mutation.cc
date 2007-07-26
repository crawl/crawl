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

#ifdef UNIX
#include "libunix.h"
#endif

#include "externs.h"

#include "defines.h"
#include "effects.h"
#include "itemprop.h"
#include "macro.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "skills2.h"
#include "stuff.h"
#include "transfor.h"
#include "view.h"


int how_mutated(void);
char body_covered(void);

const char* troll_claw_messages[3] = {
    "Your claws sharpen.",
    "Your claws sharpen.",
    "Your claws steel!"
};

const char* naga_speed_descrip[4] = {
    "You cover the ground very slowly.",    // 10*14/10 = 14
    "You cover the ground rather slowly.",  //  8*14/10 = 11
    "You cover the ground rather quickly.", //  7*14/10 = 9
    "You cover the ground quickly.",        //  6*14/10 = 8
};

const char* troll_claw_descrip[4] = {
    "You have claws for hands.",
    "You have sharp claws for hands.",
    "You have very sharp claws for hands.",
    "Your claws are sharper than steel."
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
     "Your digestive system is specialised to digest meat.",
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

    {"You cover the ground quickly.", "You cover the ground very quickly.",
     "You cover the ground extremely quickly."},

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

    {"You occasionally forget where you are.",
     "You sometimes forget where you are.",
     "You frequently forget where you are."},

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

    {"You are frail (-10 percent hp).", "You are very frail (-20 percent hp).",
     "You are extremely frail (-30 percent hp)."},

    {"You are robust (+10 percent hp).",
     "You are very robust (+20 percent hp).",
     "You are extremely robust (+30 percent hp)."},

    {"You are immune to unholy pain and torment.", "", ""},

    {"You resist negative energy.",
     "You are quite resistant to negative energy.",
     "You are immune to negative energy."},

    /* Use player_has_spell() to avoid duplication */
    {"You can summon minor demons to your aid.", "", ""},
    {"You can summon demons to your aid.", "", ""},
    {"You can hurl blasts of hellfire.", "", ""},
    {"You can call on the torments of Hell.", "", ""},
    {"You can raise the dead to walk for you.", "", ""},
// 50
    {"You can control demons.", "", ""},
    {"You can travel to (but not from) Pandemonium at will.", "", ""},
    {"You can draw strength from death and destruction.", "", ""},

    /* Not worshippers of Vehumet */
    {"You can channel magical energy from Hell.", "", ""},

    {"You can drain life in unarmed combat.", "", ""},

    /* Not conjurers/worshippers of Makhleb */
    {"You can throw forth the flames of Gehenna.", "", ""},

    {"You can throw forth the frost of Cocytus.", "", ""},

    {"You can invoke the powers of Tartarus to smite your living foes.", "", ""},
    {"You have sharp fingernails.", "Your fingernails are very sharp.",
     "You have claws for hands."},

    {"You have hooves in place of feet.", "", ""},
    // 60 - leave some space for more demonic powers...
    
    {"You have very sharp teeth.", "You have extremely sharp teeth.",
     "You have razor-sharp teeth."},
     
    {"You can exhale a cloud of poison.", "", ""},

    {"Your tail ends in a poisonous barb.",
     "Your tail ends in a sharp poisonous barb.",
     "Your tail ends in a wicked poisonous barb."}, //jmf: nagas & dracos

    {"Your wings are large and strong.", "", ""},       //jmf: dracos only

    //jmf: these next two are for evil gods to mark their followers; good gods
    //     will never accept a 'marked' worhsipper

    {"There is a blue sigil on each of your hands.",
     "There are several blue sigils on your hands and arms.",
     "Your hands, arms and shoulders are covered in intricate, arcane blue writing."},

    {"There is a green sigil on your chest.",
     "There are several green sigils on your chest and abdomen.",
     "Your chest, abdomen and neck are covered in intricate, arcane green writing."},

    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    // 70

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

// 80

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
     "You are completely covered in patterned scales (AC + 3)."},
};

/*
   If giving a mutation which must succeed (eg demonspawn), must add exception
   to the "resist mutation" mutation thing.
 */

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

    {"You feel less intelligent.", "You feel less intelligent",
     "You feel less intelligent"},
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

    {"You feel a little disoriented.", "You feel a little disoriented.",
     "Where the Hells are you?"},

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

    {"Your feet shrivel into cloven hooves.", "", ""},
   // 60

    {"Your teeth lengthen and sharpen.",
     "Your teeth lengthen and sharpen some more.",
     "Your teeth are very long and razor-sharp."},

    {"You taste something nasty.", "You taste something very nasty.",
     "You taste something extremely nasty."},

    {"A poisonous barb forms on the end of your tail.",
     "The barb on your tail looks sharper.",
     "The barb on your tail looks very sharp."},

    {"Your wings grow larger and stronger.", "", ""},

    {"Your hands itch.", "Your hands and forearms itch.",
     "Your arms, hands and shoulders itch."},

    {"Your chest itches.", "Your chest and abdomen itch.",
     "Your chest, abdomen and neck itch."},

    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    // 70

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
    // 80

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
     "Patterned scales cover you completely."},
};

const char *lose_mutation[][3] = {

    {"Your skin feels delicate.", "Your skin feels delicate.",
     "Your skin feels delicate."},

    {"You feel weaker.", "You feel weaker.", "You feel weaker."},

    {"You feel less intelligent.", "You feel less intelligent",
     "You feel less intelligent"},

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

    {"You feel less disoriented.", "You feel less disoriented.",
     "You feel less disoriented."},

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

/* Some demonic powers (which can't be lost) start here... */
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

    {"Your hooves expand and flesh out into feet!", "", ""},
    // 60
    {"Your teeth shrink and become duller.", "", ""},
    
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
    {"", "", ""},
// 70

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
// 80

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
     "Your patterned scales recede somewhat."},
};

/*
   Chance out of 10 that mutation will be given/removed randomly. 0 means never.
 */
const char mutation_rarity[] = {
    10,                         // tough skin
    8,                          // str
    8,                          // int
    8,                          // dex
    2,                          // gr scales
    1,                          // bl scales
    2,                          // grey scales
    1,                          // bone
    1,                          // repuls field
    4,                          // res poison
// 10
    5,                          // carn
    5,                          // herb
    4,                          // res fire
    4,                          // res cold
    2,                          // res elec
    3,                          // regen
    10,                         // fast meta
    7,                          // slow meta
    10,                         // abil loss
    10,                         // ""
// 20
    10,                         // ""
    2,                          // tele control
    3,                          // teleport
    5,                          // res magic
    1,                          // run
    2,                          // see invis
    8,                          // deformation
    2,                          // teleport at will
    8,                          // spit poison
    3,                          // sense surr
// 30
    4,                          // breathe fire
    3,                          // blink
    7,                          // horns
    10,                         // strong/stiff muscles
    10,                         // weak/loose muscles
    6,                          // forgetfulness
    6,                          // clarity (as the amulet)
    7,                          // berserk/temper
    10,                         // deterioration
    10,                         // blurred vision
// 40
    4,                          // resist mutation
    10,                         // frail
    5,                          // robust
/* Some demonic powers start here: */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
// 50
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,                          //jmf: claws
    1,                          //jmf: hooves
// 60
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
// 70
    2,                          // red scales
    1,                          // nac scales
    2,                          // r-grey scales
    1,                          // metal scales
    2,                          // black scales
    2,                          // wh scales
    2,                          // yel scales
    2,                          // brown scales
    2,                          // blue scales
    2,                          // purple scales
// 80
    2,                          // speckled scales
    2,                          // orange scales
    2,                          // indigo scales
    1,                          // kn red scales
    1,                          // irid scales
    1,                          // pattern scales
    0,                          //
    0,                          //
    0,                          //
    0                           //
};

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
        // breathe poison replaces spit poison:
        if (!you.mutation[MUT_BREATHE_POISON])
            result += "You can spit poison." EOL;
        else
            result += "<cyan>You can exhale a cloud of poison.</cyan>" EOL "<lightblue>";

        // slowness can be overriden
        if ( you.mutation[MUT_FAST] )
            result += "<cyan>";
        result += naga_speed_descrip[you.mutation[MUT_FAST]];
        if ( you.mutation[MUT_FAST] )
            result += "</cyan>";
        result += EOL;
        have_any = true;
        break;

    case SP_TROLL:
        if ( you.mutation[MUT_CLAWS] )
            result += "<cyan>";
        result += troll_claw_descrip[you.mutation[MUT_CLAWS]];
        if ( you.mutation[MUT_CLAWS] )
            result += "</cyan><lightblue>";
        result += EOL;
        result += "You can eat rotten meat." EOL;
        have_any = true;
        break;

    case SP_GHOUL:
        result += "Your body is rotting away." EOL;
        have_any = true;
        break;

    case SP_GREY_ELF:
        if (you.experience_level > 4)
        {
            result += "You are very charming." EOL;
            have_any = true;
        }
        break;

    case SP_HIGH_ELF:
        if (you.experience_level > 14)
        {
            result += "You are very charming." EOL;
            have_any = true;
        }
        break;

    case SP_KENKU:
        result += "You cannot wear boots or helmets." EOL;
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
        result += "You can eat rotten meat." EOL;
        have_any = true;
        break;

    case SP_HILL_ORC:
    case SP_OGRE:
        result += "You can tolerate rotten meat." EOL;
        have_any = true;
        break;

    case SP_VAMPIRE:
        result += "You are";
        result += (you.experience_level > 25 && you.hunger_state == HS_FULL) ?
                 " very strongly" :
                  (you.experience_level > 12 && you.hunger_state != HS_HUNGRY) ?
                 " strongly" : "";
        result += " in touch with the powers of death." EOL;
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

    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i] != 0)
        {
            have_any = true;

            // this is already handled above:
            if (you.species == SP_NAGA &&
                (i == MUT_BREATHE_POISON || i == MUT_FAST))
                continue;
            if (you.species == SP_TROLL && i == MUT_CLAWS)
                continue;

            const char* colourname = "";
            // mutation is actually a demonic power
            if ( you.demon_pow[i] )
            {
                if ( you.species == SP_DEMONSPAWN )
                {
                    if ( you.demon_pow[i] < you.mutation[i] )
                        colourname = "lightred";
                    else
                        colourname = "red";
                }
                else            // innate ability
                {
                    if ( you.demon_pow[i] < you.mutation[i] )
                        colourname = "cyan";
                    else
                        colourname = "lightblue";
                }
            }

            if ( you.demon_pow[i] )
            {
                result += '<';
                result += colourname;
                result += '>';
            }
            result += mutation_name(static_cast<mutation_type>(i));
            if ( you.demon_pow[i] )
            {
                result += "</";
                result += colourname;
                result += '>';
            }
            result += EOL;
        }
    }

    if (!have_any)
        result +=  "You are rather mundane." EOL;

    return formatted_string::parse_string(result);
}

void display_mutations()
{
    clrscr();
    gotoxy(1,1);
    describe_mutations().display();
    
    if (getch() == 0)
        getch();
}

static int calc_mutation_amusement_value(mutation_type which_mutation)
{
    int amusement = 16 * (11 - mutation_rarity[which_mutation]);

    switch (which_mutation)
    {
    case MUT_TOUGH_SKIN:
    case MUT_STRONG:
    case MUT_CLEVER:
    case MUT_AGILE:
    case MUT_POISON_RESISTANCE:
    case MUT_TELEPORT_CONTROL:
    case MUT_MAGIC_RESISTANCE:
    case MUT_TELEPORT_AT_WILL:
    case MUT_MAPPING:
    case MUT_CLARITY:
    case MUT_MUTATION_RESISTANCE:
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
    case MUT_BLINK:
    case MUT_HORNS:
    case MUT_LOST:
    case MUT_BERSERK:
    case MUT_DETERIORATION:
    case MUT_BLURRY_VISION:
    case MUT_FRAIL:
    case MUT_CLAWS:
    case MUT_HOOVES:
    case MUT_FANGS:
    case MUT_BREATHE_POISON:
    case MUT_STINGER:
    case MUT_BIG_WINGS:
    case MUT_BLUE_MARKS:
    case MUT_GREEN_MARKS:
        amusement *= 2; // funny!
        break;

    default:
        break;
    }

    return (amusement);
}

static bool accept_mutation( mutation_type mutat )
{
    int limit = 3;
    if ( mutat == MUT_STRONG || mutat == MUT_CLEVER ||
         mutat == MUT_AGILE || mutat == MUT_WEAK ||
         mutat == MUT_DOPEY || mutat == MUT_CLUMSY )
        limit = 14;
    
    if ( you.mutation[mutat] >= limit )
        return false;
    
    const int rarity = mutation_rarity[mutat] + you.demon_pow[mutat];
    // low rarity means unlikely to choose it
    return (rarity > random2(10));
}

bool mutate(mutation_type which_mutation, bool failMsg, bool force_mutation,
            bool demonspawn)
{
    mutation_type mutat = which_mutation;

    if (demonspawn)
        force_mutation = true;

    // Undead bodies don't mutate, they fall apart. -- bwr
    // except for demonspawn (or other permamutations) in lichform -- haranp
    if (you.is_undead && !demonspawn) 
    {
        if ((!wearing_amulet(AMU_RESIST_MUTATION) && coinflip())
                || one_chance_in(10))
        {
            mpr( "Your body decomposes!", MSGCH_MUTATION );

            if (coinflip())
                lose_stat( STAT_RANDOM, 1 );
            else
            {
                ouch( 3, 0, KILLED_BY_ROTTING );
                rot_hp( roll_dice( 1, 3 ) );
            }

            return (true);
        }

        if (failMsg)
            mpr("You feel odd for a moment.", MSGCH_MUTATION);

        return (false);
    }

    if (wearing_amulet(AMU_RESIST_MUTATION)
        && !force_mutation && !one_chance_in(10))
    {
        if (failMsg)
            mpr("You feel odd for a moment.", MSGCH_MUTATION);

        return (false);
    }

    if (you.mutation[MUT_MUTATION_RESISTANCE]
        && !force_mutation
        && (you.mutation[MUT_MUTATION_RESISTANCE] == 3 || !one_chance_in(3)))
    {
        if (failMsg)
            mpr("You feel odd for a moment.", MSGCH_MUTATION);

        return (false);
    }

    if (which_mutation == RANDOM_MUTATION)
    {
        if ( random2(15) < how_mutated() )
        {
            if (!force_mutation && !one_chance_in(3))
                return (false);
            else
                return (delete_mutation(RANDOM_MUTATION));
        }
        else
        {
            do
            {
                mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));
                if (one_chance_in(1000))
                    return false;
            } while ( !accept_mutation(mutat) );
        }
    }
    else if (which_mutation == RANDOM_XOM_MUTATION)
    {
        do
        {
            mutat = static_cast<mutation_type>(random2(NUM_MUTATIONS));

            if (one_chance_in(1000))
                return false;
            if (one_chance_in(5))
            {
                switch (random2(8))
                {
                case 0: mutat = MUT_WEAK; break;
                case 1: mutat = MUT_DOPEY; break;
                case 2: mutat = MUT_CLUMSY; break;
                case 3: mutat = MUT_DEFORMED; break;
                case 4: mutat = MUT_LOST; break;
                case 5: mutat = MUT_DETERIORATION; break;
                case 6: mutat = MUT_BLURRY_VISION; break;
                case 7: mutat = MUT_FRAIL; break;
                }
            }
        }
        while ( !accept_mutation(mutat) );
    }
    else if (you.mutation[mutat] >= 3 &&
             mutat != MUT_STRONG && mutat != MUT_CLEVER &&
             mutat != MUT_AGILE && mutat != MUT_WEAK &&
             mutat != MUT_DOPEY && mutat != MUT_CLUMSY)
    {
        return false;
    }

    if (you.mutation[mutat] > 13 && !force_mutation)
        return false;

    // These can be forced by demonspawn
    if ((mutat == MUT_TOUGH_SKIN
         || (mutat >= MUT_GREEN_SCALES && mutat <= MUT_BONEY_PLATES)
         || (mutat >= MUT_RED_SCALES && mutat <= MUT_PATTERNED_SCALES))
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
                {
                    if (you.ability_letter_table[i] == ABIL_SPIT_POISON)
                        you.ability_letter_table[i] = ABIL_BREATHE_POISON;
                }
            }
        }
    }

    // This one can be forced by demonspawn
    if (mutat == MUT_REGENERATION
        && you.mutation[MUT_SLOW_METABOLISM] > 0 && !force_mutation)
    {
        return false;           /* if you have a slow metabolism, no regen */
    }

    if (mutat == MUT_SLOW_METABOLISM && you.mutation[MUT_REGENERATION] > 0)
        return false;           /* if you have a slow metabolism, no regen */

    // This one can be forced by demonspawn
    if (mutat == MUT_ACUTE_VISION
        && you.mutation[MUT_BLURRY_VISION] > 0 && !force_mutation)
    {
        return false;
    }

    if (mutat == MUT_BLURRY_VISION && you.mutation[MUT_ACUTE_VISION] > 0)
        return false;           /* blurred vision/see invis */

    //jmf: added some checks for new mutations
    if (mutat == MUT_STINGER
        && !(you.species == SP_NAGA || player_genus(GENPC_DRACONIAN)))
    {
        return false;
    }

    // putting boots on after they are forced off. -- bwr
    if (mutat == MUT_HOOVES
        && (you.species == SP_NAGA || you.species == SP_CENTAUR
            || you.species == SP_KENKU || player_genus(GENPC_DRACONIAN)))
    {
        return false;
    }

    if (mutat == MUT_HERBIVOROUS && you.species == SP_VAMPIRE)
        return false;

    if (mutat == MUT_FANGS
        && (you.species == SP_VAMPIRE || you.species == SP_KENKU))
    {
        return false;
    }

    if (mutat == MUT_BIG_WINGS && !player_genus(GENPC_DRACONIAN))
        return false;

    // find where these things are actually changed
    // -- do not globally force redraw {dlb}
    you.redraw_hit_points = 1;
    you.redraw_magic_points = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
    you.redraw_experience = 1;
    you.redraw_gold = 1;
    //you.redraw_hunger = 1;

    switch (mutat)
    {
    case MUT_STRONG:
        if (you.mutation[MUT_WEAK] > 0)
        {
            delete_mutation(MUT_WEAK);
            return true;
        }
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_STRENGTH, 1, false);
        break;

    case MUT_CLEVER:
        if (you.mutation[MUT_DOPEY] > 0)
        {
            delete_mutation(MUT_DOPEY);
            return true;
        }
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_INTELLIGENCE, 1, false);
        break;

    case MUT_AGILE:
        if (you.mutation[MUT_CLUMSY] > 0)
        {
            delete_mutation(MUT_CLUMSY);
            return true;
        }
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_DEXTERITY, 1, false);
        break;

    case MUT_WEAK:
        if (you.mutation[MUT_STRONG] > 0)
        {
            delete_mutation(MUT_STRONG);
            return true;
        }
        modify_stat(STAT_STRENGTH, -1, true);
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_DOPEY:
        if (you.mutation[MUT_CLEVER] > 0)
        {
            delete_mutation(MUT_CLEVER);
            return true;
        }
        modify_stat(STAT_INTELLIGENCE, -1, true);
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_CLUMSY:
        if (you.mutation[MUT_AGILE] > 0)
        {
            delete_mutation(MUT_AGILE);
            return true;
        }
        modify_stat(STAT_DEXTERITY, -1, true);
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_REGENERATION:
        if (you.mutation[MUT_SLOW_METABOLISM] > 0)
        {
            // Should only get here from demonspawn, where our innate
            // ability will clear away the counter-mutation.
            while (delete_mutation(MUT_SLOW_METABOLISM))
                ;
        }
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_ACUTE_VISION:
        if (you.mutation[MUT_BLURRY_VISION] > 0)
        {
            // Should only get here from demonspawn, where our inate
            // ability will clear away the counter-mutation.
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

    case MUT_SHOCK_RESISTANCE:
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
        //if (you.mutation[mutat] == 0 || you.mutation[mutat] == 2)
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_TELEPORT_CONTROL:
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_HOOVES:            //jmf: like horns
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        remove_one_equip(EQ_BOOTS);
        break;

    case MUT_FANGS:
         mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
         break;
         
    case MUT_CLAWS:

        mpr((you.species == SP_TROLL ? troll_claw_messages
             : gain_mutation[mutat])[you.mutation[mutat]],
             MSGCH_MUTATION);

        // gloves aren't prevented until level three
        if (you.mutation[ mutat ] >= 3)
            remove_one_equip(EQ_GLOVES);
        break;

    case MUT_HORNS:             // horns force your helmet off
        {
            mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);

            if (you.equip[EQ_HELMET] != -1 &&
                (get_helmet_type(you.inv[you.equip[EQ_HELMET]]) == THELM_CAP ||
                 get_helmet_type(you.inv[you.equip[EQ_HELMET]]) == THELM_WIZARD_HAT))
            {
                break;          // horns don't push caps/wizard hats off
            }

            remove_one_equip(EQ_HELMET);
        }
        break;

    case MUT_STRONG_STIFF:
        if (you.mutation[MUT_FLEXIBLE_WEAK] > 0)
        {
            delete_mutation(MUT_FLEXIBLE_WEAK);
            return true;
        }
        modify_stat(STAT_STRENGTH, 1, true);
        modify_stat(STAT_DEXTERITY, -1, true);
        mpr(gain_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_FLEXIBLE_WEAK:
        if (you.mutation[MUT_STRONG_STIFF] > 0)
        {
            delete_mutation(MUT_STRONG_STIFF);
            return true;
        }
        modify_stat(STAT_STRENGTH, -1, true);
        modify_stat(STAT_DEXTERITY, 1, true);
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
        /* special-case check */
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
        /* special-case check */
        take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_BLACK_SCALES:
    case MUT_BONEY_PLATES:
        modify_stat(STAT_DEXTERITY, -1, true);
        // deliberate fall-through
    default:
        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_GREY2_SCALES:
        if (you.mutation[mutat] != 1)
            modify_stat(STAT_DEXTERITY, -1, true);

        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_METALLIC_SCALES:
        if (you.mutation[mutat] == 0)
            modify_stat(STAT_DEXTERITY, -2, true);
        else
            modify_stat(STAT_DEXTERITY, -1, true);

        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;

    case MUT_RED2_SCALES:
    case MUT_YELLOW_SCALES:
        if (you.mutation[mutat] != 0)
            modify_stat(STAT_DEXTERITY, -1, true);

        mpr(gain_mutation[mutat][you.mutation[mutat]], MSGCH_MUTATION);
        break;
    }

    you.mutation[mutat]++;

    /* amusement value will be 16 * (11-rarity) * Xom's-sense-of-humor */
    int amusementvalue = calc_mutation_amusement_value(mutat);
    xom_is_stimulated(amusementvalue);

    take_note(Note(NOTE_GET_MUTATION, mutat, you.mutation[mutat]));
    /* remember, some mutations don't get this far (eg frail) */
    return true;
}

int how_mutated(void)
{
    int j = 0;

    for (int i = 0; i < NUM_MUTATIONS; i++)
    {
        if (you.mutation[i] && you.demon_pow[i] < you.mutation[i])
        {
            // these allow for 14 levels:
            if (i == MUT_STRONG || i == MUT_CLEVER || i == MUT_AGILE
                || i == MUT_WEAK || i == MUT_DOPEY || i == MUT_CLUMSY)
            {
                j += (you.mutation[i] / 5 + 1);
            }
            else 
            {
                j += you.mutation[i];
            }
        }
    }

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "levels: %d", j );
#endif

    return (j);
}                               // end how_mutated()

bool delete_mutation(mutation_type which_mutation)
{
    mutation_type mutat = which_mutation;
    int i;

    if (you.mutation[MUT_MUTATION_RESISTANCE] > 1
        && (you.mutation[MUT_MUTATION_RESISTANCE] == 3 || coinflip()))
    {
        mpr("You feel rather odd for a moment.", MSGCH_MUTATION);
        return false;
    }

    if (which_mutation == RANDOM_MUTATION)
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
               || random2(10) >= mutation_rarity[mutat]
               || you.demon_pow[mutat] >= you.mutation[mutat]);
    }

    if (you.mutation[mutat] == 0)
        return false;

    if (you.demon_pow[mutat] >= you.mutation[mutat])
        return false;

    switch (mutat)
    {
    case MUT_STRONG:
        modify_stat(STAT_STRENGTH, -1, true);
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_CLEVER:
        modify_stat(STAT_INTELLIGENCE, -1, true);
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_AGILE:
        modify_stat(STAT_DEXTERITY, -1, true);
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_WEAK:
        modify_stat(STAT_STRENGTH, 1, false);
        break;

    case MUT_DOPEY:
        modify_stat(STAT_INTELLIGENCE, 1, false);
        break;

    case MUT_CLUMSY:
        // replaces earlier, redundant code - 12mar2000 {dlb}
        modify_stat(STAT_DEXTERITY, 1, false);
        break;

    case MUT_SHOCK_RESISTANCE:
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_FAST_METABOLISM:
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_SLOW_METABOLISM:
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_TELEPORT_CONTROL:
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_STRONG_STIFF:
        modify_stat(STAT_STRENGTH, -1, true);
        modify_stat(STAT_DEXTERITY, 1, true);
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_FLEXIBLE_WEAK:
        modify_stat(STAT_STRENGTH, 1, true);
        modify_stat(STAT_DEXTERITY, -1, true);
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        break;

    case MUT_FRAIL:
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        if (you.mutation[mutat] > 0)
            you.mutation[mutat]--;
        calc_hp();
        /* special-case check */
        take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_ROBUST:
        mpr(lose_mutation[mutat][0], MSGCH_MUTATION);
        if (you.mutation[mutat] > 0)
            you.mutation[mutat]--;
        calc_hp();
        /* special-case check */
        take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat]));
        return true;

    case MUT_BLACK_SCALES:
    case MUT_BONEY_PLATES:
        modify_stat(STAT_DEXTERITY, 1, true);

    default:
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_GREY2_SCALES:
        if (you.mutation[mutat] != 2)
            modify_stat(STAT_DEXTERITY, 1, true);
        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_METALLIC_SCALES:
        if (you.mutation[mutat] == 1)
            modify_stat(STAT_DEXTERITY, 2, true);
        else
            modify_stat(STAT_DEXTERITY, 1, true);

        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_RED2_SCALES:
    case MUT_YELLOW_SCALES:
        if (you.mutation[mutat] != 1)
            modify_stat(STAT_DEXTERITY, 1, true);

        mpr(lose_mutation[mutat][you.mutation[mutat] - 1], MSGCH_MUTATION);
        break;

    case MUT_BREATHE_POISON:
        // can't be removed yet, but still covered:
        if (you.species == SP_NAGA)
        {
            // natural ability to spit poison retakes the slot
            for (i = 0; i < 52; i++)
            {
                if (you.ability_letter_table[i] == ABIL_BREATHE_POISON)
                    you.ability_letter_table[i] = ABIL_SPIT_POISON;
            }
        }
        break;
    }

    // find where these things are actually altered
    /// -- do not globally force redraw {dlb}
    you.redraw_hit_points = 1;
    you.redraw_magic_points = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
    you.redraw_experience = 1;
    you.redraw_gold = 1;
    //you.redraw_hunger = 1;

    if (you.mutation[mutat] > 0)
        you.mutation[mutat]--;

    take_note(Note(NOTE_LOSE_MUTATION, mutat, you.mutation[mutat]));
    return true;
}                               // end delete_mutation()

char body_covered(void)
{
    /* checks how much of your body is covered by scales etc */
    char covered = 0;

    if (you.species == SP_NAGA)
        covered++;

    if (player_genus(GENPC_DRACONIAN))
        return 3;

    covered += you.mutation[MUT_TOUGH_SKIN];
    covered += you.mutation[MUT_GREEN_SCALES];
    covered += you.mutation[MUT_BLACK_SCALES];
    covered += you.mutation[MUT_GREY_SCALES];
    covered += you.mutation[MUT_BONEY_PLATES];
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
        level = you.mutation[ which_mutat ];

    if (which_mutat == MUT_STRONG || which_mutat == MUT_CLEVER
        || which_mutat == MUT_AGILE || which_mutat == MUT_WEAK
        || which_mutat == MUT_DOPEY || which_mutat == MUT_CLUMSY)
    {
        snprintf( mut_string, sizeof( mut_string ), "%s%d).", 
                  mutation_descrip[ which_mutat ][0], level );

        return (mut_string);
    }

    // Some mutations only have one "level", and it's better
    // to show the first level description than a blank description.
    if (mutation_descrip[ which_mutat ][ level - 1 ][0] == 0)
        return (mutation_descrip[ which_mutat ][ 0 ]);
    else 
        return (mutation_descrip[ which_mutat ][ level - 1 ]);
}                               // end mutation_name()

/* Use an attribute counter for how many demonic mutations a dspawn has */
void demonspawn(void)
{
    mutation_type whichm = NUM_MUTATIONS;
    char howm = 1;
    int counter = 0;

    const int scale_levels = body_covered();

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
            if ((you.religion != GOD_MAKHLEB || you.piety < 100) &&
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

            if (you.skills[SK_SUMMONINGS] < 3 && you.skills[SK_NECROMANCY] < 3
                && one_chance_in(10))
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

            // summoners don't get summon imp
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

            if (scale_levels < 3 && one_chance_in( 1 + scale_levels * 5 ))
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
                    howm = MINIMUM( 3 - scale_levels, levels + bonus );
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
        /* unlikely but remotely possible */
        /* I know this is a cop-out */
        modify_stat(STAT_STRENGTH, 1, true);
        modify_stat(STAT_INTELLIGENCE, 1, true);
        modify_stat(STAT_DEXTERITY, 1, true);
        mpr("You feel much better now.", MSGCH_INTRINSIC_GAIN);
    }
}                               // end demonspawn()

bool perma_mutate(mutation_type which_mut, int how_much)
{
    char levels = 0;

    if (mutate(which_mut, false, true, true))
        levels++;

    if (how_much >= 2 && mutate(which_mut, false, true, true))
        levels++;

    if (how_much >= 3 && mutate(which_mut, false, true, true))
        levels++;

    you.demon_pow[which_mut] = levels;

    return (levels > 0);
}                               // end perma_mutate()

bool give_bad_mutation(bool forceMutation, bool failMsg)
{
    mutation_type which_bad_one;

    const int temp_rand = random2(12);

    which_bad_one = ((temp_rand >= 11) ? MUT_CARNIVOROUS :
                     (temp_rand == 10) ? MUT_HERBIVOROUS :
                     (temp_rand ==  9) ? MUT_FAST_METABOLISM :
                     (temp_rand ==  8) ? MUT_WEAK :
                     (temp_rand ==  7) ? MUT_DOPEY :
                     (temp_rand ==  6) ? MUT_CLUMSY :
                     (temp_rand ==  5) ? MUT_TELEPORT :
                     (temp_rand ==  4) ? MUT_DEFORMED :
                     (temp_rand ==  3) ? MUT_LOST :
                     (temp_rand ==  2) ? MUT_DETERIORATION :
                     (temp_rand ==  1) ? MUT_BLURRY_VISION
                                       : MUT_FRAIL);

    return mutate(which_bad_one, failMsg, forceMutation);
}                               // end give_bad_mutation()

//jmf: might be useful somewhere (eg Xom or transmigration effect)
bool give_cosmetic_mutation()
{
    mutation_type mutation = NUM_MUTATIONS;
    int how_much = 0;
    int counter = 0;

    do
    {
        mutation = MUT_DEFORMED;
        how_much = 1 + random2(3);

        if (one_chance_in(6))
        {
            mutation = MUT_ROBUST;
            how_much = 1 + random2(3);
        }

        if (one_chance_in(6))
        {
            mutation = MUT_FRAIL;
            how_much = 1 + random2(3);
        }

        if (one_chance_in(5))
        {
            mutation = MUT_TOUGH_SKIN;
            how_much = 1 + random2(3);
        }

        if (one_chance_in(4))
        {
            mutation = MUT_CLAWS;
            how_much = 1 + random2(3);
        }

        if (you.species != SP_CENTAUR && you.species != SP_NAGA
            && you.species != SP_KENKU && !player_genus(GENPC_DRACONIAN)
            && one_chance_in(5))
        {
            mutation = MUT_HOOVES;
            how_much = 1;
        }

        if (you.species != SP_VAMPIRE && one_chance_in(5))
        {
            mutation = MUT_FANGS;
            how_much = 1 + random2(3);
        }
    
        if (player_genus(GENPC_DRACONIAN) && one_chance_in(5))
        {
            mutation = MUT_BIG_WINGS;
            how_much = 1;
        }

        if (one_chance_in(5))
        {
            mutation = MUT_CARNIVOROUS;
            how_much = 1 + random2(3);
        }

        if (one_chance_in(6))
        {
            mutation = MUT_HORNS;
            how_much = 1 + random2(3);
        }

        if ((you.species == SP_NAGA || player_genus(GENPC_DRACONIAN))
            && one_chance_in(4))
        {
            mutation = MUT_STINGER;
            how_much = 1 + random2(3);
        }

        if (you.species == SP_NAGA && one_chance_in(6))
        {
            mutation = MUT_BREATHE_POISON;
            how_much = 1;
        }

        if (!(you.species == SP_NAGA || player_genus(GENPC_DRACONIAN))
            && one_chance_in(7))
        {
            mutation = MUT_SPIT_POISON;
            how_much = 1;
        }

        if (!(you.species == SP_NAGA || player_genus(GENPC_DRACONIAN))
            && one_chance_in(8))
        {
            mutation = MUT_BREATHE_FLAMES;
            how_much = 1 + random2(3);
        }

        if (you.mutation[mutation] > 0)
            how_much -= you.mutation[mutation];

        if (how_much < 0)
            how_much = 0;
    }
    while (how_much == 0 && counter++ < 5000);

    if (how_much != 0)
        return mutate(mutation);
    else
        return false;
}                               // end give_cosmetic_mutation()
