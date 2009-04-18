/*
 *  File:       unrand.h
 *  Summary:    Definitions for unrandom artefacts.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */
#ifndef UNRAND_H
#define UNRAND_H

#include "defines.h"
#include "itemprop.h"

/*
   List of "unrandom" artefacts. Not the same as "fixed" artefacts, which are
   completely hardcoded (eg Singing Sword, Wrath of Trog).
   note: the order of the list doesn't matter
   Because the list numbering starts at 1, the last entry is the highest value
   which can be given to NO_UNRANDARTS (eg if the list consists of randarts no
   1, 2 or 3, NO_UNRANDARTS must be set to 3 or lower, but probably not to 0).
   Setting it higher could cause nasty problems.

   Okay, so the steps to adding a new unrandart go as follows:
   1) - Fill in a new entry below, using the following guidelines:
   true name: The name which is displayed when the item is id'd
   un-id'd name: obvious
   class: weapon, armour etc
   type: long sword, plate mail etc. Jewellery unrandarts have the powers of
   their base types in addition to anything else.
   plus: For weapons, plus to-hit. For armour, plus. For jewellery, irrelevant.
   But add 100 to make the item stickycursed. Note that the values for
   wpns and armr are +50.
   plus2: For wpns, plus to-dam. Curses are irrelevant here. Mostly unused
   for armr and totally for rings.
   colour: Obvious. Don't use BLACK, use DARKGREY instead.

   * Note * any exact combination of class, type, plus & plus2 must be unique,
   so (for example) you can't have two +5, +5 long swords in the list. Curses
   don't count as distinguishing factors.

   brand: Weapons only. Have a look in enum.h for a list, and look in fight.cc
   and describe.cc for the effects.
   Range of possible values: see enum.h

   +/- to AC, ev, str, int, dex - These are pretty obvious. Be careful - a player
   with a negative str, int or dex dies instantly, so avoid high penalties
   to these stats.
   Range: any, but be careful.

   res fire, res cold: Resists. Can be above 1; multiple sources of fire or cold
   resist *are* cumulative. Can also be -1 (but probably not -2 or below)
   for susceptibility.
   Range: -1 to about 5, after which you'll become almost immune.
   res elec: Resist electricity. Unlike fire and cold, resist electricity
   reduces electrical damage to 0. This makes multiple resists irrelevant.
   Also is no susceptibility, so don't use -1.
   Range: 0 or 1.
   res poison: same as res electricity.
   Range: 0 or 1.
   life prot: Stops energy draining and negative energy attacks. Not cumulative,
   and no susceptibility here either.
   Range: 0 or 1.

   res magic: This is cumulative, but no susceptibility. To be meaningful,
   should be set to about 20 - 60.
   Range: 0 to MAXINT probably, but about 100 is a realistic ceiling.

   see invis: Lets you see invisible things, but not submerged water beasts.
   Range: 0 or 1.
   turn invis: Gives you the ability to turn invisible using the 'a' menu.
   levitate, blink, go berserk, sense surroundings: like turn invis.
   Ranges for all these: 0 or 1.

   make noise: Irritate nearby creatures and disrupts rest. Weapons only.
   Range: 0 or 1.
   no spells: Prevents any spellcasting (but not scrolls or wands etc)
   Range: 0 or 1.
   teleport: Every now and then randomly teleports you. *Really* annoying.
   Weapons only.
   Range: 0 to about 15 (higher means more teleporting).
   no teleport: Prevents the player from teleporting, except rarely when they're
   forced to (eg banished to the Abyss).
   Range: 0 or 1.

   force berserk: Every time you attack, you go berserk. Weapons only.
   Range: 0 or 1.
   speed metabolism: Makes you consume food faster. No effect on mummies.
   Range: 0 to about 4. 4 would be horrible; 1 is annoying but tolerable.
   mutate: makes you mutate, sometimes after a long delay. No effect on some
   races (espec undead).
   Range: 0 to about 4.
   +/- to-hit/to-dam: Obvious. Affects both melee and missile. Should be left
   at 0 for weapons, which get +s normally.

   cursed: -1, 0, any positive value.
   Setting this to any value other than 0 will set the item's initial curse
   status as cursed. If the value is greater than zero, the item will also have
   one_chance_in(value) of recursing itself when re-equipped.

   stealth: -100 to 80.  Adds to stealth value.

   Some currently unused properties follow, then:

   First string: is appended to the unrandart's 'V' description when id'd.

   Second string: replaces the thing at the start of a 'V' description.
   If empty, uses the description of the unrandart's base type. Note: the
   base type of a piece of unrandart jewellery is relevant to its function, so
   don't obscure it unnecessarily.

   Then another unused string.

   2) - Add one to the #define NO_UNRANDARTS line in randart.h

   3) - Maybe increase the probability of an unrandart of the appropriate
   type being created; look in dungeon.cc for this (search for "unrand").
   Forget this step if you don't understand it; it's of very little importance.

   Done! Now recompile and wait years for it to turn up.

   Note: changing NO_UNRANDARTS probably makes savefiles incompatible.

 */

