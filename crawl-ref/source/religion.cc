/*
 *  File:       religion.cc
 *  Summary:    Misc religion related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "religion.h"

#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cmath>

#ifdef TARGET_OS_DOS
#include <dos.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "areas.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "chardump.h"
#include "coordit.h"
#include "database.h"
#include "debug.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "dgnevent.h"
#include "dlua.h"
#include "effects.h"
#include "env.h"
#include "enum.h"
#include "fprop.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "godabil.h"
#include "goditem.h"
#include "godpassive.h"
#include "godprayer.h"
#include "godwrath.h"
#include "hiscores.h"
#include "invent.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "kills.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-stuff.h"
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "shopping.h"
#include "skills2.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-mis.h"
#include "sprint.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "hints.h"
#include "view.h"
#include "xom.h"

#ifdef DEBUG_RELIGION
#    define DEBUG_DIAGNOSTICS
#    define DEBUG_GIFTS
#    define DEBUG_SACRIFICE
#    define DEBUG_PIETY
#endif

#define PIETY_HYSTERESIS_LIMIT 1

// Item offering messages for the gods:
// & is replaced by "is" or "are" as appropriate for the item.
// % is replaced by "s" or "" as appropriate.
// Text between [] only appears if the item already glows.
// <> and </> are replaced with colors.
// First message is if there's no piety gain; second is if piety gain is
// one; third is if piety gain is more than one.
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
        " glow% a dingy golden colour and disappear%.",
        " glow% a golden colour and disappear%.",
        " glow% a brilliant golden colour and disappear%.",
    },
    // Kikubaaqudgha
    {
        " convulse% and rot% away.",
        " convulse% madly and rot% away.",
        " convulse% furiously and rot% away.",
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
        " & gone without a[dditional] glow.",
        " glow% slightly [brighter ]for a moment, and & gone.",
        " glow% [brighter ]for a moment, and & gone.",
    },
    // Trog
    {
        " & slowly consumed by flames.",
        " & consumed in a column of flame.",
        " & consumed in a roaring column of flame.",
    },
    // Nemelex
    {
        " disappear% without a[dditional] glow.",
        " glow% slightly [brighter ]and disappear%.",
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
        " disappear% into the void.",
        " & consumed by the void.",
        " & voraciously consumed by the void.",
    },
    // Beogh
    {
        " slowly crumble% into the ground.",
        " crumble% into the ground.",
        " disintegrate% into the ground.",
    },
    // Jiyva
    {
        " slowly dissolve% into ooze.",
        " dissolve% into ooze.",
        " disappear% with a satisfied slurp.",
    }
};

const char* god_gain_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "recite Zin's Axioms of Law",
      "call upon Zin for vitalisation",
      "call upon Zin to imprison the lawless",
      "",
      "call upon Zin to create a sanctuary" },
    // TSO
    { "You and your allies can gain power from killing the unholy and evil.",
      "call upon the Shining One for a divine shield",
      "",
      "channel blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "receive cadavers from Kikubaaqudgha",
      "Kikubaaqudgha is protecting you from some side-effects of death magic.",
      "",
      "Kikubaaqudgha is protecting you from unholy torment.",
      "invoke torment by praying over a corpse" },
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
      "Vehumet is extending the range of your conjurations.",
      "Vehumet is reducing the cost of your destructive magics.",
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
      "Sif Muna is protecting you from miscast magic.",
      "" },
    // Trog
    { "go berserk at will",
      "call upon Trog for regeneration and magic resistance",
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
    { "provide lesser healing for yourself and others",
      "purify yourself",
      "provide greater healing for yourself and others",
      "restore your abilities",
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
      "walk on water" },
    // Jiyva
    { "request a jelly",
      "",
      "",
      "turn your foes to slime",
      "call upon Jiyva to remove your harmful mutations"
    },
    // Fedhas
    { "induce evolution",
      "call sunshine",
      "cause a ring of plants to grow",
      "spawn explosive spores",
      "control the weather"
    },
    // Cheibriados
    { "Cheibriados slows and strengthens your metabolism, "
      "and supports the use of ponderous armour.",
      "bend time to slow others",
      "",
      "inflict damage to those overly hasty",
      "step out of the time flow"
    }
};

const char* god_lose_power_messages[NUM_GODS][MAX_GOD_ABILITIES] =
{
    // no god
    { "", "", "", "", "" },
    // Zin
    { "recite Zin's Axioms of Law",
      "call upon Zin for vitalisation",
      "call upon Zin to imprison the lawless",
      "",
      "call upon Zin to create a sanctuary" },
    // TSO
    { "You and your allies can no longer gain power from killing the unholy and evil.",
      "call upon the Shining One for a divine shield",
      "",
      "channel blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "receive cadavers from Kikubaaqudgha",
      "Kikubaaqudgha is no longer shielding you from miscast death magic.",
      "",
      "Kikubaaqudgha will no longer protect you from unholy torment.",
      "invoke torment by praying over a corpse" },
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
      "Vehumet will no longer extend the range of your conjurations.",
      "Vehumet will no longer reduce the cost of your destructive magics.",
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
      "Sif Muna will no longer protect you from miscast magic.",
      "" },
    // Trog
    { "go berserk at will",
      "call upon Trog for regeneration and magic resistance",
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
    { "provide lesser healing",
      "purify yourself",
      "provide greater healing",
      "restore your abilities",
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
      "walk on water" },
    // Jiyva
    { "request a jelly",
      "",
      "",
      "turn your foes to slime",
      "call upon Jiyva to remove your harmful mutations"
    },
    // Fedhas
    { "induce evolution",
      "call sunshine",
      "cause a ring of plants to grow",
      "spawn explosive spores",
      "control the weather"
    },
    // Cheibriados
    { "Cheibriados will no longer slow or strengthen your metabolism, "
      "or support the use of ponderous armour.",
      "bend time to slow others",
      "",
      "inflict damage to those overly hasty",
      "step out of the time flow"
    }
};

typedef void (*delayed_callback)(const mgen_data &mg, int &midx, int placed);

static void _delayed_monster(const mgen_data &mg,
                             delayed_callback callback = NULL);
static void _delayed_monster_done(std::string success, std::string failure,
                                  delayed_callback callback = NULL);
static void _place_delayed_monsters();

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
            || god == GOD_LUGONU
            || god == GOD_JIYVA);
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

    case GOD_FEDHAS:
        snprintf(info, INFO_SIZE, "you promote the decay of nearby "
                                  "corpses%s",
                 verbose ? " via the decomposition <w>a</w>bility" : "");
        likes.push_back(info);
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
        snprintf(info, INFO_SIZE, "you destroy weapons (especially unholy and "
                                  "evil ones)%s",
                 verbose ? " via the <w>a</w> command (inscribe items with "
                           "<w>!D</w> to prevent their accidental destruction)"
                         : "");

        likes.push_back(info);
        break;

    case GOD_JIYVA:
        snprintf(info, INFO_SIZE, "you sacrifice items%s",
                 verbose ? " by allowing slimes to consume them" : "");
        likes.push_back(info);
        break;

    case GOD_CHEIBRIADOS:
        snprintf(info, INFO_SIZE, "you kill fast things%s",
                 verbose ? ", relative to your current speed"
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
        snprintf(info, INFO_SIZE, "you sacrifice unholy and evil items%s",
                 verbose ? " (by dropping them on an altar and praying)" : "");

        likes.push_back(info);
        break;

    case GOD_BEOGH:
        snprintf(info, INFO_SIZE, "you bless dead orcs%s",
                 verbose ? " (by standing over their remains and <w>p</w>raying)" : "");

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

    if (god_likes_fresh_corpses(which_god) && which_god != GOD_KIKUBAAQUDGHA)
    {
        snprintf(info, INFO_SIZE, "you sacrifice fresh corpses%s",
                 verbose ? " (by standing over them and <w>p</w>raying)" : "");

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
    case GOD_KIKUBAAQUDGHA:
        likes.push_back("you or your undead slaves kill living beings");
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
    case GOD_ZIN:
        likes.push_back("you or your allies kill unclean or chaotic beings");
        break;

    case GOD_SHINING_ONE:
        likes.push_back("you or your allies kill living unholy or evil beings");
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

    case GOD_KIKUBAAQUDGHA:
        likes.push_back("you or your undead slaves kill demons");
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
        likes.push_back("you or your undead slaves kill holy beings");
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

    if (god_hates_cannibalism(which_god))
        dislikes.push_back("you perform cannibalism");

    if (is_good_god(which_god))
    {
        dislikes.push_back("you drink blood");
        dislikes.push_back("you use necromancy");
        dislikes.push_back("you use unholy magic or items");
        dislikes.push_back("you attack non-hostile holy beings");
        dislikes.push_back("you or your allies kill non-hostile holy beings");
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

    case GOD_JIYVA:
        dislikes.push_back("you attack your fellow slimes");
        break;

    case GOD_FEDHAS:
        dislikes.push_back("you or your allies destroy plants");
        dislikes.push_back("allied flora die");
        dislikes.push_back("you use necromancy on corpses, chunks or skeletons");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ELYVILON: case GOD_OKAWARU:
        dislikes.push_back("you allow allies to die");
        break;

    case GOD_ZIN:
        dislikes.push_back("you allow sentient allies to die");
        break;

    default:
        break;
    }

    switch (which_god)
    {
    case GOD_ZIN:
        dislikes.push_back("you deliberately mutate yourself");
        dislikes.push_back("you polymorph monsters");
        dislikes.push_back("you use unclean or chaotic magic or items");
        dislikes.push_back("you eat the flesh of sentient beings");
        dislikes.push_back("you or your allies attack monsters in a "
                           "sanctuary");
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
        dislikes.push_back("you attempt to cast spells");
        break;

    case GOD_BEOGH:
        dislikes.push_back("you desecrate orcish remains");
        dislikes.push_back("you destroy orcish idols");
        break;

    case GOD_JIYVA:
        dislikes.push_back("you kill slimes");
        break;

    case GOD_CHEIBRIADOS:
        dislikes.push_back("you hasten yourself");
        dislikes.push_back("use unnaturally quick items");
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
    if (val <= 0)
        return;

    if (you.penance[god] > 0)
    {
#ifdef DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "Decreasing penance by %d", val);
#endif
        if (you.penance[god] <= val)
        {
            you.penance[god] = 0;

#ifdef DGL_MILESTONES
            mark_milestone("god.mollify",
                           "mollified " + god_name(god) + ".");
#endif

            const bool dead_jiyva = (god == GOD_JIYVA && jiyva_is_dead());

            simple_god_message(
                make_stringf(" seems mollified%s.",
                             dead_jiyva ? ", and vanishes" : "").c_str(),
                god);

            if (dead_jiyva)
                remove_all_jiyva_altars();

            take_note(Note(NOTE_MOLLIFY_GOD, god));

            if (you.religion == god)
            {
                // In case the best skill is Invocations, redraw the god
                // title.
                redraw_skill(you.your_name, player_title());
            }

            // Orcish bonuses are now once more effective.
            if (god == GOD_BEOGH && you.religion == god)
                 you.redraw_armour_class = true;
            // TSO's halo is once more available.
            else if (god == GOD_SHINING_ONE && you.religion == god
                && you.piety >= piety_breakpoint(0))
            {
                mpr("Your divine halo returns!");
            }

            // When you've worked through all your penance, you get
            // another chance to make hostile holy beings good neutral.
            if (is_good_god(you.religion))
                holy_beings_attitude_change();
        }
        else if (god == GOD_NEMELEX_XOBEH && you.penance[god] > 100)
        { // Nemelex's penance works actively only until 100
            if ((you.penance[god] -= val) > 100)
                return;
#ifdef DGL_MILESTONES
            mark_milestone("god.mollify",
                           "partially mollified " + god_name(god) + ".");
#endif
            simple_god_message(" seems mollified... mostly.", god);
            take_note(Note(NOTE_MOLLIFY_GOD, god));
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

bool zin_remove_all_mutations()
{
    if (!how_mutated())
    {
        mpr("You have no mutations to be cured!");
        return (false);
    }

    you.num_gifts[GOD_ZIN]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));

    simple_god_message(" draws all chaos from your body!");
    delete_all_mutations();

    return (true);
}

static bool _need_water_walking()
{
    return (!you.airborne() && you.species != SP_MERFOLK
            && grd(you.pos()) == DNGN_DEEP_WATER);
}

bool jiyva_is_dead()
{
    return (you.royal_jelly_dead
            && you.religion != GOD_JIYVA && !you.penance[GOD_JIYVA]);
}

static bool _remove_jiyva_altars()
{
    bool success = false;
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grd(*ri) == DNGN_ALTAR_JIYVA)
        {
            grd(*ri) = DNGN_FLOOR;
            success = true;
        }
    }

    return (success);
}

bool remove_all_jiyva_altars()
{
    return (apply_to_all_dungeons(_remove_jiyva_altars));
}

static void _inc_penance(god_type god, int val)
{
    if (val <= 0)
        return;

    if (you.penance[god] == 0)
    {
        god_acting gdact(god, true);

        take_note(Note(NOTE_PENANCE, god));

        you.penance[god] += val;
        you.penance[god] = std::min<unsigned char>(MAX_PENANCE,
                                                   you.penance[god]);

        // Orcish bonuses don't apply under penance.
        if (god == GOD_BEOGH)
        {
            you.redraw_armour_class = true;

            if (_need_water_walking() && !beogh_water_walk())
                fall_into_a_pool(you.pos(), true, grd(you.pos()));
        }
        // Neither does Trog's regeneration or magic resistance.
        else if (god == GOD_TROG)
        {
            if (you.attribute[ATTR_DIVINE_REGENERATION])
                remove_regen(true);

            make_god_gifts_disappear(); // only on level
        }
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

            make_god_gifts_disappear(); // only on level
        }
        // Neither does Ely's divine vigour.
        else if (god == GOD_ELYVILON)
        {
            if (you.duration[DUR_DIVINE_VIGOUR])
                remove_divine_vigour();
        }
        else if (god == GOD_JIYVA)
        {
            if (you.duration[DUR_SLIMIFY])
                you.duration[DUR_SLIMIFY] = 0;
        }

        if (you.religion == god)
        {
            // In case the best skill is Invocations, redraw the god
            // title.
            redraw_skill(you.your_name, player_title());
        }
    }
    else
    {
        you.penance[god] += val;
        you.penance[god] = std::min<unsigned char>(MAX_PENANCE,
                                                   you.penance[god]);
    }
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

int yred_random_servants(int threshold, bool force_hostile)
{
    monster_type mon_type;
    int how_many = 1;
    const int temp_rand = random2(std::min(100, threshold));

    // undead
    mon_type = ((temp_rand < 10) ? MONS_WRAITH :           // 10%
                (temp_rand < 20) ? MONS_WIGHT :            // 10%
                (temp_rand < 30) ? MONS_FLYING_SKULL :     // 10%
                (temp_rand < 39) ? MONS_SPECTRAL_WARRIOR : //  9%
                (temp_rand < 48) ? MONS_ROTTING_HULK :     //  9%
                (temp_rand < 57) ? MONS_SKELETAL_WARRIOR : //  9%
                (temp_rand < 65) ? MONS_FREEZING_WRAITH :  //  8%
                (temp_rand < 73) ? MONS_FLAMING_CORPSE :   //  8%
                (temp_rand < 80) ? MONS_GHOUL :            //  7%
                (temp_rand < 86) ? MONS_MUMMY :            //  6%
                (temp_rand < 91) ? MONS_HUNGRY_GHOST :     //  5%
                (temp_rand < 95) ? MONS_FLAYED_GHOST :     //  4%
                (temp_rand < 98) ? MONS_BONE_DRAGON        //  3%
                                 : MONS_DEATH_COB);        //  2%

    if (mon_type == MONS_FLYING_SKULL)
        how_many = 2 + random2(4);

    mgen_data mg(mon_type, !force_hostile ? BEH_FRIENDLY : BEH_HOSTILE,
                 !force_hostile ? &you : 0, 0, 0, you.pos(), MHITYOU, 0,
                 GOD_YREDELEMNUL);

    if (force_hostile)
        mg.non_actor_summoner = "the anger of Yredelemnul";

    int created = 0;
    if (force_hostile)
    {
        for (; how_many > 0; --how_many)
        {
            if (create_monster(mg) != -1)
                created++;
        }
    }
    else
    {
        for (; how_many > 0; --how_many)
            _delayed_monster(mg);
    }

    return (created);
}

static const item_def* _find_missile_launcher(int skill)
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].is_valid())
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

static bool _need_missile_gift(bool forced)
{
    const int best_missile_skill = best_skill(SK_SLINGS, SK_THROWING);
    const item_def *launcher = _find_missile_launcher(best_missile_skill);
    return ((forced || you.piety > 80
                       && random2( you.piety ) > 70
                       && one_chance_in(8))
            && you.skills[ best_missile_skill ] >= 8
            && (launcher || best_missile_skill == SK_THROWING));
}

void get_pure_deck_weights(int weights[])
{
    weights[0] = you.sacrifice_value[OBJ_ARMOUR] + 1;
    weights[1] = you.sacrifice_value[OBJ_WEAPONS]
                 + you.sacrifice_value[OBJ_STAVES]
                 + you.sacrifice_value[OBJ_MISSILES] + 1;
    weights[2] = you.sacrifice_value[OBJ_MISCELLANY]
                 + you.sacrifice_value[OBJ_JEWELLERY]
                 + you.sacrifice_value[OBJ_BOOKS];
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
        you.sacrifice_value[OBJ_MISCELLANY] *= 4;
        you.sacrifice_value[OBJ_JEWELLERY]  *= 4;
        you.sacrifice_value[OBJ_BOOKS]      *= 4;
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

#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS)
static void _show_pure_deck_chances()
{
    int weights[5];

    get_pure_deck_weights(weights);

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

static bool _give_nemelex_gift(bool forced = false)
{
    // But only if you're not levitating over deep water.
    // Merfolk don't get gifts in deep water. {due}
    if (!feat_has_solid_floor(grd(you.pos())))
        return (false);

    // Nemelex will give at least one gift early.
    if (forced
        || !you.num_gifts[GOD_NEMELEX_XOBEH]
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
        get_pure_deck_weights(weights);
        const int choice = choose_random_weighted(weights, weights+5);
        gift_type = pure_decks[choice];
#if defined(DEBUG_GIFTS) || defined(DEBUG_CARDS)
        _show_pure_deck_chances();
#endif
        int thing_created = items( 1, OBJ_MISCELLANY, gift_type,
                                   true, 1, MAKE_ITEM_RANDOM_RACE,
                                   0, 0, GOD_NEMELEX_XOBEH );

        move_item_to_grid(&thing_created, you.pos(), true);

        if (thing_created != NON_ITEM)
        {
            _update_sacrifice_weights(choice);

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

            simple_god_message(" grants you a gift!");
            more();
            canned_msg(MSG_SOMETHING_APPEARS);

            you.attribute[ATTR_CARD_COUNTDOWN] = 10;
            _inc_gift_timeout(5 + random2avg(9, 2));
            you.num_gifts[you.religion]++;
            take_note(Note(NOTE_GOD_GIFT, you.religion));
        }
        return (true);
    }

    return (false);
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

    ASSERT(!(mon->flags & MF_GOD_GIFT));

    mon->god = god;

#ifdef DEBUG
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

bool is_undead_slave(const monsters* mon)
{
    return (mon->alive() && mon->holiness() == MH_UNDEAD
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

bool is_fellow_slime(const monsters* mon)
{
    return (mon->alive() && mons_is_slime(mon)
            && mon->attitude == ATT_STRICT_NEUTRAL
            && mons_is_god_gift(mon, GOD_JIYVA));
}

bool is_neutral_plant(const monsters* mon)
{
    return (mon->alive() && mons_is_plant(mon)
            && mon->attitude == ATT_GOOD_NEUTRAL);
}

bool _has_jelly()
{
    ASSERT(you.religion == GOD_JIYVA);

    for (monster_iterator mi; mi; ++mi)
        if (mons_is_god_gift(*mi, GOD_JIYVA))
            return (true);
    return (false);
}

bool is_good_lawful_follower(const monsters* mon)
{
    return (mon->alive() && !mon->is_unholy() && !mon->is_evil()
            && !mon->is_unclean() && !mon->is_chaotic() && mon->friendly());
}

bool is_good_follower(const monsters* mon)
{
    return (mon->alive() && !mon->is_unholy() && !mon->is_evil()
            && mon->friendly());
}

bool is_follower(const monsters* mon)
{
    if (you.religion == GOD_YREDELEMNUL)
        return (is_undead_slave(mon));
    else if (you.religion == GOD_BEOGH)
        return (is_orcish_follower(mon));
    else if (you.religion == GOD_JIYVA)
        return (is_fellow_slime(mon));
    else if (you.religion == GOD_FEDHAS)
        return (is_neutral_plant(mon));
    else if (you.religion == GOD_ZIN)
        return (is_good_lawful_follower(mon));
    else if (is_good_god(you.religion))
        return (is_good_follower(mon));
    else
        return (mon->alive() && mon->friendly());
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
    if (mon->heal(healing + random2(healing + 1)))
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

    // Only brand weapons, and only override certain brands.
    if (is_artefact(wpn)
        || (wpn_brand != SPWPN_NORMAL && wpn_brand != SPWPN_DRAINING
            && wpn_brand != SPWPN_PAIN && wpn_brand != SPWPN_VAMPIRICISM
            && wpn_brand != SPWPN_REAPING && wpn_brand != SPWPN_CHAOS
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
    if (mon->res_negative_energy() == 3)
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
    const int base_increase = mon->is_holy() ? 1100 : 500;
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
            mgen_data(follower_type, BEH_FRIENDLY, &you, 0, 0,
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

    // Make sure monsters are shown.
    viewwindow(false);
    more();
    _inc_gift_timeout(4 + random2avg(7, 2));
    you.num_gifts[you.religion]++;
    take_note(Note(NOTE_GOD_GIFT, you.religion));
}

static bool _jiyva_mutate()
{
    simple_god_message(" alters your body.");

    const int rand = random2(100);

    if (rand < 10)
        return (delete_mutation(RANDOM_SLIME_MUTATION, true, false, true));
    else if (rand < 40)
        return (delete_mutation(RANDOM_NON_SLIME_MUTATION, true, false, true));
    else if (rand < 60)
        return (mutate(RANDOM_MUTATION, true, false, true));
    else if (rand < 75)
        return (mutate(RANDOM_SLIME_MUTATION, true, false, true));
    else
        return (mutate(RANDOM_GOOD_MUTATION, true, false, true));
}

static int _give_first_conjuration_book()
{
    // Assume the fire/earth book, as conjurations is strong with fire. -- bwr
    int book = BOOK_CONJURATIONS_I;

    // Conjuration books are largely Fire or Ice, so we'll use
    // that as the primary condition, and air/earth to break ties. -- bwr
    if (you.skills[SK_ICE_MAGIC] > you.skills[SK_FIRE_MAGIC]
        || you.skills[SK_FIRE_MAGIC] == you.skills[SK_ICE_MAGIC]
           && you.skills[SK_AIR_MAGIC] > you.skills[SK_EARTH_MAGIC])
    {
        book = BOOK_CONJURATIONS_II;
    }
    else if (you.skills[SK_FIRE_MAGIC] == 0 && you.skills[SK_EARTH_MAGIC] == 0)
    {
        // If we're here it's because we were going to default to the
        // fire/earth book... but we don't have those skills.  So we
        // choose randomly based on the species weighting, again
        // ignoring air/earth which are secondary in these books. - bwr
        if (random2(species_skills(SK_ICE_MAGIC, you.species)) <
            random2(species_skills(SK_FIRE_MAGIC, you.species)))
        {
            book = BOOK_CONJURATIONS_II;
        }
    }

    return (book);
}

bool do_god_gift(bool prayed_for, bool forced)
{
    ASSERT(you.religion != GOD_NO_GOD);

    // Zin and Jiyva worshippers are the only ones who can pray to ask their
    // god for stuff.  Jiyva also gives regular gifts.
    if (prayed_for && you.religion != GOD_ZIN && you.religion != GOD_JIYVA
        || !prayed_for && you.religion == GOD_ZIN)
    {
        return (false);
    }

    god_acting gdact;

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    int old_gifts = you.num_gifts[you.religion];
#endif

    bool success = false;

    // Consider a gift if we don't have a timeout and weren't already
    // praying when we prayed.
    if (forced
        || !player_under_penance() && !you.gift_timeout
        || prayed_for && (you.religion == GOD_ZIN || you.religion == GOD_JIYVA))
    {
        // Remember to check for water/lava.
        switch (you.religion)
        {
        default:
            break;

        case GOD_ZIN:
            //jmf: this "good" god will feed you (a la Nethack)
            if (forced || prayed_for && zin_sustenance())
            {
                god_speaks(you.religion, "Your stomach feels content.");
                set_hunger(6000, true);
                lose_piety(5 + random2avg(10, 2) + (you.gift_timeout ? 5 : 0));
                _inc_gift_timeout(30 + random2avg(10, 2));
                success = true;
            }
            break;

        case GOD_NEMELEX_XOBEH:
            success = _give_nemelex_gift(forced);
            break;

        case GOD_OKAWARU:
        case GOD_TROG:
        {
            // Break early if giving a gift now means it would be lost.
            if (!feat_has_solid_floor(grd(you.pos())))
                break;

            const bool need_missiles = _need_missile_gift(forced);

            if (forced && (!need_missiles || one_chance_in(4))
                || (!forced && you.piety > 130 && random2(you.piety) > 120
                    && one_chance_in(4)))
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
                    if (success)
                        _inc_gift_timeout(30 + random2avg(15, 2));
                }

                if (success)
                {
                    simple_god_message(" grants you a gift!");
                    more();

                    _inc_gift_timeout(30 + random2avg(19, 2));
                    you.num_gifts[you.religion]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
                break;
            }

            if (need_missiles)
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
        }
        case GOD_YREDELEMNUL:
            if (forced
                || random2(you.piety) >= piety_breakpoint(2) && one_chance_in(4))
            {
                // The maximum threshold occurs at piety_breakpoint(5).
                int threshold = (you.piety - piety_breakpoint(2)) * 20 / 9;
                yred_random_servants(threshold);

                _delayed_monster_done(" grants you @an@ undead servant@s@!",
                                      "", _delayed_gift_callback);
                success = true;
            }
            break;

        case GOD_JIYVA:
            if (prayed_for && jiyva_accepts_prayer())
                jiyva_paralyse_jellies();
            else if (forced ||
                     you.piety > 80 && random2(you.piety) > 50 && one_chance_in(4)
                     && you.gift_timeout == 0 && you.can_safely_mutate())
            {
                if (_jiyva_mutate())
                {
                    _inc_gift_timeout(15 + roll_dice(2, 4));
                    you.num_gifts[you.religion]++;
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }
                else
                    mpr("You feel as though nothing has changed.");
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_SIF_MUNA:
        case GOD_VEHUMET:

            // Break early if giving a gift now means it would be lost.
            if (!feat_has_solid_floor(grd(you.pos())))
                break;

            unsigned int gift = NUM_BOOKS;

            // Kikubaaqudgha gives the lesser Necromancy books in a quick
            // succession.
            if (you.religion == GOD_KIKUBAAQUDGHA)
            {
                if (you.piety >= piety_breakpoint(0)
                    && !you.had_book[BOOK_NECROMANCY])
                {
                    gift = BOOK_NECROMANCY;
                }
                else if (you.piety >= piety_breakpoint(2)
                         && !you.had_book[BOOK_DEATH])
                {
                    gift = BOOK_DEATH;
                }
                else if (you.piety >= piety_breakpoint(3)
                         && !you.had_book[BOOK_UNLIFE])
                {
                    gift = BOOK_UNLIFE;
                }
            }
            else if (forced || you.piety > 160 && random2(you.piety) > 100)
            {
                if (you.religion == GOD_SIF_MUNA)
                    gift = OBJ_RANDOM;
                else if (you.religion == GOD_VEHUMET)
                {
                    if (!you.had_book[BOOK_CONJURATIONS_I])
                        gift = _give_first_conjuration_book();
                    else if (!you.had_book[BOOK_POWER])
                        gift = BOOK_POWER;
                    else if (!you.had_book[BOOK_ANNIHILATIONS])
                        gift = BOOK_ANNIHILATIONS;  // Conjuration books.

                    if (you.skills[SK_CONJURATIONS] < you.skills[SK_SUMMONINGS]
                        || gift == NUM_BOOKS)
                    {
                        if (!you.had_book[BOOK_CALLINGS])
                            gift = BOOK_CALLINGS;
                        else if (!you.had_book[BOOK_SUMMONINGS])
                            gift = BOOK_SUMMONINGS;
                        else if (!you.had_book[BOOK_DEMONOLOGY])
                            gift = BOOK_DEMONOLOGY; // Summoning books.
                    }
                }
            }

            if (gift != NUM_BOOKS)
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
                        return (false);

                    // Mark the book type as known to avoid duplicate
                    // gifts if players don't read their gifts for some
                    // reason.
                    mark_had_book(gift);

                    move_item_to_grid( &thing_created, you.pos(), true );

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

                    // HACK: you.num_gifts keeps track of Necronomicon
                    // and weapon blessing for Kiku, so don't increase
                    // it.  Also, timeouts are meaningless for Kiku. evk
                    if (you.religion != GOD_KIKUBAAQUDGHA)
                    {
                        _inc_gift_timeout(40 + random2avg(19, 2));
                        you.num_gifts[you.religion]++;
                    }
                    take_note(Note(NOTE_GOD_GIFT, you.religion));
                }

                // Vehumet gives books less readily.
                if (you.religion == GOD_VEHUMET && success)
                    _inc_gift_timeout(10 + random2(10));
            }                   // End of giving books.
            break;              // End of book gods.
        }                       // switch (you.religion)
    }                           // End of gift giving.

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_GIFTS)
    if (old_gifts < you.num_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Total number of gifts from this god: %d",
             you.num_gifts[you.religion]);
    }
#endif
    return (success);
}

std::string god_name(god_type which_god, bool long_name)
{
    switch (which_god)
    {
    case GOD_NO_GOD: return "No God";
    case GOD_RANDOM: return "random";
    case GOD_NAMELESS: return "nameless";
    case GOD_VIABLE: return "viable";
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
    case GOD_JIYVA:
    {
        return (long_name ? god_name_jiyva(true) + " the Shapeless"
                          : god_name_jiyva(false));
    }
    case GOD_FEDHAS:        return (long_name ? "Fedhas Madash" : "Fedhas");
    case GOD_CHEIBRIADOS:  return (long_name ? "Cheibriados the Contemplative" : "Cheibriados");
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

std::string god_name_jiyva(bool second_name)
{
    std::string name = "Jiyva";
    if (second_name)
        name += " " + you.second_god_name;

    return (name);
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
    ASSERT(!crawl_state.game_is_arena());

    int orig_mon = mgrd(you.pos());

    monsters fake_mon;
    fake_mon.type       = MONS_PROGRAM_BUG;
    fake_mon.hit_points = 1;
    fake_mon.god        = god;
    fake_mon.set_position(you.pos());
    fake_mon.foe        = MHITYOU;
    fake_mon.mname      = "FAKE GOD MONSTER";

    mpr(do_mon_str_replacements(mesg, &fake_mon).c_str(), MSGCH_GOD, god);

    fake_mon.reset();
    mgrd(you.pos()) = orig_mon;
}

void religion_turn_start()
{
    if (you.turn_is_over)
        religion_turn_end();

    crawl_state.clear_god_acting();
}

void religion_turn_end()
{
    ASSERT(you.turn_is_over);
    _place_delayed_monsters();
}

std::string adjust_abil_message(const char *pmsg)
{
    int pos;
    std::string pm = pmsg;

    if ((pos = pm.find("{biology}")) != -1)
        switch(you.is_undead)
        {
        case US_UNDEAD:      // mummies -- time has no meaning!
            return "";
        case US_HUNGRY_DEAD: // ghouls
            pm.replace(pos, 9, "decay");
            break;
        case US_SEMI_UNDEAD: // vampires
        case US_ALIVE:
            pm.replace(pos, 9, "biology");
            break;
        }
    return (pm);
}

static bool _abil_chg_message(const char *pmsg, const char *youcanmsg)
{
    if (!*pmsg)
        return false;

    std::string pm = adjust_abil_message(pmsg);

    if (isupper(pmsg[0]))
        god_speaks(you.religion, pm.c_str());
    else
    {
        god_speaks(you.religion,
                   make_stringf(youcanmsg, pmsg).c_str());
    }
    return true;
}

void dock_piety(int piety_loss, int penance)
{
    static long last_piety_lecture   = -1L;
    static long last_penance_lecture = -1L;

    if (piety_loss <= 0 && penance <= 0)
        return;

    piety_loss = piety_scale(piety_loss);
    penance    = piety_scale(penance);

    if (piety_loss)
    {
        if (last_piety_lecture != you.num_turns)
        {
            // output guilt message:
            mprf("You feel%sguilty.",
                 (piety_loss == 1) ? " a little " :
                 (piety_loss <  5) ? " " :
                 (piety_loss < 10) ? " very "
                                   : " extremely ");
        }

        last_piety_lecture = you.num_turns;
        lose_piety(piety_loss);
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

// Scales a piety number, applying boosters (amulet of faith).
int piety_scale(int piety)
{
    if (piety < 0)
        return (-piety_scale(-piety));

    if (wearing_amulet(AMU_FAITH))
        return (piety + div_rand_round(piety, 3));

    return (piety);
}

void gain_piety(int original_gain, bool force)
{
    if (original_gain <= 0)
        return;

    if (crawl_state.game_is_sprint() && you.level_type == LEVEL_ABYSS && !force)
        return;

    // Xom uses piety differently...
    if (you.religion == GOD_NO_GOD || you.religion == GOD_XOM)
        return;

    int pgn = piety_scale(original_gain);

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
        // no longer have a piety cost for getting them.
        // Jiyva is an exception because there's usually a time-out and
        // the gifts aren't that precious.
        if (!one_chance_in(4) && you.religion != GOD_JIYVA)
        {
#ifdef DEBUG_PIETY
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
            do_god_gift();
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
            do_god_gift();
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

#ifdef DEBUG_PIETY
        mprf(MSGCH_DIAGNOSTICS, "Piety increasing by %d (and %d taken from "
                                "hysteresis, %d original)",
             pgn, pgn_borrowed, original_gain);
#endif
    }

    int old_piety = you.piety;

    if (crawl_state.game_is_sprint())
    {
        pgn = sprint_modify_piety(pgn);
    }

    you.piety += pgn;
    you.piety = std::min<int>(MAX_PIETY, you.piety);

    for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
    {
        if (you.piety >= piety_breakpoint(i)
            && old_piety < piety_breakpoint(i))
        {
            take_note(Note(NOTE_GOD_POWER, you.religion, i));

            // In case the best skill is Invocations, redraw the god
            // title.
            redraw_skill(you.your_name, player_title());

            if (_abil_chg_message(god_gain_power_messages[you.religion][i],
                              "You can now %s."))
            {
                learned_something_new(HINT_NEW_ABILITY_GOD);
            }

            if (you.religion == GOD_SHINING_ONE)
            {
                if (i == 0)
                    mpr("A divine halo surrounds you!");
            }

            // When you gain a piety level, you get another chance to
            // make hostile holy beings good neutral.
            if (is_good_god(you.religion))
                holy_beings_attitude_change();
        }
    }

    if (you.religion == GOD_BEOGH)
    {
        // Every piety level change also affects AC from orcish gear.
        you.redraw_armour_class = true;
    }

    if (you.religion == GOD_CHEIBRIADOS)
    {
        int diffrank = piety_rank(you.piety) - piety_rank(old_piety);
        che_handle_change(CB_PIETY, diffrank);
    }

    if (you.religion == GOD_SHINING_ONE)
    {
        // Piety change affects halo radius.
        invalidate_agrid();
    }

    if (you.piety > 160 && old_piety <= 160)
    {
        // In case the best skill is Invocations, redraw the god title.
        redraw_skill(you.your_name, player_title());

        if (!you.num_gifts[you.religion])
        {
            switch (you.religion)
            {
                case GOD_ZIN:
                    simple_god_message(" will now cure all your mutations... once.");
                    break;
                case GOD_SHINING_ONE:
                    simple_god_message(" will now bless your weapon at an altar... once.");
                    break;
                case GOD_KIKUBAAQUDGHA:
                    simple_god_message(" will now enhance your necromancy at an altar... once.");
                    break;
                case GOD_LUGONU:
                    simple_god_message(" will now corrupt your weapon at an altar... once.");
                    break;
                default:
                    break;
            }
        }

        // When you gain piety of more than 160, you get another chance
        // to make hostile holy beings good neutral.
        if (is_good_god(you.religion))
            holy_beings_attitude_change();
    }

    do_god_gift();
}

// Is the destroyed weapon valuable enough to gain piety by doing so?
// Unholy and evil weapons are handled specially.
static bool _destroyed_valuable_weapon(int value, int type)
{
    // Artefacts, including most randarts.
    if (random2(value) >= random2(250))
        return (true);

    // Medium valuable items are more likely to net piety at low piety,
    // more so for missiles, since they're worth less as single items.
    if (random2(value) >= random2((type == OBJ_MISSILES) ? 10 : 100)
        && one_chance_in(1 + you.piety / 50))
    {
        return (true);
    }

    // If not for the above, missiles shouldn't yield piety.
    if (type == OBJ_MISSILES)
        return (false);

    // Weapons, on the other hand, are always acceptable to boost low
    // piety.
    if (you.piety < piety_breakpoint(0) || player_under_penance())
        return (true);

    return (false);
}

bool ely_destroy_weapons()
{
    if (you.religion != GOD_ELYVILON)
        return (false);

    god_acting gdact;

    bool success = false;
    for (stack_iterator si(you.pos(), true); si; ++si)
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

        // item_value() multiplies by quantity.
        const int value = item_value(item, true) / item.quantity;
        dprf("Destroyed weapon value: %d", value);

        piety_gain_t pgain = PIETY_NONE;
        const bool unholy_weapon = is_unholy_item(item);
        const bool evil_weapon = is_evil_item(item);

        if (unholy_weapon || evil_weapon
            || _destroyed_valuable_weapon(value, item.base_type))
        {
            pgain = PIETY_SOME;
        }

        if (get_weapon_brand(item) == SPWPN_HOLY_WRATH)
        {
            // Weapons blessed by TSO don't get destroyed but are instead
            // returned whence they came. (jpeg)
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
            print_sacrifice_message(GOD_ELYVILON, item, pgain);
            if (unholy_weapon || evil_weapon)
            {
                const char *desc_weapon = evil_weapon ? "evil" : "unholy";

                // Print this in addition to the above!
                simple_god_message(
                    make_stringf(" welcomes the destruction of %s %s "
                                 "weapon%s.",
                                 item.quantity == 1 ? "this" : "these",
                                 desc_weapon,
                                 item.quantity == 1 ? ""     : "s").c_str(),
                    GOD_ELYVILON);
            }
        }

        if (pgain == PIETY_SOME)
            gain_piety(1);

        destroy_item(si.link());
        success = true;
    }

    if (!success)
        mpr("There are no weapons here to destroy!");

    return (success);
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
#ifdef DEBUG_PIETY
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
            && !you.num_gifts[you.religion])
        {
            // In case the best skill is Invocations, redraw the god
            // title.
            redraw_skill(you.your_name, player_title());

            if (you.religion == GOD_ZIN)
                simple_god_message(
                    " is no longer ready to cure all your mutations.");
            else if (you.religion == GOD_SHINING_ONE)
                simple_god_message(
                    " is no longer ready to bless your weapon.");
            else if (you.religion == GOD_KIKUBAAQUDGHA)
                simple_god_message(
                    " is no longer ready to enhance your necromancy.");
            else if (you.religion == GOD_LUGONU)
                simple_god_message(
                    " is no longer ready to corrupt your weapon.");
        }

        for (int i = 0; i < MAX_GOD_ABILITIES; ++i)
        {
            if (you.piety < piety_breakpoint(i)
                && old_piety >= piety_breakpoint(i))
            {
                // In case the best skill is Invocations, redraw the god
                // title.
                redraw_skill(you.your_name, player_title());

                _abil_chg_message(god_lose_power_messages[you.religion][i],
                                  "You can no longer %s.");

                if (_need_water_walking() && !beogh_water_walk())
                    fall_into_a_pool(you.pos(), true, grd(you.pos()));
            }
        }
    }

    if (!god_accepts_prayer(you.religion) && you.duration[DUR_PRAYER])
        end_prayer();

    if (you.piety > 0 && you.piety <= 5)
        learned_something_new(HINT_GOD_DISPLEASED);

    if (you.religion == GOD_BEOGH)
    {
        // Every piety level change also affects AC from orcish gear.
        you.redraw_armour_class = true;
    }

    if (you.religion == GOD_CHEIBRIADOS)
    {
        int diffrank = piety_rank(you.piety) - piety_rank(old_piety);
        che_handle_change(CB_PIETY, diffrank);
    }
}

// Fedhas worshipers are on the hook for most plants and fungi
//
// If fedhas worshipers kill a protected monster they lose piety,
// if they attack a friendly one they get penance,
// if a friendly one dies they lose piety.
bool fedhas_protects_species(int mc)
{
    return (mons_class_is_plant(mc)
            && mc != MONS_GIANT_SPORE);
}

bool fedhas_protects(const monsters * target)
{
    return target && fedhas_protects_species(target->mons_species());
}

// Fedhas neutralises most plants and fungi
bool fedhas_neutralises(const monsters * target)
{
    return (target && mons_is_plant(target));
}

void excommunication(god_type new_god)
{
    const god_type old_god = you.religion;
    ASSERT(old_god != new_god);
    ASSERT(old_god != GOD_NO_GOD);

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

    // Renouncing may have changed the conducts on our wielded or
    // quivered weapons, so refresh the display.
    you.wield_change = true;
    you.redraw_quiver = true;

    mpr("You have lost your religion!");
    more();

#ifdef DGL_MILESTONES
    mark_milestone("god.renounce", "abandoned " + god_name(old_god) + ".");
#endif

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
        yred_slaves_abandon_you();

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
        if (you.attribute[ATTR_DIVINE_REGENERATION])
            remove_regen(true);

        make_god_gifts_hostile(false);

        // Penance has to come before retribution to prevent "mollify"
        _inc_penance(old_god, 50);
        divine_retribution(old_god);
        break;

    case GOD_BEOGH:
        beogh_followers_abandon_you(); // friendly orcs around turn hostile

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
            make_god_gifts_hostile(false);
        }
        else
            make_holy_god_gifts_good_neutral(false);

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

            make_god_gifts_hostile(false);
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

            make_god_gifts_hostile(false);
        }
        break;

    case GOD_JIYVA:
        jiyva_slimes_abandon_you();

        if (you.duration[DUR_SLIMIFY])
            you.duration[DUR_SLIMIFY] = 0;

        if (you.can_safely_mutate())
        {
            god_speaks(old_god, "You feel Jiyva alter your body.");

            for (int i = 0; i < 2; ++i)
                mutate(RANDOM_BAD_MUTATION, true, false, true);
        }

        _inc_penance(old_god, 30);
        break;
    case GOD_FEDHAS:
        fedhas_plants_hostile();
        _inc_penance(old_god, 30);
        divine_retribution(old_god);
        break;

    case GOD_CHEIBRIADOS:
    default:
        _inc_penance(old_god, 25);
        break;
    }

    // When you start worshipping a non-good god, or no god, you make
    // all non-hostile holy beings that worship a good god hostile.
    if (!is_good_god(new_god) && holy_beings_attitude_change())
        mpr("The divine host forsakes you.", MSGCH_MONSTER_ENCHANT);

    // Evil hack.
    learned_something_new(HINT_EXCOMMUNICATE,
                          coord_def((int)new_god, old_piety));
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

static void _erase_between(std::string& s,
                           const std::string &left,
                           const std::string &right)
{
    std::string::size_type left_pos;
    std::string::size_type right_pos;

    while ((left_pos = s.find(left)) != std::string::npos
           && (right_pos = s.find(right, left_pos + left.size())) != std::string::npos)
        s.erase(s.begin() + left_pos, s.begin() + right_pos + right.size());
}

void print_sacrifice_message(god_type god, const item_def &item,
                             piety_gain_t piety_gain, bool your)
{
    std::string msg(_Sacrifice_Messages[god][piety_gain]);
    std::string itname = item.name(your ? DESC_CAP_YOUR : DESC_CAP_THE);
    if (itname.find("glowing") != std::string::npos)
    {
        _replace(msg, "[", "");
        _replace(msg, "]", "");
    }
    else
        _erase_between(msg, "[", "]");
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

    msg.insert(0, itname);
    msg = tag_start + msg + tag_end;

    mpr(msg, MSGCH_GOD);
}

bool god_hates_attacking_friend(god_type god, const actor *fr)
{
    if (!fr || fr->kill_alignment() != KC_FRIENDLY)
        return (false);

    return (god_hates_attacking_friend(god, fr->mons_species()));
}

bool god_hates_attacking_friend(god_type god, int species)
{
    if (mons_is_projectile(species))
        return (false);
    switch (god)
    {
        case GOD_ZIN:
        case GOD_SHINING_ONE:
        case GOD_ELYVILON:
        case GOD_OKAWARU:
            return (true);
        case GOD_BEOGH: // added penance to avoid killings for loot
            return (mons_genus(species) == MONS_ORC);
        case GOD_JIYVA:
            return (mons_class_is_slime(species));
        case GOD_FEDHAS:
            return fedhas_protects_species(species);
        default:
            return (false);
    }
}

bool god_likes_items(god_type god)
{
    if (god_likes_fresh_corpses(god))
        return (true);

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

bool god_likes_item(god_type god, const item_def& item)
{
    if (!god_likes_items(god))
        return (false);

    if (god_likes_fresh_corpses(god))
    {
        return (item.base_type == OBJ_CORPSES
                    && item.sub_type == CORPSE_BODY
                    && !food_is_rotten(item));
    }

    switch (god)
    {
    case GOD_ZIN:
        return (item.base_type == OBJ_GOLD);

    case GOD_SHINING_ONE:
        return (is_unholy_item(item) || is_evil_item(item));

    case GOD_BEOGH:
        return (item.base_type == OBJ_CORPSES
                   && mons_species(item.plus) == MONS_ORC);

    case GOD_NEMELEX_XOBEH:
        return (!is_deck(item)
                && !item.is_critical()
                && !is_rune(item)
                && item.base_type != OBJ_GOLD
                && (item.base_type != OBJ_MISCELLANY
                    || item.sub_type != MISC_HORN_OF_GERYON
                    || item.plus2));

    default:
        return (false);
    }
}

bool player_can_join_god(god_type which_god)
{
    if (you.species == SP_DEMIGOD)
        return (false);

    if (is_good_god(which_god) && you.undead_or_demonic())
        return (false);

    if (which_god == GOD_BEOGH && you.species != SP_HILL_ORC)
        return (false);

    // Fedhas hates undead, but will accept demonspawn.
    if (which_god == GOD_FEDHAS && you.holiness() == MH_UNDEAD)
        return (false);

    if (which_god == GOD_SIF_MUNA && !you.spell_no)
        return (false);

    return (true);
}

// Identify any interesting equipment when the player signs up with a
// new Service Pro^W^Wdeity.
void god_welcome_identify_gear()
{
    // Check for amulets of faith.
    item_def *amulet = you.slot_item(EQ_AMULET, false);
    if (amulet && amulet->sub_type == AMU_FAITH)
    {
        // The flash happens independent of item id.
        mpr("Your amulet flashes!", MSGCH_GOD);
        flash_view_delay(god_colour(you.religion), 300);
        set_ident_type(*amulet, ID_KNOWN_TYPE);
    }
}

void god_pitch(god_type which_god)
{
    mprf("You %s the altar of %s.",
         you.species == SP_NAGA ? "coil in front of"
                                : "kneel at",
         god_name(which_god).c_str());
    more();

    // Note: using worship we could make some gods not allow followers to
    // return, or not allow worshippers from other religions. - bwr

    // Gods can be racist...
    if (!player_can_join_god(which_god))
    {
        you.turn_is_over = false;
        if (which_god == GOD_SIF_MUNA)
            simple_god_message(" does not accept worship from the ignorant!",
                               which_god);
        else if (you.attribute[ATTR_TRANSFORMATION] == TRAN_LICH)
            simple_god_message(" says: How dare you come in such a loathsome form!",
                               which_god);
        else
            simple_god_message(" does not accept worship from those such as you!",
                               which_god);
        return;
    }

    if (which_god == GOD_LUGONU && you.penance[GOD_LUGONU])
    {
        simple_god_message(" is most displeased with you!", which_god);
        divine_retribution(GOD_LUGONU, true);
        return;
    }

    describe_god(which_god, false);

    snprintf(info, INFO_SIZE, "Do you wish to %sjoin this religion?",
             (you.worshipped[which_god]) ? "re" : "");

    cgotoxy(1, 18, GOTO_CRT);
    textcolor(channel_to_colour(MSGCH_PROMPT));
    if (!yesno(info, false, 'n', true, true, false, NULL, GOTO_CRT))
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
        you.piety = HALF_MAX_PIETY;
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

#ifdef DGL_MILESTONES
    mark_milestone("god.worship", "became a worshipper of "
                   + god_name(you.religion) + ".");
#endif

    simple_god_message(
        make_stringf(" welcomes you%s!",
                     you.worshipped[which_god] ? " back" : "").c_str());
    more();
    if (crawl_state.game_is_tutorial())
    {
        // Tutorial needs minor destruction usable.
        gain_piety(35, true);
    }

    god_welcome_identify_gear();

    // When you start worshipping a good god, you make all non-hostile
    // unholy and evil beings hostile; when you start worshipping Zin,
    // you make all non-hostile unclean and chaotic beings hostile; and
    // when you start worshipping Trog, you make all non-hostile magic
    // users hostile.
    if (is_good_god(you.religion) && unholy_and_evil_beings_attitude_change())
        mpr("Your unholy and evil allies forsake you.", MSGCH_MONSTER_ENCHANT);

    if (you.religion == GOD_ZIN && unclean_and_chaotic_beings_attitude_change())
        mpr("Your unclean and chaotic allies forsake you.", MSGCH_MONSTER_ENCHANT);
    else if (you.religion == GOD_TROG && spellcasters_attitude_change())
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
    else if (you.religion == GOD_FEDHAS)
    {
        mpr("You can call upon Fedhas to speed up the decay of corpses.",
            MSGCH_GOD);
        mpr("The plants of the dungeon cease their hostilities.", MSGCH_GOD);
    }
    else if (you.religion == GOD_CHEIBRIADOS)
    {
        mpr("You can now call upon Cheibriados to make your armour ponderous.",
            MSGCH_GOD);
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
        // permanent instead of based off of penance. - bwr
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

    // Complimentary jelly upon joining.
    if (you.religion == GOD_JIYVA)
    {
        if (you.worshipped[GOD_JIYVA] == 1)
        {
            dlua.callfn("dgn_set_persistent_var", "sb",
                        "fix_slime_vaults", true);
            // If we're on Slime:6, pretend we just entered the level
            // in order to bring down the vault walls.
            if (level_id::current() == level_id(BRANCH_SLIME_PITS, 6))
                dungeon_events.fire_event(DET_ENTERED_LEVEL);
        }

        if (!_has_jelly())
        {
            monster_type mon = MONS_JELLY;
            mgen_data mg(mon, BEH_STRICT_NEUTRAL, &you, 0, 0, you.pos(),
                         MHITNOT, 0, GOD_JIYVA);

            _delayed_monster(mg);
            simple_god_message(" grants you a jelly!");
        }
    }

    // Refresh wielded/quivered weapons in case we have a new conduct
    // on them.
    you.wield_change = true;
    you.redraw_quiver = true;

    redraw_skill(you.your_name, player_title());

    learned_something_new(HINT_CONVERT);
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

    if (god == GOD_FEDHAS)
        retval = (fedhas_protects(mon));

    return (retval);
}

bool god_likes_fresh_corpses(god_type god)
{
    return (god == GOD_OKAWARU
            || god == GOD_MAKHLEB
            || god == GOD_TROG
            || god == GOD_LUGONU
            || (god == GOD_KIKUBAAQUDGHA && you.piety >= piety_breakpoint(4)));
}

bool god_likes_spell(spell_type spell, god_type god)
{
    switch(god)
    {
    case GOD_VEHUMET:
        return (vehumet_supports_spell(spell));
    case GOD_KIKUBAAQUDGHA:
        return (spell_typematch(spell, SPTYP_NECROMANCY));
    default: // quash unhandled constants warnings
        return (false);
    }
}

bool god_hates_spell(spell_type spell, god_type god)
{
    if (is_good_god(god) && (is_unholy_spell(spell) || is_evil_spell(spell)))
        return (true);

    unsigned int disciplines = get_spell_disciplines(spell);

    switch (god)
    {
    case GOD_ZIN:
        if (is_unclean_spell(spell) || is_chaotic_spell(spell))
            return (true);
        break;
    case GOD_SHINING_ONE:
        // TSO hates using poison, but is fine with curing it, resisting
        // it, or destroying it.
        if ((disciplines & SPTYP_POISON) && spell != SPELL_CURE_POISON
            && spell != SPELL_RESIST_POISON && spell != SPELL_IGNITE_POISON)
            return (true);
    case GOD_YREDELEMNUL:
        if (is_holy_spell(spell))
            return (true);
        break;
    case GOD_FEDHAS:
        if (is_corpse_violating_spell(spell))
            return (true);
        break;
    case GOD_CHEIBRIADOS:
        if (is_hasty_spell(spell))
            return (true);
        break;
    default:
        break;
    }
    return (false);
}

harm_protection_type god_protects_from_harm(god_type god, bool actual)
{
    const int min_piety = piety_breakpoint(0);
    bool praying = (you.duration[DUR_PRAYER]
                    && random2(piety_scale(you.piety)) >= min_piety);
    bool reliable = (!actual || you.duration[DUR_PRAYER])
                    && you.piety > 130;
    bool anytime = (one_chance_in(10) ||
                    x_chance_in_y(piety_scale(you.piety), 1000));
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
        if (!actual || praying || reliable || anytime)
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
            return;

        // These gods like long-standing worshippers.
        case GOD_ELYVILON:
            if (_need_free_piety() && one_chance_in(20))
                gain_piety(1);
            return;

        case GOD_SHINING_ONE:
            if (_need_free_piety() && one_chance_in(15))
                gain_piety(1);
            return;

        case GOD_ZIN:
            if (_need_free_piety() && one_chance_in(12))
                gain_piety(1);
            return;

        // All the rest will excommunicate you if piety goes below 1.
        case GOD_YREDELEMNUL:
        case GOD_KIKUBAAQUDGHA:
        case GOD_VEHUMET:
            if (one_chance_in(17))
                lose_piety(1);
            break;

        // These gods accept corpses, so they time-out faster.
        case GOD_OKAWARU:
        case GOD_TROG:
            if (one_chance_in(14))
                lose_piety(1);
            break;

        case GOD_MAKHLEB:
        case GOD_BEOGH:
        case GOD_LUGONU:
            if (one_chance_in(16))
                lose_piety(1);
            break;

        case GOD_SIF_MUNA:
            // [dshaligram] Sif Muna is now very patient - has to be
            // to make up for the new spell training requirements, else
            // it's practically impossible to get Master of Arcane status.
            if (one_chance_in(100))
                lose_piety(1);
            break;

        case GOD_NEMELEX_XOBEH:
            // Nemelex is relatively patient.
            if (one_chance_in(35))
                lose_piety(1);
            if (you.attribute[ATTR_CARD_COUNTDOWN] > 0 && coinflip())
                you.attribute[ATTR_CARD_COUNTDOWN]--;
            break;

        case GOD_JIYVA:
            if (one_chance_in(20))
                lose_piety(1);
            break;

        case GOD_FEDHAS:
        case GOD_CHEIBRIADOS:
            // Fedhas's piety is stable over time but we need a case here to
            // avoid the error message below.
            break;

        default:
            DEBUGSTR("Bad god, no bishop!");
            return;
        }

        if (you.piety < 1)
            excommunication();
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
    case GOD_FEDHAS:
        return (CYAN);

    case GOD_YREDELEMNUL:
    case GOD_KIKUBAAQUDGHA:
    case GOD_MAKHLEB:
    case GOD_VEHUMET:
    case GOD_TROG:
    case GOD_BEOGH:
    case GOD_LUGONU:
        return (LIGHTRED);

    case GOD_XOM:
        return (YELLOW);

    case GOD_NEMELEX_XOBEH:
        return (LIGHTMAGENTA);

    case GOD_SIF_MUNA:
        return (LIGHTBLUE);

    case GOD_JIYVA:
        return (GREEN);

    case GOD_CHEIBRIADOS:
        return (LIGHTCYAN);

    case GOD_NO_GOD:
    case NUM_GODS:
    case GOD_RANDOM:
    case GOD_NAMELESS:
    default:
        break;
    }

    return (YELLOW);
}

char god_message_altar_colour(god_type god)
{
    int rnd;

    switch (god)
    {
    case GOD_SHINING_ONE:
        return (YELLOW);

    case GOD_ZIN:
        return (WHITE);

    case GOD_ELYVILON:
        return (LIGHTBLUE);     // Really, LIGHTGREY but that's plain text.

    case GOD_OKAWARU:
        return (CYAN);

    case GOD_YREDELEMNUL:
        return (coinflip() ? DARKGREY : RED);

    case GOD_BEOGH:
        return (coinflip() ? BROWN : LIGHTRED);

    case GOD_KIKUBAAQUDGHA:
        return (DARKGREY);

    case GOD_FEDHAS:
        return (coinflip() ? BROWN : GREEN);

    case GOD_XOM:
        return (random2(15) + 1);

    case GOD_VEHUMET:
        rnd = random2(3);
        return ((rnd == 0) ? LIGHTMAGENTA :
                (rnd == 1) ? LIGHTRED
                           : LIGHTBLUE);

    case GOD_MAKHLEB:
        rnd = random2(3);
        return ((rnd == 0) ? RED :
                (rnd == 1) ? LIGHTRED
                           : YELLOW);

    case GOD_TROG:
        return (RED);

    case GOD_NEMELEX_XOBEH:
        return (LIGHTMAGENTA);

    case GOD_SIF_MUNA:
        return (BLUE);

    case GOD_LUGONU:
        return (LIGHTRED);

    case GOD_CHEIBRIADOS:
        return (LIGHTCYAN);

    case GOD_JIYVA:
        return (coinflip() ? GREEN : LIGHTGREEN);

    default:
        return (YELLOW);
    }
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
        return (255);
    else
        return (breakpoints[i]);
}

// Returns true if the Shining One doesn't mind your using unchivalric
// attacks on this creature.
bool tso_unchivalric_attack_safe_monster(const monsters *mon)
{
    const mon_holy_type holiness = mon->holiness();
    return (mons_intel(mon) < I_NORMAL
            || mon->undead_or_demonic()
            || !mon->is_holy() && holiness != MH_NATURAL);
}

int get_tension(god_type god, bool count_travelling)
{
    int total = 0;

    bool nearby_monster = false;
    for (monster_iterator mons; mons; ++mons)
    {
        if (!mons->alive())
            continue;

        if (you.see_cell(mons->pos()))
        {
            if (!mons_can_hurt_player(*mons))
                continue;

            // Monster is nearby.
            if (!nearby_monster && !mons->wont_attack())
                nearby_monster = true;
        }
        else if (count_travelling)
        {
            // Is the monster trying to get somewhere nearby?
            coord_def    target;
            unsigned int travel_size = mons->travel_path.size();

            // If the monster is too far away, it doesn't count.
            if (travel_size > 3)
                continue;

            if (travel_size > 0)
                target = mons->travel_path[travel_size - 1];
            else
                target = mons->target;

            // Monster is neither nearby nor trying to get near us.
            if (!in_bounds(target) || !you.see_cell(target))
                continue;
        }

        const mon_attitude_type att = mons_attitude(*mons);
        if (att == ATT_GOOD_NEUTRAL || att == ATT_NEUTRAL)
            continue;

        if (mons->cannot_act() || mons->asleep() || mons_is_fleeing(*mons))
            continue;

        int exper = exper_value(*mons);
        if (exper <= 0)
            continue;

        // Almost dead monsters don't count as much.
        exper *= mons->hit_points;
        exper /= mons->max_hit_points;

        bool gift = false;

        if (god != GOD_NO_GOD)
            gift = mons_is_god_gift(*mons, god);

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
            if (!you.visible_to(*mons))
                exper /= 2;
            if (!mons->visible_to(&you))
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

        if (mons->has_ench(ENCH_MIGHT))
        {
            exper *= 5;
            exper /= 4;
        }

        if (mons->berserk())
        {
            // in addition to haste and might bonuses above
            exper *= 3;
            exper /= 2;
        }

        total += exper;
    }

    // At least one monster has to be nearby, for tension to count.
    if (!nearby_monster)
        return (0);

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
    {
        if (tension < 2)
            tension = 2;
        else
        {
            tension *= 3;
            tension /= 2;
        }
    }

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

            if (placed == 1)
                msg = replace_all(msg, "@s@", "");
            else
                msg = replace_all(msg, "@s@", "s");

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

            // Fake its coming from simple_god_message().
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


static bool _is_god(god_type god)
{
    return (god > GOD_NO_GOD && god < NUM_GODS);
}

static bool _is_temple_god(god_type god)
{
    if (!_is_god(god))
        return (false);

    switch(god)
    {
    case GOD_NO_GOD:
    case GOD_LUGONU:
    case GOD_BEOGH:
    case GOD_JIYVA:
        return false;

    default:
        return true;
    }
}

static bool _is_nontemple_god(god_type god)
{
    return (_is_god(god) && !_is_temple_god(god));
}

static bool _cmp_god_by_name(god_type god1, god_type god2)
{
    return (god_name(god1, false) < god_name(god2, false));
}

// Vector of temple gods.
// Sorted by name for the benefit of the overview.
std::vector<god_type> temple_god_list()
{
    std::vector<god_type> god_list;
    for (int i = 0; i < NUM_GODS; i++)
    {
        god_type god = static_cast<god_type>(i);
        if (_is_temple_god(god))
            god_list.push_back(god);
    }
    std::sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}

// Vector of non-temple gods.
// Sorted by name for the benefit of the overview.
std::vector<god_type> nontemple_god_list()
{
    std::vector<god_type> god_list;
    for (int i = 0; i < NUM_GODS; i++)
    {
        god_type god = static_cast<god_type>(i);
        if (_is_nontemple_god(god))
            god_list.push_back(god);
    }
    std::sort(god_list.begin(), god_list.end(), _cmp_god_by_name);
    return god_list;
}
