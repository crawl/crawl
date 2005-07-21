// insult generator
// Josh Fishman (c) 2001, All Rights Reserved
// This file is released under the GNU GPL, but special permission is granted
// to link with Linley Henzel's Dungeon Crawl (or Crawl) without change to
// Crawl's license.
//
// The goal of this stuff is catachronistic feel.

#include "AppHdr.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "externs.h"
#include "insult.h"
#include "mon-util.h"
#include "stuff.h"

static const char* insults1(void);
static const char* insults2(void);
static const char* insults3(void);
static const char* run_away(void);
static const char* give_up(void);
static const char* meal(void);
static const char* whilst_thou_can(void);
static const char* important_body_part(void);
static const char* important_spiritual_part(void);

static void init_cap(char *);

void init_cap(char * str)
{
    if (str != NULL)
        str[0] = toupper( str[0] );
}

void imp_taunt( struct monsters *mons )
{
    char buff[80];
    const char *mon_name = ptr_monam( mons, DESC_CAP_THE );

    snprintf( buff, sizeof(buff), 
              "%s, thou %s!", 
              random2(7) ? run_away() : give_up(),
              generic_insult() );

    init_cap( buff );

    // XXX: Not pretty, but stops truncation...
    if (strlen( mon_name ) + 11 + strlen( buff ) >= 79)
    {
        snprintf( info, INFO_SIZE, "%s shouts:", mon_name );
        mpr( info, MSGCH_TALK );

        mpr( buff, MSGCH_TALK );
    }
    else
    {
        snprintf( info, INFO_SIZE, "%s shouts, \"%s\"", mon_name, buff );
        mpr( info, MSGCH_TALK );
    }
}

void demon_taunt( struct monsters *mons )
{
    static const char * sound_list[] = 
    {
        "says",         // actually S_SILENT
        "shouts", 
        "barks", 
        "shouts", 
        "roars", 
        "screams", 
        "bellows", 
        "screeches", 
        "buzzes", 
        "moans", 
        "whines", 
        "croaks", 
        "growls", 
    };
  
    char buff[80];
    const char *mon_name = ptr_monam( mons, DESC_CAP_THE );
    const char *voice = sound_list[ mons_shouts(mons->type) ];

    if (coinflip())
    {
        snprintf( buff, sizeof(buff), 
                 "%s, thou %s!",
                 random2(3) ? give_up() : run_away(),
                 generic_insult() );
    }
    else
    {
        switch( random2( 4 ) ) 
        {
        case 0:
            snprintf( buff, sizeof(buff), 
                      "Thy %s shall be my %s!",
                      random2(4) ? important_body_part() 
                                 : important_spiritual_part(), meal() );
            break;
        case 1:
            snprintf( buff, sizeof(buff), 
                      "%s, thou tasty %s!", give_up(), meal() );
            break;
        case 2:
            snprintf( buff, sizeof(buff), 
                      "%s %s!", run_away(), whilst_thou_can() );
            break;
        case 3:
            snprintf( buff, sizeof(buff), 
                      "I %s %s thy %s!",
                      coinflip() ? "will" : "shall",
                      coinflip() ? "feast upon" : "devour",
                      random2(4) ? important_body_part() 
                                 : important_spiritual_part() );
            break;
        default:
            snprintf( buff, sizeof(buff), "Thou %s!", generic_insult() );
            break;
        }
    }

    init_cap( buff );

    // XXX: Not pretty, but stops truncation...
    if (strlen(mon_name) + strlen(voice) + strlen(buff) + 5 >= 79)
    {
        snprintf( info, INFO_SIZE, "%s %s:", mon_name, voice );
        mpr( info, MSGCH_TALK );

        mpr( buff, MSGCH_TALK );
    }
    else
    {
        snprintf( info, INFO_SIZE, "%s %s, \"%s\"", mon_name, voice, buff );
        mpr( info, MSGCH_TALK );
    }
}

const char * generic_insult(void)
{
    static char buffer[80]; //FIXME: use string objects or whatnot

    strcpy(buffer, insults1());
    strcat(buffer, " ");
    strcat(buffer, insults2());
    strcat(buffer, " ");
    strcat(buffer, insults3());

    return (buffer);
}

static const char * important_body_part(void)
{
    static const char * part_list[] = {
        "head",
        "brain",
        "heart",
        "viscera",
        "eyes",
        "lungs",
        "liver",
        "throat",
        "neck",
        "skull",
        "spine",
    };
  
    return (part_list[random2(sizeof(part_list) / sizeof(char *))]);
}

static const char * important_spiritual_part(void)
{
    static const char * part_list[] = {
        "soul",
        "spirit",
        "inner light",
        "hope",
        "faith",
        "will",
        "heart",
        "mind",
        "sanity",
        "fortitude",
        "life force",
    };

    return (part_list[random2(sizeof(part_list) / sizeof(char *))]);
}

