/*
 *  File:       abl-show.cc
 *  Summary:    Functions related to special abilities.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *  <7>    april2009     Cha    added Zot abilities and flags
 *  <6>    19mar2000     jmf    added elvish Glamour
 *  <5>     11/06/99     cdl    reduced power of minor destruction
 *
 *  <4>      9/25/99     cdl    linuxlib -> liblinux
 *
 *  <3>      5/20/99     BWR    Now use scan_randarts to
 *                              check for flags, rather than
 *                              only checking the weapon.
 *
 *  <2>      5/20/99     BWR    Extended screen line support
 *
 *  <1>      -/--/--     LRH             Created
 */

#include "AppHdr.h"
#include "abl-show.h"

#include <sstream>
#include <iomanip>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "abyss.h"
#include "beam.h"
#include "database.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "dungeon.h" // // for place_specific_stair
#include "effects.h"
#include "food.h"
#include "it_use2.h"
#include "itemprop.h"
#include "items.h"  // // for AQ_SCROLL
#include "macro.h"
#include "maps.h" // // for random_map_for_tag
#include "message.h"
#include "menu.h"
#include "misc.h"
#include "mon-util.h"
#include "monplace.h"
#include "monstuff.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"


#ifdef UNIX
#include "libunix.h"
#endif

enum ability_flag_type
{
    ABFLAG_NONE         = 0x00000000,
    ABFLAG_BREATH       = 0x00000001, // ability uses DUR_BREATH_WEAPON
    ABFLAG_DELAY        = 0x00000002, // ability has its own delay (ie recite)
    ABFLAG_PIETY        = 0x00000004, // ability has its own piety (ie vitalise)
    ABFLAG_PAIN         = 0x00000008, // ability must hurt player (ie torment)
    ABFLAG_EXHAUSTION   = 0x00000010, // fails if you.exhausted
    ABFLAG_INSTANT      = 0x00000020, // doesn't take time to use
    ABFLAG_PERMANENT_HP = 0x00000040, // costs permanent HPs
    ABFLAG_PERMANENT_MP = 0x00000080, // costs permanent MPs
    ABFLAG_CONF_OK      = 0x00000100, // can use even if confused
    ABFLAG_TWO_XP       = 0x00000200, // // costs 2 XP
    ABFLAG_TEN_XP       = 0X00000400, // // costs 10 XP
    ABFLAG_HUNDRED_XP   = 0X00000800, // // costs 100 XP
    ABFLAG_THOUSAND_XP  = 0X00001000, // // costs 1000 XP
    ABFLAG_THREE_THOUSAND_XP = 0X00002000, // // costs 3000 XP
    ABFLAG_TEN_THOUSAND_XP = 0X00004000, // // costs 10,000 XP
    ABFLAG_THIRTY_XP = 0X00008000, // // costs 30 XP
    ABFLAG_THREE_HUNDRED_XP = 0X00010000, // // costs 300 XP
    ABFLAG_ENCH_MISCAST = 0X00020000, // // severity 3 enchantment miscast
    ABFLAG_TLOC_MISCAST = 0X00040000, // // severity 3 translocation miscast
    ABFLAG_NECRO_MISCAST_MINOR = 0X00080000, // // severity 2 necro miscast
    ABFLAG_NECRO_MISCAST = 0X00100000, // // severity 3 necro miscast
    ABFLAG_TMIG_MISCAST = 0X00200000, // // severity 3 transmigration miscast
    ABFLAG_LEVEL_DRAIN = 0X00400000, // // drains 2 levels
    ABFLAG_FIFTY_XP = 0X00800000 // // costs 50 XP
};

static void _lugonu_bends_space();
static int  _find_ability_slot( ability_type which_ability );
static bool _activate_talent(const talent& tal);
static bool _do_ability(const ability_def& abil);
static void _pay_ability_costs(const ability_def& abil);
static std::string _describe_talent(const talent& tal);

// this all needs to be split into data/util/show files
// and the struct mechanism here needs to be rewritten (again)
// along with the display routine to piece the strings
// together dynamically ... I'm getting to it now {dlb}

// it makes more sense to think of them as an array
// of structs than two arrays that share common index
// values -- well, doesn't it? {dlb}

// declaring this const messes up externs later, so don't do it
ability_type god_abilities[MAX_NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_NON_ABILITY },
    // Zin
    { ABIL_ZIN_RECITE, ABIL_ZIN_VITALISATION, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_ZIN_SANCTUARY },
    // TSO
    { ABIL_NON_ABILITY, ABIL_TSO_DIVINE_SHIELD, ABIL_NON_ABILITY,
      ABIL_TSO_CLEANSING_FLAME, ABIL_TSO_SUMMON_DAEVA },
    // Kikubaaqudgha
    { ABIL_KIKU_RECALL_UNDEAD_SLAVES, ABIL_NON_ABILITY,
      ABIL_KIKU_ENSLAVE_UNDEAD, ABIL_NON_ABILITY,
      ABIL_KIKU_INVOKE_DEATH },
    // Yredelemnul
    { ABIL_YRED_ANIMATE_CORPSE, ABIL_YRED_RECALL_UNDEAD,
      ABIL_YRED_ANIMATE_DEAD, ABIL_YRED_DRAIN_LIFE,
      ABIL_YRED_CONTROL_UNDEAD },
    // Xom
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_NON_ABILITY },
    // Vehumet
    { ABIL_NON_ABILITY, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_NON_ABILITY },
    // Okawaru
    { ABIL_OKAWARU_MIGHT, ABIL_NON_ABILITY, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_OKAWARU_HASTE },
    // Makhleb
    { ABIL_NON_ABILITY, ABIL_MAKHLEB_MINOR_DESTRUCTION,
      ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB,
      ABIL_MAKHLEB_MAJOR_DESTRUCTION,
      ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB },
    // Sif Muna
    { ABIL_SIF_MUNA_CHANNEL_ENERGY,
      ABIL_SIF_MUNA_FORGET_SPELL, ABIL_NON_ABILITY,
      ABIL_NON_ABILITY, ABIL_NON_ABILITY },
    // Trog
    { ABIL_TROG_BERSERK, ABIL_TROG_REGENERATION, ABIL_NON_ABILITY,
      ABIL_TROG_BROTHERS_IN_ARMS, ABIL_NON_ABILITY },
    // Nemelex
    { ABIL_NEMELEX_DRAW_ONE, ABIL_NEMELEX_PEEK_TWO,
      ABIL_NEMELEX_TRIPLE_DRAW, ABIL_NEMELEX_MARK_FOUR,
      ABIL_NEMELEX_STACK_FIVE },
    // Elyvilon
    { ABIL_ELYVILON_LESSER_HEALING, ABIL_ELYVILON_PURIFICATION,
      ABIL_ELYVILON_HEALING, ABIL_ELYVILON_RESTORATION,
      ABIL_ELYVILON_GREATER_HEALING },
    // Lugonu
    { ABIL_LUGONU_ABYSS_EXIT, ABIL_LUGONU_BEND_SPACE,
      ABIL_LUGONU_BANISH, ABIL_LUGONU_CORRUPT,
      ABIL_LUGONU_ABYSS_ENTER },
    // Beogh
    { ABIL_NON_ABILITY, ABIL_BEOGH_SMITING,
      ABIL_NON_ABILITY, ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS,
      ABIL_NON_ABILITY }
};

