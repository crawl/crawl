/**
 * @file
 * @brief Functions for making use of inventory items.
**/

#include "AppHdr.h"

#include "item_use.h"

#include "abl-show.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "food.h"
#include "godabil.h"
#include "godconduct.h"
#include "goditem.h"
#include "hints.h"
#include "invent.h"
#include "evoke.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "mutation.h"
#include "options.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "throw.h"
#include "transform.h"
#include "uncancel.h"
#include "unwind.h"
#include "view.h"
#include "xom.h"

static bool _drink_fountain();
static int _handle_enchant_armour(int item_slot = -1,
                                  string *pre_msg = NULL);

static bool _is_cancellable_scroll(scroll_type scroll);
static bool _safe_to_remove_or_wear(const item_def &item, bool remove,
                                    bool quiet = false);

// Rather messy - we've gathered all the can't-wield logic from wield_weapon()
// here.
bool can_wield(item_def *weapon, bool say_reason,
               bool ignore_temporary_disability, bool unwield, bool only_known)
{
#define SAY(x) {if (say_reason) { x; }}

    if (!ignore_temporary_disability && you.berserk())
    {
        SAY(canned_msg(MSG_TOO_BERSERK));
        return false;
    }

    if (you.melded[EQ_WEAPON] && unwield)
    {
        SAY(mpr("Your weapon is melded into your body!"));
        return false;
    }

    if (!ignore_temporary_disability && !form_can_wield(you.form))
    {
        SAY(mpr("You can't wield anything in your present form."));
        return false;
    }

    if (!ignore_temporary_disability
        && you.weapon()
        && is_weapon(*you.weapon())
        && you.weapon()->cursed())
    {
        SAY(mprf("You can't unwield your weapon%s!",
                 !unwield ? " to draw a new one" : ""));
        return false;
    }

    // If we don't have an actual weapon to check, return now.
    if (!weapon)
        return true;

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
    {
        if (you.equip[i] != -1 && &you.inv[you.equip[i]] == weapon)
        {
            SAY(mpr("You are wearing that object!"));
            return false;
        }
    }

    if (you.species == SP_FELID && is_weapon(*weapon))
    {
        SAY(mpr("You can't use weapons."));
        return false;
    }

    if (weapon->base_type == OBJ_ARMOUR)
    {
        SAY(mpr("You can't wield armour."));
        return false;
    }

    if (weapon->base_type == OBJ_JEWELLERY)
    {
        SAY(mpr("You can't wield jewellery."));
        return false;
    }

    // Only ogres and trolls can wield giant clubs (>= 30 aum)
    // and large rocks (60 aum).
    if (you.body_size() < SIZE_LARGE
        && (item_mass(*weapon) >= 500
            || weapon->base_type == OBJ_WEAPONS
               && item_mass(*weapon) >= 300))
    {
        SAY(mpr("That's too large and heavy for you to wield."));
        return false;
    }

    // All non-weapons only need a shield check.
    if (weapon->base_type != OBJ_WEAPONS)
    {
        if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
        {
            SAY(mpr("You can't wield that with a shield."));
            return false;
        }
        else
            return true;
    }

    // Small species wielding large weapons...
    if (you.body_size(PSIZE_BODY) < SIZE_MEDIUM
        && !check_weapon_wieldable_size(*weapon, you.body_size(PSIZE_BODY)))
    {
        SAY(mpr("That's too large for you to wield."));
        return false;
    }

    if (you.undead_or_demonic() && is_holy_item(*weapon)
        && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("This weapon is holy and will not allow you to wield it.");
            // If it's a standard weapon, you know its ego now.
            if (!is_artefact(*weapon) && !is_blessed(*weapon)
                && !item_type_known(*weapon))
            {
                set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
                if (in_inventory(*weapon))
                    mpr_nocap(weapon->name(DESC_INVENTORY_EQUIP).c_str());
            }
            else if (is_artefact(*weapon) && !item_type_known(*weapon))
                artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
        }
        return false;
    }

    if (!ignore_temporary_disability
        && you.hunger_state < HS_FULL
        && get_weapon_brand(*weapon) == SPWPN_VAMPIRICISM
        && !crawl_state.game_is_zotdef()
        && !you.is_undead
        && !you_foodless()
        && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("As you grasp it, you feel a great hunger. Being not satiated, you stop.");
            // If it's a standard weapon, you know its ego now.
            if (!is_artefact(*weapon) && !is_blessed(*weapon)
                && !item_type_known(*weapon))
            {
                set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
                if (in_inventory(*weapon))
                    mpr_nocap(weapon->name(DESC_INVENTORY_EQUIP).c_str());
            }
            else if (is_artefact(*weapon) && !item_type_known(*weapon))
                artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
        }
        return false;
    }

    if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
    {
        SAY(mpr("You can't wield that with a shield."));
        return false;
    }

    // We can wield this weapon. Phew!
    return true;

#undef SAY
}

static bool _valid_weapon_swap(const item_def &item)
{
    if (is_weapon(item))
        return (you.species != SP_FELID);

    // Some misc. items need to be wielded to be evoked.
    if (is_deck(item) || item.base_type == OBJ_MISCELLANY
                         && item.sub_type == MISC_LANTERN_OF_SHADOWS)
    {
        return true;
    }

    if (item.base_type == OBJ_MISSILES
        && (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK))
    {
        return you.has_spell(SPELL_SANDBLAST);
    }

    // Snakable missiles; weapons were already handled above.
    if (item_is_snakable(item) && you.has_spell(SPELL_STICKS_TO_SNAKES))
        return true;

    // What follows pertains only to Sublimation of Blood and/or Simulacrum.
    if (!you.has_spell(SPELL_SUBLIMATION_OF_BLOOD)
        && !you.has_spell(SPELL_SIMULACRUM))
    {
        return false;
    }

    if (item.base_type == OBJ_FOOD && food_is_meaty(item))
        return item.sub_type == FOOD_CHUNK || you.has_spell(SPELL_SIMULACRUM);

    if (item.base_type == OBJ_POTIONS && item_type_known(item)
        && you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
    {
        return is_blood_potion(item);
    }

    return false;
}

/**
 * @param force If true, don't check weapon inscriptions.
 * (Assuming the player was already prompted for that.)
 */
bool wield_weapon(bool auto_wield, int slot, bool show_weff_messages,
                  bool force, bool show_unwield_msg, bool show_wield_msg)
{
    const bool was_barehanded = you.equip[EQ_WEAPON] == -1;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return false;
    }

    // Look for conditions like berserking that could prevent wielding
    // weapons.
    if (!can_wield(NULL, true, false, slot == SLOT_BARE_HANDS))
        return false;

    int item_slot = 0;          // default is 'a'

    if (auto_wield)
    {
        if (item_slot == you.equip[EQ_WEAPON]
            || you.equip[EQ_WEAPON] == -1
               && !_valid_weapon_swap(you.inv[item_slot]))
        {
            item_slot = 1;      // backup is 'b'
        }

        if (slot != -1)         // allow external override
            item_slot = slot;
    }

    // If the swap slot has a bad (but valid) item in it,
    // the swap will be to bare hands.
    const bool good_swap = (item_slot == SLOT_BARE_HANDS
                            || _valid_weapon_swap(you.inv[item_slot]));

    // Prompt if not using the auto swap command, or if the swap slot
    // is empty.
    if (item_slot != SLOT_BARE_HANDS
        && (!auto_wield || !you.inv[item_slot].defined() || !good_swap))
    {
        if (!auto_wield)
        {
            item_slot = prompt_invent_item(
                            "Wield which item (- for none, * to show all)?",
                            MT_INVLIST, OSEL_WIELD,
                            true, true, true, '-', -1, NULL, OPER_WIELD);
        }
        else
            item_slot = SLOT_BARE_HANDS;
    }

    if (prompt_failed(item_slot))
        return false;
    else if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are already wielding that!");
        return true;
    }

    // Now we really change weapons! (Most likely, at least...)
    if (you.duration[DUR_SURE_BLADE])
    {
        mpr("The bond with your blade fades away.");
        you.duration[DUR_SURE_BLADE] = 0;
    }
    // Reset the warning counter.
    you.received_weapon_warning = false;

    if (item_slot == SLOT_BARE_HANDS)
    {
        if (const item_def* wpn = you.weapon())
        {
            // Can we safely unwield this item?
            if (needs_handle_warning(*wpn, OPER_WIELD))
            {
                const string prompt =
                    "Really unwield " + wpn->name(DESC_INVENTORY) + "?";
                if (!yesno(prompt.c_str(), false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return false;
                }
            }

            if (!unwield_item(show_weff_messages))
                return false;

            if (show_unwield_msg)
                canned_msg(MSG_EMPTY_HANDED_NOW);

            // Switching to bare hands is extra fast.
            you.turn_is_over = true;
            you.time_taken *= 3;
            you.time_taken /= 10;
        }
        else
            canned_msg(MSG_EMPTY_HANDED_ALREADY);

        return true;
    }

    item_def& new_wpn(you.inv[item_slot]);

    // Non-auto_wield cases are checked below.
    if (auto_wield && !force
        && !check_warning_inscriptions(new_wpn, OPER_WIELD))
    {
        return false;
    }

    // Ensure wieldable, stat loss non-fatal
    if (!can_wield(&new_wpn, true) || !_safe_to_remove_or_wear(new_wpn, false))
        return false;

    // Unwield any old weapon.
    if (you.weapon())
    {
        if (unwield_item(show_weff_messages))
        {
            // Enable skills so they can be re-disabled later
            update_can_train();
        }
        else
            return false;
    }

    // Really ensure wieldable, even unknown brand
    if (!can_wield(&new_wpn, true, false, false, false))
    {
        if (!was_barehanded)
        {
            canned_msg(MSG_EMPTY_HANDED_NOW);

            // Switching to bare hands is extra fast.
            you.turn_is_over = true;
            you.time_taken *= 3;
            you.time_taken /= 10;
        }

        return false;
    }

    const unsigned int old_talents = your_talents(false).size();

    // Go ahead and wield the weapon.
    equip_item(EQ_WEAPON, item_slot, show_weff_messages);

    if (show_wield_msg)
        mpr_nocap(new_wpn.name(DESC_INVENTORY_EQUIP).c_str());

    check_item_hint(new_wpn, old_talents);

    // Time calculations.
    you.time_taken /= 2;

    you.wield_change  = true;
    you.m_quiver->on_weapon_changed();
    you.turn_is_over  = true;

    return true;
}

static const char *shield_base_name(const item_def *shield)
{
    return (shield->sub_type == ARM_BUCKLER ? "buckler"
                                            : "shield");
}

static const char *shield_impact_degree(int impact)
{
    return (impact > 160 ? "severely "      :
            impact > 130 ? "significantly " :
            impact > 110 ? ""
                         : NULL);
}

static void _warn_launcher_shield_slowdown(const item_def &launcher)
{
    const int slowspeed =
        launcher_final_speed(launcher, you.shield()) * player_speed() / 100;
    const int normspeed =
        launcher_final_speed(launcher, NULL) * player_speed() / 100;

    // Don't warn the player unless the slowdown is real.
    if (slowspeed > normspeed)
    {
        const char *slow_degree =
            shield_impact_degree(slowspeed * 100 / normspeed);

        if (slow_degree)
        {
            mprf(MSGCH_WARN,
                    "Your %s %sslows your rate of fire.",
                    shield_base_name(you.shield()),
                    slow_degree);
        }
    }
}

// Warn if your shield is greatly impacting the effectiveness of your weapon?
void warn_shield_penalties()
{
    if (!you.shield())
        return;

    // Warnings are limited to launchers at the moment.
    const item_def *weapon = you.weapon();
    if (!weapon)
        return;

    if (is_range_weapon(*weapon))
        _warn_launcher_shield_slowdown(*weapon);
}

bool item_is_worn(int inv_slot)
{
    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; ++i)
        if (inv_slot == you.equip[i])
            return true;

    return false;
}

