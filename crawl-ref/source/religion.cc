/*
 *  File:       religion.cc
 *  Summary:    Misc religion related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "religion.h"

#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cmath>

#ifdef DOS
#include <dos.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "beam.h"
#include "chardump.h"
#include "cloud.h"
#include "database.h"
#include "debug.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "effects.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "Kills.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "monplace.h"
#include "monstuff.h"
#include "mstuff2.h"
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "shopping.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

#if DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS 1
#    define DEBUG_GIFTS       1
#    define DEBUG_SACRIFICE   1
#    define DEBUG_PIETY       1
#endif

#define PIETY_HYSTERESIS_LIMIT 1

// Item offering messages for the gods:
// & is replaced by "is" or "are" as appropriate for the item.
// % is replaced by "s" or "" as appropriate.
// <> and </> are replaced with colors.
// First message is if there's no piety gain; second is if piety gain is
// one; third is if piety gain is more than one.
enum piety_gain_t
{
    PIETY_NONE, PIETY_SOME, PIETY_LOTS,
    NUM_PIETY_GAIN
};

static const char *_Sacrifice_Messages[NUM_GODS][NUM_PIETY_GAIN] =
{
    // No god
    {
        " & eaten by a bored swarm of bugs.",
        " & eaten by a swarm of bugs.",
        " & eaten by a ravening swarm of bugs."
    },
    // Zin
    {
        " <>barely glow%</> and disappear%.",
        " <>glow% silver</> and disappear%.",
        " <>glow% blindingly silver</> and disappear%.",
    },
    // TSO
    {
        " faintly glow% and disappear%.",
        " glow% a golden colour and disappear%.",
        " glow% a brilliant golden colour and disappear%.",
    },
    // Kikubaaqudgha
    {
        " slowly rot% away.",
        " rot% away.",
        " rot% away in an instant.",
    },
    // Yredelemnul
    {
        " slowly crumble% to dust.",
        " crumble% to dust.",
        " turn% to dust in an instant.",
    },
    // Xom (no sacrifices)
    {
        " & eaten by a bored bug.",
        " & eaten by a bug.",
        " & eaten by a greedy bug.",
    },
    // Vehumet
    {
        " fade% into nothingness.",
        " burn% into nothingness.",
        " explode% into nothingness.",
    },
    // Okawaru
    {
        " slowly burn% to ash.",
        " & consumed by flame.",
        " & consumed in a burst of flame.",
    },
    // Makhleb
    {
        " disappear% without a sign.",
        " flare% red and disappear%.",
        " flare% blood-red and disappear%.",
    },
    // Sif Muna
    {
        " & gone without a glow.",
        " glow% faintly for a moment, and & gone.",
        " glow% for a moment, and & gone.",
    },
    // Trog
    {
        " & slowly consumed by flames.",
        " & consumed in a column of flame.",
        " & consumed in a roaring column of flame.",
    },
    // Nemelex
    {
        " disappear% without a glow.",
        " glow% slightly and disappear%.",
        " glow% with a rainbow of weird colours and disappear%.",
    },
    // Elyvilon (no sacrifices, but used for weapon destruction)
    {
        " barely shimmer% and break% into pieces.",
        " shimmer% and break% into pieces.",
        " shimmer% wildly and break% into pieces.",
    },
    // Lugonu
    {
        " & disappears into the void.",
        " & consumed by the void.",
        " & voraciously consumed by the void.",
    },
    // Beogh
    {
        " slowly crumble% into the ground.",
        " crumble% into the ground.",
        " disintegrate% into the ground.",
    }
};

const char* god_gain_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "recite Zin's Axioms of Law",
      "call upon Zin for vitalisation",
      "",
      "",
      "call upon Zin to create a sanctuary" },
    // TSO
    { "You and your allies can gain power from killing evil.",
      "call upon the Shining One for a divine shield",
      "",
      "hurl blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "recall your undead slaves",
      "Kikubaaqudgha is protecting you from some side-effects of death magic.",
      "permanently enslave the undead",
      "",
      "summon an emissary of Death" },
    // Yredelemnul
    { "animate remains",
      "recall your undead slaves",
      "animate legions of the dead",
      "drain ambient lifeforce",
      "enslave living souls" },
    // Xom
    { "", "", "", "", "" },
    // Vehumet
    { "gain magical power from killing",
      "Vehumet is aiding your destructive magics.",
      "Vehumet is shielding you from summoned creatures.",
      "",
      "" },
    // Okawaru
    { "give your body great, but temporary strength",
      "",
      "",
      "",
      "haste yourself" },
    // Makhleb
    { "gain power from killing",
      "harness Makhleb's destructive might",
      "summon a lesser servant of Makhleb",
      "hurl Makhleb's greater destruction",
      "summon a greater servant of Makhleb" },
    // Sif Muna
    { "tap ambient magical fields",
      "freely open your mind to new spells",
      "",
      "Sif Muna is protecting you from some side-effects of spellcasting.",
      "" },
    // Trog
    { "go berserk at will",
      "call upon Trog for regeneration",
      "",
      "call in reinforcements",
      "" },
    // Nemelex
    { "draw cards from decks in your inventory",
      "peek at two random cards from a deck",
      "choose one out of three cards",
      "mark four cards in a deck",
      "order the top five cards of a deck, losing the rest" },
    // Elyvilon
    { "call upon Elyvilon for lesser healing",
      "call upon Elyvilon for purification",
      "call upon Elyvilon for greater healing",
      "call upon Elyvilon to restore your abilities",
      "call upon Elyvilon for divine vigour" },
    // Lugonu
    { "depart the Abyss",
      "bend space around yourself",
      "banish your foes",
      "corrupt the fabric of space",
      "gate yourself to the Abyss - for a price" },
    // Beogh
    { "Beogh supports the use of orcish gear.",
      "smite your foes",
      "gain orcish followers",
      "recall your orcish followers",
      "walk on water" }
};

const char* god_lose_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "recite Zin's Axioms of Law",
      "call upon Zin for vitalisation",
      "",
      "",
      "call upon Zin to create a sanctuary" },
    // TSO
    { "You and your allies can no longer gain power from killing evil.",
      "call upon the Shining One for a divine shield",
      "",
      "hurl blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "recall your undead slaves",
      "Kikubaaqudgha is no longer shielding you from miscast death magic.",
      "permanently enslave the undead",
      "",
      "summon an emissary of Death" },
    // Yredelemnul
    { "animate remains",
      "recall your undead slaves",
      "animate legions of the dead",
      "drain ambient lifeforce",
      "enslave living souls" },
    // Xom
    { "", "", "", "", "" },
    // Vehumet
    { "gain magical power from killing",
      "Vehumet will no longer aid your destructive magics.",
      "Vehumet will no longer shield you from summoned creatures.",
      "",
      "" },
    // Okawaru
    { "give your body great, but temporary strength",
      "",
      "",
      "",
      "haste yourself" },
    // Makhleb
    { "gain power from killing",
      "harness Makhleb's destructive might",
      "summon a lesser servant of Makhleb",
      "hurl Makhleb's greater destruction",
      "summon a greater servant of Makhleb" },
    // Sif Muna
    { "tap ambient magical fields",
      "forget spells at will",
      "",
      "Sif Muna is no longer protecting you from miscast magic.",
      "" },
    // Trog
    { "go berserk at will",
      "call upon Trog for regeneration",
      "",
      "call in reinforcements",
      "" },
    // Nemelex
    { "draw cards from decks in your inventory",
      "peek at random cards",
      "choose one out of three cards",
      "mark decks",
      "stack decks" },
    // Elyvilon
    { "call upon Elyvilon for minor healing",
      "call upon Elyvilon for purification",
      "call upon Elyvilon for major healing",
      "call upon Elyvilon to restore your abilities",
      "call upon Elyvilon for divine vigour" },
    // Lugonu
    { "depart the Abyss at will",
      "bend space around yourself",
      "banish your foes",
      "corrupt the fabric of space",
      "gate yourself to the Abyss" },
    // Beogh
    { "Beogh no longer supports the use of orcish gear.",
      "smite your foes",
      "gain orcish followers",
      "recall your orcish followers",
      "walk on water" }
};

static bool _holy_beings_attitude_change();
static bool _evil_beings_attitude_change();
static bool _chaotic_beings_attitude_change();
static bool _magic_users_attitude_change();
static bool _yred_slaves_abandon_you();
static bool _beogh_followers_abandon_you();
static void _god_smites_you(god_type god, const char *message = NULL,
                            kill_method_type death_type = NUM_KILLBY);
static bool _beogh_idol_revenge();
static void _tso_blasts_cleansing_flame(const char *message = NULL);
static bool _tso_holy_revenge();
static void _altar_prayer();
static bool _god_likes_item(god_type god, const item_def& item);
static void _dock_piety(int piety_loss, int penance);
static bool _make_god_gifts_disappear(bool level_only = true);
static bool _make_holy_god_gifts_good_neutral(bool level_only = true);
static bool _make_god_gifts_hostile(bool level_only = true);
static void _print_sacrifice_message(god_type, const item_def &,
                                     piety_gain_t, bool = false);

typedef void (*delayed_callback)(const mgen_data &mg, int &midx, int placed);

static void _delayed_monster(const mgen_data &mg,
                             delayed_callback callback = NULL);
static void _delayed_monster_done(std::string success, std::string failure,
                                  delayed_callback callback = NULL);
static void _place_delayed_monsters();

/////////////////////////////////////////////////////////////////////
// god_conduct_trigger

god_conduct_trigger::god_conduct_trigger(
    conduct_type c, int pg, bool kn, const monsters *vict)
  : conduct(c), pgain(pg), known(kn), enabled(true), victim(NULL)
{
    if (vict)
    {
        victim.reset(new monsters);
        *(victim.get()) = *vict;
    }
}

void god_conduct_trigger::set(conduct_type c, int pg, bool kn,
                              const monsters *vict)
{
    conduct = c;
    pgain = pg;
    known = kn;
    victim.reset(NULL);
    if (vict)
    {
        victim.reset(new monsters);
        *victim.get() = *vict;
    }
}

god_conduct_trigger::~god_conduct_trigger()
{
    if (enabled && conduct != NUM_CONDUCTS)
        did_god_conduct(conduct, pgain, known, victim.get());
}

bool is_evil_god(god_type god)
{
    return (god == GOD_KIKUBAAQUDGHA
            || god == GOD_MAKHLEB
            || god == GOD_YREDELEMNUL
            || god == GOD_BEOGH
            || god == GOD_LUGONU);
}

bool is_good_god(god_type god)
{
    return (god == GOD_ZIN
            || god == GOD_SHINING_ONE
            || god == GOD_ELYVILON);
}

bool is_chaotic_god(god_type god)
{
    return (god == GOD_XOM
            || god == GOD_MAKHLEB
            || god == GOD_LUGONU);
}

bool is_priest_god(god_type god)
{
    return (god == GOD_ZIN
            || god == GOD_YREDELEMNUL
            || god == GOD_BEOGH);
}

std::string get_god_powers(god_type which_god)
{
    // Return early for the special cases.
    if (which_god == GOD_NO_GOD)
        return ("");

    std::string result = getLongDescription(god_name(which_god) + " powers");
    return (result);
}

std::string get_god_likes(god_type which_god, bool verbose)
{
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return ("");

    std::string text = god_name(which_god);
    std::vector<std::string> likes;
    std::vector<std::string> really_likes;

    // Unique/unusual piety gain methods first.
    switch (which_god)
    {
    case GOD_SIF_MUNA:
        likes.push_back("you train your various spell casting skills");
        break;

    case GOD_TROG:
        snprintf(info, INFO_SIZE, "you destroy spellbooks (especially ones "
                                  "you've never read)%s",
                 verbose ? " via the <w>a</w> command" : "");

        likes.push_back(info);
        break;

    case GOD_NEMELEX_XOBEH:
        snprintf(info, INFO_SIZE, "you draw unmarked cards and use up decks%s",
                 verbose ? " (by <w>w</w>ielding and e<w>v</w>oking them)"
                         : "");

        likes.push_back(info);
        break;

    case GOD_ELYVILON:
        snprintf(info, INFO_SIZE, "you destroy weapons (especially evil ones)%s",
                 verbose ? " via the <w>a</w> command (inscribe items with "
                           "<w>!D</w> to prevent their accidental destruction)"
                         : "");

        likes.push_back(info);
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        snprintf(info, INFO_SIZE, "you donate money%s",
                 verbose ? " (by praying at an altar)" : "");

        likes.push_back(info);
        break;

    case GOD_SHINING_ONE:
        snprintf(info, INFO_SIZE, "you sacrifice evil items%s",
                 verbose ? " (by dropping them on an altar and praying)" : "");

        likes.push_back(info);
        break;

    case GOD_BEOGH:
        snprintf(info, INFO_SIZE, "you bless dead orcs%s",
                 verbose ? " (by standing over their corpses and <w>p</w>raying)" : "");

        likes.push_back(info);
        break;

    case GOD_NEMELEX_XOBEH:
        snprintf(info, INFO_SIZE, "you sacrifice items%s",
                 verbose ? " (by standing over them and <w>p</w>raying)" : "");
        likes.push_back(info);
        break;

    default:
        break;
    }

    if (god_likes_butchery(which_god))
    {
        snprintf(info, INFO_SIZE, "you butcher corpses while praying%s",
                 verbose ? " (press <w>pc</w> to do so)" : "");

        likes.push_back(info);
    }

    switch (which_god)
    {
    case GOD_VEHUMET: case GOD_MAKHLEB: case GOD_LUGONU:
        likes.push_back("you or your allies kill living beings");
        break;

    case GOD_TROG:
        likes.push_back("you or your god-given allies kill living beings");
        break;

    case GOD_YREDELEMNUL:
        likes.push_back("you or your undead slaves kill living beings");
        break;

    case GOD_KIKUBAAQUDGHA:
        likes.push_back("you kill living beings");
        likes.push_back("your god-given allies kill living beings");
        likes.push_back("your undead slaves kill living beings");
        break;

    case GOD_BEOGH:
        likes.push_back("you or your allied orcs kill living beings");
        break;

    case GOD_OKAWARU:
        likes.push_back("you kill living beings");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_SHINING_ONE:
        likes.push_back("you or your allies kill living evil beings");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_SHINING_ONE: case GOD_VEHUMET: case GOD_MAKHLEB:
    case GOD_LUGONU:
        likes.push_back("you or your allies kill the undead");
        break;

    case GOD_BEOGH:
        likes.push_back("you or your allied orcs kill the undead");
        break;

    case GOD_OKAWARU:
        likes.push_back("you kill the undead");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_SHINING_ONE: case GOD_MAKHLEB:
        likes.push_back("you or your allies kill demons");
        break;

    case GOD_TROG:
        likes.push_back("you or your god-given allies kill demons");
        break;

    case GOD_BEOGH:
        likes.push_back("you or your allied orcs kill demons");
        break;

    case GOD_OKAWARU:
        likes.push_back("you kill demons");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_MAKHLEB: case GOD_LUGONU:
        likes.push_back("you or your allies kill holy beings");
        break;

    case GOD_YREDELEMNUL:
        likes.push_back("your undead slaves kill holy beings");
        break;

    case GOD_KIKUBAAQUDGHA:
        likes.push_back("you kill holy beings");
        likes.push_back("your god-given allies kill holy beings");
        likes.push_back("your undead slaves kill holy beings");
        break;

    case GOD_BEOGH:
        likes.push_back("you or your allied orcs kill holy beings");
        break;

    default:
        break;
    }

    // Especially appreciated kills.
    switch (which_god)
    {
    case GOD_ZIN:
        really_likes.push_back("you kill monsters which cause mutation or rotting");
        break;

    case GOD_YREDELEMNUL:
        really_likes.push_back("you kill holy beings");
        break;

    case GOD_BEOGH:
        really_likes.push_back("you kill the priests of other religions");
        break;

    case GOD_TROG:
        really_likes.push_back("you kill wizards and other users of magic");
        break;

    default:
        break;
    }

    if (likes.size() == 0 && really_likes.size() == 0)
        text += " %s doesn't like anything? This a bug; please report it.";
    else
    {
        text += " likes it when ";
        text += comma_separated_line(likes.begin(), likes.end());
        text += ".";

        if (really_likes.size() > 0)
        {
            text += " ";
            text += god_name(which_god);

            text += " especially likes it when ";
            text += comma_separated_line(really_likes.begin(),
                                         really_likes.end());
            text += ".";
        }
    }

    return (text);
}

std::string get_god_dislikes(god_type which_god, bool /*verbose*/)
{
    // Return early for the special cases.
    if (which_god == GOD_NO_GOD || which_god == GOD_XOM)
        return ("");

    std::vector<std::string> dislikes;

    if (god_hates_butchery(which_god))
        dislikes.push_back("you butcher corpses while praying");

    if (god_hates_cannibalism(which_god))
        dislikes.push_back("you perform cannibalism");

    if (is_good_god(which_god))
    {
        dislikes.push_back("you drink blood");
        dislikes.push_back("you use necromancy");
        dislikes.push_back("you use unholy magic or items");
        dislikes.push_back("you attack holy beings");
        dislikes.push_back("you or your allies kill holy beings");
        dislikes.push_back("you attack neutral beings");
    }

    switch (which_god)
    {
    case GOD_ZIN:     case GOD_SHINING_ONE:  case GOD_ELYVILON:
    case GOD_OKAWARU:
        dislikes.push_back("you attack allies");
        break;

    case GOD_BEOGH:
        dislikes.push_back("you attack allied orcs");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ELYVILON: case GOD_ZIN: case GOD_OKAWARU:
        dislikes.push_back("you allow allies to die");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        dislikes.push_back("you deliberately mutate yourself");
        dislikes.push_back("you polymorph monsters");
        dislikes.push_back("you eat the flesh of sentient beings");
        dislikes.push_back("you use weapons or missiles of chaos");
        break;

    case GOD_SHINING_ONE:
        dislikes.push_back("you poison monsters");
        dislikes.push_back("you attack intelligent monsters in an "
                           "unchivalric manner");
        break;

    case GOD_ELYVILON:
        dislikes.push_back("you kill living things while praying");
        break;

    case GOD_YREDELEMNUL:
        dislikes.push_back("you use holy magic or items");
        break;

    case GOD_TROG:
        dislikes.push_back("you memorise spells");
        dislikes.push_back("you cast spells");
        break;

    case GOD_BEOGH:
        dislikes.push_back("you destroy orcish idols");
        break;

    default:
        break;
    }

    if (dislikes.empty())
        return ("");

    std::string text = god_name(which_god);
                text += " dislikes it when ";
                text += comma_separated_line(dislikes.begin(), dislikes.end(),
                                             " or ", ", ");
                text += ".";

    return (text);
}

void dec_penance(god_type god, int val)
{
    if (you.penance[god] > 0)
    {
#if DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "Decreasing penance by %d", val);
#endif
        if (you.penance[god] <= val)
        {
            simple_god_message(" seems mollified.", god);
            take_note(Note(NOTE_MOLLIFY_GOD, god));
            you.penance[god] = 0;

            // TSO's halo is once more available.
            if (god == GOD_SHINING_ONE && you.religion == god
                && you.piety >= piety_breakpoint(0))
            {
                mpr("Your divine halo returns!");
            }

            // Orcish bonuses are now once more effective.
            if (god == GOD_BEOGH && you.religion == god)
                 you.redraw_armour_class = true;

            // When you've worked through all your penance, you get
            // another chance to make hostile holy beings good neutral.
            if (is_good_god(you.religion))
                _holy_beings_attitude_change();
        }
        else
            you.penance[god] -= val;
    }
}

void dec_penance(int val)
{
    dec_penance(you.religion, val);
}

bool zin_sustenance(bool actual)
{
    return (you.piety >= piety_breakpoint(0)
            && (!actual || you.hunger_state == HS_STARVING));
}

bool yred_injury_mirror(bool actual)
{
    return (you.religion == GOD_YREDELEMNUL && !player_under_penance()
            && you.piety >= piety_breakpoint(1)
            && (!actual || you.duration[DUR_PRAYER]));
}

bool beogh_water_walk()
{
    return (you.religion == GOD_BEOGH && !player_under_penance()
            && you.piety >= piety_breakpoint(4));
}

static bool _need_water_walking()
{
    return (!player_is_airborne() && you.species != SP_MERFOLK
            && grd(you.pos()) == DNGN_DEEP_WATER);
}

static void _inc_penance(god_type god, int val)
{
    if (you.penance[god] == 0 && val > 0)
    {
        god_acting gdact(god, true);

        take_note(Note(NOTE_PENANCE, god));

        // Orcish bonuses don't apply under penance.
        if (god == GOD_BEOGH)
            you.redraw_armour_class = true;
        // Neither does Zin's divine stamina.
        else if (god == GOD_ZIN)
        {
            if (you.duration[DUR_DIVINE_STAMINA])
                remove_divine_stamina();
        }
        // Neither does TSO's halo or divine shield.
        else if (god == GOD_SHINING_ONE)
        {
            if (you.haloed())
                mpr("Your divine halo fades away.");

            if (you.duration[DUR_DIVINE_SHIELD])
                remove_divine_shield();

            _make_god_gifts_disappear(); // only on level
        }
        // Neither does Ely's divine vigour.
        else if (god == GOD_ELYVILON)
        {
            if (you.duration[DUR_DIVINE_VIGOUR])
                remove_divine_vigour();
        }
    }

    you.penance[god] += val;
    you.penance[god] = std::min<unsigned char>(MAX_PENANCE, you.penance[god]);

    if (god == GOD_BEOGH && _need_water_walking() && !beogh_water_walk())
        fall_into_a_pool( you.pos(), true, grd(you.pos()) );
}

static void _inc_penance(int val)
{
    _inc_penance(you.religion, val);
}

static void _inc_gift_timeout(int val)
{
    if (you.gift_timeout + val > 200)
        you.gift_timeout = 200;
    else
        you.gift_timeout += val;
}

static int _yred_random_servants(int threshold, bool force_hostile = false)
{
    // error trapping {dlb}
    monster_type mon = MONS_PROGRAM_BUG;
    int how_many = 1;
    int temp_rand = random2(std::min(100, threshold));

    // undead
    mon = ((temp_rand < 15) ? MONS_WRAITH :           // 15%
           (temp_rand < 30) ? MONS_WIGHT :            // 15%
           (temp_rand < 40) ? MONS_FLYING_SKULL :     // 10%
           (temp_rand < 49) ? MONS_SPECTRAL_WARRIOR : //  9%
           (temp_rand < 57) ? MONS_ROTTING_HULK :     //  8%
           (temp_rand < 64) ? MONS_SKELETAL_WARRIOR : //  7%
           (temp_rand < 70) ? MONS_FREEZING_WRAITH :  //  6%
           (temp_rand < 76) ? MONS_FLAMING_CORPSE :   //  6%
           (temp_rand < 81) ? MONS_GHOUL :            //  5%
           (temp_rand < 86) ? MONS_MUMMY :            //  5%
           (temp_rand < 90) ? MONS_VAMPIRE :          //  4%
           (temp_rand < 94) ? MONS_HUNGRY_GHOST :     //  4%
           (temp_rand < 97) ? MONS_FLAYED_GHOST :     //  3%
           (temp_rand < 99) ? MONS_SKELETAL_DRAGON    //  2%
                            : MONS_DEATH_COB);        //  1%

    if (mon == MONS_FLYING_SKULL)
        how_many = 2 + random2(4);

    mgen_data mg(mon, !force_hostile ? BEH_FRIENDLY : BEH_HOSTILE,
                 0, 0, you.pos(), MHITYOU, 0, GOD_YREDELEMNUL);

    int created = 0;
    if (force_hostile)
    {
        for (int i = 0; i < how_many; ++i)
        {
            if (create_monster(mg) != -1)
                created++;
        }
    }
    else
    {
        for (int i = 0; i < how_many; ++i)
            _delayed_monster(mg);
    }

    return (created);
}

static bool _okawaru_random_servant()
{
    // error trapping {dlb}
    monster_type mon = MONS_PROGRAM_BUG;
    int temp_rand = random2(100);

    // warriors
    mon = ((temp_rand < 15) ? MONS_ORC_WARRIOR :      // 15%
           (temp_rand < 30) ? MONS_ORC_KNIGHT :       // 15%
           (temp_rand < 40) ? MONS_NAGA_WARRIOR :     // 10%
           (temp_rand < 50) ? MONS_CENTAUR_WARRIOR :  // 10%
           (temp_rand < 60) ? MONS_STONE_GIANT :      // 10%
           (temp_rand < 70) ? MONS_FIRE_GIANT :       // 10%
           (temp_rand < 80) ? MONS_FROST_GIANT :      // 10%
           (temp_rand < 90) ? MONS_CYCLOPS :          // 10%
           (temp_rand < 95) ? MONS_HILL_GIANT         //  5%
                            : MONS_TITAN);            //  5%

    return (create_monster(
                    mgen_data::hostile_at(mon,
                        you.pos(), 0, 0, true, GOD_OKAWARU)) != -1);
}

