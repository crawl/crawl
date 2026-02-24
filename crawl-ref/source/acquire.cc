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
#include "database.h"
#include "describe.h"
#include "dungeon.h"
#include "english.h"
#include "god-abil.h"
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
#include "player-equip.h"
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
#include "tag-version.h"
#include "terrain.h"
#include "unwind.h"
#include "ui.h"

static equipment_slot _acquirement_armour_slot(int);
static armour_type _acquirement_armour_for_slot(equipment_slot);
static armour_type _acquirement_shield_type();
static armour_type _acquirement_body_armour();
static armour_type _useless_armour_type();
static bool _armour_slot_seen(equipment_slot);

/**
 * Get a randomly rounded value for the player's specified skill, unmodified
 * by crosstraining, draining, etc.
 *
 * @param skill     The skill in question; e.g. SK_ARMOUR.
 * @param mult      A multiplier to the skill, for higher precision.
 * @return          A rounded value of that skill; e.g. _skill_rdiv(SK_ARMOUR)
 *                  for a value of 10.9 will return 11 90% of the time &
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
 * @param agent     The source of the acquirement (e.g. a god)
 * @return          The armour_type of the armour to be generated.
 */
static int _acquirement_armour_subtype(int & /*quantity*/, int agent)
{
    const equipment_slot slot_type = _acquirement_armour_slot(agent);
    return _acquirement_armour_for_slot(slot_type);
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
 * Nagas and Anemocentaurs get a high extra chance for bardings, especially if
 * they haven't seen any yet.
 *
 * Guaranteed to be wearable, in principle.
 *
 * @param agent     The source of the acquirement (e.g. a god)
 * @return          A random equipment slot; e.g. SLOT_OFFHAND, SLOT_BODY_ARMOUR...
 */
static equipment_slot _acquirement_armour_slot(int agent)
{
    if (you.can_wear_barding()
        && one_chance_in(you.seen_armour[ARM_BARDING] ? 4 : 2))
    {
        return SLOT_BARDING;
    }

    vector<pair<equipment_slot, int>> weights = {
        { SLOT_BODY_ARMOUR,   (agent == GOD_OKAWARU ? 6 : 2) },
        { SLOT_OFFHAND,       1 },
        { SLOT_CLOAK,         (_armour_slot_seen(SLOT_CLOAK)   ? 1 : 3) },
        { SLOT_HELMET,        (_armour_slot_seen(SLOT_HELMET)  ? 1 : 3) },
        { SLOT_GLOVES,        (_armour_slot_seen(SLOT_GLOVES)  ? 1 : 3) },
        { SLOT_BOOTS,         (_armour_slot_seen(SLOT_BOOTS)   ? 1 : 3) },
        { SLOT_BARDING,       (_armour_slot_seen(SLOT_BARDING) ? 1 : 3) },
    };

    return filtered_vector_select<equipment_slot>(weights,
        [] (equipment_slot etyp) {
            // return true if the player can wear at least something in this
            // equipment type
            return you_can_wear(etyp) != false;
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
 * @return          The armour_type of the armour to be generated.
 */
static armour_type _acquirement_armour_for_slot(equipment_slot slot_type)
{
    switch (slot_type)
    {
        case SLOT_CLOAK:
            return random_choose(ARM_CLOAK, ARM_SCARF);
        case SLOT_GLOVES:
            return ARM_GLOVES;
        case SLOT_BOOTS:
            return ARM_BOOTS;
        case SLOT_BARDING:
            return ARM_BARDING;
        case SLOT_HELMET:
            if (you_can_wear(SLOT_HELMET))
                return random_choose(ARM_HELMET, ARM_HAT);
            return ARM_HAT;
        case SLOT_OFFHAND:
            return _acquirement_shield_type();
        case SLOT_BODY_ARMOUR:
            return _acquirement_body_armour();
        default:
            die("Unknown armour slot %d!", slot_type);
    }
}

/**
 * Choose a random type of shield to be generated via acquirement or god gifts.
 *
 * Weighted by Shields skill: at 0 skill orb/buckler/kite/tower are equally
 * likely, while at 27 skill you get 25% kite and 75% tower.
 *
 * @return A potentially wearable type of shield.
 */
static armour_type _acquirement_shield_type()
{
    vector<pair<armour_type, int>> weights = {
        { ARM_ORB,           27 - _skill_rdiv(SK_SHIELDS) },
        { ARM_BUCKLER,       27 - _skill_rdiv(SK_SHIELDS) },
        { ARM_KITE_SHIELD,   27},
        { ARM_TOWER_SHIELD,  27 + _skill_rdiv(SK_SHIELDS, 2) },
    };

    return filtered_vector_select<armour_type>(weights, [] (armour_type shtyp) {
        return check_armour_size(shtyp,  you.body_size(PSIZE_TORSO, true));
    });
}

/**
 * Determine the weight (likelihood) to acquire a specific type of body armour.
 *
 * Weighted by Armour skill.
 *
 * @param armour    The type of armour in question. (E.g. ARM_ROBE.)
 * @return          A weight for the armour.
 */
static int _body_acquirement_weight(armour_type armour)
{
    const int base_weight = armour_acq_weight(armour);
    const int ac = armour_prop(armour, PARM_AC);
    return base_weight +
            (_skill_rdiv(SK_ARMOUR) * _skill_rdiv(SK_ARMOUR) * ac * ac / 27);
}

/**
 * Choose a random type of body armour to be generated via acquirement or
 * god gifts.
 *
 * @return A potentially wearable type of body armour.
 */
static armour_type _acquirement_body_armour()
{
    vector<pair<armour_type, int>> weights;
    for (int i = ARM_FIRST_MUNDANE_BODY; i < NUM_ARMOURS; ++i)
    {
        const armour_type armour = (armour_type)i;
        if (get_armour_slot(armour) != SLOT_BODY_ARMOUR)
            continue;

        if (!check_armour_size(armour, you.body_size(PSIZE_TORSO, true)))
            continue;

        const int weight = _body_acquirement_weight(armour);

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
    vector<pair<equipment_slot, int>> weights = {
        { SLOT_BODY_ARMOUR, 1 }, { SLOT_OFFHAND, 1 }, { SLOT_CLOAK, 1 },
        { SLOT_HELMET, 1 }, { SLOT_GLOVES, 1 }, { SLOT_BOOTS, 1 },
        { SLOT_BARDING, 1},
    };

    for (auto &weight : weights)
        if (bool(you_can_wear(weight.first)))
            weight.second = 0;

    const equipment_slot* slot_ptr = random_choose_weighted(weights);
    const equipment_slot slot = slot_ptr ? *slot_ptr : SLOT_BARDING;

    switch (slot)
    {
        case SLOT_BOOTS:
            return ARM_BOOTS;
        case SLOT_BARDING:
            return ARM_BARDING;
        case SLOT_GLOVES:
            return ARM_GLOVES;
        case SLOT_HELMET:
            if (you_can_wear(SLOT_HELMET) != false)
                return ARM_HELMET;
            return random_choose(ARM_HELMET, ARM_HAT);
        case SLOT_CLOAK:
            return random_choose(ARM_CLOAK, ARM_SCARF);
        case SLOT_OFFHAND:
        {
            vector<pair<armour_type, int>> shield_weights = {
                { ARM_BUCKLER,       1 },
                { ARM_KITE_SHIELD,   1 },
                { ARM_TOWER_SHIELD,  1 },
            };

            return filtered_vector_select<armour_type>(shield_weights,
                                          [] (armour_type shtyp) {
                return !check_armour_size(shtyp,
                                          you.body_size(PSIZE_TORSO, true));
            });
        }
        case SLOT_BODY_ARMOUR:
            // only the rarest & most precious of unwearable armours for Xom
            if (you_can_wear(SLOT_BODY_ARMOUR) != false)
                return ARM_CRYSTAL_PLATE_ARMOUR;
            // arbitrary selection of [unwearable] dragon armours
            return random_choose(ARM_FIRE_DRAGON_ARMOUR,
                                 ARM_ICE_DRAGON_ARMOUR,
                                 ARM_PEARL_DRAGON_ARMOUR,
                                 ARM_GOLDEN_DRAGON_ARMOUR,
                                 ARM_SHADOW_DRAGON_ARMOUR,
                                 ARM_STORM_DRAGON_ARMOUR);
        default:
            die("Unknown slot type selected for Xom bad-armour-acq!");
    }
}

static bool _regular_staves_useless()
{
    // stave OBJ_WEAPONS are useless to regular size chars with a missing hand,
    // but we don't want to mark the skill as useless in general, because it
    // also applies to OBJ_STAVES, which are one-handed. Also a bunch of
    // unrands, which this code probably prevents from generating for this
    // case.
    if (!you.has_mutation(MUT_MISSING_HAND))
        return false;

    item_def item_considered;
    item_considered.base_type = OBJ_WEAPONS;

    // try to find a useful regular staff
    for (int i = 0; i < NUM_WEAPONS; ++i)
    {
        // ignore non-staves
        if (i == WPN_STAFF)
            continue;

        item_considered.sub_type = i;
        if (item_attack_skill(OBJ_WEAPONS, i) == SK_STAVES
            && you.hands_reqd(item_considered) != HANDS_TWO)
        {
            // found something!
            return false;
        }
    }
    return true;
}

/**
 * Randomly choose a class of weapons (those using a specific weapon skill)
 * for acquirement to give the player. Weight toward the player's skills.
 *
 * @param agent     The source of the acquirement (e.g. a god)
 * @return          An appropriate weapon skill; e.g. SK_LONG_BLADES.
 */
static skill_type _acquirement_weapon_skill(int agent)
{
    // reservoir sample.
    int count = 0;
    skill_type skill = SK_FIGHTING;
    for (skill_type sk = SK_FIRST_WEAPON;
         sk <= (agent == GOD_TROG ? SK_LAST_MELEE_WEAPON : SK_LAST_WEAPON);
         ++sk)
    {
        // Don't choose a skill that's useless
        if (is_useless_skill(sk))
            continue;

        if (sk == SK_STAVES && _regular_staves_useless())
            continue;

        // Adding a small constant allows for the occasional
        // weapon in an untrained skill.
        int weight = _skill_rdiv(sk) + 1;
        // Exaggerate the weighting if it's not a Trog gift.
        if (agent != GOD_TROG)
            weight = (weight + 1) * (weight + 2);
        count += weight;

        if (x_chance_in_y(weight, count))
            skill = sk;
    }

    return skill;
}

static int _acquirement_weapon_subtype(int & /*quantity*/, int agent)
{
    const skill_type skill = _acquirement_weapon_skill(agent);

    // Now choose a subtype which uses that skill.
    int result = OBJ_RANDOM;
    int count = 0;
    item_def item_considered;
    item_considered.base_type = OBJ_WEAPONS;
    for (int i = 0; i < NUM_WEAPONS; ++i)
    {
        const int wskill = item_attack_skill(OBJ_WEAPONS, i);

        if (wskill != skill)
            continue;
        item_considered.sub_type = i;

        int acqweight = property(item_considered, PWPN_ACQ_WEIGHT) * 100;

        // Smaller species missing a hand can acquire polearms with default weight
        // zero, namely spears, since they have no other polearm option.
        if (skill == SK_POLEARMS && you.has_mutation(MUT_MISSING_HAND)
            && you.body_size() < SIZE_MEDIUM && !acqweight)
        {
            acqweight = 100;
        }

        if (!acqweight)
            continue;

        const bool two_handed = you.hands_reqd(item_considered) == HANDS_TWO;

        if (two_handed && you.get_mutation_level(MUT_MISSING_HAND))
            continue;

        // Give two-handers somewhat less frequently to characters with
        // Shields skill and very rarely to Coglins.
        if (you.has_mutation(MUT_WIELD_OFFHAND))
        {
            if (two_handed)
                acqweight /= 10;
        }
        else if (two_handed)
            acqweight = acqweight * (54 - _skill_rdiv(SK_SHIELDS)) / 54;
        else
            acqweight = acqweight * (54 + _skill_rdiv(SK_SHIELDS)) / 54;

        if (!you.seen_weapon[i])
            acqweight *= 5; // strong emphasis on type variety, brands go only second

        // reservoir sampling
        if (x_chance_in_y(acqweight, count += acqweight))
            result = i;
    }
    ASSERT(result != OBJ_RANDOM); // make sure loop ran at least once
    return result;
}

static int _acquirement_missile_subtype(int & /*quantity*/,
                                        int /*agent*/)
{
    // Choose from among all usable missile types.
    vector<pair<missile_type, int> > missile_weights;

    missile_weights.emplace_back(MI_BOOMERANG, 50);
    missile_weights.emplace_back(MI_DART, 75);

    if (you.body_size() >= SIZE_MEDIUM)
        missile_weights.emplace_back(MI_JAVELIN, 100);

    if (you.can_throw_large_rocks())
        missile_weights.emplace_back(MI_LARGE_ROCK, 100);

    return *random_choose_weighted(missile_weights);
}

static int _acquirement_jewellery_subtype(int & /*quantity*/,
                                          int /*agent*/)
{
    int result = 0;

    // Rings are (number of usable rings) times as common as amulets.
    const int ring_num = you.equipment.num_slots[SLOT_RING];

    // Try ten times to give something the player hasn't seen.
    for (int i = 0; i < 10; i++)
    {
        result = one_chance_in(ring_num + 1) ? get_random_amulet_type()
                                             : get_random_ring_type();

        // If we haven't seen this yet, we're done.
        if (!type_is_identified(OBJ_JEWELLERY, result))
            break;
    }

    return result;
}

static vector<pair<stave_type, int>> _base_staff_weights()
{
    // Small chance to pick a totally random staff, independent of skill.
    // For some reason.
    vector<pair<stave_type, int>> weights = {{ NUM_STAVES, 5 }};
    for (int i = 0; i < NUM_STAVES; i++)
    {
        stave_type staff = static_cast<stave_type>(i);
        if (!item_type_removed(OBJ_STAVES, staff))
            weights.push_back({staff, _skill_rdiv(staff_skill(staff))});
    }
    return weights;
}

// return true if there are any non-id'd staves found
static bool _remove_ided_staff_weights(vector<pair<stave_type, int>> &weights)
{
    bool found = false;
    for (auto &weight : weights)
    {
        // leave random weight untouched
        if (weight.first == NUM_STAVES)
            continue;

        if (type_is_identified(OBJ_STAVES, weight.first))
            weight.second = 0;
        else
            found = true;
    }
    return found;
}

static int _acquirement_staff_subtype(int & /*quantity*/,
                                      int /*agent*/)
{
    vector<pair<stave_type, int>> weights = _base_staff_weights();
    _remove_ided_staff_weights(weights);

    stave_type staff = *random_choose_weighted(weights);

    // chance to choose randomly, goes to 100% if all staves are id'd or 0
    // skill. Just brute force it.
    if (staff == NUM_STAVES)
    {
        do
        {
            staff = static_cast<stave_type>(random2(NUM_STAVES));
        }
        while (item_type_removed(OBJ_STAVES, staff));
    }

    return staff;
}

static const vector<pair<misc_item_type, int> > _misc_base_weights()
{
    const bool no_allies = you.allies_forbidden();
    vector<pair<misc_item_type, int> > choices =
    {
        {MISC_BOX_OF_BEASTS,       (no_allies ? 0 : 20)},
        {MISC_SACK_OF_SPIDERS,     (no_allies ? 0 : 20)},
        {MISC_PHANTOM_MIRROR,      (no_allies ? 0 : 20)},
        // Tremorstones are better for heavily armoured characters.
        {MISC_TIN_OF_TREMORSTONES, 5 + _skill_rdiv(SK_ARMOUR) / 3},
        // everything else is evenly weighted
        {MISC_LIGHTNING_ROD,       20},
        {MISC_PHIAL_OF_FLOODS,     20},
        {MISC_CONDENSER_VANE,      20},
        {MISC_GRAVITAMBOURINE,     20},
    };
    // The player never needs more than one of any of these.
    for (auto &p : choices)
        if (you.seen_misc[p.first])
            p.second = 0;
    return choices;
}

static bool _unided_acq_misc()
{
    const vector<pair<misc_item_type, int> > choices = _misc_base_weights();
    for (auto &p : choices)
        if (p.second > 0)
            return true;
    return false;
}

/**
 * Return a miscellaneous evokable item for acquirement.
 * @return   The item type chosen.
 */
static int _acquirement_misc_subtype(int & /*quantity*/,
                                     int /*agent*/)
{
    const vector<pair<misc_item_type, int> > choices = _misc_base_weights();
    const misc_item_type * const choice = random_choose_weighted(choices);

    // Possible for everything to be 0 weight - if so just give a random spare.
    // should not be used in normal acquirement..
    if (choice == nullptr)
    {
        return random_choose(MISC_BOX_OF_BEASTS,
                             MISC_SACK_OF_SPIDERS,
                             MISC_PHANTOM_MIRROR,
                             MISC_TIN_OF_TREMORSTONES,
                             MISC_LIGHTNING_ROD,
                             MISC_PHIAL_OF_FLOODS,
                             MISC_CONDENSER_VANE,
                             MISC_GRAVITAMBOURINE);
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
static int _acquirement_wand_subtype(int & /*quantity*/,
                                     int /*agent */)
{
    const auto hex_wand_type = (wand_type)item_for_set(ITEM_SET_HEX_WANDS);
    const auto beam_wand_type = (wand_type)item_for_set(ITEM_SET_BEAM_WANDS);
    const auto blast_wand_type = (wand_type)item_for_set(ITEM_SET_BLAST_WANDS);
    const int hex_wand_weight = hex_wand_type == WAND_CHARMING
                                && you.allies_forbidden() ? 0 : 20;
    vector<pair<wand_type, int>> weights = {
        { beam_wand_type, 20 },
        { blast_wand_type, 20 },
        { hex_wand_type,  hex_wand_weight },
        { WAND_MINDBURST, 8 },
        { WAND_POLYMORPH, 5 },
        { WAND_DIGGING,   5 },
        { WAND_FLAME,     2 },
    };

    // Unknown wands get a huge weight bonus.
    for (auto &weight : weights)
        if (!type_is_identified(OBJ_WANDS, weight.first))
            weight.second *= 2;

    const wand_type* wand = random_choose_weighted(weights);
    ASSERT(wand);
    return *wand;
}

static int _acquirement_book_subtype(int & /*quantity*/,
                                     int /*agent*/)
{
    return BOOK_MINOR_MAGIC;
    //this gets overwritten later, but needs to be a sane value
    //or asserts will get set off
}

// Scale talisman chances, strongly biased in favour of those we haven't
// seen before, and also biased in favour of higher tier talismans when
// we have more Shapeshifting skill.
static vector<pair<talisman_type, int>> _scale_talisman_weights(int agent)
{
    // Xom always selects a talisman purely at random.
    if (agent == GOD_XOM)
        return {{NUM_TALISMANS, 1000}};

    // This will produce a target tier between 3 and 6 depending on skill.
    // This is very roughly one tier higher than the tier of talisman you
    // can use with your current skill, because you probably already have a
    // talisman matching your current skill and are looking for an upgrade.
    const int target_tier = min(6, div_rand_round(_skill_rdiv(SK_SHAPESHIFTING), 8) + 3);

    // Compile all talismans into one list and give them appropriate weights.
    // The random option will stay weight 5.
    vector<pair<talisman_type, int>> weights;
    for (int tier = 1; tier <=5; ++tier)
    {
        vector<talisman_type> by_tier = talismans_by_tier(tier);
        for (talisman_type type : by_tier)
        {
            int scale_value = 1;

            if (!you.seen_talisman[type])
                scale_value *= 10;

            if (tier == target_tier)
                scale_value *= 25;
            else if (abs(tier - target_tier) == 1)
                scale_value *= 15;
            else if (abs(tier - target_tier) == 2)
                scale_value *= 7;

            weights.push_back({type, scale_value});
        }
    }

    // Always a small chance for any talisman
    weights.push_back({NUM_TALISMANS, 5});

    return weights;
}

/**
 * Choose a random type of talisman to be generated via acquirement or god
 * gifts.
 *
 * Heavily weighted toward talismans the player hasn't yet seen, and also
 * weighted toward higher level talismans when the player has more
 * shapeshifting skill.
 *
 * @return          A random talisman type.
 */
static int _acquirement_talisman_subtype(int & /*quantity*/,
                                         int agent)
{
    talisman_type talisman = NUM_TALISMANS;
    vector<pair<talisman_type, int>> tiers = _scale_talisman_weights(agent);
    talisman = *random_choose_weighted(tiers);

    // Choose randomly (but don't acquire a protean talisman from a scroll)
    if (talisman == NUM_TALISMANS)
    {
        do
        {
            talisman = static_cast<talisman_type>(random2(NUM_TALISMANS));
        }
        while (agent != GOD_XOM && talisman == TALISMAN_PROTEAN);
    }

    return talisman;
}

typedef int (*acquirement_subtype_finder)(int &quantity, int agent);
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
    _acquirement_talisman_subtype,
    0, // no gems either
    0, // no gizmos (handled elsewhere)
    0, // no baubles
};

static int _find_acquirement_subtype(object_class_type &class_wanted,
                                     int &quantity,
                                     int agent)
{
    COMPILE_CHECK(ARRAYSZ(_subtype_finders) == NUM_OBJECT_CLASSES);
    ASSERT(class_wanted != OBJ_RANDOM);

    if (class_wanted == OBJ_ARMOUR && !player_can_use_armour()
        || class_wanted == OBJ_WEAPONS && you.has_mutation(MUT_NO_GRASPING)
        || class_wanted == OBJ_JEWELLERY && you.has_mutation(MUT_NO_JEWELLERY))
    {
        return OBJ_RANDOM;
    }

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
                (*_subtype_finders[class_wanted])(quantity, agent);
        }

        // Double-check our subtype for weapons is valid
        ASSERT(class_wanted != OBJ_WEAPONS || type_wanted < get_max_subtype(class_wanted));

        item_def dummy;
        dummy.base_type = class_wanted;
        dummy.sub_type = type_wanted;
        dummy.plus = 1; // empty wands would be useless
        dummy.flags |= ISFLAG_IDENTIFIED;

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
    ASSERT_RANGE(book, 0, NUM_BOOKS);
    ASSERT(book != BOOK_MANUAL);
    ASSERT(book != BOOK_PARCHMENT);
    ASSERT(book != BOOK_RANDART_LEVEL);
    ASSERT(book != BOOK_RANDART_THEME);

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
               || you.has_mutation(MUT_FORLORN);
    }

    switch (you.religion)
    {
    case GOD_TROG:
        return _is_magic_skill(skill);
    case GOD_ZIN:
        if (skill == SK_SHAPESHIFTING)
            return true;
        // fallthrough to other good gods
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

    // Give magic skills double the weight of non-magic skills, since
    // even a pure caster will be training various non-magic skills.
    return x_chance_in_y(other_weights, 2*magic_weights + other_weights);
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

        if (skl == 27 || is_useless_skill(sk) || _skill_useless_with_god(sk))
            continue;

        int w = (skl < 12) ? skl + 3 : max(0, 25 - skl);

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
    // Preidentify.
    book.flags |= ISFLAG_IDENTIFIED;

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
        int weights[NUM_BOOKS] = { 0 };
        for (int bk = 0; bk < NUM_BOOKS; bk++)
        {
            const auto bkt = static_cast<book_type>(bk);

            if (!book_exists(bkt))
                continue;

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
        if (!make_book_level_randart(book, level, agent == GOD_SIF_MUNA))
            return false;
        break;
    }
    } // switch book choice


    if (agent != GOD_XOM && agent != GOD_SIF_MUNA)
    {
        // If we couldn't make a useful book, try to make a manual instead.
        // We have to temporarily identify the book for this.
        bool useless = false;
        {
            unwind_var<iflags_t> oldflags{book.flags};
            book.flags |= ISFLAG_IDENTIFIED;
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

static int _weapon_brand_reroll_denom(int brand)
{
    switch (brand)
    {
    // These brands generate frequently on a variety of weapon types.
    case SPWPN_FLAMING:
    case SPWPN_FREEZING:
    case SPWPN_DRAINING:
    case SPWPN_HEAVY:
    case SPWPN_VENOM:
    case SPWPN_PROTECTION:
    case SPWPN_ELECTROCUTION:
        return 3;
    case SPWPN_NORMAL:
        return 10;
    default:
        return 1;
    }
}

static bool _armour_slot_seen(equipment_slot slot)
{
    item_def item;
    item.base_type = OBJ_ARMOUR;
    item.quantity = 1;

    for (int i = 0; i < NUM_ARMOURS; i++)
    {
        if (slot != get_armour_slot((armour_type)i))
            continue;
        item.sub_type = i;

        // having seen a helmet means nothing about your decision to go
        // bare-headed if you have horns
        if (!can_equip_item(item))
            continue;

        if (you.seen_armour[i])
            return true;
    }
    return false;
}

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
 * @param agent     The source of the acquirement. For god gifts, it's equal to
 *                  the god.
 */
static void _adjust_brand(item_def &item, int agent)
{
    if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_ARMOUR)
        return; // don't reroll missile brands, I guess

    if (is_artefact(item))
        return; // their own kettle of fish

    // Trog has a restricted brand table.
    if (agent == GOD_TROG && item.base_type == OBJ_WEAPONS)
    {
        // 75% chance of a brand
        item.brand = random_choose(SPWPN_NORMAL, SPWPN_HEAVY,
                                   SPWPN_FLAMING, SPWPN_ANTIMAGIC);
        return;
    }

    // Otherwise we weight "boring" brands lower.
    if (agent != GOD_TROG && item.base_type == OBJ_WEAPONS)
    {
        while (!one_chance_in(_weapon_brand_reroll_denom(get_weapon_brand(item))))
            reroll_brand(item, ISPEC_GOOD_ITEM);
    }
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
        && ((item.base_type == OBJ_WEAPONS || item.base_type == OBJ_ARMOUR)
             && !can_equip_item(item)))
    {
        return "Destroying unusable weapon or armour!";
    }

    // Trog does not gift the Wrath of Trog.
    if (agent == GOD_TROG && is_unrandom_artefact(item, UNRAND_TROG))
        return "Destroying Trog-gifted Wrath of Trog!";

    // Oka does not gift reaping weapons.
    if (agent == GOD_OKAWARU && get_weapon_brand(item) == SPWPN_REAPING)
        return "Destroying Oka-gifted reaping weapon.";

    // Oka does not gift command armour.
    if (agent == GOD_OKAWARU && get_armour_ego_type(item) == SPARM_COMMAND)
        return "Destroying Oka-gifted command armour.";

    // Oka does not gift the Mask of the Dragon.
    if (agent == GOD_OKAWARU && is_unrandom_artefact(item, UNRAND_DRAGONMASK))
        return "Destroying Oka-gifted Mask of the Dragon.";

    // Mask of the Dragon is useless if Love is sacrificed.
    if (you.get_mutation_level(MUT_NO_LOVE)
        && is_unrandom_artefact(item, UNRAND_DRAGONMASK))
    {
        return "Destroying Mask of the Dragon after Love sac!";
    }

    // Pain brand is useless if you've sacrificed Necromancy.
    if (you.get_mutation_level(MUT_NO_NECROMANCY_MAGIC)
        && get_weapon_brand(item) == SPWPN_PAIN)
    {
        return "Destroying pain weapon after Necro sac!";
    }

    // Command brand is useless if you've sacrificed Love, Armour or Summoning.
    if ((you.get_mutation_level(MUT_NO_LOVE)
        || you.get_mutation_level(MUT_NO_ARMOUR_SKILL)
        || you.get_mutation_level(MUT_NO_SUMMONING_MAGIC))
        && get_armour_ego_type(item) == SPARM_COMMAND)
    {
        return "Destroying armour of command after Love, Armour or Summ sac!";
    }

    // Death brand is useless if you've sacrificed Necro.
    if (you.get_mutation_level(MUT_NO_NECROMANCY_MAGIC)
        && get_armour_ego_type(item) == SPARM_DEATH)
    {
        return "Destroying armour of death after Necro sac!";
    }

    // Resonance brand is useless if you've sacrificed Forgecraft.
    if (you.get_mutation_level(MUT_NO_FORGECRAFT_MAGIC)
        && get_armour_ego_type(item) == SPARM_RESONANCE)
    {
        return "Destroying armour of resonance after Forgecraft sac!";
    }

    if (you.undead_or_demonic(false) && is_holy_item(item))
        return "Destroying holy weapon for evil player!";

    if (you.is_holy() && get_weapon_brand(item) == SPWPN_FOUL_FLAME)
        return "Destroying foul flame weapon for holy player!";

    return ""; // all OK
}

int acquirement_create_item(object_class_type class_wanted,
                            int agent, bool quiet,
                            const coord_def &pos,
                            int force_ego)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    // Trog/Xom gifts are generally lower quality than scroll acquirement or
    // Oka gifts. We also use lower quality for missile gifts.
    const int item_level = ((agent == GOD_TROG || agent == GOD_XOM || class_wanted == OBJ_MISSILES) ? ISPEC_GIFT : ISPEC_GOOD_ITEM);
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
                                                    agent);
        }
        ASSERT(type_wanted != -1);

        // Don't generate randart books in items(), we do that
        // ourselves.
        bool want_arts = (class_wanted != OBJ_BOOKS);
        if (agent == GOD_TROG && !one_chance_in(3))
            want_arts = false;

        thing_created = items(want_arts, class_wanted, type_wanted,
                              item_level, force_ego, agent);

        if (thing_created == NON_ITEM)
        {
            if (!quiet)
                dprf("Failed to make thing!");
            continue;
        }

        item_def &acq_item(env.item[thing_created]);

        // If we asked for a specific brand and got something back without it
        // (likely because we rolled an incompatible type), destroy the item and
        // try again.
        if (force_ego > 0)
        {
            if ((acq_item.base_type == OBJ_WEAPONS && get_weapon_brand(acq_item) != force_ego)
                || (acq_item.base_type == OBJ_ARMOUR && get_armour_ego_type(acq_item) != force_ego))
            {
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                continue;
            }
        }

        _adjust_brand(acq_item, agent);

        // Increase the chance of armour being an artefact by usually
        // rerolling non-artefacts.
        if (acq_item.base_type == OBJ_ARMOUR && !is_artefact(acq_item))
        {
            if (agent != GOD_XOM && !one_chance_in(13))
            {
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                if (!quiet)
                    dprf("Destroying already-seen item!");
                continue;
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
            acq_item.quantity = random_range(200, 1400, 2);
        else if (class_wanted == OBJ_MISSILES)
        {
            // TODO: consider doubling the gift timeout instead of adjusting
            // gift quantity. That'd be an Oka nerf, but maybe it's fine?
            if (agent == GOD_OKAWARU || agent == GOD_XOM)
                acq_item.quantity = max(1, acq_item.quantity / 2);
            else
                acq_item.quantity *= 5;
        }
        else if (quant > 1)
            acq_item.quantity = quant;

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
        else if (acq_item.base_type == OBJ_JEWELLERY
                 && !is_unrandom_artefact(acq_item))
        {
            switch (acq_item.sub_type)
            {
            case RING_STRENGTH:
            case RING_INTELLIGENCE:
            case RING_DEXTERITY:
                acq_item.plus = GOOD_STAT_RING_PLUS;
                break;
            case RING_EVASION:
                acq_item.plus = 5;
                break;
            case RING_PROTECTION:
            case RING_SLAYING:
                acq_item.plus = GOOD_RING_PLUS;
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
            // Don't acquire negative enchantment except via Xom.
            if (agent != GOD_XOM)
                acq_item.plus = max(static_cast<int>(acq_item.plus), 0);
        }
        else if (acq_item.base_type == OBJ_TALISMANS
                 && !is_artefact(acq_item) && one_chance_in(4))
        {
            make_item_randart(acq_item);
        }
        else if (acq_item.base_type == OBJ_STAVES
                 && !is_artefact(acq_item) && !one_chance_in(5))
        {
            make_item_randart(acq_item);
        }

        // Last check: don't acquire items your god hates.
        // Temporarily mark as ID'd for the purpose of checking if
        // it is a hated brand (this addresses, e.g., Elyvilon followers
        // immediately identifying evil weapons).
        // Note that Xom will happily give useless items!
        int oldflags = acq_item.flags;
        acq_item.flags |= ISFLAG_IDENTIFIED;
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

    item_set_appearance(env.item[thing_created]); // cleanup

    if (thing_created != NON_ITEM)
    {
        ASSERT(env.item[thing_created].is_valid());
        env.item[thing_created].props[ACQUIRE_KEY].get_int() = agent;
    }

    ASSERT(!is_useless_item(env.item[thing_created], false) || agent == GOD_XOM);
    ASSERT(!god_hates_item(env.item[thing_created]));

    // If we have a zero coord_def, don't move the item to the grid. Used for
    // generating scroll of acquirement items.
    if (pos.origin())
        return thing_created;

    // Moving this above the move since it might not exist after falling.
    if (thing_created != NON_ITEM && !quiet)
        canned_msg(MSG_SOMETHING_APPEARS);

    // If a god wants to give you something but the floor doesn't want it,
    // it counts as a failed acquirement - no piety, etc cost.
    if (feat_destroys_items(env.grid(pos))
        && agent > GOD_NO_GOD
        && agent < NUM_GODS)
    {
        if (!quiet && agent == GOD_XOM)
            simple_god_message(" snickers.", false, GOD_XOM);
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
    string items_key;

    bool is_gizmo;

    void init_entries();
    string get_keyhelp(bool unused) const override;
    bool examine_index(int i) override;
    bool skip_process_command(int keyin) override;
public:
    AcquireMenu(CrawlVector &aitems, string ikey, bool is_gizmo);
};

class AcquireEntry : public InvEntry
{
    string get_text() const override
    {
        const colour_t keycol = LIGHTCYAN;
        const string keystr = colour_to_str(keycol);
        const string itemstr =
            colour_to_str(menu_colour(text, item_prefix(*item), tag, false));
        const string gold_text = item->base_type == OBJ_GOLD
            ? make_stringf(" (you have %d gold)", you.gold) : "";
        return make_stringf(" <%s>%c %c </%s><%s>%s%s</%s>",
                            keystr.c_str(),
                            hotkeys[0],
                            selected() ? '+' : '-',
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

AcquireMenu::AcquireMenu(CrawlVector &aitems, string ikey,
                         bool _is_gizmo = false)
    : InvMenu(MF_SINGLESELECT | MF_QUIET_SELECT
              | MF_ALLOW_FORMATTING | MF_INIT_HOVER),
      acq_items(aitems),
      items_key(ikey),
      is_gizmo(_is_gizmo)
{
    menu_action = ACT_EXECUTE;
    action_cycle = CYCLE_TOGGLE;
    set_flags(get_flags() & ~MF_USE_TWO_COLUMNS);

    set_tag("acquirement");

    init_entries();

    if (is_gizmo)
        set_title("Choose a gizmo to assemble.");
    else
        set_title("Choose an item to acquire.");
}

static void _create_acquirement_item(item_def &item, string items_key,
                                     bool is_gizmo = false)
{
    auto &acq_items = you.props[items_key].get_vector();

    // Now that we have a selection, mark any generated unrands as not having
    // been generated, so they go back in circulation. Exclude the selected
    // item from this, if it's an unrand.
    for (item_def &aitem : acq_items)
    {
        if (is_unrandom_artefact(aitem)
            && (!is_unrandom_artefact(item)
                || !is_unrandom_artefact(aitem, item.unrand_idx)))
        {
            destroy_item(aitem, true);
        }
    }

    take_note(Note(NOTE_ACQUIRE_ITEM, 0, 0, item.name(DESC_A),
              origin_desc(item)));
    // Mark as seen so that Lucky cannot proc off it.
    item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET | ISFLAG_SEEN);
    identify_item(item);

    if (is_gizmo)
    {
        move_item_to_inv(item, true);
        // XXX: This is ugly and only works because there can never be another
        //      gizmo in our inventory, but move_item_to_inv() doesn't actually
        //      return an index or anything else we can use.
        for (int i = 0; i < MAX_GEAR; ++i)
        {
            if (you.inv[i].base_type == OBJ_GIZMOS)
            {
                mprf("You assemble %s and install it in your exoskeleton!",
                     item.name(DESC_A).c_str());
                equip_item(SLOT_GIZMO, i, false);
                break;
            }
        }
    }
    else
    {
        if (copy_item_to_grid(item, you.pos()) != NON_ITEM)
            canned_msg(MSG_SOMETHING_APPEARS);
        else
            canned_msg(MSG_NOTHING_HAPPENS);
    }

    acq_items.clear();
    you.props.erase(items_key);
}

void AcquireMenu::init_entries()
{
    menu_letter ckey = 'a';
    string key = items_key;
    for (item_def& item : acq_items)
    {
        auto newentry = make_unique<AcquireEntry>(item);
        newentry->hotkeys.clear();
        newentry->add_hotkey(ckey++);
        add_entry(std::move(newentry));
    }

    on_single_selection = [this, key](const MenuEntry& item)
    {
        // update the more with a y/n prompt
        update_more();

        if (!yesno(nullptr, true, 'n', false, false, true))
        {
            deselect_all();
            update_more(); // go back to the regular more
            return true;
        }

        item_def &acq_item = *static_cast<item_def*>(item.data);
        _create_acquirement_item(acq_item, key, is_gizmo);

        return false;
    };
}

string AcquireMenu::get_keyhelp(bool) const
{
    string help;
    vector<MenuEntry*> selected = selected_entries();
    if (selected.size() == 1 && menu_action == ACT_EXECUTE)
    {
        auto& entry = *selected[0];
        const string col = colour_to_str(channel_to_colour(MSGCH_PROMPT));
        help = make_stringf(
               "<%s>%s %s? (%s/N)</%s>\n",
               col.c_str(),
               is_gizmo ? "Assemble" : "Acquire",
               entry.text.c_str(),
               Options.easy_confirm == easy_confirm_type::none ? "Y" : "y",
               col.c_str());
    }
    else
        help = "\n";
    // looks better with a margin:
    help += string(MIN_COLS, ' ') + '\n';

    if (is_gizmo)
    {
        help += make_stringf(
        //[!] assemble|examine gizmo  [a-i] select gizmo to assemble
        //[Esc/R-Click] exit
        "<lightgrey>%s%s  %s %s</lightgrey>",
        menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE).c_str(),
        menu_action == ACT_EXECUTE ? " <w>assemble</w>|examine gizmo" :
                                     " assemble|<w>examine</w> gizmo",
        hyphenated_hotkey_letters(item_count(), 'a').c_str(),
        menu_action == ACT_EXECUTE ? "select gizmo to assemble"
                                   : "examine gizmo");
    }
    else
    {
        help += make_stringf(
            //[!] acquire|examine item  [a-i] select item to acquire
            //[Esc/R-Click] exit
            "<lightgrey>%s%s  %s %s</lightgrey>",
            menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE).c_str(),
            menu_action == ACT_EXECUTE ? " <w>acquire</w>|examine items" :
                                        " acquire|<w>examine</w> items",
            hyphenated_hotkey_letters(item_count(), 'a').c_str(),
            menu_action == ACT_EXECUTE ? "select item for acquirement"
                                    : "examine item");
    }
    return pad_more_with_esc(help);
}

bool AcquireMenu::examine_index(int i)
{
    ASSERT(i >= 0 && i < static_cast<int>(items.size()));
    // Use a copy to set flags that make the description better
    // See the similar code in shopping.cc for details about const
    // hygiene
    item_def &item = *static_cast<item_def*>(items[i]->data);

    item.flags |= (ISFLAG_IDENTIFIED | ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
    describe_item_popup(item);
    deselect_all();

    return true;
}

bool AcquireMenu::skip_process_command(int keyin)
{
    // Bypass InvMenu::skip_process_command, which disables ! and ?
    return Menu::skip_process_command(keyin);
}

static item_def _acquirement_item_def(object_class_type item_type, int agent)
{
    item_def item;

    const int item_index = acquirement_create_item(item_type, agent, true);

    if (item_index != NON_ITEM)
    {
        ASSERT(!god_hates_item(env.item[item_index]));

        // We make a copy of the item def, but we don't keep the real item.
        item = env.item[item_index];
        item.flags |= ISFLAG_IDENTIFIED;
        destroy_item(item_index);
    }

    return item;
}

vector<object_class_type> shuffled_acquirement_classes(bool scroll)
{
    vector<object_class_type> rand_classes;

    if (!you.has_mutation(MUT_NO_ARMOUR))
        rand_classes.emplace_back(OBJ_ARMOUR);

    if (!you.has_mutation(MUT_NO_GRASPING))
    {
        rand_classes.emplace_back(OBJ_WEAPONS);
        // Staves are often less interesting options one way or the
        // other (either they are exactly what your pure caster wants
        // or they are the wrong staff or you aren't interested in
        // staves). So make this option a bit rarer.
        if (one_chance_in(3))
            rand_classes.emplace_back(OBJ_STAVES);
    }

    if (!you.has_mutation(MUT_NO_JEWELLERY))
        rand_classes.emplace_back(OBJ_JEWELLERY);

    rand_classes.emplace_back(OBJ_BOOKS);

    if (!you_worship(GOD_ZIN) && !you.has_mutation(MUT_NO_FORMS))
    {
        // We want talisman acq to be rarer than others.
        if (x_chance_in_y(45, 100))
            rand_classes.emplace_back(OBJ_TALISMANS);
    }

    // dungeon generation
    if (!scroll)
    {
        if (_unided_acq_misc())
            rand_classes.emplace_back(OBJ_MISCELLANY);

        rand_classes.emplace_back(OBJ_WANDS);
    }

    shuffle_array(rand_classes);
    return rand_classes;
}

void make_acquirement_items()
{
    vector<object_class_type> rand_classes = shuffled_acquirement_classes(true);
    const int num_wanted = min(3, (int) rand_classes.size());

    CrawlVector &acq_items = you.props[ACQUIRE_ITEMS_KEY].get_vector();
    acq_items.empty();

    // Generate item defs until we have enough, skipping any random classes
    // that fail to generate an item.
    for (auto obj_type : rand_classes)
    {
        if (acq_items.size() == num_wanted)
            break;

        auto item = _acquirement_item_def(obj_type, AQ_SCROLL);
        if (item.defined())
            acq_items.push_back(item);
    }

    // Gold is guaranteed.
    auto gold_item = _acquirement_item_def(OBJ_GOLD, AQ_SCROLL);
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
        make_acquirement_items();

    auto &acq_items = you.props[ACQUIRE_ITEMS_KEY].get_vector();

    int index = 0;
    if (!clua.callfn("c_choose_acquirement", ">d", &index))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (index >= 1 && index <= acq_items.size())
    {
        _create_acquirement_item(acq_items[index - 1], ACQUIRE_ITEMS_KEY);
        return true;
    }

    AcquireMenu acq_menu(acq_items, ACQUIRE_ITEMS_KEY);
    acq_menu.show();

    return !you.props.exists(ACQUIRE_ITEMS_KEY);
}

/// Does this item duplicate an already-generated item's base/subtype plus ego? (Ignore plus.)
static bool _is_duplicate(const item_def &item, CrawlVector &acq_items)
{
    if (is_artefact(item))
        return false;
    for (item_def &aitem : acq_items)
    {
        if (!is_artefact(aitem)
            && aitem.is_type(item.base_type, item.sub_type)
            && item.brand == aitem.brand)
        {
            return true;
        }
    }
    return false;
}

static void _make_okawaru_gifts(object_class_type gift_type)
{
    ASSERT(gift_type == OBJ_WEAPONS || gift_type == OBJ_ARMOUR);

    const string key = gift_type == OBJ_WEAPONS ? OKAWARU_WEAPONS_KEY
                                                : OKAWARU_ARMOUR_KEY;
    CrawlVector &acq_items = you.props[key].get_vector();
    acq_items.clear();

    while (acq_items.size() < 4)
    {
        auto item = _acquirement_item_def(gift_type, you.religion);
        if (item.defined() && !_is_duplicate(item, acq_items))
            acq_items.push_back(item);
    }
}

bool okawaru_gift_weapon()
{
    ASSERT(!you.props.exists(OKAWARU_WEAPON_GIFTED_KEY));

    if (!you.props.exists(OKAWARU_WEAPONS_KEY))
        _make_okawaru_gifts(OBJ_WEAPONS);

    auto &acq_items = you.props[OKAWARU_WEAPONS_KEY].get_vector();

    simple_god_message(" offers you a choice of weapons!");

    int index = 0;
    if (!clua.callfn("c_choose_okawaru_weapon", ">d", &index))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (index >= 1 && index <= acq_items.size())
    {
        _create_acquirement_item(acq_items[index - 1], OKAWARU_WEAPONS_KEY);

        take_note(Note(NOTE_GOD_GIFT, you.religion));
        you.props[OKAWARU_WEAPON_GIFTED_KEY] = true;

        return true;
    }

    AcquireMenu acq_menu(acq_items, OKAWARU_WEAPONS_KEY);
    acq_menu.show();

    // Nothing selected yet.
    if (you.props.exists(OKAWARU_WEAPONS_KEY))
        return false;

    take_note(Note(NOTE_GOD_GIFT, you.religion));
    you.props[OKAWARU_WEAPON_GIFTED_KEY] = true;

    return true;
}

bool okawaru_gift_armour()
{
    ASSERT(!you.props.exists(OKAWARU_ARMOUR_GIFTED_KEY));

    if (!you.props.exists(OKAWARU_ARMOUR_KEY))
        _make_okawaru_gifts(OBJ_ARMOUR);

    auto &acq_items = you.props[OKAWARU_ARMOUR_KEY].get_vector();

    simple_god_message(" offers you a choice of armour!");

    int index = 0;
    if (!clua.callfn("c_choose_okawaru_armour", ">d", &index))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (index >= 1 && index <= acq_items.size())
    {
        _create_acquirement_item(acq_items[index - 1], OKAWARU_ARMOUR_KEY);

        take_note(Note(NOTE_GOD_GIFT, you.religion));
        you.props[OKAWARU_ARMOUR_GIFTED_KEY] = true;

        return true;
    }

    AcquireMenu acq_menu(acq_items, OKAWARU_ARMOUR_KEY);
    acq_menu.show();

    // Nothing selected yet.
    if (you.props.exists(OKAWARU_ARMOUR_KEY))
        return false;

    take_note(Note(NOTE_GOD_GIFT, you.religion));
    you.props[OKAWARU_ARMOUR_GIFTED_KEY] = true;

    return true;
}

static string _generate_gizmo_serial_number(bool at_end = false)
{
    string serial;

    // Short serial (but only at the end of names)
    if (at_end && one_chance_in(3))
    {
        serial += (coinflip() ? "Mk." : "Ver.") + std::to_string(random2(10));
        if (coinflip())
            serial += std::to_string(random2(10));
        return serial;
    }

    // 1 or 2 uppercase letters
    int num_letters = coinflip() ? 2 : 1;
    for (int i = 0; i < num_letters; ++i)
        serial += rand() % 26 + 65;

    serial += "-";

    // Generate a numerical serial. Make it shorter if we have more letters,
    // and give a chance to use trailing 0s instead of random numbers.
    int num_numbers = random_range(1, 4 - num_letters);
    int num_real_numbers = (num_numbers == 4 ? 1
                                             : random_range(1, max(1, num_numbers - 1)));

    for (int i = 0; i < num_real_numbers; ++i)
        serial += std::to_string(random2(10));

    for (int i = 0; i < num_real_numbers; ++i)
        serial += "0";

    return serial;
}

static string _generate_gizmo_name()
{
    string name;

    string noun = getMiscString("gizmo_noun");
    string modifier = getMiscString("gizmo_modifier");

    // Chance of serial number name
    if (one_chance_in(3))
    {
        // 50% chance to be first or second
        if (coinflip())
            name += modifier + noun + " " + _generate_gizmo_serial_number(true);
        else
            name += _generate_gizmo_serial_number() + " " + modifier + noun;
    }
    // Use adjective
    else
    {
        string adj = getMiscString("gizmo_adjective");

        // 50% chance of modifier, applied to either noun or adjective
        if (coinflip())
        {
            if (coinflip())
                adj = modifier + adj;
            else
                noun = modifier + noun;
        }

        name = adj + " " + noun;
    }

    return name;
}

static void _make_coglin_gizmos()
{
    CrawlVector &names = you.props[COGLIN_GIZMO_NAMES_KEY].get_vector();

    CrawlVector &acq_items = you.props[COGLIN_GIZMO_KEY].get_vector();
    acq_items.clear();

    // Generate the given number of gizmos, using previously announced names for
    // them, if they exist.
    for (unsigned int i = 0; i < COGLIN_GIZMO_NUM; ++i)
    {
        auto item = _acquirement_item_def(OBJ_GIZMOS, AQ_INVENTED);
        if (item.defined())
        {
            if (names.size() > i)
                item.props[ARTEFACT_NAME_KEY].get_string() = names[i].get_string();
            else
                item.props[ARTEFACT_NAME_KEY].get_string() = _generate_gizmo_name();

            acq_items.push_back(item);
        }
    }

    fill_gizmo_properties(acq_items);
}

bool coglin_invent_gizmo()
{
    if (inv_count(INVENT_GEAR) >= MAX_GEAR)
    {
        mpr("You don't have room to hold a gizmo! Drop something first.");
        return false;
    }

    if (!you.props.exists(COGLIN_GIZMO_KEY))
        _make_coglin_gizmos();

    auto &acq_items = you.props[COGLIN_GIZMO_KEY].get_vector();

    int index = 0;
    if (!clua.callfn("c_choose_coglin_gizmo", ">d", &index))
    {
        if (!clua.error.empty())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    }
    else if (index >= 1 && index <= acq_items.size())
    {
        _create_acquirement_item(acq_items[index - 1], COGLIN_GIZMO_KEY, true);
        you.props[INVENT_GIZMO_USED_KEY] = true;
        return true;
    }

    AcquireMenu acq_menu(acq_items, COGLIN_GIZMO_KEY, true);
    acq_menu.show();

    // Nothing selected yet.
    if (you.props.exists(COGLIN_GIZMO_KEY))
        return false;

    you.props[INVENT_GIZMO_USED_KEY] = true;

    return true;
}

// We add names to this list as they are requested, and then will use the list
// when making actual gizmos, up to however many names were already determined.
void coglin_announce_gizmo_name()
{
    CrawlVector& names = you.props[COGLIN_GIZMO_NAMES_KEY].get_vector();
    string name = _generate_gizmo_name();
    names.push_back(name);

    mprf("Your brain swirls with designs for %s. You just need some more time...",
         article_a(name).c_str());
}
