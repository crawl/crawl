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
#include "tutorial.h"

char player::stat(stat_type s) const
{
    return (max_stats[s] - stat_loss[s]);
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

static void _handle_stat_change(stat_type stat, const char *aux = NULL,
                                bool see_source = true);
static void _handle_stat_change(const char *aux = NULL, bool see_source = true);

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
        simple_god_message("'s power touches on your attributes.");
        const std::string cause = "the 'helpfulness' of "
                                  + god_name(you.religion);
        modify_stat(static_cast<stat_type>(stat_up_choice), 1, true, cause);
        modify_stat(static_cast<stat_type>(stat_down_choice), -1, true, cause);
    }
}

static kill_method_type statloss_killtype[] = {
    KILLED_BY_WEAKNESS,
    KILLED_BY_CLUMSINESS,
    KILLED_BY_STUPIDITY
};

enum stat_desc_type
{
    SD_LOSS,
    SD_DECREASE,
    SD_INCREASE,
    NUM_STAT_DESCS
};

const char* descs[NUM_STATS][NUM_STAT_DESCS] = {
    { "weakened", "weaker", "stronger" },
    { "dopey", "stupid", "clever" },
    { "clumsy", "clumsy", "agile" }
};

static const char* _stat_desc(stat_type stat, stat_desc_type desc)
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
             _stat_desc(which_stat, (amount > 0) ? SD_INCREASE : SD_DECREASE));
    }

    you.max_stats[which_stat] += amount;

    _handle_stat_change(which_stat, cause, see_source);
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
    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    // scale modifier by player_sust_abil() - right-shift
    // permissible because stat_loss is unsigned: {dlb}
    if (!force)
        stat_loss >>= player_sust_abil();

    mprf(stat_loss > 0 ? MSGCH_WARN : MSGCH_PLAIN,
         "You feel %s%s.",
         _stat_desc(which_stat, SD_LOSS),
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
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (you.stat_loss[which_stat] > 0)
    {
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
    else
        return (false);
}

static void _normalize_stat(stat_type stat)
{
    you.stat_loss[stat] = std::max<char>(you.stat_loss[stat], 0);
    you.stat_loss[stat] = std::min<char>(you.stat_loss[stat], you.max_stats[stat]);
    you.max_stats[stat] = std::min<char>(you.max_stats[stat], 72);
}

static void _handle_stat_change(stat_type stat, const char* cause, bool see_source)
{
    ASSERT(stat >= 0 && stat < NUM_STATS);

    if (you.stat(stat) < 1)
    {
        const kill_method_type kill_type = statloss_killtype[stat];
        ouch(INSTANT_DEATH, NON_MONSTER, kill_type, cause, see_source);
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