static const char * meal(void)
{
    static const char * meal_list[] = {
        "meal",
        "breakfast",
        "lunch",
        "dinner",
        "supper",
        "repast",
        "snack",
        "victuals",
        "refection",
        "junket",
        "luncheon",
        "snackling",
        "curdle",
        "snacklet",
        "mouthful",
    };

    return (meal_list[random2(sizeof(meal_list) / sizeof(char *))]);
}

static const char * run_away(void)
{
    static const char * run_away_list[] = {
        "give up",
        "quit",
        "run away",
        "escape",
        "flee",
        "fly",
        "take thy face hence",
        "remove thy stench",
        "go and return not",
        "get thee hence",
        "back with thee",
        "away with thee",
        "turn tail",
        "leave",
        "return whence thou came",
        "begone",
        "get thee gone",
        "get thee hence",
        "slither away",
        "slither home",
        "slither hence",
        "crawl home",
        "scamper home",
        "scamper hence",
        "scamper away",
        "bolt",
        "decamp",
    };

    return (run_away_list[random2(sizeof(run_away_list) / sizeof(char *))]);
}

static const char * give_up(void)
{
    static const char * give_up_list[] = {
        "give up",
        "give in",
        "quit",
        "surrender",
        "kneel",
        "beg for mercy",
        "despair",
        "submit",
        "succumb",
        "quail",
        "embrace thy failure",
        "embrace thy fall",
        "embrace thy doom",
        "embrace thy dedition",
        "embrace submission",
        "accept thy failure",
        "accept thy fall",
        "accept thy doom",
        "capitulate",
        "tremble",
        "relinquish hope",
        "taste defeat",
        "despond",
        "disclaim thyself",
        "abandon hope",
        "face thy requiem",
        "face thy fugue",
        "admit defeat",
        "flounder",
    };

    return (give_up_list[random2(sizeof(give_up_list) / sizeof(char *))]);
}

static const char * whilst_thou_can(void)
{
    static const char * threat_list[] = {
        "whilst thou can",
        "whilst thou may",
        "whilst thou are able",
        "if wit thou hast",
        "whilst thy luck holds",
        "before doom catcheth thee",
        "lest death find thee",
        "whilst thou art whole",
        "whilst life thou hast", //jmf: hmm. screen vs. this for undead?
    };

    return (threat_list[random2(sizeof(threat_list) / sizeof(char *))]);
}

static const char * insults1(void)
{
    static const char * insults1_list[] = {
        "artless",
        "baffled",
        "bawdy",
        "beslubbering",
        "bootless",
        "bumbling",
        "canting",
        "churlish",
        "cockered",
        "clouted",
        "craven",
        "currish",
        "dankish",
        "dissembling",
        "droning",
        "ducking",
        "errant",
        "fawning",
        "feckless",
        "feeble",
        "fobbing",
        "foppish",
        "froward",
        "frothy",
        "fulsome",
        "gleeking",
        "goatish",
        "gorbellied",
        "grime-gilt",
        "horrid",
        "hateful",
        "impertinent",
        "infectious",
        "jarring",
        "loggerheaded",
        "lumpish",
        "mammering",
        "mangled",
        "mewling",
        "odious",
        "paunchy",
        "pribbling",
        "puking",
        "puny",
        "qualling",
        "quaking",
        "rank",
        "pandering",
        "pecksniffian",
        "plume-plucked",
        "pottle-deep",
        "pox-marked",
        "reeling-ripe",
        "rough-hewn",
        "simpering",
        "spongy",
        "surly",
        "tottering",
        "twisted",
        "unctious",
        "unhinged",
        "unmuzzled",
        "vain",
        "venomed",
        "villainous",
        "warped",
        "wayward",
        "weedy",
        "worthless",
        "yeasty",
    };

    return (insults1_list[random2(sizeof(insults1_list) / sizeof(char*))]);
}

static const char * insults2(void)
{
    static const char * insults2_list[] = {
        "base-court",
        "bat-fowling",
        "beef-witted",
        "beetle-headed",
        "boil-brained",
        "clapper-clawed",
        "clay-brained",
        "common-kissing",
        "crook-pated",
        "dismal-dreaming",
        "ditch-delivered",
        "dizzy-eyed",
        "doghearted",
        "dread-bolted",
        "earth-vexing",
        "elf-skinned",
        "fat-kidneyed",
        "fen-sucked",
        "flap-mouthed",
        "fly-bitten",
        "folly-fallen",
        "fool-born",
        "full-gorged",
        "guts-griping",
        "half-faced",
        "hasty-witted",
        "hedge-born",
        "hell-hated",
        "idle-headed",
        "ill-breeding",
        "ill-nurtured",
        "kobold-kissing",
        "knotty-pated",
        "limp-willed",
        "milk-livered",
        "moon-mazed",
        "motley-minded",
        "onion-eyed",
        "miscreant",
        "roguish",
        "moldwarp",
        "ruttish",
        "mumble-news",
        "saucy",
        "nut-hook",
        "spleeny",
        "pigeon-egg",
        "rude-growing",
        "rump-fed",
        "shard-borne",
        "sheep-biting",
        "sow-suckled",
        "spur-galled",
        "swag-bellied",
        "tardy-gaited",
        "tickle-brained",
        "toad-spotted",
        "toenail-biting",
        "unchin-snouted",
        "weather-bitten",
        "weevil-witted",
    };

    return (insults2_list[random2(sizeof(insults2_list) / sizeof(char*))]);
}

