/**
 * @file
 * @brief Functions for making use of inventory items.
**/

#include "AppHdr.h"

#include "item_use.h"

#include "ability.h"
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "art-enum.h"
#include "butcher.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "exercise.h"
#include "food.h"
#include "godabil.h"
#include "godconduct.h"
#include "goditem.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mutation.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "prompt.h"
#include "religion.h"
#include "rot.h"
#include "shout.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-goditem.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-wpnench.h"
#include "spl-zap.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "throw.h"
#include "transform.h"
#include "uncancel.h"
#include "unwind.h"
#include "view.h"
#include "xom.h"

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

    if (player_mutation_level(MUT_MISSING_HAND)
            && you.hands_reqd(*weapon) == HANDS_TWO)
    {
        SAY(mpr("You can't wield that without your missing limb."));
        return false;
    }

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
    {
        if (you.equip[i] != -1 && &you.inv[you.equip[i]] == weapon)
        {
            SAY(mpr("You are wearing that object!"));
            return false;
        }
    }

    if (!you.could_wield(*weapon, true, true, !say_reason))
        return false;

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

    bool id_brand = false;

    if (you.undead_or_demonic() && is_holy_item(*weapon)
        && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("This weapon is holy and will not allow you to wield it.");
            id_brand = true;
        }
    }
    else if (!ignore_temporary_disability
             && you.hunger_state < HS_FULL
             && get_weapon_brand(*weapon) == SPWPN_VAMPIRISM
             && !crawl_state.game_is_zotdef()
             && you.undead_state() == US_ALIVE
             && !you_foodless()
             && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("This weapon is vampiric, and you must be Full or above to equip it.");
            id_brand = true;
        }
    }
#if TAG_MAJOR_VERSION == 34
    else if (you.species == SP_DJINNI
             && get_weapon_brand(*weapon) == SPWPN_ANTIMAGIC
             && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("As you grasp it, you feel your magic disrupted. Quickly, you stop.");
            id_brand = true;
        }
    }
#endif

    if (id_brand)
    {
        if (!is_artefact(*weapon) && !is_blessed(*weapon)
            && !item_type_known(*weapon))
        {
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
            if (in_inventory(*weapon))
                mprf_nocap("%s", weapon->name(DESC_INVENTORY_EQUIP).c_str());
        }
        else if (is_artefact(*weapon) && !item_type_known(*weapon))
            artefact_learn_prop(*weapon, ARTP_BRAND);
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

/**
 * @param force If true, don't check weapon inscriptions.
 * (Assuming the player was already prompted for that.)
 */
bool wield_weapon(bool auto_wield, int slot, bool show_weff_messages,
                  bool force, bool show_unwield_msg, bool show_wield_msg)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return false;
    }

    // Look for conditions like berserking that could prevent wielding
    // weapons.
    if (!can_wield(nullptr, true, false, slot == SLOT_BARE_HANDS))
        return false;

    int item_slot = 0;          // default is 'a'

    if (auto_wield)
    {
        if (item_slot == you.equip[EQ_WEAPON]
            || you.equip[EQ_WEAPON] == -1
               && !item_is_wieldable(you.inv[item_slot]))
        {
            item_slot = 1;      // backup is 'b'
        }

        if (slot != -1)         // allow external override
            item_slot = slot;
    }

    // If the swap slot has a bad (but valid) item in it,
    // the swap will be to bare hands.
    const bool good_swap = (item_slot == SLOT_BARE_HANDS
                            || item_is_wieldable(you.inv[item_slot]));

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
                            true, true, true, '-', -1, nullptr, OPER_WIELD);
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
            bool penance = false;
            // Can we safely unwield this item?
            if (needs_handle_warning(*wpn, OPER_WIELD, penance))
            {
                string prompt =
                    "Really unwield " + wpn->name(DESC_INVENTORY) + "?";
                if (penance)
                    prompt += " This could place you under penance!";

                if (!yesno(prompt.c_str(), false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return false;
                }
            }

            // check if you'd get stat-zeroed
            if (!_safe_to_remove_or_wear(*wpn, true))
                return false;

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

    // Really ensure wieldable, even unknown brand
    if (!can_wield(&new_wpn, true, false, false, false))
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

    const unsigned int old_talents = your_talents(false).size();

    // Go ahead and wield the weapon.
    equip_item(EQ_WEAPON, item_slot, show_weff_messages);

    if (show_wield_msg)
        mprf_nocap("%s", new_wpn.name(DESC_INVENTORY_EQUIP).c_str());

    check_item_hint(new_wpn, old_talents);

    // Time calculations.
    you.time_taken /= 2;

    you.wield_change  = true;
    you.m_quiver->on_weapon_changed();
    you.turn_is_over  = true;

    return true;
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
    ASSERT(index != nullptr);

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
                                      true, true, true, 0, -1, nullptr,
                                      oper);

        if (!prompt_failed(slot))
        {
            *index = slot;
            return true;
        }
    }

    return false;
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

/**
 * The number of turns it takes to put on or take off a given piece of armour.
 *
 * @param item      The armour in question.
 * @return          The number of turns it takes to don or doff the item.
 */
