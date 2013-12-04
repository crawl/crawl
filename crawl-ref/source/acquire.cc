/**
 * @file
 * @brief Acquirement and Trog/Oka/Sif gifts.
**/

#include "AppHdr.h"

#include "acquire.h"

#include <cstdlib>
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <queue>
#include <set>
#include <cmath>

#include "art-enum.h"
#include "artefact.h"
#include "decks.h"
#include "dungeon.h"
#include "externs.h"
#include "food.h"
#include "goditem.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "player.h"
#include "random.h"
#include "random-weight.h"
#include "religion.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"

static armour_type _random_nonbody_armour_type()
{
    return random_choose(ARM_SHIELD, ARM_CLOAK, ARM_HELMET, ARM_GLOVES,
                         ARM_BOOTS, -1);
}

static const int max_has_value = 100;
typedef FixedVector<int, max_has_value> has_vector;

static armour_type _pick_wearable_armour(const armour_type arm)
{
    armour_type result = arm;

    // Some species specific fitting problems.
    // FIXME: switch to the cleaner logic in can_wear_armour()
    switch (you.species)
    {
    case SP_OGRE:
    case SP_TROLL:
    case SP_SPRIGGAN:
        if (arm == ARM_GLOVES
            || arm == ARM_BOOTS
            || arm == ARM_CENTAUR_BARDING
            || arm == ARM_NAGA_BARDING)
        {
            result = ARM_ROBE;  // no heavy armour
        }
        else if (arm == ARM_SHIELD)
        {
            if (you.species == SP_SPRIGGAN)
                result = ARM_BUCKLER;
            else if (x_chance_in_y(5 + you.skills[SK_SHIELDS], 20))
                result = ARM_LARGE_SHIELD; // prefer big shields for giant races
        }
        else if (arm == NUM_ARMOURS)
            result = ARM_ROBE;  // no heavy armour, see below
        break;

    case SP_RED_DRACONIAN:
    case SP_WHITE_DRACONIAN:
    case SP_GREEN_DRACONIAN:
    case SP_YELLOW_DRACONIAN:
    case SP_GREY_DRACONIAN:
    case SP_BLACK_DRACONIAN:
    case SP_PURPLE_DRACONIAN:
    case SP_MOTTLED_DRACONIAN:
    case SP_PALE_DRACONIAN:
    case SP_BASE_DRACONIAN:
        if (arm == ARM_ROBE
            || arm == NUM_ARMOURS // no heavy armour
            || arm == ARM_CENTAUR_BARDING
            || arm == ARM_NAGA_BARDING)
        {
            result = random_choose(ARM_HELMET, ARM_GLOVES, ARM_BOOTS, -1);
        }
        else if (arm == ARM_SHIELD)
        {
            if (x_chance_in_y(5 + you.skills[SK_SHIELDS], 20))
                result = ARM_LARGE_SHIELD;
        }
        break;

    case SP_NAGA:
        if (arm == ARM_BOOTS || arm == ARM_CENTAUR_BARDING)
            result = ARM_NAGA_BARDING;
        if (arm == ARM_SHIELD && x_chance_in_y(5 + you.skills[SK_SHIELDS], 20))
            result = ARM_LARGE_SHIELD; // nagas have bonuses to big shields
        break;

    case SP_CENTAUR:
        if (arm == ARM_BOOTS || arm == ARM_NAGA_BARDING)
            result = ARM_CENTAUR_BARDING;
        if (arm == ARM_SHIELD && x_chance_in_y(5 + you.skills[SK_SHIELDS], 20))
            result = ARM_LARGE_SHIELD; // so have centaurs
        break;

    case SP_OCTOPODE:
        if (arm != ARM_HELMET && arm != ARM_SHIELD)
            if (coinflip())
                result = ARM_HELMET;
            else
                result = ARM_SHIELD;
        // and fall through for shield size adjustments

    default:
        if (arm == ARM_CENTAUR_BARDING || arm == ARM_NAGA_BARDING)
            result = ARM_BOOTS;
        if (arm == ARM_SHIELD)
            if (x_chance_in_y(15 - you.skills[SK_SHIELDS], 20))
                result = ARM_BUCKLER;
            else if (x_chance_in_y(you.skills[SK_SHIELDS] - 10, 15)
                     && you.species != SP_KOBOLD && you.species != SP_HALFLING)
                result = ARM_LARGE_SHIELD;
        break;
    }

    // Mutation specific problems (horns and antennae allow caps, but not
    // Horns 3 or Antennae 3 (checked below)).
    if (result == ARM_BOOTS && !player_has_feet(false)
        || result == ARM_GLOVES && you.has_claws(false) >= 3)
    {
        result = NUM_ARMOURS;
    }

    // Do this here, before acquirement()'s call to can_wear_armour(),
    // so that caps will be just as common as helmets for those
    // that can't wear helmets.
    // We check for the mutation directly to avoid acquirement fiddles
    // with vampires.
    if (arm == ARM_HELMET
        && (!you_can_wear(EQ_HELMET)
            || you.mutation[MUT_HORNS]
            || you.mutation[MUT_ANTENNAE]))
    {
        // Check for Horns 3 & Antennae 3 - Don't give a cap if those mutation
        // levels have been reached.
        if (you.mutation[MUT_HORNS] <= 2 || you.mutation[MUT_ANTENNAE] <= 2)
            result = coinflip() ? ARM_CAP : ARM_WIZARD_HAT;
        else
            result = NUM_ARMOURS;
    }

    return result;
}