// The description screen was way out of date with the actual costs.
// This table puts all the information in one place... -- bwr
//
// The four numerical fields are: MP, HP, food, and piety.
// Note:  food_cost  = val + random2avg( val, 2 )
//        piety_cost = val + random2( (val + 1) / 2 + 1 );
static const ability_def Ability_List[] =
{
    // NON_ABILITY should always come first
    { ABIL_NON_ABILITY, "No ability", 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_SPIT_POISON, "Spit Poison", 0, 0, 40, 0, ABFLAG_BREATH },

    { ABIL_MAPPING, "Sense Surroundings", 0, 0, 30, 0, ABFLAG_NONE },
    { ABIL_TELEPORTATION, "Teleportation", 3, 0, 200, 0, ABFLAG_NONE },
    { ABIL_BLINK, "Blink", 1, 0, 50, 0, ABFLAG_NONE },

    { ABIL_BREATHE_FIRE, "Breathe Fire", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_FROST, "Breathe Frost", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_POISON, "Breathe Poison Gas", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_LIGHTNING, "Breathe Lightning", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_POWER, "Breathe Power", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_STICKY_FLAME, "Breathe Sticky Flame", 0, 0, 125, 0, ABFLAG_BREATH },
    { ABIL_BREATHE_STEAM, "Breathe Steam", 0, 0, 75, 0, ABFLAG_BREATH },
    { ABIL_TRAN_BAT, "Bat Form", 2, 0, 0, 0, ABFLAG_NONE },

    { ABIL_SPIT_ACID, "Spit Acid", 0, 0, 125, 0, ABFLAG_NONE },

    { ABIL_FLY, "Fly", 3, 0, 100, 0, ABFLAG_NONE },
    { ABIL_SUMMON_MINOR_DEMON, "Summon Minor Demon", 3, 3, 75, 0, ABFLAG_NONE },
    { ABIL_SUMMON_DEMON, "Summon Demon", 5, 5, 150, 0, ABFLAG_NONE },
    { ABIL_HELLFIRE, "Hellfire", 8, 8, 200, 0, ABFLAG_NONE },
    { ABIL_TORMENT, "Torment", 9, 0, 250, 0, ABFLAG_PAIN },
    { ABIL_RAISE_DEAD, "Raise Dead", 5, 5, 150, 0, ABFLAG_NONE },
    { ABIL_CONTROL_DEMON, "Control Demon", 4, 4, 100, 0, ABFLAG_NONE },
    { ABIL_TO_PANDEMONIUM, "Gate Yourself to Pandemonium", 7, 0, 200, 0, ABFLAG_NONE },
    { ABIL_CHANNELING, "Channeling", 1, 0, 30, 0, ABFLAG_NONE },
    { ABIL_THROW_FLAME, "Throw Flame", 1, 1, 50, 0, ABFLAG_NONE },
    { ABIL_THROW_FROST, "Throw Frost", 1, 1, 50, 0, ABFLAG_NONE },
    { ABIL_BOLT_OF_DRAINING, "Bolt of Draining", 4, 4, 100, 0, ABFLAG_NONE },

    // FLY_II used to have ABFLAG_EXHAUSTION, but that's somewhat meaningless
    // as exhaustion's only (and designed) effect is preventing Berserk. -- bwr
    { ABIL_FLY_II, "Fly", 0, 0, 25, 0, ABFLAG_NONE },
    { ABIL_DELAYED_FIREBALL, "Release Delayed Fireball", 0, 0, 0, 0, ABFLAG_INSTANT },
    { ABIL_MUMMY_RESTORATION, "Self-Restoration", 1, 0, 0, 0, ABFLAG_PERMANENT_MP },

    // EVOKE abilities use Evocations and come from items:
    // Mapping, Teleportation, and Blink can also come from mutations
    // so we have to distinguish them (see above).  The off items
    // below are labeled EVOKE because they only work now if the
    // player has an item with the evocable power (not just because
    // you used a wand, potion, or miscast effect).  I didn't see
    // any reason to label them as "Evoke" in the text, they don't
    // use or train Evocations (the others do).  -- bwr
    { ABIL_EVOKE_MAPPING, "Evoke Sense Surroundings", 0, 0, 30, 0, ABFLAG_NONE },
    { ABIL_EVOKE_TELEPORTATION, "Evoke Teleportation", 3, 0, 200, 0, ABFLAG_NONE },
    { ABIL_EVOKE_BLINK, "Evoke Blink", 1, 0, 50, 0, ABFLAG_NONE },

    { ABIL_EVOKE_BERSERK, "Evoke Berserk Rage", 0, 0, 0, 0, ABFLAG_NONE },

    { ABIL_EVOKE_TURN_INVISIBLE, "Evoke Invisibility", 2, 0, 250, 0, ABFLAG_NONE },
    { ABIL_EVOKE_TURN_VISIBLE, "Turn Visible", 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_EVOKE_LEVITATE, "Evoke Levitation", 1, 0, 100, 0, ABFLAG_NONE },
    { ABIL_EVOKE_STOP_LEVITATING, "Stop Levitating", 0, 0, 0, 0, ABFLAG_NONE },

    { ABIL_END_TRANSFORMATION, "End Transformation", 0, 0, 0, 0, ABFLAG_NONE },

    // INVOCATIONS:
    // Zin
    { ABIL_ZIN_RECITE, "Recite", 3, 0, 120, 0, ABFLAG_DELAY },
    { ABIL_ZIN_VITALISATION, "Vitalisation", 0, 0, 100, 0, ABFLAG_PIETY | ABFLAG_CONF_OK },
    { ABIL_ZIN_SANCTUARY, "Sanctuary", 7, 0, 150, 15, ABFLAG_NONE },

    // The Shining One
    { ABIL_TSO_DIVINE_SHIELD, "Divine Shield", 3, 0, 50, 2, ABFLAG_NONE },
    { ABIL_TSO_CLEANSING_FLAME, "Cleansing Flame", 5, 0, 100, 2, ABFLAG_NONE },
    { ABIL_TSO_SUMMON_DAEVA, "Summon Daeva", 8, 0, 150, 4, ABFLAG_NONE },

    // Kikubaaqudgha
    { ABIL_KIKU_RECALL_UNDEAD_SLAVES, "Recall Undead Slaves", 2, 0, 50, 0, ABFLAG_NONE },
    { ABIL_KIKU_ENSLAVE_UNDEAD, "Enslave Undead", 4, 0, 150, 3, ABFLAG_NONE },
    { ABIL_KIKU_INVOKE_DEATH, "Invoke Death", 4, 0, 250, 3, ABFLAG_NONE },

    // Yredelemnul
    { ABIL_YRED_ANIMATE_CORPSE, "Animate Corpse", 1, 0, 50, 0, ABFLAG_NONE },
    { ABIL_YRED_RECALL_UNDEAD, "Recall Undead Slaves", 2, 0, 50, 0, ABFLAG_NONE },
    { ABIL_YRED_ANIMATE_DEAD, "Animate Dead", 3, 0, 100, 1, ABFLAG_NONE },
    { ABIL_YRED_DRAIN_LIFE, "Drain Life", 6, 0, 200, 2, ABFLAG_NONE },
    { ABIL_YRED_CONTROL_UNDEAD, "Control Undead", 5, 0, 150, 2, ABFLAG_NONE },

    // Okawaru
    { ABIL_OKAWARU_MIGHT, "Might", 2, 0, 50, 1, ABFLAG_NONE },
    { ABIL_OKAWARU_HASTE, "Haste",
      5, 0, 100, generic_cost::fixed(5), ABFLAG_NONE },

    // Makhleb
    { ABIL_MAKHLEB_MINOR_DESTRUCTION, "Minor Destruction", 1, 0, 20, 0, ABFLAG_NONE },
    { ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB, "Lesser Servant of Makhleb", 2, 0, 50, 1, ABFLAG_NONE },
    { ABIL_MAKHLEB_MAJOR_DESTRUCTION, "Major Destruction", 4, 0, 100, 2, ABFLAG_NONE },
    { ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB, "Greater Servant of Makhleb", 6, 0, 100, 3, ABFLAG_NONE },

    // Sif Muna
    { ABIL_SIF_MUNA_CHANNEL_ENERGY, "Channel Energy", 0, 0, 100, 0, ABFLAG_NONE },
    { ABIL_SIF_MUNA_FORGET_SPELL, "Forget Spell", 5, 0, 0, 8, ABFLAG_NONE },

    // Trog
    { ABIL_TROG_BURN_BOOKS, "Burn Books", 0, 0, 10, 0, ABFLAG_NONE },
    { ABIL_TROG_BERSERK, "Berserk", 0, 0, 200, 0, ABFLAG_NONE },
    { ABIL_TROG_REGENERATION, "Trog's Hand", 0, 0, 50, 1, ABFLAG_NONE },
    { ABIL_TROG_BROTHERS_IN_ARMS, "Brothers in Arms",
      0, 0, 100, generic_cost::range(5, 6), ABFLAG_NONE },

    // Elyvilon
    { ABIL_ELYVILON_DESTROY_WEAPONS, "Destroy Weapons", 0, 0, 0, 0, ABFLAG_NONE },
    { ABIL_ELYVILON_LESSER_HEALING, "Lesser Healing",
      1, 0, 100, generic_cost::range(0, 1), ABFLAG_CONF_OK },
    { ABIL_ELYVILON_PURIFICATION, "Purification", 2, 0, 150, 1,
      ABFLAG_CONF_OK },
    { ABIL_ELYVILON_HEALING, "Healing", 2, 0, 250, 2, ABFLAG_CONF_OK },
    { ABIL_ELYVILON_RESTORATION, "Restoration", 3, 0, 400, 3, ABFLAG_CONF_OK },
    { ABIL_ELYVILON_GREATER_HEALING, "Greater Healing", 6, 0, 600, 5,
      ABFLAG_CONF_OK },

    // Lugonu
    { ABIL_LUGONU_ABYSS_EXIT,  "Depart the Abyss", 1, 0, 150, 10, ABFLAG_NONE },
    { ABIL_LUGONU_BEND_SPACE,  "Bend Space", 1, 0, 50, 0, ABFLAG_PAIN },
    { ABIL_LUGONU_BANISH,      "Banish",
      4, 0, 200, generic_cost::range(3, 4), ABFLAG_NONE },
    { ABIL_LUGONU_CORRUPT,     "Corrupt",
      7, 5, 500, generic_cost::range(10, 14), ABFLAG_NONE },
    { ABIL_LUGONU_ABYSS_ENTER, "Enter the Abyss",
      9, 0, 500, generic_cost::fixed(35), ABFLAG_PAIN },

    // Nemelex
    { ABIL_NEMELEX_DRAW_ONE, "Draw One", 2, 0, 0, 0, ABFLAG_NONE },
    { ABIL_NEMELEX_PEEK_TWO, "Peek at Two", 3, 0, 0, 1, ABFLAG_INSTANT },
    { ABIL_NEMELEX_TRIPLE_DRAW, "Triple Draw", 2, 0, 100, 2, ABFLAG_NONE },
    { ABIL_NEMELEX_MARK_FOUR, "Mark Four", 4, 0, 125, 5, ABFLAG_NONE },
    { ABIL_NEMELEX_STACK_FIVE, "Stack Five", 5, 0, 250, 10, ABFLAG_NONE },

    // Beogh
    { ABIL_BEOGH_SMITING, "Smiting",
      3, 0, 80, generic_cost::fixed(3), ABFLAG_NONE },
    { ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS, "Recall Orcish Followers",
      2, 0, 50, 0, ABFLAG_NONE },

    // These six are unused "evil" god abilities:
    { ABIL_CHARM_SNAKE, "Charm Snake", 6, 0, 200, 5, ABFLAG_NONE },
    { ABIL_TRAN_SERPENT_OF_HELL, "Turn into Demonic Serpent", 16, 0, 600, 8, ABFLAG_NONE },
    { ABIL_BREATHE_HELLFIRE, "Breathe Hellfire", 0, 8, 200, 0, ABFLAG_BREATH },

    { ABIL_ROTTING, "Rotting", 4, 4, 0, 2, ABFLAG_NONE },
    { ABIL_TORMENT_II, "Call Torment", 9, 0, 0, 3, ABFLAG_PAIN },

    // // zot defense abilities
    { ABIL_MAKE_FUNGUS, "Make mushroom circle", 0, 0, 0, 0, ABFLAG_TEN_XP },
    { ABIL_MAKE_PLANT, "Make plant", 0, 0, 0, 0, ABFLAG_TWO_XP },
    { ABIL_MAKE_OKLOB_PLANT, "Make oklob plant", 0, 0, 0, 0, ABFLAG_HUNDRED_XP},
    { ABIL_MAKE_DART_TRAP, "Make dart trap", 0, 0, 0, 0, ABFLAG_TEN_XP },
    { ABIL_MAKE_ICE_STATUE, "Make ice statue", 0, 0, 50, 0, ABFLAG_THOUSAND_XP },
    { ABIL_MAKE_OCS, "Make crystal statue", 0, 0, 200, 0, ABFLAG_THOUSAND_XP },
    { ABIL_MAKE_SILVER_STATUE, "Make silver statue", 0, 0, 400, 0, ABFLAG_THREE_THOUSAND_XP },
    { ABIL_MAKE_CURSE_SKULL, "Make curse skull", 0, 0, 600, 0, ABFLAG_TEN_THOUSAND_XP | ABFLAG_NECRO_MISCAST_MINOR }, 
    { ABIL_MAKE_TELEPORT, "Zot-teleport", 0, 0, 0, 0, ABFLAG_TWO_XP },
    { ABIL_MAKE_ARROW_TRAP, "Make arrow trap", 0, 0, 0, 0, ABFLAG_THIRTY_XP },
    { ABIL_MAKE_BOLT_TRAP, "Make bolt trap", 0, 0, 0, 0, ABFLAG_FIFTY_XP },
    { ABIL_MAKE_SPEAR_TRAP, "Make spear trap", 0, 0, 0, 0, ABFLAG_FIFTY_XP },
    { ABIL_MAKE_AXE_TRAP, "Make axe trap", 0, 0, 0, 0, ABFLAG_HUNDRED_XP },
    { ABIL_MAKE_NEEDLE_TRAP, "Make needle trap", 0, 0, 0, 0, ABFLAG_THIRTY_XP },
    { ABIL_MAKE_NET_TRAP, "Make net trap", 0, 0, 0, 0, ABFLAG_TWO_XP },
    { ABIL_MAKE_TELEPORT_TRAP, "Make teleport trap", 0, 0, 0, 0, ABFLAG_TEN_THOUSAND_XP | ABFLAG_TLOC_MISCAST }, 
    { ABIL_MAKE_ALARM_TRAP, "Make alarm trap", 0, 0, 0, 0, ABFLAG_TWO_XP },
    { ABIL_MAKE_BLADE_TRAP, "Make blade trap", 0, 0, 0, 0, ABFLAG_THREE_HUNDRED_XP },
    { ABIL_MAKE_OKLOB_CIRCLE, "Make oklob circle", 0, 0, 0, 0, ABFLAG_THOUSAND_XP },
    { ABIL_MAKE_ACQUIRE_GOLD, "Acquire gold", 0, 0, 0, 0, ABFLAG_LEVEL_DRAIN },
    { ABIL_MAKE_ACQUIREMENT, "Acquirement", 0, 0, 0, 0, ABFLAG_LEVEL_DRAIN },
    { ABIL_MAKE_WATER, "Make water", 0, 0, 0, 0, ABFLAG_TEN_XP },
    { ABIL_MAKE_ELECTRIC_EEL, "Make electric eel", 0, 0, 0, 0, ABFLAG_HUNDRED_XP },
    { ABIL_MAKE_BAZAAR, "Make bazaar", 0, 0, 0, 0, ABFLAG_HUNDRED_XP },
    { ABIL_MAKE_ALTAR, "Make altar", 0, 0, 0, 0, ABFLAG_TWO_XP },
    { ABIL_MAKE_GRENADES, "Make grenades", 0, 0, 0, 0, ABFLAG_TWO_XP },
    { ABIL_MAKE_SAGE, "Sage", 0, 0, 300, 0,  ABFLAG_INSTANT },

    { ABIL_RENOUNCE_RELIGION, "Renounce Religion", 0, 0, 0, 0, ABFLAG_NONE },
};

const struct ability_def & get_ability_def( ability_type abil )
{
    for (unsigned int i = 0;
         i < sizeof(Ability_List) / sizeof(Ability_List[0]); i++)
    {
        if (Ability_List[i].ability == abil)
            return (Ability_List[i]);
    }

    return (Ability_List[0]);
}

std::string print_abilities()
{
    std::string text = "\n<w>a:</w> ";

    const std::vector<talent> talents = your_talents(false);

    if ( talents.empty() )
        text += "no special abilities";
    else
    {
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (i)
                text += ", ";
            text += ability_name(talents[i].which);
        }
    }

    return text;
}

const std::string make_cost_description( ability_type ability )
{
    const ability_def& abil = get_ability_def(ability);
    std::ostringstream ret;
    if (abil.mp_cost)
    {
        ret << abil.mp_cost;
        if (abil.flags & ABFLAG_PERMANENT_MP)
            ret << " Permanent";
        ret << " MP";
    }

    if (abil.hp_cost)
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << abil.hp_cost;
        if (abil.flags & ABFLAG_PERMANENT_HP)
            ret << " Permanent";
        ret << " HP";
    }

    if (abil.food_cost && you.is_undead != US_UNDEAD
        && (you.is_undead != US_SEMI_UNDEAD || you.hunger_state > HS_STARVING))
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << "Food";   // randomized and amount hidden from player
    }

    if (abil.piety_cost || abil.flags & ABFLAG_PIETY)
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << "Piety";  // randomized and amount hidden from player
    }

    if (abil.flags & ABFLAG_BREATH)
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << "Breath";
    }

    if (abil.flags & ABFLAG_DELAY)
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << "Delay";
    }

    if (abil.flags & ABFLAG_PAIN)
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << "Pain";
    }

    if (abil.flags & ABFLAG_EXHAUSTION)
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << "Exhaustion";
    }

    if (abil.flags & ABFLAG_INSTANT)
    {
        if (!ret.str().empty())
            ret << ", ";

        ret << "Instant"; // not really a cost, more of a bonus -bwr
    }

    if (abil.flags & ABFLAG_TWO_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "2 XP";      // //
    }
    if (abil.flags & ABFLAG_TEN_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "10 XP";      // //
    }
    if (abil.flags & ABFLAG_HUNDRED_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "100 XP";      // //
    }
    if (abil.flags & ABFLAG_THOUSAND_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "1,000 XP";      // //
    }
    if (abil.flags & ABFLAG_THREE_THOUSAND_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "3,000 XP";      // //
    }
    if (abil.flags & ABFLAG_TEN_THOUSAND_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "10,000 XP";      // //
    }
    if (abil.flags & ABFLAG_THIRTY_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "30 XP";      // //
    }
    if (abil.flags & ABFLAG_THREE_HUNDRED_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "300 XP";      // //
    }
    if (abil.flags & ABFLAG_FIFTY_XP)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "50 XP";      // //
    }
    if (abil.flags & ABFLAG_LEVEL_DRAIN)
    {
        if (!ret.str().empty())
            ret << ", "; 

        ret << "Level drain";      // //
    }


    // If we haven't output anything so far, then the effect has no cost
    if (ret.str().empty())
        ret << "None";

    return (ret.str());
}

