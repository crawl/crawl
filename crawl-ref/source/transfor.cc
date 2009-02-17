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

static void _drop_everything();
static void _extra_hp(int amount_extra);

bool transformation_can_wield(transformation_type trans)
{
    return (trans == TRAN_NONE
            || trans == TRAN_STATUE
            || trans == TRAN_LICH);
}

bool transform_allows_wearing_item(const item_def& item)
{
    return (
        transform_allows_wearing_item(item,
                                      static_cast<transformation_type>(
                                          you.attribute[ATTR_TRANSFORMATION])));
}

bool transform_allows_wearing_item(const item_def& item,
                                   transformation_type transform)
{
    bool rc = true;

    if (item.base_type == OBJ_JEWELLERY)
    {
        // Everything but bats can wear all jewellery; bats can
        // only wear amulets.
        if (transform == TRAN_BAT && !jewellery_is_amulet(item))
            rc = false;
    }
    else
    {
        // It's not jewellery, and it's worn, so it must be armour.
        const equipment_type eqslot = get_armour_slot(item);
        const bool is_soft_helmet = (is_helmet(item) && !is_hard_helmet(item));
        switch (transform)
        {
        // Some forms can wear everything.
        case TRAN_NONE:
        case TRAN_LICH:
            rc = true;
            break;

        // Some can't wear anything.
        case TRAN_DRAGON:
        case TRAN_SERPENT_OF_HELL:
        case TRAN_AIR:          // How did you carry it?
            rc = false;
            break;

        // And some need more complicated logic.
        case TRAN_SPIDER:
            rc = is_soft_helmet;
            break;

        case TRAN_BLADE_HANDS:
            rc = (eqslot != EQ_SHIELD && eqslot != EQ_GLOVES);
            break;

        case TRAN_STATUE:
            rc = (eqslot == EQ_CLOAK || eqslot == EQ_HELMET);
            break;

        case TRAN_ICE_BEAST:
            rc = (eqslot == EQ_CLOAK || is_soft_helmet);
            break;

        default:                // Bug-catcher.
            mprf(MSGCH_ERROR, "Unknown transformation type %d in "
                 "transform_allows_wearing_item",
                 you.attribute[ATTR_TRANSFORMATION]);
            break;
        }
    }
    return (rc);
}

static std::set<equipment_type>
_init_equipment_removal(transformation_type trans)
{
    std::set<equipment_type> result;
    if (!transformation_can_wield(trans) && you.weapon())
        result.insert(EQ_WEAPON);

    // Liches can't wield holy weapons.
    if (trans == TRAN_LICH
        && you.weapon()
        && get_weapon_brand(*you.weapon()) == SPWPN_HOLY_WRATH)
    {
        result.insert(EQ_WEAPON);
    }

    for (int i = EQ_WEAPON + 1; i < NUM_EQUIP; ++i)
    {
        const equipment_type eq = static_cast<equipment_type>(i);
        const item_def *pitem = you.slot_item(eq);
        if (pitem && !transform_allows_wearing_item(*pitem, trans))
            result.insert(eq);
    }
    return (result);
}

static void _unwear_equipment_slot(equipment_type eqslot)
{
    const int slot = you.equip[eqslot];
    item_def *item = you.slot_item(eqslot);
    if (item == NULL)
        return;

    if (eqslot == EQ_WEAPON)
    {
        unwield_item();
        canned_msg(MSG_EMPTY_HANDED);
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = slot + 1;
    }
    else if (item->base_type == OBJ_JEWELLERY)
        jewellery_remove_effects(*item, false);
    else
        unwear_armour(slot);
}

static void _remove_equipment(const std::set<equipment_type>& removed,
                              bool meld = true)
{
    // Meld items into you in (reverse) order. (std::set is a sorted container)
    std::set<equipment_type>::const_iterator iter;
    for (iter = removed.begin(); iter != removed.end(); ++iter)
    {
        const equipment_type e = *iter;
        item_def *equip = you.slot_item(e);
        if (equip == NULL)
            continue;

        bool unequip = (e == EQ_WEAPON || !meld);

        mprf("%s %s", equip->name(DESC_CAP_YOUR).c_str(),
             (unequip ? "falls away!" : "melds into your body."));

        _unwear_equipment_slot(e);
        
        if (unequip)
            you.equip[e] = -1;
    }
}

