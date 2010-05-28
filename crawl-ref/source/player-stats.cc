#include "AppHdr.h"

#include "player-stats.h"

#include "delay.h"
#include "godpassive.h"
#include "macro.h"
#include "mon-util.h"
#include "monster.h"
#include "ouch.h"
#include "player.h"
#include "religion.h"
#include "state.h"
#include "transform.h"
#include "hints.h"

char player::stat(stat_type s, bool nonneg) const
{
    const char val = max_stat(s) - stat_loss[s];
    return (nonneg ? std::max<char>(val, 0) : val);
}

char player::strength() const
{
    return (stat(STAT_STR));
}

char player::intel() const
{
    return (stat(STAT_INT));
}

char player::dex() const
{
    return (stat(STAT_DEX));
}

static int _stat_modifier(stat_type stat);

char player::max_stat(stat_type s) const
{
    return (std::min(base_stats[s] + _stat_modifier(s), 72));
}

char player::max_strength() const
{
    return (max_stat(STAT_STR));
}

char player::max_intel() const
{
    return (max_stat(STAT_INT));
}

char player::max_dex() const
{
    return (max_stat(STAT_DEX));
}

static void _handle_stat_change(stat_type stat, const char *aux = NULL,
                                bool see_source = true);
static void _handle_stat_change(const char *aux = NULL, bool see_source = true);

void attribute_increase()
{
    mpr("Your experience leads to an increase in your attributes!",
        MSGCH_INTRINSIC_GAIN);
    learned_something_new(HINT_CHOOSE_STAT);
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
    {
         const char m = you.max_stat(static_cast<stat_type>(x));
         dec_weight[x] = std::min(10, std::max(0, m - 7));
    }

    stat_up_choice = choose_random_weighted(inc_weight, inc_weight + 3);
    stat_down_choice = choose_random_weighted(dec_weight, dec_weight + 3);

    if (stat_up_choice != stat_down_choice)
    {
        simple_god_message("'s power touches on your attributes.");
        const std::string cause = "the 'helpfulness' of "
                                  + god_name(you.religion);
        modify_stat(static_cast<stat_type>(stat_up_choice), 1, true, cause.c_str());
        modify_stat(static_cast<stat_type>(stat_down_choice), -1, true, cause.c_str());
    }
}

static kill_method_type _statloss_killtype(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR:
        return KILLED_BY_WEAKNESS;
    case STAT_INT:
        return KILLED_BY_STUPIDITY;
    case STAT_DEX:
        return KILLED_BY_CLUMSINESS;
    default:
        ASSERT(false);
        return NUM_KILLBY;
    }
}

const char* descs[NUM_STATS][NUM_STAT_DESCS] = {
    { "strength", "weakened", "weaker", "stronger" },
    { "intelligence", "dopey", "stupid", "clever" },
    { "dexterity", "clumsy", "clumsy", "agile" }
};

const char* stat_desc(stat_type stat, stat_desc_type desc)
{
    return (descs[stat][desc]);
}

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const char *cause, bool see_source)
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

    _handle_stat_change(which_stat, cause, see_source);
}

void notify_stat_change(stat_type which_stat, char amount, bool suppress_msg,
                        const char *cause, bool see_source)
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

    _handle_stat_change(which_stat, cause, see_source);
}

void notify_stat_change(stat_type which_stat, char amount, bool suppress_msg,
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

    notify_stat_change(which_stat, amount, suppress_msg,
                       (verb + " " + name).c_str(), true);
}

void notify_stat_change(const char* cause)
{
    _handle_stat_change(cause);
}

