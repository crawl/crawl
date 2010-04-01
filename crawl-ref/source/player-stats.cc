#include "AppHdr.h"

#include "player-stats.h"

#include "delay.h"
#include "macro.h"
#include "mon-util.h"
#include "monster.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "state.h"
#include "transform.h"
#include "tutorial.h"

char player::strength() const
{
    return (stats[STAT_STR]);
}

char player::intel() const
{
    return (stats[STAT_INT]);
}

char player::dex() const
{
    return (stats[STAT_DEX]);
}

char player::max_strength() const
{
    return (max_stats[STAT_STR]);
}

char player::max_intel() const
{
    return (max_stats[STAT_INT]);
}

char player::max_dex() const
{
    return (max_stats[STAT_DEX]);
}

static void _handle_stat_change(stat_type stat);
static void _handle_stat_change();

void attribute_increase()
{
    mpr("Your experience leads to an increase in your attributes!",
        MSGCH_INTRINSIC_GAIN);
    learned_something_new(TUT_CHOOSE_STAT);
    mpr("Increase (S)trength, (I)ntelligence, or (D)exterity? ", MSGCH_PROMPT);

    while (true)
    {
        const int keyin = getchm();

        switch (keyin)
        {
#if defined(USE_UNIX_SIGNALS) && defined(SIGHUP_SAVE) && defined(USE_CURSES)
        case ESCAPE:
        case -1:
            // [ds] It's safe to save the game here; when the player
            // reloads, the game will re-prompt for their level-up
            // stat gain.
            if (crawl_state.seen_hups)
                sighup_save_and_exit();
            break;
#endif

        case 's':
        case 'S':
            modify_stat(STAT_STR, 1, false, "level gain");
            you.last_chosen = STAT_STR;
            return;

        case 'i':
        case 'I':
            modify_stat(STAT_INT, 1, false, "level gain");
            you.last_chosen = STAT_INT;
            return;

        case 'd':
        case 'D':
            modify_stat(STAT_DEX, 1, false, "level gain");
            you.last_chosen = STAT_DEX;
            return;
        }
    }
}

