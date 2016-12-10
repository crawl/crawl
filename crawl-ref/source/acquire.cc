/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#include "AppHdr.h"

#include "acquire.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <set>

#include "artefact.h"
#include "art-enum.h"
#include "dungeon.h"
#include "food.h"
#include "god-item.h"
#include "god-passive.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "output.h"
#include "randbook.h"
#include "random.h"
#include "religion.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"

static equipment_type _acquirement_armour_slot(bool);
static armour_type _acquirement_armour_for_slot(equipment_type, bool);
static armour_type _acquirement_shield_type();
static armour_type _acquirement_body_armour(bool);
static armour_type _useless_armour_type();


/**
 * Get a randomly rounded value for the player's specified skill, unmodified
 * by crosstraining, draining, etc.
 *
 * @param skill     The skill in question; e.g. SK_ARMOUR.
 * @param mult      A multiplier to the skill, for higher precision.
 * @return          A rounded value of that skill; e.g. _skill_rdiv(SK_ARMOUR)
 *                  for a value of 10.1 will return 11 90% of the time &
 *                  10 the remainder.
 */
static int _skill_rdiv(skill_type skill, int mult = 1)
{
    const int scale = 256;
    return div_rand_round(you.skill(skill, mult * scale, true), scale);
}

/**
 * Choose a random subtype of armour to generate through acquirement/divine
 * gifts.
 *
 * Guaranteed to be wearable, in principle.
 *
 * @param divine    Lowers the odds of high-tier body armours being chosen.
 * @return          The armour_type of the armour to be generated.
 */
static int _acquirement_armour_subtype(bool divine, int & /*quantity*/)
{
    const equipment_type slot_type = _acquirement_armour_slot(divine);
    return _acquirement_armour_for_slot(slot_type, divine);
}

/**
 * Take a set of weighted elements and a filter, and return a random element
 * from those elements that fulfills the filter condition.
 *
 * @param weights       The elements to choose from.
 * @param filter        An optional filter; if present, only elements for which
 *                      the filter returns true may be chosen.
 * @return              A random element from the given list.
 */
template<class M, class Pred>
M filtered_vector_select(vector<pair<M, int>> weights, Pred filter)
{
    for (auto &weight : weights)
    {
        if (filter && !filter(weight.first))
            weight.second = 0;
        else
            weight.second = max(weight.second, 0); // cleanup
    }

    M *chosen_elem = random_choose_weighted(weights);
    ASSERT(chosen_elem);
    return *chosen_elem;
}

/**
 * Choose a random slot to acquire armour for.
 *
 * For most races, even odds for all armour slots when acquiring, or 50-50
 * split between body armour/aux armour when getting god gifts.
 *
 * Centaurs & nagas get a high extra chance for bardings, especially if they
 * haven't seen any yet.
 *
 * Guaranteed to be wearable, in principle.
 *
 * @param divine    Whether the item is a god gift.
 * @return          A random equipment slot; e.g. EQ_SHIELD, EQ_BODY_ARMOUR...
 */
static equipment_type _acquirement_armour_slot(bool divine)
{
    if (you.species == SP_NAGA || you.species == SP_CENTAUR)
    {
        const armour_type bard =
            (you.species == SP_NAGA) ? ARM_NAGA_BARDING
                                     : ARM_CENTAUR_BARDING;
        if (one_chance_in(you.seen_armour[bard] ? 4 : 2))
            return EQ_BOOTS;
    }

    vector<pair<equipment_type, int>> weights = {
        { EQ_BODY_ARMOUR,   divine ? 5 : 1 },
        { EQ_SHIELD,        1 },
        { EQ_CLOAK,         1 },
        { EQ_HELMET,        1 },
        { EQ_GLOVES,        1 },
        { EQ_BOOTS,         1 },
    };

    return filtered_vector_select(weights, [] (equipment_type etyp) {
        return you_can_wear(etyp); // evading template nonsense
    });
}


/**
 * Choose a random subtype of armour that will fit in the given equipment slot,
 * to generate through acquirement/divine gifts.
 *
 * Guaranteed to be usable by the player & weighted weakly by their skills;
 * heavy investment in armour skill, relative to dodging & spellcasting, makes
 * heavier armours more likely to be generated.
 *
 * @param divine    Whether the armour is a god gift.
 * @return          The armour_type of the armour to be generated.
 */
static armour_type _acquirement_armour_for_slot(equipment_type slot_type,
                                                bool divine)
{
    switch (slot_type)
    {
        case EQ_CLOAK:
            return ARM_CLOAK;
        case EQ_GLOVES:
            return ARM_GLOVES;
        case EQ_BOOTS:
            switch (you.species)
            {
                case SP_NAGA:
                    return ARM_NAGA_BARDING;
                case SP_CENTAUR:
                    return ARM_CENTAUR_BARDING;
                default:
                    return ARM_BOOTS;
            }
        case EQ_HELMET:
            if (you_can_wear(EQ_HELMET) == MB_TRUE)
                return random_choose(ARM_HELMET, ARM_HAT);
            return ARM_HAT;
        case EQ_SHIELD:
            return _acquirement_shield_type();
        case EQ_BODY_ARMOUR:
            return _acquirement_body_armour(divine);
        default:
            die("Unknown armour slot %d!", slot_type);
    }
}

/**
 * Choose a random type of shield to be generated via acquirement or god gifts.
 *
 * Weighted by Shields skill & the secret racial shield bonus.
 *
 * Ratios by shields skill & player size (B = buckler, S = shield, L = lg. sh.)
 *
 *     Shields    0           5         10          15        20
 * Large:   {6B}/5S/4L  ~{1B}/1S/1L  ~{1B}/5S/7L  ~2S/3L     1S/2L
 * Med.:        2B/1S    6B/4S/1L      2B/2S/1L   4B/8S/3L   1S/1L
 * Small:      ~3B/1S     ~5B/2S      ~2B/1S     ~3B/2S     ~1B/1S
 *
 * XXX: possibly shield skill should count for more for non-med races?
 *
 * @return A potentially wearable type of shield.
 */
static armour_type _acquirement_shield_type()
{
    const int scale = 256;
    vector<pair<armour_type, int>> weights = {
        { ARM_BUCKLER,       player_shield_racial_factor() * 4 * scale
                                - _skill_rdiv(SK_SHIELDS, scale) },
        { ARM_SHIELD,        10 * scale },
        { ARM_LARGE_SHIELD,  20 * scale
                             - player_shield_racial_factor() * 4 * scale
                             + _skill_rdiv(SK_SHIELDS, scale / 2) },
    };

    return filtered_vector_select(weights, [] (armour_type shtyp) {
        return check_armour_size(shtyp,  you.body_size(PSIZE_TORSO, true));
    });
}