static talent _get_talent(ability_type ability, bool check_confused)
{
    ASSERT(ability != ABIL_NON_ABILITY);

    talent result;
    result.which = ability;

    int failure = 0;
    bool perfect = false;  // is perfect
    bool invoc = false;

    if (check_confused)
    {
        const ability_def &abil = get_ability_def(result.which);
        if (you.duration[DUR_CONF] && !testbits(abil.flags, ABFLAG_CONF_OK))
        {
            result.which = ABIL_NON_ABILITY;
            return result;
        }
    }

    // Look through the table to see if there's a preference, else
    // find a new empty slot for this ability. -- bwr
    const int index = _find_ability_slot( ability );
    if ( index != -1 )
        result.hotkey = index_to_letter(index);
    else
        result.hotkey = 0;      // means 'find later on'

    switch (ability)
    {
    // begin spell abilities
    case ABIL_DELAYED_FIREBALL:
    case ABIL_MUMMY_RESTORATION:
        perfect = true;
        failure = 0;
        break;

    // // begin zot defense abilities
    case ABIL_MAKE_FUNGUS:
    case ABIL_MAKE_PLANT:
    case ABIL_MAKE_OKLOB_PLANT:
    case ABIL_MAKE_DART_TRAP:
    case ABIL_MAKE_ICE_STATUE:
    case ABIL_MAKE_OCS:
    case ABIL_MAKE_SILVER_STATUE:
    case ABIL_MAKE_CURSE_SKULL:
    case ABIL_MAKE_TELEPORT:
    case ABIL_MAKE_ARROW_TRAP:
    case ABIL_MAKE_BOLT_TRAP:
    case ABIL_MAKE_SPEAR_TRAP:
    case ABIL_MAKE_AXE_TRAP:
    case ABIL_MAKE_NEEDLE_TRAP:
    case ABIL_MAKE_NET_TRAP:
    case ABIL_MAKE_TELEPORT_TRAP:
    case ABIL_MAKE_ALARM_TRAP:
    case ABIL_MAKE_BLADE_TRAP:
    case ABIL_MAKE_OKLOB_CIRCLE:
    case ABIL_MAKE_ACQUIRE_GOLD:
    case ABIL_MAKE_ACQUIREMENT:
    case ABIL_MAKE_WATER:
    case ABIL_MAKE_ELECTRIC_EEL:
    case ABIL_MAKE_BAZAAR:
    case ABIL_MAKE_ALTAR:
    case ABIL_MAKE_GRENADES:
    case ABIL_MAKE_SAGE:
        perfect = true;
        failure = 0;
        break;

    // begin species abilities - some are mutagenic, too {dlb}
    case ABIL_SPIT_POISON:
        failure = ((you.species == SP_NAGA) ? 20 : 40)
                        - 10 * player_mutation_level(MUT_SPIT_POISON)
                        - you.experience_level;
        break;

    case ABIL_EVOKE_MAPPING:
        failure = 30 - you.skills[SK_EVOCATIONS];
        break;

    case ABIL_MAPPING:
        failure = 40 - 10 * player_mutation_level(MUT_MAPPING)
                  - you.experience_level;
        break;

    case ABIL_BREATHE_FIRE:
        failure = ((you.species == SP_RED_DRACONIAN) ? 30 : 50)
                        - 10 * player_mutation_level(MUT_BREATHE_FLAMES)
                        - you.experience_level;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_BREATHE_FROST:
    case ABIL_BREATHE_POISON:
    case ABIL_SPIT_ACID:
    case ABIL_BREATHE_LIGHTNING:
    case ABIL_BREATHE_POWER:
    case ABIL_BREATHE_STICKY_FLAME:
        failure = 30 - you.experience_level;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_BREATHE_STEAM:
        failure = 20 - you.experience_level;

        if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
            failure -= 20;
        break;

    case ABIL_FLY:              // this is for kenku {dlb}
        failure = 45 - (3 * you.experience_level);
        break;

    case ABIL_FLY_II:           // this is for draconians {dlb}
        failure = 45 - (you.experience_level + you.strength);
        break;

    case ABIL_TRAN_BAT:
        failure = 45 - (2 * you.experience_level);
        break;
        // end species abilties (some mutagenic)

        // begin demonic powers {dlb}
    case ABIL_THROW_FLAME:
    case ABIL_THROW_FROST:
        failure = 10 - you.experience_level;
        break;

    case ABIL_SUMMON_MINOR_DEMON:
        failure = 27 - you.experience_level;
        break;

    case ABIL_CHANNELING:
    case ABIL_BOLT_OF_DRAINING:
        failure = 30 - you.experience_level;
        break;

    case ABIL_CONTROL_DEMON:
        failure = 35 - you.experience_level;
        break;

    case ABIL_SUMMON_DEMON:
        failure = 40 - you.experience_level;
        break;

    case ABIL_TO_PANDEMONIUM:
        failure = 57 - (you.experience_level * 2);
        break;

    case ABIL_HELLFIRE:
    case ABIL_RAISE_DEAD:
        failure = 50 - you.experience_level;
        break;

    case ABIL_TORMENT:
        failure = 60 - you.experience_level;
        break;

    case ABIL_BLINK:
        failure = 30 - (10 * player_mutation_level(MUT_BLINK))
                  - you.experience_level;
        break;

    case ABIL_TELEPORTATION:
        failure = ((player_mutation_level(MUT_TELEPORT_AT_WILL) > 1) ? 30 : 50)
                    - you.experience_level;
        break;
        // end demonic powers {dlb}

        // begin transformation abilities {dlb}
    case ABIL_END_TRANSFORMATION:
        perfect = true;
        failure = 0;
        break;

    case ABIL_BREATHE_HELLFIRE:
        failure = 32 - you.experience_level;
        break;
        // end transformation abilities {dlb}
        //
        // begin item abilities - some possibly mutagenic {dlb}
    case ABIL_EVOKE_TURN_INVISIBLE:
    case ABIL_EVOKE_TELEPORTATION:
        failure = 60 - 2 * you.skills[SK_EVOCATIONS];
        break;

    case ABIL_EVOKE_TURN_VISIBLE:
    case ABIL_EVOKE_STOP_LEVITATING:
        perfect = true;
        failure = 0;
        break;

    case ABIL_EVOKE_LEVITATE:
    case ABIL_EVOKE_BLINK:
        failure = 40 - 2 * you.skills[SK_EVOCATIONS];
        break;

    case ABIL_EVOKE_BERSERK:
        failure = 50 - 2 * you.skills[SK_EVOCATIONS];

        if (you.species == SP_TROLL)
            failure -= 30;
        else if (player_genus(GENPC_DWARVEN) || you.species == SP_HILL_ORC
                || you.species == SP_OGRE)
        {
            failure -= 10;
        }
        break;
        // end item abilities - some possibly mutagenic {dlb}

        // begin invocations {dlb}
    case ABIL_ELYVILON_PURIFICATION:
        invoc = true;
        failure = 20 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_ZIN_RECITE:
    case ABIL_KIKU_RECALL_UNDEAD_SLAVES:
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
    case ABIL_OKAWARU_MIGHT:
    case ABIL_ELYVILON_LESSER_HEALING:
    case ABIL_LUGONU_ABYSS_EXIT:
        invoc = true;
        failure = 30 - (you.piety / 20) - (6 * you.skills[SK_INVOCATIONS]);
        break;

    // destroying stuff doesn't train anything
    case ABIL_ELYVILON_DESTROY_WEAPONS:
        invoc = true;
        failure = 0;
        break;

    case ABIL_TROG_BURN_BOOKS:
        invoc = true;
        failure = 0;
        break;

    // These three are Trog abilities... Invocations means nothing -- bwr
    case ABIL_TROG_BERSERK:    // piety >= 30
        invoc = true;
        failure = 30 - you.piety;       // starts at 0%
        break;

    case ABIL_TROG_REGENERATION:         // piety >= 50
        invoc = true;
        failure = 80 - you.piety;       // starts at 30%
        break;

    case ABIL_TROG_BROTHERS_IN_ARMS:       // piety >= 100
        invoc = true;
        failure = 160 - you.piety;      // starts at 60%
        break;

    case ABIL_YRED_ANIMATE_CORPSE:
        invoc = true;
        failure = 40 - (you.piety / 20) - (3 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_ZIN_VITALISATION:
    case ABIL_TSO_DIVINE_SHIELD:
    case ABIL_BEOGH_SMITING:
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
    case ABIL_SIF_MUNA_FORGET_SPELL:
    case ABIL_KIKU_ENSLAVE_UNDEAD:
    case ABIL_YRED_ANIMATE_DEAD:
    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
    case ABIL_ELYVILON_HEALING:
    case ABIL_LUGONU_BEND_SPACE:
        invoc = true;
        failure = 40 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        invoc = true;
        failure = 40 - you.intel - you.skills[SK_INVOCATIONS];
        break;

    case ABIL_YRED_RECALL_UNDEAD:
        invoc = true;
        failure = 50 - (you.piety / 20) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_LUGONU_BANISH:
        invoc = true;
        failure = 60 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
    case ABIL_YRED_DRAIN_LIFE:
        invoc = true;
        failure = 60 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_TSO_CLEANSING_FLAME:
    case ABIL_ELYVILON_RESTORATION:
    case ABIL_YRED_CONTROL_UNDEAD:
    case ABIL_OKAWARU_HASTE:
    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
    case ABIL_LUGONU_CORRUPT:
        invoc = true;
        failure = 70 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_ZIN_SANCTUARY:
    case ABIL_TSO_SUMMON_DAEVA:
    case ABIL_KIKU_INVOKE_DEATH:
    case ABIL_ELYVILON_GREATER_HEALING:
    case ABIL_LUGONU_ABYSS_ENTER:
        invoc = true;
        failure = 80 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_NEMELEX_STACK_FIVE:
        invoc = true;
        failure = 80 - (you.piety / 25) - (4 * you.skills[SK_EVOCATIONS]);
        break;

    case ABIL_NEMELEX_MARK_FOUR:
        invoc = true;
        failure = 70 - (you.piety * 2 / 45)
            - (9 * you.skills[SK_EVOCATIONS] / 2);
        break;

    case ABIL_NEMELEX_TRIPLE_DRAW:
        invoc = true;
        failure = 60 - (you.piety / 20) - (5 * you.skills[SK_EVOCATIONS]);
        break;

    case ABIL_NEMELEX_PEEK_TWO:
        invoc = true;
        failure = 40 - (you.piety / 20) - (5 * you.skills[SK_EVOCATIONS]);
        break;

    case ABIL_NEMELEX_DRAW_ONE:
        invoc = true;
        perfect = true;         // Tactically important to allow perfection
        failure = 50 - (you.piety / 20) - (5 * you.skills[SK_EVOCATIONS]);
        break;

        //jmf: following for to-be-created gods
    case ABIL_CHARM_SNAKE:
        invoc = true;
        failure = 40 - (you.piety / 20) - (3 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_TRAN_SERPENT_OF_HELL:
        invoc = true;
        failure = 80 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_ROTTING:
        invoc = true;
        failure = 60 - (you.piety / 20) - (5 * you.skills[SK_INVOCATIONS]);
        break;

    case ABIL_TORMENT_II:
        invoc = true;
        failure = 70 - (you.piety / 25) - (you.skills[SK_INVOCATIONS] * 4);
        break;

    case ABIL_RENOUNCE_RELIGION:
        invoc = true;
        perfect = true;
        failure = 0;
        break;

        // end invocations {dlb}
    default:
        failure = -1;
        break;
    }

    // Perfect abilities are things which can go down to a 0%
    // failure rate (e.g., Renounce Religion.)
    if (failure <= 0 && !perfect)
        failure = 1;

    if (failure > 100)
        failure = 100;

    result.fail = failure;
    result.is_invocation = invoc;

    return result;
}

std::vector<const char*> get_ability_names()
{
    std::vector<talent> talents = your_talents(false);
    std::vector<const char*> result;
    for (unsigned int i = 0; i < talents.size(); ++i)
        result.push_back(get_ability_def(talents[i].which).name);
    return result;
}

static void _print_talent_description(talent tal)
{
    clrscr();

    std::string name   = get_ability_def(tal.which).name;

    // The suffix is necessary to distinguish between similarly named spells.
    // Yes, this is a hack. (XXX)
    std::string lookup = getLongDescription(name + "ability");
    if (lookup.empty())
    {
        // Try again without the suffix.
        lookup = getLongDescription(name);
    }

    if (lookup.empty()) // Still nothing found?
        cprintf("No description found.");
    else
    {
        std::ostringstream data;
        data << name << "$$" << lookup;
        print_description(data.str());
    }
    if (getch() == 0)
        getch();

    clrscr();
}

bool activate_ability()
{
    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    std::vector<talent> talents = your_talents(false);
    if ( talents.empty() )
    {
        // Give messages if the character cannot use innate talents right now.
        // * Vampires can't turn into bats when full of blood.
        // * Permanent flying (Kenku) cannot be turned off.
        if (you.species == SP_VAMPIRE && you.experience_level >= 3)
            mpr("Sorry, you're too full to transform right now.");
        else if (you.species == SP_KENKU && you.experience_level >= 5
                 || player_mutation_level(MUT_BIG_WINGS))
        {
            if (you.flight_mode() == FL_FLY)
                mpr("You're already flying!");
            else if (you.flight_mode() == FL_LEVITATE)
                mpr("You can only start flying from the ground.");
        }
        else
            mpr("Sorry, you're not good enough to have a special ability.");

        crawl_state.zero_turns_taken();
        return (false);
    }

    if (you.duration[DUR_CONF])
    {
        talents = your_talents(true);
        if (talents.empty())
        {
            mpr("You're too confused!");
            crawl_state.zero_turns_taken();
            return (false);
        }
    }

    int selected = -1;
    while (selected < 0)
    {
        msg::streams(MSGCH_PROMPT) << "Use which ability? (? or * to list, ! "
                                      "for descriptions)"
                                   << std::endl;

        const int keyin = get_ch();

        if (keyin == '?' || keyin == '*')
        {
            selected = choose_ability_menu(talents);
            if (selected == -1)
            {
                canned_msg( MSG_OK );
                return (false);
            }
        }
        else if (keyin == '!')
        {
            while (true)
            {
                selected = choose_ability_menu(talents, true);
                if (selected == -1)
                {
                    canned_msg( MSG_OK );
                    return (false);
                }
                _print_talent_description(talents[selected]);
            }
        }
        else if (keyin == ESCAPE || keyin == ' ' || keyin == '\r'
                 || keyin == '\n')
        {
            canned_msg( MSG_OK );
            return (false);
        }
        else if ( isalpha(keyin) )
        {
            // Try to find the hotkey.
            for (unsigned int i = 0; i < talents.size(); ++i)
            {
                if (talents[i].hotkey == keyin)
                {
                    selected = static_cast<int>(i);
                    break;
                }
            }

            // If we can't, cancel out.
            if (selected < 0)
            {
                mpr("You can't do that.");
                crawl_state.zero_turns_taken();
                return (false);
            }
        }
    }

    return _activate_talent(talents[selected]);
}

static bool _activate_talent(const talent& tal)
{
    // Doing these would outright kill the player due to stat drain.
    if (tal.which == ABIL_TRAN_BAT)
    {
        if (you.strength <= 5)
        {
            mpr("You lack the strength for this transformation.", MSGCH_WARN);
            crawl_state.zero_turns_taken();
            return (false);
        }
    }
    else if (tal.which == ABIL_END_TRANSFORMATION
             && you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT
             && you.dex <= 5)
    {
        mpr("Turning back with such low dexterity would be fatal!", MSGCH_WARN);
        more();
        crawl_state.zero_turns_taken();
        return (false);
    }

    if ((tal.which == ABIL_EVOKE_BERSERK || tal.which == ABIL_TROG_BERSERK)
        && !you.can_go_berserk(true))
    {
        crawl_state.zero_turns_taken();
        return (false);
    }

    // Some abilities don't need a hunger check.
    bool hungerCheck = true;
    switch (tal.which)
    {
        case ABIL_RENOUNCE_RELIGION:
        case ABIL_EVOKE_STOP_LEVITATING:
        case ABIL_EVOKE_TURN_VISIBLE:
        case ABIL_END_TRANSFORMATION:
        case ABIL_DELAYED_FIREBALL:
        case ABIL_MUMMY_RESTORATION:
        case ABIL_TRAN_BAT:
            hungerCheck = false;
            break;
        default:
            break;
    }

    if (hungerCheck && you.species != SP_VAMPIRE
        && you.hunger_state == HS_STARVING)
    {
        mpr("You're too hungry.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    const ability_def& abil = get_ability_def(tal.which);

    // Check that we can afford to pay the costs.
    if (!enough_mp( abil.mp_cost, false ))
    {
        crawl_state.zero_turns_taken();
        return (false);
    }

    if (!enough_hp( abil.hp_cost, false ))
    {
        crawl_state.zero_turns_taken();
        return (false);
    }
   
    if ((abil.flags & ABFLAG_TWO_XP) && (you.exp_available < 2))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }

    if ((abil.flags & ABFLAG_TEN_XP) && (you.exp_available < 10))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }

    if ((abil.flags & ABFLAG_HUNDRED_XP) && (you.exp_available < 100))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }

    if ((abil.flags & ABFLAG_FIFTY_XP) && (you.exp_available < 50))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }

    if ((abil.flags & ABFLAG_THOUSAND_XP) && (you.exp_available < 1000))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }

    if ((abil.flags & ABFLAG_THREE_THOUSAND_XP) && (you.exp_available < 3000))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }

    if ((abil.flags & ABFLAG_TEN_THOUSAND_XP) && (you.exp_available < 10000))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }
    if ((abil.flags & ABFLAG_THIRTY_XP) && (you.exp_available < 30))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }
    if ((abil.flags & ABFLAG_THREE_HUNDRED_XP) && (you.exp_available < 300))
    {
        mpr("That ability requires more experience in your experience pool."); // //
	crawl_state.zero_turns_taken(); // //
        return (false); // //
    }

    if (tal.which == ABIL_ZIN_SANCTUARY && env.sanctuary_time)
    {
        mpr("There's already a sanctuary in place on this level.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    // Don't insta-starve the player.
    // (Happens at 100, losing consciousness possible from 500 downward.)
    if (hungerCheck && you.species != SP_VAMPIRE)
    {
        const int expected_hunger = you.hunger - abil.food_cost * 2;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "hunger: %d, max. food_cost: %d, expected hunger: %d",
             you.hunger, abil.food_cost * 2, expected_hunger);
#endif
        // Safety margin for natural hunger, mutations etc.
        if (expected_hunger <= 150)
        {
            mpr("You're too hungry.");
            crawl_state.zero_turns_taken();
            return (false);
        }
    }

    if ((tal.which == ABIL_EVOKE_BERSERK || tal.which == ABIL_TROG_BERSERK)
        && !berserk_check_wielded_weapon())
    {
        crawl_state.zero_turns_taken();
        return (false);
    }

    // No turning back now... {dlb}
    if (random2avg(100, 3) < tal.fail)
    {
        mpr("You fail to use your ability.");
        you.turn_is_over = true;
        return (false);
    }

    const bool success = _do_ability(abil);
    if (success)
        _pay_ability_costs(abil);

    return (success);
}