/* This is a dummy, but still counts for NO_UNRANDARTS */
/* 1 */
{
    "Dum", "",
/* class, type, plus (to-hit), plus2 (depends on class), colour */
        OBJ_UNASSIGNED, 250, 250, 250, BLACK,
/* Properties, all approx thirty of them: */
    {
/* brand, +/- to AC, +/- to ev, +/- to str, +/- to int, +/- to dex */
        0, 0, 0, 0, 0, 0,
/* res fire, res cold, res elec, res poison, life protection, res magic */
        0, 0, 0, 0, 0, 0,
/* see invis, turn invis, levitate, blink, teleport at will, go berserk */
        0, 0, 0, 0, 0, 0,
/* sense surroundings, make noise, no spells, teleport, no teleport */
        0, 0, 0, 0, 0,
/* force berserk, speed metabolism, mutate, +/- to hit, +/- to dam (not weapons) */
        0, 0, 0, 0, 0,
/* cursed, stealth */
        0, 0
    }
    ,
/* Special description appended to the 'V' description */
        "",
/* Base description of item */
        "",
/* Unused string */
        ""
}
,


/* 2 */
{
    "long sword \"Bloodbane\"", "blackened long sword",
        OBJ_WEAPONS, WPN_LONG_SWORD, +7, +8, DARKGREY,
    {
        SPWPN_VORPAL, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 1,       // go berserk
        0, 0, 0, 0, 0,
        1, 0, 0, 0, 0,          // force berserk
        0, -20                  // stealth
    }
    ,
        "",
        "",
        ""
}
,

/* 3 */
{
    "ring of Shadows", "black ring",
        OBJ_JEWELLERY, RING_INVISIBILITY, 0, 0, DARKGREY,
    {
        0, 0, 4, 0, 0, 0,       // EV
        0, 0, 0, 0, 1, 0,       // life prot
        1, 0, 0, 0, 0, 0,       // see invis
        0, 0, 0, 0, 0,
        0, 0, 0, -3, 0,         // to hit
        0, 40                   // stealth
    }
    ,
        "",
        "",
        ""
}
,

