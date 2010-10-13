/*
 * File:       exercise.cc
 * Summary:    Collects all calls to skills.cc:exercise for
 *             easier changes to the training modell.
 */

#include "AppHdr.h"

#include <algorithm>

#include "exercise.h"

#include "godconduct.h"
#include "itemprop.h"
#include "player.h"
#include "random.h"
#include "skills.h"
#include "spl-util.h"
#include "sprint.h"
#include "state.h"

static skill_type _abil_skill(ability_type abil)
{
    switch (abil)
    {
    case ABIL_EVOKE_TELEPORTATION:
    case ABIL_EVOKE_BLINK:
    case ABIL_EVOKE_BERSERK:
    case ABIL_EVOKE_TURN_INVISIBLE:
    case ABIL_EVOKE_LEVITATE:
        return (SK_EVOCATIONS);

    case ABIL_NEMELEX_DRAW_ONE:
    case ABIL_NEMELEX_PEEK_TWO:
    case ABIL_NEMELEX_TRIPLE_DRAW:
    case ABIL_NEMELEX_MARK_FOUR:
    case ABIL_NEMELEX_STACK_FIVE:
        return (SK_EVOCATIONS);

    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
    case ABIL_ELYVILON_LESSER_HEALING_SELF:
    case ABIL_ELYVILON_LESSER_HEALING_OTHERS:
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
    case ABIL_ZIN_RECITE:
    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
    case ABIL_OKAWARU_MIGHT:
    case ABIL_JIYVA_CALL_JELLY:
    case ABIL_ZIN_VITALISATION:
    case ABIL_TSO_DIVINE_SHIELD:
    case ABIL_KIKU_RECEIVE_CORPSES:
    case ABIL_BEOGH_SMITING:
    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
    case ABIL_ELYVILON_PURIFICATION:
    case ABIL_LUGONU_BEND_SPACE:
    case ABIL_FEDHAS_SUNLIGHT:
    case ABIL_FEDHAS_PLANT_RING:
    case ABIL_FEDHAS_RAIN:
    case ABIL_FEDHAS_SPAWN_SPORES:
    case ABIL_FEDHAS_EVOLUTION:
    case ABIL_CHEIBRIADOS_TIME_BEND:
    case ABIL_YRED_ANIMATE_REMAINS:
    case ABIL_YRED_ANIMATE_DEAD:
    case ABIL_YRED_DRAIN_LIFE:
    case ABIL_ZIN_IMPRISON:
    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
    case ABIL_ELYVILON_GREATER_HEALING_SELF:
    case ABIL_ELYVILON_GREATER_HEALING_OTHERS:
    case ABIL_LUGONU_BANISH:
    case ABIL_JIYVA_SLIMIFY:
    case ABIL_TSO_CLEANSING_FLAME:
    case ABIL_OKAWARU_HASTE:
    case ABIL_CHEIBRIADOS_SLOUCH:
    case ABIL_ELYVILON_RESTORATION:
    case ABIL_LUGONU_CORRUPT:
    case ABIL_JIYVA_CURE_BAD_MUTATION:
    case ABIL_CHEIBRIADOS_TIME_STEP:
    case ABIL_ZIN_SANCTUARY:
    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
    case ABIL_ELYVILON_DIVINE_VIGOUR:
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
    case ABIL_YRED_ENSLAVE_SOUL:
    case ABIL_LUGONU_ABYSS_EXIT:
        return (SK_INVOCATIONS);

    default:
        return (SK_NONE);
    }
}