static const item_def* _find_missile_launcher(int skill)
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        const item_def &item = you.inv[i];
        if (is_range_weapon(item)
            && range_skill(item) == skill_type(skill))
        {
            return (&item);
        }
    }
    return (NULL);
}

static int _ammo_count(const item_def *launcher)
{
    int count = 0;
    const missile_type mt = launcher? fires_ammo_type(*launcher) : MI_DART;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        const item_def &item = you.inv[i];
        if (item.base_type == OBJ_MISSILES && item.sub_type == mt)
            count += item.quantity;
    }

    return (count);
}

static bool _need_missile_gift()
{
    const int best_missile_skill = best_skill(SK_SLINGS, SK_THROWING);
    const item_def *launcher = _find_missile_launcher(best_missile_skill);
    return (you.piety > 80
            && random2( you.piety ) > 70
            && !grid_destroys_items( grd(you.pos()) )
            && one_chance_in(8)
            && you.skills[ best_missile_skill ] >= 8
            && (launcher || best_missile_skill == SK_DARTS)
            && _ammo_count(launcher) < 20 + random2(35));
}

static void _get_pure_deck_weights(int weights[])
{
    weights[0] = you.sacrifice_value[OBJ_ARMOUR] + 1;
    weights[1] = you.sacrifice_value[OBJ_WEAPONS]
                 + you.sacrifice_value[OBJ_STAVES]
                 + you.sacrifice_value[OBJ_MISSILES] + 1;
    weights[2] = you.sacrifice_value[OBJ_MISCELLANY]
                 + you.sacrifice_value[OBJ_JEWELLERY]
                 + you.sacrifice_value[OBJ_BOOKS]
                 + you.sacrifice_value[OBJ_GOLD];     // only via acquirement
    weights[3] = you.sacrifice_value[OBJ_CORPSES] / 2;
    weights[4] = you.sacrifice_value[OBJ_POTIONS]
                 + you.sacrifice_value[OBJ_SCROLLS]
                 + you.sacrifice_value[OBJ_WANDS]
                 + you.sacrifice_value[OBJ_FOOD];
}

static void _update_sacrifice_weights(int which)
{
    switch (which)
    {
    case 0:
        you.sacrifice_value[OBJ_ARMOUR] /= 5;
        you.sacrifice_value[OBJ_ARMOUR] *= 4;
        break;
    case 1:
        you.sacrifice_value[OBJ_WEAPONS]  /= 5;
        you.sacrifice_value[OBJ_STAVES]   /= 5;
        you.sacrifice_value[OBJ_MISSILES] /= 5;
        you.sacrifice_value[OBJ_WEAPONS]  *= 4;
        you.sacrifice_value[OBJ_STAVES]   *= 4;
        you.sacrifice_value[OBJ_MISSILES] *= 4;
        break;
    case 2:
        you.sacrifice_value[OBJ_MISCELLANY] /= 5;
        you.sacrifice_value[OBJ_JEWELLERY]  /= 5;
        you.sacrifice_value[OBJ_BOOKS]      /= 5;
        you.sacrifice_value[OBJ_GOLD]       /= 5;
        you.sacrifice_value[OBJ_MISCELLANY] *= 4;
        you.sacrifice_value[OBJ_JEWELLERY]  *= 4;
        you.sacrifice_value[OBJ_BOOKS]      *= 4;
        you.sacrifice_value[OBJ_GOLD]       *= 4;
    case 3:
        you.sacrifice_value[OBJ_CORPSES] /= 5;
        you.sacrifice_value[OBJ_CORPSES] *= 4;
        break;
    case 4:
        you.sacrifice_value[OBJ_POTIONS] /= 5;
        you.sacrifice_value[OBJ_SCROLLS] /= 5;
        you.sacrifice_value[OBJ_WANDS]   /= 5;
        you.sacrifice_value[OBJ_FOOD]    /= 5;
        you.sacrifice_value[OBJ_POTIONS] *= 4;
        you.sacrifice_value[OBJ_SCROLLS] *= 4;
        you.sacrifice_value[OBJ_WANDS]   *= 4;
        you.sacrifice_value[OBJ_FOOD]    *= 4;
        break;
    }
}

#if DEBUG_GIFTS || DEBUG_CARDS
static void _show_pure_deck_chances()
{
    int weights[5];

    _get_pure_deck_weights(weights);

    float total = (float) (weights[0] + weights[1] + weights[2] + weights[3]
                           + weights[4]);

    mprf(MSGCH_DIAGNOSTICS, "Pure cards chances: "
         "escape %0.2f%%, destruction %0.2f%%, dungeons %0.2f%%,"
         "summoning %0.2f%%, wonders %0.2f%%",
         (float)weights[0] / total * 100.0,
         (float)weights[1] / total * 100.0,
         (float)weights[2] / total * 100.0,
         (float)weights[3] / total * 100.0,
         (float)weights[4] / total * 100.0);
}
#endif

static void _give_nemelex_gift()
{
    if (grid_destroys_items(grd(you.pos())))
        return;

    // Nemelex will give at least one gift early.
    if (you.num_gifts[GOD_NEMELEX_XOBEH] == 0
           && x_chance_in_y(you.piety + 1, piety_breakpoint(1))
        || one_chance_in(3) && x_chance_in_y(you.piety + 1, MAX_PIETY)
           && !you.attribute[ATTR_CARD_COUNTDOWN])
    {
        misc_item_type gift_type;

        // Make a pure deck.
        const misc_item_type pure_decks[] = {
            MISC_DECK_OF_ESCAPE,
            MISC_DECK_OF_DESTRUCTION,
            MISC_DECK_OF_DUNGEONS,
            MISC_DECK_OF_SUMMONING,
            MISC_DECK_OF_WONDERS
        };
        int weights[5];
        _get_pure_deck_weights(weights);
        const int choice = choose_random_weighted(weights, weights+5);
        gift_type = pure_decks[choice];
#if DEBUG_GIFTS || DEBUG_CARDS
        _show_pure_deck_chances();
#endif
        _update_sacrifice_weights(choice);

        int thing_created = items( 1, OBJ_MISCELLANY, gift_type,
                                   true, 1, MAKE_ITEM_RANDOM_RACE,
                                   0, 0, GOD_NEMELEX_XOBEH );

        if (thing_created != NON_ITEM)
        {
            // Piety|Common  | Rare  |Legendary
            // --------------------------------
            //     0:  95.00%,  5.00%,  0.00%
            //    20:  86.00%, 10.50%,  3.50%
            //    40:  77.00%, 16.00%,  7.00%
            //    60:  68.00%, 21.50%, 10.50%
            //    80:  59.00%, 27.00%, 14.00%
            //   100:  50.00%, 32.50%, 17.50%
            //   120:  41.00%, 38.00%, 21.00%
            //   140:  32.00%, 43.50%, 24.50%
            //   160:  23.00%, 49.00%, 28.00%
            //   180:  14.00%, 54.50%, 31.50%
            //   200:   5.00%, 60.00%, 35.00%
            int common_weight = 95 - (90 * you.piety / MAX_PIETY);
            int rare_weight   = 5  + (55 * you.piety / MAX_PIETY);
            int legend_weight = 0  + (35 * you.piety / MAX_PIETY);

            deck_rarity_type rarity = static_cast<deck_rarity_type>(
                random_choose_weighted(common_weight,
                                       DECK_RARITY_COMMON,
                                       rare_weight,
                                       DECK_RARITY_RARE,
                                       legend_weight,
                                       DECK_RARITY_LEGENDARY,
                                       0));

            item_def &deck(mitm[thing_created]);

            deck.special = rarity;
            deck.colour  = deck_rarity_to_color(rarity);
            deck.inscription = "god gift";

            move_item_to_grid( &thing_created, you.pos() );

            simple_god_message(" grants you a gift!");
            more();
            canned_msg(MSG_SOMETHING_APPEARS);

            you.attribute[ATTR_CARD_COUNTDOWN] = 10;
            _inc_gift_timeout(5 + random2avg(9, 2));
            you.num_gifts[you.religion]++;
            take_note(Note(NOTE_GOD_GIFT, you.religion));
        }
    }
}

void mons_make_god_gift(monsters *mon, god_type god)
{
    const god_type acting_god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;

    if (god == GOD_NO_GOD && acting_god == GOD_NO_GOD)
        return;

    if (god == GOD_NO_GOD)
        god = acting_god;

    ASSERT(acting_god == GOD_NO_GOD || god == acting_god);
    ASSERT(mon->god == GOD_NO_GOD || mon->god == god);

    mon->god = god;

#if DEBUG
    if (mon->flags & MF_GOD_GIFT)
        mprf(MSGCH_DIAGNOSTICS, "Monster '%s' is already a gift of god '%s'",
             mon->name(DESC_PLAIN, true).c_str(), god_name(god).c_str());
#endif
    mon->flags |= MF_GOD_GIFT;
}

bool mons_is_god_gift(const monsters *mon, god_type god)
{
    return ((mon->flags & MF_GOD_GIFT) && mon->god == god);
}

static bool _is_yred_enslaved_body_and_soul(const monsters* mon)
{
    return (mon->alive() && mons_enslaved_body_and_soul(mon));
}

static bool _is_yred_enslaved_soul(const monsters* mon)
{
    return (mon->alive() && mons_enslaved_soul(mon));
}

bool is_undead_slave(const monsters* mon)
{
    return (mon->alive() && mons_holiness(mon) == MH_UNDEAD
            && mon->attitude == ATT_FRIENDLY);
}

bool is_yred_undead_slave(const monsters* mon)
{
    return (is_undead_slave(mon) && mons_is_god_gift(mon, GOD_YREDELEMNUL));
}

bool is_orcish_follower(const monsters* mon)
{
    return (mon->alive() && mons_species(mon->type) == MONS_ORC
            && mon->attitude == ATT_FRIENDLY
            && mons_is_god_gift(mon, GOD_BEOGH));
}

bool is_good_lawful_follower(const monsters* mon)
{
    return (mon->alive() && !mons_is_evil_or_unholy(mon)
            && !mons_is_chaotic(mon) && mons_friendly(mon));
}

bool is_good_follower(const monsters* mon)
{
    return (mon->alive() && !mons_is_evil_or_unholy(mon)
            && mons_friendly(mon));
}

bool is_follower(const monsters* mon)
{
    if (you.religion == GOD_YREDELEMNUL)
        return is_undead_slave(mon);
    else if (you.religion == GOD_BEOGH)
        return is_orcish_follower(mon);
    else if (you.religion == GOD_ZIN)
        return is_good_lawful_follower(mon);
    else if (is_good_god(you.religion))
        return is_good_follower(mon);
    else
        return (mon->alive() && mons_friendly(mon));
}

static bool _blessing_wpn(monsters* mon)
{
    // Pick a monster's weapon.
    const int weapon = mon->inv[MSLOT_WEAPON];
    const int alt_weapon = mon->inv[MSLOT_ALT_WEAPON];

    if (weapon == NON_ITEM && alt_weapon == NON_ITEM)
        return (false);

    int slot;

    do
        slot = (coinflip()) ? weapon : alt_weapon;
    while (slot == NON_ITEM);

    item_def& wpn(mitm[slot]);

    // And enchant or uncurse it.
    if (!enchant_weapon((coinflip()) ? ENCHANT_TO_HIT
                                     : ENCHANT_TO_DAM, true, wpn))
    {
        return (false);
    }

    item_set_appearance(wpn);
    return (true);
}

static bool _blessing_AC(monsters* mon)
{
    // Pick either a monster's armour or its shield.
    const int armour = mon->inv[MSLOT_ARMOUR];
    const int shield = mon->inv[MSLOT_SHIELD];

    if (armour == NON_ITEM && shield == NON_ITEM)
        return (false);

    int slot;

    do
        slot = (coinflip()) ? armour : shield;
    while (slot == NON_ITEM);

    item_def& arm(mitm[slot]);

    int ac_change;

    // And enchant or uncurse it.
    if (!enchant_armour(ac_change, true, arm))
        return (false);

    item_set_appearance(arm);
    return (true);
}

static bool _blessing_balms(monsters *mon)
{
    // Remove poisoning, sickness, confusion, and rotting, like a potion
    // of healing, but without the healing.  Also, remove slowing and
    // fatigue.
    bool success = false;

    if (mon->del_ench(ENCH_POISON, true))
        success = true;

    if (mon->del_ench(ENCH_SICK, true))
        success = true;

    if (mon->del_ench(ENCH_CONFUSION, true))
        success = true;

    if (mon->del_ench(ENCH_ROT, true))
        success = true;

    if (mon->del_ench(ENCH_SLOW, true))
        success = true;

    if (mon->del_ench(ENCH_FATIGUE, true))
        success = true;

    return (success);
}

static bool _blessing_healing(monsters* mon)
{
    const int healing = mon->max_hit_points / 4 + 1;

    // Heal a monster.
    if (heal_monster(mon, healing + random2(healing + 1), false))
    {
        // A high-HP monster might get a unique name.
        if (x_chance_in_y(mon->max_hit_points + 1, 100))
            give_monster_proper_name(mon);
        return (true);
    }

    return (false);
}

static bool _tso_blessing_holy_wpn(monsters* mon)
{
    // Pick a monster's weapon.
    const int weapon = mon->inv[MSLOT_WEAPON];
    const int alt_weapon = mon->inv[MSLOT_ALT_WEAPON];

    if (weapon == NON_ITEM && alt_weapon == NON_ITEM)
        return (false);

    int slot;

    do
        slot = (coinflip()) ? weapon : alt_weapon;
    while (slot == NON_ITEM);

    item_def& wpn(mitm[slot]);

    const int wpn_brand = get_weapon_brand(wpn);

    // Only brand melee weapons, and only override certain brands.
    if (is_artefact(wpn) || is_range_weapon(wpn)
        || (wpn_brand != SPWPN_NORMAL && wpn_brand != SPWPN_DRAINING
            && wpn_brand != SPWPN_PAIN && wpn_brand != SPWPN_VAMPIRICISM
            && wpn_brand != SPWPN_VENOM))
    {
        return (false);
    }

    // Convert a demonic weapon into a non-demonic weapon.
    if (is_demonic(wpn))
        convert2good(wpn, false);

    // And make it holy.
    set_equip_desc(wpn, ISFLAG_GLOWING);
    set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_HOLY_WRATH);
    wpn.colour = YELLOW;

    return (true);
}

static bool _tso_blessing_holy_arm(monsters* mon)
{
    // If a monster has full negative energy resistance, get out.
    if (mons_res_negative_energy(mon) == 3)
        return (false);

    // Pick either a monster's armour or its shield.
    const int armour = mon->inv[MSLOT_ARMOUR];
    const int shield = mon->inv[MSLOT_SHIELD];

    if (armour == NON_ITEM && shield == NON_ITEM)
        return (false);

    int slot;

    do
        slot = (coinflip()) ? armour : shield;
    while (slot == NON_ITEM);

    item_def& arm(mitm[slot]);

    const int arm_brand = get_armour_ego_type(arm);

    // Override certain brands.
    if (is_artefact(arm) || arm_brand != SPARM_NORMAL)
        return (false);

    // And make it resistant to negative energy.
    set_equip_desc(arm, ISFLAG_GLOWING);
    set_item_ego_type(arm, OBJ_ARMOUR, SPARM_POSITIVE_ENERGY);
    arm.colour = WHITE;

    return (true);
}

static bool _increase_ench_duration(monsters *mon,
                                    mon_enchant ench,
                                    const int increase)
{
    // Durations are saved as 16-bit signed ints, so clamp at the largest such.
    const int MARSHALL_MAX = (1 << 15) - 1;

    const int newdur = std::min(ench.duration + increase, MARSHALL_MAX);
    if (ench.duration >= newdur)
        return (false);

    ench.duration = newdur;
    mon->update_ench(ench);

    return (true);
}

static int _tso_blessing_extend_stay(monsters* mon)
{
    if (!mon->has_ench(ENCH_ABJ))
        return (0);

    mon_enchant abj = mon->get_ench(ENCH_ABJ);

    // [ds] Disabling permanence for balance reasons, but extending
    // duration increase.  These numbers are tenths of a player turn.
    // Holy monsters get a much bigger boost than random beasties.
    const int base_increase = mon->holiness() == MH_HOLY ? 1100 : 500;
    const int increase = base_increase + random2(base_increase);
    return _increase_ench_duration(mon, abj, increase);
}

static bool _tso_blessing_friendliness(monsters* mon)
{
    if (!mon->has_ench(ENCH_CHARM))
        return (false);

    // [ds] Just increase charm duration, no permanent friendliness.
    const int base_increase = 700;
    return _increase_ench_duration(mon, mon->get_ench(ENCH_CHARM),
                                   base_increase + random2(base_increase));
}

static void _beogh_reinf_callback(const mgen_data &mg, int &midx, int placed)
{
    ASSERT(mg.god == GOD_BEOGH);

    // Beogh tries a second time to place reinforcements.
    if (midx == -1)
        midx = create_monster(mg);

    if (midx == -1)
        return;

    monsters* mon = &menv[midx];

    mon->flags |= MF_ATT_CHANGE_ATTEMPT;

    bool high_level = (mon->type == MONS_ORC_PRIEST
                       || mon->type == MONS_ORC_WARRIOR
                       || mon->type == MONS_ORC_KNIGHT);

    // For high level orcs, there's a chance of being named.
    if (high_level && one_chance_in(5))
        give_monster_proper_name(mon);
}

// If you don't currently have any followers, send a small band to help
// you out.
static void _beogh_blessing_reinforcements()
{
    // Possible reinforcement.
    const monster_type followers[] = {
        MONS_ORC, MONS_ORC, MONS_ORC_WIZARD, MONS_ORC_PRIEST
    };

    const monster_type high_xl_followers[] = {
        MONS_ORC_PRIEST, MONS_ORC_WARRIOR, MONS_ORC_KNIGHT
    };

    // Send up to four followers.
    int how_many = random2(4) + 1;

    monster_type follower_type;
    bool high_level;
    for (int i = 0; i < how_many; ++i)
    {
        high_level = false;
        if (random2(you.experience_level) >= 9 && coinflip())
        {
            follower_type = RANDOM_ELEMENT(high_xl_followers);
            high_level = true;
        }
        else
            follower_type = RANDOM_ELEMENT(followers);

        _delayed_monster(
            mgen_data(follower_type, BEH_FRIENDLY, 0, 0,
                      you.pos(), MHITYOU, 0, GOD_BEOGH),
            _beogh_reinf_callback);
    }
}

static bool _beogh_blessing_priesthood(monsters* mon)
{
    monster_type priest_type = MONS_PROGRAM_BUG;

    // Possible promotions.
    if (mon->type == MONS_ORC)
        priest_type = MONS_ORC_PRIEST;

    if (priest_type != MONS_PROGRAM_BUG)
    {
        // Turn an ordinary monster into a priestly monster, using a
        // function normally used when going up an experience level.
        // This is a hack, but there seems to be no better way for now.
        mon->upgrade_type(priest_type, true, true);
        give_monster_proper_name(mon);

        return (true);
    }

    return (false);
}

// Bless the follower indicated in follower, if any.  If there isn't
// one, bless a random follower within sight of the player, if any, or,
// with decreasing chances, any follower on the level.
// Blessing can be enforced with a wizard mode command.
bool bless_follower(monsters *follower,
                    god_type god,
                    bool (*suitable)(const monsters* mon),
                    bool force)
{
    int chance = (force ? coinflip() : random2(20));
    std::string result;

    // If a follower was specified, and it's suitable, pick it.
    // Otherwise, pick a random follower.
    if (!follower || (!force && !suitable(follower)))
    {
        // Only Beogh blesses random followers.
        if (god != GOD_BEOGH)
            return (false);

        if (chance > 2)
            return (false);

        // Choose a random follower in LOS, preferably a named or
        // priestly one (10% chance).
        follower = choose_random_nearby_monster(0, suitable, true, true, true);

        if (!follower)
        {
            if (coinflip())
                return (false);

            // Try again, without the LOS restriction (5% chance).
            follower = choose_random_nearby_monster(0, suitable, false, true,
                                                    true);

            if (!follower)
            {
                if (coinflip())
                    return (false);

                // Try *again*, on the entire level (2.5% chance).
                follower = choose_random_monster_on_level(0, suitable, false,
                                                          false, true, true);

                if (!follower)
                {
                    // If no follower was found, attempt to send
                    // reinforcements.
                    _beogh_blessing_reinforcements();

                    // Possibly send more reinforcements.
                    if (coinflip())
                        _beogh_blessing_reinforcements();

                    _delayed_monster_done("Beogh blesses you with "
                                         "reinforcements.", "");

                    // Return true, even though the reinforcements might
                    // not be placed.
                    return (true);
                }
            }
        }
    }
    ASSERT(follower);

    if (chance <= 1) // 10% chance of holy branding, or priesthood
    {
        switch (god)
        {
            case GOD_SHINING_ONE:
                if (coinflip())
                {
                    // Brand a monster's weapon with holy wrath, if
                    // possible.
                    if (_tso_blessing_holy_wpn(follower))
                    {
                        result = "holy attack power";
                        goto blessing_done;
                    }
                    else if (force)
                        mpr("Couldn't bless monster's weapon.");
                }
                else
                {
                    // Brand a monster's armour with positive energy, if
                    // possible.
                    if (_tso_blessing_holy_arm(follower))
                    {
                        result = "life defence";
                        goto blessing_done;
                    }
                    else if (force)
                        mpr("Couldn't bless monster's armour.");
                }
                break;

            case GOD_BEOGH:
                // Turn a monster into a priestly monster, if possible.
                if (_beogh_blessing_priesthood(follower))
                {
                    result = "priesthood";
                    goto blessing_done;
                }
                else if (force)
                    mpr("Couldn't promote monster to priesthood.");
                break;

            default:
                break;
        }
    }

    // Enchant a monster's weapon or armour/shield by one point, or at
    // least uncurse it, if possible (10% chance).
    // This will happen if the above blessing attempts are unsuccessful.
    if (chance <= 1)
    {
        if (coinflip())
        {
            if (_blessing_wpn(follower))
            {
                result = "extra attack power";
                give_monster_proper_name(follower);
                goto blessing_done;
            }
            else if (force)
                mpr("Couldn't enchant monster's weapon.");
        }
        else
        {
            if (_blessing_AC(follower))
            {
                result = "extra defence";
                give_monster_proper_name(follower);
                goto blessing_done;
            }
            else if (force)
                mpr("Couldn't enchant monster's armour.");
        }
    }

    // These effects happen if no other blessing was chosen (90%),
    // or if the above attempts were all unsuccessful.
    switch (god)
    {
        case GOD_SHINING_ONE:
        {
            // Extend a monster's stay if it's abjurable, optionally
            // making it friendly if it's charmed.  If neither is
            // possible, deliberately fall through.
            int more_time = _tso_blessing_extend_stay(follower);
            bool friendliness = false;

            if (!more_time || coinflip())
                friendliness = _tso_blessing_friendliness(follower);

            result = "";

            if (friendliness)
            {
                result += "friendliness";
                if (more_time)
                    result += " and ";
            }

            if (more_time)
            {
                result += (more_time == 2) ? "permanent time in this world"
                                           : "more time in this world";
            }

            if (more_time || friendliness)
                break;

            if (force)
                mpr("Couldn't increase monster's friendliness or time.");
        }

        // Deliberate fallthrough for the healing effects.
        case GOD_BEOGH:
        {
            // Remove harmful ailments from a monster, or heal it, if
            // possible.
            if (coinflip())
            {
                if (_blessing_balms(follower))
                {
                    result = "divine balms";
                    goto blessing_done;
                }
                else if (force)
                    mpr("Couldn't apply balms.");
            }

            bool healing = _blessing_healing(follower);

            if (!healing || coinflip())
            {
                if (_blessing_healing(follower))
                    healing = true;
            }

            if (healing)
            {
                result += "healing";
                break;
            }
            else if (force)
                mpr("Couldn't heal monster.");

            return (false);
        }

        default:
            break;
    }

blessing_done:

    std::string whom = "";
    if (!follower)
        whom = "you";
    else
    {
        if (you.can_see(follower))
            whom = follower->name(DESC_NOCAP_THE);
        else
            whom = "a follower";
    }

    simple_god_message(
        make_stringf(" blesses %s with %s.",
                     whom.c_str(), result.c_str()).c_str(),
        god);

#ifndef USE_TILE
    flash_monster_colour(follower, god_colour(god), 200);
#endif

    return (true);
}

