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
#include "options.h"

#include "abl-show.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "chardump.h"
#include "database.h"
#include "debug.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "effects.h"
#include "enum.h"
#include "map_knowledge.h"
#include "fprop.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "godabil.h"
#include "goditem.h"
#include "godwrath.h"
#include "hiscores.h"
#include "invent.h"
#include "it_use2.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "items.h"
#include "kills.h"
#include "los.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "monplace.h"
#include "monstuff.h"
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "quiver.h"
#include "shopping.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-mis.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transfor.h"
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
// Text between [] only appears if the item already glows.
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
        " & disappears into the void.",
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
      "",
      "",
      "call upon Zin to create a sanctuary" },
    // TSO
    { "You and your allies can gain power from killing evil.",
      "call upon the Shining One for a divine shield",
      "",
      "channel blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "receive cadavers from Kikubaaqudgha",
      "",
      "",
      "Kikubaaqudgha is protecting you from unholy torment.",
      "invoke torment by butchering corpses during prayer" },
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
    { "call sunshine",
      "cause a ring of plants to grow",
      "control the weather",
      "spawn explosive spores",
      "induce evolution"
    },
    // Cheibriados
    { "Cheibriados is slowing your {biology}.",
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
      "",
      "",
      "call upon Zin to create a sanctuary" },
    // TSO
    { "You and your allies can no longer gain power from killing evil.",
      "call upon the Shining One for a divine shield",
      "",
      "channel blasts of cleansing flame",
      "summon a divine warrior" },
    // Kikubaaqudgha
    { "receive cadavers from Kikubaaqudgha",
      "",
      "",
      "Kikubaaqudgha will no longer protect you from unholy torment.",
      "invoke torment by butchering corpses during prayer" },
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
    { "call sunshine",
      "cause a ring of plants to grow",
      "control the weather",
      "spawn explosive spores",
      "induce evolution"
    },
    // Cheibriados
    { "Cheibriados will no longer slow your {biology}.",
      "bend time to slow others",
      "",
      "inflict damage to those overly hasty",
      "step out of the time flow"
    }
};