/**
 * Determine the weight (likelihood) to acquire a specific type of body armour.
 *
 * If divine is set, returns the base weight for the armour type.
 * Otherwise, if warrior is set, multiplies the base weight by the base ac^2.
 * Otherwise, uses the player's Armour skill to crudely guess how likely they
 * are to want the armour, based on its EVP.
 *
 * @param armour    The type of armour in question. (E.g. ARM_ROBE.)
 * @param divine    Whether the 'acquirement' is actually a god gift.
 * @param warrior   Whether we think the player only cares about AC.
 * @return          A weight for the armour.
 */
static int _body_acquirement_weight(armour_type armour,
                                    bool divine, bool warrior)
{
    const int base_weight = armour_acq_weight(armour);
    if (divine)
        return base_weight; // gods don't care about your skills, apparently

    if (warrior)
    {
        const int ac = armour_prop(armour, PARM_AC);
        return base_weight * ac * ac;
    }

    // highest chance when armour skill = (displayed) evp - 3
    const int evp = armour_prop(armour, PARM_EVASION);
    const int skill = min(27, _skill_rdiv(SK_ARMOUR) + 3);
    const int sk_diff = skill + evp / 10;
    const int inv_diff = max(1, 27 - sk_diff);
    // armour closest to ideal evp is 27^3 times as likely as the furthest away
    return base_weight * inv_diff * inv_diff * inv_diff;
}

/**
 * Choose a random type of body armour to be generated via acquirement or
 * god gifts.
 *
 * @param divine      Whether the armour is a god gift.
 * @return A potentially wearable type of body armour..
 */
static armour_type _acquirement_body_armour(bool divine)
{
    // Using an arbitrary legacy formula, do we think the player doesn't care
    // about armour EVP?
    const bool warrior = random2(_skill_rdiv(SK_SPELLCASTING, 3)
                                + _skill_rdiv(SK_DODGING))
                         < random2(_skill_rdiv(SK_ARMOUR, 2));

    vector<pair<armour_type, int>> weights;
    for (int i = ARM_FIRST_MUNDANE_BODY; i < NUM_ARMOURS; ++i)
    {
        const armour_type armour = (armour_type)i;
        if (get_armour_slot(armour) != EQ_BODY_ARMOUR)
            continue;

        if (!check_armour_size(armour, you.body_size(PSIZE_TORSO, true)))
            continue;

        const int weight = _body_acquirement_weight(armour, divine, warrior);

        if (weight)
        {
            const pair<armour_type, int> weight_pair = { armour, weight };
            weights.push_back(weight_pair);
        }
    }

    const armour_type* armour_ptr = random_choose_weighted(weights);
    ASSERT(armour_ptr);
    return *armour_ptr;
}

/**
 * Choose a random type of armour that the player cannot wear, for Xom to spite
 * the player with.
 *
 * @return  A random useless armour_type.
 */
static armour_type _useless_armour_type()
{
    vector<pair<equipment_type, int>> weights = {
        { EQ_BODY_ARMOUR, 1 }, { EQ_SHIELD, 1 }, { EQ_CLOAK, 1 },
        { EQ_HELMET, 1 }, { EQ_GLOVES, 1 }, { EQ_BOOTS, 1 },
    };

    // everyone has some kind of boot-slot item they can't wear, regardless
    // of what you_can_wear() claims
    for (auto &weight : weights)
        if (you_can_wear(weight.first) == MB_TRUE && weight.first != EQ_BOOTS)
            weight.second = 0;

    const equipment_type* slot_ptr = random_choose_weighted(weights);
    const equipment_type slot = slot_ptr ? *slot_ptr : EQ_BOOTS;

    switch (slot)
    {
        case EQ_BOOTS:
            // Boots-wearers get bardings, bardings-wearers get the wrong
            // barding, everyone else gets boots.
            if (you_can_wear(EQ_BOOTS) == MB_TRUE)
                return random_choose(ARM_CENTAUR_BARDING, ARM_NAGA_BARDING);
            if (you.species == SP_NAGA)
                return ARM_CENTAUR_BARDING;
            if (you.species == SP_CENTAUR)
                return ARM_NAGA_BARDING;
            return ARM_BOOTS;
        case EQ_GLOVES:
            return ARM_GLOVES;
        case EQ_HELMET:
            if (you_can_wear(EQ_HELMET))
                return ARM_HELMET;
            return random_choose(ARM_HELMET, ARM_HAT);
        case EQ_CLOAK:
            return ARM_CLOAK;
        case EQ_SHIELD:
        {
            vector<pair<armour_type, int>> shield_weights = {
                { ARM_BUCKLER,       1 },
                { ARM_SHIELD,        1 },
                { ARM_LARGE_SHIELD,  1 },
            };

            return filtered_vector_select(shield_weights,
                                          [] (armour_type shtyp) {
                return !check_armour_size(shtyp,
                                          you.body_size(PSIZE_TORSO, true));
            });
        }
        case EQ_BODY_ARMOUR:
            // only the rarest & most precious of unwearable armours for Xom
            if (you_can_wear(EQ_BODY_ARMOUR))
                return ARM_CRYSTAL_PLATE_ARMOUR;
            // arbitrary selection of [unwearable] dragon armours
            return random_choose(ARM_FIRE_DRAGON_ARMOUR,
                                 ARM_ICE_DRAGON_ARMOUR,
                                 ARM_PEARL_DRAGON_ARMOUR,
                                 ARM_GOLD_DRAGON_ARMOUR,
                                 ARM_SHADOW_DRAGON_ARMOUR,
                                 ARM_STORM_DRAGON_ARMOUR);
        default:
            die("Unknown slot type selected for Xom bad-armour-acq!");
    }
}

static armour_type _pick_unseen_armour()
{
    // Consider shields uninteresting always, since unlike with other slots
    // players might well prefer an empty slot to wearing one. We don't
    // want to try to guess at this by looking at their weapon's handedness
    // because this would encourage switching weapons or putting on a
    // shield right before reading acquirement in some cases. --elliptic
    // This affects only the "unfilled slot" special-case, not regular
    // acquirement which can always produce (wearable) shields.
    static const equipment_type armour_slots[] =
        {  EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS  };

    armour_type picked = NUM_ARMOURS;
    int count = 0;
    for (auto &slot : armour_slots)
    {
        if (!you_can_wear(slot))
            continue;

        const armour_type sub_type = _acquirement_armour_for_slot(slot, false);
        ASSERT(sub_type != NUM_ARMOURS);

        if (!you.seen_armour[sub_type] && one_chance_in(++count))
            picked = sub_type;
    }

    return picked;
}

