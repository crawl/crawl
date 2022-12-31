/**
 * @file
 * @brief Zot clock functions.
**/

#include "AppHdr.h"

#include "zot.h"

#include "activity-interrupt-type.h" // for zot clock key
#include "branch.h" // for zot clock key
#include "database.h" // getSpeakString
#include "delay.h" // interrupt_activity
#include "god-passive.h"
#include "message.h"
#include "notes.h" // NOTE_MESSAGE
#include "state.h"

#if TAG_MAJOR_VERSION == 34
static int _old_zot_clock(const string& branch_name) {
    // The old clock was measured in turns (deca-auts), not aut.
    static const string OLD_KEY = "ZOT_CLOCK";
    if (!you.props.exists(OLD_KEY))
        return -1;
    CrawlHashTable &branch_clock = you.props[OLD_KEY];
    if (!branch_clock.exists(branch_name))
        return -1;
    return branch_clock[branch_name].get_int();
}
#endif

// Returns -1 if the player hasn't been in this branch before.
static int& _zot_clock_for(branch_type br)
{
    CrawlHashTable &branch_clock = you.props["ZOT_AUTS"];
    const string branch_name = is_hell_branch(br) ? "Hells" : branches[br].abbrevname;
    // When entering a new branch, start with an empty clock.
    // (You'll get the usual time when you finish entering.)
    if (!branch_clock.exists(branch_name))
    {
#if TAG_MAJOR_VERSION == 34
        // The old clock was measured in turns (deca-auts), not aut.
        const int old_clock = _old_zot_clock(branch_name);
        if (old_clock != -1)
            branch_clock[branch_name].get_int() = old_clock;
        else
#endif
            branch_clock[branch_name].get_int() = -1;
    }
    return branch_clock[branch_name].get_int();
}

static int& _zot_clock()
{
    return _zot_clock_for(you.where_are_you);
}

static bool _zot_clock_active_in(branch_type br)
{
    return br != BRANCH_ABYSS && !zot_immune() && !crawl_state.game_is_sprint();
}

// Is the zot clock running, or is it paused or stopped altogether?
bool zot_clock_active()
{
    return _zot_clock_active_in(you.where_are_you);
}

// Has the player stopped the zot clock?
bool zot_immune()
{
    return player_has_orb() || you.zigs_completed;
}

int turns_until_zot_in(branch_type br)
{
    const int aut = (MAX_ZOT_CLOCK - _zot_clock_for(br));
    if (have_passive(passive_t::slow_zot))
        return aut * 3 / (2 * BASELINE_DELAY);
    return aut / BASELINE_DELAY;
}

// How many turns (deca-auts) does the player have until Zot finds them?
int turns_until_zot()
{
    return turns_until_zot_in(you.where_are_you);
}

static int _zot_lifespan_div()
{
    return you.has_mutation(MUT_SHORT_LIFESPAN) ? 10 : 1;
}

// A scale from 0 to 3 of how much danger the player is in of
// reaching the end of the zot clock. 0 is no danger, 3 is almost dead.
int bezotting_level_in(branch_type br)
{
    if (!_zot_clock_active_in(br))
        return 0;

    const int remaining_turns = turns_until_zot_in(br) * _zot_lifespan_div();
    if (remaining_turns < 100)
        return 3;
    if (remaining_turns < 500)
        return 2;
    if (remaining_turns < 1000)
        return 1;
    return 0;
}

// A scale from 0 to 4 of how much danger the player is in of
// reaching the end of the zot clock in their current branch.
int bezotting_level()
{
    return bezotting_level_in(you.where_are_you);
}

// If the player was in the given branch, would they see warnings for
// nearing the end of the zot clock?
bool bezotted_in(branch_type br)
{
    return bezotting_level_in(br) > 0;
}

// Is the player seeing warnings about nearing the end of the zot clock?
bool bezotted()
{
    return bezotted_in(you.where_are_you);
}

// Decrease the zot clock when the player enters a new level.
void decr_zot_clock(bool extra_life)
{
    if (!zot_clock_active())
        return;
    int &zot = _zot_clock();

    const int div = _zot_lifespan_div();
    if (zot == -1)
    {
        // new branch
        zot = MAX_ZOT_CLOCK - ZOT_CLOCK_PER_FLOOR / div;
    }
    else
    {
        // old branch, new floor
        if (bezotted())
        {
            if (extra_life)
                mpr("As you die, Zot loses track of you.");
            else
                mpr("As you enter the new level, Zot loses track of you.");
        }
        zot = max(0, zot - ZOT_CLOCK_PER_FLOOR / div);
    }
    if (you.species == SP_METEORAN)
        update_vision_range();
}

static int _added_zot_time()
{
    if (have_passive(passive_t::slow_zot))
        return div_rand_round(you.time_taken * 2, 3);
    return you.time_taken;
}

void incr_zot_clock()
{
    if (!zot_clock_active())
        return;

    const int old_lvl = bezotting_level();
    _zot_clock() += _added_zot_time();
    if (!bezotted())
        return;

    if (_zot_clock() >= MAX_ZOT_CLOCK)
    {
        mpr("Zot's power touches on you...");
        // Take the note before decrementing max HP, so the notes have the
        // cause before the effect.
        take_note(Note(NOTE_MESSAGE, 0, 0, "Touched by the power of Zot."));
        dec_max_hp(min(3 + you.hp_max / 6, you.hp_max - 1));
        interrupt_activity(activity_interrupt::force);

        set_turns_until_zot(you.has_mutation(MUT_SHORT_LIFESPAN) ? 200 : 1000);
    }

    const int lvl = bezotting_level();
    if (old_lvl >= lvl)
        return;

    switch (lvl)
    {
        case 1:
            mprf("You have lingered too long. Zot senses you. Dive deeper or flee this branch before you suffer!");
            break;
        case 2:
            mpr("Zot draws nearer. Dive deeper or flee this branch before you suffer!");
            break;
        case 3:
            mprf("Zot has nearly found you. Suffering is imminent. Descend or flee this branch!");
            break;
    }

    if (you.species == SP_METEORAN)
        update_vision_range();

    take_note(Note(NOTE_MESSAGE, 0, 0, "Glimpsed the power of Zot."));
    interrupt_activity(activity_interrupt::force);
}

void set_turns_until_zot(int turns_left)
{
    if (turns_left < 0 || turns_left > MAX_ZOT_CLOCK / BASELINE_DELAY)
        return;

    int &clock = _zot_clock();
    clock = MAX_ZOT_CLOCK - turns_left * BASELINE_DELAY;
    if (you.species == SP_METEORAN)
        update_vision_range();
}
