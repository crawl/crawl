/**
 * @file
 * @brief Functions for gods blessing followers.
 **/

#include "AppHdr.h"

#include "god-blessing.h"

#include "env.h"
#include "message.h"
#include "mon-place.h"
#include "religion.h"
#include "stringutil.h"
#include "view.h"

static bool _blessing_balms(monster* mon)
{
    // Remove poisoning, sickness and confusion, like a potion of
    // curing, but without the healing. Also, remove slowing and
    // fatigue.
    bool success = false;

    if (mon->del_ench(ENCH_POISON, true))
        success = true;

    if (mon->del_ench(ENCH_SICK, true))
        success = true;

    if (mon->del_ench(ENCH_CONFUSION, true))
        success = true;

    if (mon->del_ench(ENCH_SLOW, true))
        success = true;

    if (mon->del_ench(ENCH_FATIGUE, true))
        success = true;

    if (mon->del_ench(ENCH_WRETCHED, true))
        success = true;

    return success;
}

static bool _blessing_healing(monster* mon)
{
    const int healing = mon->max_hit_points / 4 + 1;

    // Heal a monster.
    if (mon->heal(healing + random2(healing + 1)))
        return true;

    return false;
}

static bool _increase_ench_duration(monster* mon,
                                    mon_enchant ench,
                                    const int increase)
{
    // Durations are saved as 16-bit signed ints, so clamp at the largest such.
    const int MARSHALL_MAX = (1 << 15) - 1;

    const int newdur = min(ench.duration + increase, MARSHALL_MAX);
    if (ench.duration >= newdur)
        return false;

    ench.duration = newdur;
    mon->update_ench(ench);

    return true;
}

static bool _tso_blessing_extend_stay(monster* mon)
{
    if (!mon->has_ench(ENCH_SUMMON_TIMER))
        return false;

    mon_enchant timer = mon->get_ench(ENCH_SUMMON_TIMER);

    // These numbers are tenths of a player turn. Holy monsters get a
    // much bigger boost than random beasties, which get at most double
    // their current summon duration.
    if (mon->is_holy())
        return _increase_ench_duration(mon, timer, 1100 + random2(1100));
    else
        return _increase_ench_duration(mon, timer, min(timer.duration,
                                                       500 + random2(500)));
}

static bool _tso_blessing_friendliness(monster* mon)
{
    if (!mon->has_ench(ENCH_CHARM))
        return false;

    // [ds] Just increase charm duration, no permanent friendliness.
    const int base_increase = 700;
    return _increase_ench_duration(mon, mon->get_ench(ENCH_CHARM),
                                   base_increase + random2(base_increase));
}

/**
 * Attempt to bless a follower with curing and/or healing.
 *
 * @param[in] follower      The follower to heal.
 * @return                  The type of healing that occurred; may be empty.
 */
static string _bless_with_healing(monster* follower)
{
    string blessing = "";

    // Maybe try to cure status conditions.
    bool balms = false;
    if (coinflip())
    {
        balms = _blessing_balms(follower);
        if (balms)
            blessing = "divine balms";
        else
            dprf("Couldn't apply balms.");
    }

    // Heal the follower.
    bool healing = _blessing_healing(follower);

    // Maybe heal the follower again.
    if ((!healing || coinflip()) && _blessing_healing(follower))
        healing = true;

    if (healing)
    {
        if (balms)
            blessing += " and ";
        blessing += "healing";
    }
    else
        dprf("Couldn't heal monster.");

    return blessing;
}


/**
 * Print a message for a god blessing a follower.
 *
 * Also flash the screen, in local tiles.
 *
 * @param[in] follower  The follower being blessed.
 * @param god           The god doing the blessing.
 * @param blessing      The blessing being delivered.
 */
static void _display_god_blessing(monster* follower, god_type god,
                                  string blessing)
{
    ASSERT(follower); // XXX: change to monster &follower

    string whom = you.can_see(*follower) ? follower->name(DESC_THE)
    : "a follower";

    simple_god_message(make_stringf(" blesses %s with %s.",
                                    whom.c_str(), blessing.c_str()).c_str(),
                       god);
}

/**
 * Attempt to increase the duration of a follower.
 *
 * Their summon duration, if summoned, or charm duration, if charmed.
 *
 * @param[in] follower    The follower to bless.
 * @return                The type of blessing that was given; may be empty.
 */
static string _tso_bless_duration(monster* follower)
{
    // Extend a monster's stay if it's abjurable, or extend charm
    // duration. If neither is possible, deliberately fall through.
    const bool more_time = _tso_blessing_extend_stay(follower);
    const bool friendliness = _tso_blessing_friendliness(follower);

    if (!more_time && !friendliness)
    {
        dprf("Couldn't increase monster's friendliness or summon time.");
        return "";
    }

    string blessing = "";
    if (friendliness)
    {
        blessing += "friendliness";
        if (more_time)
            blessing += " and ";
    }

    if (more_time)
        blessing += "more time in this world";

    return blessing;
}

/**
 * Have The Shining One attempt to bless the specified follower.
 *
 * Blessings can increase duration, or heal & cure.
 *
 * @param[in] follower      The follower to try to bless.
 * @param[in] force         Whether to check follower validity.
 * @return Whether a blessing occurred.
 */
static bool _tso_bless_follower(monster* follower, bool force)
{

    if (!follower || (!force && !is_follower(*follower)))
        return false;

    string blessing = "";
    if (blessing.empty() && coinflip())
        blessing = _bless_with_healing(follower);
    if (blessing.empty())
        blessing = _tso_bless_duration(follower);

    if (blessing.empty())
        return false;

    _display_god_blessing(follower, GOD_SHINING_ONE, blessing);
    return true;
}

static bool _is_friendly_follower(const monster& mon)
{
    return mon.friendly() && is_follower(mon);
}

// Bless the follower indicated in follower, if any. If there isn't
// one, bless a random follower within sight of the player, if any, or
// any follower on the level.
// Blessing can be enforced with a wizard mode command.
bool bless_follower(monster* follower,
                    god_type god,
                    bool force)
{
    // most blessings fail, tragically...
    if (!force && !one_chance_in(4))
        return false;

    // If a follower was specified, and it's suitable, pick it.
    // Otherwise, pick a random follower.
    // XXX: factor out into another function?
    if (!follower || (!force && !is_follower(*follower)))
    {
        // Choose a random follower in LOS
        follower = choose_random_nearby_monster(_is_friendly_follower);
    }

    // Try *again*, on the entire level
    if (!follower)
        follower = choose_random_monster_on_level(_is_friendly_follower);

    switch (god)
    {
        case GOD_SHINING_ONE:   return _tso_bless_follower(follower, force);
        default: return false; // XXX: print something here?
    }
}