static bool _do_ability(const ability_def& abil)
{
    int power;
    dist abild;
    bolt beam;
    dist spd;

    // Note: the costs will not be applied until after this switch
    // statement... it's assumed that only failures have returned! -- bwr
    switch (abil.ability)
    {
    case ABIL_MAKE_FUNGUS:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Center fungus circle where?");
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx + 1, beam.ty), you.pet_target), true);
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx - 1, beam.ty), you.pet_target), true);
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx + 1, beam.ty + 1), you.pet_target), true);
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx - 1, beam.ty + 1), you.pet_target), true);
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx + 1, beam.ty - 1), you.pet_target), true);
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx - 1, beam.ty - 1), you.pet_target), true);
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty + 1), you.pet_target), true);
        place_monster( mgen_data(MONS_FUNGUS, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty - 1), you.pet_target), true);
        break; // //
    }
    case ABIL_MAKE_PLANT:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make plant where?");
        if (place_monster( mgen_data(MONS_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty), you.pet_target), true) != -1)
	  mpr("Tendrils and shoots erupt from the earth and gnarl into the form of a plant.");

        break; // //
    }
    case ABIL_MAKE_OKLOB_PLANT:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make oklob plant where?");
        if (place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty), you.pet_target), true) != -1)
	  mpr("A rhizome shoots up through the ground and merges with vitriolic spirits in the atmosphere.");

        break; // //
    }
    case ABIL_MAKE_DART_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_DART);
        break; // //
    }
    case ABIL_MAKE_ICE_STATUE:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make ice statue where?");
        if (place_monster( mgen_data(MONS_ICE_STATUE, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty), you.pet_target), true) != -1)
	  mpr("Water vapor collects and crystallizes into an icy humanoid shape.");
        break; // //
    }
    case ABIL_MAKE_OCS:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make orange crystal statue where?");
        if (place_monster( mgen_data(MONS_ORANGE_STATUE, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty), you.pet_target), true) != -1)
	  mpr("Quartz juts from the ground and forms a humanoid shape. You smell citrus.");
        break; // //
    }
    case ABIL_MAKE_SILVER_STATUE:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make silver statue where?");
        if (place_monster( mgen_data(MONS_SILVER_STATUE, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty), you.pet_target), true) != -1)
	  mpr("Droplets of mercury fall from the ceiling and turn to silver, congealing into a humanoid shape.");
        break; // //
    }
    case ABIL_MAKE_CURSE_SKULL:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make curse skull where?");
        if (place_monster( mgen_data(MONS_CURSE_SKULL, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty), you.pet_target), true) != -1)
	  mpr("You sculpt a terrible being from the primitive principle of evil.");
        break; // //
    }
    case ABIL_MAKE_TELEPORT:
    {
        you_teleport_now( true, true ); 
        break; // //
    }
    case ABIL_MAKE_ARROW_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_ARROW);
        break; // //
    }
    case ABIL_MAKE_BOLT_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_BOLT);
        break; // //
    }
    case ABIL_MAKE_SPEAR_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_SPEAR);
        break; // //
    }
    case ABIL_MAKE_AXE_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_AXE);
        break; // //
    }
    case ABIL_MAKE_NEEDLE_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_NEEDLE);
        break; // //
    }
    case ABIL_MAKE_NET_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_NET);
        break; // //
    }
    case ABIL_MAKE_TELEPORT_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_TELEPORT);
        break; // //
    }
    case ABIL_MAKE_ALARM_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_ALARM);
        break; // //
    }
    case ABIL_MAKE_BLADE_TRAP:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make trap where?");
        place_specific_trap(beam.tx, beam.ty, TRAP_BLADE);
        break; // //
    }
    case ABIL_MAKE_OKLOB_CIRCLE:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Center oklob circle where?");
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx + 1, beam.ty), you.pet_target), true);
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx - 1, beam.ty), you.pet_target), true);
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx + 1, beam.ty + 1), you.pet_target), true);
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx - 1, beam.ty + 1), you.pet_target), true);
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx + 1, beam.ty - 1), you.pet_target), true);
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx - 1, beam.ty - 1), you.pet_target), true);
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty + 1), you.pet_target), true);
        place_monster( mgen_data(MONS_OKLOB_PLANT, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty - 1), you.pet_target), true);
        break; // //
    }
    case ABIL_MAKE_ACQUIRE_GOLD:
    {
        acquirement(OBJ_GOLD, AQ_SCROLL); // //
        break;
    }
    case ABIL_MAKE_ACQUIREMENT:
    {
        acquirement(OBJ_RANDOM, AQ_SCROLL); // //
        break;
    }
    case ABIL_MAKE_WATER:
    {
        _create_pond(you.pos(), 3, false); // //
        break;
    }
    case ABIL_MAKE_ELECTRIC_EEL:
    {
        dist beam;
        direction(beam, DIR_TARGET, TARG_ANY, -1, false, false, false, false, "Make electric eel where?");
        if (place_monster( mgen_data(MONS_ELECTRICAL_EEL, BEH_FRIENDLY, 0, coord_def(beam.tx, beam.ty), you.pet_target), true) != -1)
	  mpr("You make an electric eel.");
        break; // //
    }
    case ABIL_MAKE_BAZAAR:
    {
    // Early exit: don't clobber important features.
        if (is_critical_feature(grd[you.x_pos][you.y_pos]))
        {
            mpr("The dungeon trembles momentarily.");
            return (false);
        }

        // Generate a portal to something.
        const int mapidx = random_map_for_tag("trowel_portal", false, false);
        if (mapidx == -1)
        {
            mpr("A buggy portal flickers into view, then vanishes.");
        }
        else
        {
            {
                no_messages n;
                dgn_place_map(mapidx, false, true, true, you.pos());
            }
            mpr("A mystic portal forms.");
        }

        break;
    }
    case ABIL_MAKE_ALTAR:
    {
        // Generate an altar.
        if (grd[you.x_pos][you.y_pos] == DNGN_FLOOR)
        {
            // Might get GOD_NO_GOD and no altar.
            god_type rgod = static_cast<god_type>(random2(NUM_GODS));
            grd[you.x_pos][you.y_pos] = altar_for_god(rgod);

            if (grd[you.x_pos][you.y_pos] != DNGN_FLOOR)
            {
                mprf("An altar to %s grows from the floor before you!",
                     god_name(rgod).c_str());
            }
        }
        break;
    }
    case ABIL_MAKE_GRENADES:
    { 
        if (create_monster(
               mgen_data(MONS_GIANT_SPORE, BEH_FRIENDLY, 6,
                         you.pos(), you.pet_target,
                         0)) != -1)
	    mpr( "You create a living grenade." );
        if (create_monster(
               mgen_data(MONS_GIANT_SPORE, BEH_FRIENDLY, 6,
                         you.pos(), you.pet_target,
                         0)) != -1)
	    mpr( "You create a living grenade." );
        break;
    }
    case ABIL_MAKE_SAGE:
    {
        _sage_card( 20, DECK_RARITY_RARE);
	break;
    }


    case ABIL_MUMMY_RESTORATION:
    {
        mpr( "You infuse your body with magical energy." );
        bool did_restore = restore_stat( STAT_ALL, 0, false );

        const int oldhpmax = you.hp_max;
        unrot_hp( 100 );
        if (you.hp_max > oldhpmax)
            did_restore = true;

        // If nothing happened, don't take one max MP, don't use a turn.
        if (!did_restore)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            return (false);
        }

        break;
    }

    case ABIL_DELAYED_FIREBALL:
        if ( !spell_direction(spd, beam, DIR_NONE, TARG_ENEMY) )
            return (false);

        // Note: power level of ball calculated at release -- bwr
        fireball( calc_spell_power( SPELL_DELAYED_FIREBALL, true ), beam );

        // only one allowed since this is instantaneous -- bwr
        you.attribute[ ATTR_DELAYED_FIREBALL ] = 0;
        break;

    case ABIL_SPIT_POISON:      // Naga + spit poison mutation
    {
        if (you.duration[DUR_BREATH_WEAPON])
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }

        const int pow = you.experience_level
                        + player_mutation_level(MUT_SPIT_POISON) * 5
                        + (you.species == SP_NAGA) * 10;

        if (!spell_direction(abild, beam)
            || !player_tracer(ZAP_SPIT_POISON, pow, beam))
        {
            return (false);
        }
        else
        {
            zapping(ZAP_SPIT_POISON, pow, beam);

            you.duration[DUR_BREATH_WEAPON] = 3 + random2(5);
        }
        break;
    }
    case ABIL_EVOKE_MAPPING:    // randarts
    case ABIL_MAPPING:          // Gnome + sense surrounds mut
        if (abil.ability == ABIL_MAPPING
            && player_mutation_level(MUT_MAPPING) < 3
            && (you.level_type == LEVEL_PANDEMONIUM
                || !player_in_mappable_area()))
        {
            mpr("You feel momentarily disoriented.");
            return (false);
        }

        power = (abil.ability == ABIL_EVOKE_MAPPING) ?
            you.skills[SK_EVOCATIONS] : you.experience_level;

        if ( magic_mapping(  3 + roll_dice( 2,
                             (abil.ability == ABIL_EVOKE_MAPPING) ? power :
                             power + player_mutation_level(MUT_MAPPING) * 10),
                             40 + roll_dice( 2, power), true) )
        {
            mpr("You sense your surroundings.");
        }
        else
            mpr("You feel momentarily disoriented.");

        if (abil.ability == ABIL_EVOKE_MAPPING)
            exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_EVOKE_TELEPORTATION:    // ring of teleportation
    case ABIL_TELEPORTATION:          // teleport mut
        if (player_mutation_level(MUT_TELEPORT_AT_WILL) == 3)
            you_teleport_now( true, true ); // instant and to new area of Abyss
        else
            you_teleport();

        if (abil.ability == ABIL_EVOKE_TELEPORTATION)
            exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_BREATHE_FIRE:
    case ABIL_BREATHE_FROST:
    case ABIL_BREATHE_POISON:
    case ABIL_BREATHE_LIGHTNING:
    case ABIL_SPIT_ACID:
    case ABIL_BREATHE_POWER:
    case ABIL_BREATHE_STICKY_FLAME:
    case ABIL_BREATHE_STEAM:
        if (you.duration[DUR_BREATH_WEAPON]
            && abil.ability != ABIL_SPIT_ACID)
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }
        else if (!spell_direction(abild, beam))
            return (false);

        switch (abil.ability)
        {
        case ABIL_BREATHE_FIRE:
            // Don't check for hell serpents - they get hell fire,
            // never regular fire. (GDL)
            power = you.experience_level;
            power += player_mutation_level(MUT_BREATHE_FLAMES) * 4;

            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON)
                power += 12;

            snprintf(info, INFO_SIZE, "You breathe fire%c",
                     (power < 15) ? '.':'!');

            if (!zapping(ZAP_BREATHE_FIRE, power, beam, true, info))
                return (false);
            break;

        case ABIL_BREATHE_FROST:
            if (!zapping(ZAP_BREATHE_FROST, you.experience_level, beam, true,
                         "You exhale a wave of freezing cold."))
            {
                return (false);
            }
            break;

        case ABIL_BREATHE_POISON:
            if (!zapping(ZAP_BREATHE_POISON, you.experience_level, beam, true,
                         "You exhale a blast of poison gas."))
            {
                return (false);
            }
            break;

        case ABIL_BREATHE_LIGHTNING:
            if (!zapping(ZAP_LIGHTNING, (you.experience_level * 2), beam, true,
                         "You spit a bolt of lightning."))
            {
                return (false);
            }
            break;

        case ABIL_SPIT_ACID:
            if (!zapping(ZAP_BREATHE_ACID, you.experience_level, beam, true,
                         "You spit acid."))
            {
                return (false);
            }
            break;

        case ABIL_BREATHE_POWER:
            if (!zapping(ZAP_BREATHE_POWER, you.experience_level, beam, true,
                         "You spit a bolt of incandescent energy."))
            {
                return (false);
            }
            break;

        case ABIL_BREATHE_STICKY_FLAME:
            if (!zapping(ZAP_STICKY_FLAME, you.experience_level, beam, true,
                         "You spit a glob of burning liquid."))
            {
                return (false);
            }
            break;

        case ABIL_BREATHE_STEAM:
            if (!zapping(ZAP_BREATHE_STEAM, you.experience_level, beam, true,
                         "You exhale a blast of scalding steam."))
            {
                return (false);
            }
            break;

        default:
            break;
        }

        if (abil.ability != ABIL_SPIT_ACID)
        {
            you.duration[DUR_BREATH_WEAPON] =
                3 + random2(4) + random2(30 - you.experience_level) / 2;

            if (abil.ability == ABIL_BREATHE_STEAM)
                you.duration[DUR_BREATH_WEAPON] /= 2;
        }
        break;

    case ABIL_EVOKE_BLINK:      // randarts
    case ABIL_BLINK:            // mutation
        random_blink(true);

        if (abil.ability == ABIL_EVOKE_BLINK)
            exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_EVOKE_BERSERK:    // amulet of rage, randarts
        // FIXME This is not the best way to do stuff, because
        // you have a chance of failing the evocation test, in
        // which case you'll lose MP, etc.
        // We should add a pre-failure-test check in general.
        if (you.hunger_state < HS_SATIATED)
        {
            mpr("You're too hungry to berserk.");
            return (false);
        }
        else if ( !you.can_go_berserk(true) )
            return (false);

        // only exercise if berserk succeeds
        // because of the test above, this should always happen,
        // but I'm leaving it in -- haranp
        if ( go_berserk(true) )
            exercise( SK_EVOCATIONS, 1 );
        break;

    // Fly (kenku) -- eventually becomes permanent (see acr.cc).
    case ABIL_FLY:
        cast_fly( you.experience_level * 4 );

        if (you.experience_level > 14)
            mpr("You feel very comfortable in the air.");
        break;

    // Fly (Draconians, or anything else with wings)
    case ABIL_FLY_II:
        if (you.duration[DUR_EXHAUSTED])
        {
            mpr("You're too exhausted to fly.");
            return (false);
        }
        else if (you.burden_state != BS_UNENCUMBERED)
        {
            mpr("You're carrying too much weight to fly.");
            return (false);
        }
        else
        {
            cast_fly( you.experience_level * 2 );
        }
        break;

    // DEMONIC POWERS:
    case ABIL_SUMMON_MINOR_DEMON:
        summon_lesser_demon(you.experience_level * 4);
        break;

    case ABIL_SUMMON_DEMON:
        summon_common_demon(you.experience_level * 4);
        break;

    case ABIL_HELLFIRE:
        if (your_spells(SPELL_HELLFIRE,
                        20 + you.experience_level, false) == SPRET_ABORT)
            return (false);
        break;

    case ABIL_TORMENT:
        if (you.is_undead)
        {
            mpr("The unliving cannot use this ability.");
            return (false);
        }

        torment(TORMENT_GENERIC, you.x_pos, you.y_pos);
        break;

    case ABIL_RAISE_DEAD:
        animate_dead(&you, you.experience_level * 5, BEH_FRIENDLY,
                     you.pet_target);
        break;

    case ABIL_CONTROL_DEMON:
        if (!spell_direction(abild, beam)
            || !zapping(ZAP_CONTROL_DEMON, you.experience_level * 5, beam,
                        true))
        {
            return (false);
        }
        break;

    case ABIL_TO_PANDEMONIUM:
        if (you.level_type == LEVEL_PANDEMONIUM)
        {
            mpr("You're already here.");
            return (false);
        }

        banished(DNGN_ENTER_PANDEMONIUM, "self");
        break;

    case ABIL_CHANNELING:
        mpr("You channel some magical energy.");
        inc_mp(1 + random2(5), false);
        break;

    case ABIL_THROW_FLAME:
    case ABIL_THROW_FROST:
        if (!spell_direction(abild, beam))
            return (false);

        if (!zapping((abil.ability == ABIL_THROW_FLAME ? ZAP_FLAME : ZAP_FROST),
                     you.experience_level * 3, beam, true))
        {
            return (false);
        }
        break;

    case ABIL_BOLT_OF_DRAINING:
        if (!spell_direction(abild, beam))
            return (false);

        if (!zapping(ZAP_NEGATIVE_ENERGY, you.experience_level * 6, beam, true))
            return (false);
        break;

    case ABIL_EVOKE_TURN_INVISIBLE:     // ring, randarts, darkness items
        if (you.hunger_state < HS_SATIATED)
        {
            mpr("You're too hungry to turn invisible.");
            return (false);
        }

        potion_effect( POT_INVISIBILITY, 2 * you.skills[SK_EVOCATIONS] + 5 );
        contaminate_player( 1 + random2(3), true );
        exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_EVOKE_TURN_VISIBLE:
        mpr("You feel less transparent.");
        you.duration[DUR_INVIS] = 1;
        break;

    case ABIL_EVOKE_LEVITATE:           // ring, boots, randarts
        potion_effect( POT_LEVITATION, 2 * you.skills[SK_EVOCATIONS] + 30 );
        exercise( SK_EVOCATIONS, 1 );
        break;

    case ABIL_EVOKE_STOP_LEVITATING:
        mpr("You feel heavy.");
        you.duration[DUR_LEVITATION] = 1;
        break;

    case ABIL_END_TRANSFORMATION:
        mpr("You feel almost normal.");
        you.duration[DUR_TRANSFORMATION] = 2;
        break;

    // INVOCATIONS:

    case ABIL_ZIN_RECITE:
    {
        int result = check_recital_audience();
        if (result < 0)
        {
            mpr("There's no appreciative audience!");
            return (false);
        }
        else if (result < 1)
        {
            mpr("There's no-one here to preach to!");
            return (false);
        }
        // up to (60 + 40)/2 = 50
        const int pow = ( 2*skill_bump(SK_INVOCATIONS) + you.piety / 5 ) / 2;
        start_delay(DELAY_RECITE, 3, pow, you.hp);

        exercise(SK_INVOCATIONS, 2);
        break;
    }

    case ABIL_ZIN_VITALISATION:
    {
        int result = cast_vitalisation(1 + (you.skills[SK_INVOCATIONS] / 4));
        if (result > 0)
            exercise(SK_INVOCATIONS, 2 + random2(result));
        break;
    }

    case ABIL_ZIN_SANCTUARY:
        if (cast_sanctuary(you.skills[SK_INVOCATIONS] * 4))
            exercise(SK_INVOCATIONS, 5 + random2(8));
        break;

    case ABIL_TSO_DIVINE_SHIELD:
        cast_divine_shield();
        exercise(SK_INVOCATIONS, (coinflip() ? 3 : 2));
        break;

    case ABIL_TSO_CLEANSING_FLAME:
        if (!spell_direction(spd, beam))
            return (false);

        if (!zapping(ZAP_CLEANSING_FLAME, 20 + you.skills[SK_INVOCATIONS] * 6,
                     beam, true))
        {
            return (false);
        }
        exercise(SK_INVOCATIONS, 3 + random2(6));
        break;

    case ABIL_TSO_SUMMON_DAEVA:
        summon_daeva(you.skills[SK_INVOCATIONS] * 4, GOD_SHINING_ONE);
        exercise(SK_INVOCATIONS, 8 + random2(10));
        break;

    case ABIL_KIKU_RECALL_UNDEAD_SLAVES:
        recall(1);
        exercise(SK_INVOCATIONS, 1);
        break;

    case ABIL_KIKU_ENSLAVE_UNDEAD:
        if (!spell_direction(spd, beam))
            return (false);

        if (!zapping(ZAP_ENSLAVE_UNDEAD, you.skills[SK_INVOCATIONS] * 8, beam,
                     true))
        {
            return (false);
        }
        exercise(SK_INVOCATIONS, 5 + random2(5));
        break;

    case ABIL_KIKU_INVOKE_DEATH:
        summon_demon_type(MONS_REAPER,
                          20 + you.skills[SK_INVOCATIONS] * 3,
                          GOD_KIKUBAAQUDGHA);
        exercise(SK_INVOCATIONS, 10 + random2(14));
        break;

    case ABIL_YRED_ANIMATE_CORPSE:
        mpr("You call on the dead to walk for you...");
        animate_a_corpse(you.pos(), CORPSE_BODY, BEH_FRIENDLY, you.pet_target,
                         GOD_YREDELEMNUL);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_RECALL_UNDEAD:
        recall(1);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_ANIMATE_DEAD:
        mpr("You call on the dead to walk for you...");
        animate_dead(&you, 1 + you.skills[SK_INVOCATIONS], BEH_FRIENDLY,
                     you.pet_target, GOD_YREDELEMNUL);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_DRAIN_LIFE:
        drain_life( you.skills[SK_INVOCATIONS] );
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_YRED_CONTROL_UNDEAD:
        mass_enchantment( ENCH_CHARM, you.skills[SK_INVOCATIONS] * 8, MHITYOU );
        exercise(SK_INVOCATIONS, 3 + random2(4));
        break;

    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
        mpr("You channel some magical energy.");

        inc_mp(1 + random2(you.skills[SK_INVOCATIONS] / 4 + 2), false);
        exercise(SK_INVOCATIONS, 1 + random2(3));
        break;

    case ABIL_OKAWARU_MIGHT:
        potion_effect( POT_MIGHT, you.skills[SK_INVOCATIONS] * 8 );
        exercise(SK_INVOCATIONS, 1 + random2(3));
        break;

    case ABIL_OKAWARU_HASTE:
        potion_effect( POT_SPEED, you.skills[SK_INVOCATIONS] * 8 );
        exercise(SK_INVOCATIONS, 3 + random2(7));
        break;

    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
        if (!spell_direction(spd, beam))
            return (false);

        power = you.skills[SK_INVOCATIONS]
                    + random2( 1 + you.skills[SK_INVOCATIONS] )
                    + random2( 1 + you.skills[SK_INVOCATIONS] );

        // Since the actual beam is random, check with BEAM_MMISSILE and the
        // highest range possible (electricity).
        if (!player_tracer(ZAP_DEBUGGING_RAY, power, beam, 13))
            return (false);

        switch (random2(5))
        {
        case 0: zapping( ZAP_FLAME,        power,     beam ); break;
        case 1: zapping( ZAP_PAIN,         power,     beam ); break;
        case 2: zapping( ZAP_STONE_ARROW,  power,     beam ); break;
        case 3: zapping( ZAP_ELECTRICITY,  power,     beam ); break;
        case 4: zapping( ZAP_BREATHE_ACID, power / 2, beam ); break;
        }

        exercise(SK_INVOCATIONS, 1);
        break;

    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
        summon_demon_type(static_cast<monster_type>(MONS_NEQOXEC + random2(5)),
                          20 + you.skills[SK_INVOCATIONS] * 3, GOD_MAKHLEB);
        exercise(SK_INVOCATIONS, 2 + random2(3));
        break;

    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
        if (!spell_direction(spd, beam))
            return (false);

        power = you.skills[SK_INVOCATIONS] * 3
                + random2( 1 + you.skills[SK_INVOCATIONS] )
                + random2( 1 + you.skills[SK_INVOCATIONS] );

        // Since the actual beam is random, check with BEAM_MMISSILE and the
        // highest range possible (orb of electricity).
        if (!player_tracer(ZAP_DEBUGGING_RAY, power, beam, 20))
            return (false);

        switch (random2(8))
        {
        case 0: zapping( ZAP_FIRE,               power, beam ); break;
        case 1: zapping( ZAP_FIREBALL,           power, beam ); break;
        case 2: zapping( ZAP_LIGHTNING,          power, beam ); break;
        case 3: zapping( ZAP_NEGATIVE_ENERGY,    power, beam ); break;
        case 4: zapping( ZAP_STICKY_FLAME,       power, beam ); break;
        case 5: zapping( ZAP_IRON_BOLT,          power, beam ); break;
        case 6: zapping( ZAP_ORB_OF_ELECTRICITY, power, beam ); break;

        case 7:
            you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] = 1;
            simple_god_message(" hurls a blast of lightning!", GOD_MAKHLEB);

            // Make a divine lightning bolt, and fire!
            beam.beam_source = NON_MONSTER;
            beam.type        = dchar_glyph(DCHAR_FIRED_BURST);
            beam.damage      = dice_def(3, 30);
            beam.flavour     = BEAM_ELECTRICITY;
            beam.target_x    = you.x_pos;
            beam.target_y    = you.y_pos;
            beam.name        = "blast of lightning";
            beam.colour      = LIGHTCYAN;
            beam.thrower     = KILL_YOU;
            beam.aux_source  = "Makhleb's lightning strike";
            beam.ex_size     = 1 + you.skills[SK_INVOCATIONS] / 8;
            beam.is_tracer   = false;
            beam.is_explosion = true;
            explosion(beam);

            // protection down
            mpr("Your divine protection wanes.");
            you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION] = 0;
            break;
        }

        exercise(SK_INVOCATIONS, 3 + random2(5));
        break;

    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
        summon_demon_type(static_cast<monster_type>(MONS_EXECUTIONER + random2(5)),
                          20 + you.skills[SK_INVOCATIONS] * 3, GOD_MAKHLEB);
        exercise(SK_INVOCATIONS, 6 + random2(6));
        break;

    case ABIL_TROG_BURN_BOOKS:
        if (!trog_burn_books())
            return (false);
        break;

    case ABIL_TROG_BERSERK:
        // Trog abilities don't use or train invocations.
        if (you.hunger_state < HS_SATIATED)
        {
            mpr("You're too hungry to berserk.");
            return (false);
        }

        go_berserk(true);
        break;

    case ABIL_TROG_REGENERATION:
        // Trog abilities don't use or train invocations.
        cast_regen(you.piety/2);
        break;

    case ABIL_TROG_BROTHERS_IN_ARMS:
        // Trog abilities don't use or train invocations.
        summon_berserker(you.piety +
                         random2(you.piety/4) - random2(you.piety/4),
                         GOD_TROG);
        break;

    case ABIL_SIF_MUNA_FORGET_SPELL:
        cast_selective_amnesia(true);
        break;

    case ABIL_ELYVILON_DESTROY_WEAPONS:
        if (!ely_destroy_weapons())
            return (false);
        break;

    case ABIL_ELYVILON_LESSER_HEALING:
        if (!cast_healing( 3 + (you.skills[SK_INVOCATIONS] / 6) ))
            return (false);

        exercise( SK_INVOCATIONS, 1 );
        break;

    case ABIL_ELYVILON_PURIFICATION:
        purification();
        exercise( SK_INVOCATIONS, 2 + random2(3) );
        break;

    case ABIL_ELYVILON_HEALING:
        if (!cast_healing( 10 + (you.skills[SK_INVOCATIONS] / 3) ))
            return (false);

        exercise( SK_INVOCATIONS, 3 + random2(5) );
        break;

    case ABIL_ELYVILON_RESTORATION:
        restore_stat( STAT_ALL, 0, false );
        unrot_hp( 100 );

        exercise( SK_INVOCATIONS, 4 + random2(6) );
        break;

    case ABIL_ELYVILON_GREATER_HEALING:
        if (!cast_healing( 20 + you.skills[SK_INVOCATIONS] * 2 ))
            return (false);

        exercise( SK_INVOCATIONS, 6 + random2(10) );
        break;

    case ABIL_LUGONU_ABYSS_EXIT:
        if (you.level_type != LEVEL_ABYSS)
        {
            mpr("You aren't in the Abyss!");
            return (false);       // Don't incur costs.
        }
        banished(DNGN_EXIT_ABYSS);
        exercise(SK_INVOCATIONS, 8 + random2(10));
        break;

    case ABIL_LUGONU_BEND_SPACE:
        _lugonu_bends_space();
        exercise(SK_INVOCATIONS, 2 + random2(3));
        break;

    case ABIL_LUGONU_BANISH:
        if (!spell_direction(spd, beam, DIR_NONE, TARG_ENEMY))
            return (false);

        if (beam.target_x == you.x_pos && beam.target_y == you.y_pos)
        {
            mpr("You cannot banish yourself!");
            return (false);
        }
        if (!zapping(ZAP_BANISHMENT, 16 + you.skills[SK_INVOCATIONS] * 8, beam,
                     true))
        {
            return (false);
        }
        exercise(SK_INVOCATIONS, 3 + random2(5));
        break;

    case ABIL_LUGONU_CORRUPT:
        if (!lugonu_corrupt_level(300 + you.skills[SK_INVOCATIONS] * 15))
            return (false);
        exercise(SK_INVOCATIONS, 5 + random2(5));
        break;

    case ABIL_LUGONU_ABYSS_ENTER:
    {
        if (you.level_type == LEVEL_ABYSS)
        {
            mpr("You're already here.");
            return (false);
        }
        else if (you.level_type == LEVEL_PANDEMONIUM)
        {
            mpr("That doesn't work from Pandemonium.");
            return (false);
        }

        // Move permanent hp/mp loss from leaving to entering the Abyss. (jpeg)
        const int maxloss = std::max(2, div_rand_round(you.hp_max, 30));
        // Lose permanent HP
        dec_max_hp(random_range(1, maxloss));

        // Paranoia.
        if (you.hp_max < 1)
            you.hp_max = 1;

        // Deflate HP.
        set_hp( 1 + random2(you.hp), false );

        // Lose 1d2 permanent MP.
        rot_mp(coinflip() ? 2 : 1);

        // Deflate MP.
        if (you.magic_points)
            set_mp(random2(you.magic_points), false);

        bool note_status = notes_are_active();
        activate_notes(false);  // This banishment shouldn't be noted.
        banished(DNGN_ENTER_ABYSS);
        activate_notes(note_status);
        break;
    }
    case ABIL_NEMELEX_DRAW_ONE:
        if (!choose_deck_and_draw())
            return (false);
        exercise(SK_EVOCATIONS, 1 + random2(2));
        break;

    case ABIL_NEMELEX_PEEK_TWO:
        if (!deck_peek())
            return (false);
        exercise(SK_EVOCATIONS, 2 + random2(2));
        break;

    case ABIL_NEMELEX_TRIPLE_DRAW:
        if (!deck_triple_draw())
            return (false);
        exercise(SK_EVOCATIONS, 3 + random2(3));
        break;

    case ABIL_NEMELEX_MARK_FOUR:
        if (!deck_mark())
            return (false);
        exercise(SK_EVOCATIONS, 4 + random2(4));
        break;

    case ABIL_NEMELEX_STACK_FIVE:
        if (!deck_stack())
            return (false);
        exercise(SK_EVOCATIONS, 5 + random2(5));
        break;

    case ABIL_BEOGH_SMITING:
        if (your_spells( SPELL_SMITING, (2 + skill_bump(SK_INVOCATIONS)) * 6,
                         false ) == SPRET_ABORT)
        {
            return (false);
        }
        exercise( SK_INVOCATIONS, (coinflip()? 3 : 2) );
        break;

    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
        recall(2);
        exercise(SK_INVOCATIONS, 1);
        break;

    //jmf: intended as invocations from evil god(s):
    case ABIL_CHARM_SNAKE:
        cast_snake_charm( you.experience_level * 2
                            + you.skills[SK_INVOCATIONS] * 3 );

        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_TRAN_SERPENT_OF_HELL:
        transform(10 + (you.experience_level * 2) +
                  (you.skills[SK_INVOCATIONS] * 3), TRAN_SERPENT_OF_HELL);

        exercise(SK_INVOCATIONS, 6 + random2(9));
        break;

    case ABIL_BREATHE_HELLFIRE:
        if (you.duration[DUR_BREATH_WEAPON])
        {
            canned_msg(MSG_CANNOT_DO_YET);
            return (false);
        }

        if (your_spells( SPELL_HELLFIRE,
                        20 + you.experience_level, false ) == SPRET_ABORT)
            return (false);

        you.duration[DUR_BREATH_WEAPON] +=
                        3 + random2(5) + random2(30 - you.experience_level);
        break;

    case ABIL_ROTTING:
        cast_rotting(you.experience_level * 2 + you.skills[SK_INVOCATIONS] * 3);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_TORMENT_II:
        if (you.is_undead)
        {
            mpr("The unliving cannot use this ability.");
            return (false);
        }

        torment(TORMENT_GENERIC, you.x_pos, you.y_pos);
        exercise(SK_INVOCATIONS, 2 + random2(4));
        break;

    case ABIL_TRAN_BAT:
        if (!transform(100, TRAN_BAT))
        {
            crawl_state.zero_turns_taken();
            return (false);
        }
        break;

    case ABIL_RENOUNCE_RELIGION:
        if (yesno("Really renounce your faith, foregoing its fabulous benefits?",
                  false, 'n')
            && yesno("Are you sure you won't change your mind later?",
                     false, 'n' ))
        {
            excommunication();
        }
        else
        {
            canned_msg(MSG_OK);
        }
        break;

    case ABIL_NON_ABILITY:
        mpr("Sorry, you can't do that.");
        break;
    }
    return (true);
}