static int armour_equip_delay(const item_def &item)
{
    return 5;
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

    if (player_mutation_level(MUT_MISSING_HAND) && slot == EQ_SHIELD)
    {
        if (verbose)
            mpr("You don't have enough limbs.");
        return false;
    }

    // Lear's hauberk covers also head, hands and legs.
    if (is_unrandom_artefact(item, UNRAND_LEAR))
    {
        if (!player_has_feet(!ignore_temporary))
        {
            if (verbose)
                mpr("You have no feet.");
            return false;
        }

        if (ignore_temporary)
        {
            // Hooves and talons were already checked by player_has_feet.

            if (species_has_claws(you.species) >= 3
                || player_mutation_level(MUT_CLAWS, false) >= 3)
            {
                if (verbose)
                {
                    mprf("The hauberk won't fit your %s.",
                         you.hand_name(true).c_str());
                }
                return false;
            }

            if (player_mutation_level(MUT_HORNS, false) >= 3
                || player_mutation_level(MUT_ANTENNAE, false) >= 3)
            {
                if (verbose)
                    mpr("The hauberk won't fit your head.");
                return false;
            }
        }
        else
        {
            for (int s = EQ_HELMET; s <= EQ_BOOTS; s++)
            {
                // No strange race can wear this.
                const string parts[] = { "head", you.hand_name(true),
                                         you.foot_name(true) };
                COMPILE_CHECK(ARRAYSZ(parts) == EQ_BOOTS - EQ_HELMET + 1);

                // Auto-disrobing would be nice.
                if (you.equip[s] != -1)
                {
                    if (verbose)
                    {
                        mprf("You'd need your %s free.",
                             parts[s - EQ_HELMET].c_str());
                    }
                    return false;
                }

                if (!you_tran_can_wear(s, true))
                {
                    if (verbose)
                    {
                        mprf(you_tran_can_wear(s)
                                ? "The hauberk won't fit your %s."
                                : "You have no %s!",
                             parts[s - EQ_HELMET].c_str());
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
        {
            mprf("This armour is too %s for you!",
                 (bad_size > 0) ? "big" : "small");
        }

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
            {
                mprf("You can't wear a glove with your huge claw%s!",
                     player_mutation_level(MUT_MISSING_HAND) ? "" : "s");
            }
            return false;
        }
    }

    if (sub_type == ARM_BOOTS)
    {
        if (player_mutation_level(MUT_HOOVES, false) == 3)
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

        if (you.species == SP_NAGA
#if TAG_MAJOR_VERSION == 34
            || you.species == SP_DJINNI
#endif
           )
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
        if (player_mutation_level(MUT_HORNS, false) == 3)
        {
            if (verbose)
                mpr("You can't wear any headgear with your large horns!");
            return false;
        }

        if (player_mutation_level(MUT_ANTENNAE, false) == 3)
        {
            if (verbose)
                mpr("You can't wear any headgear with your large antennae!");
            return false;
        }

        // Soft helmets (caps and wizard hats) always fit, otherwise.
        if (is_hard_helmet(item))
        {
            if (player_mutation_level(MUT_HORNS, false))
            {
                if (verbose)
                    mpr("You can't wear that with your horns!");
                return false;
            }

            if (player_mutation_level(MUT_BEAK, false))
            {
                if (verbose)
                    mpr("You can't wear that with your beak!");
                return false;
            }

            if (player_mutation_level(MUT_ANTENNAE, false))
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

    if (player_mutation_level(MUT_MISSING_HAND) && is_shield(invitem))
    {
        if (!quiet)
        {
            if (you.species == SP_OCTOPODE)
                mpr("You need the rest of your tentacles for walking.");
            else
                mprf("You'd need another %s to do that!", you.hand_name(false).c_str());
        }
        return false;
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
            {
                // Singular hand should have already been handled above.
                mprf("You'd need three %s to do that!",
                     you.hand_name(true).c_str());
            }
        }
        return false;
    }

    bool swapping = false;
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
        swapping = true;
    }

    you.turn_is_over = true;

    if (!_safe_to_remove_or_wear(invitem, false))
        return false;

    const int delay = armour_equip_delay(invitem);
    if (delay)
        start_delay(DELAY_ARMOUR_ON, delay - (swapping ? 0 : 1), item);

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


    switch (slot)
    {
    case EQ_BODY_ARMOUR:
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

    you.turn_is_over = true;

    const int delay = armour_equip_delay(invitem);
    start_delay(DELAY_ARMOUR_OFF, delay - 1, item);

    return true;
}

// Returns a list of possible ring slots.
static vector<equipment_type> _current_ring_types()
{
    vector<equipment_type> ret;
    if (you.species == SP_OCTOPODE)
    {
        for (int i = 0; i < 8; ++i)
        {
            const equipment_type slot = (equipment_type)(EQ_RING_ONE + i);

            if (player_mutation_level(MUT_MISSING_HAND)
                && slot == EQ_RING_EIGHT)
            {
                continue;
            }

            if (get_form()->slot_available(slot))
                ret.push_back(slot);
        }
    }
    else
    {
        if (player_mutation_level(MUT_MISSING_HAND) == 0)
            ret.push_back(EQ_LEFT_RING);
        ret.push_back(EQ_RIGHT_RING);
    }
    if (player_equip_unrand(UNRAND_FINGER_AMULET))
        ret.push_back(EQ_RING_AMULET);
    return ret;
}

static vector<equipment_type> _current_jewellery_types()
{
    vector<equipment_type> ret = _current_ring_types();
    ret.push_back(EQ_AMULET);
    return ret;
}

static const char _ring_slot_key(equipment_type slot)
{
    switch (slot)
    {
    case EQ_LEFT_RING:      return '<';
    case EQ_RIGHT_RING:     return '>';
    case EQ_RING_AMULET:    return '^';
    case EQ_RING_ONE:       return '1';
    case EQ_RING_TWO:       return '2';
    case EQ_RING_THREE:     return '3';
    case EQ_RING_FOUR:      return '4';
    case EQ_RING_FIVE:      return '5';
    case EQ_RING_SIX:       return '6';
    case EQ_RING_SEVEN:     return '7';
    case EQ_RING_EIGHT:     return '8';
    default:
        die("Invalid ring slot");
    }
}

static int _prompt_ring_to_remove(int new_ring)
{
    const vector<equipment_type> ring_types = _current_ring_types();
    vector<char> slot_chars;
    vector<item_def*> rings;
    for (auto eq : ring_types)
    {
        rings.push_back(you.slot_item(eq, true));
        ASSERT(rings.back());
        slot_chars.push_back(index_to_letter(rings.back()->link));
    }

    clear_messages();

    mprf(MSGCH_PROMPT,
         "You're wearing all the rings you can. Remove which one?");
    mprf(MSGCH_PROMPT, "(<w>?</w> for menu, <w>Esc</w> to cancel)");

    // FIXME: Needs TOUCH_UI version

    for (size_t i = 0; i < rings.size(); i++)
    {
        string m = "<w>";
        const char key = _ring_slot_key(ring_types[i]);
        m += key;
        if (key == '<')
            m += '<';

        m += "</w> or " + rings[i]->name(DESC_INVENTORY);
        mprf_nocap("%s", m.c_str());
    }
    flush_prev_message();

    // Deactivate choice from tile inventory.
    // FIXME: We need to be able to get the choice (item letter)n
    //        *without* the choice taking action by itself!
    int eqslot = EQ_NONE;

    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
    {
        c = getchm();
        for (size_t i = 0; i < slot_chars.size(); i++)
        {
            if (c == slot_chars[i]
                || c == _ring_slot_key(ring_types[i]))
            {
                eqslot = ring_types[i];
                c = ' ';
                break;
            }
        }
    } while (!key_is_escape(c) && c != ' ' && c != '?');

    clear_messages();

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
        prop_str += artefact_known_property(item, ARTP_STRENGTH);
        prop_int += artefact_known_property(item, ARTP_INTELLIGENCE);
        prop_dex += artefact_known_property(item, ARTP_DEXTERITY);
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
            verb = "Remov"; // -ing, not a typo
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
         inf.is_type(OBJ_JEWELLERY, RING_FLIGHT)
         || inf.base_type == OBJ_ARMOUR && inf.special == SPARM_FLYING
         || is_artefact(inf)
            && artefact_known_property(inf, ARTP_FLY);

    // assumes item can't grant flight twice
    const bool removing_ends_flight = you.flight_mode()
          && !you.racial_permanent_flight()
          && !you.attribute[ATTR_FLIGHT_UNCANCELLABLE]
          && (you.evokable_flight() == 1);

    const dungeon_feature_type feat = grd(you.pos());

    if (grants_flight && removing_ends_flight
        && is_feat_dangerous(feat, false, true))
    {
        if (!quiet)
            mpr("Losing flight right now would be fatal!");
        return false;
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
    vector<equipment_type> ring_types = _current_ring_types();
    const int num_rings = ring_types.size();
    int unwanted = 0;
    int last_inscribed = 0;
    int cursed = 0;
    int inscribed = 0;
    int melded = 0; // Both melded rings and unavailable slots.
    int available = 0;
    bool all_same = true;
    item_def* first_ring = nullptr;
    for (auto eq : ring_types)
    {
        item_def* ring = you.slot_item(eq, true);
        if (!you_tran_can_wear(eq) || you.melded[eq])
            melded++;
        else if (ring != nullptr)
        {
            if (first_ring == nullptr)
                first_ring = ring;
            else if (all_same)
            {
                if (ring->sub_type != first_ring->sub_type
                    || ring->plus  != first_ring->plus
                    || is_artefact(*ring) || is_artefact(*first_ring))
                {
                    all_same = false;
                }
            }

            if (ring->cursed())
                cursed++;
            else if (strstr(ring->inscription.c_str(), "=R"))
            {
                inscribed++;
                last_inscribed = you.equip[eq];
            }
            else
            {
                available++;
                unwanted = you.equip[eq];
            }
        }
    }

    // If the only swappable rings are inscribed =R, go ahead and use them.
    if (available == 0 && inscribed > 0)
    {
        available += inscribed;
        unwanted = last_inscribed;
    }

    // We can't put a ring on, because we're wearing all cursed ones.
    if (melded == num_rings)
    {
        // Shouldn't happen, because hogs and bats can't put on jewellery at
        // all and thus won't get this far.
        mpr("You can't wear that in your present form.");
        return false;
    }
    else if (available == 0)
    {
        mprf("You're already wearing %s cursed ring%s!%s",
             number_in_words(cursed).c_str(),
             (cursed == 1 ? "" : "s"),
             (cursed > 2 ? " Isn't that enough for you?" : ""));
        return false;
    }
    // The simple case - only one available ring.
    // If the jewellery_prompt option is true, always allow choosing the
    // ring slot (even if we still have empty slots).
    else if (available == 1 && !Options.jewellery_prompt)
    {
        if (!remove_ring(unwanted, false))
            return false;
    }
    // We can't put a ring on without swapping - because we found
    // multiple available rings.
    else
    {
        // Don't prompt if all the rings are the same.
        if (!all_same || Options.jewellery_prompt)
            unwanted = _prompt_ring_to_remove(ring_slot);

        // Cancelled:
        if (unwanted < -1)
        {
            canned_msg(MSG_OK);
            return false;
        }

        if (!remove_ring(unwanted, false))
            return false;
    }

    // Put on the new ring.
    start_delay(DELAY_JEWELLERY_ON, 1, ring_slot);

    return true;
}

static equipment_type _choose_ring_slot()
{
    clear_messages();

    mprf(MSGCH_PROMPT,
         "Put ring on which %s? (<w>Esc</w> to cancel)", you.hand_name(false).c_str());

    const vector<equipment_type> slots = _current_ring_types();
    for (auto eq : slots)
    {
        string msg = "<w>";
        const char key = _ring_slot_key(eq);
        msg += key;
        if (key == '<')
            msg += '<';

        item_def* ring = you.slot_item(eq, true);
        if (ring)
            msg += "</w> or " + ring->name(DESC_INVENTORY);
        else
            msg += "</w> - no ring";

        if (eq == EQ_LEFT_RING)
            msg += " (left)";
        else if (eq == EQ_RIGHT_RING)
            msg += " (right)";
        else if (eq == EQ_RING_AMULET)
            msg += " (amulet)";
        mprf_nocap("%s", msg.c_str());
    }
    flush_prev_message();

    equipment_type eqslot = EQ_NONE;
    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
    {
        c = getchm();
        for (auto eq : slots)
        {
            if (c == _ring_slot_key(eq)
                || (you.slot_item(eq, true)
                    && c == index_to_letter(you.slot_item(eq, true)->link)))
            {
                eqslot = eq;
                c = ' ';
                break;
            }
        }
    } while (!key_is_escape(c) && c != ' ');

    clear_messages();

    return eqslot;
}

static bool _puton_item(int item_slot, bool prompt_slot)
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

    const bool is_amulet = jewellery_is_amulet(item);

    if (!you_tran_can_wear(item)
        && (is_amulet || !you_can_wear(EQ_RING_AMULET)))
    {
        mpr("You can't wear that in your present form.");
        return false;
    }

    const vector<equipment_type> ring_types = _current_ring_types();

    if (!is_amulet)     // i.e. it's a ring
    {
        bool need_swap = true;
        for (auto eq : ring_types)
        {
            if (!you.slot_item(eq, true))
            {
                need_swap = false;
                break;
            }
        }

        if (need_swap)
            return _swap_rings(item_slot);
    }
    else if (you.slot_item(EQ_AMULET, true))
    {
        // Remove the previous one.
        if (!remove_ring(you.equip[EQ_AMULET], true))
            return false;

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

    equipment_type hand_used = EQ_NONE;

    if (is_amulet)
        hand_used = EQ_AMULET;
    else if (prompt_slot)
    {
        // Prompt for a slot, even if we have empty ring slots.
        hand_used = _choose_ring_slot();

        if (hand_used == EQ_NONE)
        {
            canned_msg(MSG_OK);
            return false;
        }
        // Allow swapping out a ring.
        else if (you.slot_item(hand_used, true))
        {
            if (!remove_ring(you.equip[hand_used], false))
                return false;

            start_delay(DELAY_JEWELLERY_ON, 1, item_slot);
            return true;
        }
    }
    else
    {
        for (auto eq : ring_types)
        {
            if (!you.slot_item(eq, true))
            {
                hand_used = eq;
                break;
            }
        }
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

    // Putting on jewellery is fast.
    you.time_taken /= 2;
    you.turn_is_over = true;

    return true;
}

bool puton_ring(int slot, bool allow_prompt)
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
                                        true, 0, -1, nullptr, OPER_PUTON);
    }

    if (prompt_failed(item_slot))
        return false;

    bool prompt = allow_prompt ? Options.jewellery_prompt : false;

    return _puton_item(item_slot, prompt);
}

bool remove_ring(int slot, bool announce)
{
    equipment_type hand_used = EQ_NONE;
    int ring_wear_2;
    bool has_jewellery = false;
    bool has_melded = false;
    const vector<equipment_type> ring_types = _current_ring_types();
    const vector<equipment_type> jewellery_slots = _current_jewellery_types();

    for (auto eq : jewellery_slots)
    {
        if (player_wearing_slot(eq))
        {
            if (has_jewellery || Options.jewellery_prompt)
            {
                // At least one other piece, which means we'll have to ask
                hand_used = EQ_NONE;
            }
            else
                hand_used = eq;

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

    if (hand_used == EQ_NONE)
    {
        const int equipn =
            (slot == -1)? prompt_invent_item("Remove which piece of jewellery?",
                                             MT_INVLIST,
                                             OBJ_JEWELLERY, true, true, true,
                                             0, -1, nullptr, OPER_REMOVE,
                                             false, false)
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
    else if (hand_used == EQ_AMULET
        && you.equip[EQ_RING_AMULET] != -1)
    {
        // This can be removed in the future if more ring amulets are added.
        ASSERT(player_equip_unrand(UNRAND_FINGER_AMULET));

        mpr("The amulet cannot be taken off without first removing the ring!");
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

bool dont_use_invis()
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

static bool _check_blood_corpses_on_ground()
{
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->is_type(OBJ_CORPSES, CORPSE_BODY)
            && mons_has_blood(si->mon_type))
        {
            return true;
        }
    }
    return false;
}

static void _vampire_corpse_help()
{
    if (you.species != SP_VAMPIRE)
        return;

    if (_check_blood_corpses_on_ground())
        mpr("Use <w>e</w> to drain blood from corpses.");
}

void drink(int slot)
{
    if (you_foodless(true))
    {
        mpr("You can't drink.");
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

    if (you.duration[DUR_NO_POTIONS])
    {
        mpr("You cannot drink potions in your current state!");
        return;
    }

    if (slot == -1)
    {
        slot = prompt_invent_item("Drink which item?",
                                  MT_INVLIST, OBJ_POTIONS,
                                  true, true, true, 0, -1, nullptr,
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

    // TODO: merge the following checks into potion.cc's can_quaff functions
    const bool alreadyknown = item_type_known(potion);

    if (alreadyknown && you.hunger_state == HS_ENGORGED
        && (is_blood_potion(potion)
#if TAG_MAJOR_VERSION == 34
            || potion.sub_type == POT_PORRIDGE
#endif
            )
        )
    {
        mpr("You are much too full right now.");
        return;
    }

    if (alreadyknown && potion.sub_type == POT_INVISIBILITY
        && dont_use_invis())
    {
        return;
    }

    if (alreadyknown && potion.sub_type == POT_BERSERK_RAGE
        && (!berserk_check_wielded_weapon()
            || !you.can_go_berserk(true, true)))
    {
        return;
    }

    if (alreadyknown && is_blood_potion(potion)
        && is_good_god(you.religion)
        && !yesno("Really drink that potion of blood?", false, 'n'))
    {
        canned_msg(MSG_OK);
        return;
    }

    if (alreadyknown && is_bad_item(potion, true))
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return;
    }

    zin_recite_interrupt();

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous = (player_in_a_dangerous_place()
                            && you.experience_level > 1);
    potion_type pot_type = (potion_type)potion.sub_type;

    if (player_under_penance(GOD_GOZAG) && one_chance_in(3))
    {
        simple_god_message(" petitions for your drink to fail.", GOD_GOZAG);

        you.turn_is_over = true;

        return;
    }

    if (!quaff_potion(potion))
        return;

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you drink an unknown potion and there is
        // a dangerous monster nearby...
        xom_is_stimulated(200);
    }

    if (is_blood_potion(potion))
    {
        // Always drink oldest potion.
        remove_oldest_perishable_item(potion);
    }

    dec_inv_item_quantity(slot, 1);
    count_action(CACT_USE, OBJ_POTIONS);
    auto_assign_item_slot(potion);
    you.turn_is_over = true;

    // This got deferred from the it_use2 switch to prevent SIGHUP abuse.
    if (pot_type == POT_EXPERIENCE)
        level_change();
}

// XXX: Only checks brands that can be rebranded to,
// there's probably a nicer way of doing this.
static bool _god_hates_brand(const int brand)
{
    if (is_good_god(you.religion)
        && (brand == SPWPN_DRAINING
            || brand == SPWPN_VAMPIRISM
            || brand == SPWPN_CHAOS))
    {
        return true;
    }

    if (you_worship(GOD_DITHMENOS)
        && (brand == SPWPN_FLAMING
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
    if (&wpn == you.weapon() && you.duration[DUR_WEAPON_BRAND])
        end_weapon_brand(wpn);
    int old_brand = get_weapon_brand(wpn);
    int new_brand = old_brand;
    const string itname = wpn.name(DESC_YOUR);

    // now try and find an appropriate brand
    while (old_brand == new_brand || _god_hates_brand(new_brand))
    {
        if (is_range_weapon(wpn))
        {
            new_brand = random_choose_weighted(
                                    30, SPWPN_FLAMING,
                                    30, SPWPN_FREEZING,
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
                                    25, SPWPN_VORPAL,
                                    20, SPWPN_VENOM,
                                    15, SPWPN_DRAINING,
                                    15, SPWPN_ELECTROCUTION,
                                    12, SPWPN_PROTECTION,
                                    8, SPWPN_VAMPIRISM,
                                    3, SPWPN_CHAOS,
                                    0);
        }
    }

    set_item_ego_type(wpn, OBJ_WEAPONS, new_brand);
    convert2bad(wpn);
}

static void _brand_weapon(item_def &wpn)
{
    you.wield_change = true;

    const string itname = wpn.name(DESC_YOUR);

    _rebrand_weapon(wpn);

    bool success = true;
    colour_t flash_colour = BLACK;

    switch (get_weapon_brand(wpn))
    {
    case SPWPN_VORPAL:
        flash_colour = YELLOW;
        mprf("%s emits a brilliant flash of light!",itname.c_str());
        break;

    case SPWPN_PROTECTION:
        flash_colour = YELLOW;
        mprf("%s projects an invisible shield of force!",itname.c_str());
        break;

    case SPWPN_EVASION:
        flash_colour = YELLOW;
        mprf("%s emits a repelling force!",itname.c_str());
        break;

    case SPWPN_FLAMING:
        flash_colour = RED;
        mprf("%s is engulfed in flames!", itname.c_str());
        break;

    case SPWPN_FREEZING:
        flash_colour = LIGHTCYAN;
        mprf("%s is covered with a thin layer of ice!", itname.c_str());
        break;

    case SPWPN_DRAINING:
        flash_colour = DARKGREY;
        mprf("%s craves living souls!", itname.c_str());
        break;

    case SPWPN_VAMPIRISM:
        flash_colour = DARKGREY;
        mprf("%s thirsts for the lives of mortals!", itname.c_str());
        break;

    case SPWPN_VENOM:
        flash_colour = GREEN;
        mprf("%s drips with poison.", itname.c_str());
        break;

    case SPWPN_ELECTROCUTION:
        flash_colour = LIGHTCYAN;
        mprf("%s crackles with electricity.", itname.c_str());
        break;

    case SPWPN_CHAOS:
        flash_colour = random_colour();
        mprf("%s erupts in a glittering mayhem of colour.", itname.c_str());
        break;

    default:
        success = false;
        break;
    }

    if (success)
    {
        item_set_appearance(wpn);
        // Message would spoil this even if we didn't identify.
        set_ident_flags(wpn, ISFLAG_KNOW_TYPE);
        mprf_nocap("%s", wpn.name(DESC_INVENTORY_EQUIP).c_str());
        // Might be rebranding to/from protection or evasion.
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        // Might be removing antimagic.
        calc_mp();
        flash_view_delay(UA_PLAYER, flash_colour, 300);
    }
    return;
}

static object_selector _enchant_selector(scroll_type scroll)
{
    if (scroll == SCR_BRAND_WEAPON)
        return OSEL_BRANDABLE_WEAPON;
    else if (scroll == SCR_ENCHANT_WEAPON)
        return OSEL_ENCHANTABLE_WEAPON;
    die("Invalid scroll type %d for _enchant_selector", (int)scroll);
}

// Returns nullptr if no weapon was chosen.
static item_def* _scroll_choose_weapon(bool alreadyknown, const string &pre_msg,
                                       scroll_type scroll)
{
    int item_slot;
    const bool branding = scroll == SCR_BRAND_WEAPON;
    const object_selector selector = _enchant_selector(scroll);

    while (true)
    {
        item_slot = prompt_invent_item(branding ? "Brand which weapon?"
                                                : "Enchant which weapon?",
                                       MT_INVLIST, selector,
                                       true, true, false);

        // The scroll is used up if we didn't know what it was originally.
        if (item_slot == PROMPT_NOTHING)
            return nullptr;

        if (item_slot == PROMPT_ABORT)
        {
            if (alreadyknown
                || crawl_state.seen_hups
                || yesno("Really abort (and waste the scroll)?", false, 0))
            {
                canned_msg(MSG_OK);
                return nullptr;
            }
            else
                continue;
        }

        item_def* wpn = &you.inv[item_slot];

        if (!is_item_selected(*wpn, selector))
        {
            mpr("Choose a valid weapon, or Esc to abort.");
            more();

            continue;
        }

        // Now we're definitely using up the scroll.
        if (alreadyknown)
            mpr(pre_msg);

        return wpn;
    }
}

// Returns true if the scroll is used up.
static bool _handle_brand_weapon(bool alreadyknown, const string &pre_msg)
{
    item_def* weapon = _scroll_choose_weapon(alreadyknown, pre_msg, SCR_BRAND_WEAPON);

    if (!weapon)
        return !alreadyknown;

    _brand_weapon(*weapon);
    return true;
}

bool enchant_weapon(item_def &wpn, bool quiet)
{
    bool success = false;

    // Get item name now before changing enchantment.
    string iname = wpn.name(DESC_YOUR);

    if (is_weapon(wpn))
    {
        if (!is_artefact(wpn)
            && wpn.base_type == OBJ_WEAPONS
            && wpn.plus < MAX_WPN_ENCHANT)
        {
            wpn.plus++;
            success = true;
            if (!quiet)
                mprf("%s glows red for a moment.", iname.c_str());
        }

        if (wpn.cursed())
        {
            if (!success)
                mprf("%s glows silver for a moment.", iname.c_str());
            success = true;
        }
        // Mark the item as uncursed, whether or not it was cursed initially.
        do_uncurse_item(wpn, true, true);
    }

    if (!success && !quiet)
        mprf("%s very briefly gains a red sheen.", iname.c_str());

    if (success)
        you.wield_change = true;

    return success;
}

// Returns true if the scroll is used up.
static bool _identify(bool alreadyknown, const string &pre_msg)
{
    int item_slot = -1;
    while (true)
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item(
                "Identify which item? (\\ to view known items)",
                MT_INVLIST, OSEL_UNIDENT, true, true, false, 0,
                -1, nullptr, OPER_ANY, true);
        }

        if (item_slot == PROMPT_NOTHING)
            return !alreadyknown;

        if (item_slot == PROMPT_ABORT)
        {
            if (alreadyknown
                || crawl_state.seen_hups
                || yesno("Really abort (and waste the scroll)?", false, 0))
            {
                canned_msg(MSG_OK);
                return !alreadyknown;
            }
            else
            {
                item_slot = -1;
                continue;
            }
        }

        item_def& item(you.inv[item_slot]);

        if (fully_identified(item)
            && (!is_deck(item) || top_card_is_known(item)))
        {
            mpr("Choose an unidentified item, or Esc to abort.");
            more();
            item_slot = -1;
            continue;
        }

        if (alreadyknown)
            mpr(pre_msg);

        set_ident_type(item, ID_KNOWN_TYPE);
        set_ident_flags(item, ISFLAG_IDENT_MASK);

        if (is_deck(item) && !top_card_is_known(item))
            deck_identify_first(item_slot);

        // Output identified item.
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
        if (item_slot == you.equip[EQ_WEAPON])
            you.wield_change = true;

        if (item.is_type(OBJ_JEWELLERY, AMU_INACCURACY)
            && item_slot == you.equip[EQ_AMULET]
            && !item_known_cursed(item))
        {
            learned_something_new(HINT_INACCURACY);
        }

        auto_assign_item_slot(item);
        return true;
    }
}

static bool _handle_enchant_weapon(bool alreadyknown, const string &pre_msg)
{
    item_def* weapon = _scroll_choose_weapon(alreadyknown, pre_msg,
                                             SCR_ENCHANT_WEAPON);
    if (!weapon)
        return !alreadyknown;

    enchant_weapon(*weapon, false);
    return true;
}

bool enchant_armour(int &ac_change, bool quiet, item_def &arm)
{
    ASSERT(arm.defined());
    ASSERT(arm.base_type == OBJ_ARMOUR);

    ac_change = 0;

    // Cannot be enchanted nor uncursed.
    if (!is_enchantable_armour(arm, true))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_HAPPENS);

        // That proved that it was uncursed.
        if (!you_worship(GOD_ASHENZARI))
            arm.flags |= ISFLAG_KNOW_CURSE;

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
        if (!quiet)
        {
            if (is_cursed)
            {
                mprf("%s glows silver for a moment.",
                     arm.name(DESC_YOUR).c_str());
            }
            else
                canned_msg(MSG_NOTHING_HAPPENS);
        }
        do_uncurse_item(arm, true, true);
        return is_cursed; // was_cursed, really
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

static int _handle_enchant_armour(bool alreadyknown, const string &pre_msg)
{
    int item_slot = -1;
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Enchant which item?", MT_INVLIST,
                                           OSEL_ENCH_ARM, true, true, false);
        }

        if (item_slot == PROMPT_NOTHING)
            return alreadyknown ? -1 : 0;

        if (item_slot == PROMPT_ABORT)
        {
            if (alreadyknown
                || crawl_state.seen_hups
                || yesno("Really abort (and waste the scroll)?", false, 0))
            {
                canned_msg(MSG_OK);
                return alreadyknown ? -1 : 0;
            }
            else
            {
                item_slot = -1;
                continue;
            }
        }

        item_def& arm(you.inv[item_slot]);

        if (!is_enchantable_armour(arm, true, true))
        {
            mpr("Choose some type of armour to enchant, or Esc to abort.");
            more();

            item_slot = -1;
            continue;
        }

        // Okay, we may actually (attempt to) enchant something.
        if (alreadyknown)
            mpr(pre_msg);

        int ac_change;
        bool result = enchant_armour(ac_change, false, arm);

        if (ac_change)
            you.redraw_armour_class = true;

        return result ? 1 : 0;
    }
    while (true);

    return 0;
}

void random_uselessness(int scroll_slot)
{
    ASSERT(!crawl_state.game_is_arena());

    int temp_rand = random2(8);

    // If this isn't from a scroll, skip the first two possibilities.
    if (scroll_slot == -1)
        temp_rand = 2 + random2(6);

    switch (temp_rand)
    {
    case 0:
        mprf("The dust glows %s!", weird_glowing_colour().c_str());
        break;

    case 1:
        mprf("The scroll reassembles itself in your %s!",
             you.hand_name(true).c_str());
        inc_inv_item_quantity(scroll_slot, 1);
        break;

    case 2:
        if (you.weapon())
        {
            mprf("%s glows %s for a moment.",
                 you.weapon()->name(DESC_YOUR).c_str(),
                 weird_glowing_colour().c_str());
        }
        else
        {
            mpr(you.hands_act("glow", weird_glowing_colour()
                                      + " for a moment."));
        }
        break;

    case 3:
        if (you.species == SP_MUMMY)
            mpr("Your bandages flutter.");
        else // if (you.can_smell())
            mprf("You smell %s.", weird_smell().c_str());
        break;

    case 4:
        mpr("You experience a momentary feeling of inescapable doom!");
        break;

    case 5:
        if (player_mutation_level(MUT_BEAK) || one_chance_in(3))
            mpr("Your brain hurts!");
        else if (you.species == SP_MUMMY || coinflip())
            mpr("Your ears itch!");
        else
            mpr("Your nose twitches suddenly!");
        break;

    case 6:
        mprf(MSGCH_SOUND, "You hear the tinkle of a tiny bell.");
        noisy(2, you.pos());
        cast_summon_butterflies(100);
        break;

    case 7:
        mprf(MSGCH_SOUND, "You hear %s.", weird_sound().c_str());
        noisy(2, you.pos());
        break;
    }
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

#if TAG_MAJOR_VERSION == 34
    if (book.sub_type == BOOK_BUGGY_DESTRUCTION)
    {
        mpr("This item has been removed, sorry!");
        return;
    }
#endif

    read_book(book);
}

static void _vulnerability_scroll()
{
    mon_enchant lowered_mr(ENCH_LOWERED_MR, 1, &you, 400);

    // Go over all creatures in LOS.
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            // If relevant, monsters have their MR halved.
            if (!mons_immune_magic(mon))
                mon->add_ench(lowered_mr);

            // Annoying but not enough to turn friendlies against you.
            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, &you);
        }
    }

    you.set_duration(DUR_LOWERED_MR, 40, 0, "Magic quickly surges around you.");
}

static bool _is_cancellable_scroll(scroll_type scroll)
{
    return scroll == SCR_IDENTIFY
           || scroll == SCR_BLINKING
           || scroll == SCR_RECHARGING
           || scroll == SCR_ENCHANT_ARMOUR
           || scroll == SCR_AMNESIA
           || scroll == SCR_REMOVE_CURSE
           || scroll == SCR_CURSE_ARMOUR
           || scroll == SCR_CURSE_JEWELLERY
           || scroll == SCR_BRAND_WEAPON
           || scroll == SCR_ENCHANT_WEAPON
           || scroll == SCR_MAGIC_MAPPING;
}

/**
 * Is the player currently able to use the 'r' command (to read books or
 * scrolls). Being too berserk, confused, or having no reading material will
 * prevent this.
 *
 * Prints corresponding messages. (Thanks, canned_msg().)
 */
bool player_can_read()
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return false;
    }

    return true;
}