static void _delayed_gift_callback(const mgen_data &mg, int &midx,
                                   int placed)
{
    if (placed <= 0)
        return;

    more();
    _inc_gift_timeout(4 + random2avg(7, 2));
    you.num_gifts[you.religion]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));
}

static void _do_god_gift(bool prayed_for)
{
    ASSERT(you.religion != GOD_NO_GOD);

    // Zin worshippers are the only ones who can pray to ask Zin for
    // stuff.
    if (prayed_for != (you.religion == GOD_ZIN))
        return;

    god_acting gdact;

#if DEBUG_DIAGNOSTICS || DEBUG_GIFTS
    int old_gifts = you.num_gifts[you.religion];
#endif

    // Consider a gift if we don't have a timeout and weren't already
    // praying when we prayed.
    if (!player_under_penance() && !you.gift_timeout
        || (prayed_for && you.religion == GOD_ZIN))
    {
        bool success = false;

        // Remember to check for water/lava.
        switch (you.religion)
        {
        default:
            break;

        case GOD_ZIN:
            //jmf: this "good" god will feed you (a la Nethack)
            if (prayed_for && zin_sustenance())
            {
                god_speaks(you.religion, "Your stomach feels content.");
                set_hunger(6000, true);
                lose_piety(5 + random2avg(10, 2) + (you.gift_timeout ? 5 : 0));
                _inc_gift_timeout(30 + random2avg(10, 2));
            }
            break;

        case GOD_NEMELEX_XOBEH:
            _give_nemelex_gift();
            break;

        case GOD_OKAWARU:
        case GOD_TROG:
            if (you.piety > 130
                && random2(you.piety) > 120
                && !grid_destroys_items(grd(you.pos()))
                && one_chance_in(4))
            {
                if (you.religion == GOD_TROG
                    || (you.religion == GOD_OKAWARU && coinflip()))
                {
                    success = acquirement(OBJ_WEAPONS, you.religion);
                }
                else
                {
                    success = acquirement(OBJ_ARMOUR, you.religion);
                    // Okawaru charges extra for armour acquirements.
                    _inc_gift_timeout(20 + random2avg(15, 2));
                }

                if (success)
                {
                    simple_god_message(" grants you a gift!");
                    more();

                    _inc_gift_timeout(30 + random2avg(19, 2));
                    you.num_gifts[ you.religion ]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
                break;
            }

            if (_need_missile_gift())
            {
                success = acquirement(OBJ_MISSILES, you.religion);
                if (success)
                {
                    simple_god_message(" grants you a gift!");
                    more();

                    _inc_gift_timeout(4 + roll_dice(2, 4));
                    you.num_gifts[you.religion]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
                break;
            }
            break;

        case GOD_YREDELEMNUL:
            if (random2(you.piety) >= piety_breakpoint(2) && one_chance_in(4))
            {
                // The maximum threshold occurs at piety_breakpoint(5).
                int threshold = (you.piety - piety_breakpoint(2)) * 20 / 9;
                _yred_random_servants(threshold);

                _delayed_monster_done(" grants you @an@ undead servant@s@!",
                                      "", _delayed_gift_callback);
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_SIF_MUNA:
        case GOD_VEHUMET:
            if (you.piety > 160 && random2(you.piety) > 100)
            {
                unsigned int gift = NUM_BOOKS;

                switch (you.religion)
                {
                case GOD_KIKUBAAQUDGHA:     // gives death books
                    if (!you.had_book[BOOK_NECROMANCY])
                        gift = BOOK_NECROMANCY;
                    else if (!you.had_book[BOOK_DEATH])
                        gift = BOOK_DEATH;
                    else if (!you.had_book[BOOK_UNLIFE])
                        gift = BOOK_UNLIFE;
                    else if (!you.had_book[BOOK_NECRONOMICON])
                        gift = BOOK_NECRONOMICON;
                    break;

                case GOD_SIF_MUNA:
                    gift = OBJ_RANDOM;      // Sif Muna - gives any
                    break;

                // Vehumet - gives conj/summ. books (higher skill first)
                case GOD_VEHUMET:
                    if (!you.had_book[BOOK_CONJURATIONS_I])
                        gift = give_first_conjuration_book();
                    else if (!you.had_book[BOOK_POWER])
                        gift = BOOK_POWER;
                    else if (!you.had_book[BOOK_ANNIHILATIONS])
                        gift = BOOK_ANNIHILATIONS;  // conj books

                    if (you.skills[SK_CONJURATIONS] < you.skills[SK_SUMMONINGS]
                        || gift == NUM_BOOKS)
                    {
                        if (!you.had_book[BOOK_CALLINGS])
                            gift = BOOK_CALLINGS;
                        else if (!you.had_book[BOOK_SUMMONINGS])
                            gift = BOOK_SUMMONINGS;
                        else if (!you.had_book[BOOK_DEMONOLOGY])
                            gift = BOOK_DEMONOLOGY; // summoning bks
                    }
                    break;
                default:
                    break;
                }

                if (gift != NUM_BOOKS
                    && !grid_destroys_items(grd(you.pos())))
                {
                    if (gift == OBJ_RANDOM)
                    {
                        // Sif Muna special: Keep quiet if acquirement fails
                        // because the player already has seen all spells.
                        success = acquirement(OBJ_BOOKS, you.religion, true);
                    }
                    else
                    {
                        int thing_created = items(1, OBJ_BOOKS, gift, true, 1,
                                                  MAKE_ITEM_RANDOM_RACE,
                                                  0, 0, you.religion);
                        if (thing_created == NON_ITEM)
                            return;

                        move_item_to_grid( &thing_created, you.pos() );

                        if (thing_created != NON_ITEM)
                        {
                            success = true;
                            mitm[thing_created].inscription = "god gift";
                        }
                    }

                    if (success)
                    {
                        simple_god_message(" grants you a gift!");
                        more();

                        _inc_gift_timeout(40 + random2avg(19, 2));
                        you.num_gifts[ you.religion ]++;
                        take_note(Note(NOTE_GOD_GIFT, you.religion));
                    }

                    // Vehumet gives books less readily
                    if (you.religion == GOD_VEHUMET && success)
                        _inc_gift_timeout(10 + random2(10));
                }                   // end of giving book
            }                       // end of book gods
            break;
        }
    }                           // end of gift giving

#if DEBUG_DIAGNOSTICS || DEBUG_GIFTS
    if (old_gifts < you.num_gifts[ you.religion ])
    {
        mprf(MSGCH_DIAGNOSTICS, "Total number of gifts from this god: %d",
             you.num_gifts[ you.religion ] );
    }
#endif
}

static bool _is_risky_sacrifice(const item_def& item)
{
    return item.base_type == OBJ_ORBS || is_rune(item);
}

static bool _confirm_pray_sacrifice(god_type god)
{
    if (Options.stash_tracking == STM_EXPLICIT && is_stash(you.pos()))
    {
        mpr("You can't sacrifice explicitly marked stashes.");
        return (false);
    }

    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (_god_likes_item(god, *si)
            && (_is_risky_sacrifice(*si)
                || has_warning_inscription(*si, OPER_PRAY)))
        {
            std::string prompt = "Really sacrifice stack with ";
            prompt += si->name(DESC_NOCAP_A);
            prompt += " in it?";

            if (!yesno(prompt.c_str(), false, 'n'))
                return (false);
        }
    }

    return (true);
}

std::string god_prayer_reaction()
{
    std::string result = god_name(you.religion);
    if (!crawl_state.need_save && crawl_state.updating_scores)
        result += " was ";
    else
        result += " is ";
    result +=
        (you.piety > 130) ? "exalted by your worship" :
        (you.piety > 100) ? "extremely pleased with you" :
        (you.piety >  70) ? "greatly pleased with you" :
        (you.piety >  40) ? "most pleased with you" :
        (you.piety >  20) ? "pleased with you" :
        (you.piety >   5) ? "noncommittal"
                          : "displeased";
    result += ".";

    return (result);
}

static bool _god_accepts_prayer(god_type god)
{
    harm_protection_type hpt = god_protects_from_harm(god, false);

    if (hpt == HPT_PRAYING || hpt == HPT_PRAYING_PLUS_ANYTIME
        || hpt == HPT_RELIABLE_PRAYING_PLUS_ANYTIME)
    {
        return (true);
    }

    if (god_likes_butchery(god))
        return (true);

    switch (god)
    {
    case GOD_ZIN:
        return (zin_sustenance(false));

    case GOD_YREDELEMNUL:
        return (yred_injury_mirror(false));

    case GOD_BEOGH:
    case GOD_NEMELEX_XOBEH:
        return (true);

    default:
        break;
    }

    return (false);
}

void pray()
{
    if (silenced(you.pos()))
    {
        mpr("You are unable to make a sound!");
        return;
    }

    // almost all prayers take time
    you.turn_is_over = true;

    const bool was_praying = !!you.duration[DUR_PRAYER];

    const god_type altar_god = grid_altar_god(grd(you.pos()));
    if (altar_god != GOD_NO_GOD)
    {
        if (you.flight_mode() == FL_LEVITATE)
        {
            you.turn_is_over = false;
            mpr("You are floating high above the altar.");
            return;
        }

        if (you.religion != GOD_NO_GOD && altar_god == you.religion)
        {
            _altar_prayer();
        }
        else if (altar_god != GOD_NO_GOD)
        {
            if (you.species == SP_DEMIGOD)
            {
                you.turn_is_over = false;
                mpr("Sorry, a being of your status cannot worship here.");
                return;
            }

            god_pitch(grid_altar_god(grd(you.pos())));
            return;
        }
    }

    if (you.religion == GOD_NO_GOD)
    {
        mprf(MSGCH_PRAY,
             "You spend a moment contemplating the meaning of %slife.",
             you.is_undead ? "un" : "");

        // Zen meditation is timeless.
        you.turn_is_over = false;
        return;
    }
    else if (!_god_accepts_prayer(you.religion))
    {
        simple_god_message(" ignores your prayer.");
        return;
    }

    // Beoghites and Nemelexites can abort out now instead of offering
    // something they don't want to lose.
    if (altar_god == GOD_NO_GOD
        && (you.religion == GOD_BEOGH ||  you.religion == GOD_NEMELEX_XOBEH)
        && !_confirm_pray_sacrifice(you.religion))
    {
        you.turn_is_over = false;
        return;
    }

    mprf(MSGCH_PRAY, "You %s prayer to %s.",
         was_praying ? "renew your" : "offer a",
         god_name(you.religion).c_str());

    you.duration[DUR_PRAYER] = 9 + (random2(you.piety) / 20)
                                 + (random2(you.piety) / 20);

    if (player_under_penance())
        simple_god_message(" demands penance!");
    else
    {
        mpr(god_prayer_reaction().c_str(), MSGCH_PRAY, you.religion);

        if (you.piety > 130)
            you.duration[DUR_PRAYER] *= 3;
        else if (you.piety > 70)
            you.duration[DUR_PRAYER] *= 2;
    }

    if (you.religion == GOD_ZIN || you.religion == GOD_BEOGH
        || you.religion == GOD_NEMELEX_XOBEH)
    {
        you.duration[DUR_PRAYER] = 1;
    }
    else if (you.religion == GOD_YREDELEMNUL || you.religion == GOD_ELYVILON)
        you.duration[DUR_PRAYER] = 20;

    // Beoghites and Nemelexites offer the items they're standing on.
    if (altar_god == GOD_NO_GOD
        && (you.religion == GOD_BEOGH ||  you.religion == GOD_NEMELEX_XOBEH))
    {
        offer_items();
    }

    if (!was_praying)
        _do_god_gift(true);

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "piety: %d (-%d)", you.piety, you.piety_hysteresis );
#endif
}

void end_prayer(void)
{
    mpr("Your prayer is over.", MSGCH_PRAY, you.religion);
    you.duration[DUR_PRAYER] = 0;
}

std::string god_name( god_type which_god, bool long_name )
{
    switch (which_god)
    {
    case GOD_NO_GOD: return "No God";
    case GOD_RANDOM: return "random";
    case GOD_NAMELESS: return "nameless";
    case GOD_ZIN:           return (long_name ? "Zin the Law-Giver" : "Zin");
    case GOD_SHINING_ONE:   return "The Shining One";
    case GOD_KIKUBAAQUDGHA: return "Kikubaaqudgha";
    case GOD_YREDELEMNUL:
        return (long_name ? "Yredelemnul the Dark" : "Yredelemnul");
    case GOD_VEHUMET: return "Vehumet";
    case GOD_OKAWARU: return (long_name ? "Warmaster Okawaru" : "Okawaru");
    case GOD_MAKHLEB: return (long_name ? "Makhleb the Destroyer" : "Makhleb");
    case GOD_SIF_MUNA:
        return (long_name ? "Sif Muna the Loreminder" : "Sif Muna");
    case GOD_TROG: return (long_name ? "Trog the Wrathful" : "Trog");
    case GOD_NEMELEX_XOBEH: return "Nemelex Xobeh";
    case GOD_ELYVILON: return (long_name ? "Elyvilon the Healer" : "Elyvilon");
    case GOD_LUGONU:   return (long_name ? "Lugonu the Unformed" : "Lugonu");
    case GOD_BEOGH:    return (long_name ? "Beogh the Brigand" : "Beogh");

    case GOD_XOM:
        if (!long_name)
            return "Xom";
        else
        {
            const char* xom_names[] = {
                "Xom the Random", "Xom the Random Number God",
                "Xom the Tricky", "Xom the Less-Predictable",
                "Xom the Unpredictable", "Xom of Many Doors",
                "Xom the Capricious", "Xom of Bloodstained Whimsey",
                "Xom of Enforced Whimsey", "Xom of Bone-Dry Humour",
                "Xom of Malevolent Giggling", "Xom of Malicious Giggling",
                "Xom the Psychotic", "Xom of Gnomic Intent",
                "Xom the Fickle", "Xom of Unknown Intention",
                "The Xom-Meister", "Xom the Begetter of Turbulence",
                "Xom the Begetter of Discontinuities"
            };
            return (one_chance_in(3) ? RANDOM_ELEMENT(xom_names)
                    : "Xom of Chaos");
        }
    case NUM_GODS: return "Buggy";
    }
    return ("");
}

god_type string_to_god(const char *_name, bool exact)
{
    std::string target(_name);
    trim_string(target);
    lowercase(target);

    if (target.empty())
        return (GOD_NO_GOD);

    int      num_partials = 0;
    god_type partial      = GOD_NO_GOD;
    for (int i = 0; i < NUM_GODS; ++i)
    {
        god_type    god  = (god_type) i;
        std::string name = lowercase_string(god_name(god, false));

        if (name == target)
            return (god);

        if (!exact && name.find(target) != std::string::npos)
        {
            // Return nothing for ambiguous partial names.
            num_partials++;
            if (num_partials > 1)
                return (GOD_NO_GOD);
            partial = god;
        }
    }

    if (!exact && num_partials == 1)
        return (partial);

    return (GOD_NO_GOD);
}

void god_speaks(god_type god, const char *mesg)
{
    ASSERT(!crawl_state.arena);

    int orig_mon = mgrd(you.pos());

    monsters fake_mon;
    fake_mon.type       = MONS_PROGRAM_BUG;
    fake_mon.hit_points = 1;
    fake_mon.god        = god;
    fake_mon.position   = you.pos();
    fake_mon.foe        = MHITYOU;
    fake_mon.mname      = "FAKE GOD MONSTER";

    mpr(do_mon_str_replacements(mesg, &fake_mon).c_str(), MSGCH_GOD, god);

    fake_mon.reset();
    mgrd(you.pos()) = orig_mon;
}

static bool _do_god_revenge(conduct_type thing_done)
{
    bool retval = false;

    switch (thing_done)
    {
    case DID_DESTROY_ORCISH_IDOL:
        retval = _beogh_idol_revenge();
        break;
    case DID_KILL_HOLY:
    case DID_HOLY_KILLED_BY_UNDEAD_SLAVE:
    case DID_HOLY_KILLED_BY_SERVANT:
        retval = _tso_holy_revenge();
        break;
    default:
        break;
    }

    return (retval);
}

// This function is the merger of done_good() and naughty().
// Returns true if god was interested (good or bad) in conduct.
bool did_god_conduct(conduct_type thing_done, int level, bool known,
                     const monsters *victim)
{
    ASSERT(!crawl_state.arena);

    bool retval = false;

    if (you.religion != GOD_NO_GOD && you.religion != GOD_XOM)
    {
        int piety_change = 0;
        int penance = 0;

        god_acting gdact;

        switch (thing_done)
        {
        case DID_DRINK_BLOOD:
            switch (you.religion)
            {
            case GOD_SHINING_ONE:
                if (!known)
                {
                    simple_god_message(" forgives your inadvertent "
                                       "blood-drinking, just this once.");
                    break;
                }
                penance = level;
                // deliberate fall through
            case GOD_ZIN:
            case GOD_ELYVILON:
                if (!known)
                {
                    simple_god_message(" forgives your inadvertent "
                                       "blood-drinking, just this once.");
                    break;
                }
                piety_change = -2*level;
                retval = true;
                break;
            default:
                break;
            }
            break;

        case DID_CANNIBALISM:
            switch (you.religion)
            {
            case GOD_ZIN:
            case GOD_SHINING_ONE:
            case GOD_ELYVILON:
            case GOD_BEOGH:
                piety_change = -level;
                penance = level;
                retval = true;
                break;
            default:
                break;
            }
            break;

        case DID_NECROMANCY:
        case DID_UNHOLY:
        case DID_ATTACK_HOLY:
            switch (you.religion)
            {
            case GOD_ZIN:
            case GOD_SHINING_ONE:
            case GOD_ELYVILON:
                if (!known && thing_done != DID_ATTACK_HOLY)
                {
                    simple_god_message(" forgives your inadvertent unholy act, "
                                       "just this once.");
                    break;
                }
                piety_change = -level;
                if (known || thing_done == DID_ATTACK_HOLY
                    && victim->attitude != ATT_HOSTILE)
                {
                    penance = level * ((you.religion == GOD_SHINING_ONE) ? 2
                                                                         : 1);
                }
                retval = true;
                break;
            default:
                break;
            }
            break;

        case DID_HOLY:
            if (you.religion == GOD_YREDELEMNUL)
            {
                if (!known)
                {
                    simple_god_message(" forgives your inadvertent holy act, "
                                       "just this once.");
                    break;
                }
                retval = true;
                piety_change = -level;
                penance = level * 2;
            }
            break;

        case DID_UNCHIVALRIC_ATTACK:
        case DID_POISON:
            if (you.religion == GOD_SHINING_ONE)
            {
                if (thing_done == DID_UNCHIVALRIC_ATTACK)
                {
                    if (tso_unchivalric_attack_safe_monster(victim))
                        break;

                    if (!known)
                    {
                        simple_god_message(" forgives your inadvertent "
                                           "dishonourable attack, just this "
                                           "once.");
                        break;
                    }
                }
                retval = true;
                piety_change = -level;
                penance = level * 2;
            }
            break;

        case DID_ATTACK_NEUTRAL:
            switch (you.religion)
            {
            case GOD_SHINING_ONE:
            case GOD_ELYVILON:
                if (!known)
                {
                    simple_god_message(" forgives your inadvertent attack on a "
                                       "neutral, just this once.");
                    break;
                }
                penance = level/2 + 1;
                // deliberate fall through
            case GOD_ZIN:
                if (!known)
                {
                    simple_god_message(" forgives your inadvertent attack on a "
                                       "neutral, just this once.");
                    break;
                }
                piety_change = -(level/2 + 1);
                retval = true;
                break;

            default:
                break;
            }
            break;

        case DID_ATTACK_FRIEND:
            if (god_hates_attacking_friend(you.religion, victim))
            {
                if (!known)
                {
                    simple_god_message(" forgives your inadvertent attack on "
                                       "an ally, just this once.");
                    break;
                }

                piety_change = -level;
                if (known)
                    penance = level * 3;
                retval = true;
            }
            break;

        case DID_FRIEND_DIED:
            switch (you.religion)
            {
            case GOD_ELYVILON: // healer god cares more about this
                if (player_under_penance())
                    penance = 1;  // if already under penance smaller bonus
                else
                    penance = level;
                // fall through
            case GOD_ZIN: // in contrast to TSO, who doesn't mind martyrs
            case GOD_OKAWARU:
                piety_change = -level;
                retval = true;
                break;
            default:
                break;
            }
            break;

        case DID_DEDICATED_BUTCHERY:  // a.k.a. field sacrifice
            switch (you.religion)
            {
            case GOD_ELYVILON:
                simple_god_message(" does not appreciate your butchering the "
                                   "dead during prayer!");
                retval = true;
                piety_change = -10;
                penance = 10;
                break;

            case GOD_OKAWARU:
            case GOD_MAKHLEB:
            case GOD_TROG:
            case GOD_LUGONU:
                simple_god_message(" accepts your offering.");
                retval = true;
                if (random2(level + 10) > 5)
                    piety_change = 1;
                break;

            default:
                break;
            }
            break;

        case DID_KILL_LIVING:
            switch (you.religion)
            {
            case GOD_ELYVILON:
                // killing only disapproved during prayer
                if (you.duration[DUR_PRAYER])
                {
                    simple_god_message(" does not appreciate your shedding "
                                       "blood during prayer!");
                    retval = true;
                    piety_change = -level;
                    penance = level * 2;
                }
                break;

            case GOD_KIKUBAAQUDGHA:
            case GOD_YREDELEMNUL:
            case GOD_OKAWARU:
            case GOD_VEHUMET:
            case GOD_MAKHLEB:
            case GOD_TROG:
            case GOD_BEOGH:
            case GOD_LUGONU:
                if (god_hates_attacking_friend(you.religion, victim))
                    break;

                simple_god_message(" accepts your kill.");
                retval = true;
                if (random2(level + 18 - you.experience_level / 2) > 5)
                    piety_change = 1;
                break;

            default:
                break;
            }
            break;

        case DID_KILL_UNDEAD:
            switch (you.religion)
            {
            case GOD_SHINING_ONE:
            case GOD_OKAWARU:
            case GOD_VEHUMET:
            case GOD_MAKHLEB:
            case GOD_BEOGH:
            case GOD_LUGONU:
                if (god_hates_attacking_friend(you.religion, victim))
                    break;

                simple_god_message(" accepts your kill.");
                retval = true;
                // Holy gods are easier to please this way
                if (random2(level + 18 - (is_good_god(you.religion) ? 0 :
                                          you.experience_level / 2)) > 4)
                    piety_change = 1;
                break;

            default:
                break;
            }
            break;

        case DID_KILL_DEMON:
            switch (you.religion)
            {
            case GOD_SHINING_ONE:
            case GOD_OKAWARU:
            case GOD_MAKHLEB:
            case GOD_TROG:
            case GOD_BEOGH:
                if (god_hates_attacking_friend(you.religion, victim))
                    break;

                simple_god_message(" accepts your kill.");
                retval = true;
                // Holy gods are easier to please this way
                if (random2(level + 18 - (is_good_god(you.religion) ? 0 :
                                          you.experience_level / 2)) > 3)
                    piety_change = 1;
                break;

            default:
                break;
            }
            break;

        case DID_KILL_NATURAL_EVIL:
            if (you.religion == GOD_SHINING_ONE
                && !god_hates_attacking_friend(you.religion, victim))
            {
                simple_god_message(" accepts your kill.");
                retval = true;
                if (random2(level + 18) > 3)
                    piety_change = 1;
            }
            break;

        case DID_KILL_CHAOTIC:
            if (you.religion == GOD_ZIN
                && !god_hates_attacking_friend(you.religion, victim))
            {
                simple_god_message(" appreciates your killing of a spawn of "
                                   "chaos.");
                retval = true;
                if (random2(level + 18) > 3)
                    piety_change = 1;
            }
            break;

        case DID_KILL_PRIEST:
            if (you.religion == GOD_BEOGH
                && !god_hates_attacking_friend(you.religion, victim))
            {
                simple_god_message(" appreciates your killing of a heretic "
                                   "priest.");
                retval = true;
                if (random2(level + 10) > 5)
                    piety_change = 1;
            }
            break;

        case DID_KILL_WIZARD:
            if (you.religion == GOD_TROG
                && !god_hates_attacking_friend(you.religion, victim))
            {
                simple_god_message(" appreciates your killing of a magic "
                                   "user.");
                retval = true;
                if (random2(level + 10) > 5)
                    piety_change = 1;
            }
            break;

        // Note that holy deaths are special, they are always noticed...
        // If you or any friendly kills one, you'll get the credit or
        // the blame.
        case DID_KILL_HOLY:
            switch (you.religion)
            {
            case GOD_ZIN:
            case GOD_SHINING_ONE:
            case GOD_ELYVILON:
                if (testbits(victim->flags, MF_CREATED_FRIENDLY)
                    || testbits(victim->flags, MF_WAS_NEUTRAL))
                {
                    level *= 3;
                    penance = level;
                }
                piety_change = -level;
                retval = true;
                break;

            case GOD_YREDELEMNUL:
            case GOD_KIKUBAAQUDGHA:
            case GOD_MAKHLEB:
            case GOD_BEOGH:
            case GOD_LUGONU:
                if (god_hates_attacking_friend(you.religion, victim))
                    break;

                simple_god_message(" accepts your kill.");
                retval = true;
                if (random2(level + 18) > 2)
                    piety_change = 1;

                if (you.religion == GOD_YREDELEMNUL)
                {
                    simple_god_message(" appreciates your killing of a holy "
                                       "being.");
                    retval = true;
                    if (random2(level + 10) > 5)
                        piety_change = 1;
                }
                break;

            default:
                break;
            }
            break;

        case DID_HOLY_KILLED_BY_UNDEAD_SLAVE:
            switch (you.religion)
            {
            case GOD_YREDELEMNUL:
            case GOD_KIKUBAAQUDGHA: // note: reapers aren't undead
            case GOD_MAKHLEB:
            case GOD_BEOGH:
            case GOD_LUGONU:
                if (god_hates_attacking_friend(you.religion, victim))
                    break;

                simple_god_message(" accepts your slave's kill.");
                retval = true;
                if (random2(level + 18) > 2)
                    piety_change = 1;
                break;

            default:
                break;
            }
            break;

        case DID_HOLY_KILLED_BY_SERVANT:
            switch (you.religion)
            {
            case GOD_ZIN:
            case GOD_SHINING_ONE:
            case GOD_ELYVILON:
                if (testbits(victim->flags, MF_CREATED_FRIENDLY)
                    || testbits(victim->flags, MF_WAS_NEUTRAL))
                {
                    level *= 3;
                    penance = level;
                }
                piety_change = -level;
                retval = true;
                break;

            case GOD_KIKUBAAQUDGHA: // note: reapers aren't undead
            case GOD_MAKHLEB:
            case GOD_BEOGH:
            case GOD_LUGONU:
                if (god_hates_attacking_friend(you.religion, victim))
                    break;

                simple_god_message(" accepts your collateral kill.");
                retval = true;
                if (random2(level + 18) > 2)
                    piety_change = 1;
                break;

            default:
                break;
            }
            break;

        case DID_LIVING_KILLED_BY_UNDEAD_SLAVE:
            switch (you.religion)
            {
            case GOD_YREDELEMNUL:
            case GOD_KIKUBAAQUDGHA:
            case GOD_VEHUMET:
            case GOD_MAKHLEB:
            case GOD_BEOGH:
            case GOD_LUGONU:
                simple_god_message(" accepts your slave's kill.");
                retval = true;
                if (random2(level + 10 - you.experience_level/3) > 5)
                    piety_change = 1;
                break;
            default:
                break;
            }
            break;

        case DID_LIVING_KILLED_BY_SERVANT:
            switch (you.religion)
            {
            case GOD_KIKUBAAQUDGHA: // note: reapers aren't undead
            case GOD_VEHUMET:
            case GOD_MAKHLEB:
            case GOD_TROG:
            case GOD_BEOGH:
            case GOD_LUGONU:
                simple_god_message(" accepts your collateral kill.");
                retval = true;
                if (random2(level + 10 - you.experience_level/3) > 5)
                    piety_change = 1;
                break;
            default:
                break;
            }
            break;

        case DID_UNDEAD_KILLED_BY_UNDEAD_SLAVE:
            switch (you.religion)
            {
            case GOD_VEHUMET:
            case GOD_MAKHLEB:
            case GOD_BEOGH:
            case GOD_LUGONU:
                simple_god_message(" accepts your slave's kill.");
                retval = true;
                if (random2(level + 10 - you.experience_level/3) > 5)
                    piety_change = 1;
                break;
            default:
                break;
            }
            break;

        case DID_UNDEAD_KILLED_BY_SERVANT:
            switch (you.religion)
            {
            case GOD_SHINING_ONE:
            case GOD_VEHUMET:
            case GOD_MAKHLEB:
            case GOD_BEOGH:
            case GOD_LUGONU:
                simple_god_message(" accepts your collateral kill.");
                retval = true;
                if (random2(level + 10 - (is_good_god(you.religion) ? 0 :
                                          you.experience_level/3)) > 5)
                {
                    piety_change = 1;
                }
                break;
            default:
                break;
            }
            break;

        case DID_DEMON_KILLED_BY_UNDEAD_SLAVE:
            switch (you.religion)
            {
            case GOD_MAKHLEB:
            case GOD_BEOGH:
                simple_god_message(" accepts your slave's kill.");
                retval = true;
                if (random2(level + 10 - you.experience_level/3) > 5)
                    piety_change = 1;
                break;
            default:
                break;
            }
            break;

        case DID_DEMON_KILLED_BY_SERVANT:
            switch (you.religion)
            {
            case GOD_SHINING_ONE:
            case GOD_MAKHLEB:
            case GOD_TROG:
            case GOD_BEOGH:
                simple_god_message(" accepts your collateral kill.");
                retval = true;
                if (random2(level + 10 - (is_good_god(you.religion) ? 0 :
                                          you.experience_level/3)) > 5)
                {
                    piety_change = 1;
                }
                break;
            default:
                break;
            }
            break;

        case DID_NATURAL_EVIL_KILLED_BY_SERVANT:
            if (you.religion == GOD_SHINING_ONE)
            {
                simple_god_message(" accepts your collateral kill.");
                retval = true;

                if (random2(level + 10) > 5)
                    piety_change = 1;
            }
            break;

        case DID_SPELL_MEMORISE:
            if (you.religion == GOD_TROG)
            {
                penance = level * 10;
                piety_change = -penance;
                retval = true;
            }
            break;

        case DID_SPELL_CASTING:
            if (you.religion == GOD_TROG)
            {
                piety_change = -level;
                penance = level * 5;
                retval = true;
            }
            break;

        case DID_SPELL_PRACTISE:
            // Like CAST, but for skill advancement.
            // Level is number of skill points gained...
            // typically 10 * exercise, but may be more/less if the
            // skill is at 0 (INT adjustment), or if the PC's pool is
            // low and makes change.
            if (you.religion == GOD_SIF_MUNA)
            {
                // Old curve: random2(12) <= spell-level, this is
                // similar, but faster at low levels (to help ease
                // things for low level spells).  Power averages about
                // (level * 20 / 3) + 10 / 3 now.  Also note that spell
                // skill practise comes just after XP gain, so magical
                // kills tend to do both at the same time (unlike
                // melee).  This means high level spells probably work
                // pretty much like they used to (use spell, get piety).
                piety_change = div_rand_round(level + 10, 80);
#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS, "Spell practise, level: %d, dpiety: %d",
                        level, piety_change);
#endif
                retval = true;
            }
            break;

        case DID_CARDS:
            if (you.religion == GOD_NEMELEX_XOBEH)
            {
                piety_change = level;
                retval = true;

                // level == 0: stacked, deck not used up
                // level == 1: used up or nonstacked
                // level == 2: used up and nonstacked
                // and there's a 1/3 chance of an additional bonus point
                // for nonstacked cards.
                int chance = 0;
                switch (level)
                {
                case 0: chance = 0;   break;
                case 1: chance = 40;  break;
                case 2: chance = 70;  break;
                default:
                case 3: chance = 100; break;
                }

                if (x_chance_in_y(chance, 100)
                    && you.attribute[ATTR_CARD_COUNTDOWN])
                {
                    you.attribute[ATTR_CARD_COUNTDOWN]--;
#if DEBUG_DIAGNOSTICS || DEBUG_CARDS || DEBUG_GIFTS
                    mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                         you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
                }
            }
            break;

        case DID_CAUSE_GLOWING:
        case DID_DELIBERATE_MUTATING:
            if (you.religion == GOD_ZIN)
            {
                if (!known && thing_done != DID_CAUSE_GLOWING)
                {
                    simple_god_message(" forgives your inadvertent chaotic "
                                       "act, just this once.");
                    break;
                }
                else
                {
                    static long last_glowing_lecture = -1L;
                    if (last_glowing_lecture != you.num_turns)
                    {
                        simple_god_message(" does not appreciate the mutagenic "
                                           "glow surrounding you!");
                        last_glowing_lecture = you.num_turns;
                    }
                }
                piety_change = -level;
                retval = true;
            }
            break;

        // level depends on intelligence: normal -> 1, high -> 2
        // cannibalism is still worse
        case DID_EAT_SOULED_BEING:
            if (you.religion == GOD_ZIN)
            {
                piety_change = -level * 5;
                if (level > 1)
                    penance = 5;
                retval = true;
            }
            break;

        case DID_CHAOS:
            if (you.religion == GOD_ZIN)
            {
                retval = true;
                if (!known)
                {
                    simple_god_message(" forgives your inadvertent chaotic "
                                       "act, just this once.");
                    break;
                }
                piety_change = -level;
                penance      = level;
            }
            break;

        case DID_DESTROY_ORCISH_IDOL:
            if (you.religion == GOD_BEOGH)
            {
                piety_change = -level;
                penance = level * 3;
                retval = true;
            }
            break;

        case DID_STABBING:                          // unused
        case DID_STIMULANTS:                        // unused
        case DID_EAT_MEAT:                          // unused
        case DID_CREATE_LIFE:                       // unused
        case DID_SPELL_NONUTILITY:                  // unused
        case NUM_CONDUCTS:
            break;
        }

        if (piety_change > 0)
            gain_piety(piety_change);
        else
            _dock_piety(-piety_change, penance);

#if DEBUG_DIAGNOSTICS
        if (retval)
        {
            static const char *conducts[] =
            {
                "",
                "Necromancy", "Holy", "Unholy", "Attack Holy", "Attack Neutral",
                "Attack Friend", "Friend Died", "Stab", "Unchivalric Attack",
                "Poison", "Field Sacrifice", "Kill Living", "Kill Undead",
                "Kill Demon", "Kill Natural Evil", "Kill Chaotic",
                "Kill Wizard", "Kill Priest", "Kill Holy",
                "Undead Slave Kill Living", "Servant Kill Living",
                "Undead Slave Kill Undead", "Servant Kill Undead",
                "Undead Slave Kill Demon", "Servant Kill Demon",
                "Servant Kill Natural Evil", "Undead Slave Kill Holy",
                "Servant Kill Holy", "Spell Memorise", "Spell Cast",
                "Spell Practise", "Spell Nonutility", "Cards", "Stimulants",
                "Drink Blood", "Cannibalism", "Eat Meat", "Eat Souled Being",
                "Deliberate Mutation", "Cause Glowing", "Use Chaos",
                "Destroy Orcish Idol", "Create Life"
            };

            COMPILE_CHECK(ARRAYSZ(conducts) == NUM_CONDUCTS, c1);
            mprf(MSGCH_DIAGNOSTICS,
                 "conduct: %s; piety: %d (%+d); penance: %d (%+d)",
                 conducts[thing_done],
                 you.piety, piety_change, you.penance[you.religion], penance);

        }
#endif
    }

    _do_god_revenge(thing_done);

    return (retval);
}