//---------------------------------------------------------------
//
// armour_prompt
//
// Prompt the user for some armour. Returns true if the user picked
// something legit.
//
//---------------------------------------------------------------
bool armour_prompt(const string & mesg, int *index, operation_types oper)
{
    ASSERT(index != NULL);

    if (inv_count() < 1)
        canned_msg(MSG_NOTHING_CARRIED);
    else if (you.berserk())
        canned_msg(MSG_TOO_BERSERK);
    else
    {
        int selector = OBJ_ARMOUR;
        if (oper == OPER_TAKEOFF && !Options.equip_unequip)
            selector = OSEL_WORN_ARMOUR;
        int slot = prompt_invent_item(mesg.c_str(), MT_INVLIST, selector,
                                      true, true, true, 0, -1, NULL,
                                      oper);

        if (!prompt_failed(slot))
        {
            *index = slot;
            return true;
        }
    }

    return false;
}

static bool cloak_is_being_removed(void)
{
    if (current_delay_action() != DELAY_ARMOUR_OFF)
        return false;

    if (you.delay_queue.front().parm1 != you.equip[ EQ_CLOAK ])
        return false;

    return true;
}

//---------------------------------------------------------------
//
// wear_armour
//
//---------------------------------------------------------------
void wear_armour(int slot) // slot is for tiles
{
    if (you.species == SP_FELID)
    {
        mpr("You can't wear anything.");
        return;
    }

    if (!form_can_wear())
    {
        mpr("You can't wear anything in your present form.");
        return;
    }

    int armour_wear_2 = 0;

    if (slot != -1)
        armour_wear_2 = slot;
    else if (!armour_prompt("Wear which item?", &armour_wear_2, OPER_WEAR))
        return;

    do_wear_armour(armour_wear_2, false);
}

static int armour_equip_delay(const item_def &item)
{
    int delay = property(item, PARM_AC);

    // Shields are comparatively easy to wear.
    if (is_shield(item))
        delay = delay / 2 + 1;

    if (delay < 1)
        delay = 1;

    return delay;
}

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary)
{
    const object_class_type base_type = item.base_type;
    if (base_type != OBJ_ARMOUR || you.species == SP_FELID)
    {
        if (verbose)
            mpr("You can't wear that.");

        return false;
    }

    const int sub_type = item.sub_type;
    const equipment_type slot = get_armour_slot(item);

    if (you.species == SP_OCTOPODE && slot != EQ_HELMET && slot != EQ_SHIELD)
    {
        if (verbose)
            mpr("You can't wear that!");
        return false;
    }

    if (player_genus(GENPC_DRACONIAN) && slot == EQ_BODY_ARMOUR)
    {
        if (verbose)
        {
            mprf("Your wings%s won't fit in that.", you.mutation[MUT_BIG_WINGS]
                 ? "" : ", even vestigial as they are,");
        }
        return false;
    }

    if (sub_type == ARM_NAGA_BARDING || sub_type == ARM_CENTAUR_BARDING)
    {
        if (you.species == SP_NAGA && sub_type == ARM_NAGA_BARDING
            || you.species == SP_CENTAUR && sub_type == ARM_CENTAUR_BARDING)
        {
            if (ignore_temporary || !player_is_shapechanged())
                return true;
            else if (verbose)
                mpr("You can wear that only in your normal form.");
        }
        else if (verbose)
            mpr("You can't wear that!");
        return false;
    }

    // Lear's hauberk covers also head, hands and legs.
    if (is_unrandom_artefact(item) && item.special == UNRAND_LEAR)
    {
        if (!player_has_feet(!ignore_temporary))
        {
            if (verbose)
                mpr("You have no feet.");
            return false;
        }

        if (!ignore_temporary)
        {
            for (int s = EQ_HELMET; s <= EQ_BOOTS; s++)
            {
                // No strange race can wear this.
                const char* parts[] = { "head", "hands", "feet" };
                // Auto-disrobing would be nice.
                if (you.equip[s] != -1)
                {
                    if (verbose)
                        mprf("You'd need your %s free.", parts[s - EQ_HELMET]);
                    return false;
                }

                if (!you_tran_can_wear(s, true))
                {
                    if (verbose)
                    {
                        mprf(you_tran_can_wear(s) ? "The hauberk won't fit your %s."
                                                  : "You have no %s!",
                             parts[s - EQ_HELMET]);
                    }
                    return false;
                }
            }
        }
    }
    else if (slot >= EQ_HELMET && slot <= EQ_BOOTS
             && !ignore_temporary
             && player_equip_unrand(UNRAND_LEAR))
    {
        // The explanation is iffy for loose headgear, especially crowns:
        // kings loved hooded hauberks, according to portraits.
        if (verbose)
            mpr("You can't wear this over your hauberk.");
        return false;
    }

    size_type player_size = you.body_size(PSIZE_TORSO, ignore_temporary);
    int bad_size = fit_armour_size(item, player_size);

    if (bad_size)
    {
        if (verbose)
            mprf("This armour is too %s for you!",
                 (bad_size > 0) ? "big" : "small");

        return false;
    }

    if (you.form == TRAN_APPENDAGE
        && ignore_temporary
        && slot == beastly_slot(you.attribute[ATTR_APPENDAGE])
        && you.mutation[you.attribute[ATTR_APPENDAGE]])
    {
        unwind_var<uint8_t> mutv(you.mutation[you.attribute[ATTR_APPENDAGE]], 0);
        // disable the mutation then check again
        return can_wear_armour(item, verbose, ignore_temporary);
    }

    if (sub_type == ARM_GLOVES)
    {
        if (you.has_claws(false) == 3)
        {
            if (verbose)
                mpr("You can't wear gloves with your huge claws!");
            return false;
        }
    }

    if (sub_type == ARM_BOOTS)
    {
        if (player_mutation_level(MUT_HOOVES) == 3)
        {
            if (verbose)
                mpr("You can't wear boots with hooves!");
            return false;
        }

        if (you.has_talons(false) == 3)
        {
            if (verbose)
                mpr("Boots don't fit your talons!");
            return false;
        }

        if (you.species == SP_NAGA || you.species == SP_DJINNI)
        {
            if (verbose)
                mpr("You have no legs!");
            return false;
        }

        if (!ignore_temporary && you.fishtail)
        {
            if (verbose)
                mpr("You don't currently have feet!");
            return false;
        }
    }

    if (slot == EQ_HELMET)
    {
        // Horns 3 & Antennae 3 mutations disallow all headgear
        if (player_mutation_level(MUT_HORNS) == 3)
        {
            if (verbose)
                mpr("You can't wear any headgear with your large horns!");
            return false;
        }

        if (player_mutation_level(MUT_ANTENNAE) == 3)
        {
            if (verbose)
                mpr("You can't wear any headgear with your large antennae!");
            return false;
        }

        // Soft helmets (caps and wizard hats) always fit, otherwise.
        if (is_hard_helmet(item))
        {
            if (player_mutation_level(MUT_HORNS))
            {
                if (verbose)
                    mpr("You can't wear that with your horns!");
                return false;
            }

            if (player_mutation_level(MUT_BEAK))
            {
                if (verbose)
                    mpr("You can't wear that with your beak!");
                return false;
            }

            if (player_mutation_level(MUT_ANTENNAE))
            {
                if (verbose)
                    mpr("You can't wear that with your antennae!");
                return false;
            }

            if (player_genus(GENPC_DRACONIAN))
            {
                if (verbose)
                    mpr("You can't wear that with your reptilian head.");
                return false;
            }

            if (you.species == SP_OCTOPODE)
            {
                if (verbose)
                    mpr("You can't wear that!");
                return false;
            }
        }
    }

    if (!ignore_temporary && !form_can_wear_item(item, you.form))
    {
        if (verbose)
            mpr("You can't wear that in your present form.");
        return false;
    }

    return true;
}

bool do_wear_armour(int item, bool quiet)
{
    const item_def &invitem = you.inv[item];
    if (!invitem.defined())
    {
        if (!quiet)
            mpr("You don't have any such object.");
        return false;
    }

    if (!can_wear_armour(invitem, !quiet, false))
        return false;

    const equipment_type slot = get_armour_slot(invitem);

    if (item == you.equip[EQ_WEAPON])
    {
        if (!quiet)
            mpr("You are wielding that object!");
        return false;
    }

    if (item_is_worn(item))
    {
        if (Options.equip_unequip)
            return !takeoff_armour(item);
        else
        {
            mpr("You're already wearing that object!");
            return false;
        }
    }

    // if you're wielding something,
    if (you.weapon()
        // attempting to wear a shield,
        && is_shield(invitem)
        && is_shield_incompatible(*you.weapon(), &invitem))
    {
        if (!quiet)
        {
            if (you.species == SP_OCTOPODE)
                mpr("You need the rest of your tentacles for walking.");
            else
                mprf("You'd need three %s to do that!", you.hand_name(true).c_str());
        }
        return false;
    }

    bool removed_cloak = false;
    int  cloak = -1;

    // Removing body armour requires removing the cloak first.
    if (slot == EQ_BODY_ARMOUR
        && you.equip[EQ_CLOAK] != -1 && !cloak_is_being_removed())
    {
        if (you.equip[EQ_BODY_ARMOUR] != -1
            && you.inv[you.equip[EQ_BODY_ARMOUR]].cursed())
        {
            if (!quiet)
            {
                mprf("%s is stuck to your body!",
                     you.inv[you.equip[EQ_BODY_ARMOUR]].name(DESC_YOUR)
                                                       .c_str());
            }
            return false;
        }
        if (!you.inv[you.equip[EQ_CLOAK]].cursed())
        {
            cloak = you.equip[EQ_CLOAK];
            if (!takeoff_armour(you.equip[EQ_CLOAK]))
                return false;

            removed_cloak = true;
        }
        else
        {
            if (!quiet)
                mpr("Your cloak prevents you from wearing the armour.");
            return false;
        }
    }

    if ((slot == EQ_CLOAK
           || slot == EQ_HELMET
           || slot == EQ_GLOVES
           || slot == EQ_BOOTS
           || slot == EQ_SHIELD
           || slot == EQ_BODY_ARMOUR)
        && you.equip[slot] != -1)
    {
        if (!takeoff_armour(you.equip[slot]))
            return false;
    }

    you.turn_is_over = true;

    if (!_safe_to_remove_or_wear(invitem, false))
        return false;

    const int delay = armour_equip_delay(invitem);
    if (delay)
        start_delay(DELAY_ARMOUR_ON, delay, item);

    if (removed_cloak)
        start_delay(DELAY_ARMOUR_ON, 1, cloak);

    return true;
}