/**
 * If the player has no items matching the given selector, give an appropriate
 * response to print. Otherwise, if they do have such items, return the empty
 * string.
 */
static string _no_items_reason(object_selector type)
{
    if (!any_items_of_type(type))
        return no_selectables_message(type);
    return "";
}

/**
 * If the player is unable to (r)ead the item in the given slot, return the
 * reason why. Otherwise (if they are able to read it), returns "", the empty
 * string.
 */
string cannot_read_item_reason(const item_def &item)
{
    // can read books, except for manuals...
    if (item.base_type == OBJ_BOOKS)
    {
        if (item.sub_type == BOOK_MANUAL)
            return "You can't read that!";
        return "";
    }

    // and scrolls - but nothing else.
    if (item.base_type != OBJ_SCROLLS)
        return "You can't read that!";

    // the below only applies to scrolls. (it's easier to read books, since
    // that's just a UI/strategic thing.)

    if (silenced(you.pos()))
        return "Magic scrolls do not work when you're silenced!";

    // water elementals
    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
        return "You cannot read scrolls while unable to breathe!";

    // ru
    if (you.duration[DUR_NO_SCROLLS])
        return "You cannot read scrolls in your current state!";

#if TAG_MAJOR_VERSION == 34
    // Prevent hot lava orcs reading scrolls
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_NO_SCROLLS))
        return "You'd burn any scroll you tried to read!";