// Rearrange stats, biased towards the stat chosen last at level up.
void jiyva_stat_action()
{
    int inc_weight[] = {1, 1, 1};
    int dec_weight[3];
    int stat_up_choice;
    int stat_down_choice;

    inc_weight[you.last_chosen] = 2;

    for (int x = 0; x < 3; ++x)
         dec_weight[x] = std::min(10, std::max(0, you.max_stats[x] - 7));

    stat_up_choice = choose_random_weighted(inc_weight, inc_weight + 3);
    stat_down_choice = choose_random_weighted(dec_weight, dec_weight + 3);

    if (stat_up_choice != stat_down_choice)
    {
        // We have a stat change noticeable to the player at this point.
        // This could be lethal if the player currently has 1 in a stat
        // but has a max stat of something higher -- perhaps we should
        // check for that?

        you.max_stats[stat_up_choice]++;
        you.max_stats[stat_down_choice]--;
        you.stats[stat_up_choice]++;
        you.stats[stat_down_choice]--;

        simple_god_message("'s power touches on your attributes.");

        _handle_stat_change();
    }
}

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const char *cause, bool see_source)
{
    ASSERT(!crawl_state.game_is_arena());

    kill_method_type kill_type = NUM_KILLBY;

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity( AI_STAT_CHANGE );

    std::string msg = "You feel ";

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    switch (which_stat)
    {
    case STAT_STR:
        kill_type    = KILLED_BY_WEAKNESS;
        msg += ((amount > 0) ? "stronger." : "weaker.");
        break;

    case STAT_INT:
        kill_type    = KILLED_BY_STUPIDITY;
        msg += ((amount > 0) ? "clever." : "stupid.");
        break;

    case STAT_DEX:
        kill_type    = KILLED_BY_CLUMSINESS;
        msg += ((amount > 0) ? "agile." : "clumsy.");
        break;

    default:
        break;
    }

    if (!suppress_msg && amount != 0)
        mpr( msg.c_str(), (amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN );

    you.stats[which_stat] += amount;
    you.max_stats[which_stat] += amount;

    if (amount < 0 && you.stats[which_stat] < 1)
    {
        if (cause == NULL)
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type);
        else
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type, cause, see_source);
    }

    _handle_stat_change(which_stat);
}

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const std::string& cause, bool see_source)
{
    modify_stat(which_stat, amount, suppress_msg, cause.c_str(), see_source);
}

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const monsters* cause)
{
    if (cause == NULL || invalid_monster(cause))
    {
        modify_stat(which_stat, amount, suppress_msg, NULL, true);
        return;
    }

    bool        vis  = you.can_see(cause);
    std::string name = cause->name(DESC_NOCAP_A, true);

    if (cause->has_ench(ENCH_SHAPESHIFTER))
        name += " (shapeshifter)";
    else if (cause->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        name += " (glowing shapeshifter)";

    modify_stat(which_stat, amount, suppress_msg, name, vis);
}

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const item_def &cause, bool removed)
{
    std::string name = cause.name(DESC_NOCAP_THE, false, true, false, false,
                                  ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
    std::string verb;

    switch (cause.base_type)
    {
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (removed)
            verb = "removing";
        else
            verb = "wearing";
        break;

    case OBJ_WEAPONS:
    case OBJ_STAVES:
        if (removed)
            verb = "unwielding";
        else
            verb = "wielding";
        break;

    case OBJ_WANDS:   verb = "zapping";  break;
    case OBJ_FOOD:    verb = "eating";   break;
    case OBJ_SCROLLS: verb = "reading";  break;
    case OBJ_POTIONS: verb = "drinking"; break;
    default:          verb = "using";
    }

    modify_stat(which_stat, amount, suppress_msg,
                verb + " " + name, true);
}

static int _strength_modifier()
{
    int result = 0;

    if (you.duration[DUR_MIGHT])
        result += 5;

    if (you.duration[DUR_DIVINE_STAMINA])
        result += you.attribute[ATTR_DIVINE_STAMINA];

    // ego items of strength
    result += 3 * count_worn_ego(SPARM_STRENGTH);

    // rings of strength
    result += player_equip(EQ_RINGS_PLUS, RING_STRENGTH);

    // randarts of strength
    result += scan_artefacts(ARTP_STRENGTH);

    // mutations
    result += player_mutation_level(MUT_STRONG)
              - player_mutation_level(MUT_WEAK);
    result += player_mutation_level(MUT_STRONG_STIFF)
              - player_mutation_level(MUT_FLEXIBLE_WEAK);
    result -= player_mutation_level(MUT_THIN_SKELETAL_STRUCTURE);

    // transformations
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_STATUE:          result +=  2; break;
    case TRAN_DRAGON:          result += 10; break;
    case TRAN_LICH:            result +=  3; break;
    case TRAN_BAT:             result -=  5; break;
    default:                                 break;
    }

    return (result);
}

static int _int_modifier()
{
    int result = 0;

    if (you.duration[DUR_BRILLIANCE])
        result += 5;

    if (you.duration[DUR_DIVINE_STAMINA])
        result += you.attribute[ATTR_DIVINE_STAMINA];

    // ego items of intelligence
    result += 3 * count_worn_ego(SPARM_INTELLIGENCE);

    // rings of intelligence
    result += player_equip(EQ_RINGS_PLUS, RING_INTELLIGENCE);

    // randarts of intelligence
    result += scan_artefacts(ARTP_INTELLIGENCE);

    // mutations
    result += player_mutation_level(MUT_CLEVER)
              - player_mutation_level(MUT_DOPEY);

    return (result);
}