bool takeoff_armour(int item)
{
    const item_def& invitem = you.inv[item];

    if (invitem.base_type != OBJ_ARMOUR)
    {
        mpr("You aren't wearing that!");
        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    const equipment_type slot = get_armour_slot(invitem);
    if (item == you.equip[slot] && you.melded[slot])
    {
        mprf("%s is melded into your body!",
             invitem.name(DESC_YOUR).c_str());
        return false;
    }

    if (!item_is_worn(item))
    {
        if (Options.equip_unequip)
            return do_wear_armour(item, true);
        else
        {
            mpr("You aren't wearing that object!");
            return false;
        }
    }

    // If we get here, we're wearing the item.
    if (invitem.cursed())
    {
        mprf("%s is stuck to your body!", invitem.name(DESC_YOUR).c_str());
        return false;
    }

    if (!_safe_to_remove_or_wear(invitem, true))
        return false;

    bool removed_cloak = false;
    int cloak = -1;

    if (slot == EQ_BODY_ARMOUR)
    {
        if (you.equip[EQ_CLOAK] != -1 && !cloak_is_being_removed())
        {
            if (!you.inv[you.equip[EQ_CLOAK]].cursed())
            {
                cloak = you.equip[ EQ_CLOAK ];
                if (!takeoff_armour(you.equip[EQ_CLOAK]))
                    return false;

                removed_cloak = true;
            }
            else
            {
                mpr("Your cloak prevents you from removing the armour.");
                return false;
            }
        }
    }
    else
    {
        switch (slot)
        {
        case EQ_SHIELD:
        case EQ_CLOAK:
        case EQ_HELMET:
        case EQ_GLOVES:
        case EQ_BOOTS:
            if (item != you.equip[slot])
            {
                mpr("You aren't wearing that!");
                return false;
            }
            break;

        default:
            break;
        }
    }

    you.turn_is_over = true;

    const int delay = armour_equip_delay(invitem);
    start_delay(DELAY_ARMOUR_OFF, delay, item);

    if (removed_cloak)
        start_delay(DELAY_ARMOUR_ON, 1, cloak);

    return true;
}

static int _prompt_ring_to_remove(int new_ring)
{
    const item_def *left  = you.slot_item(EQ_LEFT_RING, true);
    const item_def *right = you.slot_item(EQ_RIGHT_RING, true);

    mesclr();
    mprf("Wearing %s.", you.inv[new_ring].name(DESC_A).c_str());

    const char lslot = index_to_letter(left->link);
    const char rslot = index_to_letter(right->link);

#ifdef TOUCH_UI
    string prompt = "You're wearing two rings. Remove which one?";
    Popup *pop = new Popup(prompt);
    pop->push_entry(new MenuEntry(prompt, MEL_TITLE));
    InvEntry *me = new InvEntry(*left);
    pop->push_entry(me);
    me = new InvEntry(*right);
    pop->push_entry(me);

    int c;
    do
        c = pop->pop();
    while (c != lslot && c != rslot && c != '<' && c != '>'
           && !key_is_escape(c) && c != ' ');

#else
    mprf(MSGCH_PROMPT,
         "You're wearing two rings. Remove which one? (%c/%c/<</>/Esc)",
         lslot, rslot);

    mprf(" << or %s", left->name(DESC_INVENTORY).c_str());
    mprf(" > or %s", right->name(DESC_INVENTORY).c_str());
    flush_prev_message();

    // Deactivate choice from tile inventory.
    // FIXME: We need to be able to get the choice (item letter)
    //        *without* the choice taking action by itself!
    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
        c = getchm();
    while (c != lslot && c != rslot && c != '<' && c != '>'
           && !key_is_escape(c) && c != ' ');
#endif

    mesclr();

    if (key_is_escape(c) || c == ' ')
        return -1;

    const int eqslot = (c == lslot || c == '<') ? EQ_LEFT_RING
                                                : EQ_RIGHT_RING;
    return you.equip[eqslot];
}

static int _prompt_ring_to_remove_octopode(int new_ring)
{
    const item_def *rings[8];
    char slots[8];

    const int num_rings = (form_keeps_mutations() || you.form == TRAN_SPIDER
                           ? 8 : 2);

    for (int i = 0; i < num_rings; i++)
    {
        rings[i] = you.slot_item((equipment_type)(EQ_RING_ONE + i), true);
        ASSERT(rings[i]);
        slots[i] = index_to_letter(rings[i]->link);
    }

    mesclr();
//    mprf("Wearing %s.", you.inv[new_ring].name(DESC_A).c_str());

    mprf(MSGCH_PROMPT,
         "You're wearing all the rings you can. Remove which one?");
//I think it looks better without the letters.
// (%c/%c/%c/%c/%c/%c/%c/%c/Esc)",
//         one_slot, two_slot, three_slot, four_slot, five_slot, six_slot, seven_slot, eight_slot);
    mprf(MSGCH_PROMPT, "(<w>?</w> for menu, <w>Esc</w> to cancel)");

    for (int i = 0; i < num_rings; i++)
        mprf_nocap("%s", rings[i]->name(DESC_INVENTORY).c_str());
    flush_prev_message();

    // Deactivate choice from tile inventory.
    // FIXME: We need to be able to get the choice (item letter)
    //        *without* the choice taking action by itself!
    int eqslot = EQ_NONE;

    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
    {
        c = getchm();
        for (int i = 0; i < num_rings; i++)
            if (c == slots[i])
            {
                eqslot = EQ_RING_ONE + i;
                c = ' ';
                break;
            }
    } while (!key_is_escape(c) && c != ' ' && c != '?');

    mesclr();

    if (c == '?')
        return EQ_NONE;
    else if (key_is_escape(c) || eqslot == EQ_NONE)
        return -2;

    return you.equip[eqslot];
}

// Checks whether a to-be-worn or to-be-removed item affects
// character stats and whether wearing/removing it could be fatal.
// If so, warns the player, or just returns false if quiet is true.
static bool _safe_to_remove_or_wear(const item_def &item, bool remove, bool quiet)
{
    if (remove && !safe_to_remove(item, quiet))
        return false;

    int prop_str = 0;
    int prop_dex = 0;
    int prop_int = 0;
    if (item.base_type == OBJ_JEWELLERY
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        switch (item.sub_type)
        {
        case RING_STRENGTH:
            if (item.plus != 0)
                prop_str = item.plus;
            break;
        case RING_DEXTERITY:
            if (item.plus != 0)
                prop_dex = item.plus;
            break;
        case RING_INTELLIGENCE:
            if (item.plus != 0)
                prop_int = item.plus;
            break;
        default:
            break;
        }
    }
    else if (item.base_type == OBJ_ARMOUR && item_type_known(item))
    {
        switch (item.special)
        {
        case SPARM_STRENGTH:
            prop_str = 3;
            break;
        case SPARM_INTELLIGENCE:
            prop_int = 3;
            break;
        case SPARM_DEXTERITY:
            prop_dex = 3;
            break;
        default:
            break;
        }
    }

    if (is_artefact(item))
    {
        prop_str += artefact_known_wpn_property(item, ARTP_STRENGTH);
        prop_int += artefact_known_wpn_property(item, ARTP_INTELLIGENCE);
        prop_dex += artefact_known_wpn_property(item, ARTP_DEXTERITY);
    }

    if (!remove)
    {
        prop_str *= -1;
        prop_int *= -1;
        prop_dex *= -1;
    }
    stat_type red_stat = NUM_STATS;
    if (prop_str >= you.strength() && you.strength() > 0)
        red_stat = STAT_STR;
    else if (prop_int >= you.intel() && you.intel() > 0)
        red_stat = STAT_INT;
    else if (prop_dex >= you.dex() && you.dex() > 0)
        red_stat = STAT_DEX;

    if (red_stat == NUM_STATS)
        return true;

    if (quiet)
        return false;

    string verb = "";
    if (remove)
    {
        if (item.base_type == OBJ_WEAPONS)
            verb = "Unwield";
        else
            verb = "Remov";
    }
    else
    {
        if (item.base_type == OBJ_WEAPONS)
            verb = "Wield";
        else
            verb = "Wear";
    }

    string prompt = make_stringf("%sing this item will reduce your %s to zero "
                                 "or below. Continue?", verb.c_str(),
                                 stat_desc(red_stat, SD_NAME));
    if (!yesno(prompt.c_str(), true, 'n', true, false))
    {
        canned_msg(MSG_OK);
        return false;
    }
    return true;
}

// Checks whether removing an item would cause flight to end and the
// player to fall to their death.
bool safe_to_remove(const item_def &item, bool quiet)
{
    item_info inf = get_item_info(item);

    const bool grants_flight =
         inf.base_type == OBJ_JEWELLERY && inf.sub_type == RING_FLIGHT
         || inf.base_type == OBJ_ARMOUR && inf.special == SPARM_FLYING
         || is_artefact(inf)
            && artefact_known_wpn_property(inf, ARTP_FLY);

    // assumes item can't grant flight twice
    const bool removing_ends_flight =
        you.flight_mode()
        && !you.attribute[ATTR_FLIGHT_UNCANCELLABLE]
        && (you.evokable_flight() == 1);

    const dungeon_feature_type feat = grd(you.pos());

    if (grants_flight && removing_ends_flight
        && (feat == DNGN_LAVA
            || feat == DNGN_DEEP_WATER && !player_likes_water()))
    {
        if (quiet)
            return false;
        else
        {
            string fname = (feat == DNGN_LAVA ? "lava" : "deep water");
            string prompt = "Really remove this item over " + fname + "?";
            return yesno(prompt.c_str(), false, 'n');
        }
    }

    return true;
}

// Assumptions:
// you.inv[ring_slot] is a valid ring.
// EQ_LEFT_RING and EQ_RIGHT_RING are both occupied, and ring_slot is not
// one of the worn rings.
//
// Does not do amulets.
static bool _swap_rings(int ring_slot)
{
    const item_def* lring = you.slot_item(EQ_LEFT_RING, true);
    const item_def* rring = you.slot_item(EQ_RIGHT_RING, true);

    // If ring slots were melded, we should have been prevented from
    // putting on the ring at all.  If it becomes possible for just
    // one ring slot to be melded, the subsequent code will need to
    // be revisited, so prevent that, too.
    ASSERT(!you.melded[EQ_LEFT_RING]);
    ASSERT(!you.melded[EQ_RIGHT_RING]);

    if (lring->cursed() && rring->cursed())
    {
        mprf("You're already wearing two cursed rings!");
        return false;
    }

    int unwanted;

    // Don't prompt if both rings are of the same type.
    if ((lring->sub_type == rring->sub_type
         && lring->plus == rring->plus
         && lring->plus2 == rring->plus2
         && !is_artefact(*lring) && !is_artefact(*rring)) ||
        lring->cursed() || rring->cursed())
    {
        if (lring->cursed())
            unwanted = you.equip[EQ_RIGHT_RING];
        else
            unwanted = you.equip[EQ_LEFT_RING];
    }
    else
    {
        // Ask the player which existing ring is persona non grata.
        unwanted = _prompt_ring_to_remove(ring_slot);
    }

    if (unwanted == -1)
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (!remove_ring(unwanted, false))
        return false;

    // Check for stat loss.
    if (!_safe_to_remove_or_wear(you.inv[ring_slot], false))
        return false;

    // Put on the new ring.
    start_delay(DELAY_JEWELLERY_ON, 1, ring_slot);

    return true;
}

static bool _swap_rings_octopode(int ring_slot)
{
    const item_def* ring[8];
    for (int i = 0; i < 8; i++)
        ring[i] = you.slot_item((equipment_type)(EQ_RING_ONE + i), true);
    int array = 0;
    int unwanted = 0;
    int cursed = 0;
    int melded = 0; // Both melded rings and unavailable slots.
    int available = 0;

    for (int slots = EQ_RING_ONE;
         slots < NUM_EQUIP && array < 8;
         ++slots, ++array)
    {
        if (!you_tran_can_wear(slots) || you.melded[slots])
            melded++;
        else if (ring[array] != NULL)
        {
            if (ring[array]->cursed())
                cursed++;
            else
            {
                available++;
                unwanted = you.equip[slots];
            }
        }
    }

    // We can't put a ring on, because we're wearing 8 cursed ones.
    if (melded == 8)
    {
        // Shouldn't happen, because hogs and bats can't put on jewellery at
        // all and thus won't get this far.
        mpr("You can't wear that in your present form.");
        return false;
    }
    else if (available == 0)
    {
        mprf("You're already wearing %s cursed rings!%s",
             number_in_words(cursed).c_str(),
             (cursed == 8 ? " Isn't that enough for you?" : ""));
        return false;
    }
    // The simple case - only one available ring.
    else if (available == 1)
    {
        if (!remove_ring(unwanted, false))
            return false;
    }
    // We can't put a ring on without swapping - because we found
    // multiple available rings.
    else if (available > 1)
    {
        unwanted = _prompt_ring_to_remove_octopode(ring_slot);

        // Cancelled:
        if (unwanted < -1)
        {
            canned_msg(MSG_OK);
            return false;
        }

        if (!remove_ring(unwanted, false))
            return false;
    }

#if 0
    // In case something goes wrong.
    if (unwanted == -1)
    {
        canned_msg(MSG_OK);
        return false;
    }
#endif

    // Put on the new ring.
    start_delay(DELAY_JEWELLERY_ON, 1, ring_slot);

    return true;
}

static bool _puton_item(int item_slot)
{
    item_def& item = you.inv[item_slot];

    for (int eq = EQ_LEFT_RING; eq < NUM_EQUIP; eq++)
        if (item_slot == you.equip[eq])
        {
            // "Putting on" an equipped item means taking it off.
            if (Options.equip_unequip)
                return !remove_ring(item_slot);
            else
            {
                mpr("You're already wearing that object!");
                return false;
            }
        }

    if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are wielding that object.");
        return false;
    }

    if (item.base_type != OBJ_JEWELLERY)
    {
        mpr("You can only put on jewellery.");
        return false;
    }

    const bool lring = you.slot_item(EQ_LEFT_RING, true);
    const bool rring = you.slot_item(EQ_RIGHT_RING, true);
    const bool is_amulet = jewellery_is_amulet(item);
    bool blinged_octopode = false;
    if (you.species == SP_OCTOPODE)
    {
        blinged_octopode = true;
        for (int eq = EQ_RING_ONE; eq <= EQ_RING_EIGHT; eq++)
        {
            // Skip unavailable slots.
            if (!you_tran_can_wear(eq))
                continue;

            if (!you.slot_item((equipment_type)eq, true))
            {
                blinged_octopode = false;
                break;
            }
        }
    }

    if (!is_amulet)     // i.e. it's a ring
    {
        if (!you_tran_can_wear(item))
        {
            mpr("You can't wear that in your present form.");
            return false;
        }

        const item_def* gloves = you.slot_item(EQ_GLOVES, false);
        // Cursed gloves cannot be removed.
        if (gloves && gloves->cursed())
        {
            mpr("You can't take your gloves off to put on a ring!");
            return false;
        }

        if (blinged_octopode)
            return _swap_rings_octopode(item_slot);

        if (lring && rring)
            return _swap_rings(item_slot);
    }
    else if (item_def* amulet = you.slot_item(EQ_AMULET, true))
    {
        // Remove the previous one.
        if (!check_warning_inscriptions(*amulet, OPER_REMOVE)
            || !remove_ring(you.equip[EQ_AMULET], true))
        {
            return false;
        }

        // Check for stat loss.
        if (!_safe_to_remove_or_wear(item, false))
            return false;

        // Put on the new amulet.
        start_delay(DELAY_JEWELLERY_ON, 1, item_slot);

        // Assume it's going to succeed.
        return true;
    }

    // Check for stat loss.
    if (!_safe_to_remove_or_wear(item, false))
        return false;

    equipment_type hand_used;

    if (is_amulet)
        hand_used = EQ_AMULET;
    else if (you.species == SP_OCTOPODE)
    {
        for (hand_used = EQ_RING_ONE; hand_used <= EQ_RING_EIGHT;
             hand_used = (equipment_type)(hand_used + 1))
        {
            // Skip unavailble slots.
            if (!you_tran_can_wear(hand_used))
                continue;

            if (!you.slot_item(hand_used, true))
                break;
        }
        ASSERT(hand_used <= EQ_RING_EIGHT);
    }
    else
    {
        // First ring always goes on left hand.
        hand_used = EQ_LEFT_RING;

        // ... unless we're already wearing a ring on the left hand.
        if (lring && !rring)
            hand_used = EQ_RIGHT_RING;
    }

    const unsigned int old_talents = your_talents(false).size();

    // Actually equip the item.
    equip_item(hand_used, item_slot);

    check_item_hint(you.inv[item_slot], old_talents);
#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
    }