#endif

    // don't waste the player's time reading known scrolls in situations where
    // they'd be useless

    if (!item_type_known(item))
        return "";

    switch (item.sub_type)
    {
        case SCR_BLINKING:
        case SCR_TELEPORTATION:
            return you.no_tele_reason(false, item.sub_type == SCR_BLINKING);

        case SCR_AMNESIA:
            if (you.spell_no == 0)
                return "You have no spells to forget!";
            return "";

        case SCR_CURSE_WEAPON:
            if (!you.weapon())
                return "This scroll only affects a wielded weapon!";

            // assumption: wielded weapons always have their curse & brand known
            if (you.weapon()->cursed())
                return "Your weapon is already cursed!";

            if (get_weapon_brand(*you.weapon()) == SPWPN_HOLY_WRATH)
                return "Holy weapons cannot be cursed!";
            return "";

        case SCR_ENCHANT_ARMOUR:
            return _no_items_reason(OSEL_ENCH_ARM);

        case SCR_ENCHANT_WEAPON:
            return _no_items_reason(OSEL_ENCHANTABLE_WEAPON);

        case SCR_IDENTIFY:
            return _no_items_reason(OSEL_UNIDENT);

        case SCR_RECHARGING:
            return _no_items_reason(OSEL_RECHARGE);

        case SCR_REMOVE_CURSE:
            return _no_items_reason(OSEL_CURSED_WORN);

        case SCR_CURSE_ARMOUR:
            return _no_items_reason(OSEL_UNCURSED_WORN_ARMOUR);

        case SCR_CURSE_JEWELLERY:
            return _no_items_reason(OSEL_UNCURSED_WORN_JEWELLERY);

        default:
            return "";
    }
}