static armour_type _acquirement_armour_subtype(bool divine)
{
    // Increasing the representation of the non-body armour
    // slots here to make up for the fact that there's only
    // one type of item for most of them. -- bwr
    //
    // NUM_ARMOURS is body armour and handled below
    armour_type result = NUM_ARMOURS;

    if (divine)
    {
        if (coinflip())
            result = _random_nonbody_armour_type();
    }
    else
    {
        static const equipment_type armour_slots[] =
            {  EQ_SHIELD, EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS   };

        equipment_type picked = EQ_BODY_ARMOUR;
        const int num_slots = ARRAYSZ(armour_slots);
        // Start count at 1, for body armour (already picked).
        for (int i = 0, count = 1; i < num_slots; ++i)
            if (you_can_wear(armour_slots[i], true) && one_chance_in(++count))
                picked = armour_slots[i];

        switch (picked)
        {
        case EQ_SHIELD:
            result = ARM_SHIELD; break;
        case EQ_CLOAK:
            result = ARM_CLOAK;  break;
        case EQ_HELMET:
            result = ARM_HELMET; break;
        case EQ_GLOVES:
            result = ARM_GLOVES; break;
        case EQ_BOOTS:
            result = ARM_BOOTS;  break;
        default:
        case EQ_BODY_ARMOUR:
            result = NUM_ARMOURS; break;
        }

        if (you.species == SP_NAGA || you.species == SP_CENTAUR)
        {
            armour_type bard = (you.species == SP_NAGA) ? ARM_NAGA_BARDING
                                                        : ARM_CENTAUR_BARDING;
            if (one_chance_in(you.seen_armour[bard] ? 4 : 2))
                result = bard;
        }
    }

    result = _pick_wearable_armour(result);

    // Now we'll randomly pick a body armour up to plate armour (light
    // only in the case of robes or animal skins).  Unlike before, now
    // we're only giving out the finished products here, never the
    // hides. - bwr
    if (result == NUM_ARMOURS || result == ARM_ROBE)
    {
        // Start with normal base armour.
        if (result == ARM_ROBE)
        {
            // Animal skins don't get egos, so make them less likely.
            result = (one_chance_in(4) ? ARM_ANIMAL_SKIN : ARM_ROBE);

            // Armour-restricted species get a bonus chance at
            // troll/dragon armour.  (In total, the chance is almost
            // 10%.)
            if (one_chance_in(20))
            {
                result = random_choose_weighted(3, ARM_TROLL_LEATHER_ARMOUR,
                                                3, ARM_STEAM_DRAGON_ARMOUR,
                                                1, ARM_SWAMP_DRAGON_ARMOUR,
                                                1, ARM_FIRE_DRAGON_ARMOUR,
                                                0);
            }

            // Non-god acquirement not only has a much better chance, but
            // can give high-end ones as well.
            if (!divine && one_chance_in(5))
            {
                result = random_choose(
                        ARM_FIRE_DRAGON_ARMOUR,
                        ARM_ICE_DRAGON_ARMOUR,
                        ARM_STEAM_DRAGON_ARMOUR,
                        ARM_MOTTLED_DRAGON_ARMOUR,
                        ARM_STORM_DRAGON_ARMOUR,
                        ARM_GOLD_DRAGON_ARMOUR,
                        ARM_SWAMP_DRAGON_ARMOUR,
                        ARM_PEARL_DRAGON_ARMOUR,
                        -1);
            }
        }
        else
        {
            if (divine)
            {
                const armour_type armours[] = { ARM_ROBE, ARM_LEATHER_ARMOUR,
                                                ARM_RING_MAIL, ARM_SCALE_MAIL,
                                                ARM_CHAIN_MAIL,
                                                ARM_PLATE_ARMOUR };

                result = static_cast<armour_type>(RANDOM_ELEMENT(armours));

                if (x_chance_in_y(you.skills[SK_ARMOUR], 150))
                    result = ARM_CRYSTAL_PLATE_ARMOUR;
            }
            else
            {
                const armour_type armours[] =
                    { ARM_ANIMAL_SKIN, ARM_ROBE, ARM_LEATHER_ARMOUR,
                      ARM_RING_MAIL, ARM_SCALE_MAIL, ARM_CHAIN_MAIL,
                      ARM_PLATE_ARMOUR, ARM_CRYSTAL_PLATE_ARMOUR };

                const int num_arms = ARRAYSZ(armours);

                // Weight sub types relative to (armour skill + 3).
                // Actually, the AC improvement is not linear, and we
                // might also want to take into account Dodging/Stealth
                // and Strength, but this is definitely better than the
                // random chance above.

                // This formula makes sense only for casters.
                const int skill = min(27, you.skills[SK_ARMOUR] + 3);
                int total = 0;
                for (int i = 0; i < num_arms; ++i)
                {
                    int weight = max(1, 27 - abs(skill - i*3));
                    weight = weight * weight * weight;
                    total += weight;
                    if (x_chance_in_y(weight, total))
                        result = armours[i];
                }
                // ... so we override it for heavy meleers, who get mostly plates.
                // A scale mail is wasted acquirement, even if it's any but most
                // über randart).
                if (random2(you.skills[SK_SPELLCASTING] * 3
                            + you.skills[SK_DODGING])
                    < random2(you.skills[SK_ARMOUR] * 2))
                {
                    result = one_chance_in(4) ? ARM_CRYSTAL_PLATE_ARMOUR :
                                                ARM_PLATE_ARMOUR;
                }
            }
        }

        // Everyone can wear things made from hides.
        if (one_chance_in(20))
        {
            result = random_choose_weighted(20, ARM_TROLL_LEATHER_ARMOUR,
                                            20, ARM_STEAM_DRAGON_ARMOUR,
                                            15, ARM_MOTTLED_DRAGON_ARMOUR,
                                            10, ARM_SWAMP_DRAGON_ARMOUR,
                                            10, ARM_FIRE_DRAGON_ARMOUR,
                                            10, ARM_ICE_DRAGON_ARMOUR,
                                             5, ARM_STORM_DRAGON_ARMOUR,
                                             5, ARM_GOLD_DRAGON_ARMOUR,
                                             5, ARM_PEARL_DRAGON_ARMOUR,
                                             0);
        }
    }

    return result;
}