#endif

    // Putting on jewellery is as fast as wielding weapons.
    you.time_taken /= 2;
    you.turn_is_over = true;

    return true;
}

bool puton_ring(int slot)
{
    int item_slot;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Put on which piece of jewellery?",
                                        MT_INVLIST, OBJ_JEWELLERY, true, true,
                                        true, 0, -1, NULL, OPER_PUTON);
    }

    if (prompt_failed(item_slot))
        return false;

    return _puton_item(item_slot);
}

bool remove_ring(int slot, bool announce)
{
    equipment_type hand_used = EQ_NONE;
    int ring_wear_2;
    bool has_jewellery = false;
    bool has_melded = false;
    const equipment_type first = you.species == SP_OCTOPODE ? EQ_AMULET
                                                            : EQ_LEFT_RING;
    const equipment_type last = you.species == SP_OCTOPODE ? EQ_RING_EIGHT
                                                           : EQ_AMULET;

    for (int eq = first; eq <= last; eq++)
    {
        if (player_wearing_slot(eq))
        {
            if (has_jewellery)
            {
                // At least one other piece, which means we'll have to ask
                hand_used = EQ_NONE;
            }
            else
                hand_used = (equipment_type) eq;

            has_jewellery = true;
        }
        else if (you.melded[eq])
            has_melded = true;
    }

    if (!has_jewellery)
    {
        if (has_melded)
            mpr("You aren't wearing any unmelded rings or amulets.");
        else
            mpr("You aren't wearing any rings or amulets.");

        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    const item_def* gloves = you.slot_item(EQ_GLOVES);
    const bool gloves_cursed = gloves && gloves->cursed();
    if (gloves_cursed && !player_wearing_slot(EQ_AMULET))
    {
        mpr("You can't take your gloves off to remove any rings!");
        return false;
    }

    if (hand_used == EQ_NONE)
    {
        const int equipn =
            (slot == -1)? prompt_invent_item("Remove which piece of jewellery?",
                                             MT_INVLIST,
                                             OBJ_JEWELLERY, true, true, true,
                                             0, -1, NULL, OPER_REMOVE)
                        : slot;

        if (prompt_failed(equipn))
            return false;

        hand_used = item_equip_slot(you.inv[equipn]);
        if (hand_used == EQ_NONE)
        {
            mpr("You aren't wearing that.");
            return false;
        }
        else if (you.inv[equipn].base_type != OBJ_JEWELLERY)
        {
            mpr("That isn't a piece of jewellery.");
            return false;
        }
    }

    if (you.equip[hand_used] == -1)
    {
        mpr("I don't think you really meant that.");
        return false;
    }
    else if (you.melded[hand_used])
    {
        mpr("You can't take that off while it's melded.");
        return false;
    }
    else if (gloves_cursed
             && (hand_used == EQ_LEFT_RING || hand_used == EQ_RIGHT_RING))
    {
        mpr("You can't take your gloves off to remove any rings!");
        return false;
    }

    if (!check_warning_inscriptions(you.inv[you.equip[hand_used]],
                                    OPER_REMOVE))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (you.inv[you.equip[hand_used]].cursed())
    {
        if (announce)
        {
            mprf("%s is stuck to you!",
                 you.inv[you.equip[hand_used]].name(DESC_YOUR).c_str());
        }
        else
            mpr("It's stuck to you!");

        set_ident_flags(you.inv[you.equip[hand_used]], ISFLAG_KNOW_CURSE);
        return false;
    }

    ring_wear_2 = you.equip[hand_used];

    // Remove the ring.
    if (!_safe_to_remove_or_wear(you.inv[ring_wear_2], true))
        return false;

    mprf("You remove %s.", you.inv[ring_wear_2].name(DESC_YOUR).c_str());
#ifdef USE_TILE_LOCAL
    const unsigned int old_talents = your_talents(false).size();
#endif
    unequip_item(hand_used);
#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
    }
#endif

    you.time_taken /= 2;
    you.turn_is_over = true;

    return true;
}

static int _wand_range(zap_type ztype)
{
    // FIXME: Eventually we should have sensible values here.
    return LOS_RADIUS;
}

static int _max_wand_range()
{
    return LOS_RADIUS;
}

static bool _dont_use_invis()
{
    if (!you.backlit())
        return false;

    if (you.haloed() || you.glows_naturally())
    {
        mpr("You can't turn invisible.");
        return true;
    }
    else if (get_contamination_level() > 1
             && !yesno("Invisibility will do you no good right now; "
                       "use anyway?", false, 'n'))
    {
        canned_msg(MSG_OK);
        return true;
    }

    return false;
}

static targetter *_wand_targetter(const item_def *wand)
{
    int range = _wand_range(wand->zap());
    const int power = 15 + you.skill(SK_EVOCATIONS, 5) / 2;

    switch (wand->sub_type)
    {
    case WAND_FIREBALL:
        return new targetter_beam(&you, range, ZAP_FIREBALL, power, 1, 1);
    case WAND_LIGHTNING:
        return new targetter_beam(&you, range, ZAP_LIGHTNING_BOLT, power, 0, 0);
    case WAND_FLAME:
        return new targetter_beam(&you, range, ZAP_THROW_FLAME, power, 0, 0);
    case WAND_FIRE:
        return new targetter_beam(&you, range, ZAP_BOLT_OF_FIRE, power, 0, 0);
    case WAND_FROST:
        return new targetter_beam(&you, range, ZAP_THROW_FROST, power, 0, 0);
    case WAND_COLD:
        return new targetter_beam(&you, range, ZAP_BOLT_OF_COLD, power, 0, 0);
    default:
        return 0;
    }
}

void zap_wand(int slot)
{
    if (you.species == SP_FELID || !form_can_use_wand())
    {
        mpr("You have no means to grasp a wand firmly enough.");
        return;
    }

    bolt beam;
    dist zap_wand;
    int item_slot;

    // Unless the character knows the type of the wand, the targetting
    // system will default to enemies. -- [ds]
    targ_mode_type targ_mode = TARG_HOSTILE;

    beam.obvious_effect = false;
    beam.beam_source = MHITYOU;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Zap which item?",
                                       MT_INVLIST,
                                       OBJ_WANDS,
                                       true, true, true, 0, -1, NULL,
                                       OPER_ZAP);
    }

    if (prompt_failed(item_slot))
        return;

    item_def& wand = you.inv[item_slot];
    if (wand.base_type != OBJ_WANDS)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // If you happen to be wielding the wand, its display might change.
    if (you.equip[EQ_WEAPON] == item_slot)
        you.wield_change = true;

    const zap_type type_zapped = wand.zap();

    bool has_charges = true;
    if (wand.plus < 1)
    {
        if (wand.plus2 == ZAPCOUNT_EMPTY)
        {
            mpr("This wand has no charges.");
            return;
        }
        has_charges = false;
    }

    const bool alreadyknown = item_type_known(wand);
    const bool alreadytried = item_type_tried(wand);
          bool invis_enemy  = false;
    const bool dangerous    = player_in_a_dangerous_place(&invis_enemy);
    targetter *hitfunc      = 0;

    if (!alreadyknown)
        beam.effect_known = false;
    else
    {
        switch (wand.sub_type)
        {
        case WAND_DIGGING:
        case WAND_TELEPORTATION:
            targ_mode = TARG_ANY;
            break;

        case WAND_HEAL_WOUNDS:
            if (you_worship(GOD_ELYVILON))
            {
                targ_mode = TARG_ANY;
                break;
            }
            // else intentional fall-through
        case WAND_HASTING:
        case WAND_INVISIBILITY:
            targ_mode = TARG_FRIEND;
            break;

        default:
            targ_mode = TARG_HOSTILE;
            break;
        }

        hitfunc = _wand_targetter(&wand);
    }

    const int tracer_range =
        (alreadyknown && wand.sub_type != WAND_RANDOM_EFFECTS) ?
        _wand_range(type_zapped) : _max_wand_range();
    const string zap_title =
        "Zapping: " + get_menu_colour_prefix_tags(wand, DESC_INVENTORY);
    direction_chooser_args args;
    args.mode = targ_mode;
    args.range = tracer_range;
    args.top_prompt = zap_title;
    args.hitfunc = hitfunc;
    direction(zap_wand, args);

    if (hitfunc)
        delete hitfunc;

    if (!zap_wand.isValid)
    {
        if (zap_wand.isCancel)
            canned_msg(MSG_OK);
        return;
    }

    if (alreadyknown && zap_wand.target == you.pos())
    {
        if (wand.sub_type == WAND_TELEPORTATION
            && you.no_tele(false, false))
        {
            mpr("You cannot teleport right now.");
            return;
        }
        else if (wand.sub_type == WAND_INVISIBILITY
                 && _dont_use_invis())
        {
            return;
        }
    }

    if (!has_charges)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        // It's an empty wand; inscribe it that way.
        wand.plus2 = ZAPCOUNT_EMPTY;
        you.turn_is_over = true;
        return;
    }

    if (you.confused())
        zap_wand.confusion_fuzz();

    if (wand.sub_type == WAND_RANDOM_EFFECTS)
        beam.effect_known = false;

    beam.source   = you.pos();
    beam.attitude = ATT_FRIENDLY;
    beam.set_target(zap_wand);

    const bool aimed_at_self = (beam.target == you.pos());
    const int power = 15 + you.skill(SK_EVOCATIONS, 5) / 2;

    // Check whether we may hit friends, use "safe" values for random effects
    // and unknown wands (highest possible range, and unresistable beam
    // flavour). Don't use the tracer if firing at self.
    if (!aimed_at_self)
    {
        beam.range = tracer_range;
        if (!player_tracer(beam.effect_known ? type_zapped
                                             : ZAP_DEBUGGING_RAY,
                           power, beam, beam.effect_known ? 0 : 17))
        {
            return;
        }
    }

    // Zapping the wand isn't risky if you aim it away from all monsters
    // and yourself, unless there's a nearby invisible enemy and you're
    // trying to hit it at random.
    const bool risky = dangerous && (beam.friend_info.count
                                     || beam.foe_info.count
                                     || invis_enemy
                                     || aimed_at_self);

    if (risky && alreadyknown && wand.sub_type == WAND_RANDOM_EFFECTS)
    {
        // Xom loves it when you use a Wand of Random Effects and
        // there is a dangerous monster nearby...
        xom_is_stimulated(200);
    }

    // Reset range.
    beam.range = _wand_range(type_zapped);