// FIXME: merge this with you_can_wear(), can_wear_armour(), etc.
bool _mutations_prevent_wearing(const item_def& item)
{
    if (item.base_type == OBJ_JEWELLERY)
        return (false);

    const equipment_type eqslot = get_armour_slot(item);

    if (you.mutation[MUT_HORNS] && is_hard_helmet(item))
        return (true);

    if (item.sub_type == ARM_BOOTS // barding excepted!
        && (you.mutation[MUT_HOOVES] || you.mutation[MUT_TALONS]))
    {
        return (true);
    }

    if (eqslot == EQ_GLOVES && you.mutation[MUT_CLAWS] >= 2)
        return (true);

    return (false);

}

static void _rewear_equipment_slot(equipment_type e)
{
    if (e == EQ_WEAPON)         // shouldn't happen
        return;

    if (you.equip[e] == -1)
        return;

    item_def& item = you.inv[you.equip[e]];
    
    if (item.base_type == OBJ_JEWELLERY)
        jewellery_wear_effects(item);
    else
    {
        // In case the player was mutated during the transformation,
        // check whether the equipment is still wearable.
        bool force_remove = _mutations_prevent_wearing(item);

        // If you switched weapons during the transformation, make
        // sure you can still wear your shield.
        // (This is only possible with Statue Form.)
        if (e == EQ_SHIELD && you.weapon()
            && is_shield_incompatible(*you.weapon(), &item))
        {
            force_remove = true;
        }

        if (force_remove)
        {
            mprf("%s is pushed off your body!",
                 item.name(DESC_CAP_YOUR).c_str());
            you.equip[e] = -1;
        }
        else
            armour_wear_effects(you.equip[e]);
    }
}

static void _unmeld_equipment(const std::set<equipment_type>& melded)
{
    // Unmeld items in order.
    std::set<equipment_type>::const_iterator iter;
    for (iter = melded.begin(); iter != melded.end(); ++iter)
    {
        const equipment_type e = *iter;
        if (e == EQ_WEAPON || you.equip[e] == -1)
            continue;

        _rewear_equipment_slot(e);
    }
}

void unmeld_one_equip(equipment_type eq)
{
    std::set<equipment_type> e;
    e.insert(eq);
    _unmeld_equipment(e);
}

void remove_one_equip(equipment_type eq, bool meld)
{
    std::set<equipment_type> r;
    r.insert(eq);
    _remove_equipment(r, meld);
}