/**
 * Check to see if the player can read the item in the given slot, and if so,
 * reads it. (Examining books, evoking the tome of destruction, & using
 * scrolls.)
 *
 * @param slot      The slot of the item in the player's inventory. If -1, the
 *                  player is prompted to choose a slot.
 */
void read(int slot)
{
    if (!player_can_read())
        return;

    int item_slot = (slot != -1) ? slot
                                 : prompt_invent_item("Read which item?",
                                                       MT_INVLIST,
                                                       OBJ_SCROLLS,
                                                       true, true, true, 0, -1,
                                                       nullptr, OPER_READ);

    if (prompt_failed(item_slot))
        return;

    const item_def& scroll = you.inv[item_slot];
    const string failure_reason = cannot_read_item_reason(scroll);
    if (!failure_reason.empty())
    {
        mprf(MSGCH_PROMPT, "%s", failure_reason.c_str());
        return;
    }

    if (scroll.base_type == OBJ_BOOKS)
    {
        _handle_read_book(item_slot);
        return;
    }

    // Ok - now we FINALLY get to read a scroll !!! {dlb}
    you.turn_is_over = true;

    zin_recite_interrupt();

    if (you.stat_zero[STAT_INT] && !one_chance_in(5))
    {
        mpr("You almost manage to decipher the scroll,"
            " but fail in this attempt.");
        return;
    }

    // if we have blurry vision, we need to start a delay before the actual
    // scroll effect kicks in.
    if (player_mutation_level(MUT_BLURRY_VISION))
    {
        // takes 0.5, 1, 2 extra turns
        const int turns = max(1, player_mutation_level(MUT_BLURRY_VISION) - 1);
        start_delay(DELAY_BLURRY_SCROLL, turns, item_slot);
        if (player_mutation_level(MUT_BLURRY_VISION) == 1)
            you.time_taken /= 2;
    }
    else
        read_scroll(item_slot);
}


