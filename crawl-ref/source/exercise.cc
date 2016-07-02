/**
 * @file
 * @brief Collects all calls to skills.cc:exercise for
 *            easier changes to the training model.
**/

#include "AppHdr.h"

#include "exercise.h"

#include <algorithm>

#include "ability.h"
#include "godabil.h" // for USKAYAW_DID_DANCE_ACTION
#include "itemprop.h"
#include "skills.h"
#include "spl-util.h"

static void _exercise_spell(spell_type spell, bool success)
{
    // (!success) reduces skill increase for miscast spells
    int workout = 0;

    spschools_type disciplines = get_spell_disciplines(spell);

    int skillcount = count_bits(disciplines);

    if (!success)
        skillcount += 4 + random2(10);

    const int diff = spell_difficulty(spell);

    // Fill all disciplines into a vector, then shuffle the vector, and
    // exercise skills in that random order. That way, first skill don't
    // stay in the queue for a shorter time.
    bool conj = false;
    vector<skill_type> disc;
    for (const auto bit : spschools_type::range())
    {
        if (!spell_typematch(spell, bit))
            continue;

        skill_type skill = spell_type2skill(bit);
        if (skill == SK_CONJURATIONS)
            conj = true;

        disc.push_back(skill);
    }

    // We slow down the training of spells with conjurations.
    if (conj && !x_chance_in_y(skillcount, 4))
        return;

    shuffle_array(disc);

    for (const skill_type skill : disc)
    {
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

/**
 * A best-fit linear approximation of the old item mass equation for armour.
 *
 * @return 42 * EVP - 13, if the player is wearing body armour; else 0.
 */
static int _armour_mass()
{
    const int evp = you.unadjusted_body_armour_penalty();
    return max(0, 42 * evp - 13);
}

static bool _check_train_armour(int amount)
{
    const int mass_base = 100; // old animal skin mass
    if (x_chance_in_y(_armour_mass() - mass_base,
                      you.skill(SK_ARMOUR, 50)))
    {
        exercise(SK_ARMOUR, amount);
        return true;
    }
    return false;
}

static bool _check_train_dodging(int amount)
{
    if (!x_chance_in_y(_armour_mass(), 800))
    {
        exercise(SK_DODGING, amount);
        return true;
    }
    return false;
}

static void _check_train_sneak(bool invis)
{
    if (!x_chance_in_y(_armour_mass(), 1000)
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
        return; // Armour trained in check_train_armour

    if (you.berserk() || you.attribute[ATTR_SHADOWS])
        return;

    // Exercise stealth skill:
    if (!x_chance_in_y(_armour_mass(), 1000)
        // Diminishing returns for stealth training by waiting.
        && you.skills[SK_STEALTH] <= 2 + random2(3)
        && one_chance_in(15))
    {
        exercise(SK_STEALTH, 1);
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
        else if (coinflip())
            exercise(SK_FIGHTING, 1);
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
        deg = abil_skill_weight(abil);
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

    case EX_DID_USE_DECK:
        exercise(SK_INVOCATIONS, 1);
        break;

    default:
        break;
    }

    // Doing a second switch to reduce code duplication
    // If we did an action that's plausibly an attack, we might gain Uskayaw
    // piety, assuming monsters were also damaged.
    // We handle melee attacks in melee_attack::player_exercise_combat_skills()
    // because it only calls practice() sometimes.
    switch (ex)
    {
        case EX_DID_ZAP_WAND:
        case EX_DID_EVOKE_ITEM:
        case EX_DID_USE_DECK:
        case EX_DID_CAST:
        case EX_USED_ABIL:
        case EX_WILL_THROW_WEAPON:
        case EX_WILL_THROW_MSL:
        case EX_WILL_LAUNCH:
        case EX_WILL_STAB:
            you.props[USKAYAW_DID_DANCE_ACTION] = true;
        default:
            break;
    }
}