static int _acquirement_food_subtype(bool /*divine*/, int& quantity)
{
    int type_wanted;
    // Food is a little less predictable now. - bwr
    if (you.species == SP_GHOUL)
        type_wanted = FOOD_CHUNK;
    else if (you.species == SP_VAMPIRE)
    {
        // Vampires really don't want any OBJ_FOOD but OBJ_CORPSES
        // but it's easier to just give them a potion of blood
        // class type is set elsewhere
        type_wanted = POT_BLOOD;
    }
    else if (you_worship(GOD_FEDHAS))
    {
        // Fedhas worshippers get fruit to use for growth and evolution
        type_wanted = FOOD_FRUIT;
    }
    else
    {
        type_wanted = coinflip()
            ? FOOD_ROYAL_JELLY
            : player_mutation_level(MUT_HERBIVOROUS) ? FOOD_BREAD_RATION
                                                     : FOOD_MEAT_RATION;
    }

    quantity = 3 + random2(5);

    // giving more of the lower food value items
    if (type_wanted == FOOD_FRUIT)
        quantity = 8 + random2avg(15, 2);
    else if (type_wanted == FOOD_ROYAL_JELLY || type_wanted == FOOD_CHUNK)
        quantity += 2 + random2avg(10, 2);
    else if (type_wanted == POT_BLOOD)
        quantity = 8 + random2(5);

    return type_wanted;
}

/**
 * Randomly choose a class of weapons (those using a specific weapon skill)
 * for acquirement to give the player. Weight toward the player's skills.
 *
 * @param divine    Whether this is a god gift, which are less strongly
 *                  tailored to the player's skills.
 * @return          An appropriate weapon skill; e.g. SK_LONG_BLADES.
 */
static skill_type _acquirement_weapon_skill(bool divine)
{
    // reservoir sample.
    int count = 0;
    skill_type skill = SK_FIGHTING;
    for (skill_type sk = SK_FIRST_WEAPON; sk <= SK_LAST_WEAPON; ++sk)
    {
        // Adding a small constant allows for the occasional
        // weapon in an untrained skill.
        int weight = _skill_rdiv(sk) + 1;
        // Exaggerate the weighting if it's a scroll acquirement.
        if (!divine)
            weight = (weight + 1) * (weight + 2);
        count += weight;

        if (x_chance_in_y(weight, count))
            skill = sk;
    }

    return skill;
}

static int _acquirement_weapon_subtype(bool divine, int & /*quantity*/)
{
    const skill_type skill = _acquirement_weapon_skill(divine);

    int best_sk = 0;
    for (int i = SK_FIRST_WEAPON; i <= SK_LAST_WEAPON; i++)
        best_sk = max(best_sk, _skill_rdiv((skill_type)i));
    best_sk = max(best_sk, _skill_rdiv(SK_UNARMED_COMBAT));

    // Now choose a subtype which uses that skill.
    int result = OBJ_RANDOM;
    int count = 0;
    item_def item_considered;
    item_considered.base_type = OBJ_WEAPONS;
    // Let's guess the percentage of shield use the player did, this is
    // based on empirical data where pure-shield MDs get skills like 17 sh
    // 25 m&f and pure-shield Spriggans 7 sh 18 m&f. Pretend formicid
    // shield skill is 0 so they always weight towards 2H.
    const int shield_sk = you.species == SP_FORMICID
        ? 0
        : _skill_rdiv(SK_SHIELDS) * species_apt_factor(SK_SHIELDS);
    const int want_shield = min(2 * shield_sk, best_sk) + 10;
    const int dont_shield = max(best_sk - shield_sk, 0) + 10;
    // At XL 10, weapons of the handedness you want get weight *2, those of
    // opposite handedness 1/2, assuming your shields usage is respectively
    // 0% or 100% in the above formula. At skill 25 that's *3.5 .
    for (int i = 0; i < NUM_WEAPONS; ++i)
    {
        const int wskill = item_attack_skill(OBJ_WEAPONS, i);

        if (wskill != skill)
            continue;
        item_considered.sub_type = i;

        int acqweight = property(item_considered, PWPN_ACQ_WEIGHT) * 100;

        if (!acqweight)
            continue;

        const bool two_handed = you.hands_reqd(item_considered) == HANDS_TWO;

        if (two_handed && player_mutation_level(MUT_MISSING_HAND))
            continue;

        // For non-Trog/Okawaru acquirements, give a boost to high-end items.
        if (!divine && !is_range_weapon(item_considered))
        {
            if (acqweight < 500)
                acqweight = 500;
            // Quick blades get unproportionately hit by damage weighting.
            if (i == WPN_QUICK_BLADE)
                acqweight = acqweight * 25 / 9;
            int damage = property(item_considered, PWPN_DAMAGE);
            if (!two_handed)
                damage = damage * 3 / 2;
            damage *= damage * damage;
            acqweight *= damage / property(item_considered, PWPN_SPEED);
        }

        if (two_handed)
            acqweight = acqweight * dont_shield / want_shield;
        else
            acqweight = acqweight * want_shield / dont_shield;

        if (!you.seen_weapon[i])
            acqweight *= 5; // strong emphasis on type variety, brands go only second

        // reservoir sampling
        if (x_chance_in_y(acqweight, count += acqweight))
            result = i;
    }
    return result;
}

static int _acquirement_missile_subtype(bool /*divine*/, int & /*quantity*/)
{
    int count = 0;
    int skill = SK_THROWING;

    for (int i = SK_SLINGS; i <= SK_THROWING; i++)
    {
        const int sk = _skill_rdiv((skill_type)i);
        count += sk;
        if (x_chance_in_y(sk, count))
            skill = i;
    }

    missile_type result = MI_TOMAHAWK;

    switch (skill)
    {
    case SK_SLINGS:    result = MI_SLING_BULLET; break;
    case SK_BOWS:      result = MI_ARROW; break;
    case SK_CROSSBOWS: result = MI_BOLT; break;

    case SK_THROWING:
        {
            // Choose from among all usable missile types.
            vector<pair<missile_type, int> > missile_weights;

            missile_weights.emplace_back(MI_TOMAHAWK, 50);
            missile_weights.emplace_back(MI_NEEDLE, 75);

            if (you.body_size() >= SIZE_MEDIUM)
                missile_weights.emplace_back(MI_JAVELIN, 100);

            if (you.can_throw_large_rocks())
                missile_weights.emplace_back(MI_LARGE_ROCK, 100);

            result = *random_choose_weighted(missile_weights);
        }
        break;

    default:
        break;
    }
    return result;
}