static bool _tran_may_meld_cursed(int transformation)
{
    switch (transformation)
    {
    case TRAN_BAT:
        // Vampires of sufficient level may transform into bats even
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
        const equipment_type e = *iter;
        if (you.equip[e] == -1)
            continue;

        const item_def& item = you.inv[ you.equip[e] ];
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

    if (prop_str >= you.strength
        || prop_int >= you.intel
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

        const item_def& item = you.inv[you.equip[e]];

        // Wielding a stat-boosting non-weapon/non-staff won't hinder
        // transformations.
        if (e == EQ_WEAPON
            && item.base_type != OBJ_WEAPONS && item.base_type != OBJ_STAVES)
        {
            continue;
        }

        // Currently, the only nonartefacts which have stat-changing
        // effects are rings.
        if (item.base_type == OBJ_JEWELLERY)
        {
            if (!item_ident(item, ISFLAG_KNOW_PLUSES))
                continue;

            switch (item.sub_type)
            {
            case RING_STRENGTH:     prop_str += item.plus; break;
            case RING_DEXTERITY:    prop_dex += item.plus; break;
            case RING_INTELLIGENCE: prop_int += item.plus; break;
            default:                                       break;
            }
        }

        if (is_random_artefact(item))
        {
            prop_str += randart_known_wpn_property(item, RAP_STRENGTH);
            prop_int += randart_known_wpn_property(item, RAP_INTELLIGENCE);
            prop_dex += randart_known_wpn_property(item, RAP_DEXTERITY);
        }

        // Since there might be multiple items whose effects cancel each other
        // out while worn, if at any point in the order of checking this list
        // (which is the same order as when removing items) one of your stats
        // would reach 0, return true.
        if (prop_str >= you.strength
            || prop_int >= you.intel
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
        && which_trans != TRAN_DRAGON
        && which_trans != TRAN_AIR
        && which_trans != TRAN_BAT)
    {
        // This might be overkill, but it's okay because obviously
        // whatever magical ability that lets them walk on land is
        // removed when they're in water (in this case, their natural
        // form is completely over-riding any other... goes well with
        // the forced transform when entering water)... but merfolk can
        // transform into flying forms.
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

    // Catch some conditions which prevent transformation.
    if (you.is_undead
        && (you.species != SP_VAMPIRE
            || which_trans != TRAN_BAT && you.hunger_state <= HS_SATIATED))
    {
        if (!quiet)
            mpr("Your unliving flesh cannot be transformed in this way.");
        return (false);
    }

    if (which_trans == TRAN_LICH && you.duration[DUR_DEATHS_DOOR])
    {
        if (!quiet)
        {
            mpr("The transformation conflicts with an enchantment "
                "already in effect.");
        }       
        return (false);
    }

    std::set<equipment_type> rem_stuff = _init_equipment_removal(which_trans);
    
    if (_check_for_cursed_equipment(rem_stuff, which_trans, quiet))
        return (false);

    int str = 0, dex = 0, symbol = '@', colour = LIGHTGREY, xhp = 0, dur = 0;
    const char* tran_name = "buggy";
    const char* msg = "You transform into something buggy!";
    switch (which_trans)
    {
    case TRAN_SPIDER:
        tran_name = "spider";
        dex       = 5;
        symbol    = 's';
        colour    = BROWN;
        dur       = std::min(10 + random2(pow) + random2(pow), 60);
        msg       = "You turn into a venomous arachnid creature.";
        break;

    case TRAN_BLADE_HANDS:
        tran_name = "Blade Hands";
        dur       = std::min(10 + random2(pow), 100);
        msg       = "Your hands turn into razor-sharp scythe blades.";
        break;

    case TRAN_STATUE:
        tran_name = "statue";
        str       = 2;
        dex       = -2;
        xhp       = 15;
        symbol    = '8';
        colour    = LIGHTGREY;
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        if (you.species == SP_GNOME && coinflip())
            msg = "Look, a garden gnome.  How cute!";
        else if (player_genus(GENPC_DWARVEN) && one_chance_in(10))
            msg = "You inwardly fear your resemblance to a lawn ornament.";
        else
            msg = "You turn into a living statue of rough stone.";
        break;

    case TRAN_ICE_BEAST:
        tran_name = "ice beast";
        xhp       = 12;
        symbol    = 'I';
        colour    = WHITE;
        dur       = std::min(30 + random2(pow) + random2(pow), 100);
        msg       = "You turn into a creature of crystalline ice.";
        break;

    case TRAN_DRAGON:
        tran_name = "dragon";
        str       = 10;
        xhp       = 16;
        symbol    = 'D';
        colour    = GREEN;
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        if (you.species == SP_MERFOLK && player_is_swimming())
        {
            msg = "You fly out of the water as you turn into "
                "a fearsome dragon!";
        }
        else
            msg = "You turn into a fearsome dragon!";
        break;

    case TRAN_LICH:
        tran_name = "lich";
        str       = 3;
        symbol    = 'L';
        colour    = LIGHTGREY;
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        msg       = "Your body is suffused with negative energy!";
        break;

    case TRAN_SERPENT_OF_HELL:
        tran_name = "Serpent of Hell";
        str       = 13;
        xhp       = 17;
        symbol    = 'S';
        colour    = RED;
        dur       = std::min(20 + random2(pow) + random2(pow), 120);
        msg       = "You transform into a huge demonic serpent!";
        break;

    case TRAN_AIR:
        tran_name = "air";
        dex       = 8;
        symbol    = '#';
        colour    = DARKGREY;
        dur       = std::min(35 + random2(pow) + random2(pow), 150);
        msg       = "You feel diffuse...";
        break;

    case TRAN_BAT:
        tran_name = "bat";
        str       = -5;
        dex       = 5;
        symbol    = 'b';
        colour    = (you.species == SP_VAMPIRE ? DARKGREY : LIGHTGREY);
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        if (you.species == SP_VAMPIRE)
            msg = "You turn into a vampire bat.";
        else
            msg = "You turn into a bat.";
        break;

    case TRAN_NONE:
    case NUM_TRANSFORMATIONS:
        break;
    }

    if (check_transformation_stat_loss(rem_stuff, quiet,
                                       std::max(-str, 0), std::max(-dex,0)))
    {
        return (false);
    }

    // All checks done, transformation will take place now.
    you.redraw_evasion = true;
    you.redraw_armour_class = true;
    you.wield_change = true;

    // Most transformations conflict with stone skin.
    if (which_trans != TRAN_NONE
        && which_trans != TRAN_BLADE_HANDS
        && which_trans != TRAN_STATUE)
    {
        you.duration[DUR_STONESKIN] = 0;
    }

    // Give the transformation message.
    mpr(msg);

    _remove_equipment(rem_stuff);

    // Update your status.
    you.attribute[ATTR_TRANSFORMATION] = which_trans;
    you.duration[DUR_TRANSFORMATION]   = dur;
    you.symbol = symbol;
    you.colour = colour;

    if (str)
    {
        modify_stat(STAT_STRENGTH, str, true,
                    make_stringf("gaining the %s transformation",
                                 tran_name).c_str());
    }

    if (dex)
    {
        modify_stat(STAT_DEXTERITY, dex, true,
                    make_stringf("gaining the %s transformation",
                                 tran_name).c_str());
    }

    if (xhp)
        _extra_hp(xhp);

    // Extra effects
    switch (which_trans)
    {
    case TRAN_STATUE:
        if (you.duration[DUR_STONEMAIL] || you.duration[DUR_STONESKIN])
            mpr("Your new body merges with your stone armour.");
        break;

    case TRAN_ICE_BEAST:
        if (you.duration[DUR_ICY_ARMOUR])
            mpr("Your new body merges with your icy armour.");
        break;

    case TRAN_DRAGON:
        if (you.attribute[ATTR_HELD])
        {
            mpr("The net rips apart!");
            you.attribute[ATTR_HELD] = 0;
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                destroy_item(net);
        }
        break;

    case TRAN_LICH:
        // undead cannot regenerate -- bwr
        if (you.duration[DUR_REGENERATION])
        {
            mpr("You stop regenerating.", MSGCH_DURATION);
            you.duration[DUR_REGENERATION] = 0;
        }

        // silently removed since undead automatically resist poison -- bwr
        you.duration[DUR_RESIST_POISON] = 0;

        you.is_undead = US_UNDEAD;
        you.hunger_state = HS_SATIATED;  // no hunger effects while transformed
        set_redraw_status(REDRAW_HUNGER);
        break;

    case TRAN_AIR:
        _drop_everything();

        if (you.attribute[ATTR_HELD])
        {
            mpr("You drift through the net!");
            you.attribute[ATTR_HELD] = 0;
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                remove_item_stationary(mitm[net]);
        }
        break;

    default:
        break;
    }

    if (you.species != SP_VAMPIRE || which_trans != TRAN_BAT)
        _transformation_expiration_warning();

    return (true);
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
    std::set<equipment_type> melded = _init_equipment_removal(old_form);

    you.attribute[ATTR_TRANSFORMATION] = TRAN_NONE;
    you.duration[DUR_TRANSFORMATION] = 0;

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

void _extra_hp(int amount_extra) // must also set in calc_hp
{
    calc_hp();

    you.hp *= amount_extra;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);
}

void _drop_everything()
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
