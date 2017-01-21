/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "misc.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if !defined(__IBMCPP__) && !defined(TARGET_COMPILER_VC)
#include <unistd.h>
#endif

#include "coordit.h"
#include "database.h"
#include "english.h"
#include "fprop.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "message.h"
#include "monster.h"
#include "prompt.h"
#include "state.h"
#include "terrain.h"
#include "tileview.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

string weird_glowing_colour()
{
    return getMiscString("glowing_colour_name");
}

// Make the player swap positions with a given monster.
void swap_with_monster(monster* mon_to_swap)
{
    monster& mon(*mon_to_swap);
    ASSERT(mon.alive());
    const coord_def newpos = mon.pos();

    if (you.stasis())
    {
        mpr("Your stasis prevents you from teleporting.");
        return;
    }

    // Be nice: no swapping into uninhabitable environments.
    if (!you.is_habitable(newpos) || !mon.is_habitable(you.pos()))
    {
        mpr("You spin around.");
        return;
    }

    const bool mon_caught = mon.caught();
    const bool you_caught = you.attribute[ATTR_HELD];

    // If it was submerged, it surfaces first.
    mon.del_ench(ENCH_SUBMERGED);

    mprf("You swap places with %s.", mon.name(DESC_THE).c_str());

    mon.move_to_pos(you.pos(), true, true);

    if (you_caught)
    {
        check_net_will_hold_monster(&mon);
        if (!mon_caught)
        {
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
    }

    // Move you to its previous location.
    move_player_to_grid(newpos, false);

    if (mon_caught)
    {
        if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        {
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                destroy_item(net);
            mprf("The %s rips apart!", (net == NON_ITEM) ? "web" : "net");
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
        else
        {
            you.attribute[ATTR_HELD] = 1;
            if (get_trapping_net(you.pos()) != NON_ITEM)
                mpr("You become entangled in the net!");
            else
                mpr("You get stuck in the web!");
            you.redraw_quiver = true; // Account for being in a net.
        }

        if (!you_caught)
            mon.del_ench(ENCH_HELD, true);
    }
}

void handle_real_time(chrono::time_point<chrono::system_clock> now)
{
    const chrono::milliseconds elapsed =
     chrono::duration_cast<chrono::milliseconds>(now - you.last_keypress_time);
    you.real_time_delta = min<chrono::milliseconds>(
      elapsed,
      (chrono::milliseconds)(IDLE_TIME_CLAMP * 1000));
    you.real_time_ms += you.real_time_delta;
    you.last_keypress_time = now;
}

unsigned int breakpoint_rank(int val, const int breakpoints[],
                             unsigned int num_breakpoints)
{
    unsigned int result = 0;
    while (result < num_breakpoints && val >= breakpoints[result])
        ++result;

    return result;
}

void counted_monster_list::add(const monster* mons)
{
    const string name = mons->name(DESC_PLAIN);
    for (auto &entry : list)
    {
        if (entry.first->name(DESC_PLAIN) == name)
        {
            entry.second++;
            return;
        }
    }
    list.emplace_back(mons, 1);
}

int counted_monster_list::count()
{
    int nmons = 0;
    for (const auto &entry : list)
        nmons += entry.second;
    return nmons;
}

string counted_monster_list::describe(description_level_type desc,
                                      bool force_article)
{
    string out;

    for (auto i = list.begin(); i != list.end();)
    {
        const counted_monster &cm(*i);
        if (i != list.begin())
        {
            ++i;
            out += (i == list.end() ? " and " : ", ");
        }
        else
            ++i;

        out += cm.second > 1
               ? pluralise_monster(cm.first->name(desc, false, true))
               : cm.first->name(desc);
    }
    return out;
}

/**
 * Halloween or Hallowe'en (/ˌhæləˈwiːn, -oʊˈiːn, ˌhɑːl-/; a contraction of
 * "All Hallows' Evening"),[6] also known as Allhalloween,[7] All Hallows' Eve,
 * [8] or All Saints' Eve,[9] is a yearly celebration observed in a number of
 * countries on 31 October, the eve of the Western Christian feast of All
 * Hallows' Day... Within Allhallowtide, the traditional focus of All Hallows'
 * Eve revolves around the theme of using "humor and ridicule to confront the
 * power of death."[12]
 *
 * Typical festive Halloween activities include trick-or-treating (or the
 * related "guising"), attending costume parties, decorating, carving pumpkins
 * into jack-o'-lanterns, lighting bonfires, apple bobbing, visiting haunted
 * house attractions, playing pranks, telling scary stories, and watching
 * horror films.
 *
 * @return  Whether the current day is Halloween. (Cunning players may reset
 *          their system clocks to manipulate this. That's fine.)
 */
bool today_is_halloween()
{
    const time_t curr_time = time(nullptr);
    const struct tm *date = TIME_FN(&curr_time);
    // tm_mon is zero-based in case you are wondering
    return date->tm_mon == 9 && date->tm_mday == 31;
}

bool tobool(maybe_bool mb, bool def)
{
    switch (mb)
    {
    case MB_TRUE:
        return true;
    case MB_FALSE:
        return false;
    case MB_MAYBE:
    default:
        return def;
    }
}

maybe_bool frombool(bool b)
{
    return b ? MB_TRUE : MB_FALSE;
}
