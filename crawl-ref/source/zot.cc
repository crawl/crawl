/**
 * @file
 * @brief Zot clock functions.
**/

#include "AppHdr.h"

#include "zot.h"

#include "activity-interrupt-type.h" // for zot clock key
#include "branch.h" // for zot clock key
#include "coordit.h" // rectangle_iterator
#include "database.h" // getSpeakString
#include "delay.h" // interrupt_activity
#include "god-passive.h"
#include "items.h" // stack_iterator
#include "item-prop.h"
#include "item-status-flag-type.h" // ISFLAG_MIMIC
#include "message.h"
#include "notes.h" // NOTE_MESSAGE
#include "options.h" // UA_PICKUP, more_gem_info
#include "state.h"
#include "stringutil.h" // make_stringf
#include "view.h" // flash_view_delay

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

bool should_fear_zot()
{
    return bezotted();
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
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_METEORAN)
        update_vision_range();
#endif
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
        // Use your 'base' MHP (excluding forms, berserk, artefacts...)
        // to calculate loss, so that Dragon Form doesn't penalize extra HP
        // and players in unskilled talisman forms don't lose less HP.
        // TODO: also ignore ATTR_DIVINE_VIGOUR.
        const int mhp = get_real_hp(false, false);
        // However, use your current hp_max to set the max loss, so that you
        // can't go below 1 MHP ever.
        const int loss = min(3 + mhp / 6, you.hp_max - 1);
        // Take the note before decrementing max HP, so the notes have cause
        // before effect. Not sure if this should use current or base MHP.
        take_note(Note(NOTE_ZOT_TOUCHED, you.hp_max, you.hp_max - loss));
        dec_max_hp(loss);
        interrupt_activity(activity_interrupt::force);

        set_turns_until_zot(you.has_mutation(MUT_SHORT_LIFESPAN) ? 200 : 1000);
    }

    const int lvl = bezotting_level();
    if (old_lvl >= lvl)
        return;

    switch (lvl)
    {
        case 1:
            mpr("You have lingered too long. Zot senses you. "
                "Dive deeper or flee this branch before you suffer!");
            break;
        case 2:
            mpr("Zot draws nearer. "
                "Dive deeper or flee this branch before you suffer!");
            break;
        case 3:
            mpr("Zot has nearly found you. Suffering is imminent. "
                "Descend or flee this branch!");
            break;
    }

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_METEORAN)
        update_vision_range();
#endif

    take_note(Note(NOTE_MESSAGE, 0, 0, "Glimpsed the power of Zot."));
    interrupt_activity(activity_interrupt::force);
}

void set_turns_until_zot(int turns_left)
{
    if (turns_left < 0 || turns_left > MAX_ZOT_CLOCK / BASELINE_DELAY)
        return;

    int &clock = _zot_clock();
    clock = MAX_ZOT_CLOCK - turns_left * BASELINE_DELAY;
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_METEORAN)
        update_vision_range();
#endif
}


bool gem_clock_active()
{
    return !player_has_orb()
           && !crawl_state.game_is_sprint()
           && !crawl_state.game_is_descent();
}

/// Destroy all gems on the current floor. Note that there may be multiple, if
/// mimics are involved.
static void _shatter_floor_gem(gem_type gem, bool quiet = false)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        for (stack_iterator si(*ri); si; ++si)
        {
            if (!si->is_type(OBJ_GEMS, gem))
                continue;

            const bool was_mimic = si->flags & ISFLAG_MIMIC;

            item_was_destroyed(*si);
            destroy_item(si.index());
            you.gems_shattered.set(gem);

            if (quiet || !you.see_cell(*ri))
                continue;

            if (was_mimic)
            {
                mprf("The power of Zot manifests... and reveals the %s gem was"
                     " a mimic! It cackles and vanishes.", gem_adj(gem));
                continue;
            }

            mprf("With a frightful flash, the power of Zot shatters the %s"
                 " gem into ten thousand fragments!", gem_adj(gem));
            // Using UA_PICKUP here is dubious.
            flash_view_delay(UA_PICKUP, MAGENTA, 100);
            flash_view_delay(UA_PICKUP, LIGHTMAGENTA, 100);
        }
    }
}