static void _pay_ability_costs(const ability_def& abil)
{
    // currently only delayed fireball is instantaneous -- bwr
    you.turn_is_over = !(abil.flags & ABFLAG_INSTANT);

    const int food_cost  = abil.food_cost + random2avg(abil.food_cost, 2);
    const int piety_cost = abil.piety_cost.cost();

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Cost: mp=%d; hp=%d; food=%d; piety=%d",
         abil.mp_cost, abil.hp_cost, food_cost, piety_cost );
#endif

    if (abil.mp_cost)
    {
        dec_mp( abil.mp_cost );
        if (abil.flags & ABFLAG_PERMANENT_MP)
            rot_mp(1);
    }

    if (abil.hp_cost)
    {
        dec_hp( abil.hp_cost, false );
        if (abil.flags & ABFLAG_PERMANENT_HP)
            rot_hp(1);
    }
   
    if (abil.flags & ABFLAG_TWO_XP)
    {
        you.exp_available -= 2; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_TEN_XP)
    {
        you.exp_available -= 10; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_HUNDRED_XP)
    {
        you.exp_available -= 100; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_FIFTY_XP)
    {
        you.exp_available -= 50; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_THOUSAND_XP)
    {
        you.exp_available -= 1000; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_THREE_THOUSAND_XP)
    {
        you.exp_available -= 3000; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_TEN_THOUSAND_XP)
    {
        you.exp_available -= 10000; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_THIRTY_XP)
    {
        you.exp_available -= 30; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_THREE_HUNDRED_XP)
    {
        you.exp_available -= 300; // //
        you.redraw_experience = true;
    }
    if (abil.flags & ABFLAG_ENCH_MISCAST)
    {
        miscast_effect( SPTYP_ENCHANTMENT, 10, 90, 100, "power out of control");
    }
    if (abil.flags & ABFLAG_NECRO_MISCAST)
    {
        miscast_effect( SPTYP_NECROMANCY, 10, 90, 100, "power out of control");
    }
    if (abil.flags & ABFLAG_NECRO_MISCAST_MINOR)
    {
        miscast_effect( SPTYP_NECROMANCY, 5, 90, 100, "power out of control");
    }
    if (abil.flags & ABFLAG_TLOC_MISCAST)
    {
        miscast_effect( SPTYP_TRANSLOCATION, 10, 90, 100, "power out of control");
    }
    if (abil.flags & ABFLAG_TMIG_MISCAST)
    {
        miscast_effect( SPTYP_TRANSMIGRATION, 10, 90, 100, "power out of control");
    }
    if (abil.flags & ABFLAG_LEVEL_DRAIN)
    {
        lose_level();
        lose_level();
    }


    if (food_cost)
        make_hungry( food_cost, false, true );

    if (piety_cost)
        lose_piety( piety_cost );
}

