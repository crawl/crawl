/*
 *  File:       transfor.cc
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "transfor.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "delay.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "misc.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "skills2.h"
#include "stuff.h"
#include "traps.h"

void drop_everything(void);
void extra_hp(int amount_extra);

static void _init_equipment_removal(std::set<equipment_type> &rem_stuff,
                                    int which_trans)
{
    switch (which_trans)
    {
    case TRAN_SPIDER:
        // Spiders CAN wear soft helmets
        if (you.equip[EQ_HELMET] == -1
            || !is_hard_helmet(you.inv[you.equip[EQ_HELMET]]))
        {
            rem_stuff.erase(EQ_HELMET);
        }
        break;

    case TRAN_BAT:
        // Bats can't wear rings, either. This means that the only equipment
        // the player may keep wearing upon transformation is an amulet.
        rem_stuff.insert(EQ_LEFT_RING);
        rem_stuff.insert(EQ_RIGHT_RING);
        break;

    case TRAN_ICE_BEAST:
        rem_stuff.erase(EQ_CLOAK);
        // Ice beasts CAN wear soft helmets.
        if (you.equip[EQ_HELMET] == -1
            || !is_hard_helmet(you.inv[you.equip[EQ_HELMET]]))
        {
            rem_stuff.erase(EQ_HELMET);
        }
        break;

    case TRAN_LICH:
        // Liches may wear anything.
        rem_stuff.clear();
        break;

    case TRAN_BLADE_HANDS:
        rem_stuff.erase(EQ_CLOAK);
        rem_stuff.erase(EQ_HELMET);
        rem_stuff.erase(EQ_BOOTS);
        rem_stuff.erase(EQ_BODY_ARMOUR);
        break;

    case TRAN_STATUE:
        rem_stuff.erase(EQ_WEAPON); // can still hold a weapon
        rem_stuff.erase(EQ_CLOAK);
        rem_stuff.erase(EQ_HELMET);
        break;

    case TRAN_AIR:
        // Can't wear anything at all!
        rem_stuff.insert(EQ_LEFT_RING);
        rem_stuff.insert(EQ_RIGHT_RING);
        rem_stuff.insert(EQ_AMULET);
        break;
    default:
        break;
    }
}

static bool _remove_equipment(const std::set<equipment_type>& removed,
                              bool meld = true)
{
    // Weapons first.
    if (removed.find(EQ_WEAPON) != removed.end() && you.weapon())
    {
        const int wpn = you.equip[EQ_WEAPON];
        unwield_item();
        canned_msg(MSG_EMPTY_HANDED);
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = wpn + 1;
    }

    // Meld items into you in (reverse) order. (std::set is a sorted container)
    std::set<equipment_type>::const_iterator iter;
    for (iter = removed.begin(); iter != removed.end(); ++iter)
    {
        const equipment_type e = *iter;
        if (e == EQ_WEAPON || you.equip[e] == -1)
            continue;

        item_def& equip = you.inv[you.equip[e]];

        if (meld)
            mprf("%s melds into your body.", equip.name(DESC_CAP_YOUR).c_str());

        // Note that your weapon is handled specially, so base_type
        // tells us what we need to know.
        if (equip.base_type == OBJ_JEWELLERY)
            jewellery_remove_effects(equip, false);
        else // armour
            unwear_armour( you.equip[e] );

        if (!meld)
        {
            mprf("%s falls away!", equip.name(DESC_CAP_YOUR).c_str());
            you.equip[e] = -1;
        }
    }

    return (true);
}

static bool _unmeld_equipment(const std::set<equipment_type>& melded)
{
    // Unmeld items in order.
    std::set<equipment_type>::const_iterator iter;
    for (iter = melded.begin(); iter != melded.end(); ++iter)
    {
        const equipment_type e = *iter;
        if (e == EQ_WEAPON || you.equip[e] == -1)
            continue;

        item_def& equip = you.inv[you.equip[e]];

        if (equip.base_type == OBJ_JEWELLERY)
            jewellery_wear_effects(equip);
        else // armour
        {
            bool force_remove = false;

            // In case the player was mutated during the transformation,
            // check whether the equipment is still wearable.
            switch (e)
            {
            case EQ_HELMET:
                if (you.mutation[MUT_HORNS] && is_hard_helmet(equip))
                    force_remove = true;
                break;

            case EQ_GLOVES:
                if (you.mutation[MUT_CLAWS] >= 2)
                    force_remove = true;
                break;

            case EQ_BOOTS:
                if (equip.sub_type == ARM_BOOTS // i.e. not barding
                    && (you.mutation[MUT_HOOVES] || you.mutation[MUT_TALONS]))
                {
                    force_remove = true;
                }
                break;

            case EQ_SHIELD:
                // If you switched weapons during the transformation, make
                // sure you can still wear your shield.
                // (This is only possible with Statue Form.)
                if (you.weapon()
                    && is_shield_incompatible(*you.weapon(), &equip))
                {
                    force_remove = true;
                }
                break;

            default:
                break;
            }

            if (force_remove)
            {
                mprf("%s is pushed off your body!",
                     equip.name(DESC_CAP_YOUR).c_str());
                you.equip[e] = -1;
            }
            else
                armour_wear_effects(you.equip[e]);
        }
    }

    return (true);
}

bool unmeld_one_equip(equipment_type eq)
{
    std::set<equipment_type> e;
    e.insert(eq);
    return _unmeld_equipment(e);
}

bool remove_one_equip(equipment_type eq, bool meld)
{
    std::set<equipment_type> r;
    r.insert(eq);
    return _remove_equipment(r, meld);
}

static bool _tran_may_meld_cursed(int transformation)
{
    switch (transformation)
    {
    case TRAN_BAT:
        // Vampires of certain Xp may transform into bats even
        // with cursed gear.
        if (you.species == SP_VAMPIRE && you.experience_level >= 10)
            return (true);
        // intentional fall-through
    case TRAN_SPIDER:
    case TRAN_AIR:
        return (false);
    default:
        return (true);
    }
}

// Returns true if any piece of equipment that has to be removed is cursed.
// Useful for keeping low level transformations from being too useful.
static bool _check_for_cursed_equipment(const std::set<equipment_type> &remove,
                                        const int trans, bool quiet = false)
{
    std::set<equipment_type>::const_iterator iter;
    for (iter = remove.begin(); iter != remove.end(); ++iter)
    {
        equipment_type e = *iter;
        if (you.equip[e] == -1)
            continue;

        const item_def item = you.inv[ you.equip[e] ];
        if (item_cursed(item))
        {
            if (e != EQ_WEAPON && _tran_may_meld_cursed(trans))
                continue;

            // Wielding a cursed non-weapon/non-staff won't hinder
            // transformations.
            if (e == EQ_WEAPON && item.base_type != OBJ_WEAPONS
                && item.base_type != OBJ_STAVES)
            {
                continue;
            }

            if (!quiet)
            {
                mpr( "Your cursed equipment won't allow you to complete the "
                     "transformation." );
            }

            return (true);
        }
    }

    return (false);
}

// Count the stat boosts yielded by all items to be removed, and count
// future losses (caused by the transformation) like a current stat boost,
// as well. If the sum of all boosts of a stat is equal to or greater than
// the current stat, give a message and return true.
bool check_transformation_stat_loss(const std::set<equipment_type> &remove,
                                    bool quiet, int str_loss, int dex_loss,
                                    int int_loss)
{
    // Initialize with additional losses, if any.
    int prop_str = str_loss;
    int prop_dex = dex_loss;
    int prop_int = int_loss;

    // Might is very much temporary and might run out at any point during
    // your transformation, possibly resulting in stat loss caused by a
    // combination of an unequipping (and/or stat lowering) transformation
    // and Might running out at an inopportune moment.
    if (you.duration[DUR_MIGHT])
        prop_str += 5;

    if (prop_str >= you.strength || prop_int >= you.intel
        || prop_dex >= you.dex)
    {
        if (!quiet)
        {
            mpr("This transformation would result in fatal stat loss!",
                MSGCH_WARN);
        }
        return (true);
    }

    // Check over all items to be removed.
    std::set<equipment_type>::const_iterator iter;
    for (iter = remove.begin(); iter != remove.end(); ++iter)
    {
        equipment_type e = *iter;
        if (you.equip[e] == -1)
            continue;

        // Wielding a stat-boosting non-weapon/non-staff won't hinder
        // transformations.
        if (e == EQ_WEAPON)
        {
            const item_def item = you.inv[ you.equip[e] ];
            if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_STAVES)
                continue;
        }

        item_def item = you.inv[you.equip[e]];
        if (item.base_type == OBJ_JEWELLERY)
        {
            if (!item_ident( item, ISFLAG_KNOW_PLUSES ))
                continue;

            switch (item.sub_type)
            {
            case RING_STRENGTH:
                if (item.plus != 0)
                    prop_str += item.plus;
                break;
            case RING_DEXTERITY:
                if (item.plus != 0)
                    prop_dex += item.plus;
                break;
            case RING_INTELLIGENCE:
                if (item.plus != 0)
                    prop_int += item.plus;
                break;
            default:
                break;
            }
        }

        if (is_random_artefact( item ))
        {
            prop_str += randart_known_wpn_property(item, RAP_STRENGTH);
            prop_int += randart_known_wpn_property(item, RAP_INTELLIGENCE);
            prop_dex += randart_known_wpn_property(item, RAP_DEXTERITY);
        }

        // Since there might be multiple items whose effects cancel each other
        // out while worn, if at any point in the order of checking this list
        // (which is the same order as when removing items) one of your stats
        // would reach 0, return true.
        if (prop_str >= you.strength || prop_int >= you.intel
            || prop_dex >= you.dex)
        {
            if (!quiet)
            {
                mpr("This transformation would result in fatal stat loss!",
                    MSGCH_WARN);
            }
            return (true);
        }
    }

    return (false);
}

// FIXME: Switch to 4.1 transforms handling.
size_type transform_size(int psize)
{
    return you.transform_size(psize);
}

size_type player::transform_size(int psize) const
{
    const int transform = attribute[ATTR_TRANSFORMATION];
    switch (transform)
    {
    case TRAN_SPIDER:
    case TRAN_BAT:
        return SIZE_TINY;
    case TRAN_ICE_BEAST:
        return SIZE_LARGE;
    case TRAN_DRAGON:
    case TRAN_SERPENT_OF_HELL:
        return SIZE_HUGE;
    case TRAN_AIR:
        return SIZE_MEDIUM;
    default:
        return SIZE_CHARACTER;
    }
}

static void _transformation_expiration_warning()
{
    if (you.duration[DUR_TRANSFORMATION]
            <= get_expiration_threshold(DUR_TRANSFORMATION))
    {
        mpr("You have a feeling this form won't last long.");
    }
}

// Transforms you into the specified form. If quiet is true, fails silently
// (if it fails).
bool transform(int pow, transformation_type which_trans, bool quiet)
{
    if (you.species == SP_MERFOLK && player_is_swimming()
        && which_trans != TRAN_DRAGON)
    {
        // This might be overkill, but it's okay because obviously
        // whatever magical ability that lets them walk on land is
        // removed when they're in water (in this case, their natural
        // form is completely over-riding any other... goes well with
        // the forced transform when entering water)... but merfolk can
        // transform into dragons, because dragons fly. -- bwr
        if (!quiet)
            mpr("You cannot transform out of your normal form while in water.");
        return (false);
    }

    // This must occur before the untransform() and the is_undead check.
    if (you.attribute[ATTR_TRANSFORMATION]
        == static_cast<unsigned>(which_trans))
    {
        if (you.duration[DUR_TRANSFORMATION] < 100)
        {
            mpr("You extend your transformation's duration.");
            you.duration[DUR_TRANSFORMATION] += random2(pow);

            if (you.duration[DUR_TRANSFORMATION] > 100)
                you.duration[DUR_TRANSFORMATION] = 100;

            return (true);
        }
        else
        {
            if (!quiet)
                mpr("You cannot extend your transformation any further!");
            return (false);
        }
    }

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
        untransform();

    if (you.is_undead
        && (you.species != SP_VAMPIRE
            || which_trans != TRAN_BAT && you.hunger_state <= HS_SATIATED))
    {
        if (!quiet)
            mpr("Your unliving flesh cannot be transformed in this way.");
        return (false);
    }

    // Most transformations conflict with stone skin.
    if (which_trans != TRAN_NONE
        && which_trans != TRAN_BLADE_HANDS
        && which_trans != TRAN_STATUE)
    {
        you.duration[DUR_STONESKIN] = 0;
    }

    // We drop everything except jewellery by default.
    const equipment_type default_rem[] = {
        EQ_WEAPON, EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS,
        EQ_SHIELD, EQ_BODY_ARMOUR
    };

    std::set<equipment_type> rem_stuff(default_rem,
                                       default_rem + ARRAYSZ(default_rem));
    _init_equipment_removal(rem_stuff, which_trans);

    you.redraw_evasion = true;
    you.redraw_armour_class = true;
    you.wield_change = true;

    // Remember, it can still fail in the switch below...
    switch (which_trans)
    {
    case TRAN_SPIDER:           // also AC +3, ev +3, fast_run
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        // Check in case we'll auto-remove stat boosting equipment.
        if (check_transformation_stat_loss(rem_stuff, quiet))
            return (false);

        mpr("You turn into a venomous arachnid creature.");
        _remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_SPIDER;
        you.duration[DUR_TRANSFORMATION] = 10 + random2(pow) + random2(pow);

        if (you.duration[DUR_TRANSFORMATION] > 60)
            you.duration[DUR_TRANSFORMATION] = 60;

        modify_stat( STAT_DEXTERITY, 5, true,
                     "gaining the spider transformation");

        you.symbol = 's';
        you.colour = BROWN;

        _transformation_expiration_warning();
        return (true);

    case TRAN_BAT:
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        if (check_transformation_stat_loss(rem_stuff, quiet, 5)) // Str loss = 5
            return (false);

        mprf("You turn into a %sbat.",
             you.species == SP_VAMPIRE ? "vampire " : "");

        _remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_BAT;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[DUR_TRANSFORMATION] > 100)
            you.duration[DUR_TRANSFORMATION] = 100;

        // high ev, low ac, high speed
       modify_stat( STAT_DEXTERITY, 5, true,
                    "gaining the bat transformation");
       modify_stat( STAT_STRENGTH, -5, true,
                    "gaining the bat transformation" );

       you.symbol = 'b';
       you.colour = (you.species == SP_VAMPIRE ? DARKGREY : LIGHTGREY);

        if (you.species != SP_VAMPIRE)
            _transformation_expiration_warning();
        return (true);

    case TRAN_ICE_BEAST:  // also AC +3, cold +3, fire -1, pois +1
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        // Check in case we'll auto-remove stat boosting equipment.
        if (check_transformation_stat_loss(rem_stuff, quiet))
            return (false);

        mpr("You turn into a creature of crystalline ice.");

        _remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_ICE_BEAST;
        you.duration[DUR_TRANSFORMATION] = 30 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        extra_hp(12);   // must occur after attribute set

        if (you.duration[DUR_ICY_ARMOUR])
            mpr("Your new body merges with your icy armour.");

        you.symbol = 'I';
        you.colour = WHITE;

        _transformation_expiration_warning();
        return (true);

    case TRAN_BLADE_HANDS:
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        // Check in case we'll auto-remove stat boosting equipment.
        if (check_transformation_stat_loss(rem_stuff, quiet))
            return (false);

        mpr("Your hands turn into razor-sharp scythe blades.");
        _remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_BLADE_HANDS;
        you.duration[DUR_TRANSFORMATION] = 10 + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        _transformation_expiration_warning();
        return (true);

    case TRAN_STATUE: // also AC +20, ev -5, elec +1, pois +1, neg +1, slow
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        // Lose 2 dex
        if (check_transformation_stat_loss(rem_stuff, quiet, 0, 2))
            return (false);

        if (you.species == SP_GNOME && coinflip())
            mpr("Look, a garden gnome.  How cute!");
        else if (player_genus(GENPC_DWARVEN) && one_chance_in(10))
            mpr("You inwardly fear your resemblance to a lawn ornament.");
        else
            mpr("You turn into a living statue of rough stone.");

        // Too stiff to make use of shields, gloves, or armour -- bwr
        _remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_STATUE;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_DEXTERITY, -2, true,
                     "gaining the statue transformation" );
        modify_stat( STAT_STRENGTH, 2, true,
                     "gaining the statue transformation");
        extra_hp(15);   // must occur after attribute set

        if (you.duration[DUR_STONEMAIL] || you.duration[DUR_STONESKIN])
            mpr("Your new body merges with your stone armour.");

        you.symbol = '8';
        you.colour = LIGHTGREY;

        _transformation_expiration_warning();
        return (true);

    case TRAN_DRAGON:  // also AC +10, ev -3, cold -1, fire +2, pois +1, flight
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        // Check in case we'll auto-remove stat boosting equipment.
        if (check_transformation_stat_loss(rem_stuff, quiet))
            return (false);

        if (you.species == SP_MERFOLK && player_is_swimming())
        {
            mpr("You fly out of the water as you turn into "
                "a fearsome dragon!");
        }
        else
            mpr("You turn into a fearsome dragon!");

        _remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_DRAGON;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_STRENGTH, 10, true,
                     "gaining the dragon transformation" );
        extra_hp(16);   // must occur after attribute set

        you.symbol = 'D';
        you.colour = GREEN;

        if (you.attribute[ATTR_HELD])
        {
            mpr("The net rips apart!");
            you.attribute[ATTR_HELD] = 0;
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                destroy_item(net);
        }

        _transformation_expiration_warning();
        return (true);

    case TRAN_LICH:
        // AC +3, cold +1, neg +3, pois +1, is_undead, res magic +50,
        // spec_death +1, and drain attack (if empty-handed)

        if (you.duration[DUR_DEATHS_DOOR])
        {
            if (!quiet)
            {
                mpr( "The transformation conflicts with an enchantment "
                     "already in effect." );
            }

            return (false);
        }

        // Remove holy wrath weapons if necessary.
        if (you.weapon()
            && get_weapon_brand(*you.weapon()) == SPWPN_HOLY_WRATH)
        {
            rem_stuff.insert(EQ_WEAPON);
        }

        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        if (check_transformation_stat_loss(rem_stuff, quiet))
            return (false);

        mpr("Your body is suffused with negative energy!");

        _remove_equipment(rem_stuff);

        // undead cannot regenerate -- bwr
        if (you.duration[DUR_REGENERATION])
        {
            mpr("You stop regenerating.", MSGCH_DURATION);
            you.duration[DUR_REGENERATION] = 0;
        }

        // silently removed since undead automatically resist poison -- bwr
        you.duration[DUR_RESIST_POISON] = 0;

        you.attribute[ATTR_TRANSFORMATION] = TRAN_LICH;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_STRENGTH, 3, true,
                     "gaining the lich transformation" );
        you.symbol = 'L';
        you.colour = LIGHTGREY;
        you.is_undead = US_UNDEAD;
        you.hunger_state = HS_SATIATED;  // no hunger effects while transformed
        set_redraw_status( REDRAW_HUNGER );

        _transformation_expiration_warning();
        return (true);

    case TRAN_AIR:
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        // Check in case we'll auto-remove stat boosting equipment.
        if (check_transformation_stat_loss(rem_stuff, quiet))
            return (false);

        // also AC 20, ev +20, regen/2, no hunger, fire -2, cold -2, air +2,
        // pois +1, spec_earth -1
        mpr("You feel diffuse...");

        _remove_equipment(rem_stuff);

        drop_everything();

        you.attribute[ATTR_TRANSFORMATION] = TRAN_AIR;
        you.duration[DUR_TRANSFORMATION] = 35 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 150)
            you.duration[ DUR_TRANSFORMATION ] = 150;

        modify_stat( STAT_DEXTERITY, 8, true,
                     "gaining the air transformation" );
        you.symbol = '#';
        you.colour = DARKGREY;

        if (you.attribute[ATTR_HELD])
        {
            mpr("You drift through the net!");
            you.attribute[ATTR_HELD] = 0;
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                remove_item_stationary(mitm[net]);
        }

        _transformation_expiration_warning();
        return (true);

    case TRAN_SERPENT_OF_HELL:
        if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
            return (false);

        // Check in case we'll auto-remove stat boosting equipment.
        if (check_transformation_stat_loss(rem_stuff, quiet))
            return (false);

        // also AC +10, ev -5, fire +2, pois +1, life +2, slow
        mpr("You transform into a huge demonic serpent!");

        _remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_SERPENT_OF_HELL;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 120)
            you.duration[ DUR_TRANSFORMATION ] = 120;

        modify_stat( STAT_STRENGTH, 13, true,
                     "gaining the Serpent of Hell transformation");
        extra_hp(17);   // must occur after attribute set

        you.symbol = 'S';
        you.colour = RED;

        _transformation_expiration_warning();
        return (true);

    case TRAN_NONE:
    case NUM_TRANSFORMATIONS:
        break;
    }

    return (false);
}

bool transform_can_butcher_barehanded(transformation_type tt)
{
    return (tt == TRAN_BLADE_HANDS || tt == TRAN_DRAGON
            || tt == TRAN_SERPENT_OF_HELL);
}

void untransform(void)
{
    const flight_type old_flight = you.flight_mode();

    you.redraw_evasion = true;
    you.redraw_armour_class = true;
    you.wield_change = true;

    you.symbol = '@';
    you.colour = LIGHTGREY;

    // Must be unset first or else infinite loops might result. -- bwr
    const transformation_type old_form =
        static_cast<transformation_type>(you.attribute[ ATTR_TRANSFORMATION ]);

    // We may have to unmeld a couple of equipment types.
    const equipment_type default_rem[] = {
        EQ_CLOAK, EQ_HELMET, EQ_GLOVES, EQ_BOOTS, EQ_SHIELD, EQ_BODY_ARMOUR
    };

    std::set<equipment_type> melded(default_rem,
                                    default_rem + ARRAYSZ(default_rem));
    _init_equipment_removal(melded, old_form);

    you.attribute[ ATTR_TRANSFORMATION ] = TRAN_NONE;
    you.duration[ DUR_TRANSFORMATION ] = 0;

    int hp_downscale = 10;

    switch (old_form)
    {
    case TRAN_SPIDER:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        modify_stat( STAT_DEXTERITY, -5, true,
                     "losing the spider transformation" );
        break;

    case TRAN_BAT:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        modify_stat( STAT_DEXTERITY, -5, true,
                     "losing the bat transformation" );
        modify_stat( STAT_STRENGTH, 5, true,
                     "losing the bat transformation" );
        break;

    case TRAN_BLADE_HANDS:
        mpr( "Your hands revert to their normal proportions.", MSGCH_DURATION );
        you.wield_change = true;
        break;

    case TRAN_STATUE:
        mpr( "You revert to your normal fleshy form.", MSGCH_DURATION );
        modify_stat( STAT_DEXTERITY, 2, true,
                     "losing the statue transformation" );
        modify_stat( STAT_STRENGTH, -2, true,
                     "losing the statue transformation");

        // Note: if the core goes down, the combined effect soon disappears,
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_STONEMAIL])
            you.duration[DUR_STONEMAIL] = 1;

        if (you.duration[DUR_STONESKIN])
            you.duration[DUR_STONESKIN] = 1;

        hp_downscale = 15;

        break;

    case TRAN_ICE_BEAST:
        mpr( "You warm up again.", MSGCH_DURATION );

        // Note: if the core goes down, the combined effect soon disappears,
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_ICY_ARMOUR])
            you.duration[DUR_ICY_ARMOUR] = 1;

        hp_downscale = 12;

        break;

    case TRAN_DRAGON:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -10, true,
                    "losing the dragon transformation" );
        hp_downscale = 16;
        break;

    case TRAN_LICH:
        mpr( "You feel yourself come back to life.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -3, true,
                    "losing the lich transformation" );
        you.is_undead = US_ALIVE;
        break;

    case TRAN_AIR:
        mpr( "Your body solidifies.", MSGCH_DURATION );
        modify_stat(STAT_DEXTERITY, -8, true,
                    "losing the air transformation");
        break;

    case TRAN_SERPENT_OF_HELL:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -13, true,
                    "losing the Serpent of Hell transformation");
        hp_downscale = 17;
        break;

    default:
        break;
    }

    _unmeld_equipment(melded);

    // Re-check terrain now that be may no longer be flying.
    if (old_flight && you.flight_mode() == FL_NONE)
        move_player_to_grid(you.pos(), false, true, true);

    if (transform_can_butcher_barehanded(old_form))
        stop_butcher_delay();

    // If nagas wear boots while transformed, they fall off again afterwards:
    // I don't believe this is currently possible, and if it is we
    // probably need something better to cover all possibilities.  -bwr

    // Removed barding check, no transformed creatures can wear barding
    // anyway.
    // *coughs* Ahem, blade hands... -- jpeg
    if (you.species == SP_NAGA || you.species == SP_CENTAUR)
    {
        const int arm = you.equip[EQ_BOOTS];

        if (arm != -1 && you.inv[arm].sub_type == ARM_BOOTS)
            remove_one_equip(EQ_BOOTS);
    }

    if (hp_downscale != 10 && you.hp != you.hp_max)
    {
        you.hp = you.hp * 10 / hp_downscale;
        if (you.hp < 1)
            you.hp = 1;
        else if (you.hp > you.hp_max)
            you.hp = you.hp_max;
    }
    calc_hp();

    handle_interrupted_swap(true, false, true);
}

// XXX: This whole system is a mess as it still relies on special
// cases to handle a large number of things (see wear_armour()) -- bwr
bool can_equip( equipment_type use_which, bool ignore_temporary )
{
    if (use_which == EQ_HELMET
        && (player_mutation_level(MUT_HORNS)
            || player_mutation_level(MUT_BEAK)))
    {
        return (false);
    }

    if (use_which == EQ_BOOTS && !player_has_feet())
        return (false);

    if (use_which == EQ_GLOVES && you.has_claws(false) >= 3)
        return (false);

    if (!ignore_temporary)
    {
        switch (you.attribute[ATTR_TRANSFORMATION])
        {
        case TRAN_NONE:
        case TRAN_LICH:
            return (true);

        case TRAN_BLADE_HANDS:
            return (use_which != EQ_WEAPON
                    && use_which != EQ_GLOVES
                    && use_which != EQ_SHIELD);

        case TRAN_STATUE:
            return (use_which == EQ_WEAPON
                    || use_which == EQ_CLOAK
                    || use_which == EQ_HELMET);

        case TRAN_ICE_BEAST:
            return (use_which == EQ_CLOAK);

        default:
            return (false);
        }
    }

    return (true);
}

// raw comparison of an item, must use check_armour_shape for full version
bool transform_can_equip_type( int eq_slot )
{
    // FIXME FIXME FIXME
    return (false);

    // const int form = you.attribute[ATTR_TRANSFORMATION];
    // return (!must_remove( Trans[form].rem_stuff, eq_slot ));
}

void extra_hp(int amount_extra) // must also set in calc_hp
{
    calc_hp();

    you.hp *= amount_extra;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);
}

void drop_everything(void)
{
    if (inv_count() < 1)
        return;

    mpr( "You find yourself unable to carry your possessions!" );

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item( you.inv[i] ))
        {
            copy_item_to_grid( you.inv[i], you.pos() );
            you.inv[i].quantity = 0;
        }
    }
}

// Used to mark transformations which override species/mutation intrinsics.
// If phys_scales is true then we're checking to see if the form keeps
// the physical (AC/EV) properties from scales... the special intrinsic
// features (resistances, etc.) are lost in those forms however.
bool transform_changed_physiology( bool phys_scales )
{
    return (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE
            && you.attribute[ATTR_TRANSFORMATION] != TRAN_BLADE_HANDS
            && (!phys_scales
                || (you.attribute[ATTR_TRANSFORMATION] != TRAN_LICH
                    && you.attribute[ATTR_TRANSFORMATION] != TRAN_STATUE)));
}