int gem_time_left(int gem_int)
{
    gem_type gem = static_cast<gem_type>(gem_int);
    ASSERT_RANGE(gem, 0, NUM_GEM_TYPES);
    return gem_time_limit(gem) - you.gem_time_spent[gem];
}

void print_gem_warnings(int gem_int, int old_time_taken)
{
    if (!Options.more_gem_info)
        return;

    gem_type gem = static_cast<gem_type>(gem_int);
    if (gem == NUM_GEM_TYPES)
        return;

    ASSERT_RANGE(gem, 0, NUM_GEM_TYPES);
    if (!gem_clock_active() || !you.gems_found[gem] || you.gems_shattered[gem])
        return;

    const int time_taken = you.gem_time_spent[gem];
    const int limit = gem_time_limit(gem);

    if (old_time_taken + 2700 < limit && time_taken + 2700 >= limit)
    {
        mprf("If you linger in this branch much longer, the power of Zot will "
             "shatter your %s gem.", gem_adj(gem));
    }

    if (old_time_taken + 1000 < limit && time_taken + 1000 >= limit)
    {
        mprf("Zot senses the otherworldly energies of your %s gem, and will "
             "surely shatter it if you linger in this branch any longer!",
             gem_adj(gem));
    }
}

void incr_gem_clock()
{
    if (!gem_clock_active())
        return;

    const branch_type br = you.where_are_you;
    const gem_type gem = gem_for_branch(br);
    if (gem == NUM_GEM_TYPES || you.gems_shattered[gem])
        return;

    int &time_taken = you.gem_time_spent[gem];
    const int limit = gem_time_limit(gem);
    if (time_taken >= limit)
        return; // already lost...

    const int old_time_taken = time_taken;
    time_taken += you.time_taken;

    print_gem_warnings(gem, old_time_taken);

    if (time_taken < limit)
        return;

    // lose it!
    if (you.gems_found[gem])
    {
        take_note(Note(NOTE_GEM_LOST, gem));
        mark_milestone("gem.lost", make_stringf("lost the %s gem!",
                                                gem_adj(gem)));
        you.gems_shattered.set(gem);

        if (Options.more_gem_info)
        {
            mprf("With a frightful flash, the power of Zot shatters your %s gem "
                 "into ten thousand fragments!", gem_adj(gem));
            // Using UA_PICKUP here is dubious.
            flash_view_delay(UA_PICKUP, MAGENTA, 100);
            flash_view_delay(UA_PICKUP, LIGHTMAGENTA, 100);
        }
        return;
    }

    // OK, maybe it was on the level somewhere?
    _shatter_floor_gem(gem);
    // If not, we'll clean it up when we load the appropriate level.
}

void maybe_break_floor_gem()
{
    const gem_type gem = gem_for_branch(you.where_are_you);
    if (gem != NUM_GEM_TYPES
        && !you.gems_shattered[gem]
        && (crawl_state.game_is_descent() // No descent gems :(
            || gem_time_left(gem) <= 0))
    {
        _shatter_floor_gem(gem, true);
    }
}

string gem_status()
{
    if (!Options.more_gem_info)
        return "";

    const gem_type gem = gem_for_branch(you.where_are_you);
    if (gem == NUM_GEM_TYPES
        || !you.gems_found[gem]
        || you.gems_shattered[gem]
        || !gem_clock_active())
    {
        return "";
    }
    const int time_left = gem_time_left(gem);
    const int turns_left = (time_left + 9) / 10; // round up
    return make_stringf("If you linger in this branch for another %d turns, "
                        "the power of Zot will shatter your %s gem.\n",
                        turns_left, gem_adj(gem));
}