int choose_ability_menu(const std::vector<talent>& talents, bool describe)
{
    Menu abil_menu(MF_SINGLESELECT | MF_ANYPRINTABLE, "ability");

    abil_menu.set_highlighter(NULL);
    abil_menu.set_title(
        new MenuEntry("  Ability                           "
                      "Cost                    Success"));

    if (describe)
    {
        abil_menu.set_more(formatted_string::parse_string(
                                "Choose any ability to read its description, "
                                "or exit the menu with Escape."));
        abil_menu.set_flags(MF_SINGLESELECT | MF_ANYPRINTABLE |
                            MF_ALWAYS_SHOW_MORE);
    }
    else if (Options.tutorial_left)
    {
        // XXX This could be buggy if you manage to pick up lots and lots
        // of abilities during the tutorial.
        abil_menu.set_more(tut_abilities_info());
        abil_menu.set_flags(MF_SINGLESELECT | MF_ANYPRINTABLE |
                            MF_ALWAYS_SHOW_MORE);
    }

    int numbers[52];
    for (int i = 0; i < 52; ++i)
        numbers[i] = i;

    bool found_invocations = false;

    // First add all non-invocations.
    for (unsigned int i = 0; i < talents.size(); ++i)
    {
        if (talents[i].is_invocation)
            found_invocations = true;
        else
        {
            MenuEntry* me = new MenuEntry(_describe_talent(talents[i]),
                                          MEL_ITEM, 1, talents[i].hotkey);
            me->data = reinterpret_cast<void*>(numbers+i);
            abil_menu.add_entry(me);
        }
    }

    if (found_invocations)
    {
        abil_menu.add_entry(new MenuEntry("    Invocations - ", MEL_SUBTITLE));
        for (unsigned int i = 0; i < talents.size(); ++i)
        {
            if (talents[i].is_invocation)
            {
                MenuEntry* me = new MenuEntry(_describe_talent(talents[i]),
                                              MEL_ITEM, 1, talents[i].hotkey);
                me->data = reinterpret_cast<void*>(numbers+i);
                abil_menu.add_entry(me);
            }
        }
    }

    std::vector<MenuEntry*> sel = abil_menu.show(false);
    redraw_screen();
    if (sel.empty())
    {
        return -1;
    }
    else
    {
        ASSERT(sel.size() == 1);
        ASSERT(sel[0]->hotkeys.size() == 1);
        return (*(reinterpret_cast<int*>(sel[0]->data)));
    }
}