static int _acquirement_jewellery_subtype(bool /*divine*/, int & /*quantity*/)
{
    int result = 0;

    // Rings are (number of usable rings) times as common as amulets.
    // XXX: unify this with the actual check for ring slots
    const int ring_num = (you.species == SP_OCTOPODE ? 8 : 2)
                       - (player_mutation_level(MUT_MISSING_HAND) ? 1 : 0);

    // Try ten times to give something the player hasn't seen.
    for (int i = 0; i < 10; i++)
    {
        result = one_chance_in(ring_num + 1) ? get_random_amulet_type()
                                             : get_random_ring_type();

        // If we haven't seen this yet, we're done.
        if (!get_ident_type(OBJ_JEWELLERY, result))
            break;
    }

    return result;
}


static int _acquirement_staff_subtype(bool /*divine*/, int & /*quantity*/)
{
    // Try to pick an enhancer staff matching the player's best skill.
    skill_type best_spell_skill = best_skill(SK_FIRST_MAGIC_SCHOOL,
                                             SK_LAST_MAGIC);
    bool found_enhancer = false;
    int result = 0;
    do
    {
        result = random2(NUM_STAVES);
    }
    while (item_type_removed(OBJ_STAVES, result));

    switch (best_spell_skill)
    {
#define TRY_GIVE(x) { if (!you.type_ids[OBJ_STAVES][x]) \
                      {result = x; found_enhancer = true;} }
    case SK_FIRE_MAGIC:   TRY_GIVE(STAFF_FIRE);        break;
    case SK_ICE_MAGIC:    TRY_GIVE(STAFF_COLD);        break;
    case SK_AIR_MAGIC:    TRY_GIVE(STAFF_AIR);         break;
    case SK_EARTH_MAGIC:  TRY_GIVE(STAFF_EARTH);       break;
    case SK_POISON_MAGIC: TRY_GIVE(STAFF_POISON);      break;
    case SK_NECROMANCY:   TRY_GIVE(STAFF_DEATH);       break;
    case SK_CONJURATIONS: TRY_GIVE(STAFF_CONJURATION); break;
    case SK_SUMMONINGS:   TRY_GIVE(STAFF_SUMMONING);   break;
#undef TRY_GIVE
    default:                                           break;
    }
    if (one_chance_in(found_enhancer ? 2 : 3))
        return result;

    // Otherwise pick a non-enhancer staff.
    switch (random2(5))
    {
    case 0: case 1: result = STAFF_WIZARDRY;   break;
    case 2: case 3: result = STAFF_ENERGY;     break;
    case 4: result = STAFF_POWER;              break;
    }
    return result;
}

static int _acquirement_rod_subtype(bool /*divine*/, int & /*quantity*/)
{
    int result;
    do
    {
        result = random2(NUM_RODS);
    }
    while (player_mutation_level(MUT_NO_LOVE) && result == ROD_SHADOWS
           || item_type_removed(OBJ_RODS, result));
    return result;
}

/**
 * Return a miscellaneous evokable item for acquirement.
 * @return   The item type chosen.
 */
static int _acquirement_misc_subtype(bool /*divine*/, int & /*quantity*/)
{
    // Give a crystal ball based on both evocations and either spellcasting or
    // invocations if we haven't seen one.
    int skills = _skill_rdiv(SK_EVOCATIONS)
        * max(_skill_rdiv(SK_SPELLCASTING), _skill_rdiv(SK_INVOCATIONS));
    if (x_chance_in_y(skills, MAX_SKILL_LEVEL * MAX_SKILL_LEVEL)
        && !you.seen_misc[MISC_CRYSTAL_BALL_OF_ENERGY])
    {
        return MISC_CRYSTAL_BALL_OF_ENERGY;
    }

    const bool NO_LOVE = player_mutation_level(MUT_NO_LOVE);

    const vector<pair<int, int> > choices =
    {
        // These have charges, so give them a constant weight.
        {MISC_BOX_OF_BEASTS,
                                       (NO_LOVE ?     0 :  7)},
        {MISC_SACK_OF_SPIDERS,
                                       (NO_LOVE ?     0 :  7)},
        {MISC_PHANTOM_MIRROR,
                                       (NO_LOVE ?     0 :  7)},
        // The player never needs more than one.
        {MISC_DISC_OF_STORMS,
            (you.seen_misc[MISC_DISC_OF_STORMS] ?     0 : 13)},
        {MISC_LAMP_OF_FIRE,
            (you.seen_misc[MISC_LAMP_OF_FIRE] ?       0 : 18)},
        {MISC_PHIAL_OF_FLOODS,
            (you.seen_misc[MISC_PHIAL_OF_FLOODS] ?    0 : 18)},
        {MISC_FAN_OF_GALES,
            (you.seen_misc[MISC_FAN_OF_GALES] ?       0 : 18)},
    };

    const int * const choice = random_choose_weighted(choices);

    // Could be nullptr if all the weights were 0.
    return choice ? *choice : MISC_CRYSTAL_BALL_OF_ENERGY;
}

/**
 * Choose a random type of wand to be generated via acquirement or god gifts.
 *
 * Heavily weighted toward more useful wands and wands the player hasn't yet
 * seen.
 *
 * @return          A random wand type.
 */
static int _acquirement_wand_subtype(bool /*divine*/, int & /*quantity*/)
{
    // basic total: 101
    vector<pair<wand_type, int>> weights = {
        { WAND_CLOUDS,          20 },
        { WAND_LIGHTNING,       16 },
        { WAND_ACID,            16 },
        { WAND_ICEBLAST,        16 },
        { WAND_DISINTEGRATION,  5 },
        { WAND_DIGGING,         5 },
        { WAND_POLYMORPH,       5 },
        { WAND_ENSLAVEMENT,     player_mutation_level(MUT_NO_LOVE) ? 0 : 5 },
        { WAND_PARALYSIS,       5 },
        { WAND_CONFUSION,       3 },
        { WAND_RANDOM_EFFECTS,  3 },
        { WAND_FLAME,           1 },
        { WAND_SLOWING,         1 },
    };

    // Unknown wands get a huge weight bonus.
    for (auto &weight : weights)
        if (!get_ident_type(OBJ_WANDS, weight.first))
            weight.second *= 2;

    const wand_type* wand = random_choose_weighted(weights);
    ASSERT(wand);
    return *wand;
}