// These two arrays deal with the situation where a beam hits a non-fleeing
// monster, the monster starts to flee because of the damage, and then the
// beam bounces and hits the monster again.  If the monster wasn't fleeing
// when the beam started then hits from bounces shouldn't count as
// unchivalric attacks, but if the first hit from the beam *was* unchivalrous
// then all the bounces should count as unchivalrous as well.
//
// Also do the same sort of check for harming a friendly monster,
// since a Beogh worshipper zapping an orc with lightning might cause it to
// become a follower on the first hit, and the second hit would be
// against a friendly orc.
static FixedVector<bool, NUM_MONSTERS> _first_attack_conduct;
static FixedVector<bool, NUM_MONSTERS> _first_attack_was_unchivalric;
static FixedVector<bool, NUM_MONSTERS> _first_attack_was_friendly;

void religion_turn_start()
{
    if (you.turn_is_over)
        religion_turn_end();

    _first_attack_conduct.init(true);
    _first_attack_was_unchivalric.init(false);
    _first_attack_was_friendly.init(false);
    crawl_state.clear_god_acting();
}

void religion_turn_end()
{
    ASSERT(you.turn_is_over);
    _place_delayed_monsters();
}

#define NEW_GIFT_FLAGS (MF_JUST_SUMMONED | MF_GOD_GIFT)

void set_attack_conducts(god_conduct_trigger conduct[3], const monsters *mon,
                         bool known)
{
    const unsigned int midx = monster_index(mon);

    if (mons_friendly(mon))
    {
        if ((mon->flags & NEW_GIFT_FLAGS) == NEW_GIFT_FLAGS
            && mon->god != GOD_XOM)
        {
            mprf(MSGCH_ERROR, "Newly created friendly god gift '%s' was hurt "
                 "by you, shouldn't be possible; please file a bug report.",
                 mon->name(DESC_PLAIN, true).c_str());
            _first_attack_was_friendly[midx] = true;
        }
        else if(_first_attack_conduct[midx]
           || _first_attack_was_friendly[midx])
        {
            conduct[0].set(DID_ATTACK_FRIEND, 5, known, mon);
            _first_attack_was_friendly[midx] = true;
        }
    }
    else if (mons_neutral(mon))
        conduct[0].set(DID_ATTACK_NEUTRAL, 5, known, mon);

    if (is_unchivalric_attack(&you, mon)
        && (_first_attack_conduct[midx]
            || _first_attack_was_unchivalric[midx]))
    {
        conduct[1].set(DID_UNCHIVALRIC_ATTACK, 4, known, mon);
        _first_attack_was_unchivalric[midx] = true;
    }

    if (mons_is_holy(mon))
        conduct[2].set(DID_ATTACK_HOLY, mon->hit_dice, known, mon);

    _first_attack_conduct[midx] = false;
}

void enable_attack_conducts(god_conduct_trigger conduct[3])
{
    for (int i = 0; i < 3; ++i)
        conduct[i].enabled = true;
}

void disable_attack_conducts(god_conduct_trigger conduct[3])
{
    for (int i = 0; i < 3; ++i)
        conduct[i].enabled = false;
}

static void _dock_piety(int piety_loss, int penance)
{
    static long last_piety_lecture   = -1L;
    static long last_penance_lecture = -1L;

    if (piety_loss <= 0 && penance <= 0)
        return;

    if (piety_loss)
    {
        if (last_piety_lecture != you.num_turns)
        {
            // output guilt message:
            mprf( "You feel%sguilty.",
                  (piety_loss == 1) ? " a little " :
                  (piety_loss <  5) ? " " :
                  (piety_loss < 10) ? " very "
                  : " extremely " );
        }

        last_piety_lecture = you.num_turns;
        lose_piety( piety_loss );
    }

    if (you.piety < 1)
        excommunication();
    else if (penance)       // only if still in religion
    {
        if (last_penance_lecture != you.num_turns)
        {
            god_speaks(you.religion,
                       "\"You will pay for your transgression, mortal!\"");
        }
        last_penance_lecture = you.num_turns;
        _inc_penance(penance);
    }
}

void gain_piety(int pgn)
{
    if (pgn <= 0)
        return;

    // Xom uses piety differently...
    if (you.religion == GOD_NO_GOD || you.religion == GOD_XOM)
        return;

    // check to see if we owe anything first
    if (you.penance[you.religion] > 0)
    {
        dec_penance(pgn);
        return;
    }
    else if (you.gift_timeout > 0)
    {
        if (you.gift_timeout > pgn)
            you.gift_timeout -= pgn;
        else
            you.gift_timeout = 0;

        // Slow down piety gain to account for the fact that gifts
        // no longer have a piety cost for getting them
        if (!one_chance_in(4))
        {
#if DEBUG_PIETY
            mprf(MSGCH_DIAGNOSTICS, "Piety slowdown due to gift timeout.");
#endif
            return;
        }
    }

    // slow down gain at upper levels of piety
    if (you.religion != GOD_SIF_MUNA)
    {
        if (you.piety >= MAX_PIETY
            || you.piety > 150 && one_chance_in(3)
            || you.piety > 100 && one_chance_in(3))
        {
            _do_god_gift(false);
            return;
        }
    }
    else
    {
        // Sif Muna has a gentler taper off because training becomes
        // naturally slower as the player gains in spell skills.
        if (you.piety >= MAX_PIETY
            || you.piety > 150 && one_chance_in(5))
        {
            _do_god_gift(false);
            return;
        }
    }

    // Apply hysteresis.
    {
        // piety_hysteresis is the amount of _loss_ stored up, so this
        // may look backwards.
        const int old_hysteresis = you.piety_hysteresis;
        you.piety_hysteresis =
            (unsigned char)std::max<int>(0, you.piety_hysteresis - pgn);
        const int pgn_borrowed = (old_hysteresis - you.piety_hysteresis);
        pgn -= pgn_borrowed;

#if DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "Piety increasing by %d (and %d taken from "
                                "hysteresis)", pgn, pgn_borrowed);
#endif
    }

    int old_piety = you.piety;

    you.piety += pgn;
    you.piety = std::min<int>(MAX_PIETY, you.piety);

    for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
    {
        if (you.piety >= piety_breakpoint(i)
            && old_piety < piety_breakpoint(i))
        {
            take_note(Note(NOTE_GOD_POWER, you.religion, i));

            const char* pmsg = god_gain_power_messages[you.religion][i];
            const char first = pmsg[0];

            if (first)
            {
                if (isupper(first))
                    god_speaks(you.religion, pmsg);
                else
                {
                    god_speaks(you.religion,
                               make_stringf("You can now %s.", pmsg).c_str());
                }

                learned_something_new(TUT_NEW_ABILITY);
            }

            if (you.religion == GOD_SHINING_ONE)
            {
                if (i == 0)
                    mpr("A divine halo surrounds you!");
            }

            // When you gain a piety level, you get another chance to
            // make hostile holy beings good neutral.
            if (is_good_god(you.religion))
                _holy_beings_attitude_change();
        }
    }

    if (you.religion == GOD_BEOGH)
    {
        // Every piety level change also affects AC from orcish gear.
        you.redraw_armour_class = true;
    }

    if (you.piety > 160 && old_piety <= 160)
    {
        if ((you.religion == GOD_SHINING_ONE || you.religion == GOD_LUGONU)
            && you.num_gifts[you.religion] == 0)
        {
            simple_god_message( " will now bless your weapon at an altar... once.");
        }

        // When you gain piety of more than 160, you get another chance
        // to make hostile holy beings good neutral.
        if (is_good_god(you.religion))
            _holy_beings_attitude_change();
    }

    _do_god_gift(false);
}

bool is_holy_item(const item_def& item)
{
    bool retval = false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        retval = (is_blessed_blade(item)
                  || get_weapon_brand(item) == SPWPN_HOLY_WRATH);
        break;
    case OBJ_SCROLLS:
        retval = (item.sub_type == SCR_HOLY_WORD);
        break;
    case OBJ_BOOKS:
        retval = is_holy_spellbook(item);
        break;
    case OBJ_STAVES:
        retval = is_holy_rod(item);
        break;
    default:
        break;
    }

    return (retval);
}

bool is_evil_item(const item_def& item)
{
    bool retval = false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        const int item_eff = item_special_wield_effect(item);

        retval = (is_demonic(item)
                  || item.special == SPWPN_SCEPTRE_OF_ASMODEUS
                  || item.special == SPWPN_STAFF_OF_DISPATER
                  || item.special == SPWPN_SWORD_OF_CEREBOV
                  || item_eff == SPWLD_CURSE
                  || item_eff == SPWLD_TORMENT
                  || item_eff == SPWLD_ZONGULDROK
                  || item_brand == SPWPN_DRAINING
                  || item_brand == SPWPN_PAIN
                  || item_brand == SPWPN_VAMPIRICISM
                  || item_brand == SPWPN_SHADOW);
        }
        break;
    case OBJ_MISSILES:
    {
        const int item_brand = get_ammo_brand(item);
        retval = is_demonic(item) || item_brand == SPMSL_SHADOW;
        break;
    }
    case OBJ_WANDS:
        retval = (item.sub_type == WAND_DRAINING);
        break;
    case OBJ_SCROLLS:
        retval = (item.sub_type == SCR_SUMMONING
                  || item.sub_type == SCR_TORMENT);
        break;
    case OBJ_POTIONS:
        retval = is_blood_potion(item);
        break;
    case OBJ_BOOKS:
        retval = is_evil_spellbook(item);
        break;
    case OBJ_STAVES:
        retval = (item.sub_type == STAFF_DEATH || is_evil_rod(item));
        break;
    case OBJ_MISCELLANY:
        retval = (item.sub_type == MISC_BOTTLED_EFREET
                  || item.sub_type == MISC_LANTERN_OF_SHADOWS);
        break;
    default:
        break;
    }

    return (retval);
}

bool is_chaotic_item(const item_def& item)
{
    bool retval = false;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        {
        const int item_brand = get_weapon_brand(item);
        retval = (item_brand == SPWPN_CHAOS);
        }
        break;
    case OBJ_MISSILES:
        {
        const int item_brand = get_ammo_brand(item);
        retval = (item_brand == SPMSL_CHAOS);
        }
        break;
    case OBJ_WANDS:
        retval = (item.sub_type == WAND_POLYMORPH_OTHER);
        break;
    case OBJ_POTIONS:
        retval = (item.sub_type == POT_MUTATION);
        break;
    case OBJ_BOOKS:
        retval = is_chaotic_spellbook(item);
        break;
    case OBJ_STAVES:
        retval = is_chaotic_rod(item);
        break;
    default:
        break;
    }

    if (is_random_artefact(item) && randart_wpn_property(item, RAP_MUTAGENIC))
        retval = true;

    return (retval);
}

bool is_holy_discipline(int discipline)
{
    return (discipline & SPTYP_HOLY);
}

bool is_evil_discipline(int discipline)
{
    return (discipline & SPTYP_NECROMANCY);
}

bool is_holy_spell(spell_type spell, god_type god)
{
    UNUSED(god);

    unsigned int disciplines = get_spell_disciplines(spell);

    return (is_holy_discipline(disciplines));
}

bool is_evil_spell(spell_type spell, god_type god)
{
    UNUSED(god);

    unsigned int flags       = get_spell_flags(spell);
    unsigned int disciplines = get_spell_disciplines(spell);

    return ((flags & SPFLAG_UNHOLY) || (is_evil_discipline(disciplines)));
}

bool is_chaotic_spell(spell_type spell, god_type god)
{
    UNUSED(god);

    return (spell == SPELL_POLYMORPH_OTHER || spell == SPELL_ALTER_SELF);
}

// The default suitable() function for is_spellbook_type().
bool is_any_spell(spell_type spell, god_type god)
{
    UNUSED(god);

    return (true);
}

// If book_or_rod is false, only look at actual spellbooks.  Otherwise,
// only look at rods.
bool is_spellbook_type(const item_def& item, bool book_or_rod,
                       bool (*suitable)(spell_type spell, god_type god),
                       god_type god)
{
    const bool is_spellbook = (item.base_type == OBJ_BOOKS
                                  && item.sub_type != BOOK_MANUAL
                                  && item.sub_type != BOOK_DESTRUCTION);
    const bool is_rod = item_is_rod(item);

    if (!is_spellbook && !is_rod)
        return (false);

    if (!book_or_rod && is_rod)
        return (false);

    int total       = 0;
    int total_liked = 0;

    for (int i = 0; i < SPELLBOOK_SIZE; ++i)
    {
        spell_type spell = which_spell_in_book(item, i);
        if (spell == SPELL_NO_SPELL)
            continue;

        total++;
        if (suitable(spell, god))
            total_liked++;
    }

    // If at least half of the available spells are suitable, the whole
    // spellbook or rod is, too.
    return (total_liked >= (total / 2) + 1);
}

bool is_holy_spellbook(const item_def& item)
{
    return (is_spellbook_type(item, false, is_holy_spell));
}

bool is_evil_spellbook(const item_def& item)
{
    return (is_spellbook_type(item, false, is_evil_spell));
}

bool is_chaotic_spellbook(const item_def& item)
{
    return (is_spellbook_type(item, false, is_chaotic_spell));
}

bool god_hates_spellbook(const item_def& item)
{
    return (is_spellbook_type(item, false, god_hates_spell_type));
}

bool is_holy_rod(const item_def& item)
{
    return (is_spellbook_type(item, true, is_holy_spell));
}

bool is_evil_rod(const item_def& item)
{
    return (is_spellbook_type(item, true, is_evil_spell));
}