static armour_type _pick_unseen_armour()
{
    static const equipment_type armour_slots[] =
        {  EQ_SHIELD, EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS  };

    armour_type picked = NUM_ARMOURS;
    const int num_slots = ARRAYSZ(armour_slots);
    for (int i = 0, count = 0; i < num_slots; ++i)
    {
        if (!you_can_wear(armour_slots[i], true))
            continue;

        // Consider shields uninteresting always, since unlike with other slots
        // players might well prefer an empty slot to wearing one. We don't
        // want to try to guess at this by looking at their weapon's handedness
        // because this would encourage switching weapons or putting on a
        // shield right before reading acquirement in some cases. --elliptic
        // This affects only the "unfilled slot" special-case, not regular
        // acquirement which can always produce (wearable) shields.
        if (armour_slots[i] == EQ_SHIELD)
            continue;

        armour_type result;
        switch (armour_slots[i])
        {
        case EQ_SHIELD:
            result = ARM_SHIELD; break;
        case EQ_CLOAK:
            result = ARM_CLOAK;  break;
        case EQ_HELMET:
            result = ARM_HELMET; break;
        case EQ_GLOVES:
            result = ARM_GLOVES; break;
        case EQ_BOOTS:
            result = ARM_BOOTS;  break;
        default:
            continue;
        }
        result = _pick_wearable_armour(result);
        if (result == NUM_ARMOURS || you.seen_armour[result])
            continue;

        if (one_chance_in(++count))
            picked = result;
    }

    return picked;
}

// Write results into arguments.
static void _acquirement_determine_food(int& type_wanted, int& quantity)
{
    // Food is a little less predictable now. - bwr
    if (you.species == SP_GHOUL)
    {
        type_wanted = random_choose_weighted(4, FOOD_CHUNK,
                                             2, FOOD_ROYAL_JELLY,
                                             1, FOOD_AMBROSIA,
                                             0);
    }
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
        type_wanted = random_choose(FOOD_BANANA, FOOD_ORANGE, FOOD_LEMON, -1);
    }
    else
    {
        type_wanted = random_choose_weighted(
                        5, FOOD_ROYAL_JELLY,
                        3, FOOD_AMBROSIA,
                        1, player_mutation_level(MUT_HERBIVOROUS) ? FOOD_BREAD_RATION
                                                                  : FOOD_MEAT_RATION,
                        1, FOOD_HONEYCOMB,
                        0);
    }

    quantity = 3 + random2(5);

    if (type_wanted == FOOD_BANANA || type_wanted == FOOD_ORANGE || type_wanted == FOOD_LEMON)
        quantity = 8 + random2avg(15, 2);
    // giving more of the lower food value items
    else if (type_wanted == FOOD_HONEYCOMB || type_wanted == FOOD_CHUNK)
        quantity += random2avg(10, 2);
    else if (type_wanted == POT_BLOOD)
    {
    // this was above in the vampire block, but gets overwritten by line 1371
    // so moving here {due}
        quantity = 8 + random2(5);
    }
}

static int _acquirement_weapon_subtype(bool divine)
{
    // Asking for a weapon is biased towards your skills.
    // First pick a skill, weighting towards those you have.
    int count = 0;
    skill_type skill = SK_FIGHTING;
    int best_sk = 0;

    for (int i = SK_SHORT_BLADES; i <= SK_CROSSBOWS; i++)
        if (you.skills[i] > best_sk)
            best_sk = you.skills[i];

    for (int i = SK_SHORT_BLADES; i <= SK_CROSSBOWS; i++)
    {
        skill_type sk = static_cast<skill_type>(i);

        // Adding a small constant allows for the occasional
        // weapon in an untrained skill.
        int weight = you.skills[sk] + 1;
        // Exaggerate the weighting if it's a scroll acquirement.
        if (!divine)
            weight = (weight + 1) * (weight + 2);
        count += weight;

        if (x_chance_in_y(weight, count))
            skill = sk;
    }
    if (you.skills[SK_UNARMED_COMBAT] > best_sk)
        best_sk = you.skills[SK_UNARMED_COMBAT];

    // Now choose a subtype which uses that skill.
    int result = OBJ_RANDOM;
    count = 0;
    item_def item_considered;
    item_considered.base_type = OBJ_WEAPONS;
    // Let's guess the percentage of shield use the player did, this is
    // based on empirical data where pure-shield MDs get skills like 17 sh 25 m&f
    // and pure-shield Spriggans 7 sh 18 m&f.
    int shield_sk = you.skills[SK_SHIELDS] * species_apt_factor(SK_SHIELDS);
    int want_shield = min(2 * shield_sk, best_sk) + 10;
    int dont_shield = max(best_sk - shield_sk, 0) + 10;
    // At XL 10, weapons of the handedness you want get weight *2, those of
    // opposite handedness 1/2, assuming your shields usage is respectively
    // 0% or 100% in the above formula.  At skill 25 that's *3.5 .
    for (int i = 0; i < NUM_WEAPONS; ++i)
    {
        int wskill = range_skill(OBJ_WEAPONS, i);
        if (wskill == SK_THROWING)
            wskill = weapon_skill(OBJ_WEAPONS, i);

        if (wskill != skill)
            continue;
        item_considered.sub_type = i;

        // Can't get blessed blades through acquirement, only from TSO
        if (is_blessed(item_considered))
            continue;

        int acqweight = property(item_considered, PWPN_ACQ_WEIGHT) * 100;

        if (!acqweight)
            continue;

        const bool two_handed = you.hands_reqd(item_considered) == HANDS_TWO;

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

        if (x_chance_in_y(acqweight, count += acqweight))
            result = i;
    }
    return result;
}