typedef int (*acquirement_subtype_finder)(bool divine, int &quantity);
static const acquirement_subtype_finder _subtype_finders[] =
{
    _acquirement_weapon_subtype,
    _acquirement_missile_subtype,
    _acquirement_armour_subtype,
    _acquirement_wand_subtype,
    _acquirement_food_subtype,
    0, // no scrolls
    _acquirement_jewellery_subtype,
    _acquirement_food_subtype, // potion acquirement = food for vampires
    0, // books handled elsewhere
    _acquirement_staff_subtype,
    0, // no, you can't acquire the orb
    _acquirement_misc_subtype,
    0, // no corpses
    0, // gold handled elsewhere, and doesn't have subtypes anyway
    _acquirement_rod_subtype,
    0, // no runes either
};

static int _find_acquirement_subtype(object_class_type &class_wanted,
                                     int &quantity, bool divine,
                                     int agent)
{
    COMPILE_CHECK(ARRAYSZ(_subtype_finders) == NUM_OBJECT_CLASSES);
    ASSERT(class_wanted != OBJ_RANDOM);

    if (class_wanted == OBJ_ARMOUR && you.species == SP_FELID)
        return OBJ_RANDOM;

    int type_wanted = OBJ_RANDOM;

    int useless_count = 0;

    do
    {
        // Wands, rods, and misc have a common acquirement class.
        if (class_wanted == OBJ_MISCELLANY)
        {
            if (one_chance_in(8) && you.species != SP_FELID)
                class_wanted = OBJ_RODS;
            else
                class_wanted = random_choose(OBJ_WANDS, OBJ_MISCELLANY);
        }

        // Vampires acquire blood, not food.
        if (class_wanted == OBJ_FOOD && you.species == SP_VAMPIRE)
            class_wanted = OBJ_POTIONS;

        if (_subtype_finders[class_wanted])
            type_wanted = (*_subtype_finders[class_wanted])(divine, quantity);

        item_def dummy;
        dummy.base_type = class_wanted;
        dummy.sub_type = type_wanted;
        dummy.plus = 1; // empty wands would be useless
        dummy.flags |= ISFLAG_IDENT_MASK;

        if (!is_useless_item(dummy, false) && !god_hates_item(dummy)
            && (agent >= NUM_GODS || god_likes_item_type(dummy,
                                                         (god_type)agent)))
        {
            break;
        }
    }
    while (useless_count++ < 200);

    return type_wanted;
}

// The weight of a spell takes into account its disciplines' skill levels
// and the spell difficulty.
static int _spell_weight(spell_type spell)
{
    ASSERT(spell != SPELL_NO_SPELL);

    int weight = 0;
    spschools_type disciplines = get_spell_disciplines(spell);
    int count = 0;
    for (const auto disc : spschools_type::range())
    {
        if (disciplines & disc)
        {
            weight += _skill_rdiv(spell_type2skill(disc));
            count++;
        }
    }
    ASSERT(count > 0);

    // Particularly difficult spells _reduce_ the overall weight.
    int leveldiff = 5 - spell_difficulty(spell);

    return max(0, 2 * weight/count + leveldiff);
}

// When randomly picking a book for acquirement, use the sum of the
// weights of all unknown spells in the book.
static int _book_weight(book_type book)
{
    ASSERT_RANGE(book, 0, MAX_FIXED_BOOK + 1);

    int total_weight = 0;
    for (spell_type stype : spellbook_template(book))
    {
        // Skip over spells already seen.
        if (you.seen_spell[stype])
            continue;
        if (god_hates_spell(stype, you.religion))
            continue;

        total_weight += _spell_weight(stype);
    }

    return total_weight;
}

static bool _is_magic_skill(int skill)
{
    return skill >= SK_SPELLCASTING && skill < SK_INVOCATIONS;
}

static bool _skill_useless_with_god(int skill)
{
    switch (you.religion)
    {
    case GOD_TROG:
        return _is_magic_skill(skill) || skill == SK_INVOCATIONS;
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
        return skill == SK_NECROMANCY;
    case GOD_XOM:
    case GOD_NEMELEX_XOBEH:
    case GOD_KIKUBAAQUDGHA:
    case GOD_VEHUMET:
    case GOD_ASHENZARI:
    case GOD_JIYVA:
    case GOD_GOZAG:
    case GOD_NO_GOD:
        return skill == SK_INVOCATIONS;
    default:
        return false;
    }
}

/**
 * Randomly decide whether the player should get a manual from a given instance
 * of book acquirement.
 *
 * @param agent     The source of the acquirement (e.g. a god)
 * @return          Whether the player should get a manual from this book
 *                  acquirement.
 */
static bool _should_acquire_manual(int agent)
{
    // Manuals are too useful for Xom, and useless when gifted from Sif Muna.
    if (agent == GOD_XOM || agent == GOD_SIF_MUNA)
        return false;

    int magic_weights = 0;
    int other_weights = 0;

    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        const int weight = _skill_rdiv(sk);

        if (_is_magic_skill(sk))
            magic_weights += weight;
        else
            other_weights += weight;
    }

    if (you_worship(GOD_TROG))
        magic_weights = 0;

    // If someone has 25% or more magic skills, never give manuals.
    // Otherwise, count magic skills double to bias against manuals
    // for magic users.
    return magic_weights * 3 < other_weights
           && x_chance_in_y(other_weights, 2*magic_weights + other_weights);
}

/**
 * Turn a given book into an acquirement-quality manual.
 *
 * @param book[out]     The book to be turned into a manual.
 * @return              Whether a manual was successfully created.
 */
static bool _acquire_manual(item_def &book)
{
    int weights[NUM_SKILLS] = { 0 };
    int total_weights = 0;

    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
    {
        const int skl = _skill_rdiv(sk);

        if (skl == 27 || is_useless_skill(sk))
            continue;

        int w = (skl < 12) ? skl + 3 : max(0, 25 - skl);

        // Greatly reduce the chances of getting a manual for a skill
        // you couldn't use unless you switched your religion.
        if (_skill_useless_with_god(sk))
            w /= 2;

        weights[sk] = w;
        total_weights += w;
    }

    // Are we too skilled to get any manuals?
    if (total_weights == 0)
        return false;

    book.sub_type = BOOK_MANUAL;
    book.skill = static_cast<skill_type>(
                    choose_random_weighted(weights, end(weights)));
    // Set number of bonus skill points.
    book.skill_points = random_range(2000, 3000);
    return true;
}

