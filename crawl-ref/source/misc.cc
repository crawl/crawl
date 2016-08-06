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
#include "dgn-shoals.h"
#include "english.h"
#include "fight.h"
#include "fprop.h"
#include "godpassive.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "message.h"
#include "monster.h"
#include "ng-setup.h"
#include "prompt.h"
#include "religion.h"
#include "spl-clouds.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "tileview.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

string weird_glowing_colour()
{
    return getMiscString("glowing_colour_name");
}

string weird_writing()
{
    return getMiscString("writing_name");
}

string weird_smell()
{
    return getMiscString("smell_name");
}

string weird_sound()
{
    return getMiscString("sound_name");
}

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance, coord_def attack_pos,
                bool check_landing_only)
{
    ASSERT(mon); // XXX: change to const monster &mon
    ASSERT(!crawl_state.game_is_arena());
    bool bad_landing = false;

    if (!you.can_see(*mon))
        return false;

    if (attack_pos == coord_def(0, 0))
        attack_pos = you.pos();

    adj.clear();
    suffix.clear();
    would_cause_penance = false;

    if (!check_landing_only
        && (is_sanctuary(mon->pos()) || is_sanctuary(attack_pos)))
    {
        suffix = ", despite your sanctuary";
    }
    else if (check_landing_only && is_sanctuary(attack_pos))
    {
        suffix = ", when you might land in your sanctuary";
        bad_landing = true;
    }
    if (check_landing_only)
        return bad_landing;

    if (you_worship(GOD_JIYVA) && mons_is_slime(mon)
        && !(mon->is_shapeshifter() && (mon->flags & MF_KNOWN_SHIFTER)))
    {
        would_cause_penance = true;
        return true;
    }

    if (mon->friendly())
    {
        if (god_hates_attacking_friend(you.religion, mon))
        {
            adj = "your ally ";

            monster_info mi(mon, MILEV_NAME);
            if (!mi.is(MB_NAME_UNQUALIFIED))
                adj += "the ";

            would_cause_penance = true;

        }
        else
        {
            adj = "your ";

            monster_info mi(mon, MILEV_NAME);
            if (mi.is(MB_NAME_UNQUALIFIED))
                adj += "ally ";
        }

        return true;
    }

    if (find_stab_type(&you, *mon) != STAB_NO_STAB
        && you_worship(GOD_SHINING_ONE)
        && !tso_unchivalric_attack_safe_monster(mon))
    {
        adj += "helpless ";
        would_cause_penance = true;
    }

    if (mon->neutral() && is_good_god(you.religion))
    {
        adj += "neutral ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON))
            would_cause_penance = true;
    }
    else if (mon->wont_attack())
    {
        adj += "non-hostile ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON))
            would_cause_penance = true;
    }

    return !adj.empty() || !suffix.empty();
}

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool *prompted,
                        coord_def attack_pos, bool check_landing_only)
{
    ASSERT(mon); // XXX: change to const monster &mon
    bool penance = false;

    if (prompted)
        *prompted = false;

    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (you.confused() || !you.can_see(*mon))
        return false;

    string adj, suffix;
    if (!bad_attack(mon, adj, suffix, penance, attack_pos, check_landing_only))
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = mon->name(DESC_PLAIN);
    if (starts_with(mon_name, "the ")) // no "your the Royal Jelly" nor "the the RJ"
        mon_name = mon_name.substr(4); // strlen("the ")
    if (!starts_with(adj, "your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;
    string verb;
    if (beam_attack)
    {
        verb = "fire ";
        if (beam_target == mon->pos())
            verb += "at ";
        else
        {
            verb += "in " + apostrophise(mon_name) + " direction";
            mon_name = "";
        }
    }
    else
        verb = "attack ";

    const string prompt = make_stringf("Really %s%s%s?%s",
             verb.c_str(), mon_name.c_str(), suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(prompt.c_str(), false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

bool stop_attack_prompt(targetter &hitfunc, const char* verb,
                        bool (*affects)(const actor *victim), bool *prompted)
{
    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (crawl_state.which_god_acting() == GOD_XOM)
        return false;

    if (you.confused())
        return false;

    string adj, suffix;
    bool penance = false;
    counted_monster_list victims;
    for (distance_iterator di(hitfunc.origin, false, true, LOS_RADIUS); di; ++di)
    {
        if (hitfunc.is_affected(*di) <= AFF_NO)
            continue;
        const monster* mon = monster_at(*di);
        if (!mon || !you.can_see(*mon))
            continue;
        if (affects && !affects(mon))
            continue;
        string adjn, suffixn;
        bool penancen = false;
        if (bad_attack(mon, adjn, suffixn, penancen))
        {
            // record the adjectives for the first listed, or
            // first that would cause penance
            if (victims.empty() || penancen && !penance)
                adj = adjn, suffix = suffixn, penance = penancen;
            victims.add(mon);
        }
    }

    if (victims.empty())
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = victims.describe(DESC_PLAIN);
    if (starts_with(mon_name, "the ")) // no "your the Royal Jelly" nor "the the RJ"
        mon_name = mon_name.substr(4); // strlen("the ")
    if (!starts_with(adj, "your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;

    const string prompt = make_stringf("Really %s %s%s?%s",
             verb, mon_name.c_str(), suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(prompt.c_str(), false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

// Make the player swap positions with a given monster.
void swap_with_monster(monster* mon_to_swap)
{
    monster& mon(*mon_to_swap);
    ASSERT(mon.alive());
    const coord_def newpos = mon.pos();

    if (check_stasis())
        return;

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
            // Xom thinks this is hilarious if you trap yourself this way.
            if (you_caught)
                xom_is_stimulated(12);
            else
                xom_is_stimulated(200);
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