#ifdef WIZARD
    if (you.wizard)
    {
        string str = wand.inscription;
        int wiz_range = strip_number_tag(str, "range:");
        if (wiz_range != TAG_UNFOUND)
            beam.range = wiz_range;
    }
#endif

    // zapping() updates beam.
    zapping(type_zapped, power, beam);

    // Take off a charge.
    wand.plus--;

    // Zap counts count from the last recharge.
    if (wand.plus2 == ZAPCOUNT_RECHARGED)
        wand.plus2 = 0;
    // Increment zap count.
    if (wand.plus2 >= 0)
        wand.plus2++;

    // Identify if necessary.
    if (!alreadyknown && (beam.obvious_effect || type_zapped == ZAP_FIREBALL))
    {
        set_ident_type(wand, ID_KNOWN_TYPE);
        if (wand.sub_type == WAND_RANDOM_EFFECTS)
            mpr("You feel that this wand is rather unreliable.");

        mpr_nocap(wand.name(DESC_INVENTORY_EQUIP).c_str());
    }
    else
        set_ident_type(wand, ID_TRIED_TYPE);

    if (item_type_known(wand)
        && (item_ident(wand, ISFLAG_KNOW_PLUSES)
            || you.skill(SK_EVOCATIONS, 10) > 50 + random2(141)))
    {
        if (!item_ident(wand, ISFLAG_KNOW_PLUSES))
        {
            mpr("Your skill with magical items lets you calculate "
                "the power of this device...");
        }

        mprf("This wand has %d charge%s left.",
             wand.plus, wand.plus == 1 ? "" : "s");

        set_ident_flags(wand, ISFLAG_KNOW_PLUSES);
    }
    // Mark as empty if necessary.
    if (wand.plus == 0 && wand.flags & ISFLAG_KNOW_PLUSES)
        wand.plus2 = ZAPCOUNT_EMPTY;

    practise(EX_DID_ZAP_WAND);
    count_action(CACT_EVOKE, EVOC_WAND);
    alert_nearby_monsters();

    if (!alreadyknown && !alreadytried && risky)
    {
        // Xom loves it when you use an unknown wand and there is a
        // dangerous monster nearby...
        xom_is_stimulated(200);
    }

    you.turn_is_over = true;
}

void prompt_inscribe_item()
{
    if (inv_count() < 1)
    {
        mpr("You don't have anything to inscribe.");
        return;
    }

    int item_slot = prompt_invent_item("Inscribe which item?",
                                       MT_INVLIST, OSEL_ANY);

    if (prompt_failed(item_slot))
        return;

    inscribe_item(you.inv[item_slot], true);
}

static void _vampire_corpse_help()
{
    if (you.species != SP_VAMPIRE)
        return;

    if (check_blood_corpses_on_ground() || count_corpses_in_pack(true) > 0)
        mpr("If it's blood you're after, try <w>e</w>.");
}

void drink(int slot)
{
    if (you_foodless(true))
    {
        if (you.form == TRAN_TREE)
            mpr("It'd take too long for a potion to reach your roots.");
        else
            mpr("You can't drink.");
        return;
    }

    if (slot == -1)
    {
        const dungeon_feature_type feat = grd(you.pos());
        if (feat >= DNGN_FOUNTAIN_BLUE && feat <= DNGN_FOUNTAIN_BLOOD)
            if (_drink_fountain())
                return;
    }

    if (inv_count() == 0)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        _vampire_corpse_help();
        return;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.form == TRAN_BAT)
    {
        canned_msg(MSG_PRESENT_FORM);
        _vampire_corpse_help();
        return;
    }

    if (you.duration[DUR_RETCHING])
    {
        mpr("You can't gag anything down in your present state!");
        return;
    }

    if (slot == -1)
    {
        slot = prompt_invent_item("Drink which item?",
                                  MT_INVLIST, OBJ_POTIONS,
                                  true, true, true, 0, -1, NULL,
                                  OPER_QUAFF);
        if (prompt_failed(slot))
        {
            _vampire_corpse_help();
            return;
        }
    }

    item_def& potion = you.inv[slot];

    if (potion.base_type != OBJ_POTIONS)
    {
        if (you.species == SP_VAMPIRE && potion.base_type == OBJ_CORPSES)
            eat_food(slot);
        else
            mpr("You can't drink that!");
        return;
    }

    const bool alreadyknown = item_type_known(potion);

    if (alreadyknown && you.hunger_state == HS_ENGORGED
        && (is_blood_potion(potion) || potion.sub_type == POT_PORRIDGE))
    {
        mpr("You are much too full right now.");
        return;
    }

    if (alreadyknown && potion.sub_type == POT_INVISIBILITY
        && _dont_use_invis())
    {
        return;
    }

    if (alreadyknown && potion.sub_type == POT_BERSERK_RAGE
        && (!berserk_check_wielded_weapon()
            || !you.can_go_berserk(true, true)))
    {
        return;
    }

    zin_recite_interrupt();

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous = (player_in_a_dangerous_place()
                            && you.experience_level > 1);
    potion_type pot_type = (potion_type)potion.sub_type;

    // Identify item and type.
    if (potion_effect(static_cast<potion_type>(potion.sub_type),
                      40, true, alreadyknown))
    {
        set_ident_flags(potion, ISFLAG_IDENT_MASK);
        set_ident_type(potion, ID_KNOWN_TYPE);
        mpr("It was a " + potion.name(DESC_QUALNAME) + ".");
    }
    else if (!alreadyknown)
    {
        // Because all potions are identified upon quaffing we never come here.
        set_ident_type(potion, ID_TRIED_TYPE);
    }

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you drink an unknown potion and there is
        // a dangerous monster nearby...
        xom_is_stimulated(200);
    }

    if (is_blood_potion(potion))
    {
        // Always drink oldest potion.
        remove_oldest_blood_potion(potion);
    }

    dec_inv_item_quantity(slot, 1);
    count_action(CACT_USE, OBJ_POTIONS);
    you.turn_is_over = true;

    if (you.species != SP_VAMPIRE)
        lessen_hunger(40, true);

    // This got deferred from the it_use2 switch to prevent SIGHUP abuse.
    if (pot_type == POT_EXPERIENCE)
        level_change();
}

static bool _drink_fountain()
{
    const dungeon_feature_type feat = grd(you.pos());

    if (feat < DNGN_FOUNTAIN_BLUE || feat > DNGN_FOUNTAIN_BLOOD)
        return false;

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return true;
    }

    potion_type fountain_effect = NUM_POTIONS;
    if (feat == DNGN_FOUNTAIN_BLUE)
    {
        if (!yesno("Drink from the fountain?", true, 'n'))
            return false;

        mpr("You drink the pure, clear water.");
    }
    else if (feat == DNGN_FOUNTAIN_BLOOD)
    {
        if (!yesno("Drink from the fountain of blood?", true, 'n'))
            return false;

        mpr("You drink the blood.");
        fountain_effect = POT_BLOOD;
    }
    else
    {
        if (!yesno("Drink from the sparkling fountain?", true, 'n'))
            return false;

        mpr("You drink the sparkling water.");

        fountain_effect =
            random_choose_weighted(467, NUM_POTIONS,
                                   48,  POT_DECAY,
                                   40,  POT_MUTATION,
                                   40,  POT_CURING,
                                   40,  POT_HEAL_WOUNDS,
                                   40,  POT_SPEED,
                                   40,  POT_MIGHT,
                                   40,  POT_AGILITY,
                                   40,  POT_BRILLIANCE,
                                   32,  POT_DEGENERATION,
                                   27,  POT_FLIGHT,
                                   27,  POT_POISON,
                                   27,  POT_SLOWING,
                                   27,  POT_PARALYSIS,
                                   27,  POT_CONFUSION,
                                   27,  POT_INVISIBILITY,
                                   20,  POT_MAGIC,
                                   20,  POT_RESTORE_ABILITIES,
                                   20,  POT_RESISTANCE,
                                   20,  POT_STRONG_POISON,
                                   20,  POT_BERSERK_RAGE,
                                   12,  POT_BENEFICIAL_MUTATION,
                                   0);
    }

    if (fountain_effect != NUM_POTIONS && fountain_effect != POT_BLOOD)
        xom_is_stimulated(50);

    // Good gods do not punish for bad random effects. However, they do
    // punish drinking from a fountain of blood.
    potion_effect(fountain_effect, 100, true, feat != DNGN_FOUNTAIN_SPARKLING, true);

    bool gone_dry = false;
    if (feat == DNGN_FOUNTAIN_BLUE)
    {
        if (one_chance_in(20))
            gone_dry = true;
    }
    else if (feat == DNGN_FOUNTAIN_BLOOD)
    {
        // High chance of drying up, to prevent abuse.
        if (one_chance_in(3))
            gone_dry = true;
    }
    else   // sparkling fountain
    {
        if (one_chance_in(10))
            gone_dry = true;
        else if (random2(50) > 40)
        {
            // Turn fountain into a normal fountain without any message
            // but the glyph colour gives it away (lightblue vs. blue).
            grd(you.pos()) = DNGN_FOUNTAIN_BLUE;
            set_terrain_changed(you.pos());
        }
    }

    if (gone_dry)
    {
        mpr("The fountain dries up!");

        grd(you.pos()) = static_cast<dungeon_feature_type>(feat
                         + DNGN_DRY_FOUNTAIN_BLUE - DNGN_FOUNTAIN_BLUE);

        set_terrain_changed(you.pos());

        crawl_state.cancel_cmd_repeat();
    }

    you.turn_is_over = true;
    return true;
}

static void _explosion(coord_def where, actor *agent, beam_type flavour,
                       int colour, string name, string cause)
{
    bolt beam;
    beam.is_explosion = true;
    beam.aux_source = cause;
    beam.source = where;
    beam.target = where;
    beam.set_agent(agent);
    beam.range = 0;
    beam.damage = dice_def(5, 8);
    beam.ex_size = 5;
    beam.flavour = flavour;
    beam.colour = colour;
    beam.hit = AUTOMATIC_HIT;
    beam.name = name;
    beam.loudness = 10;
    beam.explode(true, false);
}

// XXX: Only checks brands that can be rebranded to,
// there's probably a nicer way of doing this.
static bool _god_hates_brand(const int brand)
{
    if (is_good_god(you.religion)
        && (brand == SPWPN_DRAINING
            || brand == SPWPN_VAMPIRICISM
            || brand == SPWPN_CHAOS))
    {
        return true;
    }

    if (you_worship(GOD_SHINING_ONE) && brand == SPWPN_VENOM)
        return true;

    if (you_worship(GOD_CHEIBRIADOS) && brand == SPWPN_CHAOS)
        return true;

    return false;
}

static void _rebrand_weapon(item_def& wpn)
{
    const int old_brand = get_weapon_brand(wpn);
    int new_brand = old_brand;
    const string itname = wpn.name(DESC_YOUR);

    // you can't rebrand blessed weapons but trying will get you some cleansing flame
    switch (wpn.sub_type)
    {
        case WPN_BLESSED_FALCHION:
        case WPN_BLESSED_LONG_SWORD:
        case WPN_BLESSED_SCIMITAR:
        case WPN_EUDEMON_BLADE:
        case WPN_BLESSED_DOUBLE_SWORD:
        case WPN_BLESSED_GREAT_SWORD:
        case WPN_BLESSED_TRIPLE_SWORD:
        case WPN_SACRED_SCOURGE:
        case WPN_TRISHULA:
            return;
    }

    // now try and find an appropriate brand
    while (old_brand == new_brand || _god_hates_brand(new_brand))
    {
        if (is_range_weapon(wpn))
        {
            new_brand = random_choose_weighted(
                                    30, SPWPN_FLAME,
                                    30, SPWPN_FROST,
                                    20, SPWPN_VENOM,
                                    20, SPWPN_VORPAL,
                                    12, SPWPN_EVASION,
                                    5, SPWPN_ELECTROCUTION,
                                    3, SPWPN_CHAOS,
                                    0);
        }
        else
        {
            new_brand = random_choose_weighted(
                                    30, SPWPN_FLAMING,
                                    30, SPWPN_FREEZING,
                                    20, SPWPN_VENOM,
                                    15, SPWPN_DRAINING,
                                    15, SPWPN_VORPAL,
                                    15, SPWPN_ELECTROCUTION,
                                    12, SPWPN_PROTECTION,
                                    8, SPWPN_VAMPIRICISM,
                                    3, SPWPN_CHAOS,
                                    0);
        }
    }

    set_item_ego_type(wpn, OBJ_WEAPONS, new_brand);

    if (old_brand == SPWPN_DISTORTION)
    {
        // you can't get rid of distortion this easily
        mprf("%s twongs alarmingly.", itname.c_str());

        // from unwield_item
        MiscastEffect(&you, NON_MONSTER, SPTYP_TRANSLOCATION, 9, 90,
                      "distortion unbrand");
    }
}