/* 4 */
{
    "scimitar of Flaming Death", "smoking scimitar",
        OBJ_WEAPONS, WPN_SCIMITAR, +7, +5, RED,
    {
        SPWPN_FLAMING, 0, 0, 0, 0, 0,
        2, -1, 0, 1, 0, 20,     // res fire, cold, poison, magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,

/* 5 */
{
    "shield of Ignorance", "dull large shield",
        OBJ_ARMOUR, ARM_LARGE_SHIELD, +5, 0, BROWN,
    {
        0, 2, 2, 0, -6, 0,      // AC, EV, int
        0, 0, 0, 0, 1, 0,       // life prot
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        3, 0                    // cursed
    }
    ,
        "",
        "",
        ""
}
,


/* 6 */
{
    "amulet of the Air", "sky-blue amulet",
        OBJ_JEWELLERY, AMU_CONTROLLED_FLIGHT, 0, 0, LIGHTCYAN,
    {
        0, 0, 3, 0, 0, 0,       // EV
        0, 0, 1, 0, 0, 0,      // resElec
        0, 0, 1, 0, 0, 0,       // levitate
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 50                   // stealth
    }
    ,
        "",
        "A sky-blue amulet.",
        ""
}
,

/* 7 */
{
    "robe of Augmentation", "silk robe",
        OBJ_ARMOUR, ARM_ROBE, +4, 0, LIGHTRED,
    {
        0, 0, 0, 2, 2, 2,       // str, int, dex
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A robe made of the finest silk.",
        ""
}
,

/* 8 */
{
    "mace of Brilliance", "brightly glowing mace",
        OBJ_WEAPONS, WPN_MACE, +5, +5, WHITE,
    {
        SPWPN_HOLY_WRATH, 5, 0, 0, 5, 0,        // AC, int
        0, 0, 0, 0, 1, 0,                       // life prot
        1, 0, 0, 0, 0, 0,                       // see invis
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, -20                                  // stealth
    }
    ,
        "",
        "",
        ""
}
,

/* 9 */
{
    "cloak of the Thief", "tattered cloak",
        OBJ_ARMOUR, ARM_CLOAK, +1, 0, DARKGREY,
    {
        0, 0, 2, 0, 0, 2,       // EV, dex
        0, 0, 0, 0, 0, 0,
        1, 1, 1, 0, 0, 0,       // see invis, turn invis, levitate
        0, 0, 0, 0, 0,
        0, 0, 0, 0, -3,         // to dam
        0, 60                   // stealth
    }
    ,
        "It allows its wearer to excel in the arts of thievery.",
        "",
        ""
}
,

/* 10 */
{
    "shield \"Bullseye\"", "round shield",
        OBJ_ARMOUR, ARM_SHIELD, +10, 0, RED,
    {
        0, 0, -5, 0, 0, 0,      // EV
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,

/* 11 */
{
    "crown of Dyrovepreva", "jewelled bronze crown",
        OBJ_ARMOUR, ARM_CAP, +3, 0, BROWN,
    {
        0, 0, 0, 0, 2, 0,       // int
        0, 0, 1, 0, 0, 0,       // res elec
        1, 0, 0, 0, 0, 0,       // see invis
        0, 0, 0, 0, 0,
        0, 1, 0, 0, 0,          // speeds metabolism
        0, 0
    }
    ,
        "",
        "A large crown of dull bronze, set with a dazzling array of gemstones.",
        ""
}
,


/* 12 */
{
    "demon blade \"Leech\"", "runed demon blade",
        OBJ_WEAPONS, WPN_DEMON_BLADE, +13, +4, MAGENTA,
    {
        SPWPN_VAMPIRICISM, 0, -1, -1, -1, -1, // AC, EV, str, int, dex
        0, 0, 0, 0, 1, 0,                     // life prot
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        4, 0                                  // cursed
    }
    ,
        "",
        "",
        ""
}
,

/* 13 */
{
    "amulet of Cekugob", "crystal amulet",
        OBJ_JEWELLERY, AMU_WARDING, +0, 0, LIGHTGREY,
    {
        0, 1, 1, 0, 0, 0,       // AC, EV
        0, 0, 1, 1, 1, 0,       // res elec, poison, life prot
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1,          // prevent teleport
        0, 2, 0, 0, 0,          // speed metabolism
        0, 0
    }
    ,
        "",
        "",
        ""
}
,


/* 14 */
{
    "robe of Misfortune", "fabulously ornate robe",
        OBJ_ARMOUR, ARM_ROBE, -5, 0, MAGENTA,
    {
        0, 0, -4, -2, -2, -2,   // EV, str, int, dex
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 0,          // prevent spellcasting, cause teleport
        0, 0, 5, 0, 0,          // radiation
        1, -80                  // cursed, stealth
    }
    ,
        "",
        "A splendid flowing robe of fur and silk.",
        ""
}

,
/* 15 */
{
    "dagger of Chilly Death", "sapphire dagger",
        OBJ_WEAPONS, WPN_DAGGER, +5, +7, LIGHTBLUE,
    {
        SPWPN_FREEZING, 0, 0, 0, 0, 0,
        -1, 2, 0, 1, 0, 20,     // res fire, cold, poison, magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A dagger made of one huge piece of sapphire.",
        ""
}
,
/* 16 */
{
    "amulet of the Four Winds", "jade amulet",
        OBJ_JEWELLERY, AMU_CLARITY, +0, 0, LIGHTGREEN,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 100,      // life prot, magic resistance
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,

/* 17 */
{
    "dagger \"Morg\"", "rusty dagger",
        OBJ_WEAPONS, WPN_DAGGER, -1, +4, LIGHTRED,
    {
        SPWPN_PAIN, 0, 0, 0, 5, 0,      // int
        0, 0, 0, 0, 0, 30,              // res magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "Many years ago it was the property of a powerful mage called "
        "Boris. He got lost in the Dungeon while seeking the Orb. ",
        "An ugly rusty dagger. ",
        ""
}
,

/* 18 */
{
    "scythe \"Finisher\"", "blackened scythe",
        OBJ_WEAPONS, WPN_SCYTHE, +3, +5, DARKGREY,
    {
        SPWPN_SPEED, 0, 0, 3, 0, 0,       // str
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        -1, 0                    // cursed
    }
    ,
        "",
        "A long and sharp scythe, specially modified for combat purposes.",
        ""
}
,

/* 19 */
{
    "sling \"Punk\"", "blue sling",
        OBJ_WEAPONS, WPN_SLING, +9, +12, LIGHTBLUE,
    {
        SPWPN_FROST, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0,               // res cold
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A sling made of weird blue leather.",
        ""
}
,
/* 20 */
{
    "bow of Krishna \"Sharnga\"", "golden bow",
        OBJ_WEAPONS, WPN_BOW, +8, +8, YELLOW,
    {
        SPWPN_SPEED, 0, 0, 0, 0, 3,       // dex
        0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0,       // see invis
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "It once belonged to a foreign god. It works best with "
        "special arrows which are not generally available.",
        "A wonderful golden bow. ",
        ""
}
,
/* 21 */
{
    "cloak of Flash", "vibrating cloak",
        OBJ_ARMOUR, ARM_CLOAK, +3, 0, RED,
    {
        0, 0, 4, 0, 0, 0,       // EV
        0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 1, 0,       // levitate, teleport
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A vibrating cloak.",
        ""
}
,
/* 22 */
{
    "giant club \"Skullcrusher\"", "brutal giant club",
        OBJ_WEAPONS, WPN_GIANT_CLUB, +0, +5, BROWN,
    {
        SPWPN_SPEED, 0, 0, 5, 0, 0,       // str
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,
/* 23 */
{
    "boots of the Assassin", "soft boots",
        OBJ_ARMOUR, ARM_BOOTS, +2, 0, BROWN,
    {
        0, 0, 0, 0, 0, 3,       // dex
        0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0,       // turn invis
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 80                   // stealth
    }
    ,
        "These boots were specially designed by the Assassin's Guild.",
        "Some soft boots.",
        ""
}
,
/* 24 */
{
    "glaive of the Guard", "polished glaive",
        OBJ_WEAPONS, WPN_GLAIVE, +5, +8, LIGHTCYAN,
    {
        SPWPN_ELECTROCUTION, 5, 0, 0, 0, 0,       // AC
        0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 1,       // see invis, go berserk
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "This weapon once belonged to Gar Dogh, the guard of a king's treasures. "
        "According to legend he was lost somewhere in the Dungeon.",
        "",
        ""
}
,
/* 25 */
{
    "sword of Jihad", "crystal sword",
        OBJ_WEAPONS, WPN_LONG_SWORD, +12, +10, WHITE,
    {
        SPWPN_HOLY_WRATH, 0, 3, 0, 0, 0,        // EV
        0, 0, 0, 0, 1, 20,                      // life prot, res magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        1, 0, 0, 0, 0,                          // force berserk
        0, -50                                  // stealth (TSO hates backstab)
    }
    ,
        "This sword was The Shining One's gift to a worshipper.",
        "A long sword made of one huge piece of crystal.",
        ""
}
,
/* 26 */
{
    "Lear's chain mail", "golden chain mail",
        OBJ_ARMOUR, ARM_CHAIN_MAIL, -1, 0, YELLOW,
    {
        0, 0, 0, 0, 0, -3,      // dex
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,          // prevent spellcasting
        0, 0, 0, 0, 0,
        -1, 0                    // cursed
    }
    ,
        "",
        "A chain mail made of pure gold.",
        ""
}
,
/* 27 */
{
    "skin of Zhor", "smelly skin",
        OBJ_ARMOUR, ARM_ANIMAL_SKIN, +4, 0, BROWN,
    {
        0, 0, 0, 0, 0, 0,
        0, 2, 0, 0, 0, 0,       // res cold
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "The skin of some strange animal.",
        ""
}
,
/* 28 */
{
    "crossbow \"Hellfire\"", "flaming crossbow",
        OBJ_WEAPONS, WPN_CROSSBOW, +6, +9, LIGHTRED,
    {
        SPWPN_FLAME, 0, 0, 0, 0, 0,
        2, -1, 0, 0, 0, 40,              // +2 fire, -1 cold, res magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A flaming crossbow, forged in the fires of the Hells.",
        ""
}
,
/* 29 */
{
    "salamander hide armour", "red leather armour",
        OBJ_ARMOUR, ARM_LEATHER_ARMOUR, +3, 0, RED,
    {
        0, 0, 0, 0, 0, 0,
        2, 0, 0, 0, 0, 0,       // res fire
        0, 0, 0, 0, 0, 1,       // go berserk
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A leather armour made of a salamander's skin.",
        ""
}
,
/* 30 */
{
    "gauntlets of War", "thick gauntlets",
        OBJ_ARMOUR, ARM_GLOVES, +3, 0, BROWN,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 3, 3,          // to hit, to dam
        0, 0
    }
    ,
        "",
        "",
        ""
}
,
/* 31 */
{
    "sword of the Doom Knight", "adamantine great sword",
        OBJ_WEAPONS, WPN_GREAT_SWORD, +13, +13, BLUE,
    {
        SPWPN_PAIN, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 50,      // res magic
        0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,          // prevent spellcasting
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "An adamantine great sword.",
        ""
}
,
/* 32 */
{
    "shield of Resistance", "bronze shield",
        OBJ_ARMOUR, ARM_SHIELD, +3, 0, LIGHTRED,
    {
        0, 0, 0, 0, 0, 0,
        1, 1, 0, 0, 0, 40,      // res fire, cold, magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A bronze shield.",
        ""
}
,
/* 33 */
{
    "robe of Folly", "dull robe",
        OBJ_ARMOUR, ARM_ROBE, -1, 0, LIGHTGREY,
    {
        0, 0, 0, 0, -5, 0,      // int
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,          // prevent spellcasting
        0, 0, 0, 0, 0,
        2, 0                    // cursed
    }
    ,
        "",
        "A dull grey robe.",
        ""
}
,
/* 34 */
{
    "necklace of Bloodlust", "blood-stained necklace",
        OBJ_JEWELLERY, AMU_RAGE, +0, 0, RED,
    {
        0, 0, 0, 2, -2, 0,      // str, int
        0, 0, 0, 0, 0, 30,      // res magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        1, 0, 0, 0, 3,          // force berserk, to dam
        3, -20                  // cursed, stealth
    }
    ,
        "",
        "",
        ""
}
,
/* 35 */
{
    "\"Eos\"", "encrusted morningstar",
        OBJ_WEAPONS, WPN_MORNINGSTAR, +5, +5, LIGHTCYAN,
    {
        SPWPN_ELECTROCUTION, 0, 0, 0, 0, 0,  // morning -> bring light/sparks?
        0, 0, 1, 0, 0, 0,       // res elec
        1, 0, 0, 0, 0, 0,       // see invis
        0, 0, 0, 0, 1,          // prevent teleportation
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,
/* 36 */
{
    "ring of Shaolin", "jade ring",
        OBJ_JEWELLERY, RING_EVASION, +8, 0, LIGHTGREEN,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,
/* 37 */
{
    "ring of Robustness", "steel ring",
        OBJ_JEWELLERY, RING_PROTECTION, +8, 0, LIGHTGREY,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,
/* 38 */
{
    "Maxwell's patent armour", "weird-looking armour",
        OBJ_ARMOUR, ARM_PLATE_MAIL, +10, 0, LIGHTGREEN,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 1, 0, 1,          // prevent spellcasting, prevent teleport
        0, 0, 0, 0, 0,
        -1, 0                   // cursed
    }
    ,
        "",
        "A weird-looking armour.",
        ""
}
,
/* 39 */
{
    "spear of Voo-Doo", "ebony spear",
        OBJ_WEAPONS, WPN_SPEAR, +2, +10, DARKGREY,
    {
        SPWPN_VAMPIRICISM, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 1, 0,       // res poison, prot life
        0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0,          // noise
        0, 0, 0, 0, 0,
        0, -30                  // stealth
    }
    ,
        "A really dark and malign artefact, which no wise man would even touch.",
        "",
        ""
}
,
/* 40 */
{
    "trident of the Octopus King", "mangy trident",
        OBJ_WEAPONS, WPN_TRIDENT, +10, +4, CYAN,
    {
        SPWPN_VENOM, 0, 0, 0, 0, 0,
        0, 0, 1, 1, 0, 50,              // res elec, res poison, res magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "This trident was stolen many years ago from the Octopus King's garden "
        "by a really unimportant and already dead man. But beware of the "
        "Octopus King's wrath!",
        "",
        ""
}
,
/* 41 */
{
    "mask of the Dragon", "blue mask",
        OBJ_ARMOUR, ARM_CAP, +0, 0, BLUE,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 40,      // res magic
        1, 0, 0, 0, 0, 0,       // see invis
        0, 0, 0, 0, 0,
        0, 0, 0, 2, 2,          // to hit, to dam
        0, 0
    }
    ,
        "",
        "A blue mask.",
        ""
}
,
/* 42 */
{
    "mithril axe \"Arga\"", "mithril axe",
        OBJ_WEAPONS, WPN_WAR_AXE, +10, +6, WHITE,
    {
        SPWPN_SPEED, 0, 0, 2, 0, 0,     // str
        0, 0, 0, 0, 0, 30,              // resist magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A beautiful mithril axe, probably lost by some dwarven hero.",
        ""
}
,
/* 43 */
{
    "Elemental Staff", "black staff",
        OBJ_WEAPONS, WPN_QUARTERSTAFF, +3, +1, DARKGREY,
    {
        SPWPN_PROTECTION, 0, 0, 0, 0, 0,
        2, 2, 0, 0, 0, 60,      // res fire, cold, magic
        0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0,          // noise
        0, 2, 0, 0, 0,          // speed metabolism
        0, 0
    }
    ,
        "This powerful staff used to belong to the leader of "
        "the Guild of Five Elements.",
        "A black glyphic staff.",
        ""
}
,
/* 44 */
{
    "hand crossbow \"Sniper\"", "black crossbow",
        OBJ_WEAPONS, WPN_HAND_CROSSBOW, +10, +0, DARKGREY,
    {
        SPWPN_VENOM, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0,       // see invis
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A hand crossbow made of some black material.",
        ""
}
,
/* 45 */
{
    "longbow \"Piercer\"", "very long metal bow",
        OBJ_WEAPONS, WPN_LONGBOW, +2, +10, CYAN,
    {
        SPWPN_VORPAL, 0, -2, 0, 0, 0,       // ev
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "An exceptionally large metal longbow.",
        ""
}
,
/* 46 */
{
    "robe of Night", "black robe",
        OBJ_ARMOUR, ARM_ROBE, +4, 0, DARKGREY,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 30,      // res magic
        1, 1, 0, 0, 0, 0,       // see invis, turn invis
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 50                   // stealth
    }
    ,
        "According to legend, this robe was the gift of Ratri the Goddess of the Night "
        "to one of her followers.",
        "A long black robe made of strange flossy material.",
        ""
}
,
/* 47 */
{
    "plutonium sword", "glowing long sword",
        OBJ_WEAPONS, WPN_LONG_SWORD, +12, +16, LIGHTGREEN,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 6, 0, 0,          // radiation
        1, -20                  // cursed, stealth
    }
    ,
        "",
        "A long sword made of weird glowing metal.",
        ""
}
,
/* 48 */
{
    "great mace \"Undeadhunter\"", "great steel mace",
        OBJ_WEAPONS, WPN_GREAT_MACE, +7, +7, LIGHTGREY,
    {
        SPWPN_HOLY_WRATH, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 0,       // life prot
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,
/* 49 */
{
    "armour of the Dragon King", "shiny dragon armour",
        OBJ_ARMOUR, ARM_GOLD_DRAGON_ARMOUR, +5, 0, YELLOW,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 50,      // res magic (base gives fire, cold, poison)
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,
/* 50 */
{
    "hat of the Alchemist", "dirty hat",
        OBJ_ARMOUR, ARM_WIZARD_HAT, +2, 0, MAGENTA,
    {
        0, 0, 0, 0, 0, 0,
        1, 1, 1, 0, 0, 30,      // res fire, cold, elec, magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A dirty hat.",
        ""
}
,
/* 51 */
{
    "Fencer's gloves", "silk gloves",
        OBJ_ARMOUR, ARM_GLOVES, +2, 0, WHITE,
    {
        0, 0, 3, 0, 0, 3,       // EV, dex
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 5, 0,          // to hit
        0, 0
    }
    ,
        "",
        "A pair of gloves made of white silk.",
        ""
}
,
/* 52 */
{
    "ring of the Mage", "sapphire ring",
        OBJ_JEWELLERY, RING_WIZARDRY, +0, 0, LIGHTBLUE,
    {
        0, 0, 0, 0, 3, 0,       // int
        0, 0, 0, 0, 0, 50,      // res magic
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
}
,

/* 53 */
{
    "blowgun of the Assassin", "tiny blowgun",
        OBJ_WEAPONS, WPN_BLOWGUN, +6, +6, WHITE,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 0,       // turn invisible
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 60                   // stealth
    }
    ,
        "",
        "It is designed for easy concealment, but still packs a nasty punch.",
        ""
}
,

/* 54 */
{
    "Wyrmbane", "scale-covered lance",
        OBJ_WEAPONS, WPN_SPEAR, +9, +6, GREEN,
    {
        SPWPN_DRAGON_SLAYING, 5, 0, 0, 0, 0, // AC
        1, 0, 0, 1, 0, 0,     // res fire, poison
        0, 0, 0, 0, 0, 1,     // go berserk
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "",
        ""
},

/* 55 */
{
    "Spriggan's Knife", "dainty little knife",
        OBJ_WEAPONS, WPN_KNIFE, +4, +10, LIGHTCYAN,
    {
        0, 0, 4, 0, 0, 4,  // +EV, +Dex
        0, 0, 0, 0, 0, 20, // +MR
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 50 // +stealth
    }
    ,
        "This knife was made by Spriggans, or for Spriggans, or possibly from "
        "Spriggans. Anyway, it's in some way associated with those fey folk.",
        "A dainty little knife.",
        ""
},

/* 56 */
{
    "cloak of Starlight", "phosphorescent cloak",
        OBJ_ARMOUR, ARM_CLOAK, 0, 0, WHITE,
    {
        0, 0, 4, 0, 0, 0,  // EV
        0, 1, 1, 0, 0, 0,  // Cold, resElec
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, -30             // Stealth
    }
    ,
        "A cloak woven of pure light beams.",
        "A phosphorescent cloak.",
        ""
},

/* 57 */
{
    "brooch of Shielding", "shield-shaped amulet",
        OBJ_JEWELLERY, AMU_WARDING, 0, 0, LIGHTBLUE,
    {
        0, 4, 4, 0, 0, 0, // +AC, EV
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "Back in the good old days, every adventurer had one of these handy "
        "devices. That, and a pony.",
        "A shield-shaped amulet.",
        ""
},

/* 58 */
{
    "whip \"Serpent-Scourge\"", "forked whip",
        OBJ_WEAPONS, WPN_WHIP, +5, +10, DARKGREY,
    {
        SPWPN_VENOM, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, // rPois
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "",
        "A double-ended whip made from the cured hides of the "
        "Lair of Beasts' deadly grey snakes.",
        ""
},

/* 59 */
{
    // This used to be a fixed artefact but since it has no special
    // properties I decided it more closely fits here. (jpeg)
    "knife of Accuracy", "thin dagger",
        OBJ_WEAPONS, WPN_DAGGER, +27, -1, LIGHTCYAN,
    {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0
    }
    ,
        "It is almost unerringly accurate.",
        "",
        ""
},

/* This is a dummy. */
{
    "Dum", "",
/* class, type, plus (to-hit), plus2 (depends on class), colour */
        OBJ_UNASSIGNED, 250, 250, 250, BLACK,
/* Properties, all approx thirty of them: */
    {
/* brand, +/- to AC, +/- to ev, +/- to str, +/- to int, +/- to dex */
        0, 0, 0, 0, 0, 0,
/* res fire, res cold, res elec, res poison, life protection, res magic */
        0, 0, 0, 0, 0, 0,
/* see invis, turn invis, levitate, blink, teleport at will, go berserk */
        0, 0, 0, 0, 0, 0,
/* sense surroundings, make noise, no spells, teleport, no teleport */
        0, 0, 0, 0, 0,
/* force berserk, speed metabolism, mutate, +/- to hit, +/- to dam (not weapons) */
        0, 0, 0, 0, 0,
/* cursed, stealth */
        0, 0
    }
    ,
/* 3 strings for describing the item in the 'V' display. */
        "",
        "",
        ""
}
#endif