static int _abil_degree(ability_type abil)
{
    switch (abil)
    {
    case ABIL_EVOKE_TELEPORTATION:
    case ABIL_EVOKE_BLINK:
    case ABIL_EVOKE_BERSERK:
    case ABIL_EVOKE_TURN_INVISIBLE:
    case ABIL_EVOKE_LEVITATE:
        return (1);

    case ABIL_NEMELEX_DRAW_ONE:
        return (1 + random2(2));
    case ABIL_NEMELEX_PEEK_TWO:
        return (2 + random2(2));
    case ABIL_NEMELEX_TRIPLE_DRAW:
        return (3 + random2(3));
    case ABIL_NEMELEX_MARK_FOUR:
        return (4 + random2(4));
    case ABIL_NEMELEX_STACK_FIVE:
        return (5 + random2(5));

    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
    case ABIL_ELYVILON_LESSER_HEALING_SELF:
    case ABIL_ELYVILON_LESSER_HEALING_OTHERS:
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
        return (1);
    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
    case ABIL_OKAWARU_MIGHT:
    case ABIL_JIYVA_CALL_JELLY:
        return (1 + random2(3));

    case ABIL_ZIN_RECITE:
        return (2);
    case ABIL_ZIN_VITALISATION:
    case ABIL_TSO_DIVINE_SHIELD:
    case ABIL_KIKU_RECEIVE_CORPSES:
    case ABIL_BEOGH_SMITING:
        return (2 + random2(2));
    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
    case ABIL_ELYVILON_PURIFICATION:
    case ABIL_LUGONU_BEND_SPACE:
    case ABIL_FEDHAS_SUNLIGHT:
    case ABIL_FEDHAS_PLANT_RING:
    case ABIL_FEDHAS_RAIN:
    case ABIL_FEDHAS_SPAWN_SPORES:
    case ABIL_FEDHAS_EVOLUTION:
    case ABIL_CHEIBRIADOS_TIME_BEND:
        return (2 + random2(3));
    case ABIL_YRED_ANIMATE_REMAINS:
    case ABIL_YRED_ANIMATE_DEAD:
    case ABIL_YRED_DRAIN_LIFE:
        return (2 + random2(4));

    case ABIL_ZIN_IMPRISON:
    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
    case ABIL_ELYVILON_GREATER_HEALING_SELF:
    case ABIL_ELYVILON_GREATER_HEALING_OTHERS:
    case ABIL_LUGONU_BANISH:
    case ABIL_JIYVA_SLIMIFY:
        return (3 + random2(5));
    case ABIL_TSO_CLEANSING_FLAME:
        return (3 + random2(6));
    case ABIL_OKAWARU_HASTE:
        return (3 + random2(7));

    case ABIL_CHEIBRIADOS_SLOUCH:
        return (4 + random2(4));
    case ABIL_ELYVILON_RESTORATION:
        return (4 + random2(6));

    case ABIL_LUGONU_CORRUPT:
    case ABIL_JIYVA_CURE_BAD_MUTATION:
    case ABIL_CHEIBRIADOS_TIME_STEP:
        return (5 + random2(5));
    case ABIL_ZIN_SANCTUARY:
        return (5 + random2(8));

    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
        return (6 + random2(6));
    case ABIL_ELYVILON_DIVINE_VIGOUR:
        return (6 + random2(10));

    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
    case ABIL_YRED_ENSLAVE_SOUL:
    case ABIL_LUGONU_ABYSS_EXIT:
        return (8 + random2(10));

    default:
        return (0);
    }
}

static void _exercise_spell(spell_type spell, bool success)
{
    // (!success) reduces skill increase for miscast spells
    int skill;
    int exer = 0;
    int workout = 0;

    unsigned int disciplines = get_spell_disciplines(spell);

    //jmf: evil evil evil -- exclude HOLY bit
    disciplines &= (~SPTYP_HOLY);

    int skillcount = count_bits( disciplines );

    if (!success)
        skillcount += 4 + random2(10);

    const int diff = spell_difficulty(spell);

    // Fill all disciplines into a vector, then shuffle the vector, and
    // exercise skills in that random order. That way, small xp pools
    // don't always train exclusively the first skill.
    std::vector<int> disc;
    for (int ndx = 0; ndx <= SPTYP_LAST_EXPONENT; ndx++)
    {
        if (!spell_typematch( spell, 1 << ndx ))
            continue;

        disc.push_back(ndx);
    }
    std::random_shuffle(disc.begin(), disc.end());

    for (unsigned int k = 0; k < disc.size(); ++k)
    {
        int ndx = disc[k];
        skill = spell_type2skill( 1 << ndx );
        workout = (random2(1 + diff) / skillcount);

        if (!one_chance_in(5))
            workout++;       // most recently, this was an automatic add {dlb}

        const int exercise_amount = exercise( skill, workout );
        exer      += exercise_amount;
    }

    /* ******************************************************************
       Other recent formulae for the above:

       * workout = random2(spell_difficulty(spell_ex)
       * (10 + (spell_difficulty(spell_ex) * 2 )) / 10 / spellsy + 1);

       * workout = spell_difficulty(spell_ex)
       * (15 + spell_difficulty(spell_ex)) / 15 / spellsy;

       spellcasting had also been generally exercised at the same time
       ****************************************************************** */

    exer += exercise(SK_SPELLCASTING, one_chance_in(3) ? 1
                                      : random2(1 + random2(diff)));

    // Avoid doubly rewarding spell practise in sprint
    // (by inflated XP and inflated piety gain)
    if (crawl_state.game_is_sprint())
        exer = sprint_modify_exp_inverse(exer);

    if (exer)
        did_god_conduct(DID_SPELL_PRACTISE, exer);
}