static bool _have_item_with_types(object_class_type basetype, int subtype)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def& item = you.inv[i];
        if (item.defined()
            && item.base_type == basetype && item.sub_type == subtype)
        {
            return true;
        }
    }
    return false;
}

static missile_type _acquirement_missile_subtype()
{
    int count = 0;
    int skill = SK_THROWING;

    for (int i = SK_SLINGS; i <= SK_THROWING; i++)
    {
        if (you.skills[i])
        {
            count += you.skills[i];
            if (x_chance_in_y(you.skills[i], count))
                skill = i;
        }
    }

    missile_type result = MI_DART;

    switch (skill)
    {
    case SK_SLINGS:    result = MI_SLING_BULLET; break;
    case SK_BOWS:      result = MI_ARROW; break;
    case SK_CROSSBOWS: result = MI_BOLT; break;

    case SK_THROWING:
        {
            // Choose from among all usable missile types.
            // Only give needles if they have a blowgun in inventory.
            vector<pair<missile_type, int> > missile_weights;

            missile_weights.push_back(make_pair(MI_DART, 100));
            missile_weights.push_back(make_pair(MI_TOMAHAWK, 100));

            if (_have_item_with_types(OBJ_WEAPONS, WPN_BLOWGUN))
                missile_weights.push_back(make_pair(MI_NEEDLE, 100));

            if (you.body_size() >= SIZE_MEDIUM)
                missile_weights.push_back(make_pair(MI_JAVELIN, 100));

            if (you.can_throw_large_rocks())
                missile_weights.push_back(make_pair(MI_LARGE_ROCK, 100));

            result = *random_choose_weighted(missile_weights);
        }
        break;

    default:
        break;
    }
    return result;
}

static int _acquirement_jewellery_subtype()
{
    int result = 0;

    // Try ten times to give something the player hasn't seen.
    for (int i = 0; i < 10; i++)
    {
        // 1/3 amulets, 2/3 rings.
        result = (one_chance_in(you.species == SP_OCTOPODE ? 9 : 3)
                                   ? get_random_amulet_type()
                                   : get_random_ring_type());

        // If we haven't seen this yet, we're done.
        if (get_ident_type(OBJ_JEWELLERY, result) == ID_UNKNOWN_TYPE)
            break;
    }

    return result;
}

static bool _want_rod()
{
    // First look at skills to determine whether the player gets a rod.
    int spell_skills = 0;
    for (int i = SK_SPELLCASTING; i <= SK_LAST_MAGIC; i++)
        spell_skills += you.skills[i];

    return random2(spell_skills) < you.skills[SK_EVOCATIONS] + 3
           && !one_chance_in(5);
}

static int _acquirement_staff_subtype(const has_vector& already_has)
{
    // Try to pick an enhancer staff matching the player's best skill.
    skill_type best_spell_skill = best_skill(SK_SPELLCASTING, SK_EVOCATIONS);
    bool found_enhancer = false;
    int result = 0;
#if TAG_MAJOR_VERSION == 34
    do
        result = random2(NUM_STAVES);
    while (result == STAFF_ENCHANTMENT || result == STAFF_CHANNELING);
#else
    result = random2(NUM_STAVES);
#endif

#define TRY_GIVE(x) { if (you.type_ids[OBJ_STAVES][x] != ID_KNOWN_TYPE) \
                      {result = x; found_enhancer = true;} }
    switch (best_spell_skill)
    {
    case SK_FIRE_MAGIC:   TRY_GIVE(STAFF_FIRE);        break;
    case SK_ICE_MAGIC:    TRY_GIVE(STAFF_COLD);        break;
    case SK_AIR_MAGIC:    TRY_GIVE(STAFF_AIR);         break;
    case SK_EARTH_MAGIC:  TRY_GIVE(STAFF_EARTH);       break;
    case SK_POISON_MAGIC: TRY_GIVE(STAFF_POISON);      break;
    case SK_NECROMANCY:   TRY_GIVE(STAFF_DEATH);       break;
    case SK_CONJURATIONS: TRY_GIVE(STAFF_CONJURATION); break;
    case SK_SUMMONINGS:   TRY_GIVE(STAFF_SUMMONING);   break;
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
    switch (random2(5))
    {
    case 0: case 1: TRY_GIVE(STAFF_WIZARDRY);   break;
    case 2: case 3: TRY_GIVE(STAFF_ENERGY);     break;
    case 4: TRY_GIVE(STAFF_POWER);              break;
#undef TRY_GIVE
    }
    return result;
}

static int _acquirement_misc_subtype()
{
    // Note: items listed early are less likely due to chances of being
    // overwritten.
    int result = random_range(MISC_FIRST_DECK, MISC_LAST_DECK);
    if (result == MISC_DECK_OF_SUMMONING && coinflip())
        result = MISC_SACK_OF_SPIDERS;
    if (result == MISC_DECK_OF_PUNISHMENT)
        result = MISC_BOX_OF_BEASTS;
    if (one_chance_in(4) && !you.seen_misc[MISC_DISC_OF_STORMS])
        result = MISC_DISC_OF_STORMS;
    if (one_chance_in(4) && !you.seen_misc[MISC_LAMP_OF_FIRE])
        result = MISC_LAMP_OF_FIRE;
    if (one_chance_in(4) && !you.seen_misc[MISC_FAN_OF_GALES])
        result = MISC_FAN_OF_GALES;
    if (one_chance_in(4) && !you.seen_misc[MISC_STONE_OF_TREMORS])
        result = MISC_STONE_OF_TREMORS;
    if (one_chance_in(4) && !you.seen_misc[MISC_PHIAL_OF_FLOODS])
        result = MISC_PHIAL_OF_FLOODS;
    if (one_chance_in(4) && !you.seen_misc[MISC_LANTERN_OF_SHADOWS])
        result = MISC_LANTERN_OF_SHADOWS;
    if (x_chance_in_y(you.skills[SK_EVOCATIONS], 27)
        && (x_chance_in_y(max(you.skills[SK_SPELLCASTING],
                              you.skills[SK_INVOCATIONS]), 27))
        && !you.seen_misc[MISC_CRYSTAL_BALL_OF_ENERGY])
    {
        result = MISC_CRYSTAL_BALL_OF_ENERGY;
    }

    return result;
}

