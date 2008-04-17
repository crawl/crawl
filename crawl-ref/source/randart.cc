/*
 *  File:       randart.cc
 *  Summary:    Random and unrandom artefact functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <8>     19 Jun 99   GDL    added IBMCPP support
 *   <7>     14/12/99    LRH    random2 -> random5
 *   <6>     11/06/99    cdl    random4 -> random2
 *
 *   <1>     -/--/--     LRH    Created
 */

#include "AppHdr.h"
#include "randart.h"

#include <cstdlib>
#include <climits>
#include <string.h>
#include <stdio.h>

#include "externs.h"

#include "database.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "place.h"
#include "player.h"
#include "religion.h"
#include "stuff.h"

#define KNOWN_PROPS_KEY "randart_known_props"

/*
   The initial generation of a randart is very simple - it occurs
   in dungeon.cc and consists of giving it a few random things - plus & plus2
   mainly.
*/

static bool god_fits_artefact(const god_type which_god, const item_def &item)
{
    if (which_god == GOD_NO_GOD)
        return (false);

    const int brand = get_weapon_brand(item);

    if (is_evil_god(which_god) && brand == SPWPN_HOLY_WRATH)
        return (false);
    else if (is_good_god(which_god) && (brand == SPWPN_DRAINING
              || brand == SPWPN_PAIN || brand == SPWPN_VAMPIRICISM))
    {
        return (false);
    }

    switch (which_god)
    {
    case GOD_BEOGH:
         if (brand == SPWPN_ORC_SLAYING)
             return (false); // goes against orcish theme
         break;

    case GOD_ELYVILON: // peaceful healer god, no weapons, no berserking
         if (item.base_type == OBJ_WEAPONS)
             return (false);

         if (item.base_type == OBJ_JEWELLERY && item.sub_type == AMU_RAGE)
             return (false);

         if (randart_wpn_property( item, RAP_ANGRY )
             || randart_wpn_property( item, RAP_BERSERK ))
         {
             return (false);
         }
         break;

    case GOD_OKAWARU: // precision fighter
         if (item.base_type == OBJ_JEWELLERY && item.sub_type == AMU_INACCURACY)
             return (false);
         break;

    case GOD_ZIN:
         if (item.base_type == OBJ_JEWELLERY && item.sub_type == RING_HUNGER)
             return (false); // goes against food theme

         if (randart_wpn_property( item, RAP_MUTAGENIC ))
             return (false); // goes against anti-mutagenic theme
         break;

    case GOD_SHINING_ONE:
         if (brand != SPWPN_HOLY_WRATH)
             return (false); // goes against holiness theme

         if (randart_wpn_property( item, RAP_INVISIBLE )
             || randart_wpn_property( item, RAP_CURSED )
             || randart_wpn_property( item, RAP_STEALTH ) > 0)
         {
             return (false); // goes against honourable combat theme
         }

         if (randart_wpn_property( item, RAP_AC ) < 0
             || randart_wpn_property( item, RAP_EVASION ) < 0
             || randart_wpn_property( item, RAP_STRENGTH ) < 0
             || randart_wpn_property( item, RAP_INTELLIGENCE ) < 0
             || randart_wpn_property( item, RAP_DEXTERITY ) < 0
             || randart_wpn_property( item, RAP_FIRE ) < 0
             || randart_wpn_property( item, RAP_COLD ) < 0
             || randart_wpn_property( item, RAP_NOISES )
             || randart_wpn_property( item, RAP_PREVENT_SPELLCASTING )
             || randart_wpn_property( item, RAP_CAUSE_TELEPORTATION )
             || randart_wpn_property( item, RAP_PREVENT_TELEPORTATION )
             || randart_wpn_property( item, RAP_ANGRY )
             || randart_wpn_property( item, RAP_METABOLISM )
             || randart_wpn_property( item, RAP_MUTAGENIC ))
         {
             return (false); // goes against blessed theme
         }

         break;

    case GOD_SIF_MUNA:
    case GOD_KIKUBAAQUDGHA:
    case GOD_VEHUMET:
         if (randart_wpn_property( item, RAP_PREVENT_SPELLCASTING))
             return (false);
         break;

    case GOD_TROG: // hates anything enhancing magic
         if (item.base_type == OBJ_JEWELLERY && (item.sub_type == RING_WIZARDRY
             || item.sub_type == RING_FIRE || item.sub_type == RING_ICE
             || item.sub_type == RING_MAGICAL_POWER))
         {
             return (false);
         }
         if (brand == SPWPN_PAIN) // involves magic
             return (false);

         if (randart_wpn_property( item, RAP_MAGICAL_POWER ))
             return (false);

    default:
         break;
    }

    return (true);
}