static bool _do_book_acquirement(item_def &book, int agent)
{
    // items() shouldn't make book a randart for acquirement items.
    ASSERT(!is_random_artefact(book));

    if (_should_acquire_manual(agent))
        return _acquire_manual(book);
    const int choice = random_choose_weighted(
                                    30, BOOK_RANDART_THEME,
       agent == GOD_SIF_MUNA ? 10 : 40, NUM_BOOKS, // normal books
                                     1, BOOK_RANDART_LEVEL);

    switch (choice)
    {
    default:
    case NUM_BOOKS:
    {
        int total_weights = 0;

        // Pick a random spellbook according to unknown spells contained.
        int weights[MAX_FIXED_BOOK + 1] = { 0 };
        for (int bk = 0; bk <= MAX_FIXED_BOOK; bk++)
        {
            const auto bkt = static_cast<book_type>(bk);

            if (is_rare_book(bkt) && agent == GOD_SIF_MUNA
                || item_type_removed(OBJ_BOOKS, bk))
            {
                continue;
            }

            weights[bk]    = _book_weight(bkt);
            total_weights += weights[bk];
        }

        if (total_weights > 0)
        {
            book.sub_type = choose_random_weighted(weights, end(weights));
            break;
        }
        // else intentional fall-through
    }
    case BOOK_RANDART_THEME:
        acquire_themed_randbook(book, agent);
        break;

    case BOOK_RANDART_LEVEL:
    {
        const int level = agent == GOD_XOM ?
            random_range(1, 9) :
            max(1, (_skill_rdiv(SK_SPELLCASTING) + 2) / 3);

        book.sub_type  = BOOK_RANDART_LEVEL;
        if (!make_book_level_randart(book, level))
            return false;
        break;
    }
    } // switch book choice
    return true;
}

static int _failed_acquirement(bool quiet)
{
    if (!quiet)
        mpr("The demon of the infinite void smiles upon you.");
    return NON_ITEM;
}

static int _weapon_brand_quality(int brand, bool range)
{
    switch (brand)
    {
    case SPWPN_SPEED:
        return range ? 3 : 5;
    case SPWPN_PENETRATION:
        return 4;
    case SPWPN_ELECTROCUTION:
    case SPWPN_DISTORTION:
    case SPWPN_HOLY_WRATH:
    case SPWPN_REAPING:
        return 3;
    case SPWPN_CHAOS:
        return 2;
    default:
        return 1;
    case SPWPN_NORMAL:
        return 0;
    case SPWPN_PAIN:
        return _skill_rdiv(SK_NECROMANCY) / 2;
    case SPWPN_VORPAL:
        return range ? 5 : 1;
    }
}

static bool _armour_slot_seen(armour_type arm)
{
    item_def item;
    item.base_type = OBJ_ARMOUR;
    item.quantity = 1;

    for (int i = 0; i < NUM_ARMOURS; i++)
    {
        if (get_armour_slot(arm) != get_armour_slot((armour_type)i))
            continue;
        item.sub_type = i;

        // having seen a helmet means nothing about your decision to go
        // bare-headed if you have horns
        if (!can_wear_armour(item, false, true))
            continue;

        if (you.seen_armour[i])
            return true;
    }
    return false;
}

static bool _is_armour_plain(const item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    if (is_artefact(item))
        return false;

    if (armour_is_special(item))
    {
        // These are always interesting, even with no brand.
        // May still be redundant, but that has another check.
        return false;
    }

    return get_armour_ego_type(item) == SPARM_NORMAL;
}

/**
 * Has the player already encountered an item with this brand?
 *
 * Only supports weapons & armour.
 *
 * @param item      The item in question.
 * @param           Whether the player has encountered another weapon or
 *                  piece of armour with the same ego.
 */
static bool _brand_already_seen(const item_def &item)
{
    switch (item.base_type)
    {
        case OBJ_WEAPONS:
            return you.seen_weapon[item.sub_type]
                   & (1<<get_weapon_brand(item));
        case OBJ_ARMOUR:
            return you.seen_armour[item.sub_type]
                   & (1<<get_armour_ego_type(item));
        default:
            die("Unsupported item type!");
    }
}

// ugh
#define ITEM_LEVEL (divine ? ISPEC_GIFT : ISPEC_GOOD_ITEM)

/**
 * Take a newly-generated acquirement item, and adjust its brand if we don't
 * like it.
 *
 * Specifically, if we think the brand is too weak (for non-divine gifts), or
 * sometimes if we've seen the brand before.
 *
 * @param item      The item which may have its brand adjusted. Not necessarily
 *                  a weapon or piece of armour.
 * @param divine    Whether the item is a god gift, rather than from
 *                  acquirement proper.
 */
static void _adjust_brand(item_def &item, bool divine)
{
    if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_ARMOUR)
        return; // don't reroll missile brands, I guess

    if (is_artefact(item))
        return; // their own kettle of fish

    // Not from a god, so we should prefer better brands.
    if (!divine && item.base_type == OBJ_WEAPONS)
    {
        while (_weapon_brand_quality(get_weapon_brand(item),
                                     is_range_weapon(item)) < random2(6))
        {
            reroll_brand(item, ITEM_LEVEL);
        }
    }

    // Try to not generate brands that were already seen, although unlike
    // jewellery and books, this is not absolute.
    while (_brand_already_seen(item) && !one_chance_in(5))
        reroll_brand(item, ITEM_LEVEL);
}

/**
 * Should the given item be rejected as an acquirement/god gift result &
 * rerolled? If so, why?
 *
 * @param item      The item in question.
 * @param agent     The entity creating the item; possibly a god.
 * @return          A reason why the item should be rejected, if it should be;
 *                  otherwise, the empty string.
 */
static string _why_reject(const item_def &item, int agent)
{
    if (agent != GOD_XOM
        && (item.base_type == OBJ_WEAPONS
                && !can_wield(&item, false, true)
            || item.base_type == OBJ_ARMOUR
                && !can_wear_armour(item, false, true)))
    {
        return "Destroying unusable weapon or armour!";
    }

    // Trog does not gift the Wrath of Trog, nor weapons of pain
    // (which work together with Necromantic magic).
    // nor fancy magic staffs (wucad mu, majin-bo)
    if (agent == GOD_TROG
        && (get_weapon_brand(item) == SPWPN_PAIN
            || is_unrandom_artefact(item, UNRAND_TROG)
            || is_unrandom_artefact(item, UNRAND_WUCAD_MU)
            || is_unrandom_artefact(item, UNRAND_MAJIN)))
    {
        return "Destroying a weapon Trog hates!";
    }

    // Pain brand is useless if you've sacrificed Necromacy.
    if (player_mutation_level(MUT_NO_NECROMANCY_MAGIC)
        && get_weapon_brand(item) == SPWPN_PAIN)
    {
        return "Destroying pain weapon after Necro sac!";
    }

    // Sif Muna shouldn't gift special books.
    // (The spells therein are still fair game for randart books.)
    if (agent == GOD_SIF_MUNA
        && is_rare_book(static_cast<book_type>(item.sub_type)))
    {
        ASSERT(item.base_type == OBJ_BOOKS);
        return "Destroying sif-gifted rarebook!";
    }

    // The crystal ball case should be handled elsewhere, but just in
    // case, it's also handled here.
    if (agent == GOD_PAKELLAS)
    {
        if (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_CRYSTAL_BALL_OF_ENERGY)
        {
            return "Destroying CBoE that Pakellas hates!";
        }
    }

    return ""; // all OK
}

