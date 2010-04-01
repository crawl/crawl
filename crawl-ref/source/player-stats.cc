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
    char* max_statp[]  = { &you.max_strength, &you.max_intel, &you.max_dex };
    char* base_statp[] = { &you.strength, &you.intel, &you.dex };
    int incremented_weight[] = {1, 1, 1};
    int decremented_weight[3];
    int stat_up_choice;
    int stat_down_choice;

    incremented_weight[you.last_chosen] = 2;

    for (int x = 0; x < 3; ++x)
         decremented_weight[x] = std::min(10, std::max(0, *max_statp[x] - 7));

    stat_up_choice = choose_random_weighted(incremented_weight,
                                            incremented_weight + 3);
    stat_down_choice = choose_random_weighted(decremented_weight,
                                              decremented_weight + 3);

    if (stat_up_choice != stat_down_choice)
    {
        // We have a stat change noticeable to the player at this point.
        // This could be lethal if the player currently has 1 in a stat
        // but has a max stat of something higher -- perhaps we should
        // check for that?

        (*max_statp[stat_up_choice])++;
        (*max_statp[stat_down_choice])--;
        (*base_statp[stat_up_choice])++;
        (*base_statp[stat_down_choice])--;

        simple_god_message("'s power touches on your attributes.");

        you.redraw_strength     = true;
        you.redraw_intelligence = true;
        you.redraw_dexterity    = true;

        burden_change();
    }
}

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const char *cause, bool see_source)
{
    ASSERT(!crawl_state.game_is_arena());

    char *ptr_stat = NULL;
    char *ptr_stat_max = NULL;
    bool *ptr_redraw = NULL;

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
        ptr_stat     = &you.strength;
        ptr_stat_max = &you.max_strength;
        ptr_redraw   = &you.redraw_strength;
        kill_type    = KILLED_BY_WEAKNESS;
        msg += ((amount > 0) ? "stronger." : "weaker.");
        break;

    case STAT_INT:
        ptr_stat     = &you.intel;
        ptr_stat_max = &you.max_intel;
        ptr_redraw   = &you.redraw_intelligence;
        kill_type    = KILLED_BY_STUPIDITY;
        msg += ((amount > 0) ? "clever." : "stupid.");
        break;

    case STAT_DEX:
        ptr_stat     = &you.dex;
        ptr_stat_max = &you.max_dex;
        ptr_redraw   = &you.redraw_dexterity;
        kill_type    = KILLED_BY_CLUMSINESS;
        msg += ((amount > 0) ? "agile." : "clumsy.");
        break;

    default:
        break;
    }

    if (!suppress_msg && amount != 0)
        mpr( msg.c_str(), (amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN );

    *ptr_stat     += amount;
    *ptr_stat_max += amount;
    *ptr_redraw    = 1;

    if (amount < 0 && *ptr_stat < 1)
    {
        if (cause == NULL)
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type);
        else
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type, cause, see_source);
    }

    if (ptr_stat == &you.strength)
    {
        burden_change();
        you.redraw_armour_class = true; // This includes shields.
    }
    if (ptr_stat == &you.dex)
        you.redraw_evasion = true;
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
    case STAT_STR:     return _strength_modifier();
    case STAT_INT: return _int_modifier();
    case STAT_DEX:    return _dex_modifier();
    default:
        mprf(MSGCH_ERROR, "Bad stat: %d", stat);
        return 0;
    }
}

