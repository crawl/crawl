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
            modify_stat(STAT_STRENGTH, 1, false, "level gain");
            you.last_chosen = STAT_STRENGTH;
            return;

        case 'i':
        case 'I':
            modify_stat(STAT_INTELLIGENCE, 1, false, "level gain");
            you.last_chosen = STAT_INTELLIGENCE;
            return;

        case 'd':
        case 'D':
            modify_stat(STAT_DEXTERITY, 1, false, "level gain");
            you.last_chosen = STAT_DEXTERITY;
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
    case STAT_STRENGTH:
        ptr_stat     = &you.strength;
        ptr_stat_max = &you.max_strength;
        ptr_redraw   = &you.redraw_strength;
        kill_type    = KILLED_BY_WEAKNESS;
        msg += ((amount > 0) ? "stronger." : "weaker.");
        break;

    case STAT_INTELLIGENCE:
        ptr_stat     = &you.intel;
        ptr_stat_max = &you.max_intel;
        ptr_redraw   = &you.redraw_intelligence;
        kill_type    = KILLED_BY_STUPIDITY;
        msg += ((amount > 0) ? "clever." : "stupid.");
        break;

    case STAT_DEXTERITY:
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
    case STAT_STRENGTH:     return _strength_modifier();
    case STAT_INTELLIGENCE: return _int_modifier();
    case STAT_DEXTERITY:    return _dex_modifier();
    default:
        mprf(MSGCH_ERROR, "Bad stat: %d", stat);
        return 0;
    }
}
