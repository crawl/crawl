/**
 * @file
 * @brief Collects all calls to skills.cc:exercise for
 *            easier changes to the training model.
**/

#include "AppHdr.h"

#include <algorithm>

#include "exercise.h"

#include "itemprop.h"
#include "player.h"
#include "random.h"
#include "skills.h"
#include "spl-util.h"

skill_type abil_skill(ability_type abil)
{
    switch (abil)
    {
    case ABIL_EVOKE_TELEPORTATION:
    case ABIL_EVOKE_BLINK:
    case ABIL_EVOKE_BERSERK:
    case ABIL_EVOKE_TURN_INVISIBLE:
    case ABIL_EVOKE_FLIGHT:
    case ABIL_EVOKE_FOG:
    case ABIL_EVOKE_TELEPORT_CONTROL:
    case ABIL_EVOKE_JUMP:
        return SK_EVOCATIONS;

    case ABIL_NEMELEX_DRAW_ONE:
    case ABIL_NEMELEX_PEEK_TWO:
    case ABIL_NEMELEX_TRIPLE_DRAW:
    case ABIL_NEMELEX_DEAL_FOUR:
    case ABIL_NEMELEX_STACK_FIVE:
        return SK_EVOCATIONS;

    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
    case ABIL_ELYVILON_LESSER_HEALING_SELF:
    case ABIL_ELYVILON_LESSER_HEALING_OTHERS:
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
    case ABIL_ZIN_RECITE:
    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
    case ABIL_OKAWARU_HEROISM:
    case ABIL_JIYVA_CALL_JELLY:
    case ABIL_ZIN_VITALISATION:
    case ABIL_TSO_DIVINE_SHIELD:
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
    case ABIL_YRED_ANIMATE_REMAINS_OR_DEAD: // Placeholder.
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
    case ABIL_OKAWARU_FINESSE:
    case ABIL_CHEIBRIADOS_SLOUCH:
    case ABIL_LUGONU_CORRUPT:
    case ABIL_JIYVA_CURE_BAD_MUTATION:
    case ABIL_CHEIBRIADOS_TIME_STEP:
    case ABIL_ZIN_SANCTUARY:
    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
    case ABIL_ELYVILON_DIVINE_VIGOUR:
    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
    case ABIL_YRED_ENSLAVE_SOUL:
    case ABIL_LUGONU_ABYSS_EXIT:
    case ABIL_CHEIBRIADOS_DISTORTION:
        return SK_INVOCATIONS;

    case ABIL_KIKU_RECEIVE_CORPSES:
    case ABIL_KIKU_TORMENT:
        return SK_NECROMANCY;

    default:
        return SK_NONE;
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
    case ABIL_EVOKE_FLIGHT:
    case ABIL_EVOKE_FOG:
    case ABIL_EVOKE_TELEPORT_CONTROL:
    case ABIL_EVOKE_JUMP:
        return 1;

    case ABIL_NEMELEX_DRAW_ONE:
        return 1 + random2(2);
    case ABIL_NEMELEX_PEEK_TWO:
        return 2 + random2(2);
    case ABIL_NEMELEX_TRIPLE_DRAW:
        return 3 + random2(3);
    case ABIL_NEMELEX_DEAL_FOUR:
        return 4 + random2(4);
    case ABIL_NEMELEX_STACK_FIVE:
        return 5 + random2(5);

    case ABIL_YRED_RECALL_UNDEAD_SLAVES:
    case ABIL_MAKHLEB_MINOR_DESTRUCTION:
    case ABIL_ELYVILON_LESSER_HEALING_SELF:
    case ABIL_ELYVILON_LESSER_HEALING_OTHERS:
    case ABIL_BEOGH_RECALL_ORCISH_FOLLOWERS:
        return 1;
    case ABIL_SIF_MUNA_CHANNEL_ENERGY:
    case ABIL_OKAWARU_HEROISM:
    case ABIL_JIYVA_CALL_JELLY:
        return 1 + random2(3);

    case ABIL_ZIN_RECITE:
        return 2;
    case ABIL_ZIN_VITALISATION:
    case ABIL_TSO_DIVINE_SHIELD:
    case ABIL_KIKU_RECEIVE_CORPSES:
    case ABIL_BEOGH_SMITING:
        return 2 + random2(2);
    case ABIL_MAKHLEB_LESSER_SERVANT_OF_MAKHLEB:
    case ABIL_LUGONU_BEND_SPACE:
    case ABIL_FEDHAS_SUNLIGHT:
    case ABIL_FEDHAS_PLANT_RING:
    case ABIL_FEDHAS_RAIN:
    case ABIL_FEDHAS_SPAWN_SPORES:
    case ABIL_FEDHAS_EVOLUTION:
    case ABIL_CHEIBRIADOS_TIME_BEND:
        return 2 + random2(3);
    case ABIL_YRED_ANIMATE_REMAINS:
    case ABIL_YRED_ANIMATE_DEAD:
    case ABIL_YRED_DRAIN_LIFE:
        return 2 + random2(4);

    case ABIL_ZIN_IMPRISON:
    case ABIL_MAKHLEB_MAJOR_DESTRUCTION:
    case ABIL_ELYVILON_GREATER_HEALING_SELF:
    case ABIL_ELYVILON_GREATER_HEALING_OTHERS:
    case ABIL_LUGONU_BANISH:
    case ABIL_JIYVA_SLIMIFY:
    case ABIL_CHEIBRIADOS_DISTORTION:
        return 3 + random2(5);
    case ABIL_TSO_CLEANSING_FLAME:
        return 3 + random2(6);
    case ABIL_OKAWARU_FINESSE:
        return 3 + random2(7);

    case ABIL_CHEIBRIADOS_SLOUCH:
        return 4 + random2(4);
    case ABIL_ELYVILON_PURIFICATION:
        return 4 + random2(6);

    case ABIL_LUGONU_CORRUPT:
    case ABIL_JIYVA_CURE_BAD_MUTATION:
    case ABIL_CHEIBRIADOS_TIME_STEP:
    case ABIL_KIKU_TORMENT:
        return 5 + random2(5);
    case ABIL_ZIN_SANCTUARY:
        return 5 + random2(8);

    case ABIL_MAKHLEB_GREATER_SERVANT_OF_MAKHLEB:
        return 6 + random2(6);
    case ABIL_ELYVILON_DIVINE_VIGOUR:
        return 6 + random2(10);

    case ABIL_TSO_SUMMON_DIVINE_WARRIOR:
    case ABIL_YRED_ENSLAVE_SOUL:
    case ABIL_LUGONU_ABYSS_EXIT:
        return 8 + random2(10);

    default:
        return 0;
    }
}