const char* ability_name(ability_type ability)
{
    return get_ability_def(ability).name;
}

static std::string _describe_talent(const talent& tal)
{
    ASSERT( tal.which != ABIL_NON_ABILITY );

    std::ostringstream desc;
    desc << std::left
         << std::setw(32) << ability_name(tal.which)
         << std::setw(24) << make_cost_description(tal.which)
         << std::setw(10) << failure_rate_to_string(tal.fail);
    return desc.str();
}

static void _add_talent(std::vector<talent>& vec, const ability_type ability,
                        bool check_confused)
{
    const talent t = _get_talent(ability, check_confused);
    if (t.which != ABIL_NON_ABILITY)
        vec.push_back(t);
}

std::vector<talent> your_talents( bool check_confused )
{
    std::vector<talent> talents;
    // // zot defense abilities; must also be updated in player.cc when these levels are changed
    if (you.experience_level >= 12)
        _add_talent(talents, ABIL_MAKE_FUNGUS, check_confused);
    if (you.experience_level >= 4)
        _add_talent(talents, ABIL_MAKE_PLANT, check_confused);
    if (you.experience_level >= 2)
        _add_talent(talents, ABIL_MAKE_OKLOB_PLANT, check_confused);
    if (you.experience_level >= 1)
        _add_talent(talents, ABIL_MAKE_DART_TRAP, check_confused);
    if (you.experience_level >= 8)
        _add_talent(talents, ABIL_MAKE_ICE_STATUE, check_confused);
    if (you.experience_level >= 14)
        _add_talent(talents, ABIL_MAKE_OCS, check_confused);
    if (you.experience_level >= 20)
        _add_talent(talents, ABIL_MAKE_SILVER_STATUE, check_confused);
    if (you.experience_level >= 26)
        _add_talent(talents, ABIL_MAKE_CURSE_SKULL, check_confused);
    if (you.experience_level >= 16)
        _add_talent(talents, ABIL_MAKE_TELEPORT, check_confused);
    if (you.experience_level >= 3)
        _add_talent(talents, ABIL_MAKE_ARROW_TRAP, check_confused);
    if (you.experience_level >= 18)
        _add_talent(talents, ABIL_MAKE_BOLT_TRAP, check_confused);
    if (you.experience_level >= 9)
        _add_talent(talents, ABIL_MAKE_SPEAR_TRAP, check_confused);
    if (you.experience_level >= 13)
        _add_talent(talents, ABIL_MAKE_AXE_TRAP, check_confused);
    if (you.experience_level >= 15)
        _add_talent(talents, ABIL_MAKE_NEEDLE_TRAP, check_confused);
    if (you.experience_level >= 7)
        _add_talent(talents, ABIL_MAKE_NET_TRAP, check_confused);
    if (you.experience_level >= 27)
        _add_talent(talents, ABIL_MAKE_TELEPORT_TRAP, check_confused);
    if (you.experience_level >= 11)
        _add_talent(talents, ABIL_MAKE_ALARM_TRAP, check_confused);
    if (you.experience_level >= 24)
        _add_talent(talents, ABIL_MAKE_BLADE_TRAP, check_confused);
    if (you.experience_level >= 22)
        _add_talent(talents, ABIL_MAKE_OKLOB_CIRCLE, check_confused);
    if (you.experience_level >= 10)
        _add_talent(talents, ABIL_MAKE_ACQUIRE_GOLD, check_confused);
    if (you.experience_level >= 25)
        _add_talent(talents, ABIL_MAKE_ACQUIREMENT, check_confused);
    if (you.experience_level >= 17)
        _add_talent(talents, ABIL_MAKE_WATER, check_confused);
    if (you.experience_level >= 19)
        _add_talent(talents, ABIL_MAKE_ELECTRIC_EEL, check_confused);
    if (you.experience_level >= 5)
        _add_talent(talents, ABIL_MAKE_ALTAR, check_confused);
    if (you.experience_level >= 21)
        _add_talent(talents, ABIL_MAKE_BAZAAR, check_confused);
    if (you.experience_level >= 6)
        _add_talent(talents, ABIL_MAKE_GRENADES, check_confused);
    if (you.experience_level >= 23)
        _add_talent(talents, ABIL_MAKE_SAGE, check_confused);


  
    // Species-based abilities
    if (you.species == SP_MUMMY && you.experience_level >= 13)
        _add_talent(talents, ABIL_MUMMY_RESTORATION, check_confused);

    if (you.species == SP_NAGA)
    {
        _add_talent(talents, player_mutation_level(MUT_BREATHE_POISON) ?
                    ABIL_BREATHE_POISON : ABIL_SPIT_POISON, check_confused);
    }
    else if (player_mutation_level(MUT_SPIT_POISON))
        _add_talent(talents, ABIL_SPIT_POISON, check_confused );

    if (player_genus(GENPC_DRACONIAN))
    {
        if (you.experience_level >= 7)
        {
            const ability_type ability = (
                (you.species == SP_GREEN_DRACONIAN)  ? ABIL_BREATHE_POISON :
                (you.species == SP_RED_DRACONIAN)    ? ABIL_BREATHE_FIRE :
                (you.species == SP_WHITE_DRACONIAN)  ? ABIL_BREATHE_FROST :
                (you.species == SP_YELLOW_DRACONIAN) ? ABIL_SPIT_ACID :
                (you.species == SP_BLACK_DRACONIAN)  ? ABIL_BREATHE_LIGHTNING :
                (you.species == SP_PURPLE_DRACONIAN) ? ABIL_BREATHE_POWER :
                (you.species == SP_PALE_DRACONIAN)   ? ABIL_BREATHE_STEAM :
                (you.species == SP_MOTTLED_DRACONIAN)? ABIL_BREATHE_STICKY_FLAME
                                                     : ABIL_NON_ABILITY);
            if (ability != ABIL_NON_ABILITY)
                _add_talent(talents, ability, check_confused );
        }
    }

    if (you.species == SP_VAMPIRE && you.experience_level >= 3
        && you.hunger_state <= HS_SATIATED
        && you.attribute[ATTR_TRANSFORMATION] != TRAN_BAT)
    {
        _add_talent(talents, ABIL_TRAN_BAT, check_confused );
    }

    if (!player_is_airborne())
    {
        // Kenku can fly, but only from the ground
        // (until level 15, when it becomes permanent until revoked).
        // jmf: "upgrade" for draconians -- expensive flight
        if (you.species == SP_KENKU && you.experience_level >= 5)
            _add_talent(talents, ABIL_FLY, check_confused );
        else if (player_genus(GENPC_DRACONIAN)
                 && player_mutation_level(MUT_BIG_WINGS))
        {
            _add_talent(talents, ABIL_FLY_II, check_confused );
        }
    }

    // Mutations.
    if (player_mutation_level(MUT_MAPPING))
        _add_talent(talents, ABIL_MAPPING, check_confused );

    if (player_mutation_level(MUT_SUMMON_MINOR_DEMONS))
        _add_talent(talents, ABIL_SUMMON_MINOR_DEMON, check_confused );

    if (player_mutation_level(MUT_SUMMON_DEMONS))
        _add_talent(talents, ABIL_SUMMON_DEMON, check_confused );

    if (player_mutation_level(MUT_HURL_HELLFIRE))
        _add_talent(talents, ABIL_HELLFIRE, check_confused );

    if (player_mutation_level(MUT_CALL_TORMENT))
        _add_talent(talents, ABIL_TORMENT, check_confused );

    if (player_mutation_level(MUT_RAISE_DEAD))
        _add_talent(talents, ABIL_RAISE_DEAD, check_confused );

    if (player_mutation_level(MUT_CONTROL_DEMONS))
        _add_talent(talents, ABIL_CONTROL_DEMON, check_confused );

    if (player_mutation_level(MUT_PANDEMONIUM))
        _add_talent(talents, ABIL_TO_PANDEMONIUM, check_confused );

    if (player_mutation_level(MUT_CHANNEL_HELL))
        _add_talent(talents, ABIL_CHANNELING, check_confused );

    if (player_mutation_level(MUT_THROW_FLAMES))
        _add_talent(talents, ABIL_THROW_FLAME, check_confused );

    if (player_mutation_level(MUT_THROW_FROST))
        _add_talent(talents, ABIL_THROW_FROST, check_confused );

    if (player_mutation_level(MUT_SMITE))
        _add_talent(talents, ABIL_BOLT_OF_DRAINING, check_confused );

    if (you.duration[DUR_TRANSFORMATION])
        _add_talent(talents, ABIL_END_TRANSFORMATION, check_confused );

    if (player_mutation_level(MUT_BLINK))
        _add_talent(talents, ABIL_BLINK, check_confused );

    if (player_mutation_level(MUT_TELEPORT_AT_WILL))
        _add_talent(talents, ABIL_TELEPORTATION, check_confused );

    // Religious abilities.
    if (you.religion == GOD_ELYVILON)
        _add_talent(talents, ABIL_ELYVILON_DESTROY_WEAPONS, check_confused );
    else if (you.religion == GOD_TROG)
        _add_talent(talents, ABIL_TROG_BURN_BOOKS, check_confused );

    // Gods take abilities away until penance completed. -- bwr
    // God abilities generally don't work while silenced (they require
    // invoking the god), but Nemelex is an exception.
    if (!player_under_penance() && (!silenced(you.x_pos, you.y_pos)
                                    || you.religion == GOD_NEMELEX_XOBEH))
    {
        for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
        {
            if (you.piety >= piety_breakpoint(i))
            {
                const ability_type abil = god_abilities[you.religion][i];
                if (abil != ABIL_NON_ABILITY)
                    _add_talent(talents, abil, check_confused);
            }
        }
    }

    // And finally, the ability to opt-out of your faith {dlb}:
    if (you.religion != GOD_NO_GOD && !silenced( you.x_pos, you.y_pos ))
        _add_talent(talents, ABIL_RENOUNCE_RELIGION, check_confused );

    //jmf: Check for breath weapons -- they're exclusive of each other, I hope!
    //     Make better come ones first.
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_SERPENT_OF_HELL)
        _add_talent(talents, ABIL_BREATHE_HELLFIRE, check_confused );
    else if (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
             || player_mutation_level(MUT_BREATHE_FLAMES))
    {
        _add_talent(talents, ABIL_BREATHE_FIRE, check_confused );
    }

    // Checking for unreleased delayed Fireball.
    if (you.attribute[ ATTR_DELAYED_FIREBALL ])
        _add_talent(talents, ABIL_DELAYED_FIREBALL, check_confused );

    // Evocations from items.
    if (scan_randarts(RAP_BLINK))
        _add_talent(talents, ABIL_EVOKE_BLINK, check_confused );

    if (wearing_amulet(AMU_RAGE) || scan_randarts(RAP_BERSERK))
        _add_talent(talents, ABIL_EVOKE_BERSERK, check_confused );

    if (scan_randarts( RAP_MAPPING ))
        _add_talent(talents, ABIL_EVOKE_MAPPING, check_confused );

    if (player_equip( EQ_RINGS, RING_INVISIBILITY )
        || player_equip_ego_type( EQ_ALL_ARMOUR, SPARM_DARKNESS )
        || scan_randarts( RAP_INVISIBLE ))
    {
        // Now you can only turn invisibility off if you have an
        // activatable item.  Wands and potions will have to time
        // out. -- bwr
        if (you.duration[DUR_INVIS])
            _add_talent(talents, ABIL_EVOKE_TURN_VISIBLE, check_confused );
        else
            _add_talent(talents, ABIL_EVOKE_TURN_INVISIBLE, check_confused );
    }

    // jmf: "upgrade" for draconians -- expensive flight
    // Note: This ability only applies to this counter.
    if (player_equip( EQ_RINGS, RING_LEVITATION )
        || player_equip_ego_type( EQ_BOOTS, SPARM_LEVITATION )
        || scan_randarts( RAP_LEVITATE ))
    {
        // Has no effect on permanently flying Kenku.
        if (!you.permanent_flight() && you.flight_mode() != FL_FLY)
        {
            // Now you can only turn levitation off if you have an
            // activatable item.  Potions and miscast effects will
            // have to time out (this makes the miscast effect actually
            // a bit annoying). -- bwr
            _add_talent(talents, you.duration[DUR_LEVITATION] ?
                        ABIL_EVOKE_STOP_LEVITATING : ABIL_EVOKE_LEVITATE,
                        check_confused);
        }
    }

    if (player_equip( EQ_RINGS, RING_TELEPORTATION )
        || scan_randarts( RAP_CAN_TELEPORT ))
    {
        _add_talent(talents, ABIL_EVOKE_TELEPORTATION, check_confused );
    }

    // Find hotkeys for the non-hotkeyed talents.
    for (unsigned int i = 0; i < talents.size(); ++i)
    {
        // Skip preassigned hotkeys.
        if ( talents[i].hotkey != 0 )
            continue;

        // Try to find a free hotkey for i, starting from Z.
        for ( int k = 51; k >= 0; ++k )
        {
            const int kkey = index_to_letter(k);
            bool good_key = true;

            // Check that it doesn't conflict with other hotkeys.
            for ( unsigned int j = 0; j < talents.size(); ++j )
            {
                if ( talents[j].hotkey == kkey )
                {
                    good_key = false;
                    break;
                }
            }

            if (good_key)
            {
                talents[i].hotkey = k;
                you.ability_letter_table[k] = talents[i].which;
                break;
            }
        }
        // In theory, we could be left with an unreachable ability
        // here (if you have 53 or more abilities simultaneously).
    }

    return talents;
}

