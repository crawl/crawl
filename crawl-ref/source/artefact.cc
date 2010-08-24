/*
 *  File:       artefact.cc
 *  Summary:    Random and unrandom artefact functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "artefact.h"

#include <cstdlib>
#include <climits>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"
#include "options.h"

#include "areas.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "describe.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "place.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "species.h"
#include "spl-book.h"
#include "stuff.h"
#include "view.h" // Elemental colours for unrandarts
#include "shout.h"

// The initial generation of a randart is very simple - it occurs in
// dungeon.cc and consists of giving it a few random things - plus &
// plus2 mainly.

static bool _god_fits_artefact(const god_type which_god, const item_def &item,
                               bool name_check_only = false)
{
    if (which_god == GOD_NO_GOD)
        return (false);

    // Jellies can't eat artefacts, so their god won't make any.
    if (which_god == GOD_JIYVA)
        return (false);

    // First check the item's base_type and sub_type, then check the
    // item's brand and other randart properties.
    bool type_bad = false;
    switch (which_god)
    {
    case GOD_ELYVILON:
        // Peaceful healer god: no weapons, no berserking.
        if (item.base_type == OBJ_WEAPONS)
            type_bad = true;

        if (item.base_type == OBJ_JEWELLERY && item.sub_type == AMU_RAGE)
            type_bad = true;
        break;

    case GOD_OKAWARU:
        // Precision fighter god: no inaccuracy.
        if (item.base_type == OBJ_JEWELLERY && item.sub_type == AMU_INACCURACY)
            type_bad = true;
        break;

    case GOD_ZIN:
        // Lawful god: no increased hunger.
        if (item.base_type == OBJ_JEWELLERY && item.sub_type == RING_HUNGER)
            type_bad = true;
        break;

    case GOD_SIF_MUNA:
    case GOD_KIKUBAAQUDGHA:
    case GOD_VEHUMET:
        // The magic gods: no weapons, no preventing spellcasting.
        if (item.base_type == OBJ_WEAPONS)
            type_bad = true;
        break;

    case GOD_TROG:
        // Anti-magic god: no spell use, no enhancing magic.
        if (item.base_type == OBJ_BOOKS)
            type_bad = true;

        if (item.base_type == OBJ_JEWELLERY && (item.sub_type == RING_WIZARDRY
            || item.sub_type == RING_FIRE || item.sub_type == RING_ICE
            || item.sub_type == RING_MAGICAL_POWER))
        {
            type_bad = true;
        }
        break;

    case GOD_CHEIBRIADOS:
        // Slow god: no quick blades, no berserking, no resist slowing.
        if (item.base_type == OBJ_WEAPONS && item.sub_type == WPN_QUICK_BLADE)
            type_bad = true;

        if (item.base_type == OBJ_JEWELLERY && item.sub_type == AMU_RAGE)
            type_bad = true;
        break;

    default:
        break;
    }

    if (type_bad && !name_check_only)
    {
        ASSERT(!"God attempting to gift invalid type of item.");
        mprf(MSGCH_ERROR, "%s attempting to gift invalid type of item.",
             god_name(which_god).c_str());
        // Prevent infinite loop in make_item_randart().
        return (true);
    }

    if (type_bad)
        return (false);

    const int brand = get_weapon_brand(item);
    const int ego   = get_armour_ego_type(item);

    if (is_evil_god(which_god) && brand == SPWPN_HOLY_WRATH)
        return (false);
    else if (is_good_god(which_god)
             && (brand == SPWPN_DRAINING || brand == SPWPN_PAIN
                 || brand == SPWPN_VAMPIRICISM || brand == SPWPN_REAPING
                 || brand == SPWPN_CHAOS
                 || is_demonic(item)
                 || artefact_wpn_property(item, ARTP_CURSED) != 0))
    {
        return (false);
    }

    switch (which_god)
    {
    case GOD_BEOGH:
        // Orc god: no orc slaying.
        if (brand == SPWPN_ORC_SLAYING)
            return (false);
        break;

    case GOD_ELYVILON:
        // Peaceful healer god: no berserking.
        if (artefact_wpn_property(item, ARTP_ANGRY)
            || artefact_wpn_property(item, ARTP_BERSERK))
        {
            return (false);
        }
        break;

    case GOD_ZIN:
        // Lawful god: no mutagenics.
        if (artefact_wpn_property(item, ARTP_MUTAGENIC))
            return (false);
        break;

    case GOD_SHINING_ONE:
        // Crusader god: holiness, honourable combat.
        if (item.base_type == OBJ_WEAPONS && brand != SPWPN_HOLY_WRATH)
            return (false);

        if (artefact_wpn_property(item, ARTP_INVISIBLE)
            || artefact_wpn_property(item, ARTP_STEALTH) > 0)
        {
            return (false);
        }
        break;

    case GOD_LUGONU:
        // Abyss god: corruption.
        if (item.base_type == OBJ_WEAPONS && brand != SPWPN_DISTORTION)
            return (false);
        break;

    case GOD_SIF_MUNA:
    case GOD_KIKUBAAQUDGHA:
    case GOD_VEHUMET:
        // The magic gods: no preventing spellcasting.
        if (artefact_wpn_property(item, ARTP_PREVENT_SPELLCASTING))
            return (false);
        break;

    case GOD_TROG:
        // Anti-magic god: no spell use, no enhancing magic.
        if (brand == SPWPN_PAIN) // Pain involves necromantic spell use.
            return (false);

        if (artefact_wpn_property(item, ARTP_MAGICAL_POWER))
            return (false);
        break;

    case GOD_FEDHAS:
        // Fedhas forbids necromancy involving corpses, only reaping
        // really applies.
        if (brand == SPWPN_REAPING)
        {
            return (false);
        }
        break;

    case GOD_CHEIBRIADOS:
        // Slow god: no speed, no berserking.
        if (brand == SPWPN_SPEED)
            return (false);

        if (ego == SPARM_RUNNING)
            return (false);

        if (artefact_wpn_property(item, ARTP_ANGRY)
            || artefact_wpn_property(item, ARTP_BERSERK))
        {
            return (false);
        }
        break;

    default:
        break;
    }

    return (true);
}

static std::string _replace_name_parts(const std::string name_in,
                                       const item_def& item)
{
    std::string name = name_in;

    god_type god_gift;
    (void) origin_is_god_gift(item, &god_gift);

    // Don't allow "player's Death" type names for god gifts (except
    // for those from Xom).
    if (name.find("@player_death@", 0) != std::string::npos
        || name.find("@player_doom@", 0) != std::string::npos)
    {
        if (god_gift == GOD_NO_GOD || god_gift == GOD_XOM)
        {
            name = replace_all(name, "@player_death@",
                               "@player_name@"
                               + getRandNameString("killer_name"));
            name = replace_all(name, "@player_doom@",
                               "@player_name@'s "
                               + getRandNameString("death_or_doom"));
        }
        else
        {
            // Simply overwrite the name with one of type "God's Favour".
            name = "of ";
            name += god_name(god_gift, false);
            name += "'s ";
            name += getRandNameString("divine_esteem");
        }
    }
    name = replace_all(name, "@player_name@", you.your_name);

    name = replace_all(name, "@player_species@",
                 species_name(static_cast<species_type>(you.species), true));

    if (name.find("@branch_name@", 0) != std::string::npos)
    {
        std::string place;
        if (one_chance_in(5))
        {
            switch (random2(8))
            {
            case 0:
            case 1:
            default:
               place = "the Abyss";
               break;
            case 2:
            case 3:
               place = "Pandemonium";
               break;
            case 4:
            case 5:
               place = "the Realm of Zot";
               break;
            case 6:
               place = "the Labyrinth";
               break;
            case 7:
               place = "the Portal Chambers";
               break;
            }
        }
        else
        {
            const branch_type branch =
                     static_cast<branch_type>(random2(BRANCH_TARTARUS));
            place = place_name( get_packed_place(branch, 1, LEVEL_DUNGEON),
                                true, false );
        }
        if (!place.empty())
            name = replace_all(name, "@branch_name@", place);
    }

    // Occasionally use long name for Xom (see religion.cc).
    name = replace_all(name, "@xom_name@", god_name(GOD_XOM, coinflip()));

    if (name.find("@god_name@", 0) != std::string::npos)
    {
        god_type which_god;

        // God gifts will always get the gifting god's name.
        if (god_gift != GOD_NO_GOD)
            which_god = god_gift;
        else
        {
            do
                which_god = static_cast<god_type>(random2(NUM_GODS - 1) + 1);
            while (!_god_fits_artefact(which_god, item, true));
        }

        name = replace_all(name, "@god_name@", god_name(which_god, false));
    }

    // copied from monster speech handling (mon-util.cc):
    // The proper possessive for a word ending in an "s" is to
    // put an apostrophe after the "s": "Chris" -> "Chris'",
    // not "Chris" -> "Chris's".  Stupid English language...
    name = replace_all(name, "s's", "s'");

    return name;
}

// Remember: disallow unrandart creation in Abyss/Pan.

// Functions defined in art-func.h are referenced in art-data.h
#include "art-func.h"

static unrandart_entry unranddata[] = {
#include "art-data.h"
};

static unrandart_entry *_seekunrandart( const item_def &item );

bool is_known_artefact( const item_def &item )
{
    if (!item_type_known(item))
        return (false);

    return (is_artefact(item));
}

bool is_artefact( const item_def &item )
{
    return (item.flags & ISFLAG_ARTEFACT_MASK);
}

// returns true is item is a pure randart
bool is_random_artefact( const item_def &item )
{
    return (item.flags & ISFLAG_RANDART);
}

// returns true if item in an unrandart
bool is_unrandom_artefact( const item_def &item )
{
    return (item.flags & ISFLAG_UNRANDART);
}

bool is_special_unrandom_artefact( const item_def &item )
{
    return (item.flags & ISFLAG_UNRANDART
            && (_seekunrandart(item)->flags & UNRAND_FLAG_SPECIAL));
}

unique_item_status_type get_unique_item_status(const item_def& item)
{
    if (item.flags & ISFLAG_UNRANDART)
        return get_unique_item_status(item.special);

    return (UNIQ_NOT_EXISTS);
}

unique_item_status_type get_unique_item_status( int art )
{
    ASSERT(art > UNRAND_START && art < UNRAND_LAST);
    return (you.unique_items[art - UNRAND_START]);
}

void set_unique_item_status(const item_def& item,
                            unique_item_status_type status )
{
    if (item.flags & ISFLAG_UNRANDART)
        set_unique_item_status(item.special, status);
}

void set_unique_item_status( int art, unique_item_status_type status )
{
    ASSERT(art > UNRAND_START && art < UNRAND_LAST);
    you.unique_items[art - UNRAND_START] = status;
}

static long _calc_seed( const item_def &item )
{
    return (item.special & RANDART_SEED_MASK);
}

void artefact_desc_properties( const item_def &item,
                              artefact_properties_t &proprt,
                              artefact_known_props_t &known,
                              bool force_fake_props)
{
    // Randart books have no randart properties.
    if (item.base_type == OBJ_BOOKS)
        return;

    artefact_wpn_properties( item, proprt, known);

    if (!force_fake_props && item_ident( item, ISFLAG_KNOW_PROPERTIES ))
        return;

    // Only jewellery need fake randart properties.
    if (item.base_type != OBJ_JEWELLERY)
        return;

    artefact_prop_type fake_rap  = ARTP_NUM_PROPERTIES;
    int               fake_plus = 1;

    // The base jewellery type is one whose property is revealed by
    // wearing it, but whose property isn't revealed by having
    // ISFLAG_KNOW_PLUSES set.  For a randart with a base type of, for
    // example, a ring of strength, wearing it sets
    // ISFLAG_KNOW_PLUSES, which reveals the ring's strength plus.
    switch (item.sub_type)
    {
    case RING_INVISIBILITY:
        fake_rap = ARTP_INVISIBLE;
        break;

    case RING_TELEPORTATION:
        fake_rap = ARTP_CAUSE_TELEPORTATION;
        break;

    case RING_MAGICAL_POWER:
        fake_rap  = ARTP_MAGICAL_POWER;
        fake_plus = 9;
        break;

    case RING_LEVITATION:
        fake_rap = ARTP_LEVITATE;
        break;

    case AMU_RAGE:
        fake_rap = ARTP_BERSERK;
        break;
    }

    if (fake_rap != ARTP_NUM_PROPERTIES)
    {
        proprt[fake_rap] += fake_plus;

        if (item_ident( item, ISFLAG_KNOW_PROPERTIES )
            || item_ident( item, ISFLAG_KNOW_TYPE ))
        {
            known[fake_rap] = true;
        }

        return;
    }

    if (!force_fake_props)
        return;

    // For auto-inscribing randart jewellery, force_fake_props folds as
    // much info about the base type as possible into the randarts
    // property struct.

    artefact_prop_type fake_rap2  = ARTP_NUM_PROPERTIES;
    int               fake_plus2 = 1;

    switch (item.sub_type)
    {
    case RING_PROTECTION:
        fake_rap  = ARTP_AC;
        fake_plus = item.plus;
        break;

    case RING_PROTECTION_FROM_FIRE:
        fake_rap = ARTP_FIRE;
        break;

    case RING_POISON_RESISTANCE:
        fake_rap = ARTP_POISON;
        break;

    case RING_PROTECTION_FROM_COLD:
        fake_rap = ARTP_COLD;
        break;

    case RING_SLAYING:
        fake_rap   = ARTP_ACCURACY;
        fake_plus  = item.plus;
        fake_rap2  = ARTP_DAMAGE;
        fake_plus2 = item.plus2;
        break;

    case RING_SEE_INVISIBLE:
        fake_rap = ARTP_EYESIGHT;
        break;

    case RING_HUNGER:
        fake_rap = ARTP_METABOLISM;
        break;

    case RING_EVASION:
        fake_rap  = ARTP_EVASION;
        fake_plus = item.plus;
        break;

    case RING_STRENGTH:
        fake_rap  = ARTP_STRENGTH;
        fake_plus = item.plus;
        break;

    case RING_INTELLIGENCE:
        fake_rap  = ARTP_INTELLIGENCE;
        fake_plus = item.plus;
        break;

    case RING_DEXTERITY:
        fake_rap  = ARTP_DEXTERITY;
        fake_plus = item.plus;
        break;

    case RING_LIFE_PROTECTION:
        fake_rap = ARTP_NEGATIVE_ENERGY;
        break;

    case RING_PROTECTION_FROM_MAGIC:
        fake_rap = ARTP_MAGIC;
        break;

    case RING_FIRE:
        fake_rap   = ARTP_FIRE;
        fake_rap2  = ARTP_COLD;
        fake_plus2 = -1;
        break;

    case RING_ICE:
        fake_rap   = ARTP_COLD;
        fake_rap2  = ARTP_FIRE;
        fake_plus2 = -1;
        break;
    }

    if (fake_rap != ARTP_NUM_PROPERTIES && fake_plus != 0)
        proprt[fake_rap] += fake_plus;

    if (fake_rap2 != ARTP_NUM_PROPERTIES && fake_plus2 != 0)
        proprt[fake_rap2] += fake_plus2;

    if (item_ident( item, ISFLAG_KNOW_PROPERTIES )
        || item_ident( item, ISFLAG_KNOW_TYPE ))
    {
        if (fake_rap != ARTP_NUM_PROPERTIES && proprt[fake_rap] != 0)
            known[fake_rap] = true;

        if (fake_rap2 != ARTP_NUM_PROPERTIES && proprt[fake_rap2] != 0)
            known[fake_rap2] = true;
    }
}

inline static void _randart_propset( artefact_properties_t &p,
                                     artefact_prop_type pt,
                                     int value,
                                     bool neg )
{
    // This shouldn't be called with 0, else no property gets added after all.
    ASSERT(value != 0);
    p[pt] = (neg? -value : value);
}

static int _randart_add_one_property( const item_def &item,
                                      artefact_properties_t &proprt )
{
    // This function assumes that at least some properties are still possible.
    const object_class_type cl = item.base_type;
    const int               ty = item.sub_type;

    const bool negench = one_chance_in(4);

    const artefact_prop_type artprops[] = { ARTP_AC, ARTP_EVASION,
                                            ARTP_STRENGTH, ARTP_INTELLIGENCE,
                                            ARTP_DEXTERITY };

    do
    {
        int prop = RANDOM_ELEMENT(artprops);

        if (proprt[prop] != 0)
            continue;

        switch (prop)
        {
        case ARTP_AC:
            if (cl == OBJ_ARMOUR
                || cl == OBJ_JEWELLERY && ty == RING_PROTECTION)
            {
                continue;
            }
            _randart_propset(proprt, ARTP_AC,
                             1 + random2(3) + random2(3) + random2(3),
                             negench);
            break;

        case ARTP_EVASION:
            if (cl == OBJ_JEWELLERY && ty == RING_EVASION)
                continue;

            _randart_propset(proprt, ARTP_EVASION,
                             1 + random2(3) + random2(3) + random2(3),
                             negench);
            break;

        case ARTP_STRENGTH:
            if (cl == OBJ_JEWELLERY && ty == RING_STRENGTH)
                continue;

            _randart_propset(proprt, ARTP_STRENGTH,
                             1 + random2(3) + random2(3),
                             negench);
            break;

        case ARTP_INTELLIGENCE:
            if (cl == OBJ_JEWELLERY && ty == RING_INTELLIGENCE)
                continue;

            _randart_propset(proprt, ARTP_INTELLIGENCE,
                             1 + random2(3) + random2(3),
                             negench);
            break;

        case ARTP_DEXTERITY:
            if (cl == OBJ_JEWELLERY && ty == RING_DEXTERITY)
                continue;

            _randart_propset(proprt, ARTP_DEXTERITY,
                             1 + random2(3) + random2(3),
                             negench);
            break;
        }
    }
    while (false);

    return (negench ? -1 : 1);
}

// An artefact will pass this check if it has any non-stat properties, and
// also if it has enough stat (AC, EV, Str, Dex, Int, Acc, Dam) properties.
// Returns how many (more) stat properties we need to add.
static int _need_bonus_stat_props( const artefact_properties_t &proprt )
{
    int num_stats   = 0;
    int num_acc_dam = 0;

    for (int i = 0; i < ARTP_NUM_PROPERTIES; i++)
    {
        if (i == ARTP_CURSED)
            continue;

        if (proprt[i] == 0)
            continue;

        if (i >= ARTP_AC && i <= ARTP_DEXTERITY)
            num_stats++;
        else if (i >= ARTP_ACCURACY && i <= ARTP_DAMAGE)
            num_acc_dam++;
        else
            return (0);
    }

    num_stats += num_acc_dam;

    // If an artefact has no properties at all, something is wrong.
    if (num_stats == 0)
        return (2);

    // Artefacts with two of more stat-only properties are fine.
    if (num_stats >= 2)
        return (0);

    // If an artefact has exactly one stat property, we might want to add
    // some more. (66% chance if it's Acc/Dam, else always.)
    if (num_acc_dam > 0)
        return (random2(3));

    return (1 + random2(2));
}

void static _get_randart_properties(const item_def &item,
                                    artefact_properties_t &proprt)
{
    const object_class_type aclass = item.base_type;
    const int atype = item.sub_type;
    int power_level = 0;

    const long seed = _calc_seed(item);
    rng_save_excursion exc;
    seed_rng(seed);

    if (aclass == OBJ_ARMOUR)
        power_level = item.plus / 2 + 2;
    else if (aclass == OBJ_JEWELLERY)
        power_level = 1 + random2(3) + random2(2);
    else // OBJ_WEAPON
        power_level = item.plus / 3 + item.plus2 / 3;

    if (power_level < 0)
        power_level = 0;

    proprt.init(0);

    if (aclass == OBJ_WEAPONS) // Only weapons get brands, of course.
    {
        proprt[ARTP_BRAND] = SPWPN_FLAMING + random2(15);        // brand

        if (one_chance_in(6))
            proprt[ARTP_BRAND] = SPWPN_FLAMING + random2(2);

        if (one_chance_in(6))
            proprt[ARTP_BRAND] = SPWPN_ORC_SLAYING + random2(5);

        if (one_chance_in(6))
            proprt[ARTP_BRAND] = SPWPN_VORPAL;

        if (proprt[ARTP_BRAND] == SPWPN_PROTECTION || proprt[ARTP_BRAND] == SPWPN_EVASION)
            proprt[ARTP_BRAND] = SPWPN_NORMAL;      // no protection or evasion

        if (is_range_weapon(item))
        {
            proprt[ARTP_BRAND] = SPWPN_NORMAL;

            if (one_chance_in(3))
            {
                int tmp = random2(20);

                proprt[ARTP_BRAND] = (tmp >= 18) ? SPWPN_SPEED :
                                     (tmp >= 14) ? SPWPN_PENETRATION :
                                     (tmp >= 12) ? SPWPN_REAPING :
                                     (tmp >=  8) ? SPWPN_VENOM
                                                 : SPWPN_VORPAL + random2(3);

                if (atype == WPN_BLOWGUN
                    && proprt[ARTP_BRAND] != SPWPN_SPEED)
                {
                    proprt[ARTP_BRAND] = SPWPN_NORMAL;
                }

                // Removed slings from getting the venom attribute: they can
                // be branded with it now using Poison Weapon, and perma-branded
                // via vorpalise weapon.

                if (atype == WPN_CROSSBOW && one_chance_in(5))
                    proprt[ARTP_BRAND] = SPWPN_ELECTROCUTION;

                // XXX: Penetration is only allowed on crossbows. This may change
                // in future.
                if (atype != WPN_CROSSBOW && proprt[ARTP_BRAND] == SPWPN_PENETRATION)
                    proprt[ARTP_BRAND] = SPWPN_NORMAL;

                // XXX: Only allow reaping brand on bows. This may change.
                if (atype != WPN_BOW && atype != WPN_LONGBOW
                    && proprt[ARTP_BRAND] == SPWPN_REAPING)
                {
                    proprt[ARTP_BRAND] = SPWPN_NORMAL;
                }
            }
        }

        if (is_demonic(item))
        {
            switch (random2(9))
            {
            case 0:
                proprt[ARTP_BRAND] = SPWPN_DRAINING;
                break;
            case 1:
                proprt[ARTP_BRAND] = SPWPN_FLAMING;
                break;
            case 2:
                proprt[ARTP_BRAND] = SPWPN_FREEZING;
                break;
            case 3:
                proprt[ARTP_BRAND] = SPWPN_ELECTROCUTION;
                break;
            case 4:
                proprt[ARTP_BRAND] = SPWPN_VAMPIRICISM;
                break;
            case 5:
                proprt[ARTP_BRAND] = SPWPN_PAIN;
                break;
            case 6:
                proprt[ARTP_BRAND] = SPWPN_VENOM;
                break;
            default:
                power_level -= 2;
            }
            power_level += 2;
        }
        else if (one_chance_in(3))
            proprt[ARTP_BRAND] = SPWPN_NORMAL;
        else
            power_level++;

        if (!is_weapon_brand_ok(atype, proprt[ARTP_BRAND], true))
        {
            proprt[ARTP_BRAND] = SPWPN_NORMAL;
            power_level--;
        }
    }

    if (!one_chance_in(5))
    {
        // AC mod - not for armours or rings of protection.
        if (one_chance_in(4 + power_level)
            && aclass != OBJ_ARMOUR
            && (aclass != OBJ_JEWELLERY || atype != RING_PROTECTION))
        {
            proprt[ARTP_AC] = 1 + random2(3) + random2(3) + random2(3);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[ARTP_AC] -= 1 + random2(3) + random2(3) + random2(3);
                power_level--;
            }
        }

        // EV mod - not for rings of evasion.
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_EVASION))
        {
            proprt[ARTP_EVASION] = 1 + random2(3) + random2(3) + random2(3);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[ARTP_EVASION] -= 1 + random2(3) + random2(3)
                    + random2(3);
                power_level--;
            }
        }

        // Str mod - not for rings of strength.
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_STRENGTH))
        {
            proprt[ARTP_STRENGTH] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[ARTP_STRENGTH] -= 1 + random2(3) + random2(3)
                                           + random2(3);
                power_level--;
            }
        }

        // Int mod - not for rings of intelligence.
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_INTELLIGENCE))
        {
            proprt[ARTP_INTELLIGENCE] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[ARTP_INTELLIGENCE] -= 1 + random2(3) + random2(3)
                                               + random2(3);
                power_level--;
            }
        }

        // Dex mod - not for rings of dexterity.
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_DEXTERITY))
        {
            proprt[ARTP_DEXTERITY] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[ARTP_DEXTERITY] -= 1 + random2(3) + random2(3)
                                            + random2(3);
                power_level--;
            }
        }
    }

    if (random2(15) >= power_level && aclass != OBJ_WEAPONS
        && (aclass != OBJ_JEWELLERY || atype != RING_SLAYING))
    {
        // Weapons and rings of slaying can't get these.
        if (one_chance_in(4 + power_level))  // to-hit
        {
            proprt[ARTP_ACCURACY] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[ARTP_ACCURACY] -= 1 + random2(3) + random2(3)
                                           + random2(3);
                power_level--;
            }
        }

        if (one_chance_in(4 + power_level))  // to-dam
        {
            proprt[ARTP_DAMAGE] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[ARTP_DAMAGE] -= 1 + random2(3) + random2(3)
                                         + random2(3);
                power_level--;
            }
        }
    }

    // This used to be: bool done_powers = (random2(12 < power_level));
    // ... which can't be right. random2(boolean) == 0, always.
    // So it's probably more along the lines of... (jpeg)
//    bool done_powers = (random2(12) < power_level);

   // Try to improve items that still have a low power level.
   bool done_powers = x_chance_in_y(power_level, 12);

    // res_fire
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY
            || (atype != RING_PROTECTION_FROM_FIRE
                && atype != RING_FIRE
                && atype != RING_ICE))
        && (aclass != OBJ_ARMOUR
            || (atype != ARM_DRAGON_ARMOUR
                && atype != ARM_ICE_DRAGON_ARMOUR
                && atype != ARM_GOLD_DRAGON_ARMOUR)))
    {
        proprt[ARTP_FIRE] = 1;
        if (one_chance_in(5))
            proprt[ARTP_FIRE]++;
        power_level++;
    }

    // res_cold
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY
            || atype != RING_PROTECTION_FROM_COLD
               && atype != RING_FIRE
               && atype != RING_ICE)
        && (aclass != OBJ_ARMOUR
            || atype != ARM_DRAGON_ARMOUR
               && atype != ARM_ICE_DRAGON_ARMOUR
               && atype != ARM_GOLD_DRAGON_ARMOUR))
    {
        proprt[ARTP_COLD] = 1;
        if (one_chance_in(5))
            proprt[ARTP_COLD]++;
        power_level++;
    }

    if (x_chance_in_y(power_level, 12) || power_level > 7)
        done_powers = true;

    // res_elec
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_ARMOUR || atype != ARM_STORM_DRAGON_ARMOUR))
    {
        proprt[ARTP_ELECTRICITY] = 1;
        power_level++;
    }

    // res_poison
    if (!done_powers
        && one_chance_in(5 + power_level)
        && (aclass != OBJ_JEWELLERY || atype != RING_POISON_RESISTANCE)
        && (aclass != OBJ_ARMOUR
            || atype != ARM_GOLD_DRAGON_ARMOUR
               && atype != ARM_SWAMP_DRAGON_ARMOUR
               && atype != ARM_NAGA_BARDING))
    {
        proprt[ARTP_POISON] = 1;
        power_level++;
    }

    // prot_life - no necromantic brands on weapons allowed
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY || atype != RING_LIFE_PROTECTION)
        && proprt[ARTP_BRAND] != SPWPN_DRAINING
        && proprt[ARTP_BRAND] != SPWPN_VAMPIRICISM
        && proprt[ARTP_BRAND] != SPWPN_PAIN
        && proprt[ARTP_BRAND] != SPWPN_REAPING)
    {
        proprt[ARTP_NEGATIVE_ENERGY] = 1;
        power_level++;
    }

    // res magic
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY || atype != RING_PROTECTION_FROM_MAGIC))
    {
        proprt[ARTP_MAGIC] = 35 + random2(65);
        power_level++;
    }

    // see_invis
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY || atype != RING_INVISIBILITY)
        && (aclass != OBJ_ARMOUR || atype != ARM_NAGA_BARDING))
    {
        proprt[ARTP_EYESIGHT] = 1;
        power_level++;
    }

    if (x_chance_in_y(power_level, 12) || power_level > 10)
        done_powers = true;

    // turn invis
    if (!done_powers
        && one_chance_in(10)
        && (aclass != OBJ_JEWELLERY || atype != RING_INVISIBILITY))
    {
        proprt[ARTP_INVISIBLE] = 1;
        power_level++;
    }

    // levitate
    if (!done_powers
        && one_chance_in(10)
        && (aclass != OBJ_JEWELLERY || atype != RING_LEVITATION))
    {
        proprt[ARTP_LEVITATE] = 1;
        power_level++;
    }

    // blink
    if (!done_powers && one_chance_in(10))
    {
        proprt[ARTP_BLINK] = 1;
        power_level++;
    }

    // go berserk
    if (!done_powers
        && one_chance_in(10)
        && (aclass != OBJ_WEAPONS || !is_range_weapon(item))
        && (aclass != OBJ_JEWELLERY || atype != AMU_RAGE))
    {
        proprt[ARTP_BERSERK] = 1;
        power_level++;
    }

    if (!done_powers && one_chance_in(10) && aclass == OBJ_ARMOUR
        && (atype == ARM_CAP || atype == ARM_SHIELD))
    {
        proprt[ARTP_BRAND] = SPARM_SPIRIT_SHIELD;
        power_level++;
    }

    // Armours get fewer powers, and are also less likely to be cursed
    // than weapons.
    if (aclass == OBJ_ARMOUR)
        power_level -= 4;

    if (power_level >= 2 && x_chance_in_y(power_level, 17))
    {
        switch (random2(9))
        {
        case 0:                     // makes noise
            if (aclass != OBJ_WEAPONS)
                break;
            proprt[ARTP_NOISES] = 1 + random2(4);
            break;
        case 1:                     // no magic
            proprt[ARTP_PREVENT_SPELLCASTING] = 1;
            break;
        case 2:                     // random teleport
            if (aclass != OBJ_WEAPONS)
                break;
            proprt[ARTP_CAUSE_TELEPORTATION] = 5 + random2(15);
            break;
        case 3:   // no teleport - doesn't affect some instantaneous
                  // teleports
            if (aclass == OBJ_JEWELLERY && atype == RING_TELEPORTATION)
                break;              // already is a ring of tport
            if (aclass == OBJ_JEWELLERY && atype == RING_TELEPORT_CONTROL)
                break;              // already is a ring of tport ctrl
            proprt[ARTP_BLINK] = 0;
            proprt[ARTP_PREVENT_TELEPORTATION] = 1;
            break;
        case 4:                     // berserk on attack
            if (aclass != OBJ_WEAPONS || is_range_weapon(item))
                break;
            proprt[ARTP_ANGRY] = 1 + random2(8);
            break;
        case 5:                     // susceptible to fire
            if (aclass == OBJ_JEWELLERY
                && (atype == RING_PROTECTION_FROM_FIRE || atype == RING_FIRE
                    || atype == RING_ICE))
            {
                break;              // already does this or something
            }
            if (aclass == OBJ_ARMOUR
                && (atype == ARM_DRAGON_ARMOUR || atype == ARM_ICE_DRAGON_ARMOUR
                    || atype == ARM_GOLD_DRAGON_ARMOUR))
            {
                break;
            }
            proprt[ARTP_FIRE] = -1;
            break;
        case 6:                     // susceptible to cold
            if (aclass == OBJ_JEWELLERY
                && (atype == RING_PROTECTION_FROM_COLD || atype == RING_FIRE
                    || atype == RING_ICE))
            {
                break;              // already does this or something
            }
            if (aclass == OBJ_ARMOUR
                && (atype == ARM_DRAGON_ARMOUR || atype == ARM_ICE_DRAGON_ARMOUR
                    || atype == ARM_GOLD_DRAGON_ARMOUR))
            {
                break;
            }
            proprt[ARTP_COLD] = -1;
            break;
        case 7:                     // speed metabolism
            if (aclass == OBJ_JEWELLERY && atype == RING_HUNGER)
                break;              // already is a ring of hunger
            if (aclass == OBJ_JEWELLERY && atype == RING_SUSTENANCE)
                break;              // already is a ring of sustenance
            proprt[ARTP_METABOLISM] = 1 + random2(3);
            break;
        case 8:
            // emits mutagenic radiation - increases
            // magic_contamination; property is chance (1 in ...) of
            // increasing magic_contamination
            proprt[ARTP_MUTAGENIC] = 2 + random2(4);
            break;
        }
    }

    if (one_chance_in(10)
        && (aclass != OBJ_ARMOUR
            || atype != ARM_CLOAK
            || get_equip_race(item) != ISFLAG_ELVEN)
        && (aclass != OBJ_ARMOUR
            || atype != ARM_BOOTS
            || get_equip_race(item) != ISFLAG_ELVEN)
        && get_armour_ego_type(item) != SPARM_STEALTH)
    {
        power_level++;
        proprt[ARTP_STEALTH] = 10 + random2(70);

        if (one_chance_in(4))
        {
            proprt[ARTP_STEALTH] = -proprt[ARTP_STEALTH] - random2(20);
            power_level--;
        }
    }

    // "Boring" artefacts (no properties, or only one stat property)
    // get an additional property, or maybe two of them.
    int add_prop = _need_bonus_stat_props(proprt);
    while (0 < add_prop--)
        power_level += _randart_add_one_property(item, proprt);

    if ((power_level < 2 && one_chance_in(5)) || one_chance_in(30))
    {
        if (one_chance_in(4))
            proprt[ARTP_CURSED] = 1 + random2(5);
        else
            proprt[ARTP_CURSED] = -1;
    }
}

static bool _redo_book(item_def &book)
{
    int num_spells  = 0;
    int num_unknown = 0;

    for (int i = 0; i < SPELLBOOK_SIZE; i++)
    {
        spell_type spell = which_spell_in_book(book, i);

        if (spell == SPELL_NO_SPELL)
            continue;

        num_spells++;
        if (!you.seen_spell[spell])
            num_unknown++;
    }

    if (num_spells <= 5 && num_unknown == 0)
        return (true);
    else if (num_spells > 5 && num_unknown <= 1)
        return (true);

    return (false);
}

static bool _init_artefact_book(item_def &book)
{
    ASSERT(book.sub_type == BOOK_RANDART_LEVEL
           || book.sub_type == BOOK_RANDART_THEME);
    ASSERT(book.plus != 0);

    god_type god;
    bool redo = (!origin_is_god_gift(book, &god) || god != GOD_XOM);

    // Plus and plus2 contain parameters to make_book_foo_randart(),
    // which might get changed after the book has been made into a
    // randart, so reset them on each iteration of the loop.
    int  plus  = book.plus;
    int  plus2 = book.plus2;
    bool book_good;
    for (int i = 0; i < 4; i++)
    {
        book.plus  = plus;
        book.plus2 = plus2;

        if (book.sub_type == BOOK_RANDART_LEVEL)
            // The parameters to this call are in book.plus and plus2.
            book_good = make_book_level_randart(book, book.plus, book.plus2);
        else
            book_good = make_book_theme_randart(book);

        if (!book_good)
            continue;

        if (redo && _redo_book(book))
            continue;

        break;
    }

    return (book_good);
}

static bool _init_artefact_properties(item_def &item)
{
    ASSERT( is_artefact( item ) );

    CrawlVector &rap = item.props[ARTEFACT_PROPS_KEY].get_vector();
    for (vec_size i = 0; i < ART_PROPERTIES; i++)
        rap[i] = static_cast<short>(0);

    if (is_unrandom_artefact( item ))
    {
        const unrandart_entry *unrand = _seekunrandart(item);

        for (int i = 0; i < ART_PROPERTIES; i++)
            rap[i] = static_cast<short>(unrand->prpty[i]);

        return (true);
    }

    if (item.base_type == OBJ_BOOKS)
        return _init_artefact_book(item);

    artefact_properties_t prop;
    _get_randart_properties(item, prop);

    for (int i = 0; i < ART_PROPERTIES; i++)
    {
        if (i == ARTP_CURSED && prop[i] < 0)
        {
            do_curse_item(item);
            continue;
        }
        rap[i] = static_cast<short>(prop[i]);
    }

    return (true);
}

void artefact_wpn_properties( const item_def &item,
                             artefact_properties_t  &proprt,
                             artefact_known_props_t &known)
{
    ASSERT( is_artefact( item ) );
    ASSERT( item.props.exists( KNOWN_PROPS_KEY ) );

    const CrawlStoreValue &_val = item.props[KNOWN_PROPS_KEY];
    ASSERT( _val.get_type() == SV_VEC );
    const CrawlVector &known_vec = _val.get_vector();
    ASSERT( known_vec.get_type()     == SV_BOOL );
    ASSERT( known_vec.size()         == ART_PROPERTIES);
    ASSERT( known_vec.get_max_size() == ART_PROPERTIES);

    if (item_ident( item, ISFLAG_KNOW_PROPERTIES ))
    {
        for (vec_size i = 0; i < ART_PROPERTIES; i++)
            known[i] = static_cast<bool>(true);
    }
    else
    {
        for (vec_size i = 0; i < ART_PROPERTIES; i++)
            known[i] = known_vec[i];
    }

    if (item.props.exists( ARTEFACT_PROPS_KEY ))
    {
        const CrawlVector &rap_vec =
            item.props[ARTEFACT_PROPS_KEY].get_vector();
        ASSERT( rap_vec.get_type()     == SV_SHORT );
        ASSERT( rap_vec.size()         == ART_PROPERTIES);
        ASSERT( rap_vec.get_max_size() == ART_PROPERTIES);

        for (vec_size i = 0; i < ART_PROPERTIES; i++)
            proprt[i] = rap_vec[i].get_short();
    }
    else if (is_unrandom_artefact( item ))
    {
        const unrandart_entry *unrand = _seekunrandart(item);

        for (int i = 0; i < ART_PROPERTIES; i++)
            proprt[i] = static_cast<short>(unrand->prpty[i]);
    }
    else
    {
        _get_randart_properties(item, proprt);
    }
}


void artefact_wpn_properties( const item_def &item,
                              artefact_properties_t &proprt )
{
    artefact_known_props_t known;

    artefact_wpn_properties(item, proprt, known);
}

int artefact_wpn_property( const item_def &item, artefact_prop_type prop,
                           bool &_known )
{
    artefact_properties_t  proprt;
    artefact_known_props_t known;

    artefact_wpn_properties( item, proprt, known );

    _known = known[prop];

    return ( proprt[prop] );
}

int artefact_wpn_property( const item_def &item, artefact_prop_type prop )
{
    bool known;

    return artefact_wpn_property( item, prop, known );
}

int artefact_known_wpn_property( const item_def &item,
                                 artefact_prop_type prop )
{
    artefact_properties_t  proprt;
    artefact_known_props_t known;

    artefact_wpn_properties( item, proprt, known );

    if (known[prop])
        return ( proprt[prop] );
    else
        return (0);
}

static int _artefact_num_props( const artefact_properties_t &proprt )
{
    int num = 0;

    // Count all properties, but exclude self-cursing.
    for (int i = 0; i < ARTP_NUM_PROPERTIES; ++i)
        if (i != ARTP_CURSED && proprt[i] != 0)
            num++;

    return num;
}

void artefact_wpn_learn_prop( item_def &item, artefact_prop_type prop )
{
    ASSERT( is_artefact( item ) );
    ASSERT( item.props.exists( KNOWN_PROPS_KEY ) );
    CrawlStoreValue &_val = item.props[KNOWN_PROPS_KEY];
    ASSERT( _val.get_type() == SV_VEC );
    CrawlVector &known_vec = _val.get_vector();
    ASSERT( known_vec.get_type()     == SV_BOOL );
    ASSERT( known_vec.size()         == ART_PROPERTIES);
    ASSERT( known_vec.get_max_size() == ART_PROPERTIES);

    if (item_ident( item, ISFLAG_KNOW_PROPERTIES ))
        return;

    known_vec[prop] = static_cast<bool>(true);
    if (Options.autoinscribe_artefacts)
        add_autoinscription( item, artefact_auto_inscription(item));
}

bool artefact_wpn_known_prop( const item_def &item, artefact_prop_type prop )
{
    bool known;
    artefact_wpn_property( item, prop, known );
    return known;
}

static std::string _get_artefact_type(const item_def &item,
                                      bool appear = false)
{
    switch (item.base_type)
    {
    case OBJ_BOOKS:
        return "book";
    case OBJ_WEAPONS:
        return "weapon";
    case OBJ_ARMOUR:
        return "armour";
    case OBJ_JEWELLERY:
        // Distinguish between amulets and rings only in appearance.
        if (!appear)
            return "jewellery";

        if (jewellery_is_amulet(item))
            return "amulet";
        else
            return "ring";
     default:
         return "artefact";
    }
}

static bool _pick_db_name( const item_def &item )
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
        return coinflip();
    case OBJ_JEWELLERY:
        return one_chance_in(5);
    default:
        return (false);
    }
}

std::string artefact_name_lookup(const item_def &item,
                                 const std::string &lookup)
{
    const std::string name = getRandNameString(lookup);
    return (!name.empty()? _replace_name_parts(name, item) : name);
}

bool artefact_name_lookup(std::string &result,
                          const item_def &item,
                          const std::string &lookup)
{
    result = artefact_name_lookup(item, lookup);
    return (!result.empty());
}

std::string artefact_name(const item_def &item, bool appearance)
{
    ASSERT(is_artefact(item));

    ASSERT(item.base_type == OBJ_WEAPONS
           || item.base_type == OBJ_ARMOUR
           || item.base_type == OBJ_JEWELLERY
           || item.base_type == OBJ_BOOKS);

    if (is_unrandom_artefact( item ))
    {
        const unrandart_entry *unrand = _seekunrandart( item );
        return (item_type_known(item) ? unrand->name : unrand->unid_name);
    }

    const long seed = _calc_seed( item );

    std::string lookup;
    std::string result;

    // Use prefix of gifting god, if applicable.
    bool god_gift = false;
    int item_orig = 0;
    if (!appearance)
    {
        // Divine gifts don't look special, so this is only necessary
        // for actually naming an item.
        item_orig = item.orig_monnum;
        if (item_orig < 0)
            item_orig = -item_orig;
        else
            item_orig = 0;

        if (item_orig > GOD_NO_GOD && item_orig < NUM_GODS)
        {
            lookup += god_name(static_cast<god_type>(item_orig), false) + " ";
            god_gift = true;
        }
    }

    // get base type
    lookup += _get_artefact_type(item, appearance);

    rng_save_excursion rng_state;
    seed_rng( seed );

    if (appearance)
    {
        std::string appear = getRandNameString(lookup, " appearance");
        if (appear.empty())
        {
            appear = getRandNameString("general appearance");
            if (appear.empty()) // still nothing found?
                appear = "non-descript";
        }

        result += appear;
        result += " ";
        result += item_base_name(item);
        return (result);
    }

    if (_pick_db_name(item))
    {
        result += item_base_name(item) + " ";

        int tries = 100;
        std::string name;
        do
        {
            (artefact_name_lookup(name, item, lookup)

             // If nothing found, try god name alone.
             || (god_gift
                 && artefact_name_lookup(name, item,
                                         god_name(
                                             static_cast<god_type>(item_orig),
                                             false)))

             // If still nothing found, try base type alone.
             || artefact_name_lookup(name, item, _get_artefact_type(item)));
        }
        while (--tries > 0 && name.length() > 25);

        if (name.empty()) // still nothing found?
            result += "of Bugginess";
        else
            result += name;
    }
    else
    {
        // construct a unique name
        const std::string st_p = make_name(random_int(), false);
        result += item_base_name(item);

        if (one_chance_in(3))
        {
            result += " of ";
            result += st_p;
        }
        else
        {
            result += " \"";
            result += st_p;
            result += "\"";
        }
    }

    return result;
}

std::string get_artefact_name( const item_def &item, bool force_known )
{
    ASSERT( is_artefact( item ) );

    if (item_type_known(item) || force_known)
    {
        // print artefact's real name
        if (item.props.exists(ARTEFACT_NAME_KEY))
            return item.props[ARTEFACT_NAME_KEY].get_string();
        return artefact_name(item, false);
    }
    // print artefact appearance
    if (item.props.exists(ARTEFACT_APPEAR_KEY))
        return item.props[ARTEFACT_APPEAR_KEY].get_string();
    return artefact_name(item, false);
}

void set_artefact_name( item_def &item, const std::string &name )
{
    ASSERT( is_artefact( item ));
    ASSERT( !name.empty() );
    item.props[ARTEFACT_NAME_KEY].get_string() = name;
}

void set_artefact_appearance( item_def &item, const std::string &appear )
{
    ASSERT( is_artefact( item ));
    ASSERT( !appear.empty() );
    item.props[ARTEFACT_APPEAR_KEY].get_string() = appear;
}

int find_unrandart_index(const item_def& artefact)
{
    return (artefact.special);
}

unrandart_entry* get_unrand_entry(int unrand_index)
{
    unrand_index -= UNRAND_START;

    if (unrand_index <= -1 || unrand_index >= NO_UNRANDARTS)
        return &unranddata[0];  // dummy unrandart
    else
        return &unranddata[unrand_index];
}

static unrandart_entry *_seekunrandart( const item_def &item )
{
    return get_unrand_entry(item.special);
}

int find_okay_unrandart(unsigned char aclass, unsigned char atype,
                        unrand_special_type specialness, bool in_abyss)
{
    int ret = -1;

    // Pick randomly among not-yet-existing unrandarts with the proper
    // base_type and sub_type.
    for (int i = 0, count = 0; i < NO_UNRANDARTS; i++)
    {
        const int              index = i + UNRAND_START;
        const unrandart_entry* entry = &unranddata[i];

        // Skip dummy entries.
        if (entry->base_type == OBJ_UNASSIGNED)
            continue;

        const unique_item_status_type status =
            get_unique_item_status(index);

        if (in_abyss && status != UNIQ_LOST_IN_ABYSS
            || !in_abyss && status != UNIQ_NOT_EXISTS)
        {
            continue;
        }

        // Never randomly generated until lost in the abyss.
        if ((!in_abyss || status != UNIQ_LOST_IN_ABYSS)
            && entry->flags & UNRAND_FLAG_NOGEN)
        {
            continue;
        }

        if (entry->base_type != aclass
            || (atype != OBJ_RANDOM && entry->sub_type != atype))
        {
            continue;
        }

        if (specialness != UNRANDSPEC_EITHER
            && specialness != get_unrand_specialness(index))
        {
            continue;
        }

        count++;

        if (one_chance_in(count))
            ret = index;
    }

    return (ret);
}

unrand_special_type get_unrand_specialness(int unrand_index)
{
    if (unrand_index < UNRAND_START || unrand_index > UNRAND_LAST)
        return (UNRANDSPEC_NORMAL);

    if (unranddata[unrand_index - UNRAND_START].flags & UNRAND_FLAG_SPECIAL)
        return (UNRANDSPEC_SPECIAL);

    return (UNRANDSPEC_NORMAL);
}

unrand_special_type get_unrand_specialness(const item_def &item)
{
    ASSERT(is_unrandom_artefact(item));
    return get_unrand_specialness(item.special);
}

int get_unrandart_num( const char *name )
{
    std::string quoted = "\"";
    quoted += name;
    quoted += "\"";
    lowercase(quoted);

    for (unsigned int i = 0; i < ARRAYSZ(unranddata); ++i)
    {
        std::string art = unranddata[i].name;
        art = replace_all(art, " ", "_");
        art = replace_all(art, "'", "");
        lowercase(art);
        if (art == name || art.find(quoted) != std::string::npos)
            return (UNRAND_START + i);
    }
    return SPWPN_NORMAL;
}

static bool _randart_is_redundant( const item_def &item,
                                   artefact_properties_t &proprt )
{
    if (item.base_type != OBJ_JEWELLERY)
        return (false);

    artefact_prop_type provides  = ARTP_NUM_PROPERTIES;
    artefact_prop_type provides2 = ARTP_NUM_PROPERTIES;

    switch (item.sub_type)
    {
    case RING_PROTECTION:
        provides = ARTP_AC;
        break;

    case RING_FIRE:
    case RING_PROTECTION_FROM_FIRE:
        provides = ARTP_FIRE;
        break;

    case RING_POISON_RESISTANCE:
        provides = ARTP_POISON;
        break;

    case RING_ICE:
    case RING_PROTECTION_FROM_COLD:
        provides = ARTP_COLD;
        break;

    case RING_STRENGTH:
        provides = ARTP_STRENGTH;
        break;

    case RING_SLAYING:
        provides  = ARTP_DAMAGE;
        provides2 = ARTP_ACCURACY;
        break;

    case RING_SEE_INVISIBLE:
        provides = ARTP_EYESIGHT;
        break;

    case RING_INVISIBILITY:
        provides = ARTP_INVISIBLE;
        break;

    case RING_HUNGER:
        provides = ARTP_METABOLISM;
        break;

    case RING_TELEPORTATION:
        provides = ARTP_CAUSE_TELEPORTATION;
        break;

    case RING_EVASION:
        provides = ARTP_EVASION;
        break;

    case RING_DEXTERITY:
        provides = ARTP_DEXTERITY;
        break;

    case RING_INTELLIGENCE:
        provides = ARTP_INTELLIGENCE;
        break;

    case RING_MAGICAL_POWER:
        provides = ARTP_MAGICAL_POWER;
        break;

    case RING_LEVITATION:
        provides = ARTP_LEVITATE;
        break;

    case RING_LIFE_PROTECTION:
        provides = ARTP_AC;
        break;

    case RING_PROTECTION_FROM_MAGIC:
        provides = ARTP_MAGIC;
        break;

    case AMU_RAGE:
        provides = ARTP_BERSERK;
        break;

    case AMU_INACCURACY:
        provides = ARTP_ACCURACY;
        break;

    case AMU_STASIS:
        provides = ARTP_PREVENT_TELEPORTATION;
        break;
    }

    if (provides == ARTP_NUM_PROPERTIES)
        return (false);

    if (proprt[provides] != 0)
        return (true);

    if (provides2 == ARTP_NUM_PROPERTIES)
        return (false);

    if (proprt[provides2] != 0)
        return (true);

    return (false);
}

static bool _randart_is_conflicting( const item_def &item,
                                     artefact_properties_t &proprt )
{
    if (item.base_type == OBJ_WEAPONS
        && get_weapon_brand(item) == SPWPN_HOLY_WRATH
        && proprt[ARTP_CURSED] != 0)
    {
        return (true);
    }

    if (item.base_type != OBJ_JEWELLERY)
        return (false);

    if (item.sub_type == AMU_STASIS
        && (proprt[ARTP_BLINK] != 0
            || proprt[ARTP_CAUSE_TELEPORTATION] != 0
            || proprt[ARTP_ANGRY] != 0
            || proprt[ARTP_BERSERK] != 0))
    {
        return (true);
    }

    artefact_prop_type conflicts = ARTP_NUM_PROPERTIES;

    switch (item.sub_type)
    {
    case RING_SUSTENANCE:
        conflicts = ARTP_METABOLISM;
        break;

    case RING_FIRE:
    case RING_ICE:
    case RING_WIZARDRY:
    case RING_MAGICAL_POWER:
        conflicts = ARTP_PREVENT_SPELLCASTING;
        break;

    case RING_TELEPORTATION:
    case RING_TELEPORT_CONTROL:
        conflicts = ARTP_PREVENT_TELEPORTATION;
        break;

    case AMU_RESIST_MUTATION:
        conflicts = ARTP_MUTAGENIC;
        break;

    case AMU_RAGE:
        conflicts = ARTP_STEALTH;
        break;
    }

    if (conflicts == ARTP_NUM_PROPERTIES)
        return (false);

    if (proprt[conflicts] != 0)
        return (true);

    return (false);
}

bool randart_is_bad( const item_def &item, artefact_properties_t &proprt )
{
    if (item.base_type == OBJ_BOOKS)
        return (false);

    if (_artefact_num_props( proprt ) == 0)
        return (true);

    // Weapons must have a brand and at least one other property.
    if (item.base_type == OBJ_WEAPONS
        && (proprt[ARTP_BRAND] == SPWPN_NORMAL
            || _artefact_num_props( proprt ) < 2))
    {
        return (true);
    }

    return (_randart_is_redundant( item, proprt )
            || _randart_is_conflicting( item, proprt ));
}

bool randart_is_bad( const item_def &item )
{
    artefact_properties_t proprt;
    artefact_wpn_properties( item, proprt );

    return randart_is_bad( item, proprt);
}

static void _artefact_setup_prop_vectors(item_def &item)
{
    CrawlHashTable &props = item.props;
    if (!props.exists(ARTEFACT_PROPS_KEY))
        props[ARTEFACT_PROPS_KEY].new_vector(SV_SHORT)
            .resize(ART_PROPERTIES);

    CrawlVector &rap = props[ARTEFACT_PROPS_KEY].get_vector();
    rap.set_max_size(ART_PROPERTIES);

    for (vec_size i = 0; i < ART_PROPERTIES; i++)
        rap[i].get_short() = 0;

    if (!item.props.exists( KNOWN_PROPS_KEY ))
    {
        props[KNOWN_PROPS_KEY].new_vector(SV_BOOL).resize(ART_PROPERTIES);
        CrawlVector &known = item.props[KNOWN_PROPS_KEY].get_vector();
        known.set_max_size(ART_PROPERTIES);
        for (vec_size i = 0; i < ART_PROPERTIES; i++)
            known[i] = static_cast<bool>(false);
    }
}

void artefact_set_name(item_def &item, const std::string &name)
{
    item.props[ARTEFACT_NAME_KEY].get_string() = name;
}

// If force_mundane is true, normally mundane items are forced to
// nevertheless become artefacts.
bool make_item_randart( item_def &item, bool force_mundane )
{
    if (item.base_type != OBJ_WEAPONS
        && item.base_type != OBJ_ARMOUR
        && item.base_type != OBJ_JEWELLERY
        && item.base_type != OBJ_BOOKS)
    {
        return (false);
    }

    if (item.base_type == OBJ_BOOKS)
    {
        if (item.sub_type != BOOK_RANDART_LEVEL
            && item.sub_type != BOOK_RANDART_THEME)
        {
            return (false);
        }
    }

    // This item already is a randart.
    if (item.flags & ISFLAG_RANDART)
        return (true);

    // Not a truly random artefact.
    if (item.flags & ISFLAG_UNRANDART)
        return (false);

    // Mundane items are much less likely to be artefacts.
    if (!force_mundane && item.is_mundane() && !one_chance_in(5))
        return (false);

    _artefact_setup_prop_vectors(item);
    item.flags |= ISFLAG_RANDART;

    god_type god_gift;
    (void) origin_is_god_gift(item, &god_gift);

    int randart_tries = 500;
    do
    {
        item.special = (random_int() & RANDART_SEED_MASK);
        // Now that we found something, initialise the props array.
        if (--randart_tries <= 0 || !_init_artefact_properties(item))
        {
            // Something went wrong that no amount of changing
            // item.special will fix.
            item.special = 0;
            item.props.erase(ARTEFACT_PROPS_KEY);
            item.props.erase(KNOWN_PROPS_KEY);
            item.flags &= ~ISFLAG_RANDART;
            return (false);
        }
    }
    while (randart_is_bad(item)
           || god_gift != GOD_NO_GOD && !_god_fits_artefact(god_gift, item));

    // get true artefact name
    if (item.props.exists(ARTEFACT_NAME_KEY))
        ASSERT(item.props[ARTEFACT_NAME_KEY].get_type() == SV_STR);
    else
        artefact_set_name(item, artefact_name(item, false));

    // get artefact appearance
    if (item.props.exists( ARTEFACT_APPEAR_KEY ))
        ASSERT(item.props[ARTEFACT_APPEAR_KEY].get_type() == SV_STR);
    else
        item.props[ARTEFACT_APPEAR_KEY].get_string() =
            artefact_name(item, true);

    return (true);
}

static void _make_faerie_armour(item_def &item)
{
    item_def doodad;
    for (int i=0; i<100; i++)
    {
        doodad.clear();
        doodad.base_type = item.base_type;
        doodad.sub_type = item.sub_type;
        if (!make_item_randart(doodad))
        {
            i--; // Forbidden props are not absolute, artefactness is.
            continue;
        }

        // These make little sense for a casting mon.
        if (artefact_wpn_property(doodad, ARTP_BERSERK)
            || artefact_wpn_property(doodad, ARTP_ANGRY)
            || artefact_wpn_property(doodad, ARTP_PREVENT_SPELLCASTING)
            || artefact_wpn_property(doodad, ARTP_CAUSE_TELEPORTATION)
            || artefact_wpn_property(doodad, ARTP_PREVENT_TELEPORTATION)
            || artefact_wpn_property(doodad, ARTP_MUTAGENIC)
            || artefact_wpn_property(doodad, ARTP_PONDEROUS))
        {
            continue;
        }
    }
    ASSERT(is_artefact(doodad));
    ASSERT(doodad.sub_type == item.sub_type);

    doodad.props[ARTEFACT_NAME_KEY].get_string()
        = item.props[ARTEFACT_NAME_KEY].get_string();
    doodad.props[ARTEFACT_APPEAR_KEY].get_string()
        = item.props[ARTEFACT_APPEAR_KEY].get_string();
    item.props = doodad.props;
    item.plus = 2 + random2(5);
}

bool make_item_unrandart( item_def &item, int unrand_index )
{
    ASSERT(unrand_index > UNRAND_START);
    ASSERT(unrand_index < (UNRAND_START + NO_UNRANDARTS));

    item.special = unrand_index;

    const unrandart_entry *unrand = &unranddata[unrand_index - UNRAND_START];
    item.base_type = unrand->base_type;
    item.sub_type  = unrand->sub_type;
    item.plus      = unrand->plus;
    item.plus2     = unrand->plus2;
    item.colour    = unrand->colour;

    item.flags |= ISFLAG_UNRANDART;
    _artefact_setup_prop_vectors(item);
    _init_artefact_properties(item);

    if (unrand->prpty[ARTP_CURSED] != 0)
        do_curse_item( item );

    // get true artefact name
    ASSERT(!item.props.exists( ARTEFACT_NAME_KEY ));
    item.props[ARTEFACT_NAME_KEY].get_string() = unrand->name;

    // get artefact appearance
    ASSERT(!item.props.exists( ARTEFACT_APPEAR_KEY ));
    item.props[ARTEFACT_APPEAR_KEY].get_string() = unrand->unid_name;

    set_unique_item_status(unrand_index, UNIQ_EXISTS);

    if (unrand_index == UNRAND_VARIABILITY)
    {
        item.plus  = random_range(-4, 16);
        item.plus2 = random_range(-4, 16);
    }
    else if (unrand_index == UNRAND_FAERIE)
        _make_faerie_armour(item);

    return (true);
}

const char *unrandart_descrip( int which_descrip, const item_def &item )
{
    // Eventually it would be great to have randomly generated descriptions
    // for randarts.
    const unrandart_entry *unrand = _seekunrandart( item );

    return ((which_descrip == 0) ? unrand->desc :
            (which_descrip == 1) ? unrand->desc_id :
            (which_descrip == 2) ? unrand->desc_end
                                 : "Unknown.");
}

void artefact_clear_properties(item_def &item)
{
    ASSERT( is_artefact( item ) );
    ASSERT( item.props.exists( ARTEFACT_PROPS_KEY ) );

    CrawlVector &rap_vec = item.props[ARTEFACT_PROPS_KEY].get_vector();
    ASSERT( rap_vec.get_type()     == SV_SHORT );
    ASSERT( rap_vec.size()         == ART_PROPERTIES);
    ASSERT( rap_vec.get_max_size() == ART_PROPERTIES);

    for (vec_size i = 0; i < ART_PROPERTIES; i++)
        rap_vec[i].get_short() = 0;
}

// Set all non-zero properties in proprt on the randart supplied.
void artefact_merge_properties(item_def &item,
                               const artefact_properties_t &proprt)
{
    ASSERT( is_artefact( item ) );
    ASSERT( item.props.exists( ARTEFACT_PROPS_KEY ) );

    CrawlVector &rap_vec = item.props[ARTEFACT_PROPS_KEY].get_vector();
    ASSERT( rap_vec.get_type()     == SV_SHORT );
    ASSERT( rap_vec.size()         == ART_PROPERTIES);
    ASSERT( rap_vec.get_max_size() == ART_PROPERTIES);

    for (vec_size i = 0; i < ART_PROPERTIES; i++)
    {
        const short value = proprt[i];
        if (value)
            rap_vec[i].get_short() = value;
    }
}

void artefact_set_properties(item_def &item,
                             const artefact_properties_t &proprt)
{
    artefact_clear_properties(item);
    artefact_merge_properties(item, proprt);
}

void artefact_set_property( item_def          &item,
                            artefact_prop_type prop,
                            int                val )
{
    ASSERT( is_artefact( item ) );
    ASSERT( item.props.exists( ARTEFACT_PROPS_KEY ) );

    CrawlVector &rap_vec = item.props[ARTEFACT_PROPS_KEY].get_vector();
    ASSERT( rap_vec.get_type()     == SV_SHORT );
    ASSERT( rap_vec.size()         == ART_PROPERTIES);
    ASSERT( rap_vec.get_max_size() == ART_PROPERTIES);

    rap_vec[prop].get_short() = val;
}

void cheibriados_make_item_ponderous(item_def &item)
{
    ASSERT(item.base_type == OBJ_ARMOUR);
    const special_armour_type brand = get_armour_ego_type(item);
    if (!is_artefact(item))
    {
        item.flags |= ISFLAG_RANDART;
        _artefact_setup_prop_vectors(item);
        const std::string suffix =
            artefact_name_lookup(
                item,
                god_name(GOD_CHEIBRIADOS) + " ponderous");
        artefact_set_name(item, item_base_name(item) + " " + suffix);
    }
    artefact_properties_t props;
    props.init(0);
    props[ARTP_PONDEROUS] = true;
    props[ARTP_BRAND] = brand;
    artefact_merge_properties(item, props);

    if (Options.autoinscribe_artefacts)
        add_autoinscription(item, artefact_auto_inscription(item));
}

template<typename Z>
static inline void artefact_pad_store_vector(CrawlVector &vector, Z value)
{
    if (vector.get_max_size() < ART_PROPERTIES)
    {
        // Authentic tribal dance to propitiate the asserts in store.cc:
        const int old_size = vector.get_max_size();
        vector.set_max_size(VEC_MAX_SIZE);
        vector.resize(ART_PROPERTIES);
        vector.set_max_size(ART_PROPERTIES);
        for (int i = old_size; i < ART_PROPERTIES; ++i)
            vector[i] = value;
    }
}

void artefact_fixup_props(item_def &item)
{
    CrawlHashTable &props = item.props;
    if (props.exists(ARTEFACT_PROPS_KEY))
        artefact_pad_store_vector(props[ARTEFACT_PROPS_KEY], short(0));

    if (props.exists(KNOWN_PROPS_KEY))
        artefact_pad_store_vector(props[KNOWN_PROPS_KEY], false);
}
