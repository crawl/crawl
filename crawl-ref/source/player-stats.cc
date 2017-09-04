#include "AppHdr.h"

#include "player-stats.h"

#include "artefact.h"
#include "clua.h"
#include "delay.h"
#include "duration-type.h"
#include "equipment-type.h"
#include "files.h"
#include "god-passive.h"
#include "hints.h"
#include "item-status-flag-type.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#ifdef TOUCH_UI
#include "menu.h"
#endif
#include "message.h"
#include "misc.h"
#include "monster.h"
#include "mon-util.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "spl-cast.h"
#include "stat-type.h"
#include "state.h"
#include "stringutil.h"
#ifdef TOUCH_UI
#include "tiledef-gui.h"
#include "tilepick.h"
#endif
#include "transform.h"

int player::stat(stat_type s, bool nonneg) const
{
    const int val = max_stat(s) - stat_loss[s];
    return nonneg ? max(val, 0) : val;
}

int player::strength(bool nonneg) const
{
    return stat(STAT_STR, nonneg);
}

int player::intel(bool nonneg) const
{
    return stat(STAT_INT, nonneg);
}

int player::dex(bool nonneg) const
{
    return stat(STAT_DEX, nonneg);
}

static int _stat_modifier(stat_type stat, bool innate_only);

/**
 * What's the player's current maximum for a stat, before ability damage is
 * applied?
 *
 * @param s      The stat in question (e.g. STAT_STR).
 * @param innate Whether to disregard stat modifiers other than those from
 *               innate mutations.
 * @return      The player's maximum for the given stat; capped at
 *              MAX_STAT_VALUE.
 */
int player::max_stat(stat_type s, bool innate) const
{
    return min(base_stats[s] + _stat_modifier(s, innate), MAX_STAT_VALUE);
}

int player::max_strength() const
{
    return max_stat(STAT_STR);
}

int player::max_intel() const
{
    return max_stat(STAT_INT);
}

int player::max_dex() const
{
    return max_stat(STAT_DEX);
}

// Base stat including innate mutations (which base_stats does not)
int innate_stat(stat_type s)
{
    return you.max_stat(s, true);
}


static void _handle_stat_change(stat_type stat);

/*
 * Opens a prompt to choose a stat, and returns the stat_type chosen.
 * If increase_attribute is true, ensure player chooses a stat. If they
 * do not, restart the prompt.  If choice fails or is cancelled, return
 * STAT_RANDOM to indicate failure. (hackish, needs better solution?)
 */
stat_type choose_stat(string title, string message, string prompt, bool increase_attribute)
{
#ifdef TOUCH_UI
    if (increase_attribute)
    {
        learned_something_new(HINT_CHOOSE_STAT);
    }
    Popup *pop = new Popup(title);
    MenuEntry *status = new MenuEntry("", MEL_SUBTITLE);
    pop->push_entry(new MenuEntry(message + prompt, MEL_TITLE));
    pop->push_entry(status);
    MenuEntry *me = new MenuEntry("Strength", MEL_ITEM, 0, 'S', false);
    me->add_tile(tile_def(TILEG_FIGHTING_ON, TEX_GUI));
    pop->push_entry(me);
    me = new MenuEntry("Intelligence", MEL_ITEM, 0, 'I', false);
    me->add_tile(tile_def(TILEG_SPELLCASTING_ON, TEX_GUI));
    pop->push_entry(me);
    me = new MenuEntry("Dexterity", MEL_ITEM, 0, 'D', false);
    me->add_tile(tile_def(TILEG_DODGING_ON, TEX_GUI));
    pop->push_entry(me);
    if (!increase_attribute)
    {
        me = new MenuEntry("Cancel", MEL_ITEM, 0, 'X', false);
        me->add_tile(tile_def(TILEG_PROMPT_NO, TEX_GUI));
        pop->push_entry(me);
    }
#else
    if (increase_attribute)
    {
        mprf(MSGCH_INTRINSIC_GAIN, "%s", message.c_str());

        learned_something_new(HINT_CHOOSE_STAT);
        if (innate_stat(STAT_STR) != you.strength()
            || innate_stat(STAT_INT) != you.intel()
            || innate_stat(STAT_DEX) != you.dex())
        {
            mprf(MSGCH_PROMPT, "Your base attributes are Str %d, Int %d, Dex %d.",
                 innate_stat(STAT_STR),
                 innate_stat(STAT_INT),
                 innate_stat(STAT_DEX));
        }
    }
    mprf(MSGCH_PROMPT, "%s", prompt.c_str());
#endif

    mouse_control mc(MOUSE_MODE_PROMPT);
    int keyin;
    bool tried_lua = false;
    // NUM_STATS is a placeholder for null
    stat_type chosen_stat = NUM_STATS;

    do
    {
        // Calling a user-defined lua function here to let players reply to
        // the prompt automatically. Either returning a string or using
        // crawl.sendkeys will work.
        if (!tried_lua && clua.callfn("choose_stat_gain", 0, 1)
            && increase_attribute)
        {
            string result;
            clua.fnreturns(">s", &result);
            keyin = result[0];
        }
        else
        {
#ifdef TOUCH_UI
            keyin = pop->pop();
#else
            keyin = getchm();
#endif
        }
        tried_lua = true;

        switch (keyin)
        {
        CASE_ESCAPE
            // It is unsafe to save the game here; continue with the turn
            // normally, when the player reloads, the game will re-prompt
            // for their level-up stat gain.
            if (crawl_state.seen_hups)
            {
                chosen_stat = STAT_RANDOM;
            }
            break;

        case 's':
        case 'S':
            chosen_stat = STAT_STR;
            break;

        case 'i':
        case 'I':
            chosen_stat = STAT_INT;
            break;

        case 'd':
        case 'D':
            chosen_stat = STAT_DEX;
            break;

        case 'X':
        default:
            // If this function was called by attribute_increase(), keep looping
            // until a valid choice has been made (or the game closes).
            if (increase_attribute)
            {
#ifdef TOUCH_UI
                status->text = "Please choose an option below"; // too naggy?
#endif
                break;
            }
            chosen_stat = STAT_RANDOM;
            break;
        }
    } while (chosen_stat == NUM_STATS);
// Manually delete Popup and MenuEntrys (unsure if it is already being deleted
// elsewhere?)
#ifdef TOUCH_UI
    delete pop;
    delete status;
    delete me;
#endif
    return chosen_stat;
}

