/*
 *  File:       transform.cc
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "transform.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "artefact.h"
#include "delay.h"
#include "env.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "player-equip.h"
#include "player-stats.h"
#include "random.h"
#include "skills2.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "xom.h"

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
        // Everything but bats can wear all jewellery; bats and pigs can
        // only wear amulets.
        if ((transform == TRAN_BAT || transform == TRAN_PIG)
             && !jewellery_is_amulet(item))
        {
            rc = false;
        }
    }
    else
    {
        // It's not jewellery, and it's worn, so it must be armour.
        const equipment_type eqslot = get_armour_slot(item);
        const bool is_soft_helmet   = is_helmet(item) && !is_hard_helmet(item);

        switch (transform)
        {
        // Some forms can wear everything.
        case TRAN_NONE:
        case TRAN_LICH:
            rc = true;
            break;

        // Some can't wear anything.
        case TRAN_DRAGON:
        case TRAN_BAT:
        case TRAN_PIG:
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
        const item_def *pitem = you.slot_item(eq, true);
        if (pitem && !transform_allows_wearing_item(*pitem, trans))
            result.insert(eq);
    }
    return (result);
}

static void _remove_equipment(const std::set<equipment_type>& removed,
                              bool meld = true, bool mutation = false)
{
    // Meld items into you in (reverse) order. (std::set is a sorted container)
    std::set<equipment_type>::const_iterator iter;
    for (iter = removed.begin(); iter != removed.end(); ++iter)
    {
        const equipment_type e = *iter;
        item_def *equip = you.slot_item(e, true);
        if (equip == NULL)
            continue;

        bool unequip = (e == EQ_WEAPON || !meld);

        mprf("%s %s%s %s", equip->name(DESC_CAP_YOUR).c_str(),
             unequip ? "fall" : "meld",
             equip->quantity > 1 ? "" : "s",
             unequip ? "away!" : "into your body.");

        if (unequip)
        {
            if (e == EQ_WEAPON)
            {
                const int slot = you.equip[EQ_WEAPON];
                unwield_item(!you.berserk());
                canned_msg(MSG_EMPTY_HANDED);
                you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = slot + 1;
            }
            else
                unequip_item(e);

            if (mutation)
            {
                // A mutation made us not only lose an equipment slot
                // but actually removed a worn item: Funny!
                xom_is_stimulated(is_artefact(*equip) ? 255 : 128);
            }
        }
        else
            meld_slot(e);
    }
}

// FIXME: merge this with you_can_wear(), can_wear_armour(), etc.
static bool _mutations_prevent_wearing(const item_def& item)
{
    const equipment_type eqslot = get_armour_slot(item);

    if (is_hard_helmet(item)
        && (player_mutation_level(MUT_HORNS)
            || player_mutation_level(MUT_BEAK)))
    {
        return (true);
    }

    // Barding is excepted here.
    if (item.sub_type == ARM_BOOTS
        && (player_mutation_level(MUT_HOOVES) >= 3
            || player_mutation_level(MUT_TALONS) >= 3))
    {
        return (true);
    }

    if (eqslot == EQ_GLOVES && player_mutation_level(MUT_CLAWS) >= 3)
        return (true);

    return (false);
}

static void _unmeld_equipment_slot(equipment_type e)
{
    item_def& item = you.inv[you.equip[e]];

    if (item.base_type == OBJ_JEWELLERY)
        unmeld_slot(e);
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
            unequip_item(e);
        }
        else
            unmeld_slot(e);
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

        _unmeld_equipment_slot(e);
    }
}

void unmeld_one_equip(equipment_type eq)
{
    std::set<equipment_type> e;
    e.insert(eq);
    _unmeld_equipment(e);
}

void remove_one_equip(equipment_type eq, bool meld, bool mutation)
{
    std::set<equipment_type> r;
    r.insert(eq);
    _remove_equipment(r, meld, mutation);
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
        if (item.cursed())
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

// Returns true if the player got prompted by an inscription warning and
// chose to opt out.
static bool _check_transformation_inscription_warning(
            const std::set<equipment_type> &remove)
{
    // Check over all items to be removed or melded.
    std::set<equipment_type>::const_iterator iter;
    for (iter = remove.begin(); iter != remove.end(); ++iter)
    {
        equipment_type e = *iter;
        if (you.equip[e] == -1)
            continue;

        const item_def& item = you.inv[you.equip[e]];

        operation_types op = OPER_WEAR;
        if (e == EQ_WEAPON)
            op = OPER_WIELD;
        else if (item.base_type == OBJ_JEWELLERY)
            op = OPER_PUTON;

        if (!check_old_item_warning(item, op))
            return (true);
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
    case TRAN_PIG:
        return SIZE_SMALL;
    case TRAN_ICE_BEAST:
        return SIZE_LARGE;
    case TRAN_DRAGON:
        return SIZE_HUGE;
    default:
        return SIZE_CHARACTER;
    }
}

void transformation_expiration_warning()
{
    if (you.duration[DUR_TRANSFORMATION]
            <= get_expiration_threshold(DUR_TRANSFORMATION))
    {
        mpr("You have a feeling this form won't last long.");
    }
}

static bool _abort_or_fizzle(bool just_check)
{
    if (!just_check && you.turn_is_over)
    {
        canned_msg(MSG_SPELL_FIZZLES);
        return (true); // pay the necessary costs
    }
    return (false); // SPRET_ABORT
}

monster_type transform_mons()
{
    switch(you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_SPIDER:
        return MONS_GIANT_COCKROACH; //uhh...
    case TRAN_STATUE:
        return MONS_STATUE;
    case TRAN_ICE_BEAST:
        return MONS_ICE_BEAST;
    case TRAN_DRAGON:
        return MONS_DRAGON;
    case TRAN_LICH:
        return MONS_LICH;
    case TRAN_BAT:
        return you.species == SP_VAMPIRE ? MONS_VAMPIRE_BAT : MONS_GIANT_BAT;
    case TRAN_PIG:
        return MONS_HOG;
    case TRAN_BLADE_HANDS:
    case TRAN_NONE:
        if (Options.show_player_species)
            return player_mons();
        return MONS_PLAYER;
    }
    ASSERT(!"unknown transformation");
    return MONS_PLAYER;
}

// Transforms you into the specified form. If force is true, checks for
// inscription warnings are skipped, and the transformation fails silently
// (if it fails). If just_check is true the transformation doesn't actually
// happen, but the method returns whether it would be successful.
bool transform(int pow, transformation_type which_trans, bool force,
               bool just_check)
{
    if (!force && crawl_state.is_god_acting())
        force = true;

    if (!force && you.transform_uncancellable)
    {
        // Jiyva's wrath-induced transformation is blocking the attempt.
        // May need to be updated if transform_uncancellable is used for
        // other uses.
        return (false);
    }

    if (you.species == SP_MERFOLK && you.swimming()
        && which_trans != TRAN_DRAGON && which_trans != TRAN_BAT)
    {
        // This might be overkill, but it's okay because obviously
        // whatever magical ability that lets them walk on land is
        // removed when they're in water (in this case, their natural
        // form is completely over-riding any other... goes well with
        // the forced transform when entering water)... but merfolk can
        // transform into flying forms.
        if (!force)
            mpr("You cannot transform out of your normal form while in water.");
        return (false);
    }

    // This must occur before the untransform() and the is_undead check.
    if (you.attribute[ATTR_TRANSFORMATION] == which_trans)
    {
        if (you.duration[DUR_TRANSFORMATION] < 100 * BASELINE_DELAY)
        {
            if (just_check)
                return (true);

            if (which_trans==TRAN_PIG)
                mpr("You feel you'll be a pig longer.");
            else
                mpr("You extend your transformation's duration.");
            you.increase_duration(DUR_TRANSFORMATION, random2(pow), 100);

            return (true);
        }
        else
        {
            if (!force && which_trans!=TRAN_PIG)
                mpr("You cannot extend your transformation any further!");
            return (false);
        }
    }

    // The actual transformation may still fail later (e.g. due to cursed
    // equipment). Ideally, untransforming should cost a turn but nothing
    // else (as does the "End Transformation" ability). As it is, you
    // pay with mana and hunger if you already untransformed.
    if (!just_check && you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
    {
        bool skip_wielding = false;
        switch (which_trans)
        {
        case TRAN_STATUE:
        case TRAN_LICH:
            break;
        default:
            skip_wielding = true;
            break;
        }
        // Skip wielding weapon if it gets unwielded again right away.
        untransform(skip_wielding);
    }

    // Catch some conditions which prevent transformation.
    if (you.is_undead
        && (you.species != SP_VAMPIRE
            || which_trans != TRAN_BAT && you.hunger_state <= HS_SATIATED))
    {
        if (!force)
            mpr("Your unliving flesh cannot be transformed in this way.");
        return (_abort_or_fizzle(just_check));
    }

    if (which_trans == TRAN_LICH && you.duration[DUR_DEATHS_DOOR])
    {
        if (!force)
        {
            mpr("The transformation conflicts with an enchantment "
                "already in effect.");
        }
        return (_abort_or_fizzle(just_check));
    }

    std::set<equipment_type> rem_stuff = _init_equipment_removal(which_trans);

    if (which_trans != TRAN_PIG
        && _check_for_cursed_equipment(rem_stuff, which_trans, force))
    {
        return (_abort_or_fizzle(just_check));
    }

    int str = 0, dex = 0, xhp = 0, dur = 0;
    const char* tran_name = "buggy";
    const char* msg = "You transform into something buggy!";
    switch (which_trans)
    {
    case TRAN_SPIDER:
        tran_name = "spider";
        dex       = 5;
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
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        if (player_genus(GENPC_DWARVEN) && one_chance_in(10))
            msg = "You inwardly fear your resemblance to a lawn ornament.";
        else
            msg = "You turn into a living statue of rough stone.";
        break;

    case TRAN_ICE_BEAST:
        tran_name = "ice beast";
        xhp       = 12;
        dur       = std::min(30 + random2(pow) + random2(pow), 100);
        msg       = "You turn into a creature of crystalline ice.";
        break;

    case TRAN_DRAGON:
        tran_name = "dragon";
        str       = 10;
        xhp       = 16;
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        if (you.species == SP_MERFOLK && you.swimming())
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
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        msg       = "Your body is suffused with negative energy!";
        break;

    case TRAN_BAT:
        tran_name = "bat";
        str       = -5;
        dex       = 5;
        dur       = std::min(20 + random2(pow) + random2(pow), 100);
        if (you.species == SP_VAMPIRE)
            msg = "You turn into a vampire bat.";
        else
            msg = "You turn into a bat.";
        break;

    case TRAN_PIG:
        tran_name = "pig";
        dur       = pow;
        msg       = "You have been turned into a pig!";
        you.transform_uncancellable = true;
        break;

    case TRAN_NONE:
    case NUM_TRANSFORMATIONS:
        break;
    }

    // If we're just pretending return now.
    if (just_check)
        return (true);

    if (!force && _check_transformation_inscription_warning(rem_stuff))
        return (_abort_or_fizzle(just_check));

    // All checks done, transformation will take place now.
    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;

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
    you.set_duration(DUR_TRANSFORMATION, dur);
    you.symbol = transform_mons();

    burden_change();

    if (str)
    {
        notify_stat_change(STAT_STR, str, true,
                    make_stringf("gaining the %s transformation",
                                 tran_name).c_str());
    }

    if (dex)
    {
        notify_stat_change(STAT_DEX, dex, true,
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

    default:
        break;
    }

    // This only has an effect if the transformation happens passively,
    // for example if Xom decides to transform you while you're busy
    // running around or butchering corpses.
    stop_delay();

    if (you.species != SP_VAMPIRE || which_trans != TRAN_BAT)
        transformation_expiration_warning();

    return (true);
}

bool transform_can_butcher_barehanded(transformation_type tt)
{
    return (tt == TRAN_BLADE_HANDS || tt == TRAN_DRAGON || tt == TRAN_ICE_BEAST);
}

void untransform(bool skip_wielding, bool skip_move)
{
    const flight_type old_flight = you.flight_mode();

    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;

    you.symbol = MONS_PLAYER;

    // Must be unset first or else infinite loops might result. -- bwr
    const transformation_type old_form =
        static_cast<transformation_type>(you.attribute[ ATTR_TRANSFORMATION ]);

    // We may have to unmeld a couple of equipment types.
    std::set<equipment_type> melded = _init_equipment_removal(old_form);

    you.attribute[ATTR_TRANSFORMATION] = TRAN_NONE;
    you.duration[DUR_TRANSFORMATION]   = 0;

    burden_change();

    int hp_downscale = 10;

    switch (old_form)
    {
    case TRAN_SPIDER:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        notify_stat_change( STAT_DEX, -5, true,
                     "losing the spider transformation" );
        break;

    case TRAN_BAT:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        notify_stat_change( STAT_DEX, -5, true,
                     "losing the bat transformation" );
        notify_stat_change( STAT_STR, 5, true,
                     "losing the bat transformation" );
        break;

    case TRAN_BLADE_HANDS:
        mpr( "Your hands revert to their normal proportions.", MSGCH_DURATION );
        you.wield_change = true;
        break;

    case TRAN_STATUE:
        mpr( "You revert to your normal fleshy form.", MSGCH_DURATION );
        notify_stat_change( STAT_DEX, 2, true,
                     "losing the statue transformation" );
        notify_stat_change( STAT_STR, -2, true,
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

        // Re-enter the terrain, it might kill us.
        if (!skip_move && feat_is_water(grd(you.pos())))
            move_player_to_grid(you.pos(), false, true);

        break;

    case TRAN_DRAGON:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        notify_stat_change(STAT_STR, -10, true,
                    "losing the dragon transformation" );
        hp_downscale = 16;
        break;

    case TRAN_LICH:
        mpr( "You feel yourself come back to life.", MSGCH_DURATION );
        notify_stat_change(STAT_STR, -3, true,
                    "losing the lich transformation" );
        you.is_undead = US_ALIVE;
        break;

    case TRAN_PIG:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        break;

    default:
        break;
    }

    _unmeld_equipment(melded);

    // Re-check terrain now that be may no longer be flying.
    if (!skip_move && old_flight && you.flight_mode() == FL_NONE)
        move_player_to_grid(you.pos(), false, true);

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

    // End Ozocubu's Icy Armour if you unmelded wearing heavy armour
    if (you.duration[DUR_ICY_ARMOUR]
        && !player_effectively_in_light_armour())
    {
        you.duration[DUR_ICY_ARMOUR] = 1;

        const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
        mprf(MSGCH_DURATION, "%s cracks your icy armour.",
             armour->name(DESC_CAP_YOUR).c_str());
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

    if (!skip_wielding)
        handle_interrupted_swap(true, false, true);

    you.turn_is_over = true;
    if (you.transform_uncancellable)
        you.transform_uncancellable = false;
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

void _extra_hp(int amount_extra) // must also set in calc_hp
{
    calc_hp();

    you.hp *= amount_extra;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);
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

std::string transform_desc(bool expire_pre)
{
    std::string text;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_SPIDER:
        text = "You are in spider-form.";
        break;
    case TRAN_BAT:
        text = "You are in ";
        if (you.species == SP_VAMPIRE)
            text += "vampire ";
        text += "bat-form.";
        break;
    case TRAN_BLADE_HANDS:
        text = "You have blades for hands.";
        break;
    case TRAN_STATUE:
        text = "You are a statue.";
        break;
    case TRAN_ICE_BEAST:
        text = "You are an ice creature.";
        break;
    case TRAN_DRAGON:
        text = "You are in dragon-form.";
        break;
    case TRAN_LICH:
        text = "You are in lich-form.";
        break;
   case TRAN_PIG:
        text += "You are a filthy swine.";
        break;
    }

    if ((you.species != SP_VAMPIRE || !player_in_bat_form())
        && dur_expiring(DUR_TRANSFORMATION))
    {
        text = expire_pre ? "Expiring :  " + text
                          : text + " (Expiring.)";
    }

    return (text);
}