int acquirement_create_item(object_class_type class_wanted,
                            int agent, bool quiet,
                            const coord_def &pos, bool debug)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    const bool divine = (agent == GOD_OKAWARU || agent == GOD_XOM
                         || agent == GOD_TROG || agent == GOD_PAKELLAS);
    int thing_created = NON_ITEM;
    int quant = 1;
#define MAX_ACQ_TRIES 40
    for (int item_tries = 0; item_tries < MAX_ACQ_TRIES; item_tries++)
    {
        int type_wanted = -1;
        if (agent == GOD_XOM && class_wanted == OBJ_ARMOUR && one_chance_in(20))
            type_wanted = _useless_armour_type();
        else
        {
            // This may clobber class_wanted (e.g. staves/rods, or vampire food)
            type_wanted = _find_acquirement_subtype(class_wanted, quant,
                                                    divine, agent);
        }
        ASSERT(type_wanted != -1);

        // Don't generate randart books in items(), we do that
        // ourselves.
        bool want_arts = (class_wanted != OBJ_BOOKS);
        if (agent == GOD_TROG && !one_chance_in(3))
            want_arts = false;

        thing_created = items(want_arts, class_wanted, type_wanted,
                              ITEM_LEVEL, 0, agent);

        if (thing_created == NON_ITEM)
        {
            if (!quiet)
                dprf("Failed to make thing!");
            continue;
        }

        item_def &acq_item(mitm[thing_created]);
        _adjust_brand(acq_item, divine);

        // For plain armour, try to change the subtype to something
        // matching a currently unfilled equipment slot.
        if (acq_item.base_type == OBJ_ARMOUR && !is_artefact(acq_item))
        {
            const special_armour_type sparm = get_armour_ego_type(acq_item);

            if (agent != GOD_XOM
                && you.seen_armour[acq_item.sub_type] & (1 << sparm)
                && x_chance_in_y(MAX_ACQ_TRIES - item_tries, MAX_ACQ_TRIES + 5)
                || !divine
                && you.seen_armour[acq_item.sub_type]
                && !one_chance_in(3)
                && item_tries < 20)
            {
                // We have seen the exact item already, it's very unlikely
                // extras will do any good.
                // For scroll acquirement, prefer base items not seen before
                // as well, even if you didn't see the exact brand yet.
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                if (!quiet)
                    dprf("Destroying already-seen item!");
                continue;
            }

            // Try to fill empty slots.
            if ((_is_armour_plain(acq_item)
                 || get_armour_slot(acq_item) == EQ_BODY_ARMOUR && coinflip())
                && _armour_slot_seen((armour_type)acq_item.sub_type))
            {
                armour_type at = _pick_unseen_armour();
                if (at != NUM_ARMOURS)
                {
                    destroy_item(thing_created, true);
                    thing_created = items(true, OBJ_ARMOUR, at,
                                          ITEM_LEVEL, 0, agent);
                }
                else if (agent != GOD_XOM && one_chance_in(3))
                {
                    // If the item is plain and there aren't any
                    // unfilled slots, we might want to roll again.
                    destroy_item(thing_created, true);
                    thing_created = NON_ITEM;
                    if (!quiet)
                        dprf("Destroying plain item!");
                    continue;
                }
            }
        }

        if (agent == GOD_TROG && coinflip()
            && acq_item.base_type == OBJ_WEAPONS && !is_range_weapon(acq_item)
            && !is_unrandom_artefact(acq_item))
        {
            // ... but Trog loves the antimagic brand specially.
            set_item_ego_type(acq_item, OBJ_WEAPONS, SPWPN_ANTIMAGIC);
        }

        const string rejection_reason = _why_reject(acq_item, agent);
        if (!rejection_reason.empty())
        {
            if (!quiet)
                dprf("%s", rejection_reason.c_str());
            destroy_item(acq_item);
            thing_created = NON_ITEM;
            continue;
        }

        ASSERT(acq_item.is_valid());

        if (class_wanted == OBJ_WANDS)
            acq_item.plus = max(static_cast<int>(acq_item.plus), 3 + random2(3));
        else if (class_wanted == OBJ_GOLD)
        {
            // New gold acquirement formula from dpeg.
            // Min=220, Max=5520, Mean=1218, Std=911
            acq_item.quantity = 10 * (20
                                    + roll_dice(1, 20)
                                    + (roll_dice(1, 8)
                                       * roll_dice(1, 8)
                                       * roll_dice(1, 8)));
        }
        else if (class_wanted == OBJ_MISSILES && !divine)
            acq_item.quantity *= 5;
        else if (quant > 1)
            acq_item.quantity = quant;

        // Remove curse flag from item, unless worshipping Ashenzari.
        if (have_passive(passive_t::want_curses))
            do_curse_item(acq_item, true);
        else
            do_uncurse_item(acq_item);

        if (acq_item.base_type == OBJ_BOOKS)
        {
            if (!_do_book_acquirement(acq_item, agent))
            {
                destroy_item(acq_item, true);
                return _failed_acquirement(quiet);
            }
            // That might have changed the item's subtype.
            item_colour(acq_item);

            // Don't mark books as seen if only generated for the
            // acquirement statistics.
            if (!debug)
                mark_had_book(acq_item);
        }
        else if (acq_item.base_type == OBJ_JEWELLERY)
        {
            switch (acq_item.sub_type)
            {
            case RING_PROTECTION:
            case RING_STRENGTH:
            case RING_INTELLIGENCE:
            case RING_DEXTERITY:
            case RING_EVASION:
            case RING_SLAYING:
                // Make sure plus is >= 1.
                acq_item.plus = max(abs((int) acq_item.plus), 1);
                break;

            case RING_LOUDNESS:
            case RING_TELEPORTATION:
            case AMU_INACCURACY:
                // These are the only truly bad pieces of jewellery.
                if (!one_chance_in(9))
                    make_item_randart(acq_item);
                break;

            default:
                break;
            }

            // bump jewel acq power up a bit
            if (one_chance_in(2) && !is_artefact(acq_item))
                make_item_randart(acq_item);
        }
        else if (acq_item.base_type == OBJ_WEAPONS
                 && !is_unrandom_artefact(acq_item))
        {
            // These can never get egos, and mundane versions are quite common,
            // so guarantee artefact status. Rarity is a bit low to compensate.
            // ...except actually, trog can give them antimagic brand, so...
            if (is_giant_club_type(acq_item.sub_type)
                && get_weapon_brand(acq_item) == SPWPN_NORMAL
                && !one_chance_in(25))
            {
                make_item_randart(acq_item, true);
            }

            if (agent == GOD_TROG || agent == GOD_OKAWARU)
            {
                if (agent == GOD_TROG)
                    acq_item.plus += random2(3);

                // On a weapon, an enchantment of less than 0 is never viable.
                acq_item.plus = max(static_cast<int>(acq_item.plus), random2(2));
            }
        }

        // Last check: don't acquire items your god hates.
        // Temporarily mark the type as ID'd for the purpose of checking if
        // it is a hated brand (this addresses, e.g., Elyvilon followers
        // immediately identifying evil weapons).
        // Note that Xom will happily give useless items!
        int oldflags = acq_item.flags;
        acq_item.flags |= ISFLAG_KNOW_TYPE;
        if ((is_useless_item(acq_item, false) && agent != GOD_XOM)
            || god_hates_item(acq_item))
        {
            if (!quiet)
                dprf("destroying useless item");
            destroy_item(thing_created);
            thing_created = NON_ITEM;
            continue;
        }
        acq_item.flags = oldflags;
        break;
    }

    if (thing_created == NON_ITEM)
        return _failed_acquirement(quiet);

    ASSERT(!is_useless_item(mitm[thing_created], false) || agent == GOD_XOM);
    ASSERT(!god_hates_item(mitm[thing_created]));

    // Moving this above the move since it might not exist after falling.
    if (thing_created != NON_ITEM && !quiet)
        canned_msg(MSG_SOMETHING_APPEARS);

    // If a god wants to give you something but the floor doesn't want it,
    // it counts as a failed acquirement - no piety, etc cost.
    if (feat_destroys_items(grd(pos))
        && agent > GOD_NO_GOD
        && agent < NUM_GODS)
    {
        if (agent == GOD_XOM)
            simple_god_message(" snickers.", GOD_XOM);
        else
            return _failed_acquirement(quiet);
    }

    move_item_to_grid(&thing_created, pos);

    if (thing_created != NON_ITEM)
    {
        ASSERT(mitm[thing_created].is_valid());
        mitm[thing_created].props[ACQUIRE_KEY].get_int() = agent;
    }
    return thing_created;
}