static void _vorpalise_weapon(bool already_known)
{
    mpr("Vorpalise which weapon?", MSGCH_PROMPT);
    // Prompt before showing the weapon list.
    if (Options.auto_list)
        more();

    wield_weapon(false);

    if (!you.weapon())
    {
        mpr("You are not wielding a weapon.");
        return;
    }

    // Check if you're wielding a brandable weapon.
    item_def& wpn = *you.weapon();
    if (wpn.base_type != OBJ_WEAPONS || wpn.sub_type == WPN_BLOWGUN
        || is_artefact(wpn))
    {
        mpr("You can't vorpalise that.");
        return;
    }

    you.wield_change = true;

    // If there's no brand, make it vorpal.
    if (get_weapon_brand(wpn) == SPWPN_NORMAL)
    {
        alert_nearby_monsters();
        mprf("%s emits a brilliant flash of light!",
             wpn.name(DESC_YOUR).c_str());
        set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_VORPAL);
        return;
    }

    // If there's a permanent brand, try to rebrand it.
    bool rebranded = false;
    if (you.duration[DUR_WEAPON_BRAND] == 0)
    {
        rebranded = true;
        _rebrand_weapon(wpn);
    }

    // There's a temporary or new brand, attempt to make it permanent.
    const string itname = wpn.name(DESC_YOUR);
    bool success = true;

    switch (get_weapon_brand(wpn))
    {
    case SPWPN_VORPAL:
    case SPWPN_PROTECTION:
    case SPWPN_EVASION:
        if (rebranded)
        {
            alert_nearby_monsters();
            mprf("%s emits a brilliant flash of light!",itname.c_str());
        }
        else // should be VORPAL only
        {
            if (get_vorpal_type(wpn) != DVORP_CRUSHING)
                mprf("%s's sharpness seems more permanent.", itname.c_str());
            else
                mprf("%s's heaviness feels very stable.", itname.c_str());
        }
        break;

    case SPWPN_FLAME:
    case SPWPN_FLAMING:
        mprf("%s is engulfed in an explosion of fire!", itname.c_str());
        immolation(10, IMMOLATION_AFFIX, already_known);
        break;

    case SPWPN_FROST:
    case SPWPN_FREEZING:
        if (cast_los_attack_spell(SPELL_OZOCUBUS_REFRIGERATION, 60,
                                  (already_known) ? &you : NULL, true)
            != SPRET_SUCCESS)
        {
            canned_msg(MSG_OK);
            success = false;
        }
        else
            mprf("%s is covered with a thin layer of ice!", itname.c_str());
        break;

    case SPWPN_DRAINING:
    case SPWPN_VAMPIRICISM:
        mprf("%s thirsts for the lives of mortals!", itname.c_str());
        drain_exp(true, 100);
        break;

    case SPWPN_VENOM:
        if (rebranded)
            mprf("%s drips with poison.", itname.c_str());
        else
            mprf("%s seems more permanently poisoned.", itname.c_str());
        toxic_radiance_effect(&you, 1);
        break;

    case SPWPN_ELECTROCUTION:
        mprf("%s releases a massive orb of lightning.", itname.c_str());
        _explosion(you.pos(), &you, BEAM_ELECTRICITY, LIGHTCYAN, "electricity",
                   "electrocution affixation");
        break;

    case SPWPN_CHAOS:
        mprf("%s erupts in a glittering mayhem of all colours.", itname.c_str());
        // need to affix it immediately, otherwise transformation will break it
        you.duration[DUR_WEAPON_BRAND] = 0;
        xom_is_stimulated(200);
        _explosion(you.pos(), &you, BEAM_CHAOS, BLACK, "chaos eruption", "chaos affixation");
        switch (random2(coinflip() ? 2 : 4))
        {
        case 3:
            if (transform(50, coinflip() ? TRAN_PIG :
                              coinflip() ? TRAN_DRAGON :
                                           TRAN_BAT))
            {
                // after getting possibly banished, we don't want you to just
                // say "end transformation" immediately
                you.transform_uncancellable = true;
                break;
            }
        case 2:
            if (you.can_safely_mutate())
            {
                // not funny on the undead
                mutate(RANDOM_MUTATION, "chaos affixation");
                break;
            }
        case 1:
            xom_acts(coinflip(), HALF_MAX_PIETY, 0); // ignore tension
        default:
            break;
        }
        break;

    case SPWPN_PAIN:
        // Can't fix pain brand (balance)...you just get tormented.
        mprf("%s shrieks out in agony!", itname.c_str());

        torment_monsters(you.pos(), &you, TORMENT_GENERIC);
        success = false;

        // This is only naughty if you know you're doing it.
        // XXX: assumes this can only happen from Vorpalise Weapon scroll.
        did_god_conduct(DID_NECROMANCY, 10,
                        get_ident_type(OBJ_SCROLLS, SCR_VORPALISE_WEAPON)
                        == ID_KNOWN_TYPE);
        break;

    case SPWPN_DISTORTION:
        // [dshaligram] Attempting to fix a distortion brand gets you a free
        // distortion effect, and no permabranding. Sorry, them's the breaks.
        mprf("%s twongs alarmingly.", itname.c_str());

        // from unwield_item
        MiscastEffect(&you, NON_MONSTER, SPTYP_TRANSLOCATION, 9, 90,
                      "distortion affixation");
        success = false;
        break;

    case SPWPN_ANTIMAGIC:
        mprf("%s repels your magic.", itname.c_str());
        drain_mp(you.species == SP_DJINNI ? 100 : you.magic_points);
        success = false;
        break;

    case SPWPN_HOLY_WRATH:
        mprf("%s emits a blast of cleansing flame.", itname.c_str());
        _explosion(you.pos(), &you, BEAM_HOLY, YELLOW, "cleansing flame",
                   rebranded ? "holy wrath rebrand" : "holy wrath affixation");
        success = false;
        break;

    default:
        success = false;
        break;
    }

    if (success)
    {
        you.duration[DUR_WEAPON_BRAND] = 0;
        item_set_appearance(wpn);
        // Might be rebranding to/from protection or evasion.
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        // Might be removing antimagic.
        calc_mp();
    }
    return;
}

bool enchant_weapon(item_def &wpn, int acc, int dam, const char *colour)
{
    bool success = false;

    // Get item name now before changing enchantment.
    string iname = wpn.name(DESC_YOUR);
    const char *s = wpn.quantity == 1 ? "s" : "";

    // Blowguns only have one stat.
    if (wpn.base_type == OBJ_WEAPONS && wpn.sub_type == WPN_BLOWGUN)
    {
        acc = acc + dam;
        dam = 0;
    }

    if (is_weapon(wpn))
    {
        if (!is_artefact(wpn) && wpn.base_type == OBJ_WEAPONS)
        {
            while (acc--)
            {
                if (wpn.plus < 4 || !x_chance_in_y(wpn.plus, MAX_WPN_ENCHANT))
                    wpn.plus++, success = true;
            }
            while (dam--)
            {
                if (wpn.plus2 < 4 || !x_chance_in_y(wpn.plus2, MAX_WPN_ENCHANT))
                    wpn.plus2++, success = true;
            }
            if (success && colour)
                mprf("%s glow%s %s for a moment.", iname.c_str(), s, colour);
        }
        if (wpn.cursed())
        {
            if (!success && colour)
            {
                if (const char *space = strchr(colour, ' '))
                    colour = space + 1;
                mprf("%s glow%s silvery %s for a moment.", iname.c_str(), s, colour);
            }
            do_uncurse_item(wpn, true, true);
            success = true;
        }
    }

    if (!success && colour)
    {
        if (!wpn.defined())
            iname = "Your " + you.hand_name(true);
        mprf("%s very briefly gain%s a %s sheen.", iname.c_str(), s, colour);
    }

    if (success)
        you.wield_change = true;

    return success;
}

static void _handle_enchant_weapon(int acc, int dam, const char *colour)
{
    item_def nothing, *weapon = you.weapon();
    if (!weapon)
        weapon = &nothing;
    enchant_weapon(*weapon, acc, dam, colour);
}

bool enchant_armour(int &ac_change, bool quiet, item_def &arm)
{
    ASSERT(arm.defined() && arm.base_type == OBJ_ARMOUR);

    ac_change = 0;

    // Cannot be enchanted nor uncursed.
    if (!is_enchantable_armour(arm, true))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_HAPPENS);

        return false;
    }

    const bool is_cursed = arm.cursed();

    // Turn hides into mails where applicable.
    // NOTE: It is assumed that armour which changes in this way does
    // not change into a form of armour with a different evasion modifier.
    if (armour_is_hide(arm, false))
    {
        if (!quiet)
        {
            mprf("%s glows purple and changes!",
                 arm.name(DESC_YOUR).c_str());
        }

        ac_change = property(arm, PARM_AC);
        hide2armour(arm);
        ac_change = property(arm, PARM_AC) - ac_change;

        do_uncurse_item(arm, true, true);

        // No additional enchantment.
        return true;
    }

    // Even if not affected, it may be uncursed.
    if (!is_enchantable_armour(arm, false))
    {
        if (is_cursed)
        {
            if (!quiet)
            {
                mprf("%s glows silver for a moment.",
                     arm.name(DESC_YOUR).c_str());
            }

            do_uncurse_item(arm, true, true);
            return true;
        }
        else
        {
            if (!quiet)
                canned_msg(MSG_NOTHING_HAPPENS);

            return false;
        }
    }

    // Output message before changing enchantment and curse status.
    if (!quiet)
    {
        mprf("%s glows green for a moment.",
             arm.name(DESC_YOUR).c_str());
    }

    arm.plus++;
    ac_change++;
    do_uncurse_item(arm, true, true);

    return true;
}

static int _handle_enchant_armour(int item_slot, string *pre_msg)
{
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Enchant which item?", MT_INVLIST,
                                           OSEL_ENCH_ARM, true, true, false);
        }
        if (prompt_failed(item_slot))
            return -1;

        item_def& arm(you.inv[item_slot]);

        if (!is_enchantable_armour(arm, true, true))
        {
            mpr("Choose some type of armour to enchant, or Esc to abort.");
            if (Options.auto_list)
                more();

            item_slot = -1;
            continue;
        }

        // Okay, we may actually (attempt to) enchant something.
        if (pre_msg)
            mpr(pre_msg->c_str());

        int ac_change;
        bool result = enchant_armour(ac_change, false, arm);

        if (ac_change)
            you.redraw_armour_class = true;

        return result ? 1 : 0;
    }
    while (true);

    return 0;
}

static void _handle_read_book(int item_slot)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.stat_zero[STAT_INT])
    {
        mpr("Reading books requires mental cohesion, which you lack.");
        return;
    }

    item_def& book(you.inv[item_slot]);
    ASSERT(book.sub_type != BOOK_MANUAL);

    if (book.sub_type == BOOK_DESTRUCTION)
    {
        if (silenced(you.pos()))
            mpr("This book does not work if you cannot read it aloud!");
        else
            tome_of_power(item_slot);
        return;
    }

    while (true)
    {
        // Spellbook
        const int ltr = read_book(book, RBOOK_READ_SPELL);

        if (ltr < 'a' || ltr > 'h')     //jmf: was 'g', but 8=h
        {
            mesclr();
            return;
        }

        const spell_type spell = which_spell_in_book(book,
                                                     letter_to_index(ltr));
        if (spell == SPELL_NO_SPELL)
        {
            mesclr();
            return;
        }

        describe_spell(spell, &book);

        // Player memorised spell which was being looked at.
        if (you.turn_is_over)
            return;
    }
}

