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

#include "ability.h"
#include "artefact.h"
#include "art-enum.h"
#include "colour.h"
#include "describe.h"
#include "dungeon.h"
#include "god-item.h"
#include "god-passive.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "item-use.h"
#include "invent.h"
#include "known-items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "notes.h"
#include "output.h"
#include "options.h"
#include "prompt.h"
#include "randbook.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "unwind.h"
#include "ui.h"

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
static int _acquirement_armour_subtype(bool divine, int & /*quantity*/,
                                       int /*agent*/)
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
template<class M>
M filtered_vector_select(vector<pair<M, int>> weights, function<bool(M)> filter)
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
 * Nagas get a high extra chance for bardings, especially if they haven't
 * seen any yet.
 *
 * Guaranteed to be wearable, in principle.
 *
 * @param divine    Whether the item is a god gift.
 * @return          A random equipment slot; e.g. EQ_SHIELD, EQ_BODY_ARMOUR...
 */
static equipment_type _acquirement_armour_slot(bool divine)
{
    if (you.wear_barding()
        && one_chance_in(you.seen_armour[ARM_BARDING] ? 4 : 2))
    {
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

    return filtered_vector_select<equipment_type>(weights,
        [] (equipment_type etyp) {
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
            if (you_can_wear(EQ_CLOAK) == MB_TRUE)
                return random_choose(ARM_CLOAK, ARM_SCARF);
            return ARM_SCARF;
        case EQ_GLOVES:
            return ARM_GLOVES;
        case EQ_BOOTS:
            if (you.wear_barding())
                return ARM_BARDING;
            else
                return ARM_BOOTS;
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
 * Ratios by shields skill & player size (B = buckler, K = kite shield, P = tower shield)
 *
 *     Shields    0           5         10          15        20
 * Large:   {6B}/5K/4P  ~{1B}/1K/1P  ~{1B}/5K/7P  ~2K/3P     1K/2P
 * Med.:        2B/1K    6B/4K/1P      2B/2K/1P   4B/8K/3P   1K/1P
 * Small:      ~3B/1K     ~5B/2K      ~2B/1K     ~3B/2K     ~1B/1K
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
        { ARM_KITE_SHIELD,        10 * scale },
        { ARM_TOWER_SHIELD,  20 * scale
                             - player_shield_racial_factor() * 4 * scale
                             + _skill_rdiv(SK_SHIELDS, scale / 2) },
    };

    return filtered_vector_select<armour_type>(weights, [] (armour_type shtyp) {
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
    int light_pref = _skill_rdiv(SK_SPELLCASTING, 3);
    light_pref += _skill_rdiv(SK_DODGING);
    light_pref = random2(light_pref);
    const bool warrior = light_pref < random2(_skill_rdiv(SK_ARMOUR, 2));

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
            // Boots-wearers get bardings, everyone else gets boots.
            if (you_can_wear(EQ_BOOTS) == MB_TRUE)
                return ARM_BARDING;
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
                { ARM_KITE_SHIELD,        1 },
                { ARM_TOWER_SHIELD,  1 },
            };

            return filtered_vector_select<armour_type>(shield_weights,
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

/**
 * Randomly choose a class of weapons (those using a specific weapon skill)
 * for acquirement to give the player. Weight toward the player's skills.
 *
 * @param divine    Whether this is a god gift, which are less strongly
 *                  tailored to the player's skills.
 * @return          An appropriate weapon skill; e.g. SK_LONG_BLADES.
 */
static skill_type _acquirement_weapon_skill(bool divine, int agent)
{
    // reservoir sample.
    int count = 0;
    skill_type skill = SK_FIGHTING;
    for (skill_type sk = SK_FIRST_WEAPON;
         sk <= (agent == GOD_TROG ? SK_LAST_MELEE_WEAPON : SK_LAST_WEAPON);
         ++sk)
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

static int _acquirement_weapon_subtype(bool divine, int & /*quantity*/, int agent)
{
    const skill_type skill = _acquirement_weapon_skill(divine, agent);

    int best_sk = 0;
    for (int i = SK_FIRST_WEAPON;
         i <= (agent == GOD_TROG ? SK_LAST_MELEE_WEAPON : SK_LAST_WEAPON);
         i++)
    {
        best_sk = max(best_sk, _skill_rdiv((skill_type)i));
    }
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

        if (two_handed && you.get_mutation_level(MUT_MISSING_HAND))
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

static int _acquirement_missile_subtype(bool /*divine*/, int & /*quantity*/,
                                        int /*agent*/)
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

    missile_type result = MI_BOOMERANG;

    switch (skill)
    {
    case SK_SLINGS:    result = MI_SLING_BULLET; break;
    case SK_BOWS:      result = MI_ARROW; break;
    case SK_CROSSBOWS: result = MI_BOLT; break;

    case SK_THROWING:
        {
            // Choose from among all usable missile types.
            vector<pair<missile_type, int> > missile_weights;

            missile_weights.emplace_back(MI_BOOMERANG, 50);
            missile_weights.emplace_back(MI_DART, 75);

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

static int _acquirement_jewellery_subtype(bool /*divine*/, int & /*quantity*/,
                                          int /*agent*/)
{
    int result = 0;

    // Rings are (number of usable rings) times as common as amulets.
    // XXX: unify this with the actual check for ring slots
    const int ring_num = (you.species == SP_OCTOPODE ? 8 : 2)
                       - (you.get_mutation_level(MUT_MISSING_HAND) ? 1 : 0);

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


static int _acquirement_staff_subtype(bool /*divine*/, int & /*quantity*/,
                                      int /*agent*/)
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
    return coinflip() ? STAFF_WIZARDRY : STAFF_ENERGY;
}

/**
 * Return a miscellaneous evokable item for acquirement.
 * @return   The item type chosen.
 */
static int _acquirement_misc_subtype(bool /*divine*/, int & /*quantity*/,
                                     int /*agent*/)
{
    const bool NO_LOVE = you.get_mutation_level(MUT_NO_LOVE);

    const vector<pair<int, int> > choices =
    {
        // The player never needs more than one of these.
        {MISC_BOX_OF_BEASTS,
            (NO_LOVE || you.seen_misc[MISC_BOX_OF_BEASTS] ? 0 : 20)},
        {MISC_PHANTOM_MIRROR,
            (NO_LOVE || you.seen_misc[MISC_PHANTOM_MIRROR] ? 0 : 20)},
        // Tremorstones are better for heavily armoured characters.
        {MISC_TIN_OF_TREMORSTONES,
            (you.seen_misc[MISC_TIN_OF_TREMORSTONES]
                     ? 0 : 5 + _skill_rdiv(SK_ARMOUR) / 3)},
        {MISC_LIGHTNING_ROD,
            (you.seen_misc[MISC_LIGHTNING_ROD]   ? 0 : 20)},
        {MISC_PHIAL_OF_FLOODS,
            (you.seen_misc[MISC_PHIAL_OF_FLOODS] ? 0 : 20)},
        {MISC_CONDENSER_VANE,
            (you.seen_misc[MISC_CONDENSER_VANE] ? 0 : 20)},
    };

    const int * const choice = random_choose_weighted(choices);

    // Possible for everything to be 0 weight - if so just give a random spare.
    if (choice == nullptr)
    {
        return random_choose(MISC_BOX_OF_BEASTS,
                             MISC_PHANTOM_MIRROR,
                             MISC_TIN_OF_TREMORSTONES,
                             MISC_LIGHTNING_ROD,
                             MISC_PHIAL_OF_FLOODS,
                             MISC_CONDENSER_VANE);
    }

    return *choice;
}

/**
 * Choose a random type of wand to be generated via acquirement or god gifts.
 *
 * Heavily weighted toward more useful wands and wands the player hasn't yet
 * seen.
 *
 * @return          A random wand type.
 */
static int _acquirement_wand_subtype(bool /*divine*/, int & /*quantity*/,
                                     int /*agent */)
{
    // basic total: 120
    vector<pair<wand_type, int>> weights = {
        { WAND_ACID,            18 },
        { WAND_ICEBLAST,        18 },
        { WAND_ENSLAVEMENT,     you.get_mutation_level(MUT_NO_LOVE) ? 0 : 8 },
        { WAND_PARALYSIS,       8 },
        { WAND_DISINTEGRATION,  5 },
        { WAND_POLYMORPH,       5 },
        { WAND_DIGGING,         5 },
        { WAND_RANDOM_EFFECTS,  2 },
        { WAND_FLAME,           1 },
    };

    // Unknown wands get a huge weight bonus.
    for (auto &weight : weights)
        if (!get_ident_type(OBJ_WANDS, weight.first))
            weight.second *= 2;

    const wand_type* wand = random_choose_weighted(weights);
    ASSERT(wand);
    return *wand;
}

static int _acquirement_book_subtype(bool /*divine*/, int & /*quantity*/,
                                     int /*agent*/)
{
    return BOOK_MINOR_MAGIC;
    //this gets overwritten later, but needs to be a sane value
    //or asserts will get set off
}

typedef int (*acquirement_subtype_finder)(bool divine, int &quantity, int agent);
static const acquirement_subtype_finder _subtype_finders[] =
{
    _acquirement_weapon_subtype,
    _acquirement_missile_subtype,
    _acquirement_armour_subtype,
    _acquirement_wand_subtype,
#if TAG_MAJOR_VERSION == 34
    0, // no food
#endif
    0, // no scrolls
    _acquirement_jewellery_subtype,
    0, // no potions
    _acquirement_book_subtype,
    _acquirement_staff_subtype,
    0, // no, you can't acquire the orb
    _acquirement_misc_subtype,
    0, // no corpses
    0, // gold handled elsewhere, and doesn't have subtypes anyway
#if TAG_MAJOR_VERSION == 34
    0, // no rods
#endif
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
        // Wands and misc have a common acquirement class.
        if (class_wanted == OBJ_MISCELLANY)
            class_wanted = random_choose(OBJ_WANDS, OBJ_MISCELLANY);

        if (_subtype_finders[class_wanted])
        {
            type_wanted =
                (*_subtype_finders[class_wanted])(divine, quantity, agent);
        }

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
        // Skip over spells already in library.
        if (you.spell_library[stype])
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
    if (skill == SK_INVOCATIONS)
    {
        // No active invocations, or uses a different skill.
        return invo_skill() != SK_INVOCATIONS
               || you_worship(GOD_XOM)
               || you_worship(GOD_VEHUMET)
               || you_worship(GOD_NO_GOD);
    }

    switch (you.religion)
    {
    case GOD_TROG:
        return _is_magic_skill(skill);
    case GOD_ZIN:
    case GOD_SHINING_ONE:
    case GOD_ELYVILON:
        return skill == SK_NECROMANCY;
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
        acquire_themed_randbook(book, agent);
        break;
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

    // If we couldn't make a useful book, try to make a manual instead.
    // We have to temporarily identify the book for this.
    if (agent != GOD_XOM && agent != GOD_SIF_MUNA)
    {
        bool useless = false;
        {
            unwind_var<iflags_t> oldflags{book.flags};
            book.flags |= ISFLAG_KNOW_TYPE;
            useless = is_useless_item(book);
        }
        if (useless)
        {
            destroy_item(book);
            book.base_type = OBJ_BOOKS;
            book.quantity = 1;
            return _acquire_manual(book);
        }
    }

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
 * Specifically, when any of:
 *   - The god doesn't like the brand (for divine gifts)
 *   - We think the brand is too weak (for non-divine gifts)
 *   - Sometimes if we've seen the brand before.
 *
 * @param item      The item which may have its brand adjusted. Not necessarily
 *                  a weapon or piece of armour.
 * @param divine    Whether the item is a god gift, rather than from
 *                  acquirement proper.
 * @param agent     The source of the acquirement. For god gifts, it's equal to
 *                  the god.
 */
static void _adjust_brand(item_def &item, bool divine, int agent)
{
    if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_ARMOUR)
        return; // don't reroll missile brands, I guess

    if (is_artefact(item))
        return; // their own kettle of fish


    // Trog has a restricted brand table.
    if (agent == GOD_TROG && item.base_type == OBJ_WEAPONS)
    {
        // 75% chance of a brand
        item.brand = random_choose(SPWPN_NORMAL, SPWPN_VORPAL,
                                   SPWPN_FLAMING, SPWPN_ANTIMAGIC);
        return;
    }

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

    // Trog does not gift the Wrath of Trog.
    if (agent == GOD_TROG && is_unrandom_artefact(item, UNRAND_TROG))
        return "Destroying Trog-gifted Wrath of Trog!";

    // Pain brand is useless if you've sacrificed Necromacy.
    if (you.get_mutation_level(MUT_NO_NECROMANCY_MAGIC)
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

    return ""; // all OK
}

int acquirement_create_item(object_class_type class_wanted,
                            int agent, bool quiet,
                            const coord_def &pos)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    const bool divine = (agent == GOD_OKAWARU || agent == GOD_XOM
                         || agent == GOD_TROG
#if TAG_MAJOR_VERSION == 34
                         || agent == GOD_PAKELLAS
#endif
                        );
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
            // This may clobber class_wanted (e.g. staves)
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
        _adjust_brand(acq_item, divine, agent);

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
            int quantity_rnd = roll_dice(1, 8); // ensure rnd sequence points
            quantity_rnd *= roll_dice(1, 8);
            quantity_rnd *= roll_dice(1, 8);
            acq_item.quantity = 10 * (20
                                    + roll_dice(1, 20)
                                    + quantity_rnd);
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

            case RING_ATTENTION:
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

            if (agent == GOD_TROG)
                acq_item.plus += random2(3);
            // God gifts (except Xom's) never have a negative enchantment
            if (divine && agent != GOD_XOM)
                acq_item.plus = max(static_cast<int>(acq_item.plus), 0);
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

    item_set_appearance(mitm[thing_created]); // cleanup

    if (thing_created != NON_ITEM)
    {
        ASSERT(mitm[thing_created].is_valid());
        mitm[thing_created].props[ACQUIRE_KEY].get_int() = agent;
    }

    ASSERT(!is_useless_item(mitm[thing_created], false) || agent == GOD_XOM);
    ASSERT(!god_hates_item(mitm[thing_created]));

    // If we have a zero coord_def, don't move the item to the grid. Used for
    // generating scroll of acquirement items.
    if (pos.origin())
        return thing_created;

    // Moving this above the move since it might not exist after falling.
    if (thing_created != NON_ITEM && !quiet)
        canned_msg(MSG_SOMETHING_APPEARS);

    // If a god wants to give you something but the floor doesn't want it,
    // it counts as a failed acquirement - no piety, etc cost.
    if (feat_destroys_items(grd(pos))
        && agent > GOD_NO_GOD
        && agent < NUM_GODS)
    {
        if (!quiet && agent == GOD_XOM)
            simple_god_message(" snickers.", GOD_XOM);
        else
            return _failed_acquirement(quiet);
    }

    move_item_to_grid(&thing_created, pos, quiet);

    return thing_created;
}

class AcquireMenu : public InvMenu
{
    friend class AcquireEntry;

    CrawlVector &acq_items;

    void init_entries();
    void update_help();
    bool acquire_selected();

    virtual bool process_key(int keyin) override;

public:
    AcquireMenu(CrawlVector &aitems);
};

class AcquireEntry : public InvEntry
{
    string get_text(bool need_cursor = false) const override
    {
        need_cursor = need_cursor && show_cursor;
        const colour_t keycol = LIGHTCYAN;
        const string keystr = colour_to_str(keycol);
        const string itemstr =
            colour_to_str(menu_colour(text, item_prefix(*item), tag));
        const string gold_text = item->base_type == OBJ_GOLD
            ? make_stringf(" (you have %d gold)", you.gold) : "";
        return make_stringf(" <%s>%c%c%c%c</%s><%s>%s%s</%s>",
                            keystr.c_str(),
                            hotkeys[0],
                            need_cursor ? '[' : ' ',
                            selected() ? '+' : '-',
                            need_cursor ? ']' : ' ',
                            keystr.c_str(),
                            itemstr.c_str(),
                            text.c_str(),
                            gold_text.c_str(),
                            itemstr.c_str());
    }

public:
    AcquireEntry(const item_def& i) : InvEntry(i)
    {
        show_background = false;
    }
};

AcquireMenu::AcquireMenu(CrawlVector &aitems)
    : InvMenu(MF_SINGLESELECT | MF_NO_SELECT_QTY | MF_QUIET_SELECT
              | MF_ALWAYS_SHOW_MORE | MF_ALLOW_FORMATTING),
      acq_items(aitems)
{
    menu_action = ACT_EXECUTE;
    set_flags(get_flags() & ~MF_USE_TWO_COLUMNS);

    set_tag("acquirement");

    init_entries();

    update_help();

    set_title("Choose an item to acquire.");
}

void AcquireMenu::init_entries()
{
    menu_letter ckey = 'a';
    for (item_def& item : acq_items)
    {
        auto newentry = make_unique<AcquireEntry>(item);
        newentry->hotkeys.clear();
        newentry->add_hotkey(ckey++);
        add_entry(move(newentry));
    }
}

static string _hyphenated_letters(int how_many, char first)
{
    string s = "<w>";
    s += first;
    s += "</w>";
    if (how_many > 1)
    {
        s += "-<w>";
        s += first + how_many - 1;
        s += "</w>";
    }
    return s;
}

void AcquireMenu::update_help()
{
    string top_line = string(80, ' ') + '\n';

    set_more(formatted_string::parse_string(top_line + make_stringf(
        //[!] acquire|examine item  [a-i] select item to acquire
        //[Esc/R-Click] exit
        "%s  [%s] %s\n"
        "[Esc/R-Click] exit",
        menu_action == ACT_EXECUTE ? "[<w>!</w>] <w>acquire</w>|examine items" :
                                     "[<w>!</w>] acquire|<w>examine</w> items",
        _hyphenated_letters(item_count(), 'a').c_str(),
        menu_action == ACT_EXECUTE ? "select item for acquirement"
                                   : "examine item")));
}

static void _create_acquirement_item(item_def &item)
{
    auto &acq_items = you.props[ACQUIRE_ITEMS_KEY].get_vector();

    // Now that we have a selection, mark any generated unrands as not having
    // been generated, so they go back in circulation. Exclude the selected
    // item from this, if it's an unrand.
    for (auto aitem : acq_items)
    {
        if (is_unrandom_artefact(aitem)
            && (!is_unrandom_artefact(item)
                || !is_unrandom_artefact(aitem, item.unrand_idx)))
        {
            set_unique_item_status(aitem, UNIQ_NOT_EXISTS);
        }
    }

    take_note(Note(NOTE_ACQUIRE_ITEM, 0, 0, item.name(DESC_A),
              origin_desc(item)));
    item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);

    set_ident_type(item, true);

    if (copy_item_to_grid(item, you.pos()))
        canned_msg(MSG_SOMETHING_APPEARS);
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    acq_items.clear();
    you.props.erase(ACQUIRE_ITEMS_KEY);
}

bool AcquireMenu::acquire_selected()
{
    vector<MenuEntry*> selected = selected_entries();
    ASSERT(selected.size() == 1);
    auto& entry = *selected[0];

    const string col = colour_to_str(channel_to_colour(MSGCH_PROMPT));
    update_help();
    const formatted_string old_more = more;
    more = formatted_string::parse_string(make_stringf(
               "<%s>Acquire %s? (%s/N)</%s>\n",
               col.c_str(),
               entry.text.c_str(),
               Options.easy_confirm == easy_confirm_type::none ? "Y" : "y",
               col.c_str()));
    more += old_more;
    update_more();

    if (!yesno(nullptr, true, 'n', false, false, true))
    {
        deselect_all();
        more = old_more;
        update_more();
        return true;
    }

    item_def &acq_item = *static_cast<item_def*>(entry.data);
    _create_acquirement_item(acq_item);

    return false;
}

bool AcquireMenu::process_key(int keyin)
{
    switch (keyin)
    {
    case '!':
    case '?':
        if (menu_action == ACT_EXECUTE)
            menu_action = ACT_EXAMINE;
        else
            menu_action = ACT_EXECUTE;
        update_help();
        update_more();
        return true;
    default:
        break;
    }

    if (keyin - 'a' >= 0 && keyin - 'a' < (int)items.size())
    {
        if (menu_action == ACT_EXAMINE)
        {
            // Use a copy to set flags that make the description better
            // See the similar code in shopping.cc for details about const
            // hygene
            item_def& item(*const_cast<item_def*>(dynamic_cast<AcquireEntry*>(
                items[letter_to_index(keyin)])->item));

            item.flags |= (ISFLAG_IDENT_MASK | ISFLAG_NOTED_ID
                           | ISFLAG_NOTED_GET);
            describe_item_popup(item);

            return true;
        }
        else
        {
            const unsigned int i = keyin - 'a';
            select_item_index(i, 1, false);
            return acquire_selected();
        }
    }

    const bool ret = InvMenu::process_key(keyin);
    auto selected = selected_entries();
    if (selected.size() == 1)
        return acquire_selected();
    else
        return ret;
}

static item_def _acquirement_item_def(object_class_type item_type)
{
    item_def item;

    const int item_index = acquirement_create_item(item_type, AQ_SCROLL, true);

    if (item_index != NON_ITEM)
    {
        ASSERT(!god_hates_item(mitm[item_index]));

        // We make a copy of the item def, but we don't keep the real item.
        item = mitm[item_index];
        set_ident_flags(item,
                // Act as if we've recieved this item already to prevent notes.
                ISFLAG_IDENT_MASK | ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
        destroy_item(item_index);
    }

    return item;
}

static void _make_acquirement_items()
{
    vector<object_class_type> rand_classes;

    if (you.species != SP_FELID)
    {
        rand_classes.emplace_back(OBJ_WEAPONS);
        rand_classes.emplace_back(OBJ_ARMOUR);
        rand_classes.emplace_back(OBJ_STAVES);
    }

    rand_classes.emplace_back(OBJ_JEWELLERY);
    rand_classes.emplace_back(OBJ_BOOKS);

    const int num_wanted = min(3, (int) rand_classes.size());
    shuffle_array(rand_classes);

    CrawlVector &acq_items = you.props[ACQUIRE_ITEMS_KEY].get_vector();
    acq_items.empty();

    // Generate item defs until we have enough, skipping any random classes
    // that fail to generate an item.
    for (auto obj_type : rand_classes)
    {
        if (acq_items.size() == num_wanted)
            break;

        auto item = _acquirement_item_def(obj_type);
        if (item.defined())
            acq_items.push_back(item);
    }

    // Gold is guaranteed.
    auto gold_item = _acquirement_item_def(OBJ_GOLD);
    if (gold_item.defined())
            acq_items.push_back(gold_item);
}

/*
 * Handle scroll of acquirement.
 *
 * Generate acquirement choices as items in a prop if these don't already exist
 * (because a scroll was read and canceled. Then either get the acquirement
 * choice from the c_choose_acquirement lua handler if one exists or present a
 * menu for the player to choose an item.
 *
 * returns True if the scroll was used, false if it was canceled.
*/
bool acquirement_menu()
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.props.exists(ACQUIRE_ITEMS_KEY))
        _make_acquirement_items();

    auto &acq_items = you.props[ACQUIRE_ITEMS_KEY].get_vector();

#ifdef CLUA_BINDINGS
    int index = 0;
    if (!clua.callfn("c_choose_acquirement", ">d", &index))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (index >= 1 && index <= acq_items.size())
    {
        _create_acquirement_item(acq_items[index - 1]);
        return true;
    }
#endif

    AcquireMenu acq_menu(acq_items);
    acq_menu.show();

    return !you.props.exists(ACQUIRE_ITEMS_KEY);
}