static bool _altar_prayer();
static bool _god_likes_item(god_type god, const item_def& item);
static void _dock_piety(int piety_loss, int penance);
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
        snprintf(info, INFO_SIZE, "you promote decomposition of nearby corpses%s",
                 verbose ? " via the <w>a</w> command" : "");
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
        snprintf(info, INFO_SIZE, "you destroy weapons (especially evil ones)%s",
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
        snprintf(info, INFO_SIZE, "you sacrifice evil items%s",
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

    if (god_likes_fresh_corpses(which_god))
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
        likes.push_back("you or your undead slaves kill living beings");
        break;

    case GOD_KIKUBAAQUDGHA:
        likes.push_back("you kill living beings");
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
        really_likes.push_back("you kill chaotic monsters");
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
        dislikes.push_back("you use necromancy");
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
        dislikes.push_back("you use chaotic magic or items");
        dislikes.push_back("you eat the flesh of sentient beings");
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
#if DEBUG_PIETY
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
        { // Nemelex' penance works actively only until 100
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
    monster_type mon = MONS_PROGRAM_BUG;
    int how_many = 1;
    const int temp_rand = random2(std::min(100, threshold));

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

static int _ammo_count(const item_def *launcher)
{
    int count = 0;
    const missile_type mt = launcher? fires_ammo_type(*launcher) : MI_DART;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!you.inv[i].is_valid())
            continue;

        const item_def &item = you.inv[i];
        if (item.base_type != OBJ_MISSILES)
            continue;

        if (item.sub_type == mt
            || mt == MI_STONE && item.sub_type == MI_SLING_BULLET)
        {
            count += item.quantity;
        }
    }

    return (count);
}

static bool _need_missile_gift()
{
    const int best_missile_skill = best_skill(SK_SLINGS, SK_THROWING);
    const item_def *launcher = _find_missile_launcher(best_missile_skill);
    return (you.piety > 80
            && random2( you.piety ) > 70
            && !feat_destroys_items( grd(you.pos()) )
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
    if (feat_destroys_items(grd(you.pos())))
        return;

    // Nemelex will give at least one gift early.
    if (!you.num_gifts[GOD_NEMELEX_XOBEH]
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

            move_item_to_grid(&thing_created, you.pos());

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

    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *monster = &menv[i];
        if (mons_is_god_gift(monster, GOD_JIYVA))
            return (true);
    }

    return (false);
}

bool is_good_lawful_follower(const monsters* mon)
{
    return (mon->alive() && !mon->is_evil() && !mon->is_chaotic()
            && mon->friendly());
}

bool is_good_follower(const monsters* mon)
{
    return (mon->alive() && !mon->is_evil() && mon->friendly());
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

    // Zin and Jiyva worshippers are the only ones who can pray to ask their
    // god for stuff.
    if (prayed_for != (you.religion == GOD_ZIN || you.religion == GOD_JIYVA))
        return;

    god_acting gdact;

#if DEBUG_DIAGNOSTICS || DEBUG_GIFTS
    int old_gifts = you.num_gifts[you.religion];
#endif

    // Consider a gift if we don't have a timeout and weren't already
    // praying when we prayed.
    if (!player_under_penance() && !you.gift_timeout
        || (prayed_for && you.religion == GOD_ZIN || you.religion == GOD_JIYVA))
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
                && !feat_destroys_items(grd(you.pos()))
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
                yred_random_servants(threshold);

                _delayed_monster_done(" grants you @an@ undead servant@s@!",
                                      "", _delayed_gift_callback);
            }
            break;

        case GOD_JIYVA:
            if (prayed_for && jiyva_grant_jelly())
            {
                int jelly_count = 0;
                for (radius_iterator ri(you.pos(), 9); ri; ++ri)
                {
                    int item = igrd(*ri);

                    if (item != NON_ITEM)
                    {
                        for (stack_iterator si(*ri); si; ++si)
                            if (one_chance_in(7))
                                jelly_count++;
                    }
                }

                if (jelly_count > 0)
                {
                    int count_created = 0;
                    for (; jelly_count > 0; --jelly_count)
                    {
                        mgen_data mg(MONS_JELLY, BEH_STRICT_NEUTRAL, 0, 0,
                                     you.pos(), MHITNOT, 0, GOD_JIYVA);

                        if (create_monster(mg) != -1)
                            count_created++;

                        // Sanity check: Stop if spawning further jellies
                        // would excommunicate us.
                        if (you.piety - (count_created + 1) * 5 <= 0)
                            break;
                    }

                    if (count_created > 0)
                    {
                        mprf(MSGCH_PRAY, "%s!",
                             count_created > 1 ? "Some jellies appear"
                                               : "A jelly appears");
                    }

                    lose_piety(5 * count_created);
                }
            }
            break;

        case GOD_KIKUBAAQUDGHA:
        case GOD_SIF_MUNA:
        case GOD_VEHUMET:

            unsigned int gift = NUM_BOOKS;

            // Kikubaaqudgha gives the lesser Necromancy books in a quick
            // succession.
            if (you.religion == GOD_KIKUBAAQUDGHA)
            {
                if (you.piety >= piety_breakpoint(0)
                    && !you.had_book[BOOK_NECROMANCY])
                    gift = BOOK_NECROMANCY;
                else if (you.piety >= piety_breakpoint(2)
                    && !you.had_book[BOOK_DEATH])
                    gift = BOOK_DEATH;
                else if (you.piety >= piety_breakpoint(3)
                    && !you.had_book[BOOK_UNLIFE])
                    gift = BOOK_UNLIFE;
            }
            else if (you.piety > 160 && random2(you.piety) > 100)
            {
                if (you.religion == GOD_SIF_MUNA)
                    gift = OBJ_RANDOM;
                else if (you.religion == GOD_VEHUMET)
                {
                    if (!you.had_book[BOOK_CONJURATIONS_I])
                        gift = give_first_conjuration_book();
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

            if (gift != NUM_BOOKS
                && !feat_destroys_items(grd(you.pos())))
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

                    // Mark the book type as known to avoid duplicate
                    // gifts if players don't read their gifts for some
                    // reason.
                    mark_had_book(gift);

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

#if DEBUG_DIAGNOSTICS || DEBUG_GIFTS
    if (old_gifts < you.num_gifts[you.religion])
    {
        mprf(MSGCH_DIAGNOSTICS, "Total number of gifts from this god: %d",
             you.num_gifts[you.religion]);
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

    if (god_likes_fresh_corpses(god) || god_likes_butchery(god))
        return (true);

    switch (god)
    {
    case GOD_ZIN:
        return (zin_sustenance(false));

    case GOD_YREDELEMNUL:
        return (yred_injury_mirror(false));

    case GOD_JIYVA:
        return (jiyva_grant_jelly(false));

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

    bool something_happened = false;
    const god_type altar_god = feat_altar_god(grd(you.pos()));
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
            something_happened = _altar_prayer();
        }
        else if (altar_god != GOD_NO_GOD)
        {
            if (you.species == SP_DEMIGOD)
            {
                you.turn_is_over = false;
                mpr("Sorry, a being of your status cannot worship here.");
                return;
            }

            god_pitch(feat_altar_god(grd(you.pos())));
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
        if (!something_happened)
        {
            simple_god_message(" ignores your prayer.");
            you.turn_is_over = false;
        }
        return;
    }

    // Beoghites and Nemelexites can abort now instead of offering
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

    // Assume for now that gods who like fresh corpses and/or butchery
    // don't use prayer for anything else.
    if (you.religion == GOD_ZIN
        || you.religion == GOD_BEOGH
        || you.religion == GOD_NEMELEX_XOBEH
        || you.religion == GOD_JIYVA
        || god_likes_fresh_corpses(you.religion)
        || god_likes_butchery(you.religion))
    {
        you.duration[DUR_PRAYER] = 1;
    }
    else if (you.religion == GOD_ELYVILON
        || you.religion == GOD_YREDELEMNUL)
    {
        you.duration[DUR_PRAYER] = 20;
    }

    // Gods who like fresh corpses, Beoghites and Nemelexites offer the
    // items they're standing on.
    if (altar_god == GOD_NO_GOD
        && (god_likes_fresh_corpses(you.religion)
            || you.religion == GOD_BEOGH || you.religion == GOD_NEMELEX_XOBEH))
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

std::string god_name(god_type which_god, bool long_name)
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
    ASSERT(!crawl_state.arena);

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
            if (you.religion == GOD_FEDHAS)
            {
                if (known)
                {
                    piety_change = -level;
                    penance = level;
                    retval = true;
                }
                else
                {
                    simple_god_message(" forgives your inadvertent necromancy, "
                                       "just this once.");
                }
                break;
            }
            // else fall-through

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

                if (thing_done == DID_ATTACK_HOLY
                    && !testbits(victim->flags, MF_CREATED_FRIENDLY)
                    && !testbits(victim->flags, MF_WAS_NEUTRAL))
                {
                    break;
                }

                piety_change = -level;
                penance = level * ((you.religion == GOD_SHINING_ONE) ? 2
                                                                     : 1);
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

       case DID_KILL_SLIME:
            if (you.religion == GOD_JIYVA)
            {
                retval = true;
                piety_change = -level;
                penance = level * 2;
            }
            break;

       case DID_KILL_PLANT:
       case DID_ALLY_KILLED_PLANT:
           // Piety loss but no penance for killing a plant.
           if (you.religion == GOD_FEDHAS)
           {
               retval = true;
               piety_change = -level;
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

            case GOD_JIYVA:
                if (mons_is_slime(victim))
                {
                    piety_change = -(level/2 + 3);
                    penance = level/2 + 3;
                    retval = true;
                }
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
                // Converted allies (marked as TSOites) can be martyrs.
                if (victim->god == GOD_SHINING_ONE)
                    break;

                if (player_under_penance())
                    penance = 1;  // if already under penance smaller bonus
                else
                    penance = level;
                // fall through

            case GOD_ZIN:
                // Converted allies (marked as TSOites) can be martyrs.
                if (victim->god == GOD_SHINING_ONE)
                    break;
                // fall through

            case GOD_FEDHAS:
                // double-check god because of fall-throughs from other gods
                // Toadstools are an exception for this conduct
                if (you.religion == GOD_FEDHAS && (!fedhas_protects(victim)
                        || victim->mons_species() == MONS_TOADSTOOL))
                    break;
                // fall through

            case GOD_OKAWARU:
                piety_change = -level;
                retval = true;
                break;

            default:
                break;
            }
            break;

        case DID_DEDICATED_BUTCHERY:
            switch (you.religion)
            {
            case GOD_ELYVILON:
                simple_god_message(" does not appreciate your butchering the "
                                   "dead during prayer!");
                retval = true;
                piety_change = -level;
                penance = level;
                break;

            default:
                break;
            }
            break;

        case DID_KILL_LIVING:
            switch (you.religion)
            {
            case GOD_ELYVILON:
                // Killing is only disapproved of during prayer.
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
                // Holy gods are easier to please this way.
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
                // Holy gods are easier to please this way.
                if (random2(level + 18 - (is_good_god(you.religion) ? 0 :
                                          you.experience_level / 2)) > 3)
                {
                    piety_change = 1;
                }
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

        case DID_KILL_FAST:
            if (you.religion == GOD_CHEIBRIADOS
                && !god_hates_attacking_friend(you.religion, victim))
            {
                simple_god_message(" appreciates the change of pace.");
                retval = true;
                if (random2(level+18) > 3)
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
                if (!testbits(victim->flags, MF_CREATED_FRIENDLY)
                    && !testbits(victim->flags, MF_WAS_NEUTRAL))
                {
                    break;
                }

                penance = level * 3;
                piety_change = -level * 3;
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
            case GOD_KIKUBAAQUDGHA:
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
                if (!testbits(victim->flags, MF_CREATED_FRIENDLY)
                    && !testbits(victim->flags, MF_WAS_NEUTRAL))
                {
                    break;
                }

                penance = level * 3;
                piety_change = -level * 3;
                retval = true;
                break;

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

                if (thing_done == DID_CAUSE_GLOWING)
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

        case DID_DESECRATE_ORCISH_REMAINS:
            if (you.religion == GOD_BEOGH)
            {
                piety_change = -level;
                retval = true;
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

        case DID_HASTY:
            if (you.religion == GOD_CHEIBRIADOS)
            {
                if (!known)
                {
                    simple_god_message(" forgives your accidental hurry, just this once.");
                    break;
                }
                simple_god_message(" thinks you should slow down.");
                piety_change = -level;
                if (level > 5)
                    penance = level - 5;
                retval = true;
            }
            break;

        case DID_NOTHING:
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
                "Kill Wizard", "Kill Priest", "Kill Holy", "Kill Fast",
                "Undead Slave Kill Living", "Servant Kill Living",
                "Undead Slave Kill Undead", "Servant Kill Undead",
                "Undead Slave Kill Demon", "Servant Kill Demon",
                "Servant Kill Natural Evil", "Undead Slave Kill Holy",
                "Servant Kill Holy", "Spell Memorise", "Spell Cast",
                "Spell Practise", "Spell Nonutility", "Cards", "Stimulants",
                "Drink Blood", "Cannibalism", "Eat Meat", "Eat Souled Being",
                "Deliberate Mutation", "Cause Glowing", "Use Chaos",
                "Desecrate Orcish Remains", "Destroy Orcish Idol",
                "Create Life", "Kill Slime", "Kill Plant", "Ally Kill Plant",
                "Was Hasty"
            };

            COMPILE_CHECK(ARRAYSZ(conducts) == NUM_CONDUCTS, c1);
            mprf(MSGCH_DIAGNOSTICS,
                 "conduct: %s; piety: %d (%+d); penance: %d (%+d)",
                 conducts[thing_done],
                 you.piety, piety_change, you.penance[you.religion], penance);

        }
#endif
    }

    do_god_revenge(thing_done);

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

    if (mon->friendly())
    {
        if ((mon->flags & NEW_GIFT_FLAGS) == NEW_GIFT_FLAGS
            && mon->god != GOD_XOM)
        {
            mprf(MSGCH_ERROR, "Newly created friendly god gift '%s' was hurt "
                 "by you, shouldn't be possible; please file a bug report.",
                 mon->name(DESC_PLAIN, true).c_str());
            _first_attack_was_friendly[midx] = true;
        }
        else if (_first_attack_conduct[midx]
                 || _first_attack_was_friendly[midx])
        {
            conduct[0].set(DID_ATTACK_FRIEND, 5, known, mon);
            _first_attack_was_friendly[midx] = true;
        }
    }
    else if (mon->neutral())
        conduct[0].set(DID_ATTACK_NEUTRAL, 5, known, mon);

    if (is_unchivalric_attack(&you, mon)
        && (_first_attack_conduct[midx]
            || _first_attack_was_unchivalric[midx]))
    {
        conduct[1].set(DID_UNCHIVALRIC_ATTACK, 4, known, mon);
        _first_attack_was_unchivalric[midx] = true;
    }

    if (mon->holiness() == MH_HOLY)
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

static bool _abil_chg_message(const char *pmsg, const char *youcanmsg)
{
    int pos;

    if (!*pmsg)
        return false;

    std::string pm = pmsg;
    if ((pos = pm.find("{biology}")) != -1)
        switch(you.is_undead)
        {
        case US_UNDEAD:      // mummies -- time has no meaning!
            return false;
        case US_HUNGRY_DEAD: // ghouls
            pm.replace(pos, 9, "decay");
            break;
        case US_SEMI_UNDEAD: // vampires
        case US_ALIVE:
            pm.replace(pos, 9, "biology");
            break;
        }

    if (isupper(pmsg[0]))
        god_speaks(you.religion, pm.c_str());
    else
    {
        god_speaks(you.religion,
                   make_stringf(youcanmsg, pmsg).c_str());
    }
    return true;
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

            // In case the best skill is Invocations, redraw the god
            // title.
            redraw_skill(you.your_name, player_title());

            if (_abil_chg_message(god_gain_power_messages[you.religion][i],
                              "You can now %s."))
            {
                learned_something_new(TUT_NEW_ABILITY_GOD);
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

    _do_god_gift(false);
}

// Is the destroyed weapon valuable enough to gain piety by doing so?
// Evil weapons are handled specially.
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

        // item_value() multiplies by quantity.
        const int value = item_value(item, true) / item.quantity;
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Destroyed weapon value: %d", value);
#endif

        piety_gain_t pgain = PIETY_NONE;
        const bool is_evil_weapon = is_evil_item(item);

        if (is_evil_weapon || _destroyed_valuable_weapon(value, item.base_type))
            pgain = PIETY_SOME;

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
            _print_sacrifice_message(GOD_ELYVILON, item, pgain);
            if (is_evil_weapon)
            {
                // Print this is addition to the above!
                simple_god_message(
                    make_stringf(" welcomes the destruction of %s evil "
                                 "weapon%s.",
                                 item.quantity == 1 ? "this" : "these",
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
            && !you.num_gifts[you.religion])
        {
            // In case the best skill is Invocations, redraw the god
            // title.
            redraw_skill(you.your_name, player_title());

            if (you.religion == GOD_ZIN)
                simple_god_message(" is no longer ready to cure all your mutations.");
            else if (you.religion == GOD_SHINING_ONE)
                simple_god_message(" is no longer ready to bless your weapon.");
            else if (you.religion == GOD_KIKUBAAQUDGHA)
                simple_god_message(" is no longer ready to enhance your necromancy.");
            else if (you.religion == GOD_LUGONU)
                simple_god_message(" is no longer ready to corrupt your weapon.");
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
    learned_something_new(TUT_EXCOMMUNICATE,
                          coord_def((int)new_god, old_piety));
}

static bool _bless_weapon(god_type god, brand_type brand, int colour)
{
    item_def& wpn = *you.weapon();

    // Only bless non-artefact melee weapons.
    if (is_artefact(wpn) || is_range_weapon(wpn))
        return (false);

    std::string prompt = "Do you wish to have your weapon ";
    if (brand == SPWPN_PAIN)
        prompt += "bloodied with pain";
    else if (brand == SPWPN_DISTORTION)
        prompt += "corrupted";
    else
        prompt += "blessed";
    prompt += "?";

    if (!yesno(prompt.c_str(), true, 'n'))
        return (false);

    you.duration[DUR_WEAPON_BRAND] = 0;     // just in case

    std::string old_name = wpn.name(DESC_NOCAP_A);
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
            wpn.flags |= ISFLAG_BLESSED_BLADE;
        }

        burden_change();
    }
    else if (is_evil_god(god))
    {
        convert2bad(wpn);

        burden_change();
    }

    you.wield_change = true;
    you.num_gifts[god]++;
    std::string desc  = old_name + " ";
            desc += (god == GOD_SHINING_ONE   ? "blessed by the Shining One" :
                     god == GOD_LUGONU        ? "corrupted by Lugonu" :
                     god == GOD_KIKUBAAQUDGHA ? "bloodied by Kikubaaqudgha"
                                              : "touched by the gods");

    take_note(Note(NOTE_ID_ITEM, 0, 0,
              wpn.name(DESC_NOCAP_A).c_str(), desc.c_str()));
    wpn.flags |= ISFLAG_NOTED_ID;

    mpr("Your weapon shines brightly!", MSGCH_GOD);

    you.flash_colour = colour;
    viewwindow(true, false);

    simple_god_message(" booms: Use this gift wisely!");

    if (god == GOD_SHINING_ONE)
    {
        holy_word(100, HOLY_WORD_TSO, you.pos(), true);

        // Un-bloodify surrounding squares.
        for (radius_iterator ri(you.pos(), 3, true, true); ri; ++ri)
            if (is_bloodcovered(*ri))
                env.pgrid(*ri) &= ~(FPROP_BLOODY);
    }

    if (god == GOD_KIKUBAAQUDGHA)
    {
        torment(TORMENT_KIKUBAAQUDGHA, you.pos());

        // Bloodify surrounding squares (75% chance).
        for (radius_iterator ri(you.pos(), 2, true, true); ri; ++ri)
            if (!is_bloodcovered(*ri) && !one_chance_in(4))
                env.pgrid(*ri) |= FPROP_BLOODY;
    }

#ifndef USE_TILE
    // Allow extra time for the flash to linger.
    delay(1000);
#endif

    return (true);
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

static void _print_sacrifice_message(god_type god, const item_def &item,
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

    formatted_message_history(msg, MSGCH_GOD);
}

static bool _altar_prayer()
{
    // Different message from when first joining a religion.
    mpr("You prostrate yourself in front of the altar and pray.");

    if (you.religion == GOD_XOM)
        return (false);

    god_acting gdact;

    bool did_bless = false;

    // TSO blesses weapons with holy wrath, and long blades specially.
    if (you.religion == GOD_SHINING_ONE
        && !you.num_gifts[GOD_SHINING_ONE]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn
            && (get_weapon_brand(*wpn) != SPWPN_HOLY_WRATH
            || is_blessed_blade_convertible(*wpn)))
        {
            did_bless = _bless_weapon(GOD_SHINING_ONE, SPWPN_HOLY_WRATH,
                                      YELLOW);
        }
    }

    // Lugonu blesses weapons with distortion.
    if (you.religion == GOD_LUGONU
        && !you.num_gifts[GOD_LUGONU]
        && !player_under_penance()
        && you.piety > 160)
    {
        item_def *wpn = you.weapon();

        if (wpn && get_weapon_brand(*wpn) != SPWPN_DISTORTION)
            did_bless = _bless_weapon(GOD_LUGONU, SPWPN_DISTORTION, MAGENTA);
    }

    // Kikubaaqudgha blesses weapons with pain, or gives you a Necronomicon.
    if (you.religion == GOD_KIKUBAAQUDGHA
        && !you.num_gifts[GOD_KIKUBAAQUDGHA]
        && !player_under_penance()
        && you.piety > 160)
    {
        simple_god_message(
            " will bloody your weapon with pain or grant you the Necronomicon.");

        bool kiku_did_bless_weapon = false;

        item_def *wpn = you.weapon();

        // Does the player want a pain branding?
        if (wpn && get_weapon_brand(*wpn) != SPWPN_PAIN)
        {
            kiku_did_bless_weapon =
                _bless_weapon(GOD_KIKUBAAQUDGHA, SPWPN_PAIN, RED);
            did_bless = kiku_did_bless_weapon;
        }
        else
            mpr("You have no weapon to bloody with pain.");

        // If not, ask if the player wants a Necronomicon.
        if (!kiku_did_bless_weapon)
        {
            if (!yesno("Do you wish to receive the Necronomicon?", true, 'n'))
                return (false);

            int thing_created = items(1, OBJ_BOOKS, BOOK_NECRONOMICON, true, 1,
                                      MAKE_ITEM_RANDOM_RACE,
                                      0, 0, you.religion);

            if (thing_created == NON_ITEM)
                return (false);

            move_item_to_grid(&thing_created, you.pos());

            if (thing_created != NON_ITEM)
            {
                simple_god_message(" grants you a gift!");
                more();

                you.num_gifts[you.religion]++;
                did_bless = true;
                take_note(Note(NOTE_GOD_GIFT, you.religion));
                mitm[thing_created].inscription = "god gift";
            }
        }

        // Return early so we don't offer our Necronomicon to Kiku.
        return (did_bless);
    }

    offer_items();

    return (did_bless);
}

bool god_hates_attacking_friend(god_type god, const actor *fr)
{
    if (!fr || fr->kill_alignment() != KC_FRIENDLY)
        return (false);

    return (god_hates_attacking_friend(god, fr->mons_species()));
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

static bool _god_likes_item(god_type god, const item_def& item)
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
        return (is_evil_item(item));

    case GOD_BEOGH:
        return (item.base_type == OBJ_CORPSES
                   && mons_species(item.plus) == MONS_ORC);

    case GOD_NEMELEX_XOBEH:
        return (!is_deck(item));

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
// out of 20 arrows, etc.).  Does not modify the actual item in any way.
static piety_gain_t _sacrifice_one_item_noncount(const item_def& item)
{
    piety_gain_t relative_piety_gain = PIETY_NONE;

    if (god_likes_fresh_corpses(you.religion))
    {
        if (x_chance_in_y(13, 19))
        {
            gain_piety(1);
            relative_piety_gain = PIETY_SOME;
        }
    }
    else
    {
        switch (you.religion)
        {
        case GOD_SHINING_ONE:
            gain_piety(1);
            relative_piety_gain = PIETY_SOME;
            break;

        case GOD_BEOGH:
        {
            const int item_orig = item.orig_monnum - 1;

            int chance = 4;

            if (item_orig == MONS_SAINT_ROKA)
                chance += 12;
            else if (item_orig == MONS_ORC_HIGH_PRIEST)
                chance += 8;
            else if (item_orig == MONS_ORC_PRIEST)
                chance += 4;

            if (food_is_rotten(item))
                chance--;
            else if (item.sub_type == CORPSE_SKELETON)
                chance -= 2;

            if (x_chance_in_y(chance, 20))
            {
                gain_piety(1);
                relative_piety_gain = PIETY_SOME;
            }
            break;
        }

        case GOD_NEMELEX_XOBEH:
        {
            // item_value() multiplies by quantity.
            const int value = item_value(item) / item.quantity;

            if (you.attribute[ATTR_CARD_COUNTDOWN] && x_chance_in_y(value, 800))
            {
                you.attribute[ATTR_CARD_COUNTDOWN]--;
#if DEBUG_DIAGNOSTICS || DEBUG_CARDS || DEBUG_SACRIFICE
                mprf(MSGCH_DIAGNOSTICS, "Countdown down to %d",
                     you.attribute[ATTR_CARD_COUNTDOWN]);
#endif
            }
            // Nemelex piety gain is fairly fast... at least when you
            // have low piety.
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
                // Count chunks and blood potions towards decks of
                // Summoning.
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
        }

        default:
            break;
        }
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

        if (!yesno("Do you wish to donate half of your money?", true, 'n'))
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

        you.del_gold(donation_cost);

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
        if (god_likes_fresh_corpses(you.religion))
            simple_god_message(" only cares about fresh corpses!");
        else if (you.religion == GOD_SHINING_ONE)
            simple_god_message(" only cares about evil items!");
        else if (you.religion == GOD_BEOGH)
            simple_god_message(" only cares about orcish remains!");
        else if (you.religion == GOD_NEMELEX_XOBEH)
            simple_god_message(" expects you to use your decks, not offer them!");
    }
}

bool player_can_join_god(god_type which_god)
{
    if (you.species == SP_DEMIGOD)
        return (false);

    if (is_good_god(which_god) && you.is_unholy())
        return (false);

    if (which_god == GOD_ZIN && you.is_chaotic())
        return (false);

    if (which_god == GOD_BEOGH && you.species != SP_HILL_ORC)
        return (false);

    // Fedhas hates undead, but will accept demonspawn.
    if (which_god == GOD_FEDHAS && you.holiness() == MH_UNDEAD)
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
        divine_retribution(GOD_LUGONU, true);
        return;
    }

    describe_god(which_god, false);

    snprintf(info, INFO_SIZE, "Do you wish to %sjoin this religion?",
             (you.worshipped[which_god]) ? "re" : "");

    if (!yesno(info, false, 'n') || !yesno("Are you sure?", false, 'n'))
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

    // When you start worshipping a good god, you make all non-hostile
    // evil and unholy beings hostile; when you start worshipping Zin,
    // you make all non-hostile chaotic beings hostile; and when you
    // start worshipping Trog, you make all non-hostile magic users
    // hostile.
    if (is_good_god(you.religion) && evil_beings_attitude_change())
        mpr("Your evil allies forsake you.", MSGCH_MONSTER_ENCHANT);

    if (you.religion == GOD_ZIN && chaotic_beings_attitude_change())
        mpr("Your chaotic allies forsake you.", MSGCH_MONSTER_ENCHANT);
    else if (you.religion == GOD_TROG && magic_users_attitude_change())
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

    // Complimentary jelly upon joining.
    if (you.religion == GOD_JIYVA)
    {
        if (you.worshipped[GOD_JIYVA] == 1)
            slime_vault_change(false);

        if (!_has_jelly())
        {
            monster_type mon = MONS_JELLY;
            mgen_data mg(mon, BEH_STRICT_NEUTRAL, 0, 0, you.pos(), MHITNOT, 0,
                         GOD_JIYVA);

            _delayed_monster(mg);
            simple_god_message(" grants you a jelly!");
        }
    }

    // Refresh wielded/quivered weapons in case we have a new conduct
    // on them.
    you.wield_change = true;
    you.redraw_quiver = true;

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

    if (god == GOD_FEDHAS)
        retval = (fedhas_protects(mon));

    return (retval);
}

bool god_likes_fresh_corpses(god_type god)
{
    return (god == GOD_OKAWARU
            || god == GOD_MAKHLEB
            || god == GOD_TROG
            || god == GOD_LUGONU);
}

bool god_likes_butchery(god_type god)
{
    return (god == GOD_KIKUBAAQUDGHA && you.piety >= piety_breakpoint(4));
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

void offer_and_butcher_corpse(int corpse)
{
    // We always give the "good" (piety-gain) message when doing
    // dedicated butchery.  Uh, call it a feature.
    _print_sacrifice_message(you.religion, mitm[corpse], PIETY_SOME);

    did_god_conduct(DID_DEDICATED_BUTCHERY, 10);

    // ritual sacrifice can also bloodify the ground
    const monster_type mons_class = static_cast<monster_type>(mitm[corpse].plus);
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
            return;

        // These gods like long-standing worshippers.
        case GOD_ELYVILON:
            if (_need_free_piety() && one_chance_in(20))
                gain_piety(1);
            return;

        case GOD_CHEIBRIADOS:
            if (you.hunger_state < HS_FULL)
                return;
            if (_need_free_piety() && one_chance_in(12))
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
            || mon->is_evil()
            || holiness != MH_NATURAL && holiness != MH_HOLY);
}

int get_tension(god_type god, bool count_travelling)
{
    ASSERT(god != GOD_NO_GOD);

    int total = 0;

    bool nearby_monster = false;
    for (int midx = 0; midx < MAX_MONSTERS; ++midx)
    {
        const monsters* mons = &menv[midx];

        if (!mons->alive())
            continue;

        if (you.see_cell(mons->pos()))
        {
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

        const mon_attitude_type att = mons_attitude(mons);
        if (att == ATT_GOOD_NEUTRAL || att == ATT_NEUTRAL)
            continue;

        if (mons->cannot_act() || mons->asleep() || mons_is_fleeing(mons))
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
            if (!you.visible_to(mons))
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