static const char * insults3(void)
{
    static const char * insults3_list[] = {
        "apple-john",
        "baggage",
        "bandersnitch",
        "barnacle",
        "beggar",
        "bladder",
        "boar-pig",
        "bounder",
        "bugbear",
        "bum-bailey",
        "canker-blossom",
        "clack-dish",
        "clam",
        "clotpole",
        "coxcomb",
        "codpiece",
        "death-token",
        "dewberry",
        "dingleberry",
        "flap-bat",
        "flax-wench",
        "flirt-gill",
        "foot-licker",
        "fustilarian",
        "giglet",
        "gnoll-tail",
        "gudgeon",
        "guttersnipe",
        "haggard",
        "harpy",
        "hedge-pig",
        "horn-beast",
        "hugger-mugger",
        "joithead",
        "lewdster",
        "lout",
        "maggot-pie",
        "malt-worm",
        "mammet",
        "measle",
        "mendicant",
        "minnow reeky",
        "mule",
        "nightsoil",
        "nobody",
        "nothing",
        "pigeon-egg",
        "pignut",
        "pimple",
        "pustule",
        "puttock",
        "pumpion",
        "ratsbane",
        "scavenger",
        "scut",
	"serf",
        "simpleton",
        "skainsmate",
        "slime mold",
        "snaffler",
        "snake-molt",
        "strumpet",
        "surfacer",
        "tinkerer",
        "tiddler",
        "urchin",
        "varlet",
        "vassal",
        "vulture",
        "wastrel",
        "wagtail",
        "whey-face",
        "wormtrail",
        "yak-dropping",
        "zombie-fodder",
    };

    return (insults3_list[random2(sizeof(insults3_list) / sizeof(char*))]);
}

// currently unused:
#if 0
const char * racial_insult(void)
{
    static const char * food3[] = {
        "snackling",
        "crunchlet",
        "half-meal",
        "supper-setting",
        "snacklet",
        "noshlet",
        "morsel",
        "mug-up",
        "bite-bait",
        "crunch-chow",
        "snack-pap",
        "grub",
    };

    static const char * elf1[] = {
        "weakly",
        "sickly",
        "frail",
        "delicate",
        "fragile",
        "brittle",
        "tender",
        "mooning",
        "painted",
        "lily-hearted",
        "dandy",
        "featherweight",
        "flimsy",
        "rootless",
        "spindly",
        "puny",
        "shaky",
        "prissy",
    };

    static const char * halfling3[] = {
        "half-pint",
        "footstool",
        "munchkin",
        "side-stool",
        "pudgelet",
        "groundling",
        "burrow-snipe",
        "hole-bolter",
        "low-roller",
        "runt",
        "peewee",
        "mimicus",
        "manikin",
        "hop-o-thumb",
        "knee-biter",
        "burrow-botch",
        "hole-pimple",
        "hovel-pustule",
    };

    static const char * spriggan3[] = {
        "rat-rider",
        "mouthfull",
        "quarter-pint",
        "nissette",
        "fizzle-flop",
        "spell-botch",
        "feeblet",
        "weakling",
        "pinchbeck-pixie",
        "ankle-biter",
        "bootstain",
        "nano-nebbish",
        "sopling",
        "shrunken violet",
        "sissy-prig",
        "pussyfoot",
        "creepsneak",
    };

    static const char * dwarf2[] = {
        "dirt-grubbing",
        "grit-sucking",
        "muck-plodding",
        "stone-broke",
        "pelf-dandling",
        "fault-botching",
        "gravel-groveling",
        "boodle-bothering",
        "cabbage-coddling",
        "rhino-raveling",
        "thigh-biting",
        "dirt-delving",
    };

    static const char * kenku2[] = {
        "hollow-boned",
        "feather-brained",
        "beak-witted",
        "hen-pecked",
        "lightweight",
        "frail-limbed",
        "bird-brained",
        "featherweight",
        "pigeon-toed",
        "crow-beaked",
        "magpie-eyed",
        "mallardish",
    };

    static const char * minotaur3[] = {
        "bull-brain",
        "cud-chewer",
        "calf-wit",
        "bovine",
        //"mooer", // of Venice
        "cow",
        "cattle",
        "meatloaf",
        "veal",
        "meatball",
        "rump-roast",
        "briscut",
        "cretin",
        "walking sirloin",
    };

    switch (you.species) 
    {
    default:
    break;
    }
}
#endif