static int _acquirement_wand_subtype()
{
    int picked = NUM_WANDS;

    int total = 0;
    for (int type = 0; type < NUM_WANDS; ++type)
    {
        int w = 0;

        // First, weight according to usefulness.
        switch (type)
        {
        case WAND_HASTING:          // each 17.9%, group unknown each 26.3%
        case WAND_HEAL_WOUNDS:
            w = 25; break;
        case WAND_TELEPORTATION:    // each 10.7%, group unknown each 17.6%
            w = 15; break;
        case WAND_FIRE:             // each 5.7%, group unknown each 9.3%
        case WAND_COLD:
        case WAND_LIGHTNING:
        case WAND_DRAINING:
        case WAND_INVISIBILITY:
        case WAND_FIREBALL:
            w = 8; break;
        case WAND_DIGGING:          // each 3.6%, group unknown each 6.25%
        case WAND_DISINTEGRATION:
        case WAND_POLYMORPH:
        case WAND_ENSLAVEMENT:
            w = 5; break;
        case WAND_FLAME:            // each 0.7%, group unknown each 1.4%
        case WAND_FROST:
        case WAND_CONFUSION:
        case WAND_PARALYSIS:
        case WAND_SLOWING:
        case WAND_MAGIC_DARTS:
        case WAND_RANDOM_EFFECTS:
        default:
            w = 1; break;
        }

        // Unknown wands get another huge weight bonus.
        if (get_ident_type(OBJ_WANDS, type) == ID_UNKNOWN_TYPE)
            w *= 2;

        total += w;
        if (x_chance_in_y(w, total))
            picked = type;
    }

    return picked;
}

static int _find_acquirement_subtype(object_class_type &class_wanted,
                                     int &quantity, bool divine,
                                     int agent = -1)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    int type_wanted = OBJ_RANDOM;

    // Write down what the player is carrying.
    has_vector already_has;
    already_has.init(0);
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        const item_def& item = you.inv[i];
        if (item.defined() && item.base_type == class_wanted)
        {
            ASSERT(item.sub_type < max_has_value);
            already_has[item.sub_type] += item.quantity;
        }
    }

    bool try_again = (class_wanted == OBJ_JEWELLERY
                      || class_wanted == OBJ_STAVES
                      || class_wanted == OBJ_MISCELLANY);
    int useless_count = 0;

    while (1)
    {
        // Staves and rods have a common acquirement class.
        if (class_wanted == OBJ_STAVES || class_wanted == OBJ_RODS)
            class_wanted = _want_rod() ? OBJ_RODS : OBJ_STAVES;

        switch (class_wanted)
        {
        case OBJ_FOOD:
            // Clobber class_wanted for vampires.
            if (you.species == SP_VAMPIRE)
                class_wanted = OBJ_POTIONS;
            // Deliberate fall-through
        case OBJ_POTIONS: // Should only happen for vampires.
            // set type_wanted and quantity
            _acquirement_determine_food(type_wanted, quantity);
            break;

        case OBJ_WEAPONS:    type_wanted = _acquirement_weapon_subtype(divine);  break;
        case OBJ_MISSILES:   type_wanted = _acquirement_missile_subtype(); break;
        case OBJ_ARMOUR:     type_wanted = _acquirement_armour_subtype(divine); break;
        case OBJ_MISCELLANY: type_wanted = _acquirement_misc_subtype(); break;
        case OBJ_WANDS:      type_wanted = _acquirement_wand_subtype(); break;
        case OBJ_STAVES:     type_wanted = _acquirement_staff_subtype(already_has);
            break;
        case OBJ_RODS:       type_wanted = random2(NUM_RODS); break;
        case OBJ_JEWELLERY:  type_wanted = _acquirement_jewellery_subtype();
            break;
        default: break;         // gold, books
        }

        item_def dummy;
        dummy.base_type = class_wanted;
        dummy.sub_type = type_wanted;
        dummy.plus = 1; // empty wands would be useless
        dummy.flags |= ISFLAG_IDENT_MASK;

        if ((is_useless_item(dummy, false) || god_hates_item(dummy))
            && useless_count++ < 200)
        {
            continue;
        }

        if (!try_again)
            break;

        ASSERT(type_wanted < max_has_value);
        if (!already_has[type_wanted])
            break;
        if (one_chance_in(200))
            break;
    }

    return type_wanted;
}