static int _strength_modifier()
{
    int result = 0;

    if (you.duration[DUR_MIGHT])
        result += 5;

    if (you.duration[DUR_DIVINE_STAMINA])
        result += you.attribute[ATTR_DIVINE_STAMINA];

    result += che_boost(CB_STATS);

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

    result += che_boost(CB_STATS);

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

    result += che_boost(CB_STATS);

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

static int _stat_modifier(stat_type stat)
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
    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    // scale modifier by player_sust_abil() - right-shift
    // permissible because stat_loss is unsigned: {dlb}
    if (!force)
        stat_loss >>= player_sust_abil();

    mprf(stat_loss > 0 ? MSGCH_WARN : MSGCH_PLAIN,
         "You feel %s%s.",
         stat_desc(which_stat, SD_LOSS),
         stat_loss > 0 ? "" : " for a moment");

    if (stat_loss > 0)
    {
        you.stat_loss[which_stat] += stat_loss;
        _handle_stat_change(which_stat);
        return (true);
    }
    else
        return (false);
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

static std::string _stat_name(stat_type stat)
{
    switch (stat)
    {
    case STAT_STR:
        return ("strength");
    case STAT_INT:
        return ("intelligence");
    case STAT_DEX:
        return ("dexterity");
    default:
        ASSERT(false);
        return ("BUG");
    }
}

static stat_type _random_lost_stat()
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
    return (choice);
}

// Restore the stat in which_stat by the amount in stat_gain, displaying
// a message if suppress_msg is false, and doing so in the recovery
// channel if recovery is true.  If stat_gain is 0, restore the stat
// completely.
bool restore_stat(stat_type which_stat, unsigned char stat_gain,
                  bool suppress_msg, bool recovery)
{
    // A bit hackish, but cut me some slack, man! --
    // Besides, a little recursion never hurt anyone {dlb}:
    if (which_stat == STAT_ALL)
    {
        bool stat_restored = false;
        for (int i = 0; i < NUM_STATS; ++i)
            if (restore_stat(static_cast<stat_type>(i), stat_gain, suppress_msg))
                stat_restored = true;

        return (stat_restored);
    }

    if (which_stat == STAT_RANDOM)
        which_stat = _random_lost_stat();

    if (which_stat >= NUM_STATS || you.stat_loss[which_stat] == 0)
        return (false);

    if (!suppress_msg)
    {
        mprf(recovery ? MSGCH_RECOVERY : MSGCH_PLAIN,
             "You feel your %s returning.",
             _stat_name(which_stat).c_str());
    }

    if (stat_gain == 0 || stat_gain > you.stat_loss[which_stat])
        stat_gain = you.stat_loss[which_stat];

    you.stat_loss[which_stat] -= stat_gain;
    _handle_stat_change(which_stat);
    return (true);
}

static void _normalize_stat(stat_type stat)
{
    ASSERT(you.stat_loss[stat] >= 0);
    // XXX: this doesn't prevent effective stats over 72.
    you.base_stats[stat] = std::min<char>(you.base_stats[stat], 72);
}

// Number of turns of stat at zero you start with.
#define STAT_ZERO_START 10
// Number of turns of stat at zero you can survive.
#define STAT_DEATH_TURNS 100

static void _handle_stat_change(stat_type stat, const char* cause, bool see_source)
{
    ASSERT(stat >= 0 && stat < NUM_STATS);

    if (you.stat(stat) <= 0 && you.stat_zero[stat] == 0)
    {
        you.stat_zero[stat] = STAT_ZERO_START;
        mprf(MSGCH_WARN, "You have lost your %s.", stat_desc(stat, SD_NAME));
        // 2 to 5 turns of paralysis (XXX: decremented right away?)
        you.increase_duration(DUR_PARALYSIS, 2 + random2(3));
    }

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

static void _handle_stat_change(const char* aux, bool see_source)
{
    for (int i = 0; i < NUM_STATS; ++i)
        _handle_stat_change(static_cast<stat_type>(i), aux, see_source);
}

// Called once per turn.
void update_stat_zero()
{
    for (int i = 0; i < NUM_STATS; ++i)
    {
        stat_type s = static_cast<stat_type>(i);
        if (you.stat(s) <= 0)
            you.stat_zero[s]++;
        else if (you.stat_zero[s] > 0)
        {
            you.stat_zero[s]--;
            if (you.stat_zero[s] == 0)
            {
                mprf("Your %s has recovered.", stat_desc(s, SD_NAME));
                you.redraw_stats[s] = true;
            }
        }

        if (you.stat_zero[i] > STAT_DEATH_TURNS)
            ouch(INSTANT_DEATH, NON_MONSTER, _statloss_killtype(s));
    }
}