// Note: we're trying for a behaviour where the player gets
// to keep their assigned invocation slots if they get excommunicated
// and then rejoin (but if they spend time with another god we consider
// the old invocation slots void and erase them).  We also try to
// protect any bindings the character might have made into the
// traditional invocation slots (A-E and X). -- bwr
static void _set_god_ability_helper( ability_type abil, char letter )
{
    int i;
    const int index = letter_to_index( letter );

    for (i = 0; i < 52; i++)
        if (you.ability_letter_table[i] == abil)
            break;

    if (i == 52)    // Ability is not already assigned.
    {
        // If slot is unoccupied, move in.
        if (you.ability_letter_table[index] == ABIL_NON_ABILITY)
            you.ability_letter_table[index] = abil;
    }
}

// Return GOD_NO_GOD if it isn't a god ability, otherwise return
// the index of the god..
static int _is_god_ability(int abil)
{
    if (abil == ABIL_NON_ABILITY)
        return GOD_NO_GOD;

    for (int i = 0; i < MAX_NUM_GODS; ++i)
        for (int j = 0; j < MAX_GOD_ABILITIES; ++j)
        {
            if (god_abilities[i][j] == abil)
                return i;
        }

    return GOD_NO_GOD;
}

void set_god_ability_slots( void )
{
    ASSERT( you.religion != GOD_NO_GOD );

    int i;

    _set_god_ability_helper( ABIL_RENOUNCE_RELIGION, 'X' );

    // Clear out other god invocations.
    for (i = 0; i < 52; i++)
    {
        const int god = _is_god_ability(you.ability_letter_table[i]);
        if (god != GOD_NO_GOD && god != you.religion)
            you.ability_letter_table[i] = ABIL_NON_ABILITY;
    }

    // Finally, add in current god's invocations in traditional slots.
    int num = 0;
    for (i = 0; i < MAX_GOD_ABILITIES; ++i)
    {
        if (god_abilities[you.religion][i] != ABIL_NON_ABILITY)
        {
            _set_god_ability_helper(god_abilities[you.religion][i], 'a' + num);
            ++num;
        }
    }
}


// Returns an index (0-51) if successful, -1 if you should
// just use the next one.
static int _find_ability_slot( ability_type which_ability )
{
    for (int slot = 0; slot < 52; slot++)
        if (you.ability_letter_table[slot] == which_ability)
            return slot;

    // No requested slot, find new one and make it preferred.

    // Skip over a-e (invocations)
    for (int slot = 5; slot < 52; slot++)
    {
        if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
        {
            you.ability_letter_table[slot] = which_ability;
            return slot;
        }
    }

    // If we can't find anything else, try a-e.
    for (int slot = 4; slot >= 0; slot--)
    {
        if (you.ability_letter_table[slot] == ABIL_NON_ABILITY)
        {
            you.ability_letter_table[slot] = which_ability;
            return slot;
        }
    }

    // All letters are assigned.
    return -1;
}

////////////////////////////////////////////////////////////////////////////

static int _lugonu_warp_monster(int x, int y, int pow, int)
{
    if (!in_bounds(x, y) || mgrd[x][y] == NON_MONSTER)
        return (0);

    monsters &mon = menv[ mgrd[x][y] ];

    if (!mons_friendly(&mon))
        behaviour_event( &mon, ME_ANNOY, MHITYOU );

    if (check_mons_resist_magic(&mon, pow * 2))
    {
        mprf("%s %s.",
             mon.name(DESC_CAP_THE).c_str(), mons_resist_string(&mon));
        return (1);
    }

    const int damage = 1 + random2(pow / 6);
    if (mon.type == MONS_BLINK_FROG)
        mon.heal(damage, false);
    else if (!check_mons_resist_magic(&mon, pow))
    {
        mon.hurt(&you, damage);
        if (!mon.alive())
            return (1);
    }

    mon.blink();

    return (1);
}

static void _lugonu_warp_area(int pow)
{
    apply_area_around_square( _lugonu_warp_monster, you.x_pos, you.y_pos, pow );
}

static void _lugonu_bends_space()
{
    const int pow = 4 + skill_bump(SK_INVOCATIONS);
    const bool area_warp = random2(pow) > 9;

    mprf("Space bends %saround you!", area_warp? "sharply " : "");

    if (area_warp)
        _lugonu_warp_area(pow);

    random_blink(false, true);

    const int damage = roll_dice(1, 4);
    ouch(damage, 0, KILLED_BY_WILD_MAGIC, "a spatial distortion");
}

////////////////////////////////////////////////////////////////////////
// generic_cost

int generic_cost::cost() const
{
    return base + (add > 0 ? random2avg(add, rolls) : 0);
}
