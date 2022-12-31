#include "AppHdr.h"

#include "god-prayer.h"

#include <cmath>

#include "artefact.h"
#include "database.h"
#include "describe-god.h"
#include "env.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-passive.h"
#include "hiscores.h"
#include "invent.h"
#include "item-use.h"
#include "makeitem.h"
#include "message.h"
#include "notes.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#include "unwind.h"
#include "xom.h"

string god_prayer_reaction()
{
    string result = uppercase_first(god_name(you.religion));
    const int rank = god_favour_rank(you.religion);
    if (crawl_state.player_is_dead())
        result += " was ";
    else
        result += " is ";
    result +=
        (rank == 7) ? "exalted by your worship" :
        (rank == 6) ? "extremely pleased with you" :
        (rank == 5) ? "greatly pleased with you" :
        (rank == 4) ? "most pleased with you" :
        (rank == 3) ? "pleased with you" :
        (rank == 2) ? "aware of your devotion"
                    : "noncommittal";
    result += ".";

    return result;
}

static bool _prompt_ecu_worship(const vector<god_type> &gods)
{
    vector<string> god_name_list;
    for (size_t i = 0; i < gods.size(); ++i)
    {
        god_name_list.push_back(make_stringf("(%c) %s", char('a' + i),
                                             god_name(gods[i]).c_str()));
    }
    const string god_names = comma_separated_line(god_name_list.begin(),
                                                  god_name_list.end(), " or ");
    // This should be a proper menu, but I absolutely cannot be bothered with
    // webtiles menu code.
    mprf(MSGCH_PROMPT, "This altar belongs to %s, but you can't tell which.\n"
         "Press the corresponding letter to learn more about a god, "
         "or press enter to convert or escape to cancel.", god_names.c_str());
    while (true) {
        flush_prev_message();
        int ch = getch_ck();
        switch (ch)
        {
        case CK_ENTER:
        case 'y':
        case 'Y':
            return true;
        case 'n':
        case 'N':
        case CK_ESCAPE:
        case CK_REDRAW:
            return false;
        default:
            break;
        }
        int god_idx = ch - 'a';
        if (god_idx >= 0 && god_idx < int(gods.size()))
            describe_god(gods[god_idx]);
    }
}

static bool _pray_ecumenical_altar()
{
    const vector<god_type> gods = get_ecu_gods(you.pos());
    if (!_prompt_ecu_worship(gods))
    {
        canned_msg(MSG_OK);
        return false;
    }
    if (!you_worship(GOD_NO_GOD)
        && !yesno(make_stringf("Really abandon %s for an unknown god?",
                               god_name(you.religion).c_str()).c_str(),
                               false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    const god_type altar_god = *random_iterator(gods);
    dungeon_terrain_changed(you.pos(), altar_for_god(altar_god));
    take_note(Note(NOTE_MESSAGE, 0, 0,
                   "Prayed at the altar of an unknown god."));
    you.turn_is_over = true;
    {
        // Don't check for or charge a Gozag service fee.
        unwind_var<int> fakepoor(you.attribute[ATTR_GOLD_GENERATED], 0);
        if (!player_can_join_god(altar_god))
        {
            print_god_rejection(altar_god);
            return true;
        }

        mprf(MSGCH_GOD, "%s accepts your prayer!",
                        god_name(altar_god).c_str());
        if (you_worship(altar_god))
            return true;
        join_religion(altar_god);
    }

    if (you_worship(GOD_RU))
        you.props[RU_SACRIFICE_PROGRESS_KEY] = 9999;
    else if (you_worship(GOD_ASHENZARI))
        you.props[ASHENZARI_CURSE_PROGRESS_KEY] = 9999;
    else if (you_worship(GOD_XOM))
        xom_is_stimulated(200, XM_INTRIGUED, true);
    else if (you_worship(GOD_YREDELEMNUL))
        give_yred_bonus_zombies(1);
    else
        gain_piety(20, 1, false);

    mark_milestone("god.ecumenical", "prayed at an ecumenical altar.");
    return true;
}

/**
 * Attempt to convert to the given god.
 *
 * @return True if the conversion happened, false otherwise.
 */
void try_god_conversion(god_type god)
{
    ASSERT(god != GOD_NO_GOD);

    if (you.has_mutation(MUT_FORLORN))
    {
        mpr("A being of your status worships no god.");
        return;
    }

    if (god == GOD_ECUMENICAL)
    {
        _pray_ecumenical_altar();
        return;
    }

    if (you_worship(GOD_NO_GOD) || god != you.religion)
    {
        // consider conversion
        you.turn_is_over = true;
        // But if we don't convert then god_pitch
        // makes it not take a turn after all.
        god_pitch(god);
    }
    else
    {
        // Already worshipping this god - just print a message.
        mprf(MSGCH_GOD, "You offer a %sprayer to %s.",
             you.cannot_speak() ? "silent " : "",
             god_name(god).c_str());
    }
}

int zin_tithe(const item_def& item, int quant, bool converting)
{
    if (item.tithe_state == TS_NO_TITHE)
        return 0;

    int taken = 0;
    int due = quant += you.attribute[ATTR_TITHE_BASE];
    if (due > 0)
    {
        int tithe = due / 10;
        due -= tithe * 10;
        // Those high enough in the hierarchy get to reap the benefits.
        // You're never big enough to be paid, the top is not having to pay
        // (and even that at 200 piety, for a brief moment until it decays).
        tithe = min(tithe,
                    (you.penance[GOD_ZIN] + MAX_PIETY - you.piety) * 2 / 3);
        if (tithe <= 0)
        {
            // update the remainder anyway
            you.attribute[ATTR_TITHE_BASE] = due;
            return 0;
        }
        taken = tithe;
        you.attribute[ATTR_DONATIONS] += tithe;
        mprf("You pay a tithe of %d gold.", tithe);

        if (item.tithe_state == TS_NO_PIETY) // seen before worshipping Zin
        {
            tithe = 0;
            simple_god_message(" ignores your late donation.");
        }
        // A single scroll can give you more than D:1-18, Lair and Orc
        // together, limit the gains. You're still required to pay from
        // your sudden fortune, yet it's considered your duty to the Church
        // so piety is capped. If you want more piety, donate more!
        //
        // Note that the stepdown is not applied to other gains: it would
        // be simpler, yet when a monster combines a number of gold piles
        // you shouldn't be penalized.
        int denom = 2;
        if (item.props.exists(ACQUIRE_KEY)) // including "acquire any" in vaults
        {
            tithe = stepdown_value(tithe, 10, 10, 50, 50);
            dprf("Gold was acquired, reducing gains to %d.", tithe);
        }
        else
        {
            if (player_in_branch(BRANCH_ORC) && !converting)
            {
                // Another special case: Orc gives simply too much compared to
                // other branches.
                denom *= 2;
            }
            // Avg gold pile value: 10 + depth/2.
            tithe *= 47;
            denom *= 20 + env.absdepth0;
        }
        gain_piety(tithe * 3, denom);
    }
    you.attribute[ATTR_TITHE_BASE] = due;
    return taken;
}