// The weight of a spell takes into account its disciplines' skill levels
// and the spell difficulty.
static int _spell_weight(spell_type spell)
{
    ASSERT(spell != SPELL_NO_SPELL);

    int weight = 0;
    unsigned int disciplines = get_spell_disciplines(spell);
    int count = 0;
    for (int i = 0; i <= SPTYP_LAST_EXPONENT; i++)
    {
        int disc = 1 << i;
        if (disciplines & disc)
        {
            int skill = you.skills[spell_type2skill(disc)];

            weight += skill;
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
    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_type stype = which_spell_in_book(book, i);
        if (stype == SPELL_NO_SPELL)
            continue;

        // Skip over spells already seen.
        if (you.seen_spell[stype])
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
    case GOD_FEDHAS:
        return skill == SK_NECROMANCY;
    case GOD_XOM:
    case GOD_NEMELEX_XOBEH:
    case GOD_KIKUBAAQUDGHA:
    case GOD_VEHUMET:
    case GOD_ASHENZARI:
    case GOD_NO_GOD:
        return skill == SK_INVOCATIONS;
    default:
        return false;
    }
}

static bool _do_book_acquirement(item_def &book, int agent)
{
    // items() shouldn't make book a randart for acquirement items.
    ASSERT(!is_random_artefact(book));

    int          level       = (you.skills[SK_SPELLCASTING] + 2) / 3;

    level = max(1, level);

    if (agent == GOD_XOM)
        level = random_range(1, 9);

    int choice = NUM_BOOKS;

    bool knows_magic = false;
    // Manuals are too useful for Xom, and useless when gifted from Sif Muna.
    if (agent != GOD_XOM && agent != GOD_SIF_MUNA)
    {
        int magic_weights = 0;
        int other_weights = 0;

        for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
        {
            skill_type sk = static_cast<skill_type>(i);
            int weight = you.skills[sk];

            // Anyone can get Spellcasting 1. Doesn't prove anything.
            if (sk == SK_SPELLCASTING && weight >= 1)
                weight--;

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
        if (magic_weights * 3 < other_weights
            && x_chance_in_y(other_weights, 2*magic_weights + other_weights))
        {
            choice = BOOK_MANUAL;
            if (magic_weights > 0)
                knows_magic = true;
        }
    }

    if (choice == NUM_BOOKS)
    {
        choice = random_choose_weighted(
                                        30, BOOK_RANDART_THEME,
           agent == GOD_SIF_MUNA ? 10 : 40, NUM_BOOKS, // normal books
                     level == -1 ?  0 :  1, BOOK_RANDART_LEVEL, 0);
    }

    // Acquired randart books have a chance of being named after the player.
    string owner = "";
    if (agent == AQ_SCROLL && one_chance_in(12)
        || agent == AQ_CARD_GENIE && one_chance_in(6))
    {
        owner = you.your_name;
    }

    switch (choice)
    {
    default:
    case NUM_BOOKS:
    {
        int total_weights = 0;

        // Pick a random spellbook according to unknown spells contained.
        int weights[MAX_FIXED_BOOK+1];
        for (int bk = 0; bk <= MAX_FIXED_BOOK; bk++)
        {
            if (bk > MAX_NORMAL_BOOK && agent == GOD_SIF_MUNA)
            {
                weights[bk] = 0;
                continue;
            }

#if TAG_MAJOR_VERSION == 34
            if (bk == BOOK_STALKING)
            {
                weights[bk] = 0;
                continue;
            }
#endif
            weights[bk]    = _book_weight(static_cast<book_type>(bk));
            total_weights += weights[bk];
        }

        if (total_weights > 0)
        {
            book.sub_type = choose_random_weighted(weights,
                                                   weights + ARRAYSZ(weights));
            break;
        }
        // else intentional fall-through
    }
    case BOOK_RANDART_THEME:
        book.sub_type = BOOK_RANDART_THEME;
        if (!make_book_theme_randart(book, 0, 0, 5 + coinflip(), 20,
                                     SPELL_NO_SPELL, owner))
        {
            return false;
        }
        break;

    case BOOK_RANDART_LEVEL:
    {
        book.sub_type  = BOOK_RANDART_LEVEL;
        if (!make_book_level_randart(book, level, -1, owner))
            return false;
        break;
    }

    case BOOK_MANUAL:
    {
        // The Tome of Destruction is rare enough we won't change this.
        if (book.sub_type == BOOK_DESTRUCTION)
            return true;

        int weights[NUM_SKILLS];
        int total_weights = 0;

        for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; i++)
        {
            skill_type sk = static_cast<skill_type>(i);

            int skl = you.skills[sk];

            if (skl == 27 || is_useless_skill(sk))
            {
                weights[sk] = 0;
                continue;
            }

            int w = (skl < 12) ? skl + 3 : max(0, 25 - skl);

            // Give a bonus for some highly sought after skills.
            if (sk == SK_FIGHTING || sk == SK_ARMOUR || sk == SK_SPELLCASTING
                || sk == SK_INVOCATIONS || sk == SK_EVOCATIONS)
            {
                w += 5;
            }

            // Greatly reduce the chances of getting a manual for a skill
            // you couldn't use unless you switched your religion.
            if (_skill_useless_with_god(sk))
                w /= 2;

            // If we don't have any magic skills, make non-magic skills
            // more likely.
            if (!knows_magic && !_is_magic_skill(sk))
                w *= 2;

            weights[sk] = w;
            total_weights += w;
        }

        // Are we too skilled to get any manuals?
        if (total_weights == 0)
            return false;

        book.sub_type = BOOK_MANUAL;
        book.plus     = choose_random_weighted(weights, weights + NUM_SKILLS);
        // Set number of bonus skill points.
        book.plus2    = random_range(2000, 3000);
        break;
    } // manuals
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
        return you.skills[SK_NECROMANCY] / 2;
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

    if (item.sub_type != ARM_ANIMAL_SKIN
        && item.sub_type >= ARM_MIN_UNBRANDED
        && item.sub_type <= ARM_MAX_UNBRANDED)
    {
        // These are always interesting, even with no brand.
        // May still be redundant, but that has another check.
        return false;
    }

    return get_armour_ego_type(item) == SPARM_NORMAL;
}

int acquirement_create_item(object_class_type class_wanted,
                            int agent, bool quiet,
                            const coord_def &pos, bool debug)
{
    ASSERT(class_wanted != OBJ_RANDOM);

    const bool divine = (agent == GOD_OKAWARU || agent == GOD_XOM
                         || agent == GOD_TROG);
    int thing_created = NON_ITEM;
    int quant = 1;
#define ITEM_LEVEL (divine ? MAKE_GIFT_ITEM : MAKE_GOOD_ITEM)
#define MAX_ACQ_TRIES 40
    for (int item_tries = 0; item_tries < MAX_ACQ_TRIES; item_tries++)
    {
        // This may clobber class_wanted (e.g. staves/rods, or vampire food)
        int type_wanted = _find_acquirement_subtype(class_wanted, quant,
                                                    divine, agent);

        // Don't generate randart books in items(), we do that
        // ourselves.
        bool want_arts = (class_wanted != OBJ_BOOKS);
        if (agent == GOD_TROG && !one_chance_in(3))
            want_arts = false;

        thing_created = items(want_arts, class_wanted, type_wanted, true,
                               ITEM_LEVEL, MAKE_ITEM_RANDOM_RACE,
                               0, 0, agent);

        if (thing_created == NON_ITEM)
            continue;

        item_def &doodad(mitm[thing_created]);

        // Not a god, prefer better brands.
        if (!divine && !is_artefact(doodad) && doodad.base_type == OBJ_WEAPONS)
        {
            while (_weapon_brand_quality(get_weapon_brand(doodad),
                                        is_range_weapon(doodad)) < random2(6))
            {
                reroll_brand(doodad, ITEM_LEVEL);
            }
        }

        // Try to not generate brands that were already seen, although unlike
        // jewelry and books, this is not absolute.
        while (!is_artefact(doodad)
               && (doodad.base_type == OBJ_WEAPONS
                     && you.seen_weapon[doodad.sub_type]
                        & (1<<get_weapon_brand(doodad))
                   || doodad.base_type == OBJ_ARMOUR
                     && you.seen_armour[doodad.sub_type]
                        & (1<<get_armour_ego_type(doodad)))
               && !one_chance_in(5))
        {
            reroll_brand(doodad, ITEM_LEVEL);
        }

        // For plain armour, try to change the subtype to something
        // matching a currently unfilled equipment slot.
        if (doodad.base_type == OBJ_ARMOUR && !is_artefact(doodad))
        {
            const special_armour_type sparm = get_armour_ego_type(doodad);

            if (agent != GOD_XOM
                  && you.seen_armour[doodad.sub_type] & (1 << sparm)
                  && x_chance_in_y(MAX_ACQ_TRIES - item_tries, MAX_ACQ_TRIES + 5)
                || !divine
                   && you.seen_armour[doodad.sub_type]
                   && !one_chance_in(3)
                   && item_tries < 20)
            {
                // We have seen the exact item already, it's very unlikely
                // extras will do any good.
                // For scroll acquirement, prefer base items not seen before
                // as well, even if you didn't see the exact brand yet.
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                continue;
            }

            // Try to fill empty slots.
            if ((_is_armour_plain(doodad)
                 || get_armour_slot(doodad) == EQ_BODY_ARMOUR && coinflip())
                && _armour_slot_seen((armour_type)doodad.sub_type))
            {
                armour_type at = _pick_unseen_armour();
                if (at != NUM_ARMOURS)
                {
                    destroy_item(thing_created, true);
                    thing_created = items(true, OBJ_ARMOUR, at, true,
                               ITEM_LEVEL, MAKE_ITEM_RANDOM_RACE,
                               0, 0, agent);
                }
                else if (agent != GOD_XOM && one_chance_in(3))
                {
                    // If the item is plain and there aren't any
                    // unfilled slots, we might want to roll again.
                    destroy_item(thing_created, true);
                    thing_created = NON_ITEM;
                    continue;
                }
            }
        }

        // bias racial make towards the player
        if (!is_artefact(doodad))
            maybe_set_item_race(doodad, get_species_race(you.species), 3);

        if (doodad.base_type == OBJ_WEAPONS
               && !can_wield(&doodad, false, true)
            || doodad.base_type == OBJ_ARMOUR
               && !can_wear_armour(doodad, false, true))
        {
            destroy_item(thing_created, true);
            thing_created = NON_ITEM;
            continue;
        }

        // Trog does not gift the Wrath of Trog, nor weapons of pain
        // (which work together with Necromantic magic).
        if (agent == GOD_TROG)
        {
            // ... but he loves the antimagic brand specially.
            if (coinflip() && doodad.base_type == OBJ_WEAPONS
                && !is_range_weapon(doodad) && !is_unrandom_artefact(doodad))
            {
                set_item_ego_type(doodad, OBJ_WEAPONS, SPWPN_ANTIMAGIC);
            }

            int brand = get_weapon_brand(doodad);
            if (brand == SPWPN_PAIN
                || is_unrandom_artefact(doodad)
                   && (doodad.special == UNRAND_TROG
                       || doodad.special == UNRAND_WUCAD_MU))
            {
                destroy_item(thing_created, true);
                thing_created = NON_ITEM;
                continue;
            }
        }

        // MT - Check: god-gifted weapons and armour shouldn't kill you.
        // Except Xom.
        if ((agent == GOD_TROG || agent == GOD_OKAWARU)
            && is_artefact(doodad))
        {
            artefact_properties_t  proprt;
            artefact_wpn_properties(doodad, proprt);

            // Check vs. stats. positive stats will automatically fall
            // through.  As will negative stats that won't kill you.
            if (-proprt[ARTP_STRENGTH] >= you.strength()
                || -proprt[ARTP_INTELLIGENCE] >= you.intel()
                || -proprt[ARTP_DEXTERITY] >= you.dex())
            {
                // Try again.
                destroy_item(thing_created);
                thing_created = NON_ITEM;
                continue;
            }
        }

        // Sif Muna shouldn't gift special books.
        // (The spells therein are still fair game for randart books.)
        if (agent == GOD_SIF_MUNA
            && doodad.sub_type >= MIN_RARE_BOOK
            && doodad.sub_type <= MAX_RARE_BOOK)
        {
            ASSERT(doodad.base_type == OBJ_BOOKS);

            // Try again.
            destroy_item(thing_created);
            thing_created = NON_ITEM;
            continue;
        }

        ASSERT(doodad.is_valid());

        if (class_wanted == OBJ_WANDS)
            doodad.plus = max(static_cast<int>(doodad.plus), 3 + random2(3));
        else if (class_wanted == OBJ_GOLD)
        {
            // New gold acquirement formula from dpeg.
            // Min=220, Max=5520, Mean=1218, Std=911
            doodad.quantity = 10 * (20
                                    + roll_dice(1, 20)
                                    + (roll_dice(1, 8)
                                       * roll_dice(1, 8)
                                       * roll_dice(1, 8)));
        }
        else if (class_wanted == OBJ_MISSILES && !divine)
            doodad.quantity *= 5;
        else if (quant > 1)
            doodad.quantity = quant;

        if (is_blood_potion(doodad))
            init_stack_blood_potions(doodad);

        // Remove curse flag from item, unless worshipping Ashenzari.
        if (you_worship(GOD_ASHENZARI))
            do_curse_item(doodad, true);
        else
            do_uncurse_item(doodad, false);

        if (doodad.base_type == OBJ_BOOKS)
        {
            if (!_do_book_acquirement(doodad, agent))
            {
                destroy_item(doodad, true);
                return _failed_acquirement(quiet);
            }
            // Don't mark books as seen if only generated for the
            // acquirement statistics.
            if (!debug)
                mark_had_book(doodad);
        }
        else if (doodad.base_type == OBJ_JEWELLERY)
        {
            switch (doodad.sub_type)
            {
            case RING_PROTECTION:
            case RING_STRENGTH:
            case RING_INTELLIGENCE:
            case RING_DEXTERITY:
            case RING_EVASION:
                // Make sure plus is >= 1.
                doodad.plus = max(abs((int) doodad.plus), 1);
                break;

            case RING_SLAYING:
                // Two plusses to handle here, and accuracy can be +0.
                doodad.plus = abs(doodad.plus);
                doodad.plus2 = max(abs((int) doodad.plus2), 2);
                break;

            case RING_HUNGER:
            case AMU_INACCURACY:
                // These are the only truly bad pieces of jewellery.
                if (!one_chance_in(9))
                    make_item_randart(doodad);
                break;

            default:
                break;
            }
        }
        else if (doodad.base_type == OBJ_WEAPONS
                 && !is_unrandom_artefact(doodad))
        {
            // These can never get egos, and mundane versions are quite common, so
            // guarantee artefact status.  Rarity is a bit low to compensate.
            if (is_giant_club_type(doodad.sub_type))
            {
                if (!one_chance_in(25))
                    make_item_randart(doodad, true);
            }

            int plusmod = random2(4);
            if (agent == GOD_TROG)
            {
                // More damage, less accuracy.
                doodad.plus  -= plusmod;
                doodad.plus2 += plusmod;
                if (!is_artefact(doodad))
                    doodad.plus = max(static_cast<int>(doodad.plus), 0);
            }
            else if (agent == GOD_OKAWARU)
            {
                // More accuracy, less damage.
                doodad.plus  += plusmod;
                doodad.plus2 -= plusmod;
                if (!is_artefact(doodad))
                    doodad.plus2 = max(static_cast<int>(doodad.plus2), 0);
            }
        }
        else if (is_deck(doodad))
        {
            doodad.special = !one_chance_in(3) ? DECK_RARITY_LEGENDARY :
                             !one_chance_in(5) ? DECK_RARITY_RARE :
                                                 DECK_RARITY_COMMON;
        }

        // Last check: don't acquire items your god hates.
        // Temporarily mark the type as ID'd for the purpose of checking if
        // it is a hated brand (this addresses, e.g., Elyvilon followers
        // immediately identifying evil weapons).
        // Note that Xom will happily give useless items!
        int oldflags = doodad.flags;
        doodad.flags |= ISFLAG_KNOW_TYPE;
        if ((is_useless_item(doodad, false) && agent != GOD_XOM)
            || god_hates_item(doodad))
        {
            destroy_item(thing_created);
            thing_created = NON_ITEM;
            continue;
        }
        doodad.flags = oldflags;
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
    if (feat_destroys_item(grd(pos), mitm[thing_created])
        && (agent > GOD_NO_GOD)
        && (agent < NUM_GODS))
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
        mitm[thing_created].props["acquired"].get_int() = agent;
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
        bad_class.set(OBJ_WANDS);
        bad_class.set(OBJ_STAVES);
        bad_class.set(OBJ_RODS);
    }
    bad_class.set(OBJ_FOOD, you_foodless_normally() && !you_worship(GOD_FEDHAS));

    static struct { object_class_type type; const char* name; } acq_classes[] =
    {
        { OBJ_WEAPONS,    "Weapon" },
        { OBJ_ARMOUR,     "Armour" },
        { OBJ_JEWELLERY,  "Jewellery" },
        { OBJ_BOOKS,      "Book" },
        { OBJ_STAVES,     "Staff" },
        { OBJ_WANDS,      "Wand" },
        { OBJ_MISCELLANY, "Miscellaneous" },
        { OBJ_FOOD,       0 }, // amended below
        { OBJ_GOLD,       "Gold" },
        { OBJ_MISSILES,   "Ammunition" },
    };
    ASSERT(acq_classes[7].type == OBJ_FOOD);
    acq_classes[7].name = you_worship(GOD_FEDHAS) ? "Fruit":
                          you.species == SP_VAMPIRE  ? "Blood":
                                                       "Food";

    int thing_created = NON_ITEM;

    if (item_index == NULL)
        item_index = &thing_created;

    *item_index = NON_ITEM;

    while (class_wanted == OBJ_RANDOM)
    {
        ASSERT(!quiet);
        mesclr();

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
                mpr(line.c_str());
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