/**
 * Handle manual, permanent character stat increases. (Usually from every third
 * XL.
 *
 * @return Whether the stat was actually increased (HUPs can interrupt this).
 */
bool attribute_increase()
{
    // Gnolls don't get stat gains
    if (you.species == SP_GNOLL)
        return true;

    const string popup_title = "Increase Attributes";
    const string stat_gain_message = make_stringf("Your experience leads to a%s "
                                                  "increase in your attributes!",
                                                  you.species == SP_DEMIGOD ?
                                                  " dramatic" : "n");
    string prompt = "Increase (S)trength, (I)ntelligence, or (D)exterity? ";
#ifdef TOUCH_UI
    prompt = " Increase:";
#endif
    crawl_state.stat_gain_prompt = true;

    const int statgain = you.species == SP_DEMIGOD ? 2 : 1;

    stat_type chosen_stat = choose_stat(popup_title, stat_gain_message, prompt, true);

    //If STAT_RANDOM, choose_stat closed prematurely due to game closing
    if (chosen_stat == STAT_RANDOM)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < statgain; i++)
        {
                modify_stat(chosen_stat, 1, false);
        }
        return true;
    }
}

/*
 * Have Jiyva increase a player stat by one and decrease a different stat by
 * one.
 *
 * This considers armour evp and skills to determine which stats to change. A
 * target stat vector is created based on these factors, which is then fuzzed,
 * and then a shuffle of the player's stat points that doesn't increase the l^2
 * distance to the target vector is chosen.
*/
void jiyva_stat_action()
{
    int cur_stat[NUM_STATS];
    int stat_total = 0;
    int target_stat[NUM_STATS];
    for (int x = 0; x < NUM_STATS; ++x)
    {
        cur_stat[x] = you.stat(static_cast<stat_type>(x), false);
        stat_total += cur_stat[x];
    }

    int evp = you.unadjusted_body_armour_penalty();
    target_stat[STAT_STR] = max(9, evp);
    target_stat[STAT_INT] = 9;
    target_stat[STAT_DEX] = 9;
    int remaining = stat_total - 18 - target_stat[0];

    // Divide up the remaining stat points between Int and either Str or Dex,
    // based on skills.
    if (remaining > 0)
    {
        int magic_weights = 0;
        int other_weights = 0;
        for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        {
            int weight = you.skills[sk];

            if (sk >= SK_SPELLCASTING && sk < SK_INVOCATIONS)
                magic_weights += weight;
            else
                other_weights += weight;
        }
        // We give pure Int weighting if the player is sufficiently
        // focused on magic skills.
        other_weights = max(other_weights - magic_weights / 2, 0);

        // Now scale appropriately and apply the Int weighting
        magic_weights = div_rand_round(remaining * magic_weights,
                                       magic_weights + other_weights);
        other_weights = remaining - magic_weights;
        target_stat[STAT_INT] += magic_weights;

        // Heavy armour weights towards Str, Dodging skill towards Dex.
        int str_weight = 10 * evp;
        int dex_weight = 10 + you.skill(SK_DODGING, 10);

        // Now apply the Str and Dex weighting.
        const int str_adj = div_rand_round(other_weights * str_weight,
                                           str_weight + dex_weight);
        target_stat[STAT_STR] += str_adj;
        target_stat[STAT_DEX] += (other_weights - str_adj);
    }
    // Add a little fuzz to the target.
    for (int x = 0; x < NUM_STATS; ++x)
        target_stat[x] += random2(5) - 2;
    int choices = 0;
    int stat_up_choice = 0;
    int stat_down_choice = 0;
    // Choose a random stat shuffle that doesn't increase the l^2 distance to
    // the (fuzzed) target.
    for (int gain = 0; gain < NUM_STATS; ++gain)
        for (int lose = 0; lose < NUM_STATS; ++lose)
        {
            if (gain != lose && cur_stat[lose] > 1
                && target_stat[gain] - cur_stat[gain] > target_stat[lose] - cur_stat[lose]
                && cur_stat[gain] < MAX_STAT_VALUE && you.base_stats[lose] > 1)
            {
                choices++;
                if (one_chance_in(choices))
                {
                    stat_up_choice = gain;
                    stat_down_choice = lose;
                }
            }
        }
    if (choices)
    {
        simple_god_message("'s power touches on your attributes.");
        modify_stat(static_cast<stat_type>(stat_up_choice), 1, false);
        modify_stat(static_cast<stat_type>(stat_down_choice), -1, false);
    }
}