static bool _check_train_armour(int amount)
{
    if (const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false))
    {
        // XXX: animal skin; should be a better way to get at that.
        const int mass_base = 100;
        const int mass = std::max(item_mass(*armour) - mass_base, 0);
        if (x_chance_in_y(mass, 50 * you.skill(SK_ARMOUR)))
        {
            exercise(SK_ARMOUR, amount);
            return (true);
        }
    }
    return (false);
}

static bool _check_train_dodging(int amount)
{
    const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
    const int mass = armour? item_mass(*armour) : 0;
    if (!x_chance_in_y(mass, 800))
    {
        exercise(SK_DODGING, amount);
        return (true);
    }
    return (false);
}

static void _check_train_sneak(bool invis)
{
    const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
    const int armour_mass = body_armour? item_mass(*body_armour) : 0;
    if (!x_chance_in_y(armour_mass, 1000)
        && you.burden_state == BS_UNENCUMBERED
        && !you.attribute[ATTR_SHADOWS]
            // If invisible, training happens much more rarely.
        && (!invis && one_chance_in(25) || one_chance_in(100)))
    {
        exercise(SK_STEALTH, 1);
    }
}

static void _exercise_passive()
{
    if (one_chance_in(6) && _check_train_armour(1))
    {
        // Armour trained in check_train_armour
    }
    // Exercise stealth skill:
    else if (you.burden_state == BS_UNENCUMBERED
             && !you.berserk()
             && !you.attribute[ATTR_SHADOWS])
    {
        const item_def *body_armour = you.slot_item(EQ_BODY_ARMOUR, false);
        const int armour_mass = body_armour? item_mass(*body_armour) : 0;
        if (!x_chance_in_y(armour_mass, 1000)
            // Diminishing returns for stealth training by waiting.
            && you.skills[SK_STEALTH] <= 2 + random2(3)
            && one_chance_in(18))
        {
            exercise(SK_STEALTH, 1);
        }
    }
}

static void _exercise(skill_type sk, int degree, int limit)
{
    if (limit < 0 || you.skills[sk] < limit)
        exercise(sk, degree);
}

