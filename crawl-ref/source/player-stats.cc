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
#include "libutil.h"
#include "macro.h"
#ifdef TOUCH_UI
#include "menu.h"
#endif
#include "message.h"
#include "monster.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "stat-type.h"
#include "state.h"
#include "stringutil.h"
#ifdef TOUCH_UI
#include "rltiles/tiledef-gui.h"
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

/**
 * Handle manual, permanent character stat increases. (Usually from every third
 * XL.
 *
 * @return Whether the stat was actually increased (HUPs can interrupt this).
 */
bool attribute_increase()
{
    const bool need_caps = Options.easy_confirm != easy_confirm_type::all;

    const string stat_gain_message = make_stringf("Your experience leads to a%s "
                                                  "increase in your attributes!",
                                                  you.species == SP_DEMIGOD ?
                                                  " dramatic" : "n");
    crawl_state.stat_gain_prompt = true;
#ifdef TOUCH_UI
    learned_something_new(HINT_CHOOSE_STAT);
    Menu pop(MF_SINGLESELECT | MF_ANYPRINTABLE);
    MenuEntry * const status = new MenuEntry("", MEL_SUBTITLE);
    MenuEntry * const s_me = new MenuEntry("Strength", MEL_ITEM, 1,
                                                        need_caps ? 'S' : 's');
    s_me->add_tile(tile_def(TILEG_FIGHTING_ON));
    MenuEntry * const i_me = new MenuEntry("Intelligence", MEL_ITEM, 1,
                                                        need_caps ? 'I' : 'i');
    i_me->add_tile(tile_def(TILEG_SPELLCASTING_ON));
    MenuEntry * const d_me = new MenuEntry("Dexterity", MEL_ITEM, 1,
                                                        need_caps ? 'D' : 'd');
    d_me->add_tile(tile_def(TILEG_DODGING_ON));

    pop.set_title(new MenuEntry("Increase Attributes", MEL_TITLE));
    pop.add_entry(new MenuEntry(stat_gain_message + " Increase:", MEL_TITLE));
    pop.add_entry(status);
    pop.add_entry(s_me);
    pop.add_entry(i_me);
    pop.add_entry(d_me);
#else
    mprf(MSGCH_INTRINSIC_GAIN, "%s", stat_gain_message.c_str());
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
    mprf(MSGCH_PROMPT, need_caps
        ? "Increase (S)trength, (I)ntelligence, or (D)exterity? "
        : "Increase (s)trength, (i)ntelligence, or (d)exterity? ");
#endif
    mouse_control mc(MOUSE_MODE_PROMPT);

    const int statgain = you.species == SP_DEMIGOD ? 2 : 1;

    bool tried_lua = false;
    int keyin;
    while (true)
    {
        // Calling a user-defined lua function here to let players reply to
        // the prompt automatically. Either returning a string or using
        // crawl.sendkeys will work.
        if (!tried_lua && clua.callfn("choose_stat_gain", 0, 1))
        {
            string result;
            clua.fnreturns(">s", &result);
            keyin = toupper_safe(result[0]);
        }
        else
        {
#ifdef TOUCH_UI
            pop.show();
            keyin = pop.getkey();
#else
            while ((keyin = getchm()) == CK_REDRAW)
            {
                redraw_screen();
                update_screen();
            }
#endif
        }
        tried_lua = true;

        if (!need_caps)
            keyin = toupper_safe(keyin);

        switch (keyin)
        {
        CASE_ESCAPE
            // It is unsafe to save the game here; continue with the turn
            // normally, when the player reloads, the game will re-prompt
            // for their level-up stat gain.
            if (crawl_state.seen_hups)
                return false;
            break;

        case 'S':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_STR, 1, false);
            return true;

        case 'I':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_INT, 1, false);
            return true;

        case 'D':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_DEX, 1, false);
            return true;

        case 's':
        case 'i':
        case 'd':
#ifdef TOUCH_UI
            status->text = "Uppercase letters only, please.";
#else
            mprf(MSGCH_PROMPT, "Uppercase letters only, please.");
#endif
            break;
#ifdef TOUCH_UI
        default:
            status->text = "Please choose an option below"; // too naggy?
#endif
        }
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
        interrupt_activity(activity_interrupt::stat_change);

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

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity(activity_interrupt::stat_change);

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
    int result = 0;

    if (!innate_only)
    {
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
    int result = 0;

    if (!innate_only)
    {
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
    int result = 0;

    if (!innate_only)
    {
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