static god_type gift_from_god(const item_def item)
{
    // maybe god gift?
    god_type god_gift = GOD_NO_GOD;

    if (item.orig_monnum < 0)
    {
        int help = -item.orig_monnum - 2;
        if (help > GOD_NO_GOD && help < NUM_GODS)
            god_gift = static_cast<god_type>(help);
    }

    return god_gift;
}

static std::string replace_name_parts(const std::string name_in,
                                      const item_def item)
{
    std::string name = name_in;

    god_type god_gift = gift_from_god(item);

    // Don't allow "player's Death" type names for god gifts (except Xom!)
    if (god_gift != GOD_NO_GOD && god_gift != GOD_XOM
        && name.find("@player_name@'s", 0) != std::string::npos
        && name.find("Death", 0) != std::string::npos)
    {
        // simply overwrite the name with one of type "god's Favour"
        name = "of ";
        name += god_name(god_gift, false);
        name += "'s ";
        name += getRandNameString("divine_esteem");
    }
    name = replace_all(name, "@player_name@", you.your_name);

    name = replace_all(name, "@player_species@",
                 species_name(static_cast<species_type>(you.species), 1));

    name = replace_all(name, "@race_name@",
                 species_name(static_cast<species_type>(random2(SP_ELF)),1));

    if (name.find("@branch_name@", 0) != std::string::npos)
    {
        std::string place;
        if (one_chance_in(5))
        {
            switch(random2(8))
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

    // occasionally use long name for Xom (see religion.cc)
    name = replace_all(name, "@xom_name@", god_name(GOD_XOM, coinflip()));

    if (name.find("@god_name@", 0) != std::string::npos)
    {
        god_type which_god;

        // God gifts will always get the gifting god's name
        if (god_gift != GOD_NO_GOD)
            which_god = god_gift;
        else
        {
           do
           {
                which_god = static_cast<god_type>(random2(NUM_GODS));
           }
           while (!god_fits_artefact(which_god, item));
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

// Remember: disallow unrandart creation in abyss/pan

/*
   The following unrandart bits were taken from $pellbinder's mon-util code
   (see mon-util.h & mon-util.cc) and modified (LRH). They're in randart.cc and
   not randart.h because they're only used in this code module.
*/

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

static unrandart_entry *seekunrandart( const item_def &item );

void set_unrandart_exist(int whun, bool is_exist)
{
    unrandart_exist[whun] = is_exist;
}

bool does_unrandart_exist(int whun)
{
    return (unrandart_exist[whun]);
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

static long calc_seed( const item_def &item )
{
    return (item.special & RANDART_SEED_MASK);
}

void randart_desc_properties( const item_def &item,
                              randart_properties_t &proprt,
                              randart_known_props_t &known,
                              bool force_fake_props)
{
    randart_wpn_properties( item, proprt, known);

    if ( !force_fake_props && item_ident( item, ISFLAG_KNOW_PROPERTIES ) )
        return;

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

    case RING_STRENGTH:
        fake_rap = RAP_STRENGTH;
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

    case RING_DEXTERITY:
        fake_rap  = RAP_DEXTERITY;
        fake_plus = item.plus;
        break;

    case RING_INTELLIGENCE:
        fake_rap  = RAP_INTELLIGENCE;
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

    if (fake_rap != RAP_NUM_PROPERTIES)
        proprt[fake_rap] += fake_plus;

    if (fake_rap2 != RAP_NUM_PROPERTIES)
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

inline static void randart_propset( randart_properties_t &p,
                                    randart_prop_type pt,
                                    int value,
                                    bool neg )
{
    p[pt] = neg? -value : value;
}

static int randart_add_one_property( const item_def &item,
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
    if (cl == OBJ_ARMOUR || (cl == OBJ_JEWELLERY && ty == RING_PROTECTION))
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
        randart_propset(proprt, RAP_AC,
                        1 + random2(3) + random2(3) + random2(3),
                        negench);
        break;
    case 1:
        randart_propset(proprt, RAP_EVASION,
                        1 + random2(3) + random2(3) + random2(3),
                        negench);
        break;
    case 2:
        randart_propset(proprt, RAP_STRENGTH,
                        1 + random2(3) + random2(3),
                        negench);
        break;
    case 3:
        randart_propset(proprt, RAP_INTELLIGENCE,
                        1 + random2(3) + random2(3),
                        negench);
        break;
    case 4:
        randart_propset(proprt, RAP_DEXTERITY,
                        1 + random2(3) + random2(3),
                        negench);
        break;
    }

    return negench ? 0 : 1;
}

void randart_wpn_properties( const item_def &item,
                             randart_properties_t &proprt,
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

    if ( item_ident( item, ISFLAG_KNOW_PROPERTIES ) )
    {
        for (vec_size i = 0; i < RA_PROPERTIES; i++)
            known[i] = (bool) true;
    }
    else
    {
        for (vec_size i = 0; i < RA_PROPERTIES; i++)
            known[i] = known_vec[i];
    }

    const object_class_type aclass = item.base_type;
    const int atype = item.sub_type;

    int power_level = 0;

    if (is_unrandom_artefact( item ))
    {
        const unrandart_entry *unrand = seekunrandart( item );

        for (int i = 0; i < RA_PROPERTIES; i++)
            proprt[i] = unrand->prpty[i];

        return;
    }

    const long seed = calc_seed( item );

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

    if (aclass == OBJ_WEAPONS)  // Only weapons get brands, of course
    {
        proprt[RAP_BRAND] = SPWPN_FLAMING + random2(15);        // brand

        if (one_chance_in(6))
            proprt[RAP_BRAND] = SPWPN_FLAMING + random2(2);

        if (one_chance_in(6))
            proprt[RAP_BRAND] = SPWPN_ORC_SLAYING + random2(5);

        if (proprt[RAP_BRAND] == SPWPN_DRAGON_SLAYING
            && weapon_skill(item) != SK_POLEARMS)
        {
            proprt[RAP_BRAND] = 0;      // missile wpns
        }

        if (one_chance_in(6))
            proprt[RAP_BRAND] = SPWPN_VORPAL;

        if (proprt[RAP_BRAND] == SPWPN_FLAME
            || proprt[RAP_BRAND] == SPWPN_FROST)
        {
            proprt[RAP_BRAND] = 0;      // missile wpns
        }

        if (proprt[RAP_BRAND] == SPWPN_PROTECTION)
            proprt[RAP_BRAND] = 0;      // no protection

        // if this happens, things might get broken -- bwr
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
                    && (proprt[RAP_BRAND] == SPWPN_VORPAL
                        || proprt[RAP_BRAND] == SPWPN_VENOM))
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

    if (random2(15) >= power_level && aclass != OBJ_WEAPONS &&
        (aclass != OBJ_JEWELLERY || atype != RING_SLAYING))
    {
        // Weapons and rings of slaying can't get these
        if (one_chance_in(4 + power_level))  // to-hit
        {
            proprt[RAP_ACCURACY] = 1 + random2(3) + random2(2);
            power_level++;
            if (one_chance_in(4))
            {
                proprt[RAP_ACCURACY] -= 1 + random2(3) + random2(3) +
                    random2(3);
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

    bool done_powers = (random2(12 < power_level));

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
            || (atype != RING_PROTECTION_FROM_COLD
                && atype != RING_FIRE
                && atype != RING_ICE))
        && (aclass != OBJ_ARMOUR
            || (atype != ARM_DRAGON_ARMOUR
                && atype != ARM_ICE_DRAGON_ARMOUR
                && atype != ARM_GOLD_DRAGON_ARMOUR)))
    {
        proprt[RAP_COLD] = 1;
        if (one_chance_in(5))
            proprt[RAP_COLD]++;
        power_level++;
    }

    if (random2(12) < power_level || power_level > 7)
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
            || atype != ARM_SWAMP_DRAGON_ARMOUR))
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
        && (aclass != OBJ_JEWELLERY || atype != RING_INVISIBILITY))
    {
        proprt[RAP_EYESIGHT] = 1;
        power_level++;
    }

    if (random2(12) < power_level || power_level > 10)
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

    if (power_level >= 2 && random2(17) < power_level)
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
            if (aclass != OBJ_WEAPONS)
                break;
            proprt[RAP_ANGRY] = 1 + random2(8);
            break;
        case 5:                     // susceptible to fire
            if (aclass == OBJ_JEWELLERY
                && (atype == RING_PROTECTION_FROM_FIRE || atype == RING_FIRE
                    || atype == RING_ICE))
                break;              // already does this or something
            if (aclass == OBJ_ARMOUR
                && (atype == ARM_DRAGON_ARMOUR || atype == ARM_ICE_DRAGON_ARMOUR
                    || atype == ARM_GOLD_DRAGON_ARMOUR))
                break;
            proprt[RAP_FIRE] = -1;
            break;
        case 6:                     // susceptible to cold
            if (aclass == OBJ_JEWELLERY
                && (atype == RING_PROTECTION_FROM_COLD || atype == RING_FIRE
                    || atype == RING_ICE))
                break;              // already does this or something
            if (aclass == OBJ_ARMOUR
                && (atype == ARM_DRAGON_ARMOUR || atype == ARM_ICE_DRAGON_ARMOUR
                    || atype == ARM_GOLD_DRAGON_ARMOUR))
                break;
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
        && get_armour_ego_type( item ) != SPARM_STEALTH)
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
        power_level += randart_add_one_property(item, proprt);

    if ((power_level < 2 && one_chance_in(5)) || one_chance_in(30))
        proprt[RAP_CURSED] = 1;
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

    if ( item_ident( item, ISFLAG_KNOW_PROPERTIES ) )
        return;
    else
        known_vec[prop] = (bool) true;
}

bool randart_wpn_known_prop( const item_def &item, randart_prop_type prop )
{
    bool known;
    randart_wpn_property( item, prop, known );
    return known;
}

static std::string get_artefact_type(const int type)
{
    switch (type)
    {
     case OBJ_WEAPONS:
         return "weapon";
     case OBJ_ARMOUR:
         return "armour";
     case OBJ_JEWELLERY:
         return "jewellery";
     default:
         return "artefact";
    }
}

static bool pick_db_name( const item_def &item )
{
    if (is_blessed(item))
        return true;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
        return coinflip();
    case OBJ_JEWELLERY:
        return one_chance_in(5);
    default:
        return false;
    }
}

std::string randart_name( const item_def &item )
{
    ASSERT(is_artefact(item));

    ASSERT(item.base_type == OBJ_WEAPONS
           || item.base_type == OBJ_ARMOUR
           || item.base_type == OBJ_JEWELLERY);

    if (is_unrandom_artefact( item ))
    {
        const unrandart_entry *unrand = seekunrandart( item );
        return (item_type_known(item) ? unrand->name : unrand->unid_name);
    }

    const long seed = calc_seed( item );

    std::string lookup;
    std::string result;

    // use prefix of gifting god, if applicable
    bool god_gift = false;
    int item_orig = 0;
    if (item_type_known(item)) // god prefix not necessary for appearance
    {
        item_orig = item.orig_monnum;
        if (item_orig < 0)
            item_orig = -item_orig - 2;
        else
            item_orig = 0;

        if (item_orig > GOD_NO_GOD && item_orig < NUM_GODS)
        {
            lookup += god_name(static_cast<god_type>(item_orig), false) + " ";
            god_gift = true;
        }
    }

    // get base type
    lookup += get_artefact_type(item.base_type);

    rng_save_excursion rng_state;
    seed_rng( seed );

    if (!item_type_known(item))
    {
        std::string appear = getRandNameString(lookup, " appearance");
        if (appear.empty() // nothing found for lookup
            // don't allow "jewelled jewelled helmet"
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
        return result;
    }

    if (pick_db_name(item))
    {
        result += item_base_name(item) + " ";

        std::string name = getRandNameString(lookup);

        if (name.empty() && god_gift) // if nothing found, try god name alone
        {
            name = getRandNameString(god_name(static_cast<god_type>(item_orig), false));

            if (name.empty()) // if still nothing found, try base type alone
                name = getRandNameString(get_artefact_type(item.base_type).c_str());
        }

        if (name.empty()) // still nothing found?
            result += "of Bugginess";
        else
            result += replace_name_parts(name, item);
    }
    else
    {
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

static unrandart_entry *seekunrandart( const item_def &item )
{
    const int idx = find_unrandart_index(item);
    if ( idx == -1 )
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

// which == 0 (default) gives random fixed artefact.
// Returns true if successful.
bool make_item_fixed_artefact( item_def &item, bool in_abyss, int which )
{
    const bool force = (which != 0);

    if (!force)
    {
        which = SPWPN_START_FIXEDARTS +
            random2(SPWPN_START_NOGEN_FIXEDARTS - SPWPN_START_FIXEDARTS);
    }

    const unique_item_status_type status =
        get_unique_item_status( OBJ_WEAPONS, which );

    if ((status == UNIQ_EXISTS
            || (in_abyss && status == UNIQ_NOT_EXISTS)
            || (!in_abyss && status == UNIQ_LOST_IN_ABYSS))
        && !force)
    {
        return (false);
    }

    switch (which)
    {
    case SPWPN_SINGING_SWORD:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_LONG_SWORD;
        item.plus  = 7;
        item.plus2 = 6;
        break;

    case SPWPN_WRATH_OF_TROG:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_BATTLEAXE;
        item.plus  = 3;
        item.plus2 = 11;
        break;

    case SPWPN_SCYTHE_OF_CURSES:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_SCYTHE;
        item.plus  = 13;
        item.plus2 = 13;
        break;

    case SPWPN_MACE_OF_VARIABILITY:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_MACE;
        item.plus  = random2(16) - 4;
        item.plus2 = random2(16) - 4;
        break;

    case SPWPN_GLAIVE_OF_PRUNE:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_GLAIVE;
        item.plus  = 0;
        item.plus2 = 12;
        break;

    case SPWPN_SCEPTRE_OF_TORMENT:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_MACE;
        item.plus  = 7;
        item.plus2 = 6;
        break;

    case SPWPN_SWORD_OF_ZONGULDROK:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_LONG_SWORD;
        item.plus  = 9;
        item.plus2 = 9;
        break;

    case SPWPN_SWORD_OF_POWER:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_GREAT_SWORD;
        item.plus  = 0; // set on wield
        item.plus2 = 0; // set on wield
        break;

    case SPWPN_KNIFE_OF_ACCURACY:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_DAGGER;
        item.plus  = 27;
        item.plus2 = -1;
        break;

    case SPWPN_STAFF_OF_OLGREB:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_QUARTERSTAFF;
        item.plus  = 0; // set on wield
        item.plus2 = 0; // set on wield
        break;

    case SPWPN_VAMPIRES_TOOTH:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_DAGGER;
        item.plus  = 3;
        item.plus2 = 4;
        break;

    case SPWPN_STAFF_OF_WUCAD_MU:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_QUARTERSTAFF;
        item.plus  = 0; // set on wield
        item.plus2 = 0; // set on wield
        break;

    case SPWPN_SWORD_OF_CEREBOV:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_GREAT_SWORD;
        item.plus  = 6;
        item.plus2 = 6;
        item.colour = YELLOW;
        do_curse_item( item );
        break;

    case SPWPN_STAFF_OF_DISPATER:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_QUARTERSTAFF;
        item.plus  = 4;
        item.plus2 = 4;
        item.colour = YELLOW;
        break;

    case SPWPN_SCEPTRE_OF_ASMODEUS:
        item.base_type = OBJ_WEAPONS;
        item.sub_type = WPN_QUARTERSTAFF;
        item.plus  = 7;
        item.plus2 = 7;
        item.colour = RED;
        break;

    default:
        DEBUGSTR( "Trying to create illegal fixed artefact!" );
        return (false);
    }

    // If we get here, we've made the artefact
    item.special = which;
    item.quantity = 1;

    // Items originally generated in the abyss and not found will be
    // shifted to "lost in abyss", and will only be found there. -- bwr
    set_unique_item_status( OBJ_WEAPONS, which, UNIQ_EXISTS );

    return (true);
}

static bool randart_is_redundant( const item_def &item,
                                  randart_properties_t &proprt )
{
    if (item.base_type != OBJ_JEWELLERY)
        return false;

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
        provides = RAP_CAN_TELEPORT;
        provides = RAP_CAUSE_TELEPORTATION;
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
        return false;

    if (proprt[provides] != 0)
        return true;

    if (provides2 == RAP_NUM_PROPERTIES)
        return false;

    if (proprt[provides2] != 0)
        return true;

    return false;
}

static bool randart_is_conflicting( const item_def &item,
                                    randart_properties_t &proprt )
{
    if (item.base_type != OBJ_JEWELLERY)
        return false;

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
        return false;

    if (proprt[conflicts] != 0)
        return true;

    return false;
}

bool randart_is_bad( const item_def &item, randart_properties_t &proprt )
{
    if (randart_wpn_num_props( proprt ) == 0)
        return true;

    return ( randart_is_redundant( item, proprt ) ||
             randart_is_conflicting( item, proprt ) );
}

bool randart_is_bad( const item_def &item )
{
    randart_properties_t proprt;
    randart_wpn_properties( item, proprt );

    return randart_is_bad( item, proprt);
}

bool make_item_randart( item_def &item )
{
    if (item.base_type != OBJ_WEAPONS
        && item.base_type != OBJ_ARMOUR
        && item.base_type != OBJ_JEWELLERY)
    {
        return (false);
    }

    if (item.flags & ISFLAG_RANDART)
        return (true);

    if (item.flags & ISFLAG_UNRANDART)
        return (false);

    ASSERT(!item.props.exists( KNOWN_PROPS_KEY ));
    item.props[KNOWN_PROPS_KEY].new_vector(SV_BOOL).resize(RA_PROPERTIES);
    CrawlVector &known = item.props[KNOWN_PROPS_KEY];
    known.set_max_size(RA_PROPERTIES);
    for (vec_size i = 0; i < RA_PROPERTIES; i++)
        known[i] = (bool) false;

    item.flags |= ISFLAG_RANDART;

    god_type god_gift = gift_from_god(item);

    do
    {
        item.special = (random_int() & RANDART_SEED_MASK);
    }
    while (randart_is_bad(item)
        || (god_gift != GOD_NO_GOD && !god_fits_artefact(god_gift, item)));

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

    item.base_type = unranddata[unrand_index].ura_cl;
    item.sub_type  = unranddata[unrand_index].ura_ty;
    item.plus      = unranddata[unrand_index].ura_pl;
    item.plus2     = unranddata[unrand_index].ura_pl2;
    item.colour    = unranddata[unrand_index].ura_col;

    item.flags |= ISFLAG_UNRANDART;
    item.special = unranddata[ unrand_index ].prpty[ RAP_BRAND ];

    if (unranddata[ unrand_index ].prpty[ RAP_CURSED ])
        do_curse_item( item );

    set_unrandart_exist( unrand_index, true );

    return (true);
}                               // end make_item_unrandart()

const char *unrandart_descrip( int which_descrip, const item_def &item )
{
/* Eventually it would be great to have randomly generated descriptions for
   randarts. */
    const unrandart_entry *unrand = seekunrandart( item );

    return ((which_descrip == 0) ? unrand->spec_descrip1 :
            (which_descrip == 1) ? unrand->spec_descrip2 :
            (which_descrip == 2) ? unrand->spec_descrip3
                                 : "Unknown.");
}