static const char* descs[NUM_STATS][NUM_STAT_DESCS] =
{
    { "strength", "weakened", "weaker", "stronger" },
    { "intelligence", "dopey", "stupid", "clever" },
    { "dexterity", "clumsy", "clumsy", "agile" }
};

const char* stat_desc(stat_type stat, stat_desc_type desc)
{
    return descs[stat][desc];
}

void modify_stat(stat_type which_stat, int amount, bool suppress_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity(AI_STAT_CHANGE);

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (!suppress_msg)
    {
        mprf((amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN,
             "You feel %s.",
             stat_desc(which_stat, (amount > 0) ? SD_INCREASE : SD_DECREASE));
    }

    you.base_stats[which_stat] += amount;

    _handle_stat_change(which_stat);
}

void notify_stat_change(stat_type which_stat, int amount, bool suppress_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Gnolls don't change stats, so don't notify
    if (you.species == SP_GNOLL)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity(AI_STAT_CHANGE);

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (!suppress_msg)
    {
        mprf((amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN,
             "You feel %s.",
             stat_desc(which_stat, (amount > 0) ? SD_INCREASE : SD_DECREASE));
    }

    _handle_stat_change(which_stat);
}

void notify_stat_change()
{
    for (int i = 0; i < NUM_STATS; ++i)
        _handle_stat_change(static_cast<stat_type>(i));
}

static int _mut_level(mutation_type mut, bool innate_only)
{
    return you.get_base_mutation_level(mut, true, !innate_only, !innate_only);
}

static int _strength_modifier(bool innate_only)
{
    // Gnolls can't modify their stats
    if (you.species == SP_GNOLL)
        return 0;

    int result = 0;

    if (!innate_only)
    {
        if (you.duration[DUR_MIGHT] || you.duration[DUR_BERSERK])
            result += 5;

        if (you.duration[DUR_DIVINE_STAMINA])
            result += you.attribute[ATTR_DIVINE_STAMINA];

        result += chei_stat_boost();

        // ego items of strength
        result += 3 * count_worn_ego(SPARM_STRENGTH);

        // rings of strength
        result += you.wearing(EQ_RINGS_PLUS, RING_STRENGTH);

        // randarts of strength
        result += you.scan_artefacts(ARTP_STRENGTH);

        // form
        result += get_form()->str_mod;
    }

    // mutations
    result += 2 * (_mut_level(MUT_STRONG, innate_only)
                   - _mut_level(MUT_WEAK, innate_only));
#if TAG_MAJOR_VERSION == 34
    result += _mut_level(MUT_STRONG_STIFF, innate_only)
              - _mut_level(MUT_FLEXIBLE_WEAK, innate_only);
#endif

    return result;
}

static int _int_modifier(bool innate_only)
{
    if (you.species == SP_GNOLL)
        return 0;

    int result = 0;

    if (!innate_only)
    {
        if (you.duration[DUR_BRILLIANCE])
            result += 5;

        if (you.duration[DUR_DIVINE_STAMINA])
            result += you.attribute[ATTR_DIVINE_STAMINA];

        result += chei_stat_boost();

        // ego items of intelligence
        result += 3 * count_worn_ego(SPARM_INTELLIGENCE);

        // rings of intelligence
        result += you.wearing(EQ_RINGS_PLUS, RING_INTELLIGENCE);

        // randarts of intelligence
        result += you.scan_artefacts(ARTP_INTELLIGENCE);
    }

    // mutations
    result += 2 * (_mut_level(MUT_CLEVER, innate_only)
                   - _mut_level(MUT_DOPEY, innate_only));

    return result;
}

static int _dex_modifier(bool innate_only)
{
    // Gnolls can't modify their stats
    if (you.species == SP_GNOLL)
        return 0;

    int result = 0;

    if (!innate_only)
    {
        if (you.duration[DUR_AGILITY])
            result += 5;

        if (you.duration[DUR_DIVINE_STAMINA])
            result += you.attribute[ATTR_DIVINE_STAMINA];

        result += chei_stat_boost();

        // ego items of dexterity
        result += 3 * count_worn_ego(SPARM_DEXTERITY);

        // rings of dexterity
        result += you.wearing(EQ_RINGS_PLUS, RING_DEXTERITY);

        // randarts of dexterity
        result += you.scan_artefacts(ARTP_DEXTERITY);

        // form
        result += get_form()->dex_mod;
    }

    // mutations
    result += 2 * (_mut_level(MUT_AGILE, innate_only)
                  - _mut_level(MUT_CLUMSY, innate_only));
#if TAG_MAJOR_VERSION == 34
    result += _mut_level(MUT_FLEXIBLE_WEAK, innate_only)
              - _mut_level(MUT_STRONG_STIFF, innate_only);
    result -= _mut_level(MUT_ROUGH_BLACK_SCALES, innate_only);
#endif
    result += 2 * _mut_level(MUT_THIN_SKELETAL_STRUCTURE, innate_only);

    return result;
}

static int _stat_modifier(stat_type stat, bool innate_only)
{
    switch (stat)
    {
    case STAT_STR: return _strength_modifier(innate_only);
    case STAT_INT: return _int_modifier(innate_only);
    case STAT_DEX: return _dex_modifier(innate_only);
    default:
        mprf(MSGCH_ERROR, "Bad stat: %d", stat);
        return 0;
    }
}

static string _stat_name(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR:
        return "strength";
    case STAT_INT:
        return "intelligence";
    case STAT_DEX:
        return "dexterity";
    default:
        die("invalid stat");
    }
}

int stat_loss_roll()
{
    const int loss = 30 + random2(30);
    dprf("Stat loss points: %d", loss);

    return loss;
}

bool lose_stat(stat_type which_stat, int stat_loss, bool force)
{
    // Gnolls cannot be stat drained
    if (you.species == SP_GNOLL)
        return false;

    if (stat_loss <= 0)
        return false;

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (!force)
    {
        if (you.duration[DUR_DIVINE_STAMINA] > 0)
        {
            mprf("Your divine stamina protects you from %s loss.",
                 _stat_name(which_stat).c_str());
            return false;
        }
    }

    mprf(MSGCH_WARN, "You feel %s.", stat_desc(which_stat, SD_LOSS));

    you.stat_loss[which_stat] = min<int>(100,
                                         you.stat_loss[which_stat] + stat_loss);
    if (!you.attribute[ATTR_STAT_LOSS_XP])
        you.attribute[ATTR_STAT_LOSS_XP] = stat_loss_roll();
    _handle_stat_change(which_stat);
    return true;
}

stat_type random_lost_stat()
{
    stat_type choice = NUM_STATS;
    int found = 0;
    for (int i = 0; i < NUM_STATS; ++i)
        if (you.stat_loss[i] > 0)
        {
            found++;
            if (one_chance_in(found))
                choice = static_cast<stat_type>(i);
        }
    return choice;
}

// Restore the stat in which_stat by the amount in stat_gain, displaying
// a message if suppress_msg is false, and doing so in the recovery
// channel if recovery is true. If stat_gain is 0, restore the stat
// completely.
bool restore_stat(stat_type which_stat, int stat_gain,
                  bool suppress_msg, bool recovery)
{
    // A bit hackish, but cut me some slack, man! --
    // Besides, a little recursion never hurt anyone {dlb}:
    if (which_stat == STAT_ALL)
    {
        bool stat_restored = false;
        for (int i = 0; i < NUM_STATS; ++i)
            if (restore_stat((stat_type) i, stat_gain, suppress_msg))
                stat_restored = true;

        return stat_restored;
    }

    if (which_stat == STAT_RANDOM)
        which_stat = random_lost_stat();

    if (which_stat >= NUM_STATS || you.stat_loss[which_stat] == 0)
        return false;

    if (!suppress_msg)
    {
        mprf(recovery ? MSGCH_RECOVERY : MSGCH_PLAIN,
             "You feel your %s returning.",
             _stat_name(which_stat).c_str());
    }

    if (stat_gain == 0 || stat_gain > you.stat_loss[which_stat])
        stat_gain = you.stat_loss[which_stat];

    you.stat_loss[which_stat] -= stat_gain;

    // If we're fully recovered, clear out stat loss recovery timer.
    if (random_lost_stat() == NUM_STATS)
        you.attribute[ATTR_STAT_LOSS_XP] = 0;

    _handle_stat_change(which_stat);
    return true;
}

static void _normalize_stat(stat_type stat)
{
    ASSERT(you.stat_loss[stat] >= 0);
    you.base_stats[stat] = min<int8_t>(you.base_stats[stat], MAX_STAT_VALUE);
}

static void _handle_stat_change(stat_type stat)
{
    ASSERT_RANGE(stat, 0, NUM_STATS);

    if (you.stat(stat) <= 0 && !you.duration[stat_zero_duration(stat)])
    {
        // Time required for recovery once the stat is restored, randomised slightly.
        you.duration[stat_zero_duration(stat)] =
            (20 + random2(20)) * BASELINE_DELAY;
        mprf(MSGCH_WARN, "You have lost your %s.", stat_desc(stat, SD_NAME));
        take_note(Note(NOTE_MESSAGE, 0, 0, make_stringf("Lost %s.",
            stat_desc(stat, SD_NAME)).c_str()), true);
        // 2 to 5 turns of paralysis (XXX: decremented right away?)
        you.increase_duration(DUR_PARALYSIS, 2 + random2(3));
    }

    you.redraw_stats[stat] = true;
    _normalize_stat(stat);

    switch (stat)
    {
    case STAT_STR:
        you.redraw_armour_class = true; // includes shields
        you.redraw_evasion = true; // Might reduce EV penalty
        break;

    case STAT_INT:
        break;

    case STAT_DEX:
        you.redraw_evasion = true;
        you.redraw_armour_class = true; // includes shields
        break;

    default:
        break;
    }
}

duration_type stat_zero_duration(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR:
        return DUR_COLLAPSE;
    case STAT_INT:
        return DUR_BRAINLESS;
    case STAT_DEX:
        return DUR_CLUMSY;
    default:
        die("invalid stat");
    }
}