static int _dex_modifier()
{
    int result = 0;

    if (you.duration[DUR_AGILITY])
        result += 5;

    if (you.duration[DUR_DIVINE_STAMINA])
        result += you.attribute[ATTR_DIVINE_STAMINA];

    // ego items of dexterity
    result += 3 * count_worn_ego(SPARM_DEXTERITY);

    // rings of dexterity
    result += player_equip(EQ_RINGS_PLUS, RING_DEXTERITY);

    // randarts of dexterity
    result += scan_artefacts(ARTP_DEXTERITY);

    // mutations
    result += player_mutation_level(MUT_AGILE)
              - player_mutation_level(MUT_CLUMSY);
    result += player_mutation_level(MUT_FLEXIBLE_WEAK)
              - player_mutation_level(MUT_STRONG_STIFF);

    result += player_mutation_level(MUT_THIN_SKELETAL_STRUCTURE);
    result -= player_mutation_level(MUT_ROUGH_BLACK_SCALES);

    // transformations
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_SPIDER: result +=  5; break;
    case TRAN_STATUE: result -=  2; break;
    case TRAN_BAT:    result +=  5; break;
    default:                        break;
    }

    return (result);
}

int stat_modifier(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR: return _strength_modifier();
    case STAT_INT: return _int_modifier();
    case STAT_DEX: return _dex_modifier();
    default:
        mprf(MSGCH_ERROR, "Bad stat: %d", stat);
        return 0;
    }
}

// use player::decrease_stats() instead iff:
// (a) player_sust_abil() should not factor in; and
// (b) there is no floor to the final stat values {dlb}
bool lose_stat(stat_type which_stat, unsigned char stat_loss, bool force,
               const char *cause, bool see_source)
{
    bool statLowered = false;   // must initialise to false {dlb}

    kill_method_type kill_type = NUM_KILLBY;

    // begin outputing message: {dlb}
    std::string msg = "You feel ";

    // set pointers to appropriate variables: {dlb}
    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    switch (which_stat)
    {
    case STAT_STR:
        msg       += "weakened";
        kill_type  = KILLED_BY_WEAKNESS;
        break;

    case STAT_DEX:
        msg       += "clumsy";
        kill_type  = KILLED_BY_CLUMSINESS;
        break;

    case STAT_INT:
        msg       += "dopey";
        kill_type  = KILLED_BY_STUPIDITY;
        break;

    default:
        ASSERT(false);
    }

    // scale modifier by player_sust_abil() - right-shift
    // permissible because stat_loss is unsigned: {dlb}
    if (!force)
        stat_loss >>= player_sust_abil();

    if (stat_loss > 0)
    {
        you.stats[which_stat] -= stat_loss;
        _handle_stat_change(which_stat);
    }
    else
        msg += " for a moment";

    msg += ".";
    mpr(msg, statLowered ? MSGCH_WARN : MSGCH_PLAIN);

    if (you.stats[which_stat] < 1)
    {
        if (cause == NULL)
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type);
        else
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type, cause, see_source);
    }

    return (stat_loss > 0);
}

bool lose_stat(stat_type which_stat, unsigned char stat_loss, bool force,
               const std::string cause, bool see_source)
{
    return lose_stat(which_stat, stat_loss, force, cause.c_str(), see_source);
}