bool acquirement(object_class_type class_wanted, int agent,
                 bool quiet, int* item_index, bool debug)
{
    ASSERT(!crawl_state.game_is_arena());

    FixedBitVector<NUM_OBJECT_CLASSES> bad_class;
    if (you.species == SP_FELID)
    {
        bad_class.set(OBJ_WEAPONS);
        bad_class.set(OBJ_MISSILES);
        bad_class.set(OBJ_ARMOUR);
        bad_class.set(OBJ_STAVES);
        bad_class.set(OBJ_RODS);
    }
    if (player_mutation_level(MUT_NO_ARTIFICE))
        bad_class.set(OBJ_MISCELLANY);

    bad_class.set(OBJ_FOOD, you_foodless_normally() && !you_worship(GOD_FEDHAS));

    static struct { object_class_type type; const char* name; } acq_classes[] =
    {
        { OBJ_WEAPONS,    "Weapon" },
        { OBJ_ARMOUR,     "Armour" },
        { OBJ_JEWELLERY,  "Jewellery" },
        { OBJ_BOOKS,      "Book" },
        { OBJ_STAVES,     "Staff " },
        { OBJ_MISCELLANY, "Evocables" },
        { OBJ_FOOD,       0 }, // amended below
        { OBJ_GOLD,       "Gold" },
    };
    ASSERT(acq_classes[6].type == OBJ_FOOD);
    acq_classes[6].name = you_worship(GOD_FEDHAS) ? "Fruit":
                          you.species == SP_VAMPIRE  ? "Blood":
                                                       "Food";

    int thing_created = NON_ITEM;

    if (item_index == nullptr)
        item_index = &thing_created;

    *item_index = NON_ITEM;

    while (class_wanted == OBJ_RANDOM)
    {
        ASSERT(!quiet);
        clear_messages();

        string line;
        for (unsigned int i = 0; i < ARRAYSZ(acq_classes); i++)
        {
            int len = max(strlen(acq_classes[i].name),
                          strlen(acq_classes[(i + ARRAYSZ(acq_classes) / 2)
                                             % ARRAYSZ(acq_classes)].name));
            if (bad_class[acq_classes[i].type])
                line += make_stringf("     %-*s", len, "");
            else
                line += make_stringf(" [%c] %-*s", i + 'a', len, acq_classes[i].name);

            if (i == ARRAYSZ(acq_classes) / 2 - 1 || i == ARRAYSZ(acq_classes) - 1)
            {
                line.erase(0, 1);
                mpr(line);
                line.clear();
            }
        }
        mprf(MSGCH_PROMPT, "What kind of item would you like to acquire? (\\ to view known items)");

        const int keyin = toalower(get_ch());
        if (keyin >= 'a' && keyin < 'a' + (int)ARRAYSZ(acq_classes))
            class_wanted = acq_classes[keyin - 'a'].type;
        else if (keyin == '\\')
            check_item_knowledge(), redraw_screen();
        else
        {
            // Lets wizards escape out of accidentally choosing acquirement.
            if (agent == AQ_WIZMODE)
            {
                canned_msg(MSG_OK);
                return false;
            }

            // If we've gotten a HUP signal then the player will be unable
            // to make a selection.
            if (crawl_state.seen_hups)
            {
                dprf("Acquirement interrupted by HUP signal.");
                you.turn_is_over = false;
                return false;
            }
        }

        if (class_wanted >= NUM_OBJECT_CLASSES || bad_class[class_wanted])
            class_wanted = OBJ_RANDOM;
    }

    *item_index = acquirement_create_item(class_wanted, agent, quiet,
                                          you.pos(), debug);
    ASSERT(*item_index == NON_ITEM || !god_hates_item(mitm[*item_index]));

    return true;
}