void practise(exer_type ex, int param1)
{
    skill_type sk = SK_NONE;
    int limit = -1;
    int deg = 0;

    switch (ex)
    {
    case EX_WILL_STAB:
        sk = SK_STABBING;
        deg = 1 + random2avg(5, 4);
        exercise(sk, deg);
        break;

    case EX_WILL_HIT_HELPLESS:
        limit = 1;
    case EX_WILL_HIT:
        sk = static_cast<skill_type>(param1);
        _exercise(sk, 1, limit);
        if (one_chance_in(3))
            _exercise(SK_FIGHTING, 1, limit);
        break;

    case EX_MONSTER_WILL_HIT:
        if (coinflip())
            _check_train_armour(coinflip() ? 2 : 1);
        break;

    case EX_MONSTER_MAY_HIT:
         _check_train_dodging(1);
         break;

    case EX_WILL_LAUNCH:
        sk = static_cast<skill_type>(param1);
        switch (sk)
        {
        case SK_SLINGS:
            deg = 1 + random2avg(3, 2);
            break;
        case SK_THROWING:
        case SK_BOWS:
        case SK_CROSSBOWS:
            deg = coinflip() ? 2 : 1;
            break;
        default:
            break;
        }
        exercise(sk, deg);
        break;

    case EX_WILL_THROW_MSL:
        switch (param1) // missile subtype
        {
        case MI_DART:
            deg = 1 + random2avg(3, 2) + coinflip();
            break;
        case MI_JAVELIN:
            deg = 1 + coinflip() + coinflip();
            break;
        case MI_THROWING_NET:
            deg = 1 + coinflip();
            break;
        default:
            deg = coinflip();
            break;
        }
        exercise(SK_THROWING, deg);
        break;

    case EX_WILL_THROW_WEAPON:
    case EX_WILL_THROW_POTION:
        if (coinflip())
            exercise(SK_THROWING, 1);
        break;

    case EX_WILL_THROW_OTHER:
        if (one_chance_in(20))
            exercise(SK_THROWING, 1);
        break;

    case EX_USED_ABIL:
    {
        ability_type abil = static_cast<ability_type>(param1);
        sk = _abil_skill(abil);
        deg = _abil_degree(abil);
        if (sk != SK_NONE)
            exercise(sk, deg);
        break;
    }

    case EX_DID_CAST:
    case EX_DID_MISCAST:
        _exercise_spell(static_cast<spell_type>(param1),
                        ex == EX_DID_CAST);
        break;

    case EX_WILL_READ_SCROLL:
        _exercise(SK_SPELLCASTING, coinflip() ? 2 : 1, 1);
        break;

    case EX_FOUND_SECRET_DOOR:
    case EX_TRAP_FOUND:
        exercise(SK_TRAPS_DOORS, 1 + random2(2));
        break;

    case EX_TRAP_PASSIVE:
        exercise(SK_TRAPS_DOORS, 3);
        break;

    case EX_TRAP_TRIGGER:
        exercise(SK_TRAPS_DOORS, 1 + random2(2));
        break;

    case EX_TRAP_DISARM:
        // param1 == you.absdepth0
        exercise(SK_TRAPS_DOORS, 1 + random2(5) + param1 / 5);
        break;

    case EX_TRAP_DISARM_FAIL:
        // param1 == you.absdepth0
        exercise(SK_TRAPS_DOORS, 1 + random2(param1 / 5));
        break;

    case EX_TRAP_DISARM_TRIGGER:
        if (coinflip())
            exercise(SK_TRAPS_DOORS, 1);
        break;

    case EX_REMOVE_NET:
        exercise(SK_TRAPS_DOORS, 1);
        break;

    case EX_SAGE:
        sk = static_cast<skill_type>(param1);
        exercise(sk, 20);
        break;

    case EX_READ_MANUAL:
        sk = static_cast<skill_type>(param1);
        exercise(sk, 500);
        break;

    case EX_SHIELD_BLOCK:
    case EX_SHIELD_TRAP:
        if (coinflip())
            exercise(SK_SHIELDS, 1);
        break;

    case EX_DODGE_TRAP:
        if (coinflip())
            _check_train_dodging(1);
        break;

    case EX_SHIELD_BEAM_FAIL:
        if (one_chance_in(6))
            exercise(SK_SHIELDS, 1);
        break;

    case EX_BEAM_MAY_HIT:
        if (coinflip())
            _check_train_dodging(1);
        break;

    case EX_BEAM_WILL_HIT:
        if (one_chance_in(4))
            _check_train_armour(1);
        break;

    case EX_SNEAK:
    case EX_SNEAK_INVIS:
        _check_train_sneak(ex == EX_SNEAK_INVIS);
        break;

    case EX_DID_EVOKE_ITEM:
        // XXX: degree determination is just passed in but
        // should be done here.
        exercise(SK_EVOCATIONS, param1);
        break;
    case EX_DID_ZAP_WAND:
    case EX_WILL_READ_TOME:
        exercise(SK_EVOCATIONS, 1);
        break;

    case EX_WAIT:
        _exercise_passive();
        break;
    default:
        break;
    }
}