bool is_chaotic_rod(const item_def& item)
{
    return (is_spellbook_type(item, true, is_chaotic_spell));
}

bool god_hates_rod(const item_def& item)
{
    return (is_spellbook_type(item, true, god_hates_spell_type));
}

bool good_god_hates_item_handling(const item_def &item)
{
    return (is_good_god(you.religion) && is_evil_item(item)
            && (is_demonic(item) || item_type_known(item)));
}

bool god_hates_item_handling(const item_def &item)
{
    switch (you.religion)
    {
    case GOD_ZIN:
        if (item_type_known(item) && is_chaotic_item(item))
            return (true);
        break;

    case GOD_SHINING_ONE:
    {
        if (!item_type_known(item))
            return (false);

        switch (item.base_type)
        {
        case OBJ_WEAPONS:
        {
            const int item_brand = get_weapon_brand(item);
            if (item_brand == SPWPN_VENOM)
                return (true);
            break;
        }

        case OBJ_MISSILES:
        {
            const int item_brand = get_ammo_brand(item);
            if (item_brand == SPMSL_POISONED || item_brand == SPMSL_CURARE)
                return (true);
            break;
        }

        case OBJ_STAVES:
            if (item.sub_type == STAFF_POISON)
                return (true);
            break;

        default:
            break;
        }
        break;
    }

    case GOD_YREDELEMNUL:
        if (item_type_known(item) && is_holy_item(item))
            return (true);
        break;

    case GOD_TROG:
        if (item.base_type == OBJ_BOOKS
            && item.sub_type != BOOK_MANUAL
            && item.sub_type != BOOK_DESTRUCTION)
        {
            return (true);
        }
        break;

    default:
        break;
    }

    if (god_hates_spellbook(item) || god_hates_rod(item))
        return (true);

    return (false);
}

bool god_hates_spell_type(spell_type spell, god_type god)
{
    if (is_good_god(god) && is_evil_spell(spell))
        return (true);

    unsigned int disciplines = get_spell_disciplines(spell);

    switch (god)
    {
    case GOD_ZIN:
        if (is_chaotic_spell(spell))
            return (true);
        break;

    case GOD_SHINING_ONE:
        // TSO hates using poison, but is fine with curing it, resisting
        // it, or destroying it.
        if ((disciplines & SPTYP_POISON) && spell != SPELL_CURE_POISON
            && spell != SPELL_RESIST_POISON && spell != SPELL_IGNITE_POISON)
        {
            return (true);
        }

    case GOD_YREDELEMNUL:
        if (is_holy_spell(spell))
            return (true);
        break;

    default:
        break;
    }

    return (false);
}

bool god_dislikes_spell_type(spell_type spell, god_type god)
{
    if (god_hates_spell_type(spell, god))
        return (true);

    unsigned int flags       = get_spell_flags(spell);
    unsigned int disciplines = get_spell_disciplines(spell);

    switch (god)
    {
    case GOD_SHINING_ONE:
        // TSO probably wouldn't like spells which would put enemies
        // into a state where attacking them would be unchivalrous.
        if (spell == SPELL_CAUSE_FEAR || spell == SPELL_PARALYSE
            || spell == SPELL_CONFUSE || spell == SPELL_MASS_CONFUSION
            || spell == SPELL_SLEEP   || spell == SPELL_MASS_SLEEP)
        {
            return (true);
        }
        break;

    case GOD_XOM:
        // Ideally, Xom would only like spells which have a random effect,
        // are risky to use, or would otherwise amuse him, but that would
        // be a really small number of spells.

        // Xom would probably find these extra boring.
        if (flags & (SPFLAG_HELPFUL | SPFLAG_NEUTRAL | SPFLAG_ESCAPE
                     | SPFLAG_RECOVERY | SPFLAG_MAPPING))
        {
            return (true);
        }

        // Things are more fun for Xom the less the player knows in
        // advance.
        if (disciplines & SPTYP_DIVINATION)
            return (true);

        // Holy spells are probably too useful for Xom to find them
        // interesting.
        if (disciplines & SPTYP_HOLY)
            return (true);
        break;

    case GOD_ELYVILON:
        // A peaceful god of healing wouldn't like combat spells.
        if (disciplines & SPTYP_CONJURATION)
            return (true);

        // Also doesn't like battle spells of the non-conjuration type.
        if (flags & SPFLAG_BATTLE)
            return (true);
        break;

    default:
        break;
    }

    return (false);
}

bool god_dislikes_spell_discipline(int discipline, god_type god)
{
    ASSERT(discipline < (1 << (SPTYP_LAST_EXPONENT + 1)));

    if (is_good_god(god) && is_evil_discipline(discipline))
        return (true);

    switch (god)
    {
    case GOD_SHINING_ONE:
        return (discipline & SPTYP_POISON);

    case GOD_YREDELEMNUL:
        return (discipline & SPTYP_HOLY);

    case GOD_XOM:
        return (discipline & (SPTYP_DIVINATION | SPTYP_HOLY));

    case GOD_ELYVILON:
        return (discipline & (SPTYP_CONJURATION | SPTYP_SUMMONING));

    default:
        break;
    }

    return (false);
}

// Is the destroyed weapon valuable enough to gain piety by doing so?
// Evil weapons are handled specially.
static bool _destroyed_valuable_weapon(int value, int type)
{
    // Artefacts (incl. most randarts).
    if (random2(value) >= random2(250))
        return (true);

    // Medium valuable items are more likely to net piety at low piety.
    // This includes missiles in sufficiently large quantities.
    if (random2(value) >= random2(100)
        && one_chance_in(1 + you.piety/50))
    {
        return (true);
    }

    // If not for the above, missiles shouldn't yield piety.
    if (type == OBJ_MISSILES)
        return (false);

    // Weapons, on the other hand, are always acceptable to boost low piety.
    if (you.piety < 30 || player_under_penance())
        return (true);

    return (false);
}

bool ely_destroy_weapons()
{
    if (you.religion != GOD_ELYVILON)
        return (false);

    god_acting gdact;

    bool success = false;
    for (stack_iterator si(you.pos()); si; ++si)
    {
        item_def& item(*si);
        if (item.base_type != OBJ_WEAPONS
                && item.base_type != OBJ_MISSILES
            || item_is_stationary(item)) // Held in a net?
        {
            continue;
        }

        if (!check_warning_inscriptions(item, OPER_DESTROY))
        {
            mpr("Won't destroy {!D} inscribed item.");
            continue;
        }

        const int value = item_value( item, true );
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Destroyed weapon value: %d", value);
#endif

        piety_gain_t pgain = PIETY_NONE;
        const bool is_evil_weapon = is_evil_item(item);
        if (is_evil_weapon || _destroyed_valuable_weapon(value, item.base_type))
        {
            pgain = PIETY_SOME;
            gain_piety(1);
        }

        if (get_weapon_brand(item) == SPWPN_HOLY_WRATH)
        {
            // Weapons blessed by TSO don't get destroyed but are instead
            // returned whence they came. (jpeg)
//            _print_sacrifice_message(GOD_SHINING_ONE, item, pgain);
            simple_god_message(
                make_stringf(" %sreclaims %s.",
                             pgain == PIETY_SOME ? "gladly " : "",
                             item.name(DESC_NOCAP_THE).c_str()).c_str(),
                GOD_SHINING_ONE);
        }
        else
        {
            // Elyvilon doesn't care about item sacrifices at altars, so
            // I'm stealing _Sacrifice_Messages.
            _print_sacrifice_message(GOD_ELYVILON, item, pgain);
            if (is_evil_weapon)
            {
                // Print this is addition to the above!
                simple_god_message(" welcomes the destruction of this evil "
                                   "weapon.", GOD_ELYVILON);
            }
        }

        destroy_item(si.link());
        success = true;
    }

    if (!success)
        mpr("There are no weapons here to destroy!");

    return (success);
}

// Returns false if the invocation fails (no spellbooks in sight, etc.).
bool trog_burn_spellbooks()
{
    if (you.religion != GOD_TROG)
        return (false);

    god_acting gdact;

    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (si->base_type == OBJ_BOOKS
            && si->sub_type != BOOK_MANUAL
            && si->sub_type != BOOK_DESTRUCTION)
        {
            mpr("Burning your own feet might not be such a smart idea!");
            return (false);
        }
    }

    int totalpiety = 0;

    for (radius_iterator ri(you.pos(), LOS_RADIUS, true, true, true); ri; ++ri)
    {
        // If a grid is blocked, books lying there will be ignored.
        // Allow bombing of monsters.
        const unsigned short cloud = env.cgrid(*ri);
        if (grid_is_solid(grd(*ri))
            || cloud != EMPTY_CLOUD && env.cloud[cloud].type != CLOUD_FIRE)
        {
            continue;
        }

        int count = 0;
        int rarity = 0;
        for (stack_iterator si(*ri); si; ++si)
        {
            if (si->base_type != OBJ_BOOKS
                || si->sub_type == BOOK_MANUAL
                || si->sub_type == BOOK_DESTRUCTION)
            {
                continue;
            }

            // Ignore {!D} inscribed books.
            if (!check_warning_inscriptions(*si, OPER_DESTROY))
            {
                mpr("Won't ignite {!D} inscribed book.");
                continue;
            }

            rarity += book_rarity(si->sub_type);
            // Piety increases by 2 for books never cracked open, else 1.
            // Conversely, rarity influences the duration of the pyre.
            if (!item_type_known(*si))
                totalpiety += 2;
            else
                totalpiety++;

#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Burned book rarity: %d", rarity);
#endif
            destroy_item(si.link());
            count++;
        }

        if (count)
        {
            if (cloud != EMPTY_CLOUD)
            {
                // Reinforce the cloud.
                mpr("The fire roars with new energy!");
                const int extra_dur = count + random2(rarity / 2);
                env.cloud[cloud].decay += extra_dur * 5;
                env.cloud[cloud].set_whose(KC_YOU);
                continue;
            }

            const int duration = std::min(4 + count + random2(rarity/2), 23);
            place_cloud(CLOUD_FIRE, *ri, duration, KC_YOU);

            mpr(count == 1 ? "The book bursts into flames."
                           : "The books burst into flames.", MSGCH_GOD);
        }
    }

    if (!totalpiety)
    {
         mpr("You cannot see a spellbook to ignite!");
         return (false);
    }
    else
    {
         simple_god_message(" is delighted!", GOD_TROG);
         gain_piety(totalpiety);
    }
    return (true);
}

void lose_piety(int pgn)
{
    if (pgn <= 0)
        return;

    const int old_piety = you.piety;

    // Apply hysteresis.
    const int old_hysteresis = you.piety_hysteresis;
    you.piety_hysteresis = (unsigned char)std::min<int>(
        PIETY_HYSTERESIS_LIMIT, you.piety_hysteresis + pgn);
    const int pgn_borrowed = (you.piety_hysteresis - old_hysteresis);
    pgn -= pgn_borrowed;
#if DEBUG_PIETY
    mprf(MSGCH_DIAGNOSTICS,
         "Piety decreasing by %d (and %d added to hysteresis)",
         pgn, pgn_borrowed);
#endif

    if (you.piety - pgn < 0)
        you.piety = 0;
    else
        you.piety -= pgn;

    // Don't bother printing out these messages if you're under
    // penance, you wouldn't notice since all these abilities
    // are withheld.
    if (!player_under_penance() && you.piety != old_piety)
    {
        if (you.piety <= 160 && old_piety > 160
            && (you.religion == GOD_SHINING_ONE || you.religion == GOD_LUGONU)
            && you.num_gifts[you.religion] == 0)
        {
            simple_god_message(" is no longer ready to bless your weapon.");
        }

        for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
        {
            if (you.piety < piety_breakpoint(i)
                && old_piety >= piety_breakpoint(i))
            {
                const char* pmsg = god_lose_power_messages[you.religion][i];
                const char first = pmsg[0];

                if (first)
                {
                    if (isupper(first))
                        god_speaks(you.religion, pmsg);
                    else
                    {
                        god_speaks(you.religion,
                                   make_stringf("You can no longer %s.",
                                                pmsg).c_str());
                    }
                }

                if (_need_water_walking() && !beogh_water_walk())
                    fall_into_a_pool(you.pos(), true, grd(you.pos()));
            }
        }
    }

    if (!_god_accepts_prayer(you.religion) && you.duration[DUR_PRAYER])
        end_prayer();

    if (you.piety > 0 && you.piety <= 5)
        learned_something_new(TUT_GOD_DISPLEASED);

    if (you.religion == GOD_BEOGH)
    {
        // Every piety level change also affects AC from orcish gear.
        you.redraw_armour_class = true;
    }
}

static bool _tso_retribution()
{
    // holy warriors/cleansing theme
    const god_type god = GOD_SHINING_ONE;

    int punishment = random2(7);

    switch (punishment)
    {
    case 0:
    case 1:
    case 2: // summon holy warriors (3/7)
    {
        bool success = false;
        int how_many = 1 + random2(you.experience_level / 5) + random2(3);

        for (int i = 0; i < how_many; ++i)
        {
            if (summon_holy_warrior(100, god, 0, true, true, true))
                success = true;
        }

        simple_god_message(success ? " sends the divine host to punish "
                                     "you for your evil ways!"
                                   : "'s divine host fails to appear.", god);

        break;
    }
    case 3:
    case 4: // cleansing flame (2/7)
        _tso_blasts_cleansing_flame();
        break;
    case 5:
    case 6: // either noisiness or silence (2/7)
        if (coinflip())
        {
            simple_god_message(" booms out: \"Take the path of righteousness! REPENT!\"", god);
            noisy(25, you.pos()); // same as scroll of noise
        }
        else
        {
            god_speaks(god, "You feel the Shining One's silent rage upon you!");
            cast_silence(25);
        }
        break;
    }
    return (false);
}

static void _zin_remove_good_mutations()
{
    if (!how_mutated() || player_mutation_level(MUT_MUTATION_RESISTANCE) == 3)
        return;

    bool success = false;

    simple_god_message(" draws some chaos from your body!", GOD_ZIN);

    bool failMsg = true;

    for (int i = 7; i >= 0; --i)
    {
        if (i <= random2(10) && delete_mutation(RANDOM_GOOD_MUTATION, failMsg))
            success = true;
        else
            failMsg = false;
    }

    if (success && !how_mutated())
    {
        simple_god_message(" rids your body of chaos!", GOD_ZIN);
        dec_penance(GOD_ZIN, 1);
    }
}

static bool _zin_retribution()
{
    // surveillance/creeping doom theme
    const god_type god = GOD_ZIN;

    int punishment = random2(10);

    // If not mutated or can't unmutate, do something else instead.
    if (punishment > 7
        && (!how_mutated()
            || player_mutation_level(MUT_MUTATION_RESISTANCE) == 3))
    {
        punishment = random2(8);
    }

    switch (punishment)
    {
    case 0:
    case 1:
    case 2: // summon eyes or pestilence (30%)
        if (random2(you.experience_level) > 7 && !one_chance_in(5))
        {
            const monster_type eyes[] = {
                MONS_GIANT_EYEBALL, MONS_EYE_OF_DRAINING,
                MONS_EYE_OF_DEVASTATION, MONS_GREAT_ORB_OF_EYES
            };

            const int how_many = 1 + (you.experience_level / 10) + random2(3);
            bool success = false;

            for (int i = 0; i < how_many; ++i)
            {
                const monster_type mon = RANDOM_ELEMENT(eyes);

                coord_def mon_pos;
                if (!monster_random_space(mon, mon_pos))
                    mon_pos = you.pos();

                if (mons_place(
                        mgen_data::hostile_at(mon,
                            mon_pos, 0, 0, true, god)) != -1)
                {
                    success = true;
                }
            }

            if (success)
                god_speaks(god, "You feel Zin's eyes turn towards you...");
            else
            {
                simple_god_message("'s eyes are elsewhere at the moment.",
                                   god);
            }
        }
        else
        {
            bool success = cast_summon_swarm(you.experience_level * 20,
                                             god, true, true);
            simple_god_message(success ? " sends a plague down upon you!"
                                       : "'s plague fails to arrive.", god);
        }
        break;
    case 3:
    case 4: // recital (20%)
        simple_god_message(" recites the Axioms of Law to you!", god);
        switch (random2(3))
        {
        case 0:
            confuse_player(3 + random2(10), false);
            break;
        case 1:
            you.put_to_sleep();
            break;
        case 2:
            you.paralyse(NULL, 3 + random2(10));
            break;
        }
        break;
    case 5:
    case 6: // famine (20%)
        simple_god_message(" sends a famine down upon you!", god);
        make_hungry(you.hunger / 2, false);
        break;
    case 7: // noisiness (10%)
        simple_god_message(" booms out: \"Turn to the light! REPENT!\"", god);
        noisy(25, you.pos()); // same as scroll of noise
        break;
    case 8:
    case 9: // remove good mutations (20%)
        _zin_remove_good_mutations();
        break;
    }
    return (false);
}

static void _ely_dull_inventory_weapons()
{
    int chance = 50;
    int num_dulled = 0;
    int quiver_link;

    you.m_quiver->get_desired_item(NULL, &quiver_link);

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        if (you.inv[i].base_type == OBJ_WEAPONS
            || you.inv[i].base_type == OBJ_MISSILES)
        {
            // Don't dull artefacts at all, or weapons below -1/-1.
            if (you.inv[i].base_type == OBJ_WEAPONS)
            {
                if (is_artefact(you.inv[i]) || you.inv[i].plus <= -1
                    && you.inv[i].plus2 <= -1)
                {
                    continue;
                }
            }
            // Don't dull missiles below -1.
            else if (you.inv[i].plus <= -1)
                continue;

            // 2/3 of the time, don't do anything.
            if (!one_chance_in(3))
                continue;

            bool wielded = false;
            bool quivered = false;

            if (you.inv[i].link == you.equip[EQ_WEAPON])
                wielded = true;
            if (you.inv[i].link == quiver_link)
                quivered = true;

            // Dull the weapon or missile(s).
            if (you.inv[i].plus > -1)
                you.inv[i].plus--;

            if (you.inv[i].base_type == OBJ_WEAPONS
                && you.inv[i].plus2 > -1)
            {
                you.inv[i].plus2--;
            }

            // Update the weapon/ammo display, if necessary.
            if (wielded)
                you.wield_change = true;
            if (quivered)
                you.m_quiver->on_item_fired(you.inv[i]);

            chance += item_value(you.inv[i], true) / 50;
            num_dulled++;
        }
    }

    if (num_dulled > 0)
    {
        if (x_chance_in_y(chance + 1, 100))
            dec_penance(GOD_ELYVILON, 1);

        simple_god_message(
            make_stringf(" dulls %syour weapons.",
                         num_dulled > 1 ? "" : "one of ").c_str(),
            GOD_ELYVILON);
    }
}

static bool _elyvilon_retribution()
{
    // healing/interference with fighting theme
    const god_type god = GOD_ELYVILON;

    simple_god_message("'s displeasure finds you.", god);

    switch (random2(5))
    {
    case 0:
    case 1:
        confuse_player(3 + random2(10), false);
        break;

    case 2: // mostly flavour messages
        MiscastEffect(&you, -god, SPTYP_POISON, one_chance_in(3) ? 1 : 0,
                      "the displeasure of Elyvilon");
        break;

    case 3:
    case 4: // Dull weapons in your inventory.
        _ely_dull_inventory_weapons();
        break;
    }

    return (true);
}

static bool _makhleb_retribution()
{
    // demonic servant theme
    const god_type god = GOD_MAKHLEB;

    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        bool success = (create_monster(
                           mgen_data::hostile_at(
                               static_cast<monster_type>(
                                   MONS_EXECUTIONER + random2(5)),
                               you.pos(), 0, 0, true, god)) != -1);

        simple_god_message(success ? " sends a greater servant after you!"
                                   : "'s greater servant is unavoidably "
                                     "detained.", god);
    }
    else
    {
        int how_many = 1 + (you.experience_level / 7);
        int count = 0;

        for (int i = 0; i < how_many; ++i)
        {
            if (create_monster(
                    mgen_data::hostile_at(
                        static_cast<monster_type>(
                            MONS_NEQOXEC + random2(5)),
                        you.pos(), 0, 0, true, god)) != -1)
            {
                count++;
            }
        }

        simple_god_message(count > 1 ? " sends minions to punish you." :
                           count > 0 ? " sends a minion to punish you."
                                     : "'s minions fail to arrive.", god);
    }

    return (true);
}

static bool _kikubaaqudgha_retribution()
{
    // death/necromancy theme
    const god_type god = GOD_KIKUBAAQUDGHA;

    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        bool success = false;
        int how_many = 1 + (you.experience_level / 5) + random2(3);

        for (int i = 0; i < how_many; ++i)
        {
            if (create_monster(
                    mgen_data::hostile_at(MONS_REAPER,
                        you.pos(), 0, 0, true, god)) != -1)
            {
                success = true;
            }
        }

        if (success)
            simple_god_message(" unleashes Death upon you!", god);
        else
            god_speaks(god, "Death has been delayed... for now.");
    }
    else
    {
        god_speaks(god,
            coinflip() ? "You hear Kikubaaqudgha cackling."
                       : "Kikubaaqudgha's malice focuses upon you.");
        MiscastEffect(&you, -god, SPTYP_NECROMANCY, 5 + you.experience_level,
                      random2avg(88, 3), "the malice of Kikubaaqudgha");
    }

    return (true);
}

static bool _yredelemnul_retribution()
{
    // undead theme
    const god_type god = GOD_YREDELEMNUL;

    if (random2(you.experience_level) > 4)
    {
        if (one_chance_in(4) && animate_dead(&you, you.experience_level * 5,
                                             BEH_HOSTILE, MHITYOU, god, false))
        {
            simple_god_message(" animates the dead around you.", god);
            animate_dead(&you, you.experience_level * 5, BEH_HOSTILE, MHITYOU,
                         god);
        }
        else if (you.religion == god && coinflip()
            && _yred_slaves_abandon_you())
        {
            ;
        }
        else
        {
            int how_many = 1 + random2(1 + (you.experience_level / 5));
            int count = 0;

            for (; how_many > 0; --how_many)
                count += _yred_random_servants(100, true);

            simple_god_message(count > 1 ? " sends servants to punish you." :
                               count > 0 ? " sends a servant to punish you."
                                         : "'s servants fail to arrive.", god);
        }
    }
    else
    {
        simple_god_message("'s anger turns toward you for a moment.", god);
        MiscastEffect(&you, -god, SPTYP_NECROMANCY, 5 + you.experience_level,
                      random2avg(88, 3), "the anger of Yredelemnul");
    }

    return (true);
}

static bool _trog_retribution()
{
    // physical/berserk theme
    const god_type god = GOD_TROG;

    if (coinflip())
    {
        int count = 0;
        int points = 3 + you.experience_level * 3;

        {
            no_messages msg;

            while (points > 0)
            {
                int cost = std::min(random2(8) + 3, points);

                // quick reduction for large values
                if (points > 20 && coinflip())
                {
                    points -= 10;
                    cost = 10;
                }

                points -= cost;

                if (summon_berserker(cost * 20, god, 0, true))
                    count++;
            }
        }

        simple_god_message(count > 1 ? " sends monsters to punish you." :
                           count > 0 ? " sends a monster to punish you."
                                     : " has no time to punish you... now.",
                           god);
    }
    else if (!one_chance_in(3))
    {
        simple_god_message("'s voice booms out, \"Feel my wrath!\"", god);

        // A collection of physical effects that might be better
        // suited to Trog than wild fire magic... messages could
        // be better here... something more along the lines of apathy
        // or loss of rage to go with the anti-berserk effect-- bwr
        switch (random2(6))
        {
        case 0:
            potion_effect(POT_DECAY, 100);
            break;

        case 1:
        case 2:
            lose_stat(STAT_STRENGTH, 1 + random2(you.strength / 5), true,
                      "divine retribution from Trog");
            break;

        case 3:
            if (!you.duration[DUR_PARALYSIS])
            {
                dec_penance(god, 3);
                mpr( "You suddenly pass out!", MSGCH_WARN );
                you.duration[DUR_PARALYSIS] = 2 + random2(6);
            }
            break;

        case 4:
        case 5:
            if (you.duration[DUR_SLOW] < 90)
            {
                dec_penance(god, 1);
                mpr( "You suddenly feel exhausted!", MSGCH_WARN );
                you.duration[DUR_EXHAUSTED] = 100;
                slow_player(100);
            }
            break;
        }
    }
    else
    {
        //jmf: returned Trog's old Fire damage
        // -- actually, this function partially exists to remove that,
        //    we'll leave this effect in, but we'll remove the wild
        //    fire magic. -- bwr
        dec_penance(god, 2);
        mpr("You feel Trog's fiery rage upon you!", MSGCH_WARN);
        MiscastEffect(&you, -god, SPTYP_FIRE, 8 + you.experience_level,
                      random2avg(98, 3), "the fiery rage of Trog");
    }

    return (true);
}

