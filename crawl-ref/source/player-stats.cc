#include "AppHdr.h"

#include "player-stats.h"

#include "artefact.h"
#include "clua.h"
#include "delay.h"
#include "duration-type.h"
#include "files.h"
#include "god-passive.h"
#include "hints.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "monster.h"
#include "notes.h"
#include "ouch.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "stat-type.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "transform.h"

static int _stat_modifier(stat_type stat, bool innate_only);

/**
 * What's the player's value for a given stat?
 *
 * @param s      The stat in question (e.g. STAT_STR).
 * @param nonneg Whether to cap the stat at 0.
 * @param innate_only Whether to disregard stat modifiers other than those from
 *                    innate mutations.
 * @return       The player's value for a given stat; capped at MAX_STAT_VALUE.
 */
int player::stat(stat_type s, bool nonneg, bool innate_only) const
{
    const int val = min(base_stats[s] + _stat_modifier(s, innate_only), MAX_STAT_VALUE);
    return nonneg ? max(val, 0) : val;
}

int player::strength(bool nonneg) const
{
    return stat(STAT_STR, nonneg, false);
}

int player::intel(bool nonneg) const
{
    return stat(STAT_INT, nonneg, false);
}

int player::dex(bool nonneg) const
{
    return stat(STAT_DEX, nonneg, false);
}

// Base stat including innate mutations, but no temporary effects.
int innate_stat(stat_type s)
{
    return you.stat(s, false, true);
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

    const int statgain = you.has_mutation(MUT_DIVINE_ATTRS) ? 4 : 2;

    const string stat_gain_message = make_stringf("Your experience leads to a%s "
                                                  "increase in your attributes!",
                                                  (statgain > 2) ?
                                                  " dramatic" : "n");
    crawl_state.stat_gain_prompt = true;
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
    mouse_control mc(MOUSE_MODE_PROMPT);

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
            while ((keyin = getchm()) == CK_REDRAW)
            {
                redraw_screen();
                update_screen();
            }
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
            mprf(MSGCH_PROMPT, "Uppercase letters only, please.");
            break;
        }
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
    if (mut == MUT_NON_MUTATION)
        return 0;
    return you.get_base_mutation_level(mut, true, !innate_only, !innate_only);
}

struct mut_stat_effect
{
    mutation_type mut;
    int s;
    int i;
    int d;

    int effect(stat_type which) const
    {
        switch (which)
        {
        case STAT_STR: return s;
        case STAT_INT: return i;
        case STAT_DEX: return d;
        default: break;
        }
        return 0; // or ASSERT
    }

    int apply(stat_type which, bool innate_only) const
    {
        return _mut_level(mut, innate_only) * effect(which);
    }
};

static const vector<mut_stat_effect> mut_stat_effects = {
    //               s   i   d
    { MUT_STRONG,    4, -1, -1 },
    { MUT_AGILE,    -1, -1,  4 },
    { MUT_CLEVER,   -1,  4, -1 },
    { MUT_WEAK,     -2,  0,  0 },
    { MUT_BIG_BRAIN, 0,  2,  0 },
    { MUT_DOPEY,     0, -2,  0 },
    { MUT_CLUMSY,    0,  0, -2 },
    { MUT_THIN_SKELETAL_STRUCTURE,
                     0,  0,  2 },
#if TAG_MAJOR_VERSION == 34
    { MUT_ROUGH_BLACK_SCALES, 0, 0, -1},
    { MUT_STRONG_STIFF, 1, 0, -1 },
    { MUT_FLEXIBLE_WEAK, -1, 0, 1 },
#endif
};

static int _get_mut_effects(stat_type which_stat, bool innate_only)
{
    int total = 0;
    for (const auto &e : mut_stat_effects)
        total += e.apply(which_stat, innate_only);
    return total;
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
        result += 3 * you.wearing_ego(OBJ_ARMOUR, SPARM_STRENGTH);

        // rings of strength
        result += you.wearing_jewellery(RING_STRENGTH);

        // randarts of strength
        result += you.scan_artefacts(ARTP_STRENGTH);

        // form
        result += get_form()->str_mod;
    }

    // mutations
    result += _get_mut_effects(STAT_STR, innate_only);

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
        result += 3 * you.wearing_ego(OBJ_ARMOUR, SPARM_INTELLIGENCE);

        // rings of intelligence
        result += you.wearing_jewellery(RING_INTELLIGENCE);

        // randarts of intelligence
        result += you.scan_artefacts(ARTP_INTELLIGENCE);
    }

    // mutations
    result += _get_mut_effects(STAT_INT, innate_only);

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
        result += 3 * you.wearing_ego(OBJ_ARMOUR, SPARM_DEXTERITY);

        // rings of dexterity
        result += you.wearing_jewellery(RING_DEXTERITY);

        // randarts of dexterity
        result += you.scan_artefacts(ARTP_DEXTERITY);

        // form
        result += get_form()->dex_mod;
    }

    // mutations
    result += _get_mut_effects(STAT_DEX, innate_only);

    return result;
}

static int _base_stat_with_muts(stat_type s)
{
    // XX semi code dup (with player::max_stat)
    return min(you.base_stats[s] + _get_mut_effects(s, false), MAX_STAT_VALUE);
}

static int _base_stat_with_new_mut(stat_type which_stat, mutation_type mut)
{
    int base = _base_stat_with_muts(which_stat);
    for (const auto &e : mut_stat_effects)
        if (e.mut == mut)
            base += e.apply(which_stat, false);
    return base;
}

/// whether a mutation innately causes stat zero. Does not look at equpment etc.
bool mutation_causes_stat_zero(mutation_type mut)
{
    // not very elegant...
    return _base_stat_with_new_mut(STAT_STR, mut) <= 0
        || _base_stat_with_new_mut(STAT_INT, mut) <= 0
        || _base_stat_with_new_mut(STAT_DEX, mut) <= 0;
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

static void _handle_stat_change(stat_type stat)
{
    ASSERT_RANGE(stat, 0, NUM_STATS);

    const bool was_zero = you.attribute[ATTR_STAT_ZERO] & (1 << (int)stat);
    const int val = you.stat(stat);

    if (val <= 0 && !was_zero)
    {
        // Notify the player and make the penalty explicit.
        mprf(MSGCH_WARN, "You have lost all your %s. It will be difficult to act "
                         "quickly in this state!", stat_desc(stat, SD_NAME));

        you.attribute[ATTR_STAT_ZERO] |= 1 << (int)stat;
    }
    else if (was_zero && val > 0)
    {
        mprf(MSGCH_RECOVERY, "You have recovered your %s.", stat_desc(stat, SD_NAME));
        you.attribute[ATTR_STAT_ZERO] &= ~(1 << (int)stat);
    }

    you.redraw_stats[stat] = true;
}

bool have_stat_zero()
{
    return you.attribute[ATTR_STAT_ZERO] > 0;
}