// use player::decrease_stats() instead iff:
// (a) player_sust_abil() should not factor in; and
// (b) there is no floor to the final stat values {dlb}
bool lose_stat(unsigned char which_stat, unsigned char stat_loss, bool force,
               const char *cause, bool see_source)
{
    bool statLowered = false;   // must initialise to false {dlb}
    char *ptr_stat   = NULL;
    bool *ptr_redraw = NULL;
    char newValue = 0;          // holds new value, for comparison to old {dlb}

    kill_method_type kill_type = NUM_KILLBY;

    // begin outputing message: {dlb}
    std::string msg = "You feel ";

    // set pointers to appropriate variables: {dlb}
    if (which_stat == STAT_RANDOM)
        which_stat = random2(NUM_STATS);

    switch (which_stat)
    {
    case STAT_STR:
        msg       += "weakened";
        ptr_stat   = &you.strength;
        ptr_redraw = &you.redraw_strength;
        kill_type  = KILLED_BY_WEAKNESS;
        break;

    case STAT_DEX:
        msg       += "clumsy";
        ptr_stat   = &you.dex;
        ptr_redraw = &you.redraw_dexterity;
        kill_type  = KILLED_BY_CLUMSINESS;
        break;

    case STAT_INT:
        msg       += "dopey";
        ptr_stat   = &you.intel;
        ptr_redraw = &you.redraw_intelligence;
        kill_type  = KILLED_BY_STUPIDITY;
        break;
    }

    // scale modifier by player_sust_abil() - right-shift
    // permissible because stat_loss is unsigned: {dlb}
    if (!force)
        stat_loss >>= player_sust_abil();

    // newValue is current value less modifier: {dlb}
    newValue = *ptr_stat - stat_loss;

    // conceivable that stat was already *at* three
    // or stat_loss zeroed by player_sust_abil(): {dlb}
    //
    // Actually, that code was somewhat flawed.  Several race-class combos
    // can start with a stat lower than three, and this block (which
    // used to say '!=' would actually cause stat gain with the '< 3'
    // check that used to be above.  Crawl has stat-death code and I
    // don't see why we shouldn't be using it here.  -- bwr
    if (newValue < *ptr_stat)
    {
        *ptr_stat = newValue;
        *ptr_redraw = 1;

        // handle burden change, where appropriate: {dlb}
        if (ptr_stat == &you.strength)
            burden_change();

        statLowered = true;  // that is, stat was lowered (not just changed)
    }

    // a warning to player that s/he cut it close: {dlb}
    if (!statLowered)
        msg += " for a moment";

    // finish outputting message: {dlb}
    msg += ".";
    mpr(msg.c_str(), statLowered ? MSGCH_WARN : MSGCH_PLAIN);

    if (newValue < 1)
    {
        if (cause == NULL)
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type);
        else
            ouch(INSTANT_DEATH, NON_MONSTER, kill_type, cause, see_source);
    }


    return (statLowered);
}                               // end lose_stat()

bool lose_stat(unsigned char which_stat, unsigned char stat_loss, bool force,
               const std::string cause, bool see_source)
{
    return lose_stat(which_stat, stat_loss, force, cause.c_str(), see_source);
}

bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
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

bool lose_stat(unsigned char which_stat, unsigned char stat_loss,
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
bool restore_stat(unsigned char which_stat, unsigned char stat_gain,
                  bool suppress_msg, bool recovery)
{
    bool stat_restored = false;

    // A bit hackish, but cut me some slack, man! --
    // Besides, a little recursion never hurt anyone {dlb}:
    if (which_stat == STAT_ALL)
    {
        for (unsigned char loopy = STAT_STR; loopy < NUM_STATS; ++loopy)
            if (restore_stat(loopy, stat_gain, suppress_msg))
                stat_restored = true;

        return (stat_restored);                // early return {dlb}
    }

    // The real function begins here. {dlb}
    char *ptr_stat = NULL;
    char *ptr_stat_max = NULL;
    bool *ptr_redraw = NULL;

    std::string msg = "You feel your ";

    if (which_stat == STAT_RANDOM)
        which_stat = random2(NUM_STATS);

    switch (which_stat)
    {
    case STAT_STR:
        msg += "strength";

        ptr_stat = &you.strength;
        ptr_stat_max = &you.max_strength;
        ptr_redraw = &you.redraw_strength;
        break;

    case STAT_INT:
        msg += "intelligence";

        ptr_stat = &you.intel;
        ptr_stat_max = &you.max_intel;
        ptr_redraw = &you.redraw_intelligence;
        break;

    case STAT_DEX:
        msg += "dexterity";

        ptr_stat = &you.dex;
        ptr_stat_max = &you.max_dex;
        ptr_redraw = &you.redraw_dexterity;
        break;
    }

    if (*ptr_stat < *ptr_stat_max)
    {
        msg += " returning.";
        if (!suppress_msg)
            mpr(msg.c_str(), (recovery) ? MSGCH_RECOVERY : MSGCH_PLAIN);

        if (stat_gain == 0 || *ptr_stat + stat_gain > *ptr_stat_max)
            stat_gain = *ptr_stat_max - *ptr_stat;

        if (stat_gain != 0)
        {
            *ptr_stat += stat_gain;
            *ptr_redraw = true;
            stat_restored = true;

            if (ptr_stat == &you.strength)
                burden_change();
        }
    }

    return (stat_restored);
}
