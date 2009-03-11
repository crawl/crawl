/*
 *  File:       randart.cc
 *  Summary:    Random and unrandom artefact functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "randart.h"

#include <cstdlib>
#include <climits>
#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "database.h"
#include "describe.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "place.h"
#include "player.h"
#include "religion.h"
#include "spl-book.h"
#include "stuff.h"

#define KNOWN_PROPS_KEY    "randart_known_props"
#define RANDART_PROPS_KEY  "randart_props"
#define RANDART_NAME_KEY   "randart_name"
#define RANDART_APPEAR_KEY "randart_appearance"

static const char* _get_fixedart_name(const item_def &item);

// The initial generation of a randart is very simple - it occurs in
// dungeon.cc and consists of giving it a few random things - plus &
// plus2 mainly.

static bool _god_fits_artefact(const god_type which_god, const item_def &item,
                               bool name_check_only = false)
{
    if (which_god == GOD_NO_GOD)
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

    if (is_evil_god(which_god) && brand == SPWPN_HOLY_WRATH)
        return (false);
    else if (is_good_god(which_god)
             && (brand == SPWPN_DRAINING || brand == SPWPN_PAIN
                 || brand == SPWPN_VAMPIRICISM
                 || randart_wpn_property(item, RAP_CURSED) != 0))
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
        if (randart_wpn_property(item, RAP_ANGRY)
            || randart_wpn_property(item, RAP_BERSERK))
        {
            return (false);
        }
        break;

    case GOD_ZIN:
        // Lawful god: no mutagenics.
        if (randart_wpn_property(item, RAP_MUTAGENIC))
            return (false);
        break;

    case GOD_SHINING_ONE:
        // Crusader god: holiness, honourable combat.
        if (item.base_type == OBJ_WEAPONS && brand != SPWPN_HOLY_WRATH)
            return (false);

        if (randart_wpn_property(item, RAP_INVISIBLE)
            || randart_wpn_property(item, RAP_STEALTH) > 0)
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
        if (randart_wpn_property(item, RAP_PREVENT_SPELLCASTING))
            return (false);
        break;

    case GOD_TROG:
        // Anti-magic god: no spell use, no enhancing magic.
        if (brand == SPWPN_PAIN) // Pain involves necromantic spell use.
            return (false);

        if (randart_wpn_property(item, RAP_MAGICAL_POWER))
            return (false);

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
                 species_name(static_cast<species_type>(you.species), 1));

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

// The following unrandart bits were taken from $pellbinder's mon-util
// code (see mon-util.h & mon-util.cc) and modified (LRH).  They're in
// randart.cc and not randart.h because they're only used in this code
// module.

struct unrandart_entry
{
    const char *name;        // true name of unrandart (max 31 chars)
    const char *unid_name;   // un-id'd name of unrandart (max 31 chars)

    object_class_type ura_cl;        // class of ura
    int ura_ty;        // type of ura
    int ura_pl;        // plus of ura
    int ura_pl2;       // plus2 of ura
    int ura_col;       // colour of ura
    short prpty[RA_PROPERTIES];

    // special description added to 'v' command output (max 31 chars)
    const char *spec_descrip1;
    // special description added to 'v' command output (max 31 chars)
    const char *spec_descrip2;
    // special description added to 'v' command output (max 31 chars)
    const char *spec_descrip3;
};

static unrandart_entry unranddata[] = {
#include "unrand.h"
};

static FixedVector < bool, NO_UNRANDARTS > unrandart_exist;

static unrandart_entry *_seekunrandart( const item_def &item );

void set_unrandart_exist(int whun, bool is_exist)
{
    unrandart_exist[whun] = is_exist;
}

bool does_unrandart_exist(int whun)
{
    return (unrandart_exist[whun]);
}

bool is_known_artefact( const item_def &item )
{
    if (!item_type_known(item))
        return (false);

    return (is_artefact(item));
}

bool is_artefact( const item_def &item )
{
    return (is_random_artefact(item) || is_fixed_artefact(item));
}

// returns true is item is a pure randart or an unrandart
bool is_random_artefact( const item_def &item )
{
    return (item.flags & ISFLAG_ARTEFACT_MASK);
}

// returns true if item in an unrandart
bool is_unrandom_artefact( const item_def &item )
{
    return (item.flags & ISFLAG_UNRANDART);
}

// returns true if item is one of the original fixed artefacts
bool is_fixed_artefact( const item_def &item )
{
    if (!is_random_artefact( item )
        && item.base_type == OBJ_WEAPONS
        && item.special >= SPWPN_START_FIXEDARTS)
    {
        return (true);
    }

    return (false);
}

unique_item_status_type get_unique_item_status( object_class_type base_type,
                                                int art )
{
    if (base_type == OBJ_WEAPONS
        && art >= SPWPN_START_FIXEDARTS && art < SPWPN_START_NOGEN_FIXEDARTS)
    {
        return (you.unique_items[art - SPWPN_START_FIXEDARTS]);
    }
    else
        return (UNIQ_NOT_EXISTS);
}

void set_unique_item_status( object_class_type base_type, int art,
                             unique_item_status_type status )
{
    if (base_type == OBJ_WEAPONS
        && art >= SPWPN_START_FIXEDARTS && art < SPWPN_START_NOGEN_FIXEDARTS)
    {
        you.unique_items[art - SPWPN_START_FIXEDARTS] = status;
    }
}

static long _calc_seed( const item_def &item )
{
    return (item.special & RANDART_SEED_MASK);
}

void randart_desc_properties( const item_def &item,
                              randart_properties_t &proprt,
                              randart_known_props_t &known,
                              bool force_fake_props)
{
    // Randart books have no randart properties.
    if (item.base_type == OBJ_BOOKS)
        return;

    randart_wpn_properties( item, proprt, known);

    if (!force_fake_props && item_ident( item, ISFLAG_KNOW_PROPERTIES ))
        return;

    // Only jewellery need fake randart properties.
    if (item.base_type != OBJ_JEWELLERY)
        return;

    randart_prop_type fake_rap  = RAP_NUM_PROPERTIES;
    int               fake_plus = 1;

    // The base jewellery type is one whose property is revealed by
    // wearing it, but whose property isn't revealed by having
    // ISFLAG_KNOW_PLUSES set.  For a randart with a base type of, for
    // example, a ring of strength, wearing it sets
    // ISFLAG_KNOW_PLUSES, which reveals the ring's strength plus.
    switch (item.sub_type)
    {
    case RING_INVISIBILITY:
        fake_rap = RAP_INVISIBLE;
        break;

    case RING_TELEPORTATION:
        fake_rap = RAP_CAUSE_TELEPORTATION;
        break;

    case RING_MAGICAL_POWER:
        fake_rap  = RAP_MAGICAL_POWER;
        fake_plus = 9;
        break;

    case RING_LEVITATION:
        fake_rap = RAP_LEVITATE;
        break;

    case AMU_RAGE:
        fake_rap = RAP_BERSERK;
        break;
    }

    if (fake_rap != RAP_NUM_PROPERTIES)
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

    randart_prop_type fake_rap2  = RAP_NUM_PROPERTIES;
    int               fake_plus2 = 1;

    switch (item.sub_type)
    {
    case RING_PROTECTION:
        fake_rap  = RAP_AC;
        fake_plus = item.plus;
        break;

    case RING_PROTECTION_FROM_FIRE:
        fake_rap = RAP_FIRE;
        break;

    case RING_POISON_RESISTANCE:
        fake_rap = RAP_POISON;
        break;

    case RING_PROTECTION_FROM_COLD:
        fake_rap = RAP_COLD;
        break;

    case RING_SLAYING:
        fake_rap   = RAP_ACCURACY;
        fake_plus  = item.plus;
        fake_rap2  = RAP_DAMAGE;
        fake_plus2 = item.plus2;
        break;

    case RING_SEE_INVISIBLE:
        fake_rap = RAP_EYESIGHT;
        break;

    case RING_HUNGER:
        fake_rap = RAP_METABOLISM;
        break;

    case RING_EVASION:
        fake_rap  = RAP_EVASION;
        fake_plus = item.plus;
        break;

    case RING_STRENGTH:
        fake_rap  = RAP_STRENGTH;
        fake_plus = item.plus;
        break;

    case RING_INTELLIGENCE:
        fake_rap  = RAP_INTELLIGENCE;
        fake_plus = item.plus;
        break;

    case RING_DEXTERITY:
        fake_rap  = RAP_DEXTERITY;
        fake_plus = item.plus;
        break;

    case RING_LIFE_PROTECTION:
        fake_rap = RAP_NEGATIVE_ENERGY;
        break;

    case RING_PROTECTION_FROM_MAGIC:
        fake_rap = RAP_MAGIC;
        break;

    case RING_FIRE:
        fake_rap   = RAP_FIRE;
        fake_rap2  = RAP_COLD;
        fake_plus2 = -1;
        break;

    case RING_ICE:
        fake_rap   = RAP_COLD;
        fake_rap2  = RAP_FIRE;
        fake_plus2 = -1;
        break;

    case AMU_INACCURACY:
        fake_rap  = RAP_ACCURACY;
        fake_plus = -5;
        break;
    }

    if (fake_rap != RAP_NUM_PROPERTIES && fake_plus != 0)
        proprt[fake_rap] += fake_plus;

    if (fake_rap2 != RAP_NUM_PROPERTIES && fake_plus2 != 0)
        proprt[fake_rap2] += fake_plus2;

    if (item_ident( item, ISFLAG_KNOW_PROPERTIES )
        || item_ident( item, ISFLAG_KNOW_TYPE ))
    {
        if (fake_rap != RAP_NUM_PROPERTIES && proprt[fake_rap] != 0)
            known[fake_rap] = true;

        if (fake_rap2 != RAP_NUM_PROPERTIES && proprt[fake_rap2] != 0)
            known[fake_rap2] = true;
    }
}

inline static void _randart_propset( randart_properties_t &p,
                                     randart_prop_type pt,
                                     int value,
                                     bool neg )
{
    // This shouldn't be called with 0, else no property gets added after all.
    ASSERT(value != 0);
    p[pt] = (neg? -value : value);
}

static int _randart_add_one_property( const item_def &item,
                                      randart_properties_t &proprt )
{
    // This function assumes that no properties have been added to this
    // randart yet.

    const object_class_type cl = item.base_type;
    const int ty = item.sub_type;

    // 0 - ac, 1 - ev, 2 - str, 3 - int, 4 - dex
    int prop;
    int skip = -1;

    // Determine if we need to skip any of the above.
    if (cl == OBJ_ARMOUR || cl == OBJ_JEWELLERY && ty == RING_PROTECTION)
        skip = 0;
    else if (cl == OBJ_JEWELLERY && ty == RING_EVASION)
        skip = 1;
    else if (cl == OBJ_JEWELLERY && ty == RING_STRENGTH)
        skip = 2;
    else if (cl == OBJ_JEWELLERY && ty == RING_INTELLIGENCE)
        skip = 3;
    else if (cl == OBJ_JEWELLERY && ty == RING_DEXTERITY)
        skip = 4;

    // Pick a random enchantment, taking into account the skipped index.
    if (skip >= 0)
    {
        prop = random2(4);
        if (prop >= skip)
            prop++;
    }
    else
    {
        prop = random2(5);
    }

    const bool negench = one_chance_in(4);

    switch(prop)
    {
    default:
    case 0:
        _randart_propset(proprt, RAP_AC,
                         1 + random2(3) + random2(3) + random2(3),
                         negench);
        break;
    case 1:
        _randart_propset(proprt, RAP_EVASION,
                         1 + random2(3) + random2(3) + random2(3),
                         negench);
        break;
    case 2:
        _randart_propset(proprt, RAP_STRENGTH,
                         1 + random2(3) + random2(3),
                         negench);
        break;
    case 3:
        _randart_propset(proprt, RAP_INTELLIGENCE,
                         1 + random2(3) + random2(3),
                         negench);
        break;
    case 4:
        _randart_propset(proprt, RAP_DEXTERITY,
                         1 + random2(3) + random2(3),
                         negench);
        break;
    }

    return (negench ? -1 : 1);
}

void static _get_randart_properties(const item_def &item,
                                    randart_properties_t &proprt)
{
    const object_class_type aclass = item.base_type;
    const int atype = item.sub_type;
    int power_level = 0;

    const long seed = _calc_seed( item );
    rng_save_excursion exc;
    seed_rng( seed );

    if (aclass == OBJ_ARMOUR)
        power_level = item.plus / 2 + 2;
    else if (aclass == OBJ_JEWELLERY)
        power_level = 1 + random2(3) + random2(2);
    else // OBJ_WEAPON
        power_level = item.plus / 3 + item.plus2 / 3;

    if (power_level < 0)
        power_level = 0;

    proprt.init(0);

    if (aclass == OBJ_WEAPONS) // only weapons get brands, of course
    {
        proprt[RAP_BRAND] = SPWPN_FLAMING + random2(15);        // brand

        if (one_chance_in(6))
            proprt[RAP_BRAND] = SPWPN_FLAMING + random2(2);

        if (one_chance_in(6))
            proprt[RAP_BRAND] = SPWPN_ORC_SLAYING + random2(5);

        if (proprt[RAP_BRAND] == SPWPN_DRAGON_SLAYING
            && weapon_skill(item) != SK_POLEARMS)
        {
            proprt[RAP_BRAND] = SPWPN_NORMAL;
        }

        if (proprt[RAP_BRAND] == SPWPN_VENOM
            && get_vorpal_type(item) == DVORP_CRUSHING)
        {
            proprt[RAP_BRAND] = SPWPN_NORMAL;
        }

        if (one_chance_in(6))
            proprt[RAP_BRAND] = SPWPN_VORPAL;

        if (proprt[RAP_BRAND] == SPWPN_FLAME
            || proprt[RAP_BRAND] == SPWPN_FROST)
        {
            proprt[RAP_BRAND] = SPWPN_NORMAL;      // missile wpns
        }

        if (proprt[RAP_BRAND] == SPWPN_PROTECTION)
            proprt[RAP_BRAND] = SPWPN_NORMAL;      // no protection

        // if this happens, things might get broken - bwr
        if (proprt[RAP_BRAND] == SPWPN_SPEED && atype == WPN_QUICK_BLADE)
            proprt[RAP_BRAND] = SPWPN_NORMAL;

        if (is_range_weapon(item))
        {
            proprt[RAP_BRAND] = SPWPN_NORMAL;

            if (one_chance_in(3))
            {
                int tmp = random2(20);

                proprt[RAP_BRAND] = (tmp >= 18) ? SPWPN_SPEED :
                                    (tmp >= 14) ? SPWPN_PROTECTION :
                                    (tmp >= 10) ? SPWPN_VENOM
                                                : SPWPN_VORPAL + random2(3);

                if (atype == WPN_BLOWGUN
                    && proprt[RAP_BRAND] != SPWPN_SPEED
                    && proprt[RAP_BRAND] != SPWPN_PROTECTION)
                {
                    proprt[RAP_BRAND] = SPWPN_NORMAL;
                }

                if (atype == WPN_SLING
                    && proprt[RAP_BRAND] == SPWPN_VENOM)
                {
                    proprt[RAP_BRAND] = SPWPN_NORMAL;
                }
            }
        }

        if (is_demonic(item))
        {
            switch (random2(9))
            {
            case 0:
                proprt[RAP_BRAND] = SPWPN_DRAINING;
                break;
            case 1:
                proprt[RAP_BRAND] = SPWPN_FLAMING;
                break;
            case 2:
                proprt[RAP_BRAND] = SPWPN_FREEZING;
                break;
            case 3:
                proprt[RAP_BRAND] = SPWPN_ELECTROCUTION;
                break;
            case 4:
                proprt[RAP_BRAND] = SPWPN_VAMPIRICISM;
                break;
            case 5:
                proprt[RAP_BRAND] = SPWPN_PAIN;
                break;
            case 6:
                proprt[RAP_BRAND] = SPWPN_VENOM;
                break;
            default:
                power_level -= 2;
            }
            power_level += 2;
        }
        else if (one_chance_in(3))
            proprt[RAP_BRAND] = SPWPN_NORMAL;
        else
            power_level++;
    }

    if (!one_chance_in(5))
    {
        // AC mod - not for armours or rings of protection
        if (one_chance_in(4 + power_level)
            && aclass != OBJ_ARMOUR
            && (aclass != OBJ_JEWELLERY || atype != RING_PROTECTION))
        {
            proprt[RAP_AC] = 1 + random2(3) + random2(3) + random2(3);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_AC] -= 1 + random2(3) + random2(3) + random2(3);
                power_level--;
            }
        }

        // ev mod - not for rings of evasion
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_EVASION))
        {
            proprt[RAP_EVASION] = 1 + random2(3) + random2(3) + random2(3);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_EVASION] -= 1 + random2(3) + random2(3)
                    + random2(3);
                power_level--;
            }
        }

        // str mod - not for rings of strength
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_STRENGTH))
        {
            proprt[RAP_STRENGTH] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_STRENGTH] -= 1 + random2(3) + random2(3)
                                          + random2(3);
                power_level--;
            }
        }

        // int mod - not for rings of intelligence
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_INTELLIGENCE))
        {
            proprt[RAP_INTELLIGENCE] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_INTELLIGENCE] -= 1 + random2(3) + random2(3)
                                              + random2(3);
                power_level--;
            }
        }

        // dex mod - not for rings of dexterity
        if (one_chance_in(4 + power_level)
            && (aclass != OBJ_JEWELLERY || atype != RING_DEXTERITY))
        {
            proprt[RAP_DEXTERITY] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_DEXTERITY] -= 1 + random2(3) + random2(3)
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
            proprt[RAP_ACCURACY] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_ACCURACY] -= 1 + random2(3) + random2(3)
                                          + random2(3);
                power_level--;
            }
        }

        if (one_chance_in(4 + power_level))  // to-dam
        {
            proprt[RAP_DAMAGE] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_DAMAGE] -= 1 + random2(3) + random2(3) + random2(3);
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
        proprt[RAP_FIRE] = 1;
        if (one_chance_in(5))
            proprt[RAP_FIRE]++;
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
        proprt[RAP_COLD] = 1;
        if (one_chance_in(5))
            proprt[RAP_COLD]++;
        power_level++;
    }

    if (x_chance_in_y(power_level, 12) || power_level > 7)
        done_powers = true;

    // res_elec
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_ARMOUR || atype != ARM_STORM_DRAGON_ARMOUR))
    {
        proprt[RAP_ELECTRICITY] = 1;
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
        proprt[RAP_POISON] = 1;
        power_level++;
    }

    // prot_life - no necromantic brands on weapons allowed
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY || atype != RING_LIFE_PROTECTION)
        && proprt[RAP_BRAND] != SPWPN_DRAINING
        && proprt[RAP_BRAND] != SPWPN_VAMPIRICISM
        && proprt[RAP_BRAND] != SPWPN_PAIN)
    {
        proprt[RAP_NEGATIVE_ENERGY] = 1;
        power_level++;
    }

    // res magic
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY || atype != RING_PROTECTION_FROM_MAGIC))
    {
        proprt[RAP_MAGIC] = 35 + random2(65);
        power_level++;
    }

    // see_invis
    if (!done_powers
        && one_chance_in(4 + power_level)
        && (aclass != OBJ_JEWELLERY || atype != RING_INVISIBILITY)
        && (aclass != OBJ_ARMOUR || atype != ARM_NAGA_BARDING))
    {
        proprt[RAP_EYESIGHT] = 1;
        power_level++;
    }

    if (x_chance_in_y(power_level, 12) || power_level > 10)
        done_powers = true;

    // turn invis
    if (!done_powers
        && one_chance_in(10)
        && (aclass != OBJ_JEWELLERY || atype != RING_INVISIBILITY))
    {
        proprt[RAP_INVISIBLE] = 1;
        power_level++;
    }

    // levitate
    if (!done_powers
        && one_chance_in(10)
        && (aclass != OBJ_JEWELLERY || atype != RING_LEVITATION))
    {
        proprt[RAP_LEVITATE] = 1;
        power_level++;
    }

    // blink
    if (!done_powers && one_chance_in(10))
    {
        proprt[RAP_BLINK] = 1;
        power_level++;
    }

    // teleport
    if (!done_powers
        && one_chance_in(10)
        && (aclass != OBJ_JEWELLERY || atype != RING_TELEPORTATION))
    {
        proprt[RAP_CAN_TELEPORT] = 1;
        power_level++;
    }

    // go berserk
    if (!done_powers
        && one_chance_in(10)
        && (aclass != OBJ_WEAPONS || !is_range_weapon(item))
        && (aclass != OBJ_JEWELLERY || atype != AMU_RAGE))
    {
        proprt[RAP_BERSERK] = 1;
        power_level++;
    }

    // sense surroundings
    if (!done_powers && one_chance_in(10))
    {
        proprt[RAP_MAPPING] = 1;
        power_level++;
    }

    // Armours get fewer powers, and are also less likely to be
    // cursed than weapons
    if (aclass == OBJ_ARMOUR)
        power_level -= 4;

    if (power_level >= 2 && x_chance_in_y(power_level, 17))
    {
        switch (random2(9))
        {
        case 0:                     // makes noise
            if (aclass != OBJ_WEAPONS)
                break;
            proprt[RAP_NOISES] = 1 + random2(4);
            break;
        case 1:                     // no magic
            proprt[RAP_PREVENT_SPELLCASTING] = 1;
            break;
        case 2:                     // random teleport
            if (aclass != OBJ_WEAPONS)
                break;
            proprt[RAP_CAUSE_TELEPORTATION] = 5 + random2(15);
            break;
        case 3:   // no teleport - doesn't affect some instantaneous
                  // teleports
            if (aclass == OBJ_JEWELLERY && atype == RING_TELEPORTATION)
                break;              // already is a ring of tport
            if (aclass == OBJ_JEWELLERY && atype == RING_TELEPORT_CONTROL)
                break;              // already is a ring of tport ctrl
            proprt[RAP_BLINK] = 0;
            proprt[RAP_CAN_TELEPORT] = 0;
            proprt[RAP_PREVENT_TELEPORTATION] = 1;
            break;
        case 4:                     // berserk on attack
            if (aclass != OBJ_WEAPONS || is_range_weapon(item))
                break;
            proprt[RAP_ANGRY] = 1 + random2(8);
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
            proprt[RAP_FIRE] = -1;
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
            proprt[RAP_COLD] = -1;
            break;
        case 7:                     // speed metabolism
            if (aclass == OBJ_JEWELLERY && atype == RING_HUNGER)
                break;              // already is a ring of hunger
            if (aclass == OBJ_JEWELLERY && atype == RING_SUSTENANCE)
                break;              // already is a ring of sustenance
            proprt[RAP_METABOLISM] = 1 + random2(3);
            break;
        case 8:
            // emits mutagenic radiation - increases
            // magic_contamination.  property is chance (1 in ...) of
            // increasing magic_contamination
            proprt[RAP_MUTAGENIC] = 2 + random2(4);
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
        proprt[RAP_STEALTH] = 10 + random2(70);

        if (one_chance_in(4))
        {
            proprt[RAP_STEALTH] = -proprt[RAP_STEALTH] - random2(20);
            power_level--;
        }
    }

    if (randart_wpn_num_props(proprt) == 0)
        power_level += _randart_add_one_property(item, proprt);

    if ((power_level < 2 && one_chance_in(5)) || one_chance_in(30))
    {
        if (one_chance_in(4))
            proprt[RAP_CURSED] = 1 + random2(5);
        else
            proprt[RAP_CURSED] = -1;
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

static bool _init_randart_book(item_def &book)
{
    ASSERT(book.sub_type == BOOK_RANDART_LEVEL
           || book.sub_type == BOOK_RANDART_THEME);
    ASSERT(book.plus != 0);

    god_type god;
    bool redo = (!origin_is_god_gift(book, &god) || god != GOD_XOM);

    // Plus and plus2 contain paramaters to make_book_foo_randart()
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
            book_good = make_book_level_randart(book);
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

static bool _init_randart_properties(item_def &item)
{
    ASSERT( is_random_artefact( item ) );
    CrawlHashTable &props = item.props;
    if (!props.exists( RANDART_PROPS_KEY ))
        props[RANDART_PROPS_KEY].new_vector(SV_SHORT).resize(RA_PROPERTIES);

    CrawlVector &rap = props[RANDART_PROPS_KEY];
    rap.set_max_size(RA_PROPERTIES);

    for (vec_size i = 0; i < RA_PROPERTIES; i++)
        rap[i] = (short) 0;

    if (is_unrandom_artefact( item ))
    {
        const unrandart_entry *unrand = _seekunrandart( item );

        for (int i = 0; i < RA_PROPERTIES; i++)
            rap[i] = (short) unrand->prpty[i];

        return (true);
    }

    if (item.base_type == OBJ_BOOKS)
        return _init_randart_book(item);

    randart_properties_t prop;
    _get_randart_properties(item, prop);

    for (int i = 0; i < RA_PROPERTIES; i++)
    {
        if (i == RAP_CURSED && prop[i] < 0)
        {
            do_curse_item(item);
            continue;
        }
        rap[i] = (short) prop[i];
    }

    return (true);
}

void randart_wpn_properties( const item_def &item,
                             randart_properties_t  &proprt,
                             randart_known_props_t &known)
{
    ASSERT( is_random_artefact( item ) );
    ASSERT( item.props.exists( KNOWN_PROPS_KEY ) );

    const CrawlStoreValue &_val = item.props[KNOWN_PROPS_KEY];
    ASSERT( _val.get_type() == SV_VEC );
    const CrawlVector &known_vec = _val.get_vector();
    ASSERT( known_vec.get_type()     == SV_BOOL );
    ASSERT( known_vec.size()         == RA_PROPERTIES);
    ASSERT( known_vec.get_max_size() == RA_PROPERTIES);

    if (item_ident( item, ISFLAG_KNOW_PROPERTIES ))
    {
        for (vec_size i = 0; i < RA_PROPERTIES; i++)
            known[i] = (bool) true;
    }
    else
    {
        for (vec_size i = 0; i < RA_PROPERTIES; i++)
            known[i] = known_vec[i];
    }

    if (item.props.exists( RANDART_PROPS_KEY ))
    {
        const CrawlVector &rap_vec = item.props[RANDART_PROPS_KEY].get_vector();
        ASSERT( rap_vec.get_type()     == SV_SHORT );
        ASSERT( rap_vec.size()         == RA_PROPERTIES);
        ASSERT( rap_vec.get_max_size() == RA_PROPERTIES);

        for (vec_size i = 0; i < RA_PROPERTIES; i++)
            proprt[i] = rap_vec[i].get_short();
    }
    else if (is_unrandom_artefact( item ))
    {
        const unrandart_entry *unrand = _seekunrandart( item );

        for (int i = 0; i < RA_PROPERTIES; i++)
            proprt[i] = (short) unrand->prpty[i];
    }
    else
    {
        _get_randart_properties(item, proprt);
    }
}


void randart_wpn_properties( const item_def &item,
                             randart_properties_t &proprt )
{
    randart_known_props_t known;

    randart_wpn_properties(item, proprt, known);
}

int randart_wpn_property( const item_def &item, randart_prop_type prop,
                          bool &_known )
{
    randart_properties_t  proprt;
    randart_known_props_t known;

    randart_wpn_properties( item, proprt, known );

    _known = known[prop];

    return ( proprt[prop] );
}

int randart_wpn_property( const item_def &item, randart_prop_type prop )
{
    bool known;

    return randart_wpn_property( item, prop, known );
}

int randart_known_wpn_property( const item_def &item, randart_prop_type prop )
{
    randart_properties_t  proprt;
    randart_known_props_t known;

    randart_wpn_properties( item, proprt, known );

    if (known[prop])
        return ( proprt[prop] );
    else
        return (0);
}

int randart_wpn_num_props( const item_def &item )
{
    randart_properties_t proprt;
    randart_wpn_properties( item, proprt );

    return randart_wpn_num_props( proprt );
}

int randart_wpn_num_props( const randart_properties_t &proprt )
{
    int num = 0;

    for (int i = 0; i < RAP_NUM_PROPERTIES; i++)
        if (proprt[i] != 0)
            num++;

    return num;
}

void randart_wpn_learn_prop( item_def &item, randart_prop_type prop )
{
    ASSERT( is_random_artefact( item ) );
    ASSERT( item.props.exists( KNOWN_PROPS_KEY ) );
    CrawlStoreValue &_val = item.props[KNOWN_PROPS_KEY];
    ASSERT( _val.get_type() == SV_VEC );
    CrawlVector &known_vec = _val.get_vector();
    ASSERT( known_vec.get_type()     == SV_BOOL );
    ASSERT( known_vec.size()         == RA_PROPERTIES);
    ASSERT( known_vec.get_max_size() == RA_PROPERTIES);

    if (item_ident( item, ISFLAG_KNOW_PROPERTIES ))
        return;

    known_vec[prop] = (bool) true;
    if (Options.autoinscribe_randarts)
        add_autoinscription( item, randart_auto_inscription(item));
}

bool randart_wpn_known_prop( const item_def &item, randart_prop_type prop )
{
    bool known;
    randart_wpn_property( item, prop, known );
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
        // Only in appearance distinguish between amulets and rings.
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

std::string artefact_name(const item_def &item, bool appearance)
{
    ASSERT(is_artefact(item));

    ASSERT(item.base_type == OBJ_WEAPONS
           || item.base_type == OBJ_ARMOUR
           || item.base_type == OBJ_JEWELLERY
           || item.base_type == OBJ_BOOKS);

    if (is_fixed_artefact( item ))
        return _get_fixedart_name( item );

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
        if (appear.empty() // nothing found for lookup
            // Don't allow "jewelled jewelled helmet".
            || item.base_type == OBJ_ARMOUR
               && item.sub_type == ARM_HELMET
               && appear == "jewelled"
               && get_helmet_desc(item) == THELM_DESC_JEWELLED)
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
            name = getRandNameString(lookup);

            if (name.empty() && god_gift)
            {
                // If nothing found, try god name alone.
                name = getRandNameString(
                           god_name(static_cast<god_type>(item_orig), false));

                if (name.empty())
                {
                    // If still nothing found, try base type alone.
                    name = getRandNameString(
                               _get_artefact_type(item).c_str());
                }
            }

            name = _replace_name_parts(name, item);
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

std::string get_artefact_name( const item_def &item )
{
    ASSERT( is_artefact( item ) );

    if (item_type_known(item))
    {
        // print artefact's real name
        if (item.props.exists(RANDART_NAME_KEY))
            return item.props[RANDART_NAME_KEY].get_string();
        return artefact_name(item, false);
    }
    // print artefact appearance
    if (item.props.exists(RANDART_APPEAR_KEY))
        return item.props[RANDART_APPEAR_KEY].get_string();
    return artefact_name(item, false);
}

void set_randart_name( item_def &item, const std::string &name )
{
    ASSERT( is_random_artefact( item ));
    ASSERT( !name.empty() );
    item.props[RANDART_NAME_KEY].get_string() = name;
}

void set_randart_appearance( item_def &item, const std::string &appear )
{
    ASSERT( is_random_artefact( item ));
    ASSERT( !appear.empty() );
    item.props[RANDART_APPEAR_KEY].get_string() = appear;
}

int find_unrandart_index(const item_def& artefact)
{
    for (int i = 0; i < NO_UNRANDARTS; i++)
    {
        const unrandart_entry& candidate = unranddata[i];
        if (candidate.ura_cl == artefact.base_type
            && candidate.ura_ty == artefact.sub_type
            && candidate.ura_pl == artefact.plus
            && candidate.ura_pl2 == artefact.plus2
            && candidate.ura_col == artefact.colour)
        {
            return i;
        }
    }

    return (-1);
}

static unrandart_entry *_seekunrandart( const item_def &item )
{
    const int idx = find_unrandart_index(item);
    if (idx == -1)
        return &unranddata[0];  // dummy unrandart
    else
        return &unranddata[idx];
}

int find_unrandart_index(int item_number)
{
    return find_unrandart_index(mitm[item_number]);
}

int find_okay_unrandart(unsigned char aclass, unsigned char atype)
{
    int ret = -1;

    // Pick randomly among not-yet-existing unrandarts with the proper
    // base_type and sub_type.
    for (int i = 0, count = 0; i < NO_UNRANDARTS; i++)
    {
        if (unranddata[i].ura_cl == aclass
            && !does_unrandart_exist(i)
            && (atype == OBJ_RANDOM || unranddata[i].ura_ty == atype))
        {
            count++;

            if (one_chance_in(count))
                ret = i;
        }
    }

    return (ret);
}

struct fixedart_setting
{
    int which;
    const char* name;
    const char* appearance;
    object_class_type base;
    int subtype;
    int acc;
    int dam;
    int colour;
    bool curse;
};

const fixedart_setting fixedarts[] = {

    {
        SPWPN_SINGING_SWORD,
        "Singing Sword",
        "golden long sword",
        OBJ_WEAPONS,
        WPN_LONG_SWORD,
        7,
        7,
        YELLOW,
        false
    },

    {
        SPWPN_WRATH_OF_TROG,
        "Wrath of Trog",
        "bloodstained battleaxe",
        OBJ_WEAPONS,
        WPN_BATTLEAXE,
        3,
        11,
        RED,
        false
    },

    {
        SPWPN_SCYTHE_OF_CURSES,
        "Scythe of Curses",
        "warped scythe",
        OBJ_WEAPONS,
        WPN_SCYTHE,
        13,
        13,
        DARKGREY,
        true
    },

    {
        SPWPN_MACE_OF_VARIABILITY,
        "Mace of Variability",
        "shimmering mace",
        OBJ_WEAPONS,
        WPN_MACE,
        random2(16) - 4,
        random2(16) - 4,
        random_colour(),
        false
    },

    {
        SPWPN_GLAIVE_OF_PRUNE,
        "Glaive of Prune",
        "purple glaive",
        OBJ_WEAPONS,
        WPN_GLAIVE,
        0,
        12,
        MAGENTA,
        false
    },

    {
        SPWPN_SCEPTRE_OF_TORMENT,
        "Sceptre of Torment",
        "jewelled golden mace",
        OBJ_WEAPONS,
        WPN_MACE,
        7,
        6,
        YELLOW,
        false
    },

    {
        SPWPN_SWORD_OF_ZONGULDROK,
        "Sword of Zonguldrok",
        "bone long sword",
        OBJ_WEAPONS,
        WPN_LONG_SWORD,
        9,
        9,
        LIGHTGREY,
        false
    },

    {
        SPWPN_SWORD_OF_POWER,
        "Sword of Power",
        "chunky great sword",
        OBJ_WEAPONS,
        WPN_GREAT_SWORD,
        0, // set on wield
        0, // set on wield
        RED,
        false
    },

    {
        SPWPN_STAFF_OF_OLGREB,
        "Staff of Olgreb",
        "green glowing staff",
        OBJ_WEAPONS,
        WPN_QUARTERSTAFF,
        0, // set on wield
        0, // set on wield
        GREEN,
        false
    },

    {
        SPWPN_VAMPIRES_TOOTH,
        "Vampire's Tooth",
        "ivory dagger",
        OBJ_WEAPONS,
        WPN_DAGGER,
        3,
        4,
        WHITE,
        false
    },

    {
        SPWPN_STAFF_OF_WUCAD_MU,
        "Staff of Wucad Mu",
        "ephemeral quarterstaff",
        OBJ_WEAPONS,
        WPN_QUARTERSTAFF,
        0, // set on wield
        0, // set on wield
        BROWN,
        false
    },

    {
        SPWPN_SWORD_OF_CEREBOV,
        "Sword of Cerebov",
        "great serpentine sword",
        OBJ_WEAPONS,
        WPN_GREAT_SWORD,
        6,
        6,
        YELLOW,
        true
    },

    {
        SPWPN_STAFF_OF_DISPATER,
        "Staff of Dispater",
        "golden staff",
        OBJ_WEAPONS,
        WPN_QUARTERSTAFF,
        4,
        4,
        YELLOW,
        false
    },

    {
        SPWPN_SCEPTRE_OF_ASMODEUS,
        "Sceptre of Asmodeus",
        "ruby sceptre",
        OBJ_WEAPONS,
        WPN_QUARTERSTAFF,
        7,
        7,
        RED,
        false
    }
};

static const char* _get_fixedart_name(const item_def &item)
{
    // Find the appropriate fixed artefact.
    for (unsigned int i = 0; i < ARRAYSZ(fixedarts); ++i)
    {
        if (fixedarts[i].which == item.special)
        {
            const fixedart_setting *fixed = &fixedarts[i];
            return (item_type_known(item) ? fixed->name : fixed->appearance);
        }
    }
    return (item_type_known(item) ? "Unnamed Artefact" : "buggy fixedart");
}

int get_fixedart_num( const char *name )
{
    for (unsigned int i = 0; i < ARRAYSZ(fixedarts); ++i)
    {
        std::string art = fixedarts[i].name;
        lowercase(art);
        if (replace_all(art, " ", "_") == name)
            return fixedarts[i].which;
    }
    return SPWPN_NORMAL;
}

// which == 0 (default) gives random fixed artefact.
// Returns true if successful.
bool make_item_fixed_artefact( item_def &item, bool in_abyss, int which )
{
    const bool force = (which != 0);
    const fixedart_setting *fixedart = NULL;

    if (!force)
    {
        which = SPWPN_START_FIXEDARTS
                + random2(SPWPN_START_NOGEN_FIXEDARTS - SPWPN_START_FIXEDARTS);
    }

    const unique_item_status_type status =
        get_unique_item_status( OBJ_WEAPONS, which );

    if (!force
        && (status == UNIQ_EXISTS
            || in_abyss && status == UNIQ_NOT_EXISTS
            || !in_abyss && status == UNIQ_LOST_IN_ABYSS))
    {
        return (false);
    }

    // Find the appropriate fixed artefact.
    for (unsigned int i = 0; i < ARRAYSZ(fixedarts); ++i)
    {
        if (fixedarts[i].which == which)
        {
            fixedart = &fixedarts[i];
            break;
        }
    }

    // None found?
    if (fixedart == NULL)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_ERROR, "Couldn't find fixed artefact %d", which);
#endif
        return (false);
    }

    // If we get here, we've made the artefact
    item.base_type = fixedart->base;
    item.sub_type  = fixedart->subtype;
    item.plus      = fixedart->acc;
    item.plus2     = fixedart->dam;
    item.colour    = fixedart->colour;
    item.special   = which;
    item.quantity  = 1;

    // get true artefact name
    ASSERT(!item.props.exists( RANDART_NAME_KEY ));
    item.props[RANDART_NAME_KEY].get_string() = fixedart->name;

    // get artefact appearance
    ASSERT(!item.props.exists( RANDART_APPEAR_KEY ));
    item.props[RANDART_APPEAR_KEY].get_string() = fixedart->appearance;

    if (fixedart->curse)
        do_curse_item(item);

    // Items originally generated in the abyss and not found will be
    // shifted to "lost in abyss", and will only be found there. -- bwr
    set_unique_item_status( OBJ_WEAPONS, which, UNIQ_EXISTS );

    return (true);
}

static bool _randart_is_redundant( const item_def &item,
                                   randart_properties_t &proprt )
{
    if (item.base_type != OBJ_JEWELLERY)
        return (false);

    randart_prop_type provides  = RAP_NUM_PROPERTIES;
    randart_prop_type provides2 = RAP_NUM_PROPERTIES;

    switch (item.sub_type)
    {
    case RING_PROTECTION:
        provides = RAP_AC;
        break;

    case RING_FIRE:
    case RING_PROTECTION_FROM_FIRE:
        provides = RAP_FIRE;
        break;

    case RING_POISON_RESISTANCE:
        provides = RAP_POISON;
        break;

    case RING_ICE:
    case RING_PROTECTION_FROM_COLD:
        provides = RAP_COLD;
        break;

    case RING_STRENGTH:
        provides = RAP_STRENGTH;
        break;

    case RING_SLAYING:
        provides  = RAP_DAMAGE;
        provides2 = RAP_ACCURACY;
        break;

    case RING_SEE_INVISIBLE:
        provides = RAP_EYESIGHT;
        break;

    case RING_INVISIBILITY:
        provides = RAP_INVISIBLE;
        break;

    case RING_HUNGER:
        provides = RAP_METABOLISM;
        break;

    case RING_TELEPORTATION:
        provides  = RAP_CAN_TELEPORT;
        provides2 = RAP_CAUSE_TELEPORTATION;
        break;

    case RING_EVASION:
        provides = RAP_EVASION;
        break;

    case RING_DEXTERITY:
        provides = RAP_DEXTERITY;
        break;

    case RING_INTELLIGENCE:
        provides = RAP_INTELLIGENCE;
        break;

    case RING_MAGICAL_POWER:
        provides = RAP_MAGICAL_POWER;
        break;

    case RING_LEVITATION:
        provides = RAP_LEVITATE;
        break;

    case RING_LIFE_PROTECTION:
        provides = RAP_AC;
        break;

    case RING_PROTECTION_FROM_MAGIC:
        provides = RAP_MAGIC;
        break;

    case AMU_RAGE:
        provides = RAP_BERSERK;
        break;

    case AMU_INACCURACY:
        provides = RAP_ACCURACY;
        break;
    }

    if (provides == RAP_NUM_PROPERTIES)
        return (false);

    if (proprt[provides] != 0)
        return (true);

    if (provides2 == RAP_NUM_PROPERTIES)
        return (false);

    if (proprt[provides2] != 0)
        return (true);

    return (false);
}

static bool _randart_is_conflicting( const item_def &item,
                                     randart_properties_t &proprt )
{
    if (item.base_type != OBJ_JEWELLERY)
        return (false);

    randart_prop_type conflicts = RAP_NUM_PROPERTIES;

    switch (item.sub_type)
    {
    case RING_SUSTENANCE:
        conflicts = RAP_METABOLISM;
        break;

    case RING_FIRE:
    case RING_ICE:
    case RING_WIZARDRY:
    case RING_MAGICAL_POWER:
        conflicts = RAP_PREVENT_SPELLCASTING;
        break;

    case RING_TELEPORTATION:
    case RING_TELEPORT_CONTROL:
        conflicts = RAP_PREVENT_TELEPORTATION;
        break;

    case AMU_RESIST_MUTATION:
        conflicts = RAP_MUTAGENIC;
        break;

    case AMU_RAGE:
        conflicts = RAP_STEALTH;
        break;
    }

    if (conflicts == RAP_NUM_PROPERTIES)
        return (false);

    if (proprt[conflicts] != 0)
        return (true);

    return (false);
}

bool randart_is_bad( const item_def &item, randart_properties_t &proprt )
{
    if (item.base_type == OBJ_BOOKS)
        return (false);

    if (randart_wpn_num_props( proprt ) == 0)
        return (true);

    return (_randart_is_redundant( item, proprt )
            || _randart_is_conflicting( item, proprt ));
}

bool randart_is_bad( const item_def &item )
{
    randart_properties_t proprt;
    randart_wpn_properties( item, proprt );

    return randart_is_bad( item, proprt);
}

bool make_item_blessed_blade( item_def &item )
{
    if (item.base_type != OBJ_WEAPONS)
        return (false);

    // already is an artefact
    if (is_artefact(item))
        return (false);

    // mark as a random artefact
    item.flags |= ISFLAG_RANDART;

    ASSERT(!item.props.exists( KNOWN_PROPS_KEY ));
    item.props[KNOWN_PROPS_KEY].new_vector(SV_BOOL).resize(RA_PROPERTIES);
    CrawlVector &known = item.props[KNOWN_PROPS_KEY];
    known.set_max_size(RA_PROPERTIES);
    for (vec_size i = 0; i < RA_PROPERTIES; i++)
        known[i] = (bool) false;

    ASSERT(!item.props.exists( RANDART_PROPS_KEY ));
    item.props[RANDART_PROPS_KEY].new_vector(SV_SHORT).resize(RA_PROPERTIES);
    CrawlVector &rap = item.props[RANDART_PROPS_KEY];
    rap.set_max_size(RA_PROPERTIES);
    for (vec_size i = 0; i < RA_PROPERTIES; i++)
        rap[i] = (short) 0;

    // blessed blade of the Shining One
    rap[RAP_BRAND] = (short) SPWPN_HOLY_WRATH;

    // set artefact name
    ASSERT(!item.props.exists( RANDART_NAME_KEY ));
    item.props[RANDART_NAME_KEY].get_string() = "Blessed Blade";

    // set artefact appearance
    ASSERT(!item.props.exists( RANDART_APPEAR_KEY ));
    item.props[RANDART_APPEAR_KEY].get_string() = "brightly glowing blade";

    // in case this is ever needed anywhere
    item.special = (random_int() & RANDART_SEED_MASK);

    return (true);
}

bool make_item_randart( item_def &item )
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

    if (item_is_mundane(item))
        return (false);

    ASSERT(!item.props.exists( KNOWN_PROPS_KEY ));
    ASSERT(!item.props.exists( RANDART_NAME_KEY ));
    ASSERT(!item.props.exists( RANDART_APPEAR_KEY ));
    item.props[KNOWN_PROPS_KEY].new_vector(SV_BOOL).resize(RA_PROPERTIES);
    CrawlVector &known = item.props[KNOWN_PROPS_KEY];
    known.set_max_size(RA_PROPERTIES);
    for (vec_size i = 0; i < RA_PROPERTIES; i++)
        known[i] = (bool) false;

    item.flags |= ISFLAG_RANDART;

    god_type god_gift;
    (void) origin_is_god_gift(item, &god_gift);

    do
    {
        item.special = (random_int() & RANDART_SEED_MASK);
        // Now that we found something, initialize the props array.
        if (!_init_randart_properties(item))
        {
            // Something went wrong that no amount of changing
            // item.special will fix.
            item.special = 0;
            item.props.erase(RANDART_PROPS_KEY);
            item.props.erase(KNOWN_PROPS_KEY);
            item.flags &= ~ISFLAG_RANDART;
            return (false);
        }
    }
    while (randart_is_bad(item)
           || god_gift != GOD_NO_GOD && !_god_fits_artefact(god_gift, item));

    // get true artefact name
    if (item.props.exists( RANDART_NAME_KEY ))
        ASSERT(item.props[RANDART_NAME_KEY].get_type() == SV_STR);
    else
        item.props[RANDART_NAME_KEY].get_string() = artefact_name(item, false);

    // get artefact appearance
    if (item.props.exists( RANDART_APPEAR_KEY ))
        ASSERT(item.props[RANDART_APPEAR_KEY].get_type() == SV_STR);
    else
        item.props[RANDART_APPEAR_KEY].get_string() = artefact_name(item, true);

    return (true);
}

bool make_item_unrandart( item_def &item, int unrand_index )
{
    if (!item.props.exists( KNOWN_PROPS_KEY ))
    {
        item.props[KNOWN_PROPS_KEY].new_vector(SV_BOOL).resize(RA_PROPERTIES);
        CrawlVector &known = item.props[KNOWN_PROPS_KEY];
        known.set_max_size(RA_PROPERTIES);
        for (vec_size i = 0; i < RA_PROPERTIES; i++)
            known[i] = (bool) false;
    }

    const unrandart_entry *unrand = &unranddata[unrand_index];
    item.base_type = unrand->ura_cl;
    item.sub_type  = unrand->ura_ty;
    item.plus      = unrand->ura_pl;
    item.plus2     = unrand->ura_pl2;
    item.colour    = unrand->ura_col;

    item.flags |= ISFLAG_UNRANDART;
    _init_randart_properties(item);

    item.special = unrand->prpty[RAP_BRAND];
    if (item.special != 0)
    {
        do_curse_item( item );

        // If the property doesn't allow for recursing, clear it now.
        if (item.special < 0)
            item.special = 0;
    }

    // get true artefact name
    ASSERT(!item.props.exists( RANDART_NAME_KEY ));
    item.props[RANDART_NAME_KEY].get_string() = unrand->name;

    // get artefact appearance
    ASSERT(!item.props.exists( RANDART_APPEAR_KEY ));
    item.props[RANDART_APPEAR_KEY].get_string() = unrand->unid_name;

    set_unrandart_exist( unrand_index, true );

    return (true);
}

const char *unrandart_descrip( int which_descrip, const item_def &item )
{
    // Eventually it would be great to have randomly generated descriptions
    // for randarts.
    const unrandart_entry *unrand = _seekunrandart( item );

    return ((which_descrip == 0) ? unrand->spec_descrip1 :
            (which_descrip == 1) ? unrand->spec_descrip2 :
            (which_descrip == 2) ? unrand->spec_descrip3
                                 : "Unknown.");
}

void randart_set_properties( item_def             &item,
                             randart_properties_t &proprt )
{
    ASSERT( is_random_artefact( item ) );
    ASSERT( !is_unrandom_artefact ( item ) );
    ASSERT( item.props.exists( RANDART_PROPS_KEY ) );

    CrawlVector &rap_vec = item.props[RANDART_PROPS_KEY].get_vector();
    ASSERT( rap_vec.get_type()     == SV_SHORT );
    ASSERT( rap_vec.size()         == RA_PROPERTIES);
    ASSERT( rap_vec.get_max_size() == RA_PROPERTIES);

    for (vec_size i = 0; i < RA_PROPERTIES; i++)
        rap_vec[i].get_short() = proprt[i];
}

void randart_set_property( item_def         &item,
                           randart_prop_type prop,
                           int               val )
{
    ASSERT( is_random_artefact( item ) );
    ASSERT( !is_unrandom_artefact ( item ) );
    ASSERT( item.props.exists( RANDART_PROPS_KEY ) );

    CrawlVector &rap_vec = item.props[RANDART_PROPS_KEY].get_vector();
    ASSERT( rap_vec.get_type()     == SV_SHORT );
    ASSERT( rap_vec.size()         == RA_PROPERTIES);
    ASSERT( rap_vec.get_max_size() == RA_PROPERTIES);

    rap_vec[prop].get_short() = val;
}