/**
 * Read the provided scroll.
 *
 * Does NOT check whether the player can currently read, whether the scroll is
 * currently useless, etc. Likewise doesn't handle blurry vision, setting
 * you.turn_is_over, and other externals. DOES destroy one scroll, unless the
 * player chooses to cancel at the last moment.
 *
 * @param slot      The slot of the item in the player's inventory.
 */
void read_scroll(int item_slot)
{
    item_def& scroll = you.inv[item_slot];
    const scroll_type which_scroll = static_cast<scroll_type>(scroll.sub_type);
    const int prev_quantity = scroll.quantity;
    const bool alreadyknown = item_type_known(scroll);

    // For cancellable scrolls leave printing this message to their
    // respective functions.
    const string pre_succ_msg =
            make_stringf("As you read the %s, it crumbles to dust.",
                          scroll.name(DESC_QUALNAME).c_str());
    if (!_is_cancellable_scroll(which_scroll))
    {
        mpr(pre_succ_msg);
        // Actual removal of scroll done afterwards. -- bwr
    }

    const bool dangerous = player_in_a_dangerous_place();

    // ... but some scrolls may still be cancelled afterwards.
    bool cancel_scroll = false;
    bool bad_effect = false; // for Xom: result is bad (or at least dangerous)

    switch (which_scroll)
    {
    case SCR_RANDOM_USELESSNESS:
        random_uselessness(item_slot);
        break;

    case SCR_BLINKING:
    {
        const string reason = you.no_tele_reason(true, true);
        if (!reason.empty())
        {
            mpr(pre_succ_msg);
            mpr(reason);
            break;
        }

        const bool safely_cancellable
            = alreadyknown && !player_mutation_level(MUT_BLURRY_VISION);
        if (allow_control_teleport())
        {
            cancel_scroll = (cast_controlled_blink(100, false,
                                                   safely_cancellable)
                             == SPRET_ABORT) && alreadyknown;
        }
        else if (alreadyknown
                 && !yesno("Your blink will be uncontrolled - continue anyway?",
                            false, 'n'))
        {
            canned_msg(MSG_OK);
            cancel_scroll = true;
        }
        else
            uncontrolled_blink();

        if (!cancel_scroll)
            mpr(pre_succ_msg); // ordering is iffy but w/e
    }
        break;

    case SCR_TELEPORTATION:
        you_teleport();
        break;

    case SCR_REMOVE_CURSE:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            remove_curse(false);
        }
        else
            cancel_scroll = !remove_curse(true, pre_succ_msg);
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
        cast_shadow_creatures(MON_SUMM_SCROLL);
        break;

    case SCR_FOG:
        mpr("The scroll dissolves into smoke.");
        big_cloud(random_smoke_type(), &you, you.pos(), 50, 8 + random2(8));
        break;

    case SCR_MAGIC_MAPPING:
        if (alreadyknown && testbits(env.level_flags, LFLAG_NO_MAP))
        {
            cancel_scroll = true;
            mpr("It would have no effect in this place.");
            break;
        }
        mpr(pre_succ_msg);
        magic_mapping(500, 100, false);
        break;

    case SCR_TORMENT:
        torment(&you, TORMENT_SCROLL, you.pos());

        // This is only naughty if you know you're doing it.
        did_god_conduct(DID_NECROMANCY, 10, item_type_known(scroll));
        bad_effect = true;
        break;

    case SCR_IMMOLATION:
    {
        // Dithmenos hates trying to play with fire, even if it does nothing.
        did_god_conduct(DID_FIRE, 2 + random2(3), item_type_known(scroll));

        bool had_effect = false;
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (mons_immune_magic(*mi) || mi->is_summoned())
                continue;

            if (mi->add_ench(mon_enchant(ENCH_INNER_FLAME, 0, &you)))
                had_effect = true;
        }

        if (had_effect)
            mpr("The creatures around you are filled with an inner flame!");
        else
            mpr("The air around you briefly surges with heat, but it dissipates.");

        bad_effect = true;
        break;
    }

    case SCR_CURSE_WEAPON:
    {
        // Not you.weapon() because we want to handle melded weapons too.
        item_def * const weapon = you.slot_item(EQ_WEAPON, true);
        if (!weapon || !is_weapon(*weapon) || weapon->cursed())
        {
            bool plural = false;
            const string weapon_name =
                weapon ? weapon->name(DESC_YOUR)
                       : "Your " + you.hand_name(true, &plural);
            mprf("%s very briefly gain%s a black sheen.",
                 weapon_name.c_str(), plural ? "" : "s");
        }
        else
        {
            // Also sets wield_change.
            do_curse_item(*weapon, false);
            learned_something_new(HINT_YOU_CURSED);
            bad_effect = true;
        }
        break;
    }

    case SCR_ENCHANT_WEAPON:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of enchant weapon.");
            // Pause to display the message before jumping to the weapon list.
            more();
        }

        cancel_scroll = !_handle_enchant_weapon(alreadyknown, pre_succ_msg);
        break;

    case SCR_BRAND_WEAPON:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of brand weapon.");
            // Pause to display the message before jumping to the weapon list.
            more();
        }

        cancel_scroll = !_handle_brand_weapon(alreadyknown, pre_succ_msg);
        break;

    case SCR_IDENTIFY:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of identify.");
            more();
            // Do this here so it doesn't turn up in the ID menu.
            set_ident_type(scroll, ID_KNOWN_TYPE);
        }
        cancel_scroll = !_identify(alreadyknown, pre_succ_msg);
        break;

    case SCR_RECHARGING:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of recharging.");
            more();
        }
        cancel_scroll = (recharge_wand(alreadyknown, pre_succ_msg) == -1);
        break;

    case SCR_ENCHANT_ARMOUR:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg);
            mpr("It is a scroll of enchant armour.");
            more();
        }
        cancel_scroll =
            (_handle_enchant_armour(alreadyknown, pre_succ_msg) == -1);
        break;

    // Should always be identified by Ashenzari.
    case SCR_CURSE_ARMOUR:
    case SCR_CURSE_JEWELLERY:
    {
        const bool armour = which_scroll == SCR_CURSE_ARMOUR;
        cancel_scroll = !curse_item(armour, pre_succ_msg);
        break;
    }

    case SCR_HOLY_WORD:
    {
        holy_word(100, HOLY_WORD_SCROLL, you.pos(), false, &you);

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
            mpr(pre_succ_msg);
        if (you.spell_no == 0)
            mpr("You feel forgetful for a moment.");
        else if (!alreadyknown)
            cast_selective_amnesia();
        else
            cancel_scroll = (cast_selective_amnesia(pre_succ_msg) == -1);
        break;

    default:
        mpr("Read a buggy scroll, please report this.");
        break;
    }

    if (cancel_scroll)
        you.turn_is_over = false;

    set_ident_type(scroll, ID_KNOWN_TYPE);
    set_ident_flags(scroll, ISFLAG_KNOW_TYPE); // for notes

    string scroll_name = scroll.name(DESC_QUALNAME).c_str();

    if (!cancel_scroll)
    {
        dec_inv_item_quantity(item_slot, 1);
        count_action(CACT_USE, OBJ_SCROLLS);
    }

    if (!alreadyknown
        && which_scroll != SCR_ACQUIREMENT
        && which_scroll != SCR_BRAND_WEAPON
        && which_scroll != SCR_ENCHANT_WEAPON
        && which_scroll != SCR_IDENTIFY
        && which_scroll != SCR_ENCHANT_ARMOUR
        && which_scroll != SCR_RECHARGING)
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

    if (!alreadyknown)
        auto_assign_item_slot(scroll);

}