static bool _beogh_retribution()
{
    // orcish theme
    const god_type god = GOD_BEOGH;

    switch (random2(8))
    {
    case 0: // smiting (25%)
    case 1:
        _god_smites_you(GOD_BEOGH);
        break;

    case 2: // send out one or two dancing weapons (12.5%)
    {
        int num_created = 0;
        int num_to_create = (coinflip()) ? 1 : 2;

        // Need a species check, in case this retribution is a result of
        // drawing the Wrath card.
        const bool am_orc = (you.species == SP_HILL_ORC);

        for (int i = 0; i < num_to_create; ++i)
        {
            const int temp_rand = random2(13);
            int wpn_type = ((temp_rand ==  0) ? WPN_CLUB :
                            (temp_rand ==  1) ? WPN_MACE :
                            (temp_rand ==  2) ? WPN_FLAIL :
                            (temp_rand ==  3) ? WPN_MORNINGSTAR :
                            (temp_rand ==  4) ? WPN_DAGGER :
                            (temp_rand ==  5) ? WPN_SHORT_SWORD :
                            (temp_rand ==  6) ? WPN_LONG_SWORD :
                            (temp_rand ==  7) ? WPN_SCIMITAR :
                            (temp_rand ==  8) ? WPN_GREAT_SWORD :
                            (temp_rand ==  9) ? WPN_HAND_AXE :
                            (temp_rand == 10) ? WPN_BATTLEAXE :
                            (temp_rand == 11) ? WPN_SPEAR
                                              : WPN_HALBERD);

            // Create item.
            int slot = items(0, OBJ_WEAPONS, wpn_type,
                             true, you.experience_level,
                             am_orc ? MAKE_ITEM_NO_RACE : MAKE_ITEM_ORCISH,
                             0, 0, GOD_BEOGH);

            if (slot == -1)
                continue;

            item_def& item = mitm[slot];

            // Set item ego type.
            set_item_ego_type(item, OBJ_WEAPONS,
                              am_orc ? SPWPN_ORC_SLAYING : SPWPN_ELECTROCUTION);

            // Manually override item plusses.
            item.plus  = random2(3);
            item.plus2 = random2(3);

            if (coinflip())
                item.flags |= ISFLAG_CURSED;

            // Let the player see what he's being attacked by.
            set_ident_flags(item, ISFLAG_KNOW_TYPE);

            // Now create monster.
            int midx =
                create_monster(
                    mgen_data::hostile_at(MONS_DANCING_WEAPON,
                        you.pos(), 0, 0, true, god));

            // Hand item information over to monster.
            if (midx != -1)
            {
                monsters *mon = &menv[midx];

                // Destroy the old weapon.
                // Arguably we should use destroy_item() here.
                mitm[mon->inv[MSLOT_WEAPON]].clear();
                mon->inv[MSLOT_WEAPON] = NON_ITEM;

                unwind_var<int> save_speedinc(mon->speed_increment);
                if (mon->pickup_item(mitm[slot], false, true))
                {
                    num_created++;

                    // 50% chance of weapon disappearing on "death".
                    if (coinflip())
                        mon->flags |= MF_HARD_RESET;
                }
                else
                {
                    // It wouldn't pick up the weapon.
                    monster_die(mon, KILL_DISMISSED, NON_MONSTER, true, true);
                    mitm[slot].clear();
                }
            }
            else // Didn't work out! Delete item.
                mitm[slot].clear();
        }
        if (num_created > 0)
        {
            std::ostringstream msg;
            msg << " throws "
                << (num_created > 1 ? "implements" : "an implement")
                << " of " << (am_orc ? "orc slaying" : "electrocution")
                << " at you.";
            simple_god_message(msg.str().c_str(), god);
            break;
        } // else fall through
    }
    case 4: // 25%, relatively harmless
    case 5: // in effect, only for penance
        if (you.religion == god && _beogh_followers_abandon_you())
            break;
        // else fall through
    default: // send orcs after you (3/8 to 5/8)
    {
        const int points = you.experience_level + 3
                           + random2(you.experience_level * 3);

        monster_type punisher;
        // "natural" bands
        if (points >= 30) // min: lvl 7, always: lvl 27
            punisher = MONS_ORC_WARLORD;
        else if (points >= 24) // min: lvl 6, always: lvl 21
            punisher = MONS_ORC_HIGH_PRIEST;
        else if (points >= 18) // min: lvl 4, always: lvl 15
            punisher = MONS_ORC_KNIGHT;
        else if (points > 10) // min: lvl 3, always: lvl 8
            punisher = MONS_ORC_WARRIOR;
        else
            punisher = MONS_ORC;

        int mons = create_monster(
                       mgen_data::hostile_at(punisher,
                           you.pos(), 0, MG_PERMIT_BANDS, true, god));

        // sometimes name band leader
        if (mons != -1 && one_chance_in(3))
            give_monster_proper_name(&menv[mons]);

        simple_god_message(
            mons != -1 ? " sends forth an army of orcs."
                       : " is still gathering forces against you.", god);
    }
    }

    return (true);
}

static bool _okawaru_retribution()
{
    // warrior theme
    const god_type god = GOD_OKAWARU;

    int how_many = 1 + (you.experience_level / 5);
    int count = 0;

    for (; how_many > 0; --how_many)
        count += _okawaru_random_servant();

    simple_god_message(count > 0 ? " sends forces against you!"
                                 : "'s forces are busy with other wars.", god);

    return (true);
}

static bool _sif_muna_retribution()
{
    // magic/intelligence theme
    const god_type god = GOD_SIF_MUNA;

    simple_god_message("'s wrath finds you.", god);
    dec_penance(god, 1);

    switch (random2(10))
    {
    case 0:
    case 1:
        lose_stat(STAT_INTELLIGENCE, 1 + random2(you.intel / 5), true,
                  "divine retribution from Sif Muna");
        break;

    case 2:
    case 3:
    case 4:
        confuse_player(3 + random2(10), false);
        break;

    case 5:
    case 6:
        MiscastEffect(&you, -god, SPTYP_DIVINATION, 9, 90,
                      "the will of Sif Muna");
        break;

    case 7:
    case 8:
        if (you.magic_points > 0)
        {
            dec_mp(100);  // This should zero it.
            mpr("You suddenly feel drained of magical energy!", MSGCH_WARN);
        }
        break;

    case 9:
        // This will set all the extendable duration spells to
        // a duration of one round, thus potentially exposing
        // the player to real danger.
        antimagic();
        mpr("You sense a dampening of magic.", MSGCH_WARN);
        break;
    }

    return (true);
}

static bool _lugonu_retribution()
{
    // abyssal servant theme
    const god_type god = GOD_LUGONU;

    if (coinflip())
    {
        simple_god_message("'s wrath finds you!", god);
        MiscastEffect(&you, -god, SPTYP_TRANSLOCATION, 9, 90, "Lugonu's touch");
        // No return - Lugonu's touch is independent of other effects.
    }
    else if (coinflip())
    {
        // Give extra opportunities for embarrassing teleports.
        simple_god_message("'s wrath finds you!", god);
        mpr("Space warps around you!");
        if (!one_chance_in(3))
            you_teleport_now(false);
        else
            random_blink(false);

        // No return.
    }

    if (random2(you.experience_level) > 7 && !one_chance_in(5))
    {
        bool success = (create_monster(
                           mgen_data::hostile_at(
                               static_cast<monster_type>(
                                   MONS_GREEN_DEATH + random2(3)),
                               you.pos(), 0, 0, true, god)) != -1);

        simple_god_message(success ? " sends a demon after you!"
                                   : "'s demon is unavoidably detained.", god);
    }
    else
    {
        bool success = false;
        int how_many = 1 + (you.experience_level / 7);

        for (int loopy = 0; loopy < how_many; ++loopy)
        {
            if (create_monster(
                   mgen_data::hostile_at(
                       static_cast<monster_type>(
                           MONS_NEQOXEC + random2(5)),
                       you.pos(), 0, 0, true, god)) != -1)
            {
                success = true;
            }
        }

        simple_god_message(success ? " sends minions to punish you."
                                   : "'s minions fail to arrive.", god);
    }

    return (false);
}

static bool _vehumet_retribution()
{
    // conjuration/summoning theme
    const god_type god = GOD_VEHUMET;

    simple_god_message("'s vengeance finds you.", god);
    MiscastEffect(&you, -god, coinflip() ? SPTYP_CONJURATION : SPTYP_SUMMONING,
                   8 + you.experience_level, random2avg(98, 3),
                   "the wrath of Vehumet");
    return (true);
}

static bool _nemelex_retribution()
{
    // card theme
    const god_type god = GOD_NEMELEX_XOBEH;

    // like Xom, this might actually help the player -- bwr
    simple_god_message(" makes you draw from the Deck of Punishment.", god);
    draw_from_deck_of_punishment();
    return (true);
}

bool divine_retribution( god_type god )
{
    ASSERT(god != GOD_NO_GOD);

    // Good gods don't use divine retribution on their followers, and
    // gods don't use divine retribution on followers of gods they don't
    // hate.
    if ((god == you.religion && is_good_god(god))
        || (god != you.religion && !god_hates_your_god(god)))
    {
        return (false);
    }

    god_acting gdact(god, true);

    bool do_more    = true;
    bool did_retrib = true;
    switch (god)
    {
    // One in ten chance that Xom might do something good...
    case GOD_XOM: xom_acts(one_chance_in(10), abs(you.piety - 100)); break;
    case GOD_SHINING_ONE:   do_more = _tso_retribution(); break;
    case GOD_ZIN:           do_more = _zin_retribution(); break;
    case GOD_MAKHLEB:       do_more = _makhleb_retribution(); break;
    case GOD_KIKUBAAQUDGHA: do_more = _kikubaaqudgha_retribution(); break;
    case GOD_YREDELEMNUL:   do_more = _yredelemnul_retribution(); break;
    case GOD_TROG:          do_more = _trog_retribution(); break;
    case GOD_BEOGH:         do_more = _beogh_retribution(); break;
    case GOD_OKAWARU:       do_more = _okawaru_retribution(); break;
    case GOD_LUGONU:        do_more = _lugonu_retribution(); break;
    case GOD_VEHUMET:       do_more = _vehumet_retribution(); break;
    case GOD_NEMELEX_XOBEH: do_more = _nemelex_retribution(); break;
    case GOD_SIF_MUNA:      do_more = _sif_muna_retribution(); break;
    case GOD_ELYVILON:      do_more = _elyvilon_retribution(); break;

    default:
#if DEBUG_DIAGNOSTICS || DEBUG_RELIGION
        mprf(MSGCH_DIAGNOSTICS, "No retribution defined for %s.",
             god_name(god).c_str());
#endif
        do_more    = false;
        did_retrib = false;
        break;
    }

    // Sometimes divine experiences are overwhelming...
    if (do_more && one_chance_in(5) && you.experience_level < random2(37))
    {
        if (coinflip())
        {
            mpr( "The divine experience confuses you!", MSGCH_WARN);
            confuse_player(3 + random2(10));
        }
        else
        {
            if (you.duration[DUR_SLOW] < 90)
            {
                mpr( "The divine experience leaves you feeling exhausted!",
                     MSGCH_WARN );

                slow_player(random2(20));
            }
        }
    }

    // Just the thought of retribution mollifies the god by at least a
    // point...the punishment might have reduced penance further.
    dec_penance(god, 1 + random2(3));

    return (did_retrib);
}

static bool _holy_beings_on_level_attitude_change()
{
    bool success = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (monster->alive()
            && mons_is_holy(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Holy attitude changing: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            // If you worship a good god, you get another chance to make
            // neutral and hostile holy beings good neutral.
            if (is_good_god(you.religion) && !mons_wont_attack(monster))
            {
                if (testbits(monster->flags, MF_ATT_CHANGE_ATTEMPT))
                {
                    monster->flags &= ~MF_ATT_CHANGE_ATTEMPT;

                    success = true;
                }
            }
            // If you don't worship a good god, you make all friendly
            // and good neutral holy beings that worship a good god
            // hostile.
            else if (!is_good_god(you.religion) && mons_wont_attack(monster)
                && is_good_god(monster->god))
            {
                monster->attitude = ATT_HOSTILE;
                monster->del_ench(ENCH_CHARM, true);
                behaviour_event(monster, ME_ALERT, MHITYOU);
                // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

                success = true;
            }
        }
    }

    return success;
}

static bool _holy_beings_attitude_change()
{
    return apply_to_all_dungeons(_holy_beings_on_level_attitude_change);
}

static bool _evil_beings_on_level_attitude_change()
{
    bool success = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (monster->alive()
            && mons_is_evil_or_unholy(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Evil attitude changing: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            // If you worship a good god, you make all friendly and good
            // neutral evil and unholy beings hostile.
            if (is_good_god(you.religion) && mons_wont_attack(monster))
            {
                monster->attitude = ATT_HOSTILE;
                monster->del_ench(ENCH_CHARM, true);
                behaviour_event(monster, ME_ALERT, MHITYOU);
                // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

                success = true;
            }
        }
    }

    return success;
}

static bool _evil_beings_attitude_change()
{
    return apply_to_all_dungeons(_evil_beings_on_level_attitude_change);
}

static bool _chaotic_beings_on_level_attitude_change()
{
    bool success = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (monster->alive()
            && mons_is_chaotic(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Chaotic attitude changing: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            // If you worship Zin, you make all friendly and good neutral
            // chaotic beings hostile.
            if (you.religion == GOD_ZIN && mons_wont_attack(monster))
            {
                monster->attitude = ATT_HOSTILE;
                monster->del_ench(ENCH_CHARM, true);
                behaviour_event(monster, ME_ALERT, MHITYOU);
                // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

                success = true;
            }
        }
    }

    return success;
}

static bool _chaotic_beings_attitude_change()
{
    return apply_to_all_dungeons(_chaotic_beings_on_level_attitude_change);
}

static bool _magic_users_on_level_attitude_change()
{
    bool success = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (monster->alive()
            && mons_is_magic_user(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Magic user attitude changing: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            // If you worship Trog, you make all friendly and good neutral
            // magic users hostile.
            if (you.religion == GOD_TROG && mons_wont_attack(monster))
            {
                monster->attitude = ATT_HOSTILE;
                monster->del_ench(ENCH_CHARM, true);
                behaviour_event(monster, ME_ALERT, MHITYOU);
                // For now CREATED_FRIENDLY/WAS_NEUTRAL stays.

                success = true;
            }
        }
    }

    return success;
}

static bool _magic_users_attitude_change()
{
    return apply_to_all_dungeons(_magic_users_on_level_attitude_change);
}

// Make summoned (temporary) friendly god gifts disappear on penance
// or when abandoning the god in question.  If seen, only count monsters
// where the player can see the change, and output a message.
static bool _make_god_gifts_on_level_disappear(bool seen = false)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;
    int count = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (is_follower(monster)
            && monster->has_ench(ENCH_ABJ)
            && mons_is_god_gift(monster, god))
        {
            if (!seen || simple_monster_message(monster, " abandons you!"))
                count++;

            // The monster disappears.
            monster_die(monster, KILL_DISMISSED, NON_MONSTER);
        }
    }

    return (count);
}

static bool _god_gifts_disappear_wrapper()
{
    return (_make_god_gifts_on_level_disappear());
}

// Make friendly god gifts disappear on all levels, or on only the
// current one.
static bool _make_god_gifts_disappear(bool level_only)
{
    bool success = _make_god_gifts_on_level_disappear(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(_god_gifts_disappear_wrapper) || success);
}

// When abandoning the god in question, turn friendly god gifts good
// neutral.  If seen, only count monsters where the player can see the
// change, and output a message.
static bool _make_holy_god_gifts_on_level_good_neutral(bool seen = false)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;
    int count = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (is_follower(monster)
            && !monster->has_ench(ENCH_CHARM)
            && mons_is_holy(monster)
            && mons_is_god_gift(monster, god))
        {
            // monster changes attitude
            monster->attitude = ATT_GOOD_NEUTRAL;

            if (!seen || simple_monster_message(monster, " becomes indifferent."))
                count++;
        }
    }

    return (count);
}

static bool _holy_god_gifts_good_neutral_wrapper()
{
    return (_make_holy_god_gifts_on_level_good_neutral());
}

// Make friendly holy god gifts turn good neutral on all levels, or on
// only the current one.
static bool _make_holy_god_gifts_good_neutral(bool level_only)
{
    bool success = _make_holy_god_gifts_on_level_good_neutral(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(_holy_god_gifts_good_neutral_wrapper) || success);
}

// When abandoning the god in question, turn friendly god gifts hostile.
// If seen, only count monsters where the player can see the change, and
// output a message.
static bool _make_god_gifts_on_level_hostile(bool seen = false)
{
    const god_type god =
        (crawl_state.is_god_acting()) ? crawl_state.which_god_acting()
                                      : GOD_NO_GOD;
    int count = 0;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (is_follower(monster)
            && mons_is_god_gift(monster, god))
        {
            // monster changes attitude and behaviour
            monster->attitude = ATT_HOSTILE;
            monster->del_ench(ENCH_CHARM, true);
            behaviour_event(monster, ME_ALERT, MHITYOU);

            if (!seen || simple_monster_message(monster, " turns against you!"))
                count++;
        }
    }

    return (count);
}

static bool _god_gifts_hostile_wrapper()
{
    return (_make_god_gifts_on_level_hostile());
}

// Make friendly god gifts turn hostile on all levels, or on only the
// current one.
static bool _make_god_gifts_hostile(bool level_only)
{
    bool success = _make_god_gifts_on_level_hostile(true);

    if (level_only)
        return (success);

    return (apply_to_all_dungeons(_god_gifts_hostile_wrapper) || success);
}

static bool _yred_enslaved_souls_on_level_disappear()
{
    bool success = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (_is_yred_enslaved_soul(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Undead soul disappearing: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            simple_monster_message(monster, " is freed.");

            // The monster disappears.
            monster_die(monster, KILL_DISMISSED, NON_MONSTER);

            success = true;
        }
    }

    return (success);
}

static bool _yred_slaves_on_level_abandon_you()
{
    bool success = false;

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (_is_yred_enslaved_body_and_soul(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Undead soul abandoning: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            yred_make_enslaved_soul(monster, true, true, true);

            success = true;
        }
        else if (is_yred_undead_slave(monster))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Undead abandoning: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            monster->attitude = ATT_HOSTILE;
            behaviour_event(monster, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY stays.

            success = true;
        }
    }

    return (success);
}

static bool _beogh_followers_on_level_abandon_you()
{
    bool success = false;

    // Note that orc high priests' summons are gifts of Beogh, so we
    // can't use is_orcish_follower() here.
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (mons_is_god_gift(monster, GOD_BEOGH))
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Orc abandoning: %s on level %d, branch %d",
                 monster->name(DESC_PLAIN).c_str(),
                 static_cast<int>(you.your_level),
                 static_cast<int>(you.where_are_you));
#endif

            monster->attitude = ATT_HOSTILE;
            behaviour_event(monster, ME_ALERT, MHITYOU);
            // For now CREATED_FRIENDLY stays.

            success = true;
        }
    }

    return (success);
}

static bool _yred_souls_disappear()
{
    return (apply_to_all_dungeons(_yred_enslaved_souls_on_level_disappear));
}

// Upon excommunication, ex-Yredelemnulites lose all their undead
// slaves.  When under penance, Yredelemnulites can lose all undead
// slaves in sight.
static bool _yred_slaves_abandon_you()
{
    bool reclaim = false;
    int num_reclaim = 0;
    int num_slaves = 0;

    if (you.religion != GOD_YREDELEMNUL)
        reclaim =
            apply_to_all_dungeons(_yred_slaves_on_level_abandon_you);
    else
    {
        for (radius_iterator ri(you.pos(), 9); ri; ++ri)
        {
            monsters *monster = monster_at(*ri);
            if (monster == NULL)
                continue;

            if (_is_yred_enslaved_body_and_soul(monster)
                || is_yred_undead_slave(monster))
            {
                num_slaves++;

                const int hd = monster->hit_dice;

                // During penance, followers get a saving throw.
                if (random2((you.piety-you.penance[GOD_YREDELEMNUL])/18) +
                    random2(you.skills[SK_INVOCATIONS]-6)
                    > random2(hd) + hd + random2(5))
                {
                    continue;
                }


                if (_is_yred_enslaved_body_and_soul(monster))
                    yred_make_enslaved_soul(monster, true, true, true);
                else
                {
                    monster->attitude = ATT_HOSTILE;
                    behaviour_event(monster, ME_ALERT, MHITYOU);
                    // For now CREATED_FRIENDLY stays.
                }

                num_reclaim++;

                reclaim = true;
            }
        }
    }

    if (reclaim)
    {
        if (you.religion != GOD_YREDELEMNUL)
        {
            simple_god_message(" reclaims all of your granted undead slaves!",
                               GOD_YREDELEMNUL);
        }
        else if (num_reclaim > 0)
        {
            if (num_reclaim == 1 && num_slaves > 1)
                simple_god_message(" reclaims one of your granted undead slaves!");
            else if (num_reclaim == num_slaves)
                simple_god_message(" reclaims your granted undead slaves!");
            else
                simple_god_message(" reclaims some of your granted undead slaves!");
        }

        return (true);
    }

    return (false);
}

// Upon excommunication, ex-Beoghites lose all their orcish followers,
// plus all monsters created by their priestly orcish followers.  When
// under penance, Beoghites can lose all orcish followers in sight,
// subject to a few limitations.
static bool _beogh_followers_abandon_you()
{
    bool reconvert = false;
    int num_reconvert = 0;
    int num_followers = 0;

    if (you.religion != GOD_BEOGH)
    {
        reconvert =
            apply_to_all_dungeons(_beogh_followers_on_level_abandon_you);
    }
    else
    {
        for (radius_iterator ri(you.pos(), 9); ri; ++ri)
        {
            monsters *monster = monster_at(*ri);
            if (monster == NULL)
                continue;

            // Note that orc high priests' summons are gifts of Beogh,
            // so we can't use is_orcish_follower() here.
            if (mons_is_god_gift(monster, GOD_BEOGH))
            {
                num_followers++;

                if (mons_player_visible(monster)
                    && !mons_is_sleeping(monster)
                    && !mons_is_confused(monster)
                    && !mons_cannot_act(monster))
                {
                    const int hd = monster->hit_dice;

                    // During penance, followers get a saving throw.
                    if (random2((you.piety-you.penance[GOD_BEOGH])/18) +
                        random2(you.skills[SK_INVOCATIONS]-6)
                        > random2(hd) + hd + random2(5))
                    {
                        continue;
                    }

                    monster->attitude = ATT_HOSTILE;
                    behaviour_event(monster, ME_ALERT, MHITYOU);
                    // For now CREATED_FRIENDLY stays.

                    if (you.can_see(monster))
                        num_reconvert++; // Only visible ones.

                    reconvert = true;
                }
            }
        }
    }

    if (reconvert) // Maybe all of them are invisible.
    {
        simple_god_message("'s voice booms out, \"Who do you think you "
                           "are?\"", GOD_BEOGH);

        std::ostream& chan = msg::streams(MSGCH_MONSTER_ENCHANT);

        if (you.religion != GOD_BEOGH)
            chan << "All of your followers decide to abandon you.";
        else if (num_reconvert > 0)
        {
            if (num_reconvert == 1 && num_followers > 1)
                chan << "One of your followers decides to abandon you.";
            else if (num_reconvert == num_followers)
                chan << "Your followers decide to abandon you.";
            else
                chan << "Some of your followers decide to abandon you.";
        }

        chan << std::endl;

        return (true);
    }

    return (false);
}

// Currently only used when orcish idols have been destroyed.
static std::string _get_beogh_speech(const std::string key)
{
    std::string result = getSpeakString("Beogh " + key);

    if (result.empty())
        return ("Beogh is angry!");

    return (result);
}

// Destroying orcish idols (a.k.a. idols of Beogh) may anger Beogh.
static bool _beogh_idol_revenge()
{
    god_acting gdact(GOD_BEOGH, true);

    // Beogh watches his charges closely, but for others doesn't always
    // notice.
    if (you.religion == GOD_BEOGH
        || (you.species == SP_HILL_ORC && coinflip())
        || one_chance_in(3))
    {
        const char *revenge;

        if (you.religion == GOD_BEOGH)
            revenge = _get_beogh_speech("idol follower").c_str();
        else if (you.species == SP_HILL_ORC)
            revenge = _get_beogh_speech("idol hill orc").c_str();
        else
            revenge = _get_beogh_speech("idol other").c_str();

        _god_smites_you(GOD_BEOGH, revenge);

        return (true);
    }

    return (false);
}