bool lose_stat(stat_type which_stat, unsigned char stat_loss,
               const monsters* cause, bool force)
{
    if (cause == NULL || invalid_monster(cause))
        return lose_stat(which_stat, stat_loss, force, NULL, true);

    bool        vis  = you.can_see(cause);
    std::string name = cause->name(DESC_NOCAP_A, true);

    if (cause->has_ench(ENCH_SHAPESHIFTER))
        name += " (shapeshifter)";
    else if (cause->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        name += " (glowing shapeshifter)";

    return lose_stat(which_stat, stat_loss, force, name, vis);
}

bool lose_stat(stat_type which_stat, unsigned char stat_loss,
               const item_def &cause, bool removed, bool force)
{
    std::string name = cause.name(DESC_NOCAP_THE, false, true, false, false,
                                  ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
    std::string verb;

    switch (cause.base_type)
    {
    case OBJ_ARMOUR:
    case OBJ_JEWELLERY:
        if (removed)
            verb = "removing";
        else
            verb = "wearing";
        break;

    case OBJ_WEAPONS:
    case OBJ_STAVES:
        if (removed)
            verb = "unwielding";
        else
            verb = "wielding";
        break;

    case OBJ_WANDS:   verb = "zapping";  break;
    case OBJ_FOOD:    verb = "eating";   break;
    case OBJ_SCROLLS: verb = "reading";  break;
    case OBJ_POTIONS: verb = "drinking"; break;
    default:          verb = "using";
    }

    return lose_stat(which_stat, stat_loss, force, verb + " " + name, true);
}

// Restore the stat in which_stat by the amount in stat_gain, displaying
// a message if suppress_msg is false, and doing so in the recovery
// channel if recovery is true.  If stat_gain is 0, restore the stat
// completely.
bool restore_stat(stat_type which_stat, unsigned char stat_gain,
                  bool suppress_msg, bool recovery)
{
    bool stat_restored = false;

    // A bit hackish, but cut me some slack, man! --
    // Besides, a little recursion never hurt anyone {dlb}:
    if (which_stat == STAT_ALL)
    {
        for (int i = 0; i < NUM_STATS; ++i)
            if (restore_stat(static_cast<stat_type>(i), stat_gain, suppress_msg))
                stat_restored = true;

        return (stat_restored);
    }

    std::string msg = "You feel your ";

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    switch (which_stat)
    {
    case STAT_STR:
        msg += "strength";
        break;
    case STAT_INT:
        msg += "intelligence";
        break;
    case STAT_DEX:
        msg += "dexterity";
        break;
    default:
        ASSERT(false);
    }

    if (you.stats[which_stat] < you.max_stats[which_stat])
    {
        msg += " returning.";
        if (!suppress_msg)
            mpr(msg.c_str(), (recovery) ? MSGCH_RECOVERY : MSGCH_PLAIN);

        if (stat_gain == 0 || you.stats[which_stat] + stat_gain > you.max_stats[which_stat])
            stat_gain = you.max_stats[which_stat] - you.stats[which_stat];

        if (stat_gain != 0)
        {
            you.stats[which_stat] += stat_gain;
            stat_restored = true;

            _handle_stat_change(which_stat);
        }
    }

    return (stat_restored);
}

static void _mod_stat(stat_type stat, int mod)
{
    if (mod)
    {
        you.stats[stat] += mod;
        you.max_stats[stat] += mod;
        _handle_stat_change(stat);
    }
}

void modify_all_stats(int STmod, int IQmod, int DXmod)
{
    _mod_stat(STAT_STR, STmod);
    _mod_stat(STAT_INT, IQmod);
    _mod_stat(STAT_DEX, DXmod);
}

static void _normalize_stat(stat_type stat)
{
    you.stats[stat] = std::max<char>(you.stats[stat], 0);
    you.stats[stat] = std::min<char>(you.stats[stat], 72);
    you.max_stats[stat] = std::min<char>(you.max_stats[stat], 72);
}

static void _handle_stat_change(stat_type stat)
{
    ASSERT(stat >= 0 && stat < NUM_STATS);

    you.redraw_stats[stat] = true;
    _normalize_stat(stat);

    switch (stat)
    {
    case STAT_STR:
        burden_change();
        you.redraw_armour_class = true;
        break;

    case STAT_INT:
        break;

    case STAT_DEX:
        you.redraw_evasion = true;
        break;

    default:
        break;
    }
}

static void _handle_stat_change()
{
    for (int i = 0; i < NUM_STATS; ++i)
        _handle_stat_change(static_cast<stat_type>(i));
}