bool stasis_blocks_effect(bool calc_unid,
                          const char *msg, int noise,
                          const char *silenced_msg,
                          const char *formicid_msg)
{
    if (you.stasis(calc_unid))
    {
        item_def *amulet = you.slot_item(EQ_AMULET, false);

        // For non-amulet sources of stasis.
        if (amulet && amulet->sub_type != AMU_STASIS)
            amulet = 0;

        if (msg)
        {
            // Override message for formicids
            if (you.species == SP_FORMICID)
            {
                mpr(formicid_msg ? formicid_msg :
                                   "Your stasis keeps you stable.");
            }
            else
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
                    mpr(message);
            }
        }
        return true;
    }
    return false;
}

#ifdef USE_TILE
// Interactive menu for item drop/use.

void tile_item_use_floor(int idx)
{
    if (mitm[idx].is_type(OBJ_CORPSES, CORPSE_BODY))
        butchery(&mitm[idx]);
}

void tile_item_pickup(int idx, bool part)
{
    if (item_is_stationary(mitm[idx]))
    {
        mpr("You can't pick that up.");
        return;
    }

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

void tile_item_eat_floor(int idx)
{
    // XXX: refactor this
    if (mitm[idx].base_type == OBJ_CORPSES
            && you.species == SP_VAMPIRE
        || mitm[idx].base_type == OBJ_FOOD
            && you.undead_state() != US_UNDEAD && you.species != SP_VAMPIRE)
    {
        if (can_eat(mitm[idx], false))
            eat_item(mitm[idx]);
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
    else if (item_is_wieldable(item))
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
                || item.sub_type == CORPSE_SKELETON)
            {
                break;
            }
            // intentional fall-through for Vampires
        case OBJ_FOOD:
            if (check_warning_inscriptions(item, OPER_EAT))
                eat_food(idx);
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
                read(idx);
            return;

        case OBJ_JEWELLERY:
            if (equipped && !equipped_weapon)
                remove_ring(idx);
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