// For unidentified scrolls of recharging, identify and enchant armour
// offer full choice of inventory and only identify the scroll if you chose
// something that is affected by the scroll. Once they're identified, you'll
// get the limited inventory listing.
// Returns true if the scroll had an obvious effect and should be identified.
static bool _scroll_modify_item(item_def scroll)
{
    ASSERT(scroll.base_type == OBJ_SCROLLS);

    int item_slot;

retry:
    do
    {
        // Get the slot of the item the scroll is to be used on.
        // Ban the scroll's own slot from the prompt to avoid the stupid situation
        // where you use identify on itself.
        item_slot = prompt_invent_item("Use on which item? (\\ to view known items)",
                                       MT_INVLIST, OSEL_SCROLL_TARGET, true, true, false, 0,
                                       scroll.link, NULL, OPER_ANY, true);

        if (item_slot == PROMPT_NOTHING)
            return false;

        if (item_slot == PROMPT_ABORT
            && yesno("Really abort (and waste the scroll)?"))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    while (item_slot < 0);

    item_def &item = you.inv[item_slot];

    if (item_is_melded(you.inv[item_slot]))
    {
        mpr("This item is melded into your body!");
        if (Options.auto_list)
            more();
        goto retry;
    }

    bool show_msg = true;
    const char* id_prop = nullptr;

    switch (scroll.sub_type)
    {
    case SCR_IDENTIFY:
        if (!fully_identified(item) || is_deck(item) && !top_card_is_known(item))
        {
            identify(-1, item_slot);
            return true;
        }
        else
            id_prop = "SCR_ID";
        break;

    case SCR_RECHARGING:
        if (item_is_rechargeable(item) && recharge_wand(item_slot, false))
            return true;
        id_prop = "SCR_RC";
        break;

    case SCR_ENCHANT_ARMOUR:
        if (is_enchantable_armour(item, true))
        {
            // Might still fail because of already high enchantment.
            // (If so, already prints the "Nothing happens" message.)
            if (_handle_enchant_armour(item_slot) > 0)
                return true;
            show_msg = false;
        }

        id_prop = "SCR_EA";
        break;

    default:
        mprf("Buggy scroll %d can't modify item!", scroll.sub_type);
        break;
    }

    if (id_prop)
        you.type_id_props[id_prop] = item.name(DESC_PLAIN, false, false, false);

    // Oops, wrong item...
    if (show_msg)
        canned_msg(MSG_NOTHING_HAPPENS);
    return false;
}

static void _vulnerability_scroll()
{
    // First cast antimagic on yourself.
    antimagic();

    mon_enchant lowered_mr(ENCH_LOWERED_MR, 1, &you, 40);

    // Go over all creatures in LOS.
    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            debuff_monster(mon);

            // If relevant, monsters have their MR halved.
            if (!mons_immune_magic(mon))
                mon->add_ench(lowered_mr);

            // Annoying but not enough to turn friendlies against you.
            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, &you);
        }
    }

    you.set_duration(DUR_LOWERED_MR, 40, 0,
                     "Magic dampens, then quickly surges around you.");
}

bool _is_cancellable_scroll(scroll_type scroll)
{
    return (scroll == SCR_IDENTIFY
            || scroll == SCR_BLINKING
            || scroll == SCR_RECHARGING
            || scroll == SCR_ENCHANT_ARMOUR
            || scroll == SCR_AMNESIA
            || scroll == SCR_REMOVE_CURSE
            || scroll == SCR_CURSE_ARMOUR
            || scroll == SCR_CURSE_JEWELLERY);
}

void read_scroll(int slot)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return;
    }

    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
    {
        mpr("You cannot read scrolls while unable to breathe!");
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    int item_slot = (slot != -1) ? slot
                                 : prompt_invent_item("Read which item?",
                                                       MT_INVLIST,
                                                       OBJ_SCROLLS,
                                                       true, true, true, 0, -1,
                                                       NULL, OPER_READ);

    if (prompt_failed(item_slot))
        return;

    item_def& scroll = you.inv[item_slot];

    if ((scroll.base_type != OBJ_BOOKS || scroll.sub_type == BOOK_MANUAL)
        && scroll.base_type != OBJ_SCROLLS)
    {
        mpr("You can't read that!");
        crawl_state.zero_turns_taken();
        return;
    }

    // Here we try to read a book {dlb}:
    if (scroll.base_type == OBJ_BOOKS)
    {
        _handle_read_book(item_slot);
        return;
    }

    if (you.form == TRAN_WISP)
    {
        crawl_state.zero_turns_taken();
        return mpr("You have no means to unroll scrolls.");
    }

    if (silenced(you.pos()))
    {
        mpr("Magic scrolls do not work when you're silenced!");
        crawl_state.zero_turns_taken();
        return;
    }

    // Prevent hot lava orcs reading scrolls
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_NO_SCROLLS))
    {
        crawl_state.zero_turns_taken();
        return mpr("You'd burn any scroll you tried to read!");
    }

    const scroll_type which_scroll = static_cast<scroll_type>(scroll.sub_type);
    const bool alreadyknown = item_type_known(scroll);

    if (alreadyknown)
    {
        switch (which_scroll)
        {
        case SCR_BLINKING:
        case SCR_TELEPORTATION:
            if (you.no_tele(false, false, which_scroll == SCR_BLINKING))
            {
                mpr("You cannot teleport right now.");
                return;
            }
            break;

        case SCR_ENCHANT_ARMOUR:
            if (!any_items_to_select(OSEL_ENCH_ARM, true))
                return;
            break;

        case SCR_ENCHANT_WEAPON_I:
        case SCR_ENCHANT_WEAPON_II:
        case SCR_ENCHANT_WEAPON_III:
            if (you.weapon() && is_weapon(*you.weapon())
                && !you.weapon()->cursed()
                && (is_artefact(*you.weapon())
                    || you.weapon()->base_type != OBJ_WEAPONS))
            {
                mpr("This weapon cannot be enchanted.");
                return;
            }
            else if (!you.weapon() || !is_weapon(*you.weapon()))
            {
                mpr("You are not wielding a weapon.");
                return;
            }
            break;

        case SCR_IDENTIFY:
            if (!any_items_to_select(OSEL_UNIDENT, true))
                return;
            break;

        case SCR_RECHARGING:
            if (!any_items_to_select(OSEL_RECHARGE, true))
                return;
            break;

        case SCR_AMNESIA:
            if (you.spell_no == 0)
            {
                canned_msg(MSG_NO_SPELLS);
                return;
            }
            break;

        case SCR_REMOVE_CURSE:
            if (!any_items_to_select(OSEL_CURSED_WORN, true))
                return;
            break;

        case SCR_CURSE_ARMOUR:
            if (you_worship(GOD_ASHENZARI)
                && !any_items_to_select(OSEL_UNCURSED_WORN_ARMOUR, true))
            {
                return;
            }
            break;

        case SCR_CURSE_JEWELLERY:
            if (you_worship(GOD_ASHENZARI)
                && !any_items_to_select(OSEL_UNCURSED_WORN_JEWELLERY, true))
            {
                return;
            }
            break;

        default:
            break;
        }
    }

    // Ok - now we FINALLY get to read a scroll !!! {dlb}
    you.turn_is_over = true;

    zin_recite_interrupt();

    // ... but some scrolls may still be cancelled afterwards.
    bool cancel_scroll = false;

    if (you.stat_zero[STAT_INT] && !one_chance_in(5))
    {
        // mpr("You stumble in your attempt to read the scroll. Nothing happens!");
        // mpr("Your reading takes too long for the scroll to take effect.");
        // mpr("Your low mental capacity makes reading really difficult. You give up!");
        mpr("You almost manage to decipher the scroll, but fail in this attempt.");
        return;
    }

    // Imperfect vision prevents players from reading actual content {dlb}:
    if (player_mutation_level(MUT_BLURRY_VISION)
        && x_chance_in_y(player_mutation_level(MUT_BLURRY_VISION), 5))
    {
        mpr((player_mutation_level(MUT_BLURRY_VISION) == 3 && one_chance_in(3))
                        ? "This scroll appears to be blank."
                        : "The writing blurs in front of your eyes.");
        return;
    }

    // For cancellable scrolls leave printing this message to their
    // respective functions.
    string pre_succ_msg =
            make_stringf("As you read the %s, it crumbles to dust.",
                          scroll.name(DESC_QUALNAME).c_str());
    if (which_scroll != SCR_IMMOLATION && !_is_cancellable_scroll(which_scroll))
    {
        mpr(pre_succ_msg.c_str());
        // Actual removal of scroll done afterwards. -- bwr
    }

    const bool dangerous = player_in_a_dangerous_place();

    // It is the exception, not the rule, that the scroll will not
    // be identified. {dlb}
    bool id_the_scroll = true;  // to prevent unnecessary repetition
    bool tried_on_item = false; // used to modify item (?EA, ?RC, ?ID)

    bool bad_effect = false; // for Xom: result is bad (or at least dangerous)

    int prev_quantity = you.inv[item_slot].quantity;

    switch (which_scroll)
    {
    case SCR_RANDOM_USELESSNESS:
        random_uselessness(item_slot);
        break;

    case SCR_BLINKING:
        // XXX Because some checks in blink() are made before players get to
        // choose target location it is possible to "abuse" scrolls' free
        // cancelling to get some normally hidden information (i.e. presence
        // of (unidentified) -Tele gear).
        if (!alreadyknown)
        {
            mpr(pre_succ_msg.c_str());
            blink(1000, false);
        }
        else
            cancel_scroll = (blink(1000, false, false, &pre_succ_msg) == -1);
        break;

    case SCR_TELEPORTATION:
        you_teleport();
        break;

    case SCR_REMOVE_CURSE:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            id_the_scroll = remove_curse(false);
        }
        else
            cancel_scroll = !remove_curse(true, &pre_succ_msg);
        break;

    case SCR_ACQUIREMENT:
        mpr("This is a scroll of acquirement!");
        more();
        // Identify it early in case the player checks the '\' screen.
        set_ident_type(scroll, ID_KNOWN_TYPE);
        run_uncancel(UNC_ACQUIREMENT, AQ_SCROLL);
        break;

    case SCR_FEAR:
        mpr("You assume a fearsome visage.");
        mass_enchantment(ENCH_FEAR, 1000);
        break;

    case SCR_NOISE:
        noisy(25, you.pos(), "You hear a loud clanging noise!");
        break;

    case SCR_SUMMONING:
        cast_shadow_creatures(true);
        break;

    case SCR_FOG:
        mpr("The scroll dissolves into smoke.");
        big_cloud(random_smoke_type(), &you, you.pos(), 50, 8 + random2(8));
        break;

    case SCR_MAGIC_MAPPING:
        magic_mapping(500, 90 + random2(11), false);
        break;

    case SCR_TORMENT:
        torment(&you, TORMENT_SCROLL, you.pos());

        // This is only naughty if you know you're doing it.
        did_god_conduct(DID_NECROMANCY, 10, item_type_known(scroll));
        bad_effect = true;
        break;

    case SCR_IMMOLATION:
        mprf("The scroll explodes in your %s!", you.hand_name(true).c_str());

        // Doesn't destroy scrolls anymore, so no special check needed. (jpeg)
        immolation(10, IMMOLATION_SCROLL, alreadyknown);
        bad_effect = true;
        more();
        break;

    case SCR_CURSE_WEAPON:
        if (!you.weapon()
            || !is_weapon(*you.weapon())
            || you.weapon()->cursed())
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        else
        {
            // Also sets wield_change.
            do_curse_item(*you.weapon(), false);
            learned_something_new(HINT_YOU_CURSED);
            bad_effect = true;
        }
        break;

    case SCR_ENCHANT_WEAPON_I:
        _handle_enchant_weapon(1, 0, "green");
        break;

    case SCR_ENCHANT_WEAPON_II:
        _handle_enchant_weapon(0, 1, "red");
        break;

    case SCR_ENCHANT_WEAPON_III:
        _handle_enchant_weapon(1 + random2(2), 1 + random2(2), "bright yellow");
        break;

    case SCR_VORPALISE_WEAPON:
        if (!alreadyknown)
            mpr("It is a scroll of vorpalise weapon.");
        _vorpalise_weapon(alreadyknown);
        break;

    case SCR_IDENTIFY:
        if (!item_type_known(scroll))
        {
            mpr(pre_succ_msg.c_str());
            id_the_scroll = _scroll_modify_item(scroll);
            if (!id_the_scroll)
                tried_on_item = true;
        }
        else
            cancel_scroll = (identify(-1, -1, &pre_succ_msg) == 0);
        break;

    case SCR_RECHARGING:
        if (!item_type_known(scroll))
        {
            mpr(pre_succ_msg.c_str());
            id_the_scroll = _scroll_modify_item(scroll);
            if (!id_the_scroll)
                tried_on_item = true;
        }
        else
            cancel_scroll = (recharge_wand(-1, true, &pre_succ_msg) == -1);
        break;

    case SCR_ENCHANT_ARMOUR:
        if (!item_type_known(scroll))
        {
            mpr(pre_succ_msg.c_str());
            id_the_scroll = _scroll_modify_item(scroll);
            if (!id_the_scroll)
                tried_on_item = true;
        }
        else
            cancel_scroll = (_handle_enchant_armour(-1, &pre_succ_msg) == -1);
        break;

    case SCR_CURSE_ARMOUR:
    case SCR_CURSE_JEWELLERY:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            if (curse_item(which_scroll == SCR_CURSE_ARMOUR, false))
                bad_effect = true;
            else
                id_the_scroll = false;
        }
        else if (curse_item(which_scroll == SCR_CURSE_ARMOUR, true,
                            &pre_succ_msg))
        {
            bad_effect = true;
        }
        else
            cancel_scroll = you_worship(GOD_ASHENZARI);

        break;

    case SCR_HOLY_WORD:
    {
        int pow = 100;

        if (is_good_god(you.religion))
        {
            pow += (you_worship(GOD_SHINING_ONE)) ? you.piety :
                                                       you.piety / 2;
        }

        holy_word(pow, HOLY_WORD_SCROLL, you.pos(), false, &you);

        // This is always naughty, even if you didn't affect anyone.
        // Don't speak those foul holy words even in jest!
        did_god_conduct(DID_HOLY, 10, item_type_known(scroll));
        break;
    }

    case SCR_SILENCE:
        cast_silence(30);
        break;

    case SCR_VULNERABILITY:
        _vulnerability_scroll();
        break;

    case SCR_AMNESIA:
        if (!alreadyknown)
            mpr(pre_succ_msg.c_str());
        if (you.spell_no == 0)
            mpr("You feel forgetful for a moment.");
        else if (!alreadyknown)
            cast_selective_amnesia();
        else
            cancel_scroll = (cast_selective_amnesia(&pre_succ_msg) == -1);
        break;

    default:
        mpr("Read a buggy scroll, please report this.");
        break;
    }

    if (cancel_scroll)
        you.turn_is_over = false;

    set_ident_type(scroll, id_the_scroll ? ID_KNOWN_TYPE :
                           tried_on_item ? ID_TRIED_ITEM_TYPE
                                         : ID_TRIED_TYPE);

    // Finally, destroy and identify the scroll.
    if (id_the_scroll)
        set_ident_flags(scroll, ISFLAG_KNOW_TYPE); // for notes

    string scroll_name = scroll.name(DESC_QUALNAME).c_str();

    if (!cancel_scroll)
    {
        dec_inv_item_quantity(item_slot, 1);
        count_action(CACT_USE, OBJ_SCROLLS);
    }

    if (id_the_scroll
        && !alreadyknown
        && which_scroll != SCR_ACQUIREMENT
        && which_scroll != SCR_VORPALISE_WEAPON)
    {
        mprf("It %s a %s.",
             you.inv[item_slot].quantity < prev_quantity ? "was" : "is",
             scroll_name.c_str());
    }

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you read an unknown scroll and there is a
        // dangerous monster nearby... (though not as much as potions
        // since there are no *really* bad scrolls, merely useless ones).
        xom_is_stimulated(bad_effect ? 100 : 50);
    }
}