static void _print_good_god_holy_being_speech(bool neutral,
                                              const std::string key,
                                              monsters *mon,
                                              msg_channel_type channel)
{
    std::string full_key = "good_god_";
    if (!neutral)
        full_key += "non";
    full_key += "neutral_holy_being_";
    full_key += key;

    std::string msg = getSpeakString(full_key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        mpr(msg.c_str(), channel);
    }
}

// Holy monsters may turn good neutral when encountering followers of
// the good gods, and be made worshippers of TSO if necessary.
void good_god_holy_attitude_change(monsters *holy)
{
    ASSERT(mons_is_holy(holy));

    if (you.can_see(holy)) // show reaction
    {
        _print_good_god_holy_being_speech(true, "reaction", holy,
                                          MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3))
            _print_good_god_holy_being_speech(true, "speech", holy,
                                              MSGCH_TALK);
    }

    holy->attitude = ATT_GOOD_NEUTRAL;

    // The monster is not really *created* neutral, but should it become
    // hostile later on, it won't count as a good kill.
    holy->flags |= MF_WAS_NEUTRAL;

    // If the holy being was previously worshipping a different god,
    // make it worship TSO.
    holy->god = GOD_SHINING_ONE;

    // Avoid immobile "followers".
    behaviour_event(holy, ME_ALERT, MHITNOT);
}

void good_god_holy_fail_attitude_change(monsters *holy)
{
    ASSERT(mons_is_holy(holy));

    if (you.can_see(holy)) // show reaction
    {
        _print_good_god_holy_being_speech(false, "reaction", holy,
                                          MSGCH_FRIEND_ENCHANT);

        if (!one_chance_in(3))
            _print_good_god_holy_being_speech(false, "speech", holy,
                                              MSGCH_TALK);
    }
}

static void _tso_blasts_cleansing_flame(const char *message)
{
    // TSO won't protect you from his own cleansing flame, and Xom is too
    // capricious to protect you from it.
    if (you.religion != GOD_SHINING_ONE && you.religion != GOD_XOM
        && !player_under_penance() && x_chance_in_y(you.piety, MAX_PIETY * 2))
    {
        god_speaks(you.religion,
                   make_stringf("\"Mortal, I have averted the wrath of %s... "
                                "this time.\"",
                                god_name(GOD_SHINING_ONE).c_str()).c_str());
    }
    else
    {
        // If there's a message, display it before firing.
        if (message)
            god_speaks(GOD_SHINING_ONE, message);

        simple_god_message(" blasts you with cleansing flame!",
                           GOD_SHINING_ONE);

        cleansing_flame(20 + (you.experience_level * 7) / 3,
                        CLEANSING_FLAME_TSO, you.pos());
    }
}

// Currently only used when holy beings have been killed.
static std::string _get_tso_speech(const std::string key)
{
    std::string result = getSpeakString("TSO " + key);

    if (result.empty())
        return ("The Shining One is angry!");

    return (result);
}

// Killing holy beings may anger TSO.
static bool _tso_holy_revenge()
{
    god_acting gdact(GOD_SHINING_ONE, true);

    // TSO watches evil god worshippers more closely.
    if (!is_good_god(you.religion)
        && ((is_evil_god(you.religion) && one_chance_in(3))
            || one_chance_in(4)))
    {
        const char *revenge;

        if (is_evil_god(you.religion))
            revenge = _get_tso_speech("holy evil").c_str();
        else
            revenge = _get_tso_speech("holy other").c_str();

        _tso_blasts_cleansing_flame(revenge);

        return (true);
    }

    return (false);
}

void yred_make_enslaved_soul(monsters *mon, bool force_hostile,
                             bool quiet, bool unlimited)
{
    if (!unlimited)
        _yred_souls_disappear();

    const int type = mon->type;
    monster_type soul_type = mons_species(type);
    const std::string whose =
        you.can_see(mon) ? apostrophise(mon->name(DESC_CAP_THE))
                         : mon->pronoun(PRONOUN_CAP_POSSESSIVE);
    const bool twisted = coinflip();
    int corps = -1;

    // If the monster's held in a net, get it out.
    mons_clear_trapping_net(mon);

    const monsters orig = *mon;

    if (twisted)
    {
        mon->type = mons_zombie_size(soul_type) == Z_BIG ?
            MONS_ABOMINATION_LARGE : MONS_ABOMINATION_SMALL;
        mon->base_monster = MONS_PROGRAM_BUG;
    }
    else
    {
        // Drop the monster's corpse, so that it can be properly
        // re-equipped below.
        corps = place_monster_corpse(mon, true, true);
    }

    // Drop the monster's equipment.
    monster_drop_ething(mon);

    // Recreate the monster as an abomination, or as itself before
    // turning it into a spectral thing below.
    define_monster(*mon);

    mon->colour = ETC_UNHOLY;

    mon->flags |= MF_CREATED_FRIENDLY;
    mon->flags |= MF_ENSLAVED_SOUL;

    if (twisted)
        // Mark abominations as undead.
        mon->flags |= MF_HONORARY_UNDEAD;
    else if (corps != -1)
    {
        // Turn the monster into a spectral thing, minus the usual
        // adjustments for zombified monsters.
        mon->type = MONS_SPECTRAL_THING;
        mon->base_monster = soul_type;

        // Re-equip the spectral thing.
        equip_undead(mon->pos(), corps, monster_index(mon),
                     mon->base_monster);

        // Destroy the monster's corpse, as it's no longer needed.
        destroy_item(corps);
    }

    name_zombie(mon, &orig);

    mons_make_god_gift(mon, GOD_YREDELEMNUL);

    mon->attitude = !force_hostile ? ATT_FRIENDLY : ATT_HOSTILE;
    behaviour_event(mon, ME_ALERT, !force_hostile ? MHITNOT : MHITYOU);

    if (!quiet)
    {
        mprf("%s soul %s, and %s.", whose.c_str(),
             twisted        ? "becomes twisted" : "remains intact",
             !force_hostile ? "is now yours"    : "fights you");
    }
}

static void _print_converted_orc_speech(const std::string key,
                                        monsters *mon,
                                        msg_channel_type channel)
{
    std::string msg = getSpeakString("beogh_converted_orc_" + key);

    if (!msg.empty())
    {
        msg = do_mon_str_replacements(msg, mon);
        mpr(msg.c_str(), channel);
    }
}

// Orcs may turn friendly when encountering followers of Beogh, and be
// made gifts of Beogh.
void beogh_convert_orc(monsters *orc, bool emergency,
                       bool converted_by_follower)
{
    ASSERT(mons_species(orc->type) == MONS_ORC);

    if (you.can_see(orc)) // show reaction
    {
        if (emergency || !orc->alive())
        {
            if (converted_by_follower)
            {
                _print_converted_orc_speech("reaction_battle_follower", orc,
                                            MSGCH_FRIEND_ENCHANT);
                _print_converted_orc_speech("speech_battle_follower", orc,
                                            MSGCH_TALK);
            }
            else
            {
                _print_converted_orc_speech("reaction_battle", orc,
                                            MSGCH_FRIEND_ENCHANT);
                _print_converted_orc_speech("speech_battle", orc, MSGCH_TALK);
            }
        }
        else
        {
            _print_converted_orc_speech("reaction_sight", orc,
                                        MSGCH_FRIEND_ENCHANT);

            if (!one_chance_in(3))
                _print_converted_orc_speech("speech_sight", orc, MSGCH_TALK);
        }
    }

    orc->attitude = ATT_FRIENDLY;

    // The monster is not really *created* friendly, but should it
    // become hostile later on, it won't count as a good kill.
    orc->flags |= MF_CREATED_FRIENDLY;

    // Prevent assertion if the orc was previously worshipping a
    // different god, rather than already worshipping Beogh or being an
    // atheist.
    orc->god = GOD_NO_GOD;

    mons_make_god_gift(orc, GOD_BEOGH);

    if (orc->is_patrolling())
    {
        // Make orcs stop patrolling and forget their patrol point,
        // they're supposed to follow you now.
        orc->patrol_point = coord_def(0, 0);
    }

    if (!orc->alive())
        orc->hit_points = std::min(random_range(1, 4), orc->max_hit_points);

    // Avoid immobile "followers".
    behaviour_event(orc, ME_ALERT, MHITNOT);
}

void excommunication(god_type new_god)
{
    const god_type old_god = you.religion;
    ASSERT(old_god != new_god);

    const bool was_haloed = you.haloed();
    const int  old_piety  = you.piety;

    god_acting gdact(old_god, true);

    take_note(Note(NOTE_LOSE_GOD, old_god));

    you.duration[DUR_PRAYER] = 0;
    you.duration[DUR_PIETY_POOL] = 0; // your loss
    you.religion = GOD_NO_GOD;
    you.piety = 0;
    you.piety_hysteresis = 0;
    redraw_skill(you.your_name, player_title());

    mpr("You have lost your religion!");
    more();

    if (god_hates_your_god(old_god, new_god))
    {
        simple_god_message(
            make_stringf(" does not appreciate desertion%s!",
                         god_hates_your_god_reaction(old_god, new_god).c_str()).c_str(),
            old_god);
    }

    switch (old_god)
    {
    case GOD_XOM:
        xom_acts(false, abs(you.piety - 100) * 2);
        _inc_penance(old_god, 50);
        break;

    case GOD_KIKUBAAQUDGHA:
        MiscastEffect(&you, -old_god, SPTYP_NECROMANCY,
                      5 + you.experience_level, random2avg(88, 3),
                      "the malice of Kikubaaqudgha");
        _inc_penance(old_god, 30);
        break;

    case GOD_YREDELEMNUL:
        _yred_slaves_abandon_you();

        MiscastEffect(&you, -old_god, SPTYP_NECROMANCY,
                      5 + you.experience_level, random2avg(88, 3),
                      "the anger of Yredelemnul");
        _inc_penance(old_god, 30);
        break;

    case GOD_VEHUMET:
        MiscastEffect(&you, -old_god,
                      (coinflip() ? SPTYP_CONJURATION : SPTYP_SUMMONING),
                      8 + you.experience_level, random2avg(98, 3),
                      "the wrath of Vehumet");
        _inc_penance(old_god, 25);
        break;

    case GOD_MAKHLEB:
        MiscastEffect(&you, -old_god,
                      (coinflip() ? SPTYP_CONJURATION : SPTYP_SUMMONING),
                      8 + you.experience_level, random2avg(98, 3),
                      "the fury of Makhleb");
        _inc_penance(old_god, 25);
        break;

    case GOD_TROG:
        _make_god_gifts_hostile(false);

        // Penance has to come before retribution to prevent "mollify"
        _inc_penance(old_god, 50);
        divine_retribution(old_god);
        break;

    case GOD_BEOGH:
        _beogh_followers_abandon_you(); // friendly orcs around turn hostile

        // You might have lost water walking at a bad time...
        if (_need_water_walking())
            fall_into_a_pool(you.pos(), true, grd(you.pos()));

        // Penance has to come before retribution to prevent "mollify"
        _inc_penance(old_god, 50);
        divine_retribution(old_god);
        break;

    case GOD_SIF_MUNA:
        _inc_penance(old_god, 50);
        break;

    case GOD_NEMELEX_XOBEH:
        nemelex_shuffle_decks();
        _inc_penance(old_god, 150); // Nemelex penance is special
        break;

    case GOD_LUGONU:
        if (you.level_type == LEVEL_DUNGEON)
        {
            simple_god_message(" casts you into the Abyss!", old_god);
            banished(DNGN_ENTER_ABYSS, "Lugonu's wrath");
        }
        _inc_penance(old_god, 50);
        break;

    case GOD_SHINING_ONE:
        if (was_haloed)
            mpr("Your divine halo fades away.");

        if (you.duration[DUR_DIVINE_SHIELD])
            remove_divine_shield();

        // Leaving TSO for a non-good god will make all your followers
        // abandon you.  Leaving him for a good god will make your holy
        // followers (his daeva and angel servants) indifferent, while
        // leaving your other followers (blessed with friendliness by
        // his power, but not his servants) alone.
        if (!is_good_god(new_god))
        {
            _inc_penance(old_god, 50);
            _make_god_gifts_hostile(false);
        }
        else
            _make_holy_god_gifts_good_neutral(false);

        break;

    case GOD_ZIN:
        if (you.duration[DUR_DIVINE_STAMINA])
            remove_divine_stamina();

        if (env.sanctuary_time)
            remove_sanctuary();

        // Leaving Zin for a non-good god will make all your followers
        // (originally from TSO) abandon you.
        if (!is_good_god(new_god))
        {
            _inc_penance(old_god, 25);

            god_acting good_gdact(GOD_SHINING_ONE, true);

            _make_god_gifts_hostile(false);
        }
        break;

    case GOD_ELYVILON:
        if (you.duration[DUR_DIVINE_VIGOUR])
            remove_divine_vigour();

        // Leaving Elyvilon for a non-good god will make all your
        // followers (originally from TSO) abandon you.
        if (!is_good_god(new_god))
        {
            _inc_penance(old_god, 30);

            god_acting good_gdact(GOD_SHINING_ONE, true);

            _make_god_gifts_hostile(false);
        }
        break;

    default:
        _inc_penance(old_god, 25);
        break;
    }

    // When you start worshipping a non-good god, or no god, you make
    // all non-hostile holy beings that worship a good god hostile.
    if (!is_good_god(new_god) && _holy_beings_attitude_change())
        mpr("The divine host forsakes you.", MSGCH_MONSTER_ENCHANT);

    // Evil hack.
    learned_something_new(TUT_EXCOMMUNICATE,
                          coord_def((int)new_god, old_piety));
}

static bool _bless_weapon(god_type god, brand_type brand, int colour)
{
    item_def& wpn = *you.weapon();

    // Only bless melee weapons.
    if (!is_artefact(wpn) && !is_range_weapon(wpn))
    {
        you.duration[DUR_WEAPON_BRAND] = 0;     // just in case

        set_equip_desc(wpn, ISFLAG_GLOWING);
        set_item_ego_type(wpn, OBJ_WEAPONS, brand);
        wpn.colour = colour;

        const bool is_cursed = item_cursed(wpn);

        enchant_weapon(ENCHANT_TO_HIT, true, wpn);

        if (coinflip())
            enchant_weapon(ENCHANT_TO_HIT, true, wpn);

        enchant_weapon(ENCHANT_TO_DAM, true, wpn);

        if (coinflip())
            enchant_weapon(ENCHANT_TO_DAM, true, wpn);

        if (is_cursed)
            do_uncurse_item(wpn);

        if (god == GOD_SHINING_ONE)
        {
            convert2good(wpn);

            if (is_blessed_blade_convertible(wpn))
            {
                origin_acquired(wpn, GOD_SHINING_ONE);
                make_item_blessed_blade(wpn);
            }

            burden_change();
        }

        you.wield_change = true;
        you.num_gifts[god]++;
        take_note(Note(NOTE_GOD_GIFT, you.religion));

        you.flash_colour = colour;
        viewwindow(true, false);

        mprf(MSGCH_GOD, "Your weapon shines brightly!");
        simple_god_message(" booms: Use this gift wisely!");

        if (god == GOD_SHINING_ONE)
        {
            holy_word(100, HOLY_WORD_TSO, you.pos(), true);
#ifndef USE_TILE
            delay(1000);
#endif

            // Un-bloodify surrounding squares.
            for (radius_iterator ri(you.pos(), 3, true, true); ri; ++ri)
                if (is_bloodcovered(*ri))
                    env.map(*ri).property &= ~(FPROP_BLOODY);
        }

        return (true);
    }

    return (false);
}

static void _replace(std::string& s,
                     const std::string &find,
                     const std::string &repl)
{
    std::string::size_type start = 0;
    std::string::size_type found;

    while ((found = s.find(find, start)) != std::string::npos)
    {
        s.replace( found, find.length(), repl );
        start = found + repl.length();
    }
}

static void _print_sacrifice_message(god_type god, const item_def &item,
                                     piety_gain_t piety_gain, bool your)
{
    std::string msg(_Sacrifice_Messages[god][piety_gain]);
    _replace(msg, "%", (item.quantity == 1? "s" : ""));
    _replace(msg, "&", (item.quantity == 1? "is" : "are"));
    const char *tag_start, *tag_end;
    switch (piety_gain)
    {
    case PIETY_NONE:
        tag_start = "<lightgrey>";
        tag_end = "</lightgrey>";
        break;
    default:
    case PIETY_SOME:
        tag_start = tag_end = "";
        break;
    case PIETY_LOTS:
        tag_start = "<white>";
        tag_end = "</white>";
        break;
    }

    msg.insert(0, item.name(your ? DESC_CAP_YOUR : DESC_CAP_THE));
    msg = tag_start + msg + tag_end;

    formatted_message_history(msg, MSGCH_GOD);
}

static void _altar_prayer()
{
    // Different message from when first joining a religion.
    mpr("You prostrate yourself in front of the altar and pray.");

    if (you.religion == GOD_XOM)
        return;

    god_acting gdact;

    // TSO blesses weapons with holy wrath, and long blades specially.
    if (you.religion == GOD_SHINING_ONE
        && !you.num_gifts[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        const int wpn = get_player_wielded_weapon();

        if (wpn != -1
            && (get_weapon_brand(you.inv[wpn]) != SPWPN_HOLY_WRATH
                || is_blessed_blade_convertible(you.inv[wpn])))
        {
            _bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH, YELLOW);
        }
    }

    // Lugonu blesses weapons with distortion.
    if (you.religion == GOD_LUGONU
        && !you.num_gifts[GOD_LUGONU]
        && !player_under_penance()
        && you.piety > 160)
    {
        const int wpn = get_player_wielded_weapon();

        if (wpn != -1 && get_weapon_brand(you.inv[wpn]) != SPWPN_DISTORTION)
            _bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, RED);
    }

    offer_items();
}

bool god_hates_attacking_friend(god_type god, const actor *fr)
{
    if (!fr || fr->kill_alignment() != KC_FRIENDLY)
        return (false);

    return god_hates_attacking_friend(god, fr->mons_species());
}

bool god_hates_attacking_friend(god_type god, int species)
{
    switch (god)
    {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
        case GOD_OKAWARU:
            return (true);
        case GOD_BEOGH: // added penance to avoid killings for loot
            return (species == MONS_ORC);

        default:
            return (false);
    }
}

bool god_likes_items(god_type god)
{
    switch (god)
    {
    case GOD_ZIN: case GOD_SHINING_ONE: case GOD_BEOGH: case GOD_NEMELEX_XOBEH:
        return (true);

    case GOD_NO_GOD: case NUM_GODS: case GOD_RANDOM: case GOD_NAMELESS:
        mprf(MSGCH_ERROR, "Bad god, no biscuit! %d", static_cast<int>(god) );
    default:
        return (false);
    }
}

static bool _god_likes_item(god_type god, const item_def& item)
{
    if (!god_likes_items(god))
        return (false);

    switch (god)
    {
    case GOD_ZIN:
        return (item.base_type == OBJ_GOLD);

    case GOD_SHINING_ONE:
        return (is_evil_item(item));

    case GOD_BEOGH:
        return (item.base_type == OBJ_CORPSES
                   && item.sub_type == CORPSE_BODY
                   && mons_species(item.plus) == MONS_ORC);

    case GOD_NEMELEX_XOBEH:
        return !is_deck(item);

    default:
        return (false);
    }
}

static int _leading_sacrifice_group()
{
    int weights[5];
    _get_pure_deck_weights(weights);
    int best_i = -1, maxweight = -1;
    for ( int i = 0; i < 5; ++i )
    {
        if ( best_i == -1 || weights[i] > maxweight )
        {
            maxweight = weights[i];
            best_i = i;
        }
    }
    return best_i;
}

static void _give_sac_group_feedback(int which)
{
    ASSERT( which >= 0 && which < 5 );
    const char* names[] = {
        "Escape", "Destruction", "Dungeons", "Summoning", "Wonder"
    };
    mprf(MSGCH_GOD, "A symbol of %s coalesces before you, then vanishes.",
         names[which]);
}

// God effects of sacrificing one item from a stack (e.g., a weapon, one
// out of 20 arrows, etc.) Does not modify the actual item in any way.
static piety_gain_t _sacrifice_one_item_noncount(const item_def& item)
{
    piety_gain_t relative_piety_gain = PIETY_NONE;
    // item_value() multiplies by quantity
    const int value = item_value(item) / item.quantity;

    switch (you.religion)
    {
    case GOD_SHINING_ONE:
        gain_piety(1);
        relative_piety_gain = PIETY_SOME;
        break;

    case GOD_BEOGH:
    {
        const int item_orig = item.orig_monnum - 1;

        if ((item_orig == MONS_SAINT_ROKA && !one_chance_in(5))
            || (item_orig == MONS_ORC_HIGH_PRIEST && x_chance_in_y(3, 5))
            || (item_orig == MONS_ORC_PRIEST && x_chance_in_y(2, 5))
            || one_chance_in(5))
        {
            gain_piety(1);
            relative_piety_gain = PIETY_SOME;
        }
        break;
    }

    case GOD_NEMELEX_XOBEH:
        if (you.attribute[ATTR_CARD_COUNTDOWN] && x_chance_in_y(value, 800))
        {
            you.attribute[ATTR_CARD_COUNTDOWN]--;
#if DEBUG_DIAGNOSTICS || DEBUG_CARDS || DEBUG_SACRIFICE
            mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                 you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
        }
        // Nemelex piety gain is fairly fast... at least
        // when you have low piety.
        if (item.base_type == OBJ_CORPSES && one_chance_in(2 + you.piety/50)
            || x_chance_in_y(value/2 + 1, 30 + you.piety/2))
        {
            if (is_artefact(item))
            {
                gain_piety(2);
                relative_piety_gain = PIETY_LOTS;
            }
            else
            {
                gain_piety(1);
                relative_piety_gain = PIETY_SOME;
            }
        }

        if (item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK
            || is_blood_potion(item))
        {
            // Count chunks and blood potions towards decks of Summoning.
            you.sacrifice_value[OBJ_CORPSES] += value;
        }
        else if (item.base_type == OBJ_CORPSES)
        {
#if DEBUG_GIFTS || DEBUG_CARDS || DEBUG_SACRIFICE
            mprf(MSGCH_DIAGNOSTICS, "Corpse mass is %d",
                 item_mass(item));
#endif
            you.sacrifice_value[item.base_type] += item_mass(item);
        }
        else
            you.sacrifice_value[item.base_type] += value;
        break;

    default:
        break;
    }

    return (relative_piety_gain);
}

static int _gold_to_donation(int gold)
{
    return static_cast<int>((gold * (int)log((double)gold)) / MAX_PIETY);
}