static void _exercise_spell(spell_type spell, bool success)
{
    // (!success) reduces skill increase for miscast spells
    skill_type skill;
    int workout = 0;

    unsigned int disciplines = get_spell_disciplines(spell);

    int skillcount = count_bits(disciplines);

    if (!success)
        skillcount += 4 + random2(10);

    const int diff = spell_difficulty(spell);

    // Fill all disciplines into a vector, then shuffle the vector, and
    // exercise skills in that random order. That way, first skill don't
    // stay in the queue for a shorter time.
    bool conj = false;
    vector<skill_type> disc;
    for (int ndx = 0; ndx <= SPTYP_LAST_EXPONENT; ndx++)
    {
        if (!spell_typematch(spell, 1 << ndx))
            continue;

        skill = spell_type2skill(1 << ndx);
        if (skill == SK_CONJURATIONS)
            conj = true;

        disc.push_back(skill);
    }

    // We slow down the training of spells with conjurations.
    if (conj && !x_chance_in_y(skillcount, 4))
        return;

    shuffle_array(disc);

    for (unsigned int k = 0; k < disc.size(); ++k)
    {
        skill = disc[k];
        workout = (random2(1 + diff) / skillcount);

        if (!one_chance_in(5))
            workout++;       // most recently, this was an automatic add {dlb}

        exercise(skill, workout);
    }

    /* ******************************************************************
       Other recent formulae for the above:

       * workout = random2(spell_difficulty(spell_ex)
       * (10 + (spell_difficulty(spell_ex) * 2)) / 10 / spellsy + 1);

       * workout = spell_difficulty(spell_ex)
       * (15 + spell_difficulty(spell_ex)) / 15 / spellsy;

       spellcasting had also been generally exercised at the same time
       ****************************************************************** */

    exercise(SK_SPELLCASTING, 1 + random2(1 + diff) / skillcount);
}

static bool _check_train_armour(int amount)
{
    if (const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false))
    {
        // XXX: animal skin; should be a better way to get at that.
        const int mass_base = 100;
        const int mass = max(item_mass(*armour) - mass_base, 0);
        if (x_chance_in_y(mass, you.skill(SK_ARMOUR, 50)))
        {
            exercise(SK_ARMOUR, amount);
            return true;
        }
    }
    return false;
}

static bool _check_train_dodging(int amount)
{
    const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
    const int mass = armour? item_mass(*armour) : 0;
    if (!x_chance_in_y(mass, 800))
    {
        exercise(SK_DODGING, amount);
        return true;
    }
    return false;
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
            && one_chance_in(15))
        {
            exercise(SK_STEALTH, 1);
        }
    }
}

void practise(exer_type ex, int param1)
{
    skill_type sk = SK_NONE;
    int deg = 0;

    switch (ex)
    {
    case EX_WILL_STAB:
        sk = SK_STEALTH;
        deg = 1 + random2avg(5, 4);
        exercise(sk, deg);
        break;

    case EX_WILL_HIT:
        sk = static_cast<skill_type>(param1);
        exercise(sk, 1);
        if (coinflip())
            exercise(SK_FIGHTING, 1);
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
        case SK_THROWING: // Probably obsolete.
        case SK_BOWS:
        case SK_CROSSBOWS:
            deg = 1;
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
        case MI_TOMAHAWK:
        case MI_JAVELIN:
        case MI_THROWING_NET:
            deg = 1;
            break;
        default: // Throwing stones and large rocks.
            deg = coinflip();
            break;
        }
        exercise(SK_THROWING, deg);
        break;

    case EX_WILL_THROW_WEAPON:
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
        sk = abil_skill(abil);
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

    case EX_SHIELD_BLOCK:
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