void examine_object(void)
{
    int item_slot = prompt_invent_item("Examine which item?",
                                        MT_INVLIST, -1,
                                        true, true, true, 0, -1, NULL,
                                        OPER_EXAMINE);
    if (prompt_failed(item_slot))
        return;

    describe_item(you.inv[item_slot], true);
    redraw_screen();
    mesclr();
}

bool stasis_blocks_effect(bool calc_unid,
                          bool identify,
                          const char *msg, int noise,
                          const char *silenced_msg)
{
    if (you.stasis(calc_unid))
    {
        item_def *amulet = you.slot_item(EQ_AMULET, false);

        // Just in case a non-amulet stasis source is added.
        if (amulet && amulet->sub_type != AMU_STASIS)
            amulet = 0;

        if (msg)
        {
            const string name(amulet? amulet->name(DESC_YOUR) : "Something");
            const string message = make_stringf(msg, name.c_str());

            if (noise)
            {
                if (!noisy(noise, you.pos(), message.c_str())
                    && silenced_msg)
                {
                    mprf(silenced_msg, name.c_str());
                }
            }
            else
                mpr(message.c_str());
        }

        // In all cases, the amulet auto-ids if requested.
        if (amulet && identify && !item_type_known(*amulet))
            wear_id_type(*amulet);
        return true;
    }
    return false;
}

item_def* get_only_unided_ring()
{
    item_def* found = NULL;

    for (int i = EQ_LEFT_RING; i <= EQ_RING_EIGHT; i++)
    {
        if (i == EQ_AMULET)
            continue;

        if (!player_wearing_slot(i))
            continue;

        item_def& item = you.inv[you.equip[i]];
        if (!item_type_known(item))
        {
            if (found)
            {
                // Both rings are unid'd, so return NULL.
                return NULL;
            }
            found = &item;
        }
    }
    return found;
}

#ifdef USE_TILE
// Interactive menu for item drop/use.

void tile_item_use_floor(int idx)
{
    if (mitm[idx].base_type == OBJ_CORPSES
        && mitm[idx].sub_type != CORPSE_SKELETON
        && !food_is_rotten(mitm[idx]))
    {
        butchery(idx);
    }
}

void tile_item_pickup(int idx, bool part)
{
    if (part)
    {
        pickup_menu(idx);
        return;
    }
    pickup_single_item(idx, -1);
}

void tile_item_drop(int idx, bool partdrop)
{
    int quantity = you.inv[idx].quantity;
    if (partdrop && quantity > 1)
    {
        quantity = prompt_for_int("Drop how many? ", true);
        if (quantity < 1)
        {
            canned_msg(MSG_OK);
            return;
        }
        if (quantity > you.inv[idx].quantity)
            quantity = you.inv[idx].quantity;
    }
    drop_item(idx, quantity);
}

static bool _prompt_eat_bad_food(const item_def food)
{
    if (food.base_type != OBJ_CORPSES
        && (food.base_type != OBJ_FOOD || food.sub_type != FOOD_CHUNK))
    {
        return true;
    }

    if (!is_bad_food(food))
        return true;

    const string food_colour = item_prefix(food);
    string colour            = "";
    string colour_off        = "";

    const int col = menu_colour(food.name(DESC_A), food_colour, "pickup");
    if (col != -1)
        colour = colour_to_str(col);

    if (!colour.empty())
    {
        // Order is important here.
        colour_off  = "</" + colour + ">";
        colour      = "<" + colour + ">";
    }

    const string qualifier = colour
                             + (is_poisonous(food)      ? "poisonous" :
                                is_mutagenic(food)      ? "mutagenic" :
                                causes_rot(food)        ? "rot-inducing" :
                                is_forbidden_food(food) ? "forbidden" : "")
                             + colour_off;

    string prompt  = "Really ";
           prompt += (you.species == SP_VAMPIRE ? "drink from" : "eat");
           prompt += " this " + qualifier;
           prompt += (food.base_type == OBJ_CORPSES ? " corpse"
                                                    : " chunk of meat");
           prompt += "?";

    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    return true;
}

void tile_item_eat_floor(int idx)
{
    if (mitm[idx].base_type == OBJ_CORPSES
            && you.species == SP_VAMPIRE
        || mitm[idx].base_type == OBJ_FOOD
            && you.is_undead != US_UNDEAD && you.species != SP_VAMPIRE)
    {
        if (can_ingest(mitm[idx], false)
            && _prompt_eat_bad_food(mitm[idx]))
        {
            eat_floor_item(idx);
        }
    }
}

void tile_item_use_secondary(int idx)
{
    const item_def item = you.inv[idx];

    if (item.base_type == OBJ_WEAPONS && is_throwable(&you, item))
    {
        if (check_warning_inscriptions(item, OPER_FIRE))
            fire_thing(idx); // fire weapons
    }
    else if (you.equip[EQ_WEAPON] == idx)
        wield_weapon(true, SLOT_BARE_HANDS);
    else if (_valid_weapon_swap(item))
    {
        // secondary wield for several spells and such
        wield_weapon(true, idx); // wield
    }
}

void tile_item_use(int idx)
{
    const item_def item = you.inv[idx];

    // Equipped?
    bool equipped = false;
    bool equipped_weapon = false;
    for (unsigned int i = 0; i < NUM_EQUIP; i++)
    {
        if (you.equip[i] == idx)
        {
            equipped = true;
            if (i == EQ_WEAPON)
                equipped_weapon = true;
            break;
        }
    }

    // Special case for folks who are wielding something
    // that they shouldn't be wielding.
    // Note that this is only a problem for equipables
    // (otherwise it would only waste a turn)
    if (you.equip[EQ_WEAPON] == idx
        && (item.base_type == OBJ_ARMOUR
            || item.base_type == OBJ_JEWELLERY))
    {
        wield_weapon(true, SLOT_BARE_HANDS);
        return;
    }

    const int type = item.base_type;

    // Use it
    switch (type)
    {
        case OBJ_WEAPONS:
        case OBJ_STAVES:
        case OBJ_RODS:
        case OBJ_MISCELLANY:
        case OBJ_WANDS:
            // Wield any unwielded item of these types.
            if (!equipped && item_is_wieldable(item))
            {
                wield_weapon(true, idx);
                return;
            }
            // Evoke misc. items, rods, or wands.
            if (item_is_evokable(item, false))
            {
                evoke_item(idx);
                return;
            }
            // Unwield wielded items.
            if (equipped)
                wield_weapon(true, SLOT_BARE_HANDS);
            return;

        case OBJ_MISSILES:
            if (check_warning_inscriptions(item, OPER_FIRE))
                fire_thing(idx);
            return;

        case OBJ_ARMOUR:
            if (!form_can_wear())
            {
                mpr("You can't wear or remove anything in your present form.");
                return;
            }
            if (equipped && !equipped_weapon)
            {
                if (check_warning_inscriptions(item, OPER_TAKEOFF))
                    takeoff_armour(idx);
            }
            else if (check_warning_inscriptions(item, OPER_WEAR))
                wear_armour(idx);
            return;

        case OBJ_CORPSES:
            if (you.species != SP_VAMPIRE
                || item.sub_type == CORPSE_SKELETON
                || food_is_rotten(item))
            {
                break;
            }
            // intentional fall-through for Vampires
        case OBJ_FOOD:
            if (check_warning_inscriptions(item, OPER_EAT)
                && _prompt_eat_bad_food(item))
            {
                eat_food(idx);
            }
            return;

        case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
                return;
            if (!item_is_spellbook(item) || !you.skill(SK_SPELLCASTING))
            {
                if (check_warning_inscriptions(item, OPER_READ))
                    _handle_read_book(idx);
            } // else it's a spellbook
            else if (check_warning_inscriptions(item, OPER_MEMORISE))
                learn_spell(); // offers all spells, might not be what we want
            return;

        case OBJ_SCROLLS:
            if (check_warning_inscriptions(item, OPER_READ))
                read_scroll(idx);
            return;

        case OBJ_JEWELLERY:
            if (equipped && !equipped_weapon)
            {
                if (check_warning_inscriptions(item, OPER_REMOVE))
                    remove_ring(idx);
            }
            else if (check_warning_inscriptions(item, OPER_PUTON))
                puton_ring(idx);
            return;

        case OBJ_POTIONS:
            if (check_warning_inscriptions(item, OPER_QUAFF))
                drink(idx);
            return;

        default:
            return;
    }
}
#endif