void offer_items()
{
    if (you.religion == GOD_NO_GOD)
        return;

    int i = igrd(you.pos());

    if (!god_likes_items(you.religion) && i != NON_ITEM)
    {
        simple_god_message(" doesn't care about such mundane gifts.",
                           you.religion);
        return;
    }

    god_acting gdact;

    // donate gold to gain piety distributed over time
    if (you.religion == GOD_ZIN)
    {
        if (you.gold == 0)
        {
            mpr("You don't have anything to sacrifice.");
            return;
        }

        if (!yesno("Do you wish to part with half of your money?", true, 'n'))
            return;

        const int donation_cost = (you.gold / 2) + 1;
        const int donation = _gold_to_donation(donation_cost);

#if DEBUG_DIAGNOSTICS || DEBUG_SACRIFICE || DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "A donation of $%d amounts to an "
             "increase of piety by %d.", donation_cost, donation);
#endif
        // Take a note of the donation.
        take_note(Note(NOTE_DONATE_MONEY, donation_cost));

        you.attribute[ATTR_DONATIONS] += donation_cost;

        you.gold -= donation_cost;

        if (donation < 1)
        {
            simple_god_message(" finds your generosity lacking.");
            return;
        }

        you.duration[DUR_PIETY_POOL] += donation;
        if (you.duration[DUR_PIETY_POOL] > 30000)
            you.duration[DUR_PIETY_POOL] = 30000;

        const int estimated_piety =
            std::min(MAX_PENANCE + MAX_PIETY,
                     you.piety + you.duration[DUR_PIETY_POOL]);

        if (player_under_penance())
        {
            if (estimated_piety >= you.penance[GOD_ZIN])
                mpr("You feel that you will soon be absolved of all your sins.");
            else
                mpr("You feel that your burden of sins will soon be lighter.");
            return;
        }

        std::string result = "You feel that " + god_name(GOD_ZIN)
                           + " will soon be ";
        result +=
            (estimated_piety > 130) ? "exalted by your worship" :
            (estimated_piety > 100) ? "extremely pleased with you" :
            (estimated_piety >  70) ? "greatly pleased with you" :
            (estimated_piety >  40) ? "most pleased with you" :
            (estimated_piety >  20) ? "pleased with you" :
            (estimated_piety >   5) ? "noncommittal"
                                    : "displeased";
        result += (donation >= 30 && you.piety <= 170) ? "!" : ".";

        mpr(result.c_str());

        return; // doesn't accept anything else for sacrifice
    }

    if (i == NON_ITEM) // nothing to sacrifice
        return;

    int num_sacced = 0;
    int num_disliked = 0;

    const int old_leading = _leading_sacrifice_group();

    while (i != NON_ITEM)
    {
        item_def &item(mitm[i]);
        const int next = item.link;  // in case we can't get it later.
        const bool disliked = !_god_likes_item(you.religion, item);

        if (item_is_stationary(item) || disliked)
        {
            i = next;
            if (disliked)
                num_disliked++;
            continue;
        }

        // Ignore {!D} inscribed items.
        if (!check_warning_inscriptions(item, OPER_DESTROY))
        {
            mpr("Won't sacrifice {!D} inscribed item.");
            i = next;
            continue;
        }

        if (_god_likes_item(you.religion, item)
            && (_is_risky_sacrifice(item)
                || item.inscription.find("=p") != std::string::npos))
        {
            const std::string msg =
                  "Really sacrifice " + item.name(DESC_NOCAP_A) + "?";

            if (!yesno(msg.c_str()))
            {
                i = next;
                continue;
            }
        }

        piety_gain_t relative_gain = PIETY_NONE;

#if DEBUG_DIAGNOSTICS || DEBUG_SACRIFICE
        mprf(MSGCH_DIAGNOSTICS, "Sacrifice item value: %d",
             item_value(item));
#endif

        for (int j = 0; j < item.quantity; ++j)
        {
            const piety_gain_t gain = _sacrifice_one_item_noncount(item);

            // Update piety gain if necessary.
            if (gain != PIETY_NONE)
            {
                if (relative_gain == PIETY_NONE)
                    relative_gain = gain;
                else            // some + some = lots
                    relative_gain = PIETY_LOTS;
            }
        }

        _print_sacrifice_message(you.religion, mitm[i], relative_gain);
        item_was_destroyed(mitm[i]);
        destroy_item(i);
        i = next;
        num_sacced++;
    }

    if (num_sacced > 0 && you.religion == GOD_NEMELEX_XOBEH)
    {
        const int new_leading = _leading_sacrifice_group();
        if (old_leading != new_leading || one_chance_in(50))
            _give_sac_group_feedback(new_leading);

#if DEBUG_GIFTS || DEBUG_CARDS || DEBUG_SACRIFICE
        _show_pure_deck_chances();
#endif
    }
    // Explanatory messages if nothing the god likes is sacrificed.
    else if (num_sacced == 0 && num_disliked > 0)
    {
        // Zin was handled above, and the other gods don't care about
        // sacrifices.
        if (you.religion == GOD_SHINING_ONE)
            simple_god_message(" only cares about evil items!");
        else if (you.religion == GOD_BEOGH)
            simple_god_message(" only cares about orc corpses!");
        else if (you.religion == GOD_NEMELEX_XOBEH)
            simple_god_message(" expects you to use your decks, not offer them!");
    }
}

bool player_can_join_god(god_type which_god)
{
    if (you.species == SP_DEMIGOD)
        return (false);

    if (player_is_unholy() && is_good_god(which_god))
        return (false);

    if (which_god == GOD_BEOGH && you.species != SP_HILL_ORC)
        return (false);

    return (true);
}

void god_pitch(god_type which_god)
{
    mprf("You %s the altar of %s.",
         you.species == SP_NAGA ? "coil in front of"
                                : "kneel at",
         god_name(which_god).c_str());
    more();

    // Note: using worship we could make some gods not allow followers to
    // return, or not allow worshippers from other religions.  -- bwr

    // Gods can be racist...
    if (!player_can_join_god(which_god))
    {
        you.turn_is_over = false;
        simple_god_message(" does not accept worship from those such as you!",
                           which_god);
        return;
    }

    if (which_god == GOD_LUGONU && you.penance[GOD_LUGONU])
    {
        simple_god_message(" is most displeased with you!", which_god);
        god_acting gdact(GOD_LUGONU, true);
        _lugonu_retribution();
        return;
    }

    describe_god( which_god, false );

    snprintf( info, INFO_SIZE, "Do you wish to %sjoin this religion?",
              (you.worshipped[which_god]) ? "re" : "" );

    if (!yesno( info, false, 'n' ) || !yesno("Are you sure?", false, 'n'))
    {
        you.turn_is_over = false; // Okay, opt out.
        redraw_screen();
        return;
    }

    // OK, so join the new religion.

    redraw_screen();

    const int old_piety = you.piety;
    // Are you switching between good gods?
    const bool good_god_switch = is_good_god(you.religion)
                                 && is_good_god(which_god);

    // Leave your prior religion first.
    if (you.religion != GOD_NO_GOD)
        excommunication(which_god);

    // Welcome to the fold!
    you.religion = static_cast<god_type>(which_god);

    if (you.religion == GOD_XOM)
    {
        // Xom uses piety and gift_timeout differently.
        you.piety = (MAX_PIETY / 2);
        you.gift_timeout = random2(40) + random2(40);
    }
    else
    {
        you.piety = 15; // to prevent near instant excommunication
        you.piety_hysteresis = 0;
        you.gift_timeout = 0;
    }

    set_god_ability_slots();    // remove old god's slots, reserve new god's
#ifdef DGL_WHEREIS
    whereis_record();
#endif
    simple_god_message(
        make_stringf(" welcomes you%s!",
                     you.worshipped[which_god] ? " back" : "").c_str());
    more();

    // When you start worshipping a good god, you make all non-hostile
    // evil and unholy beings hostile; when you start worshipping Zin,
    // you make all non-hostile chaotic beings hostile; and when you
    // start worshipping Trog, you make all non-hostile magic users
    // hostile.
    if (is_good_god(you.religion) && _evil_beings_attitude_change())
        mpr("Your evil allies forsake you.", MSGCH_MONSTER_ENCHANT);

    if (you.religion == GOD_ZIN && _chaotic_beings_attitude_change())
        mpr("Your chaotic allies forsake you.", MSGCH_MONSTER_ENCHANT);
    else if (you.religion == GOD_TROG && _magic_users_attitude_change())
        mpr("Your magic-using allies forsake you.", MSGCH_MONSTER_ENCHANT);

    if (you.religion == GOD_ELYVILON)
    {
        mpr("You can now call upon Elyvilon to destroy weapons lying "
            "on the ground.", MSGCH_GOD);
    }
    else if (you.religion == GOD_TROG)
    {
        mpr("You can now call upon Trog to burn spellbooks in your "
            "surroundings.", MSGCH_GOD);
    }

    if (you.worshipped[you.religion] < 100)
        you.worshipped[you.religion]++;

    take_note(Note(NOTE_GET_GOD, you.religion));

    // Currently, penance is just zeroed.  This could be much more
    // interesting.
    you.penance[you.religion] = 0;

    if (is_good_god(you.religion))
    {
        // Give a piety bonus when switching between good gods.
        if (good_god_switch && old_piety > 15)
            gain_piety(std::min(30, old_piety - 15));
    }
    else if (is_evil_god(you.religion))
    {
        // Note: Using worshipped[] we could make this sort of grudge
        // permanent instead of based off of penance. -- bwr
        if (you.penance[GOD_SHINING_ONE])
        {
            _inc_penance(GOD_SHINING_ONE, 30);
            god_speaks(GOD_SHINING_ONE,
                       "\"You will pay for your evil ways, mortal!\"");
        }
    }

    // Note that you.worshipped[] has already been incremented.
    if (you.religion == GOD_LUGONU && you.worshipped[GOD_LUGONU] == 1)
        gain_piety(20);         // allow instant access to first power

    redraw_skill(you.your_name, player_title());

    learned_something_new(TUT_CONVERT);
}

bool god_hates_your_god(god_type god, god_type your_god)
{
    ASSERT(god != your_god);

    // Non-good gods always hate your current god.
    if (!is_good_god(god))
        return (true);

    // Zin hates chaotic gods.
    if (god == GOD_ZIN && is_chaotic_god(your_god))
        return (true);

    return (is_evil_god(your_god));
}

std::string god_hates_your_god_reaction(god_type god, god_type your_god)
{
    if (god_hates_your_god(god, your_god))
    {
        // Non-good gods always hate your current god.
        if (!is_good_god(god))
            return ("");

        // Zin hates chaotic gods.
        if (god == GOD_ZIN && is_chaotic_god(your_god))
            return (" for chaos");

        if (is_evil_god(your_god))
            return (" for evil");
    }

    return ("");
}

bool god_hates_cannibalism(god_type god)
{
    return (is_good_god(god) || god == GOD_BEOGH);
}

bool god_hates_killing(god_type god, const monsters* mon)
{
    bool retval = false;
    const mon_holy_type holiness = mon->holiness();

    switch (holiness)
    {
        case MH_HOLY:
            retval = (is_good_god(god));
            break;
        case MH_NATURAL:
            retval = (god == GOD_ELYVILON);
            break;
        default:
            break;
    }

    return retval;
}

bool god_likes_butchery(god_type god)
{
    return (god == GOD_OKAWARU
            || god == GOD_MAKHLEB
            || god == GOD_TROG
            || god == GOD_LUGONU);
}

bool god_hates_butchery(god_type god)
{
    return (god == GOD_ELYVILON);
}

harm_protection_type god_protects_from_harm(god_type god, bool actual)
{
    const int min_piety = piety_breakpoint(0);
    bool praying = (you.duration[DUR_PRAYER]
                    && random2(you.piety) >= min_piety);
    bool reliable = (you.piety > 130);
    bool anytime = (one_chance_in(10) || x_chance_in_y(you.piety, 1000));
    bool penance = (you.penance[god] > 0);

    // If actual is true, return HPT_NONE if the given god can protect
    // the player from harm, but doesn't actually do so.
    switch (god)
    {
    case GOD_BEOGH:
        if (!penance && (!actual || anytime))
            return (HPT_ANYTIME);
        break;
    case GOD_ZIN:
    case GOD_SHINING_ONE:
        if (!actual || anytime)
            return (HPT_ANYTIME);
        break;
    case GOD_ELYVILON:
        if (!actual || praying || anytime)
        {
            if (you.piety >= min_piety)
            {
                return (reliable) ? HPT_RELIABLE_PRAYING_PLUS_ANYTIME
                                  : HPT_PRAYING_PLUS_ANYTIME;
            }
            else
                return (HPT_ANYTIME);
        }
        break;
    default:
        break;
    }

    return (HPT_NONE);
}

static void _god_smites_you(god_type god, const char *message,
                            kill_method_type death_type)
{
    ASSERT(god != GOD_NO_GOD);

    // Your god won't protect you from his own smiting, and Xom is too
    // capricious to protect you from any god's smiting.
    if (you.religion != god && you.religion != GOD_XOM
        && !player_under_penance() && x_chance_in_y(you.piety, MAX_PIETY * 2))
    {
        god_speaks(you.religion,
                   make_stringf("\"Mortal, I have averted the wrath of %s... "
                                "this time.\"", god_name(god).c_str()).c_str());
    }
    else
    {
        if (death_type == NUM_KILLBY)
        {
            switch (god)
            {
                case GOD_BEOGH:     death_type = KILLED_BY_BEOGH_SMITING; break;
                case GOD_SHINING_ONE: death_type = KILLED_BY_TSO_SMITING; break;
                default:            death_type = KILLED_BY_DIVINE_WRATH;  break;
            }
        }

        std::string aux;

        if (death_type != KILLED_BY_BEOGH_SMITING
            && death_type != KILLED_BY_TSO_SMITING)
        {
            aux = "smote by " + god_name(god);
        }

        // If there's a message, display it before smiting.
        if (message)
            god_speaks(god, message);

        int divine_hurt = 10 + random2(10);

        for (int i = 0; i < 5; ++i)
            divine_hurt += random2(you.experience_level);

        simple_god_message(" smites you!", god);
        ouch(divine_hurt, NON_MONSTER, death_type, aux.c_str());
        dec_penance(god, 1);
    }
}

void offer_corpse(int corpse)
{
    // We always give the "good" (piety-gain) message when doing
    // dedicated butchery. Uh, call it a feature.
    _print_sacrifice_message(you.religion, mitm[corpse], PIETY_SOME);

    did_god_conduct(DID_DEDICATED_BUTCHERY, 10);

    // ritual sacrifice can also bloodify the ground
    const int mons_class = mitm[corpse].plus;
    const int max_chunks = mons_weight(mons_class) / 150;
    bleed_onto_floor(you.pos(), mons_class, max_chunks, true);
    destroy_item(corpse);
}

// Returns true if the player can use the good gods' passive piety gain.
static bool _need_free_piety()
{
    return (you.piety < 150 || you.gift_timeout || you.penance[you.religion]);
}

//jmf: moved stuff from effects::handle_time()
void handle_god_time()
{
    if (one_chance_in(100))
    {
        // Choose a god randomly from those to whom we owe penance.
        //
        // Proof: (By induction)
        //
        // 1) n = 1, probability of choosing god is one_chance_in(1)
        // 2) Asuume true for n = k (ie. prob = 1 / n for all n)
        // 3) For n = k + 1,
        //
        //      P:new-found-god = 1 / n (see algorithm)
        //      P:other-gods = (1 - P:new-found-god) * P:god-at-n=k
        //                             1        1
        //                   = (1 - -------) * ---
        //                           k + 1      k
        //
        //                          k         1
        //                   = ----------- * ---
        //                        k + 1       k
        //
        //                       1       1
        //                   = -----  = ---
        //                     k + 1     n
        //
        // Therefore, by induction the probability is uniform.  As for
        // why we do it this way... it requires only one pass and doesn't
        // require an array.

        god_type which_god = GOD_NO_GOD;
        unsigned int count = 0;

        for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
        {
            // Nemelex penance is special: it's only "active"
            // when penance > 100, else it's passive.
            if (you.penance[i] && (i != GOD_NEMELEX_XOBEH
                                   || you.penance[i] > 100))
            {
                count++;
                if (one_chance_in(count))
                    which_god = static_cast<god_type>(i);
            }
        }

        if (which_god != GOD_NO_GOD)
            divine_retribution(which_god);
    }

    // Update the god's opinion of the player.
    if (you.religion != GOD_NO_GOD)
    {
        switch (you.religion)
        {
        case GOD_XOM:
            xom_tick();
            break;

        // These gods like long-standing worshippers.
        case GOD_ELYVILON:
            if (_need_free_piety() && one_chance_in(20))
                gain_piety(1);
            break;

        case GOD_SHINING_ONE:
            if (_need_free_piety() && one_chance_in(15))
                gain_piety(1);
            break;

        case GOD_ZIN:
            if (_need_free_piety() && one_chance_in(12))
                gain_piety(1);
            break;

        case GOD_YREDELEMNUL:
        case GOD_KIKUBAAQUDGHA:
        case GOD_VEHUMET:
            if (one_chance_in(17))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        // These gods accept corpses, so they time-out faster.
        case GOD_OKAWARU:
        case GOD_TROG:
            if (one_chance_in(14))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        case GOD_MAKHLEB:
        case GOD_BEOGH:
        case GOD_LUGONU:
            if (one_chance_in(16))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        case GOD_SIF_MUNA:
            // [dshaligram] Sif Muna is now very patient - has to be
            // to make up for the new spell training requirements, else
            // it's practically impossible to get Master of Arcane status.
            if (one_chance_in(100))
                lose_piety(1);
            if (you.piety < 1)
                excommunication();
            break;

        case GOD_NEMELEX_XOBEH:
            // Nemelex is relatively patient.
            if (one_chance_in(35))
                lose_piety(1);
            if (you.attribute[ATTR_CARD_COUNTDOWN] > 0 && coinflip())
                you.attribute[ATTR_CARD_COUNTDOWN]--;
            if (you.piety < 1)
                excommunication();
            break;

        default:
            DEBUGSTR("Bad god, no bishop!");
        }
    }
}

// yet another wrapper for mpr() {dlb}:
void simple_god_message(const char *event, god_type which_deity)
{
    std::string msg = god_name(which_deity) + event;
    msg = apostrophise_fixup(msg);
    god_speaks(which_deity, msg.c_str());
}

int god_colour(god_type god) // mv - added
{
    switch (god)
    {
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
    case GOD_OKAWARU:
        return(CYAN);

    case GOD_YREDELEMNUL:
    case GOD_KIKUBAAQUDGHA:
    case GOD_MAKHLEB:
    case GOD_VEHUMET:
    case GOD_TROG:
    case GOD_BEOGH:
    case GOD_LUGONU:
        return(LIGHTRED);

    case GOD_XOM:
        return(YELLOW);

    case GOD_NEMELEX_XOBEH:
        return(LIGHTMAGENTA);

    case GOD_SIF_MUNA:
        return(LIGHTBLUE);

    case GOD_NO_GOD:
    case NUM_GODS:
    case GOD_RANDOM:
    case GOD_NAMELESS:
    default:
        break;
    }

    return(YELLOW);
}

int piety_rank(int piety)
{
    const int breakpoints[] = { 161, 120, 100, 75, 50, 30, 6 };
    const int numbreakpoints = ARRAYSZ(breakpoints);
    if (piety < 0)
        piety = you.piety;

    for (int i = 0; i < numbreakpoints; ++i)
    {
        if (piety >= breakpoints[i])
            return numbreakpoints - i;
    }

    return 0;
}

int piety_breakpoint(int i)
{
    int breakpoints[MAX_GOD_ABILITIES] = { 30, 50, 75, 100, 120 };
    if (i >= MAX_GOD_ABILITIES || i < 0)
        return 255;
    else
        return breakpoints[i];
}

// Returns true if the Shining One doesn't mind your using unchivalric
// attacks on this creature.
bool tso_unchivalric_attack_safe_monster(const monsters *mon)
{
    const mon_holy_type holiness = mon->holiness();
    return (mons_intel(mon) < I_NORMAL
            || mons_is_evil(mon)
            || (holiness != MH_NATURAL && holiness != MH_HOLY));
}

int get_tension(god_type god)
{
    ASSERT(god != GOD_NO_GOD);

    int total = 0;

    for (int midx = 0; midx < MAX_MONSTERS; ++midx)
    {
        const monsters* mons = &menv[midx];

        if (!mons->alive())
            continue;

        if (see_grid(mons->pos()))
            ; // Monster is nearby.
        else
        {
            // Is the monster trying to get somewhere nearby?
            coord_def    target;
            unsigned int travel_size = mons->travel_path.size();

            if (travel_size > 0)
                target = mons->travel_path[travel_size - 1];
            else
                target = mons->target;

            // Monster is neither nearby nor trying to get near us.
            if (!in_bounds(target) || !see_grid(target))
                continue;
        }

        const mon_attitude_type att = mons_attitude(mons);
        if (att == ATT_GOOD_NEUTRAL || att == ATT_NEUTRAL)
            continue;

        if (mons_cannot_act(mons) || mons->asleep() || mons_is_fleeing(mons))
            continue;

        int exper = exper_value(mons);
        if (exper <= 0)
            continue;

        // Almost dead monsters don't count as much.
        exper *= mons->hit_points;
        exper /= mons->max_hit_points;

        const bool gift = mons_is_god_gift(mons, god);

        if (att == ATT_HOSTILE)
        {
            // God is punishing you with a hostile gift, so it doesn't
            // count towards tension.
            if (gift)
                continue;
        }
        else if (att == ATT_FRIENDLY)
        {
            // Friendly monsters being around to help you reduce
            // tension.
            exper = -exper;

            // If it's a god gift, it reduces tension even more, since
            // the god is already helping you out.
            if (gift)
                exper *= 2;
        }

        if (att != ATT_FRIENDLY)
        {
            if (!mons_player_visible(mons))
                exper /= 2;
            if (!player_monster_visible(mons))
                exper *= 2;
        }

        if (mons->confused() || mons->caught())
            exper /= 2;

        if (mons->has_ench(ENCH_SLOW))
        {
            exper *= 2;
            exper /= 3;
        }

        if (mons->has_ench(ENCH_HASTE))
        {
            exper *= 3;
            exper /= 2;
        }

        if (mons->has_ench(ENCH_BERSERK))
            exper *= 2;

        total += exper;
    }
    const int scale = 1;

    int tension = total;

    // Tension goes up inversely proportional to the percentage of max
    // hp you have.
    tension *= (scale + 1) * you.hp_max;
    tension /= you.hp_max + scale * you.hp;

    // Divides by 1 at level 1, 200 at level 27.
    const int exp_lev  = you.get_experience_level();
    const int exp_need = exp_needed(exp_lev + 1);
    const int factor   = (int)ceil(sqrt(exp_need / 30.0));
    const int div      = 1 + factor;

    tension /= div;

    if (you.level_type == LEVEL_ABYSS)
        tension = std::max(2, tension);

    if (you.cannot_act())
    {
        tension *= 10;
        tension  = std::max(1, tension);

        return (tension);
    }

    if (you.confused())
        tension *= 2;

    if (you.caught())
        tension *= 2;

    if (you.duration[DUR_SLOW])
    {
        tension *= 3;
        tension /= 2;
    }

    if (you.duration[DUR_HASTE])
    {
        tension *= 2;
        tension /= 3;
    }

    return std::max(0, tension);
}

/////////////////////////////////////////////////////////////////////////////
// Stuff for placing god gift monsters after the player's turn has ended.
/////////////////////////////////////////////////////////////////////////////

static std::vector<mgen_data>       _delayed_data;
static std::deque<delayed_callback> _delayed_callbacks;
static std::deque<unsigned int>     _delayed_done_trigger_pos;
static std::deque<delayed_callback> _delayed_done_callbacks;
static std::deque<std::string>      _delayed_success;
static std::deque<std::string>      _delayed_failure;

static void _delayed_monster(const mgen_data &mg, delayed_callback callback)
{
    _delayed_data.push_back(mg);
    _delayed_callbacks.push_back(callback);
}

static void _delayed_monster_done(std::string success, std::string failure,
                                  delayed_callback callback)
{
    const unsigned int size = _delayed_data.size();
    ASSERT(size > 0);

    _delayed_done_trigger_pos.push_back(size - 1);
    _delayed_success.push_back(success);
    _delayed_failure.push_back(failure);
    _delayed_done_callbacks.push_back(callback);
}

static void _place_delayed_monsters()
{
    int      placed   = 0;
    god_type prev_god = GOD_NO_GOD;
    for (unsigned int i = 0; i < _delayed_data.size(); i++)
    {
        mgen_data &mg          = _delayed_data[i];
        delayed_callback cback = _delayed_callbacks[i];

        if (prev_god != mg.god)
        {
            placed   = 0;
            prev_god = mg.god;
        }

        int midx = create_monster(mg);

        if (cback)
            (*cback)(mg, midx, placed);

        if (midx != -1)
            placed++;

        if (_delayed_done_trigger_pos.size() > 0
            && _delayed_done_trigger_pos[0] == i)
        {
            cback = _delayed_done_callbacks[0];

            std::string msg;
            if (placed > 0)
                msg = _delayed_success[0];
            else
                msg = _delayed_failure[0];

            if (placed == 1)
            {
                msg = replace_all(msg, "@a@", "a");
                msg = replace_all(msg, "@an@", "an");
            }
            else
            {
                msg = replace_all(msg, " @a@", "");
                msg = replace_all(msg, " @an@", "");
            }

            if (placed > 1)
                msg = replace_all(msg, "@s@", "s");
            else
                msg = replace_all(msg, "@s@", "");

            prev_god = GOD_NO_GOD;
            _delayed_done_trigger_pos.pop_front();
            _delayed_success.pop_front();
            _delayed_failure.pop_front();
            _delayed_done_callbacks.pop_front();

            if (msg == "")
            {
                if (cback)
                    (*cback)(mg, midx, placed);
                continue;
            }

            // Fake it coming from simple_god_message().
            if (msg[0] == ' ' || msg[0] == '\'')
                msg = god_name(mg.god) + msg;

            msg = apostrophise_fixup(msg);
            trim_string(msg);

            god_speaks(mg.god, msg.c_str());

            if (cback)
                (*cback)(mg, midx, placed);
        }
    }

    _delayed_data.clear();
    _delayed_callbacks.clear();
    _delayed_done_trigger_pos.clear();
    _delayed_success.clear();
    _delayed_failure.clear();
}