bool have_stat_zero()
{
    for (int i = 0; i < NUM_STATS; ++i)
        if (you.duration[stat_zero_duration(static_cast<stat_type> (i))])
            return true;

    return false;
}

/* 
 * Gnoll ability to shift three points from one attribute to another,
 * becoming drained if successful.
 */
spret_type gnoll_shift_attributes()
{
    const string popup_title = "Shift Attributes";
    string prompt = "Shift from (S)trength, (I)ntelligence, or (D)exterity? ";
#ifdef TOUCH_UI
    prompt = "Shift from:";
#endif
    stat_type source_attribute = choose_stat(popup_title, "", prompt, false);

    //If STAT_RANDOM, choose_stat was cancelled so abort skill
    if (source_attribute == STAT_RANDOM)
    {
        mprf(MSGCH_PROMPT, "Okay, then.");
        return SPRET_ABORT;
    }
    //Cannot shift into stat zero or below
    else if ((you.stat(source_attribute) - 3) <= 0 )
    {
        mprf("You do not have enough %s to transfer!", _stat_name(source_attribute).c_str());
        return SPRET_ABORT;
    }

    prompt = "Shift to (S)trength, (I)ntelligence, or (D)exterity? ";
#ifdef TOUCH_UI
    prompt = "Shift to:";
#endif
    stat_type destination_attribute = choose_stat(popup_title, "", prompt, false);

    //If STAT_RANDOM, choose_stat was cancelled so abort skill
    if (destination_attribute == STAT_RANDOM)
    {    
        mprf(MSGCH_PROMPT, "Okay, then.");
        return SPRET_ABORT;
    }
    else if (source_attribute == destination_attribute)
    {
        mpr("You don't feel any different.");
        return SPRET_ABORT;
    }
    else
    {
        modify_stat(source_attribute, -3, false);
        modify_stat(destination_attribute, 3, false);
        drain_player(75, true, true);
        return SPRET_SUCCESS;
    }
}
