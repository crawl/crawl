/*
 *  File:       item_use.cc
 *  Summary:    Functions for making use of inventory items.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "item_use.h"

#include <math.h>
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "externs.h"

#include "abl-show.h"
#include "artefact.h"
#include "beam.h"
#include "cio.h"
#include "cloud.h"
#include "colour.h"
#include "command.h"
#include "coordit.h"
#include "debug.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "map_knowledge.h"
#include "fight.h"
#include "food.h"
#include "godabil.h"
#include "goditem.h"
#include "godpassive.h"
#include "invent.h"
#include "it_use2.h"
#include "it_use3.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "los.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-util.h"
#include "mon-place.h"
#include "mutation.h"
#include "terrain.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-stuff.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "player.h"
#include "player-equip.h"
#include "player-stats.h"
#include "quiver.h"
#include "religion.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "teleport.h"
#include "transform.h"
#include "traps.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "xom.h"

static bool _drink_fountain();
static bool _handle_enchant_weapon( enchant_stat_type which_stat,
                                    bool quiet = false, int item_slot = -1 );
static bool _handle_enchant_armour( int item_slot = -1 );

static int  _fire_prompt_for_item();
static bool _fire_validate_item(int selected, std::string& err);

// Rather messy - we've gathered all the can't-wield logic from wield_weapon()
// here.
bool can_wield(item_def *weapon, bool say_reason,
               bool ignore_temporary_disability)
{
#define SAY(x) if (say_reason) { x; } else

    if (!ignore_temporary_disability && you.berserk())
    {
        SAY(canned_msg(MSG_TOO_BERSERK));
        return (false);
    }

    if (!can_equip( EQ_WEAPON, ignore_temporary_disability ))
    {
        SAY(mpr("You can't wield anything in your present form."));
        return (false);
    }

    if (!ignore_temporary_disability
        && you.weapon()
        && you.weapon()->base_type == OBJ_WEAPONS
        && you.weapon()->cursed())
    {
        SAY(mpr("You can't unwield your weapon to draw a new one!"));
        return (false);
    }

    // If we don't have an actual weapon to check, return now.
    if (!weapon)
        return (true);

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
    {
        if (you.equip[i] != -1 && &you.inv[you.equip[i]] == weapon)
        {
            SAY(mpr("You are wearing that object!"));
            return (false);
        }
    }

    // All non-weapons only need a shield check.
    if (weapon->base_type != OBJ_WEAPONS)
    {
        if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
        {
            SAY(mpr("You can't wield that with a shield."));
            return (false);
        }
        else
            return (true);
    }

    if (you.body_size(PSIZE_TORSO) < SIZE_LARGE && item_mass(*weapon) >= 300)
    {
        SAY(mpr("That's too large and heavy for you to wield."));
        return (false);
    }

    // Small species wielding large weapons...
    if (you.body_size(PSIZE_BODY) < SIZE_MEDIUM
        && !check_weapon_wieldable_size(*weapon, you.body_size(PSIZE_BODY)))
    {
        SAY(mpr("That's too large for you to wield."));
        return (false);
    }

    if (you.undead_or_demonic() && is_holy_item(*weapon))
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
                    mpr(weapon->name(DESC_INVENTORY_EQUIP).c_str());
            }
        }
        return (false);
    }

    if (!ignore_temporary_disability
        && you.hunger_state < HS_FULL
        && get_weapon_brand(*weapon) == SPWPN_VAMPIRICISM
        && !you.is_undead)
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
                    mpr(weapon->name(DESC_INVENTORY_EQUIP).c_str());
            }
        }
        return (false);
    }

    if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
    {
        SAY(mpr("You can't wield that with a shield."));
        return (false);
    }

    // We can wield this weapon. Phew!
    return (true);

#undef SAY
}

static bool _valid_weapon_swap(const item_def &item)
{
    // Weapons and staves are valid weapons.
    if (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_STAVES)
        return (true);

    // Some misc. items need to be wielded to be evoked.
    if (is_deck(item) || item.base_type == OBJ_MISCELLANY
                         && item.sub_type == MISC_LANTERN_OF_SHADOWS)
    {
        return (true);
    }

    // Some missiles need to be wielded for spells.
    if (item.base_type == OBJ_MISSILES)
    {
        if (item.sub_type == MI_STONE)
            return (you.has_spell(SPELL_SANDBLAST));

        if (item.sub_type == MI_ARROW)
            return (you.has_spell(SPELL_STICKS_TO_SNAKES));

        return (false);
    }

    // Bone Shards.
    if (item.base_type == OBJ_CORPSES)
    {
        return (item.sub_type == CORPSE_SKELETON
                && you.has_spell(SPELL_BONE_SHARDS));
    }

    // Sublimation of Blood.
    if (!you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
        return (false);

    if (item.base_type == OBJ_FOOD)
        return (item.sub_type == FOOD_CHUNK);

    if (item.base_type == OBJ_POTIONS && item_type_known(item))
    {
       return (item.sub_type == POT_BLOOD
               || item.sub_type == POT_BLOOD_COAGULATED);
    }

    return (false);
}

// If force is true, don't check weapon inscriptions.
// (Assuming the player was already prompted for that.)
bool wield_weapon(bool auto_wield, int slot, bool show_weff_messages,
                  bool force, bool show_unwield_msg)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    // Look for conditions like berserking that could prevent wielding
    // weapons.
    if (!can_wield(NULL, true))
        return (false);

    int item_slot = 0;          // default is 'a'

    if (auto_wield)
    {
        if (item_slot == you.equip[EQ_WEAPON])
            item_slot = 1;      // backup is 'b'

        if (slot != -1)         // allow external override
            item_slot = slot;
    }

    // If the swap slot has a bad (but valid) item in it,
    // the swap will be to bare hands.
    const bool good_swap = (item_slot == PROMPT_GOT_SPECIAL
                            || _valid_weapon_swap(you.inv[item_slot]));

    // Prompt if not using the auto swap command, or if the swap slot
    // is empty.
    if (item_slot != PROMPT_GOT_SPECIAL
        && (!auto_wield || !you.inv[item_slot].is_valid() || !good_swap))
    {
        if (!auto_wield)
        {
            item_slot = prompt_invent_item(
                            "Wield which item (- for none, * to show all)?",
                            MT_INVLIST, OSEL_WIELD,
                            true, true, true, '-', -1, NULL, OPER_WIELD);
        }
        else
            item_slot = PROMPT_GOT_SPECIAL;
    }

    if (prompt_failed(item_slot))
        return (false);
    else if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are already wielding that!");
        return (true);
    }

    // Now we really change weapons! (Most likely, at least...)
    if (you.duration[DUR_SURE_BLADE])
    {
        mpr("The bond with your blade fades away.");
        you.duration[DUR_SURE_BLADE] = 0;
    }
    // Reset the warning counter.
    you.received_weapon_warning = false;

    if (item_slot == PROMPT_GOT_SPECIAL)  // '-' or bare hands
    {
        if (const item_def* wpn = you.weapon())
        {
            // Can we safely unwield this item?
            if (needs_handle_warning(*wpn, OPER_WIELD))
            {
                const std::string prompt =
                    "Really unwield " + wpn->name(DESC_INVENTORY) + "?";
                if (!yesno(prompt.c_str(), false, 'n'))
                    return (false);
            }

            if (!unwield_item(show_weff_messages))
                return (false);

            if (show_unwield_msg)
                canned_msg(MSG_EMPTY_HANDED);

            // Switching to bare hands is extra fast.
            you.turn_is_over = true;
            you.time_taken *= 3;
            you.time_taken /= 10;
        }
        else
            mpr("You are already empty-handed.");

        return (true);
    }

    item_def& new_wpn(you.inv[item_slot]);

    if (!can_wield(&new_wpn, true))
        return (false);

    // For non-auto_wield cases checked above.
    if (auto_wield && !force
        && !check_warning_inscriptions(new_wpn, OPER_WIELD))
    {
        return (false);
    }

    // Make sure that wielding the weapon won't kill the player.
    if (!safe_to_remove_or_wear(new_wpn, false))
        return (false);

    // Unwield any old weapon.
    if (you.weapon() && !unwield_item(show_weff_messages))
        return (false);

    const unsigned int old_talents = your_talents(false).size();

    // Go ahead and wield the weapon.
    equip_item(EQ_WEAPON, item_slot, show_weff_messages);

    mpr(new_wpn.name(DESC_INVENTORY_EQUIP).c_str());

    // Warn player about low str/dex or throwing skill.
    if (show_weff_messages)
        wield_warning();

    if (Hints.hints_left)
    {
        if (new_wpn.cursed())
            learned_something_new(HINT_YOU_CURSED);
        else if (your_talents(false).size() > old_talents)
            learned_something_new(HINT_NEW_ABILITY_ITEM);
    }

    // Time calculations.
    you.time_taken /= 2;

    you.wield_change  = true;
    you.m_quiver->on_weapon_changed();
    you.turn_is_over  = true;

    return (true);
}

static const char *shield_base_name(const item_def *shield)
{
    return (shield->sub_type == ARM_BUCKLER? "buckler"
                                           : "shield");
}

static const char *shield_impact_degree(int impact)
{
    return (impact > 160 ? "severely "      :
            impact > 130 ? "significantly " :
            impact > 110 ? ""
                         : NULL);
}

static void warn_launcher_shield_slowdown(const item_def &launcher)
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

    // Warnings are limited to bows and quarterstaves at the moment.
    const item_def *weapon = you.weapon();
    if (!weapon)
        return;

    if (is_range_weapon(*weapon))
        warn_launcher_shield_slowdown(*weapon);
    else if (weapon->base_type == OBJ_WEAPONS
             && weapon_skill(*weapon) == SK_STAVES)
    {
        mprf(MSGCH_WARN, "Your %s severely limits your weapon's effectiveness.",
             shield_base_name(you.shield()));
    }
}

//---------------------------------------------------------------
//
// armour_prompt
//
// Prompt the user for some armour. Returns true if the user picked
// something legit.
//
//---------------------------------------------------------------
bool armour_prompt(const std::string & mesg, int *index, operation_types oper)
{
    ASSERT(index != NULL);

    bool  succeeded = false;
    int   slot;

    if (inv_count() < 1)
        canned_msg(MSG_NOTHING_CARRIED);
    else if (you.berserk())
        canned_msg(MSG_TOO_BERSERK);
    else
    {
        int selector = OBJ_ARMOUR;
        if (oper == OPER_TAKEOFF && !Options.equip_unequip)
            selector = OSEL_WORN_ARMOUR;
        slot = prompt_invent_item( mesg.c_str(), MT_INVLIST, selector,
                                   true, true, true, 0, -1, NULL,
                                   oper );

        if (!prompt_failed(slot))
        {
            *index = slot;
            succeeded = true;
        }
    }

    return (succeeded);
}

static bool cloak_is_being_removed( void )
{
    if (current_delay_action() != DELAY_ARMOUR_OFF)
        return (false);

    if (you.delay_queue.front().parm1 != you.equip[ EQ_CLOAK ])
        return (false);

    return (true);
}

//---------------------------------------------------------------
//
// wear_armour
//
//---------------------------------------------------------------
void wear_armour(int slot) // slot is for tiles
{
    if (!player_can_handle_equipment())
    {
        mpr("You can't wear anything in your present form.");
        return;
    }

    int armour_wear_2 = 0;

    if (slot != -1)
        armour_wear_2 = slot;
    else if (!armour_prompt("Wear which item?", &armour_wear_2, OPER_WEAR))
        return;

    // Wear the armour.
    if (safe_to_remove_or_wear( you.inv[armour_wear_2],
                                wearing_slot(armour_wear_2) ))
    {
        do_wear_armour( armour_wear_2, false );
    }
}

static int armour_equip_delay(const item_def &item)
{
    int delay = property( item, PARM_AC );

    // Shields are comparatively easy to wear.
    if (is_shield( item ))
        delay = delay / 2 + 1;

    if (delay < 1)
        delay = 1;

    return (delay);
}

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary)
{
    const object_class_type base_type = item.base_type;
    if (base_type != OBJ_ARMOUR)
    {
        if (verbose)
           mpr("You can't wear that.");

        return (false);
    }

    bool can_wear = true;
    const int sub_type = item.sub_type;
    const equipment_type slot = get_armour_slot(item);

    if (sub_type == ARM_NAGA_BARDING)
        can_wear = (you.species == SP_NAGA);
    else if (sub_type == ARM_CENTAUR_BARDING)
        can_wear = (you.species == SP_CENTAUR);
    else
    {
        can_wear = (fit_armour_size(item,
                        you.body_size(PSIZE_TORSO, ignore_temporary)) == 0);
    }

    if (!can_wear)
    {
        if (verbose)
            mpr("You can't wear that!");

        return (false);
    }

    if (sub_type == ARM_GLOVES)
    {
        if (you.has_claws(false) >= 3)
        {
            if (verbose)
                mpr( "You can't wear gloves with your huge claws!" );

            return (false);
        }
    }

    if (sub_type == ARM_BOOTS)
    {
        if (player_mutation_level(MUT_HOOVES) >= 3)
        {
            if (verbose)
                mpr("You can't wear boots with hooves!");
            return (false);
        }

        if (player_mutation_level(MUT_TALONS) >= 3)
        {
            if (verbose)
                mpr("Boots don't fit your talons!");
           return (false);
        }

        if (you.species == SP_NAGA)
        {
            if (verbose)
                mpr("You can't wear that!");
            return (false);
        }

        if (!ignore_temporary && you.swimming()
            && you.species == SP_MERFOLK)
        {
            if (verbose)
               mpr("You don't currently have feet!");

            return (false);
        }
    }

    if (you.species == SP_NAGA && sub_type == ARM_NAGA_BARDING
        && (ignore_temporary || !player_is_shapechanged()))
    {
        // It fits.
        return (true);
    }
    else if (you.species == SP_CENTAUR
             && sub_type == ARM_CENTAUR_BARDING
             && (ignore_temporary || !player_is_shapechanged()))
    {
        // It fits.
        return (true);
    }
    else if (slot == EQ_HELMET)
    {
        // Soft helmets (caps and wizard hats) always fit.
        if (!is_hard_helmet( item ))
            return (true);

        if (player_mutation_level(MUT_HORNS))
        {
            if (verbose)
                mpr("You can't wear that with your horns!");

            return (false);
        }

        if (player_mutation_level(MUT_BEAK))
        {
            if (verbose)
                mpr("You can't wear that with your beak!");

            return (false);
        }

        if (player_mutation_level(MUT_ANTENNAE))
        {
            if (verbose)
                mpr("You can't wear that with your antennae!");

            return (false);
        }
    }

    if (!can_equip( slot, ignore_temporary ))
    {
        if (verbose)
            mpr("You can't wear that in your present form.");

        return (false);
    }

    // Giant races and draconians.
    if (you.body_size(PSIZE_TORSO, ignore_temporary) >= SIZE_LARGE
        || player_genus(GENPC_DRACONIAN))
    {
        if (sub_type >= ARM_LEATHER_ARMOUR
               && sub_type <= ARM_PLATE_MAIL
            || sub_type == ARM_GLOVES
            || sub_type == ARM_BOOTS
            || sub_type == ARM_BUCKLER
            || sub_type == ARM_CRYSTAL_PLATE_MAIL
            || is_hard_helmet(item))
        {
            if (verbose)
               mpr("This armour doesn't fit on your body.");

            return (false);
        }
    }

    // Tiny races.
    if (you.body_size(PSIZE_TORSO, ignore_temporary) <= SIZE_LITTLE)
    {
        if ((sub_type >= ARM_LEATHER_ARMOUR
                && sub_type <= ARM_PLATE_MAIL)
            || sub_type == ARM_GLOVES
            || sub_type == ARM_BOOTS
            || sub_type == ARM_SHIELD
            || sub_type == ARM_LARGE_SHIELD
            || sub_type == ARM_CRYSTAL_PLATE_MAIL
            || is_hard_helmet(item))
        {
            if (verbose)
               mpr("This armour doesn't fit on your body.");

            return (false);
        }
    }

    return (true);
}

bool do_wear_armour(int item, bool quiet)
{
    const item_def &invitem = you.inv[item];
    if (!invitem.is_valid())
    {
        if (!quiet)
           mpr("You don't have any such object.");

        return (false);
    }

    if (!can_wear_armour(invitem, !quiet, false))
        return (false);

    const equipment_type slot = get_armour_slot(invitem);

    if (item == you.equip[EQ_WEAPON])
    {
        if (!quiet)
           mpr("You are wielding that object!");

        return (false);
    }

    if (wearing_slot(item))
    {
        if (Options.equip_unequip)
            return (!takeoff_armour(item));
        else
        {
            mpr("You're already wearing that object!");
            return (false);
        }
    }

    // if you're wielding something,
    if (you.weapon()
        // attempting to wear a shield,
        && is_shield(invitem)
        && is_shield_incompatible(*you.weapon(), &invitem))
    {
        if (!quiet)
           mpr("You'd need three hands to do that!");

        return (false);
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
                     you.inv[you.equip[EQ_BODY_ARMOUR]].name(DESC_CAP_YOUR)
                                                       .c_str());
            }
            return (false);
        }
        if (!you.inv[you.equip[EQ_CLOAK]].cursed())
        {
            cloak = you.equip[EQ_CLOAK];
            if (!takeoff_armour(you.equip[EQ_CLOAK]))
                return (false);

            removed_cloak = true;
        }
        else
        {
            if (!quiet)
               mpr("Your cloak prevents you from wearing the armour.");

            return (false);
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
            return (false);
    }

    if (!safe_to_remove_or_wear(invitem, false))
        return (false);

    you.turn_is_over = true;

    const int delay = armour_equip_delay(invitem);
    if (delay)
        start_delay(DELAY_ARMOUR_ON, delay, item);

    if (removed_cloak)
        start_delay(DELAY_ARMOUR_ON, 1, cloak);

    return (true);
}

bool takeoff_armour(int item)
{
    const item_def& invitem = you.inv[item];

    if (invitem.base_type != OBJ_ARMOUR)
    {
        mpr("You aren't wearing that!");
        return (false);
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    const equipment_type slot = get_armour_slot(invitem);
    if (item == you.equip[slot] && you.melded[slot])
    {
        mprf("%s is melded into your body!",
             invitem.name(DESC_CAP_YOUR).c_str());
        return (false);
    }

    if (!wearing_slot(item))
    {
        if (Options.equip_unequip)
            return do_wear_armour(item, true);
        else
        {
            mpr("You aren't wearing that object!");
            return (false);
        }
    }

    // If we get here, we're wearing the item.
    if (invitem.cursed())
    {
        mprf("%s is stuck to your body!", invitem.name(DESC_CAP_YOUR).c_str());
        return (false);
    }

    if (!safe_to_remove_or_wear(invitem, true))
        return (false);

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
                    return (false);

                removed_cloak = true;
            }
            else
            {
                mpr("Your cloak prevents you from removing the armour.");
                return (false);
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
                return (false);
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

    return (true);
}

bool item_is_quivered(const item_def &item)
{
    return (item.link == you.m_quiver->get_fire_item());
}

int get_next_fire_item(int current, int direction)
{
    std::vector<int> fire_order;
    you.m_quiver->get_fire_order(fire_order);

    if (fire_order.size() == 0)
        return -1;
    if (current == -1)
        return fire_order[0];

    for (unsigned i = 0; i < fire_order.size(); i++)
    {
        if (fire_order[i] == current)
        {
            unsigned int next =
                (i + direction + fire_order.size()) % fire_order.size();
            return fire_order[next];
        }
    }
    return fire_order[0];
}

class fire_target_behaviour : public targeting_behaviour
{
public:
    fire_target_behaviour()
        : chosen_ammo(false),
          selected_from_inventory(false),
          need_redraw(false)
    {
        m_slot = you.m_quiver->get_fire_item(&m_noitem_reason);
        set_prompt();
    }

    // targeting_behaviour API
    virtual command_type get_command(int key = -1);
    virtual bool should_redraw() const { return need_redraw; }
    virtual void clear_redraw()        { need_redraw = false; }
    virtual void update_top_prompt(std::string* p_top_prompt);

public:
    const item_def* active_item() const;
    // FIXME: these should be privatized and given accessors.
    int m_slot;
    bool chosen_ammo;

private:
    void set_prompt();
    void cycle_fire_item(bool forward);
    void pick_fire_item_from_inventory();
    void display_help();

    std::string prompt;
    std::string m_noitem_reason;
    std::string internal_prompt;
    bool selected_from_inventory;
    bool need_redraw;
};

void fire_target_behaviour::update_top_prompt(std::string* p_top_prompt)
{
    *p_top_prompt = internal_prompt;
}

const item_def* fire_target_behaviour::active_item() const
{
    if (m_slot == -1)
        return NULL;
    else
        return &you.inv[m_slot];
}

void fire_target_behaviour::set_prompt()
{
    std::string old_prompt = internal_prompt; // Keep for comparison at the end.
    internal_prompt.clear();

    // Figure out if we have anything else to cycle to.
    const int next_item = get_next_fire_item(m_slot, +1);
    const bool no_other_items = (next_item == -1 || next_item == m_slot);

    std::ostringstream msg;

    // Build the action.
    if (!active_item())
    {
        msg << "Firing ";
    }
    else
    {
        const launch_retval projected = is_launched(&you, you.weapon(),
                                                    *active_item());
        switch (projected)
        {
        case LRET_FUMBLED:  msg << "Awkwardly throwing "; break;
        case LRET_LAUNCHED: msg << "Firing ";             break;
        case LRET_THROWN:   msg << "Throwing ";           break;
        }
    }

    // And a key hint.
    msg << (no_other_items ? "(i - inventory)"
                           : "(i - inventory. (,) - cycle)")
        << ": ";

    // Describe the selected item for firing.
    if (!active_item())
    {
        msg << "<red>" << m_noitem_reason << "</red>";
    }
    else
    {
        const char* colour = (selected_from_inventory ? "lightgrey" : "w");
        msg << "<" << colour << ">"
            << active_item()->name(DESC_INVENTORY_EQUIP)
            << "</" << colour << ">";
    }

    // Write it out.
    internal_prompt += tagged_string_substr(msg.str(), 0, crawl_view.msgsz.x);

    // Never unset need_redraw here, because we might have cleared the
    // screen or something else which demands a redraw.
    if (internal_prompt != old_prompt)
        need_redraw = true;
}

// Cycle to the next (forward == true) or previous (forward == false)
// fire item.
void fire_target_behaviour::cycle_fire_item(bool forward)
{
    const int next = get_next_fire_item(m_slot, forward ? 1 : -1);
    if (next != m_slot && next != -1)
    {
        m_slot = next;
        selected_from_inventory = false;
        chosen_ammo = true;
    }
    set_prompt();
}

void fire_target_behaviour::pick_fire_item_from_inventory()
{
    need_redraw = true;
    std::string err;
    const int selected = _fire_prompt_for_item();
    if (selected >= 0 && _fire_validate_item(selected, err))
    {
        m_slot = selected;
        selected_from_inventory = true;
        chosen_ammo = true;
    }
    else if (!err.empty())
    {
        mpr(err);
        more();
    }
    set_prompt();
}

void fire_target_behaviour::display_help()
{
    show_targeting_help();
    redraw_screen();
    need_redraw = true;
    set_prompt();
}

command_type fire_target_behaviour::get_command(int key)
{
    if (key == -1)
        key = get_key();

    switch (key)
    {
    case '(': case CONTROL('N'): cycle_fire_item(true);  return (CMD_NO_CMD);
    case ')': case CONTROL('P'): cycle_fire_item(false); return (CMD_NO_CMD);
    case 'i': pick_fire_item_from_inventory(); return (CMD_NO_CMD);
    case '?': display_help(); return (CMD_NO_CMD);
    case CMD_TARGET_CANCEL: chosen_ammo = false; break;
    }

    return targeting_behaviour::get_command(key);
}

static bool _fire_choose_item_and_target(int& slot, dist& target,
                                         bool teleport = false)
{
    fire_target_behaviour beh;
    const bool was_chosen = (slot != -1);

    if (was_chosen)
    {
        std::string warn;
        if (!_fire_validate_item(slot, warn))
        {
            mpr(warn.c_str());
            return (false);
        }
        // Force item to be the prechosen one.
        beh.m_slot = slot;
    }

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    args.needs_path = !teleport;
    args.behaviour = &beh;

    direction(target, args);

    if (!beh.active_item())
    {
        canned_msg(MSG_OK);
        return (false);
    }
    if (!target.isValid)
    {
        if (target.isCancel)
            canned_msg(MSG_OK);
        return (false);
    }

    you.m_quiver->on_item_fired(*beh.active_item(), beh.chosen_ammo);
    you.redraw_quiver = true;
    slot = beh.m_slot;

    return (true);
}

// Bring up an inventory screen and have user choose an item.
// Returns an item slot, or -1 on abort/failure
// On failure, returns error text, if any.
static int _fire_prompt_for_item()
{
    if (inv_count() < 1)
        return -1;

    int slot = prompt_invent_item( "Fire/throw which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_THROWABLE, true, true, true, 0, -1,
                                   NULL, OPER_FIRE );

    if (slot == PROMPT_ABORT || slot == PROMPT_NOTHING)
        return -1;

    return slot;
}

// Returns false and err text if this item can't be fired.
static bool _fire_validate_item(int slot, std::string &err)
{
    if (slot == you.equip[EQ_WEAPON]
        && you.inv[slot].base_type == OBJ_WEAPONS
        && you.inv[slot].cursed())
    {
        err = "That weapon is stuck to your hand!";
        return (false);
    }
    else if (wearing_slot(slot))
    {
        err = "You are wearing that object!";
        return (false);
    }
    return (true);
}

// Returns true if warning is given.
static bool _fire_warn_if_impossible()
{
    // FIXME: merge this into transform_can_equip_slot()
    const int trans = you.attribute[ATTR_TRANSFORMATION];
    // If you can't wield it, you can't throw it.
    if (trans == TRAN_SPIDER
        || trans == TRAN_BLADE_HANDS
        || trans == TRAN_ICE_BEAST
        || trans == TRAN_DRAGON
        || trans == TRAN_BAT)
    {
        canned_msg(MSG_PRESENT_FORM);
        return (true);
    }

    if (you.attribute[ATTR_HELD])
    {
        const item_def *weapon = you.weapon();
        if (!weapon || !is_range_weapon(*weapon))
        {
            mpr("You cannot throw anything while held in a net!");
            return (true);
        }
        else if (weapon->sub_type != WPN_BLOWGUN)
        {
            mprf("You cannot shoot with your %s while held in a net!",
                 weapon->name(DESC_BASENAME).c_str());
            return (true);
        }
        // Else shooting is possible.
    }
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return (true);
    }
    return (false);
}

int get_ammo_to_shoot(int item, dist &target, bool teleport)
{
    if (_fire_warn_if_impossible())
    {
        flush_input_buffer( FLUSH_ON_FAILURE );
        return (-1);
    }

    if (!_fire_choose_item_and_target(item, target, teleport))
        return (-1);

    std::string warn;
    if (!_fire_validate_item(item, warn))
    {
        mpr(warn.c_str());
        return (-1);
    }
    return (item);
}

// If item == -1, prompt the user.
// If item passed, it will be put into the quiver.
void fire_thing(int item)
{
    dist target;
    item = get_ammo_to_shoot(item, target);
    if (item == -1)
        return;

    if (check_warning_inscriptions(you.inv[item], OPER_FIRE))
    {
        bolt beam;
        throw_it( beam, item, false, 0, &target );
    }
}

// Basically does what throwing used to do: throw an item without changing
// the quiver.
void throw_item_no_quiver()
{
    if (_fire_warn_if_impossible())
    {
        flush_input_buffer( FLUSH_ON_FAILURE );
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    std::string warn;
    int slot = _fire_prompt_for_item();

    if (slot == -1)
    {
        canned_msg(MSG_OK);
        return;
    }

    if (!_fire_validate_item(slot, warn))
    {
        mpr(warn.c_str());
        return;
    }

    // Okay, item is valid.
    bolt beam;
    throw_it( beam, slot );
}

// Returns delay multiplier numerator (denominator should be 100) for the
// launcher with the currently equipped shield.
int launcher_shield_slowdown(const item_def &launcher, const item_def *shield)
{
    int speed_adjust = 100;
    if (!shield)
        return (speed_adjust);

    const int shield_type = shield->sub_type;
    hands_reqd_type hands = hands_reqd(launcher, you.body_size());

    switch (hands)
    {
    default:
    case HANDS_ONE:
        break;

    case HANDS_HALF:
        speed_adjust = shield_type == ARM_BUCKLER  ? 105 :
                       shield_type == ARM_SHIELD   ? 125 :
                                                     150;
        break;

    case HANDS_TWO:
        speed_adjust = shield_type == ARM_BUCKLER  ? 125 :
                       shield_type == ARM_SHIELD   ? 150 :
                                                     200;
        break;
    }

    // Adjust for shields skill.
    if (speed_adjust > 100)
        speed_adjust -= ((speed_adjust - 100) * 5 / 10)
                            * you.skills[SK_SHIELDS] / 27;

    return (speed_adjust);
}

// Returns the attack cost of using the launcher, taking skill and shields
// into consideration. NOTE: You must pass in the shield; if you send in
// NULL, this function assumes no shield is in use.
int launcher_final_speed(const item_def &launcher, const item_def *shield)
{
    const int  str_weight   = weapon_str_weight( launcher );
    const int  dex_weight   = 10 - str_weight;
    const skill_type launcher_skill = range_skill( launcher );
    const int shoot_skill = you.skills[launcher_skill];
    const int bow_brand = get_weapon_brand( launcher );

    int speed_base = 10 * property( launcher, PWPN_SPEED );
    int speed_min = 70;
    int speed_stat = str_weight * you.strength() + dex_weight * you.dex();

    // Reduce runaway bow overpoweredness.
    if (launcher_skill == SK_BOWS)
        speed_min = 60;

    if (shield)
    {
        const int speed_adjust = launcher_shield_slowdown(launcher, shield);

        // Shields also reduce the speed cap.
        speed_base = speed_base * speed_adjust / 100;
        speed_min =  speed_min  * speed_adjust / 100;
    }

    // Do the same when trying to shoot while held in a net
    // (only possible with blowguns).
    if (you.attribute[ATTR_HELD])
    {
        int speed_adjust = 105; // Analogous to buckler and one-handed weapon.
        speed_adjust -= ((speed_adjust - 100) * 5 / 10)
                            * you.skills[SK_THROWING] / 27;

        // Also reduce the speed cap.
        speed_base = speed_base * speed_adjust / 100;
        speed_min =  speed_min  * speed_adjust / 100;
    }

    int speed = speed_base - 4 * shoot_skill * speed_stat / 250;
    if (speed < speed_min)
        speed = speed_min;

    if (bow_brand == SPWPN_SPEED)
    {
        // Speed nerf as per 4.1. Even with the nerf, bows of speed are the
        // best bows, bar none.
        speed = 2 * speed / 3;
    }

    return (speed);
}

// Determines if the end result of the combined launcher + ammo brands a
// fire/frost beam.
// positive: frost, negative: flame, zero: neither
bool elemental_missile_beam(int launcher_brand, int ammo_brand)
{
    if (launcher_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
        return (true);

    int element = (launcher_brand == SPWPN_FROST
                   + ammo_brand == SPMSL_FROST
                   - launcher_brand == SPWPN_FLAME
                   - ammo_brand == SPMSL_FLAME);

    return (element != 0);
}

static bool _poison_hit_victim(bolt& beam, actor* victim, int dmg, int corpse)
{
    if (!victim->alive() || victim->res_poison() > 0)
        return (false);

    if (beam.is_tracer)
        return (true);

    int levels = 0;

    actor* agent = beam.agent();

    if (dmg > 0 || beam.ench_power == AUTOMATIC_HIT
                   && x_chance_in_y(90 - 3 * victim->armour_class(), 100))
    {
        levels = 1 + random2(3);
    }

    if (levels <= 0)
        return (false);

    victim->poison(agent, levels);

    return (true);
}

static bool _item_penetrates_victim(const bolt &beam, const actor *victim,
                                    int &used)
{
    if (beam.aimed_at_feet)
        return (false);

    used = 0;

    return (true);
}

static bool _silver_damages_victim(bolt &beam, actor* victim, int &dmg,
                                   std::string &dmg_msg)
{

    int mutated = 0;

    // For mutation damage, we want to count innate mutations for
    // the demonspawn, but not for other species.
    if (you.species == SP_DEMONSPAWN)
        mutated = how_mutated(true, true);
    else
        mutated = how_mutated(false, true);

    if ((victim->holiness() == MH_UNDEAD && !victim->is_insubstantial())
        || victim->is_chaotic()
        || (victim == &you && player_is_shapechanged()))
    {
        dmg *= 2;
    }
    else if (victim == &you && mutated > 0)
    {
        int multiplier = 100 + (mutated * 5);

        if (multiplier > 200)
            multiplier = 200;

        dmg = (dmg * multiplier) / 100;
    }
    else
    {
        return (false);
    }

    if (!beam.is_tracer && you.can_see(victim))
       dmg_msg = "The silver sears " + victim->name(DESC_NOCAP_THE) + "!";

    return (false);
}

static bool _reaping_hit_victim(bolt& beam, actor* victim, int dmg, int corpse)
{
    if (beam.is_tracer || victim->alive() || corpse == -1
        || corpse == NON_ITEM || victim->atype() == ACT_PLAYER)
    {
        return (false);
    }

    return (mons_reaped(beam.agent(), victim->as_monster()));
}

static bool _dispersal_hit_victim(bolt& beam, actor* victim, int dmg,
                                  int corpse)
{
    const actor* agent = beam.agent();

    if (!victim->alive() || victim == agent)
        return (false);

    if (beam.is_tracer)
        return (true);

    if (victim->atype() == ACT_PLAYER && item_blocks_teleport(true, true))
    {
        canned_msg(MSG_STRANGE_STASIS);
        return (false);
    }

    const bool was_seen = you.can_see(victim);
    const bool no_sanct = victim->kill_alignment() == KC_OTHER;

    coord_def pos, pos2;

    int tries = 0;
    do
    {
        if (!random_near_space(victim->pos(), pos, false, true, false,
                               no_sanct))
        {
            return (false);
        }
    }
    while (!victim->is_habitable(pos) && tries++ < 100);

    if (!victim->is_habitable(pos))
        return (false);

    tries = 0;
    do
        random_near_space(victim->pos(), pos2, false, true, false, no_sanct);
    while (!victim->is_habitable(pos2) && tries++ < 100);

    if (!victim->is_habitable(pos2))
        return (false);

    // Pick the square further away from the agent.
    const coord_def from = agent->pos();
    if (in_bounds(pos2)
        && grid_distance(pos2, from) > grid_distance(pos, from))
    {
        pos = pos2;
    }

    if (pos == victim->pos())
        return (false);

    const coord_def oldpos = victim->pos();

    if (victim->atype() == ACT_PLAYER)
    {
        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, you.pos(), 1 + random2(3), KC_YOU);

        victim->moveto(pos);
        canned_msg(MSG_YOU_BLINK);
    }
    else
    {
        monsters *mon = victim->as_monster();

        if (!(mon->flags & MF_WAS_IN_VIEW))
            mon->seen_context = "thin air";

        mon->move_to_pos(pos);

        // Leave a purple cloud.
        place_cloud(CLOUD_TLOC_ENERGY, oldpos, 1 + random2(3),
                    victim->kill_alignment());

        mon->apply_location_effects(oldpos);
        mon->check_redraw(oldpos);

        const bool        seen = you.can_see(mon);
        const std::string name = mon->name(DESC_CAP_THE);
        if (was_seen && seen)
            mprf("%s blinks!", name.c_str());
        else if (was_seen && !seen)
            mprf("%s vanishes!", name.c_str());
    }

    return (true);
}

static bool _electricity_water_explosion(const bolt &beam, const actor *victim,
                                         int &used)
{
    used = 1000;

    ASSERT(!beam.is_tracer);
    if (you.can_see(victim))
        mpr("Electricity arcs through the water!");
    conduct_electricity(victim->pos(), beam.agent());

    return (true);
}

static bool _charged_hit_victim(bolt &beam, actor* victim, int &dmg,
                                   std::string &dmg_msg)
{
    if (victim->airborne() || victim->res_elec() > 0 || !one_chance_in(3))
        return (false);

    // A hack and code duplication, but that's easier than adding accounting
    // for each of multiple brands.
    if (victim->id() == MONS_SIXFIRHY)
    {
        if (!beam.is_tracer)
            victim->heal(10 + random2(15), false);
        // physical damage is still done
    }
    else
        dmg += 10 + random2(15);

    if (beam.is_tracer)
        return (false);

    if (you.can_see(victim))
    {
        if (victim->atype() == ACT_PLAYER)
            dmg_msg = "You are electrocuted!";
        else if (victim->id() == MONS_SIXFIRHY)
            dmg_msg = victim->name(DESC_CAP_THE) + " is charged up!";
        else
            dmg_msg = "There is a sudden explosion of sparks!";
    }

    if (feat_is_water(grd(victim->pos())))
    {
        beam.range_funcs.insert(beam.range_funcs.begin(),
            _electricity_water_explosion);
    }

    return (false);
}

static bool _blessed_hit_victim(bolt &beam, actor* victim, int &dmg,
                                   std::string &dmg_msg)
{
    if (victim->undead_or_demonic())
    {
        dmg += 1 + (random2(dmg * 15) / 10);

        if (!beam.is_tracer && you.can_see(victim))
           dmg_msg = victim->name(DESC_CAP_THE) + " "
                   + victim->conj_verb("convulse") + "!";
    }

    return (false);
}

int _blowgun_power_roll(bolt &beam)
{
    actor* agent = beam.agent();
    if (!agent)
        return 0;

    // Could check player shield skill here or something,
    // but that won't work with potential other sources
    // of reflection, and it doesn't matter anyway. [rob]
    if (beam.reflections > 0)
        return (agent->get_experience_level() / 3);

    int base_power;
    item_def* blowgun;
    if (agent->atype() == ACT_MONSTER)
    {
        base_power = agent->get_experience_level();
        blowgun = agent->as_monster()->launcher();
    }
    else
    {
        base_power = agent->skill(SK_THROWING);
        blowgun = agent->weapon();
    }

    ASSERT(blowgun && blowgun->sub_type == WPN_BLOWGUN);

    return (base_power + blowgun->plus);
}

bool _blowgun_check(bolt &beam, actor* victim, bool message = true)
{
    actor* agent = beam.agent();

    if (!agent || agent->atype() == ACT_MONSTER || beam.reflections > 0)
        return (true);

    monsters* mons = victim->as_monster();

    const int skill = you.skills[SK_THROWING];
    const item_def* wp = agent->weapon();
    ASSERT(wp && wp->sub_type == WPN_BLOWGUN);
    const int enchantment = wp->plus;

    // You have a really minor chance of hitting with no skills or good
    // enchants.
    if (mons->hit_dice < 15 && random2(100) <= 2)
        return (true);

    const int resist_roll = 2 + random2(4 + skill + enchantment);

    dprf("Brand rolled %d against monster HD: %d.",
         resist_roll, mons->hit_dice);

    if (resist_roll < mons->hit_dice)
    {
        simple_monster_message(mons, " resists!");
        return (false);
    }

    return (true);
}

static bool _paralysis_hit_victim(bolt& beam, actor* victim, int dmg,
                                 int corpse)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->paralyse(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _sleep_hit_victim(bolt& beam, actor* victim, int dmg,
                              int corpse)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->put_to_sleep(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _confusion_hit_victim(bolt &beam, actor* victim, int dmg,
                                  int corpse)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->confuse(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _slow_hit_victim(bolt &beam, actor* victim, int dmg,
                                  int corpse)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->slow_down(beam.agent(), 5 + random2(blowgun_power));
    return (true);
}

static bool _sickness_hit_victim(bolt &beam, actor* victim, int dmg,
                                 int corpse)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    int blowgun_power = _blowgun_power_roll(beam);
    victim->sicken(40 + random2(blowgun_power));
    return (true);
}

static bool _rage_hit_victim(bolt &beam, actor* victim, int dmg,
                             int corpse)
{
    if (beam.is_tracer)
        return (false);

    if (!_blowgun_check(beam, victim))
        return (false);

    if (victim->atype() == ACT_MONSTER)
        victim->as_monster()->go_frenzy();
    else
        victim->go_berserk(false);

    return (true);
}

bool setup_missile_beam(const actor *agent, bolt &beam, item_def &item,
                        std::string &ammo_name, bool &returning)
{
    dungeon_char_type zapsym = NUM_DCHAR_TYPES;
    switch (item.base_type)
    {
    case OBJ_WEAPONS:    zapsym = DCHAR_FIRED_WEAPON;  break;
    case OBJ_MISSILES:   zapsym = DCHAR_FIRED_MISSILE; break;
    case OBJ_ARMOUR:     zapsym = DCHAR_FIRED_ARMOUR;  break;
    case OBJ_WANDS:      zapsym = DCHAR_FIRED_STICK;   break;
    case OBJ_FOOD:       zapsym = DCHAR_FIRED_CHUNK;   break;
    case OBJ_UNKNOWN_I:  zapsym = DCHAR_FIRED_BURST;   break;
    case OBJ_SCROLLS:    zapsym = DCHAR_FIRED_SCROLL;  break;
    case OBJ_JEWELLERY:  zapsym = DCHAR_FIRED_TRINKET; break;
    case OBJ_POTIONS:    zapsym = DCHAR_FIRED_FLASK;   break;
    case OBJ_UNKNOWN_II: zapsym = DCHAR_FIRED_ZAP;     break;
    case OBJ_BOOKS:      zapsym = DCHAR_FIRED_BOOK;    break;
    case OBJ_STAVES:     zapsym = DCHAR_FIRED_STICK;   break;
    default: break;
    }

    beam.glyph = dchar_glyph(zapsym);
    beam.was_missile = true;

    item_def *launcher  = const_cast<actor*>(agent)->weapon(0);
    if (launcher && !item.launched_by(*launcher))
        launcher = NULL;

    int bow_brand = SPWPN_NORMAL;
    if (launcher != NULL)
        bow_brand = get_weapon_brand(*launcher);

    int ammo_brand = get_ammo_brand(item);

    // Launcher brand does not override ammunition.
    if (ammo_brand != SPMSL_NORMAL && bow_brand != SPWPN_NORMAL)
    {
        // But not for Nessos.
        if (agent->atype() == ACT_MONSTER)
        {
            const monsters* mon = static_cast<const monsters*>(agent);
            if (mon->type != MONS_NESSOS)
                bow_brand = SPWPN_NORMAL;
        }
        else
            bow_brand = SPWPN_NORMAL;
    }

    if (is_launched(agent, launcher, item) == LRET_FUMBLED)
    {
        // We want players to actually carry blowguns and bows, not just rely
        // on high to-hit modifiers.  Rationalization: The poison/magic/
        // whatever is only applied to the tips.  -sorear

        bow_brand = SPWPN_NORMAL;
        ammo_brand = SPMSL_NORMAL;
    }

    // This is a bit of a special case because it applies even for melee
    // weapons, for which brand is normally ignored.
    returning = get_weapon_brand(item) == SPWPN_RETURNING
                || ammo_brand == SPMSL_RETURNING;

    if (agent->atype() == ACT_PLAYER)
    {
        beam.attitude      = ATT_FRIENDLY;
        beam.beam_source   = NON_MONSTER;
        beam.smart_monster = true;
        beam.thrower       = KILL_YOU_MISSILE;
    }
    else
    {
        const monsters *mon = agent->as_monster();

        beam.attitude      = mons_attitude(mon);
        beam.beam_source   = mon->mindex();
        beam.smart_monster = (mons_intel(mon) >= I_NORMAL);
        beam.thrower       = KILL_MON_MISSILE;
    }

    beam.item         = &item;
    beam.effect_known = item_ident(item, ISFLAG_KNOW_TYPE);
    beam.source       = agent->pos();
    beam.colour       = item.colour;
    beam.flavour      = BEAM_MISSILE;
    beam.is_beam      = false;
    beam.aux_source.clear();

    beam.can_see_invis = agent->can_see_invisible();

    const unrandart_entry* entry = launcher && is_unrandom_artefact(*launcher)
        ? get_unrand_entry(launcher->special) : NULL;

    if (entry && entry->fight_func.launch)
    {
        setup_missile_type sm =
            entry->fight_func.launch(launcher, &beam, &ammo_name,
                                     &returning);

        switch(sm)
        {
        case SM_CONTINUE:
            break;
        case SM_FINISHED:
            return (false);
        case SM_CANCEL:
            return (true);
        }
    }

    bool poisoned   = (bow_brand == SPWPN_VENOM
                        || ammo_brand == SPMSL_POISONED);

    const bool exploding    = (ammo_brand == SPMSL_EXPLODING);
    const bool penetrating  = (bow_brand  == SPWPN_PENETRATION
                                || ammo_brand == SPMSL_PENETRATION);
    const bool silver       = (ammo_brand == SPMSL_SILVER);
    const bool disperses    = (ammo_brand == SPMSL_DISPERSAL);
    const bool reaping      = (bow_brand  == SPWPN_REAPING
                               || ammo_brand == SPMSL_REAPING);
    const bool charged      = bow_brand  == SPWPN_ELECTROCUTION;
    const bool blessed      = bow_brand == SPWPN_HOLY_WRATH;

    const bool paralysis    = ammo_brand == SPMSL_PARALYSIS;
    const bool slow         = ammo_brand == SPMSL_SLOW;
    const bool sleep        = ammo_brand == SPMSL_SLEEP;
    const bool confusion    = ammo_brand == SPMSL_CONFUSION;
    const bool sickness     = ammo_brand == SPMSL_SICKNESS;
    const bool rage         = ammo_brand == SPMSL_RAGE;

    ASSERT(!exploding || !is_artefact(item));

    beam.name = item.name(DESC_PLAIN, false, false, false);

    // The chief advantage here is the extra damage this does
    // against susceptible creatures.

    // Note that bow_brand is known since the bow is equipped.

    bool beam_changed = false;

    // Chaos overides flame and frost/ice.
    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
    {
        // Chaos can't be poisoned, since that might conflict with
        // the random healing effect or overlap with the random
        // poisoning effect.
        poisoned = false;
        if (item.special == SPWPN_VENOM || item.special == SPMSL_CURARE)
            item.special = SPMSL_NORMAL;

        beam.effect_known = false;

        beam.flavour = BEAM_CHAOS;
        if (ammo_brand != SPMSL_CHAOS)
        {
            beam.name    += " of chaos";
            ammo_name    += " of chaos";
        }
        else
        {
            set_ident_flags (item, ISFLAG_KNOW_TYPE);
            beam_changed = true;
        }
        beam.colour  = ETC_RANDOM;
    }
    else if ((bow_brand == SPWPN_FLAME || ammo_brand == SPMSL_FLAME)
             && ammo_brand != SPMSL_FROST && bow_brand != SPWPN_FROST)
    {
        beam.flavour = BEAM_FIRE;
        if (ammo_brand != SPMSL_FLAME)
        {
            beam.name    += " of flame";
            ammo_name    += " of flame";
        }
        else
        {
            set_ident_flags (item, ISFLAG_KNOW_TYPE);
            beam_changed = true;
        }

        beam.colour  = RED;
    }
    else if ((bow_brand == SPWPN_FROST || ammo_brand == SPMSL_FROST)
             && ammo_brand != SPMSL_FLAME && bow_brand != SPWPN_FLAME)
    {
        beam.flavour = BEAM_COLD;
        if (ammo_brand != SPMSL_FROST)
        {
            beam.name    += " of frost";
            ammo_name   += " of frost";
        }
        else
        {
            set_ident_flags (item, ISFLAG_KNOW_TYPE);
            beam_changed = true;
        }
        beam.colour  = WHITE;
    }

    if (beam_changed)
        beam.name = item.name(DESC_PLAIN, false, false, false);

    ammo_name = item.name(DESC_PLAIN);

    ASSERT(beam.flavour == BEAM_MISSILE || !is_artefact(item));

    if (silver)
        beam.damage_funcs.push_back(_silver_damages_victim);
    if (poisoned)
        beam.hit_funcs.push_back(_poison_hit_victim);
    if (penetrating)
    {
        beam.range_funcs.push_back(_item_penetrates_victim);
        beam.hit_verb = "pierces through";
    }
    if (reaping)
        beam.hit_funcs.push_back(_reaping_hit_victim);
    if (disperses)
        beam.hit_funcs.push_back(_dispersal_hit_victim);
    if (charged)
        beam.damage_funcs.push_back(_charged_hit_victim);
    if (blessed)
        beam.damage_funcs.push_back(_blessed_hit_victim);

    // New needle brands have no effect when thrown without launcher.
    if (launcher != NULL)
    {
        if (paralysis)
            beam.hit_funcs.push_back(_paralysis_hit_victim);
        if (slow)
            beam.hit_funcs.push_back(_slow_hit_victim);
        if (sleep)
            beam.hit_funcs.push_back(_sleep_hit_victim);
        if (confusion)
            beam.hit_funcs.push_back(_confusion_hit_victim);
        if (sickness)
            beam.hit_funcs.push_back(_sickness_hit_victim);
        if (rage)
            beam.hit_funcs.push_back(_rage_hit_victim);
    }

    if (reaping && item.special != SPMSL_REAPING)
    {
        beam.name = "shadowy " + beam.name;
        ammo_name = "shadowy " + ammo_name;
    }

    if (disperses && item.special != SPMSL_DISPERSAL)
    {
        beam.name = "dispersing " + beam.name;
        ammo_name = "dispersing " + ammo_name;
    }

    // XXX: This doesn't make sense, but it works.
    if (poisoned && item.special != SPMSL_POISONED)
    {
        beam.name = "poisoned " + beam.name;
        ammo_name = "poisoned " + ammo_name;
    }

    if (penetrating && item.special != SPMSL_PENETRATION)
    {
        beam.name = "penetrating " + beam.name;
        ammo_name = "penetrating " + ammo_name;
    }

    if (silver && item.special != SPMSL_SILVER)
    {
        beam.name = "silvery " + beam.name;
        ammo_name = "silvery " + ammo_name;
    }

    if (blessed)
    {
        beam.name = "blessed " + beam.name;
        ammo_name = "blessed " + ammo_name;
    }

    // Do this here so that we get all the name mods except for a
    // redundant "exploding".
    if (exploding)
    {
         bolt *expl = new bolt(beam);

         expl->is_explosion = true;
         expl->damage       = dice_def(2, 5);
         expl->ex_size      = 1;

         if (beam.flavour == BEAM_MISSILE)
         {
             expl->flavour = BEAM_FRAG;
             expl->name   += " fragments";

             const std::string short_name =
                 item.name(DESC_PLAIN, false, false, false, false,
                           ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK
                           | ISFLAG_RACIAL_MASK);

             expl->name = replace_all(expl->name, item.name(DESC_PLAIN),
                                      short_name);
         }
         expl->name = "explosion of " + expl->name;

         beam.special_explosion = expl;
    }

    if (exploding && item.special != SPMSL_EXPLODING)
    {
        beam.name = "exploding " + beam.name;
        ammo_name = "exploding " + ammo_name;
    }

    if (beam.flavour != BEAM_MISSILE)
    {
        returning = false;

        beam.glyph = dchar_glyph(DCHAR_FIRED_BOLT);
    }

    if (!is_artefact(item))
        ammo_name = article_a(ammo_name, true);
    else
        ammo_name = "the " + ammo_name;

    return (false);
}

// XXX This is a bit too generous, as it lets the player determine
// that the bolt of fire he just shot from a flaming bow is actually
// a poison arrow. Hopefully this isn't too abusable.
static bool determines_ammo_brand(int bow_brand, int ammo_brand)
{
    if (bow_brand == SPWPN_FLAME && ammo_brand == SPMSL_FLAME)
        return (false);
    if (bow_brand == SPWPN_FROST && ammo_brand == SPMSL_FROST)
        return (false);
    if (bow_brand == SPWPN_VENOM && ammo_brand == SPMSL_POISONED)
        return (false);
    if (bow_brand == SPWPN_CHAOS && ammo_brand == SPMSL_CHAOS)
        return (false);
    if (bow_brand == SPWPN_PENETRATION && ammo_brand == SPMSL_PENETRATION)
        return (false);
    if (bow_brand == SPWPN_REAPING && ammo_brand == SPMSL_REAPING)
        return (false);

    return (true);
}

static int stat_adjust(int value, int stat, int statbase,
                       const int maxmult = 160, const int minmult = 40)
{
    int multiplier = (statbase + (stat - statbase) / 2) * 100 / statbase;
    if (multiplier > maxmult)
        multiplier = maxmult;
    else if (multiplier < minmult)
        multiplier = minmult;

    if (multiplier > 100)
        value = value * (100 + random2avg(multiplier - 100, 2)) / 100;
    else if (multiplier < 100)
        value = value * (100 - random2avg(100 - multiplier, 2)) / 100;

    return (value);
}

static int str_adjust_thrown_damage(int dam)
{
    return stat_adjust(dam, you.strength(), 15, 160, 90);
}

static int dex_adjust_thrown_tohit(int hit)
{
    return stat_adjust(hit, you.dex(), 13, 160, 90);
}

static void identify_floor_missiles_matching(item_def mitem, int idflags)
{
    mitem.flags &= ~idflags;

    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            for (stack_iterator si(coord_def(x,y)); si; ++si)
            {
                if ((si->flags & ISFLAG_THROWN) && items_stack(*si, mitem))
                    si->flags |= idflags;
            }
}

void _merge_ammo_in_inventory(int slot)
{
    if (!you.inv[slot].is_valid())
        return;

    bool done_anything = false;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (i == slot || !you.inv[i].is_valid())
            continue;

        // Merge with the thrower slot. This could be a bad
        // thing if you're wielding IDed ammo and firing from
        // an unIDed stack...but that's a pretty remote case.
        if (items_stack(you.inv[i], you.inv[slot]))
        {
            if (!done_anything)
                mpr("You combine your ammunition.");

            inc_inv_item_quantity(slot, you.inv[i].quantity, true);
            dec_inv_item_quantity(i, you.inv[i].quantity);
            done_anything = true;
        }
    }
}

void throw_noise(actor* act, const bolt &pbolt, const item_def &ammo)
{
    const item_def* launcher = act->weapon();

    if (launcher == NULL || launcher->base_type != OBJ_WEAPONS)
        return;

    if (is_launched(act, launcher, ammo) != LRET_LAUNCHED)
        return;

    // Throwing and blowguns are silent...
    int         level = 0;
    const char* msg   = NULL;

    switch(launcher->sub_type)
    {
    case WPN_BLOWGUN:
        return;

    case WPN_SLING:
        level = 1;
        msg   = "You hear a whirring sound.";
        break;
     case WPN_BOW:
        level = 5;
        msg   = "You hear a twanging sound.";
        break;
     case WPN_LONGBOW:
        level = 6;
        msg   = "You hear a loud twanging sound.";
        break;
     case WPN_CROSSBOW:
        level = 7;
        msg   = "You hear a thunk.";
        break;

    default:
        DEBUGSTR("Invalid launcher '%s'",
                 launcher->name(DESC_PLAIN).c_str());
        return;
    }
    if (act->atype() == ACT_PLAYER || you.can_see(act))
        msg = NULL;

    noisy(level, act->pos(), msg, act->mindex());
}

// throw_it - currently handles player throwing only.  Monster
// throwing is handled in mon-act:_mons_throw()
// Note: If teleport is true, assume that pbolt is already set up,
// and teleport the projectile onto the square.
//
// Return value is only relevant if dummy_target is non-NULL, and returns
// true if dummy_target is hit.
bool throw_it(bolt &pbolt, int throw_2, bool teleport, int acc_bonus,
              dist *target)
{
    dist thr;
    int shoot_skill = 0;
    bool ammo_ided = false;

    // launcher weapon sub-type
    weapon_type lnchType;

    int baseHit      = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;  // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;  // special add from launcher
    int exHitBonus   = 0, exDamBonus = 0;    // 'extra' bonus from skill/dex/str
    int effSkill     = 0;        // effective launcher skill
    int dice_mult    = 100;
    bool returning   = false;    // Item can return to pack.
    bool did_return  = false;    // Returning item actually does return to pack.
    int slayDam      = 0;

    if (you.confused())
        thr.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
    else if (target)
        thr = *target;
    else
    {
        direction_chooser_args args;
        args.mode = TARG_HOSTILE;
        direction(thr, args);

        if (!thr.isValid)
        {
            if (thr.isCancel)
                canned_msg(MSG_OK);

            return (false);
        }
    }
    pbolt.set_target(thr);

    item_def& thrown = you.inv[throw_2];
    ASSERT(thrown.is_valid());

    // Figure out if we're thrown or launched.
    const launch_retval projected = is_launched(&you, you.weapon(), thrown);

    // Making a copy of the item: changed only for venom launchers.
    item_def item = thrown;
    item.quantity = 1;
    item.slot     = index_to_letter(item.link);

    // Items that get a temporary brand from a player spell lose the
    // brand as soon as the player lets go of the item.  Can't call
    // unwield_item() yet since the beam might get canceled.
    if (you.duration[DUR_WEAPON_BRAND] && projected != LRET_LAUNCHED
        && throw_2 == you.equip[EQ_WEAPON])
    {
        set_item_ego_type(item, OBJ_WEAPONS, SPWPN_NORMAL);
    }

    std::string ammo_name;

    if (setup_missile_beam(&you, pbolt, item, ammo_name, returning))
    {
        you.turn_is_over = false;
        return (false);
    }

    // Did we know the ammo's brand before throwing it?
    const bool ammo_brand_known = item_type_known(thrown);

    // Get the ammo/weapon type.  Convenience.
    const object_class_type wepClass = thrown.base_type;
    const int               wepType  = thrown.sub_type;

    // Determine range.
    int max_range = 0;
    int range = 0;

    if (projected)
    {
        if (wepType == MI_LARGE_ROCK)
        {
            range     = 1 + random2( you.strength() / 5 );
            max_range = you.strength() / 5;
            if (you.can_throw_large_rocks())
            {
                range     += random_range(4, 7);
                max_range += 7;
            }
        }
        else if (wepType == MI_THROWING_NET)
        {
            max_range = range = 2 + you.body_size(PSIZE_BODY);
        }
        else
        {
            max_range = range = LOS_RADIUS;
        }
    }
    else
    {
        // Range based on mass & strength, between 1 and 9.
        max_range = range = std::max(you.strength()-item_mass(thrown)/10 + 3, 1);
    }

    range = std::min(range, LOS_RADIUS);
    max_range = std::min(max_range, LOS_RADIUS);

    // For the tracer, use max_range. For the actual shot, use range.
    pbolt.range = max_range;

    // Save the special explosion (exploding missiles) for later.
    // Need to clear this if unknown to avoid giving away the explosion.
    bolt* expl = pbolt.special_explosion;
    if (!pbolt.effect_known)
        pbolt.special_explosion = NULL;

    // Don't do the tracing when using Portaled Projectile, or when confused.
    if (!teleport && !you.confused())
    {
        // Set values absurdly high to make sure the tracer will
        // complain if we're attempting to fire through allies.
        pbolt.hit    = 100;
        pbolt.damage = dice_def(1, 100);

        // Init tracer variables.
        pbolt.foe_info.reset();
        pbolt.friend_info.reset();
        pbolt.foe_ratio = 100;
        pbolt.is_tracer = true;

        pbolt.fire();

        // Should only happen if the player answered 'n' to one of those
        // "Fire through friendly?" prompts.
        if (pbolt.beam_cancelled)
        {
            canned_msg(MSG_OK);
            you.turn_is_over = false;
            if (pbolt.special_explosion != NULL)
                delete pbolt.special_explosion;
            return (false);
        }
        pbolt.hit    = 0;
        pbolt.damage = dice_def();
    }
    pbolt.is_tracer = false;

    // Reset values.
    pbolt.range = range;
    pbolt.special_explosion = expl;

    bool unwielded = false;
    if (throw_2 == you.equip[EQ_WEAPON] && thrown.quantity == 1)
    {
        if (!wield_weapon(true, PROMPT_GOT_SPECIAL, true, false, false))
            return (false);

        unwielded = true;
    }

    // Now start real firing!
    origin_set_unknown(item);

    if (is_blood_potion(item) && thrown.quantity > 1)
    {
        // Initialise thrown potion with oldest potion in stack.
        long val = remove_oldest_blood_potion(thrown);
        val -= you.num_turns;
        item.props.clear();
        init_stack_blood_potions(item, val);
    }

    // Even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    if (!teleport)
        pbolt.set_target(thr);

    // Get the launcher class, type.  Convenience.
    if (!you.weapon())
        lnchType = NUM_WEAPONS;
    else
        lnchType = static_cast<weapon_type>(you.weapon()->sub_type);

    // baseHit and damage for generic objects
    baseHit = std::min(0, you.strength() - item_mass(item) / 10);
    baseDam = item_mass(item) / 100;

    // special: might be throwing generic weapon;
    // use base wep. damage, w/ penalty
    if (wepClass == OBJ_WEAPONS)
        baseDam = std::max(0, property(item, PWPN_DAMAGE) - 4);

    // Extract weapon/ammo bonuses due to magic.
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    int bow_brand = SPWPN_NORMAL;

    if (projected == LRET_LAUNCHED)
        bow_brand = get_weapon_brand(*you.weapon());

    int ammo_brand = get_ammo_brand( item );

    if (projected == LRET_FUMBLED)
    {
        // See comment in setup_missile_beam.  Why is this duplicated?
        ammo_brand = SPMSL_NORMAL;
    }

    // CALCULATIONS FOR LAUNCHED WEAPONS
    if (projected == LRET_LAUNCHED)
    {
        const item_def &launcher = *you.weapon();

        // Extract launcher bonuses due to magic.
        lnchHitBonus = launcher.plus;
        lnchDamBonus = launcher.plus2;

        const int item_base_dam = property( item, PWPN_DAMAGE );
        const int lnch_base_dam = property( launcher, PWPN_DAMAGE );

        const skill_type launcher_skill = range_skill( launcher );

        baseHit = property( launcher, PWPN_HIT );
        baseDam = lnch_base_dam + random2(1 + item_base_dam);

        // Slings are terribly weakened otherwise.
        if (lnch_base_dam == 0)
            baseDam = item_base_dam;

        // If we've a zero base damage + an elemental brand, up the damage
        // slightly so the brand has something to work with. This should
        // only apply to needles.
        if (!baseDam && elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to consider
        // this beam "needle-like". (XXX)
        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
            pbolt.ench_power = AUTOMATIC_HIT;

        dprf(   "Base hit == %d; Base damage == %d "
                "(item %d + launcher %d)",
                        baseHit, baseDam,
                        item_base_dam, lnch_base_dam);

        // Fix ammo damage bonus, since missiles only use inv_plus.
        ammoDamBonus = ammoHitBonus;

        // Check for matches; dwarven, elven, orcish.
        if (!(get_equip_race(*you.weapon()) == 0))
        {
            if (get_equip_race(*you.weapon()) == get_equip_race(item))
            {
                baseHit++;
                baseDam++;

                // elves with elven bows
                if (get_equip_race(*you.weapon()) == ISFLAG_ELVEN
                    && player_genus(GENPC_ELVEN))
                {
                    baseHit++;
                }
            }
        }

        // Lower accuracy if held in a net.
        if (you.attribute[ATTR_HELD])
            baseHit--;

        // For all launched weapons, maximum effective specific skill
        // is twice throwing skill.  This models the fact that no matter
        // how 'good' you are with a bow, if you know nothing about
        // trajectories you're going to be a damn poor bowman.  Ditto
        // for crossbows and slings.

        // [dshaligram] Throwing now two parts launcher skill, one part
        // ranged combat. Removed the old model which is... silly.

        // [jpeg] Throwing now only affects actual throwing weapons,
        // i.e. not launched ones. (Sep 10, 2007)

        shoot_skill = you.skills[launcher_skill];
        effSkill    = shoot_skill;

        const int speed = launcher_final_speed(launcher, you.shield());
        dprf("Final launcher speed: %d", speed);
        you.time_taken = speed * you.time_taken / 100;

        // [dshaligram] Improving missile weapons:
        //  - Remove the strength/enchantment cap where you need to be strong
        //    to exploit a launcher bonus.
        //  - Add on launcher and missile pluses to extra damage.

        // [dshaligram] This can get large...
        exDamBonus = lnchDamBonus + random2(1 + ammoDamBonus);
        exDamBonus = (exDamBonus > 0   ? random2(exDamBonus + 1)
                                       : -random2(-exDamBonus + 1));
        exHitBonus = (lnchHitBonus > 0 ? random2(lnchHitBonus + 1)
                                       : -random2(-lnchHitBonus + 1));

        // Identify ammo type if the information is there. Note
        // that the bow is always type-identified because it's
        // wielded.
        if (determines_ammo_brand(bow_brand, ammo_brand))
        {
            set_ident_flags(item, ISFLAG_KNOW_TYPE);
            if (ammo_brand != SPMSL_NORMAL)
            {
                set_ident_flags(you.inv[throw_2], ISFLAG_KNOW_TYPE);
                ammo_ided = true;
            }
        }

        // Removed 2 random2(2)s from each of the learning curves, but
        // left slings because they're hard enough to develop without
        // a good source of shot in the dungeon.
        switch (launcher_skill)
        {
        case SK_SLINGS:
        {
            // Slings are really easy to learn because they're not
            // really all that good, and it's harder to get ammo anyway.
            exercise(SK_SLINGS, 1 + random2avg(3, 2));

            // Sling bullets are designed for slinging and easier to aim.
            if (wepType == MI_SLING_BULLET)
                baseHit += 4;

            exHitBonus += (effSkill * 3) / 2;

            // Strength is good if you're using a nice sling.
            int strbonus = (10 * (you.strength() - 10)) / 9;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // cap
            strbonus = std::min(lnchDamBonus + 1, strbonus);

            exDamBonus += strbonus;
            // Add skill for slings... helps to find those vulnerable spots.
            dice_mult = dice_mult * (14 + random2(1 + effSkill)) / 14;

            // Now kill the launcher damage bonus.
            lnchDamBonus = std::min(0, lnchDamBonus);
            break;
        }

        // Blowguns take a _very_ steady hand; a lot of the bonus
        // comes from dexterity.  (Dex bonus here as well as below.)
        case SK_THROWING:
            baseHit -= 2;
            exercise(SK_THROWING, (coinflip()? 2 : 1));
            exHitBonus += (effSkill * 3) / 2 + you.dex() / 2;

            // No extra damage for blowguns.
            // exDamBonus = 0;

            // Now kill the launcher damage and ammo bonuses.
            lnchDamBonus = std::min(0, lnchDamBonus);
            ammoDamBonus = std::min(0, ammoDamBonus);
            break;

        case SK_BOWS:
        {
            baseHit -= 3;
            exercise(SK_BOWS, (coinflip()? 2 : 1));
            exHitBonus += (effSkill * 2);

            // Strength is good if you're using a nice bow.
            int strbonus = (10 * (you.strength() - 10)) / 4;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // Cap; reduced this cap, because we don't want to allow
            // the extremely-strong to quadruple the enchantment bonus.
            strbonus = std::min(lnchDamBonus + 1, strbonus);

            exDamBonus += strbonus;

            // Add in skill for bows - helps you to find those vulnerable spots.
            // exDamBonus += effSkill;

            dice_mult = dice_mult * (17 + random2(1 + effSkill)) / 17;

            // Now kill the launcher damage bonus.
            lnchDamBonus = std::min(0, lnchDamBonus);
            break;
        }
            // Crossbows are easy for unskilled people.

        case SK_CROSSBOWS:
            exercise(SK_CROSSBOWS, (coinflip()? 2 : 1));
            baseHit++;
            exHitBonus += (3 * effSkill) / 2 + 6;
            // exDamBonus += effSkill * 2 / 3 + 4;

            dice_mult = dice_mult * (22 + random2(1 + effSkill)) / 22;

        default:
            break;
        }

        if (bow_brand == SPWPN_VORPAL)
        {
            // Vorpal brand adds 30% damage bonus. Increased from 25%
            // because at 25%, vorpal brand is completely inferior to
            // speed. At 30% it's marginally better than speed when
            // fighting monsters with very heavy armour.
            dice_mult = dice_mult * 130 / 100;
        }

        if (ammo_brand == SPMSL_STEEL)
            dice_mult = dice_mult * 150 / 100;

        // ID check. Can't ID off teleported projectiles, uh, because
        // it's too weird. Also it messes up the messages.
        if (item_ident(*you.weapon(), ISFLAG_KNOW_PLUSES))
        {
            if (!teleport
                && !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES)
                && x_chance_in_y(shoot_skill, 100))
            {
                set_ident_flags( item, ISFLAG_KNOW_PLUSES );
                set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_PLUSES );
                ammo_ided = true;
                identify_floor_missiles_matching(item, ISFLAG_KNOW_PLUSES);
                mprf("You are firing %s.",
                     you.inv[throw_2].name(DESC_NOCAP_A).c_str());
            }
        }
        else if (!teleport && x_chance_in_y(shoot_skill, 100))
        {
            item_def& weapon = *you.weapon();
            set_ident_flags(weapon, ISFLAG_KNOW_PLUSES);

            mprf("You are wielding %s.", weapon.name(DESC_NOCAP_A).c_str());

            more();
            you.wield_change = true;
        }
    }

    // check for returning ammo from launchers
    if (returning && projected == LRET_LAUNCHED)
    {
        switch (lnchType)
        {
            case WPN_CROSSBOW:
                if (returning && !one_chance_in(1 + skill_bump(SK_CROSSBOWS)))
                    did_return = true;
                break;
            case WPN_SLING:
                if (returning && !one_chance_in(1 + skill_bump(SK_SLINGS)))
                    did_return = true;
                break;
            case WPN_BOW:
            case WPN_LONGBOW:
                if (returning && !one_chance_in(1 + skill_bump(SK_BOWS)))
                    did_return = true;
                break;
            case WPN_BLOWGUN:
                if (returning && !one_chance_in(1 + skill_bump(SK_THROWING)))
                    did_return = true;
                break;
            default:
                break;
        }
    }

    // CALCULATIONS FOR THROWN WEAPONS
    if (projected == LRET_THROWN)
    {
        returning = returning && !teleport;

        if (returning && !one_chance_in(1 + skill_bump(SK_THROWING)))
            did_return = true;

        baseHit = 0;

        // Missiles only use inv_plus.
        if (wepClass == OBJ_MISSILES)
            ammoDamBonus = ammoHitBonus;

        // All weapons that use 'throwing' go here.
        if (wepClass == OBJ_WEAPONS
            || (wepClass == OBJ_MISSILES
                && (wepType == MI_STONE || wepType == MI_LARGE_ROCK
                    || wepType == MI_DART || wepType == MI_JAVELIN)))
        {
            // Elves with elven weapons.
            if (get_equip_race(item) == ISFLAG_ELVEN
                && player_genus(GENPC_ELVEN))
            {
                baseHit++;
            }

            // Give an appropriate 'tohit':
            // * hand axes and clubs are -5
            // * daggers are +1
            // * spears are -1
            // * rocks are 0
            if (wepClass == OBJ_WEAPONS)
            {
                switch (wepType)
                {
                    case WPN_DAGGER:
                        baseHit++;
                        break;
                    case WPN_SPEAR:
                        baseHit--;
                        break;
                    default:
                        baseHit -= 5;
                        break;
                }
            }
            else if (wepClass == OBJ_MISSILES)
            {
                switch (wepType)
                {
                    case MI_DART:
                        baseHit += 2;
                        break;
                    case MI_JAVELIN:
                        baseHit++;
                        break;
                    default:
                        break;
                }
            }

            exHitBonus = you.skills[SK_THROWING] * 2;

            baseDam = property(item, PWPN_DAMAGE);

            // [dshaligram] The defined base damage applies only when used
            // for launchers. Hand-thrown stones do only half
            // base damage. Yet another evil 4.0ism.
            if (wepClass == OBJ_MISSILES && wepType == MI_STONE)
            {
                baseDam = div_rand_round(baseDam, 2);
            }

            // Dwarves/orcs with dwarven/orcish weapons.
            if (get_equip_race(item) == ISFLAG_DWARVEN
                   && player_genus(GENPC_DWARVEN)
                || get_equip_race(item) == ISFLAG_ORCISH
                   && you.species == SP_HILL_ORC)
            {
                baseDam++;
            }

            exDamBonus =
                (10 * (you.skills[SK_THROWING] / 2 + you.strength() - 10)) / 12;

            // Now, exDamBonus is a multiplier.  The full multiplier
            // is applied to base damage, but only a third is applied
            // to the magical modifier.
            exDamBonus = (exDamBonus * (3 * baseDam + ammoDamBonus)) / 30;
        }

        if (wepClass == OBJ_MISSILES)
        {
            // Identify ammo type.
            set_ident_flags(you.inv[throw_2], ISFLAG_KNOW_TYPE);
            ammo_ided = true;

            switch (wepType)
            {
            case MI_LARGE_ROCK:
                if (you.can_throw_large_rocks())
                    baseHit = 1;
                break;

            case MI_DART:
                // Darts also using throwing skills, now.
                exHitBonus += skill_bump(SK_THROWING);
                exDamBonus += you.skills[SK_THROWING] * 3 / 5;

                // exercise skills
                exercise(SK_THROWING, 1 + random2avg(3, 2));
                break;

            case MI_JAVELIN:
                // Javelins use throwing skill.
                exHitBonus += skill_bump(SK_THROWING);
                exDamBonus += you.skills[SK_THROWING] * 3 / 5;

                // Adjust for strength and dex.
                exDamBonus = str_adjust_thrown_damage(exDamBonus);
                exHitBonus = dex_adjust_thrown_tohit(exHitBonus);

                // High dex helps damage a bit, too (aim for weak spots).
                exDamBonus = stat_adjust(exDamBonus, you.dex(), 20, 150, 100);

                // Javelins train throwing quickly.
                exercise(SK_THROWING, 1 + coinflip());
                break;

            case MI_THROWING_NET:
                // Nets use throwing skill.  They don't do any damage!
                baseDam = 0;
                exDamBonus = 0;
                ammoDamBonus = 0;

                // ...but accuracy is important for this one.
                baseHit = 1;
                exHitBonus += (skill_bump(SK_THROWING) * 7 / 2);
                // Adjust for strength and dex.
                exHitBonus = dex_adjust_thrown_tohit(exHitBonus);

                // Nets train throwing.
                exercise(SK_THROWING, 1);
                break;
            }
        }

        // exercise skill
        if (coinflip())
            exercise(SK_THROWING, 1);

        // ID check
        if (!teleport
            && !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES)
            && x_chance_in_y(you.skills[SK_THROWING], 100))
        {
            set_ident_flags( item, ISFLAG_KNOW_PLUSES );
            set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_PLUSES );
            identify_floor_missiles_matching(item, ISFLAG_KNOW_PLUSES);
            ammo_ided = true;
            mprf("You are throwing %s.",
                 you.inv[throw_2].name(DESC_NOCAP_A).c_str());
        }
    }

    if (pbolt.flavour != BEAM_MISSILE)
        // [dshaligram] Branded arrows are much stronger.
        dice_mult = (dice_mult * 150) / 100;

    // Dexterity bonus, and possible skill increase for silly throwing.
    if (projected)
    {
        if (wepType != MI_LARGE_ROCK && wepType != MI_THROWING_NET)
        {
            exHitBonus += you.dex() / 2;

            // slaying bonuses
            if (wepType != MI_NEEDLE)
            {
                slayDam = slaying_bonus(PWPN_DAMAGE, true);
                slayDam = (slayDam < 0 ? -random2(1 - slayDam)
                                       :  random2(1 + slayDam));
            }

            exHitBonus += slaying_bonus(PWPN_HIT, true);
        }
    }
    else // LRET_FUMBLED
    {
        if (one_chance_in(20))
            exercise(SK_THROWING, 1);

        exHitBonus = you.dex() / 4;
    }

    // FINALISE tohit and damage
    if (exHitBonus >= 0)
        pbolt.hit = baseHit + random2avg(exHitBonus + 1, 2);
    else
        pbolt.hit = baseHit - random2avg(0 - (exHitBonus - 1), 2);

    if (exDamBonus >= 0)
        pbolt.damage = dice_def(1, baseDam + random2(exDamBonus + 1));
    else
        pbolt.damage = dice_def(1, baseDam - random2(0 - (exDamBonus - 1)));

    pbolt.damage.size  = dice_mult * pbolt.damage.size / 100;
    pbolt.damage.size += slayDam;

    // Only add bonuses if we're throwing something sensible.
    if (projected || wepClass == OBJ_WEAPONS)
    {
        pbolt.hit += ammoHitBonus + lnchHitBonus;
        pbolt.damage.size += ammoDamBonus + lnchDamBonus;
    }

    // Add in bonus (only from Portal Projectile for now).
    if (acc_bonus != DEBUG_COOKIE)
        pbolt.hit += acc_bonus;

    scale_dice(pbolt.damage);

    dprf(   "H:%d+%d;a%dl%d.  D:%d+%d;a%dl%d -> %d,%dd%d",
              baseHit, exHitBonus, ammoHitBonus, lnchHitBonus,
              baseDam, exDamBonus, ammoDamBonus, lnchDamBonus,
              pbolt.hit, pbolt.damage.num, pbolt.damage.size );

    // Create message.
    mprf( "%s %s%s %s.",
          teleport  ? "Magically, you" : "You",
          projected ? "" : "awkwardly ",
          projected == LRET_LAUNCHED ? "shoot" : "throw",
          ammo_name.c_str() );

    // Ensure we're firing a 'missile'-type beam.
    pbolt.is_beam   = false;
    pbolt.is_tracer = false;

    // Mark this item as thrown if it's a missile, so that we'll pick it up
    // when we walk over it.
    if (wepClass == OBJ_MISSILES || wepClass == OBJ_WEAPONS)
        item.flags |= ISFLAG_THROWN;

    bool hit = false;
    if (teleport)
    {
        // Violating encapsulation somewhat...oh well.
        pbolt.use_target_as_pos = true;
        pbolt.affect_cell();
        pbolt.affect_endpoint();
        if (!did_return && acc_bonus != DEBUG_COOKIE)
            pbolt.drop_object();
    }
    else
    {
        if (Hints.hints_left)
            Hints.hints_throw_counter++;

        // Dropping item copy, since the launched item might be different.
        pbolt.drop_item = !did_return;
        pbolt.fire();

        // The item can be destroyed before returning.
        if (did_return && thrown_object_destroyed(&item, pbolt.target))
            did_return = false;
    }

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
    {
        did_god_conduct(DID_CHAOS, 2 + random2(3),
                        bow_brand == SPWPN_CHAOS || ammo_brand_known);
    }

    if (bow_brand == SPWPN_REAPING || ammo_brand == SPMSL_REAPING)
    {
        did_god_conduct(DID_NECROMANCY, 2,
                        bow_brand == SPWPN_REAPING || ammo_brand_known);
        did_god_conduct(DID_CORPSE_VIOLATION, 2,
                        bow_brand == SPWPN_REAPING || ammo_brand_known);
    }

    if (ammo_brand == SPMSL_RAGE)
        did_god_conduct(DID_HASTY, 6 + random2(3), ammo_brand_known);

    if (did_return)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow(false);
        pbolt.fire();

        msg::stream << item.name(DESC_CAP_THE) << " returns to your pack!"
                    << std::endl;

        // Player saw the item return.
        if (!is_artefact(you.inv[throw_2]))
        {
            // Since this only happens for non-artefacts, also mark properties
            // as known.
            set_ident_flags(you.inv[throw_2],
                            ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES);
        }
    }
    else
    {
        // Should have returned but didn't.
        if (returning && item_type_known(you.inv[throw_2]))
        {
            msg::stream << item.name(DESC_CAP_THE)
                        << " fails to return to your pack!" << std::endl;
        }
        dec_inv_item_quantity(throw_2, 1);
        if (unwielded)
            canned_msg(MSG_EMPTY_HANDED);
    }

    throw_noise(&you, pbolt, thrown);

    // ...any monster nearby can see that something has been thrown, even
    // if it didn't make any noise.
    alert_nearby_monsters();

    if (ammo_ided)
        _merge_ammo_in_inventory(throw_2);

    you.turn_is_over = true;

    if (pbolt.special_explosion != NULL)
        delete pbolt.special_explosion;

    return (hit);
}

bool thrown_object_destroyed(item_def *item, const coord_def& where)
{
    ASSERT(item != NULL);

    std::string name = item->name(DESC_PLAIN, false, true, false);

    // Exploding missiles are always destroyed.
    if (name.find("explod") != std::string::npos)
        return (true);

    int chance = 0;

    if (item->base_type == OBJ_MISSILES)
    {
        int brand = get_ammo_brand(*item);
        if (brand == SPMSL_CHAOS || brand == SPMSL_DISPERSAL)
            return (true);

        // [dshaligram] Removed influence of Throwing on ammo preservation.
        // The effect is nigh impossible to perceive.
        switch (item->sub_type)
        {
        case MI_NEEDLE:
            chance = (brand == SPMSL_CURARE ? 3 : 6);
            break;

        case MI_SLING_BULLET:
        case MI_STONE:
        case MI_ARROW:
        case MI_BOLT:
            chance = 4;
            break;

        case MI_DART:
            chance = 3;
            break;

        case MI_JAVELIN:
            chance = 10;
            break;

        case MI_THROWING_NET:
            // Doesn't get destroyed by throwing.
            break;

        case MI_LARGE_ROCK:
        default:
            chance = 25;
            break;
        }

        if (brand == SPMSL_STEEL)
            chance *= 10;
        if (brand == SPMSL_FLAME)
            chance /= 2;
        if (brand == SPMSL_FROST)
            chance /= 2;
        if (brand == SPMSL_REAPING)
            chance /= 4;
    }

    // Enchanted projectiles get an extra shot at avoiding
    // destruction: plus / (3 + plus) chance of survival.
    bool destroyed = (chance == 0) ? false : (one_chance_in(chance)
                                              && x_chance_in_y(3, item->plus + 3));

    return (destroyed);
}

static int _prompt_ring_to_remove(int new_ring)
{
    const item_def *left  = you.slot_item(EQ_LEFT_RING, true);
    const item_def *right = you.slot_item(EQ_RIGHT_RING, true);

    if (left->cursed() && right->cursed())
    {
        mprf("You're already wearing two cursed rings!");
        return (-1);
    }

    mesclr();
    mprf("Wearing %s.", you.inv[new_ring].name(DESC_NOCAP_A).c_str());

    const char lslot = index_to_letter(left->link);
    const char rslot = index_to_letter(right->link);

    mprf(MSGCH_PROMPT,
         "You're wearing two rings. Remove which one? (%c/%c/<</>/Esc)",
         lslot, rslot);

    mprf(" << or %s", left->name(DESC_INVENTORY).c_str());
    mprf(" > or %s", right->name(DESC_INVENTORY).c_str());
    flush_prev_message();

    // Deactivate choice from tile inventory.
    // FIXME: We need to be able to get the choice (item letter)
    //        *without* the choice taking action by itself!
    mouse_control mc(MOUSE_MODE_MORE);
    int c;
    do
        c = getchm();
    while (c != lslot && c != rslot && c != '<' && c != '>'
           && c != ESCAPE && c != ' ');

    mesclr();

    if (c == ESCAPE || c == ' ')
        return (-1);

    const int eqslot = (c == lslot || c == '<') ? EQ_LEFT_RING
                                                : EQ_RIGHT_RING;

    if (!check_warning_inscriptions(you.inv[you.equip[eqslot]], OPER_REMOVE))
        return (-1);

    return (you.equip[eqslot]);
}

// Checks whether a to-be-worn or to-be-removed item affects
// character stats and whether wearing/removing it could be fatal.
// If so, warns the player, or just returns false if quiet is true.
bool safe_to_remove_or_wear(const item_def &item, bool remove,
                            bool quiet)
{
    bool prop_lev = false;
    bool fatal_liquid = false;
    dungeon_feature_type gridhere = grd(you.pos());

    // Don't warn when putting on an unknown item.
    if (item.base_type == OBJ_JEWELLERY
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        switch (item.sub_type)
        {
        case RING_LEVITATION:
            prop_lev = true;
            break;
        default:
            break;
        }
    }

    if (item.base_type == OBJ_ARMOUR && item_type_known(item))
    {
        switch (item.special)
        {
        case SPARM_LEVITATION:
            prop_lev = true;
            break;

        default:
            break;
        }
    }

    if (is_artefact(item))
    {
        prop_lev = prop_lev || artefact_known_wpn_property(item, ARTP_LEVITATE);

        if (!remove && artefact_known_wpn_property(item, ARTP_EYESIGHT))
        {
            // We might have to turn autopickup back on again.
            // This is not optimal, in that it could also happen if we do
            // not know the property (in which case it should become known).
            autotoggle_autopickup(false);
        }
    }

    // If we are levitating and about to remove a levitation-granting item
    // make sure this doesn't cause us to drown or die to stat loss.
    if (prop_lev && remove && you.is_levitating())
    {
        if (you.species == SP_MERFOLK && feat_is_water(gridhere))
        {
            fatal_liquid = false;
        }

        if (gridhere == DNGN_LAVA
            || (gridhere == DNGN_DEEP_WATER && !beogh_water_walk()
                && !you.swimming()))
        {
            // We'd fall into water!  See if we would drown.
            coord_def empty;

            // Check if we'd be able to scramble out.
            if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE
                || (carrying_capacity() / 2) <= you.burden
                || !empty_surrounds(you.pos(), DNGN_FLOOR, 1, false, empty))
            {
                fatal_liquid = true;
            }

            if (gridhere == DNGN_LAVA)
            {
                int res = player_res_fire(false);

                if (res <= 0 || (110 / res) >= you.hp)
                    fatal_liquid = true;
            }
        }

        if (you.permanent_levitation()
            && (item.base_type != OBJ_ARMOUR || item.sub_type != ARM_BOOTS))
        {
            fatal_liquid = false; // we won't fall!
        }
    }

    if (remove)
    {
        if (prop_lev && fatal_liquid)
        {
            if (!quiet)
            {
                mprf(MSGCH_WARN, "%s this item would be fatal, so you refuse "
                                 "to do that.",
                                 (item.base_type == OBJ_WEAPONS ? "Unwielding"
                                                                : "Removing"));
            }
            return (false);
        }
    }
    return (true);
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

    if (lring->cursed() && rring->cursed())
    {
        mprf("You're already wearing two cursed rings!");
        return (false);
    }

    int unwanted;

    // Don't prompt if both rings are of the same type.
    if (lring->sub_type == rring->sub_type
        && lring->plus == rring->plus
        && lring->plus2 == rring->plus2
        && !is_artefact(*lring) && !is_artefact(*rring))
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
        return (false);
    }

    if (!remove_ring(unwanted, false))
        return (false);

    // Check that the new ring won't kill us.
    if (!safe_to_remove_or_wear(you.inv[ring_slot], false))
        return (false);

    // Put on the new ring.
    start_delay(DELAY_JEWELLERY_ON, 1, ring_slot);

    return (true);
}

bool puton_item(int item_slot)
{
    item_def& item = you.inv[item_slot];

    if (item_slot == you.equip[EQ_LEFT_RING]
        || item_slot == you.equip[EQ_RIGHT_RING]
        || item_slot == you.equip[EQ_AMULET])
    {
        // "Putting on" an equipped item means taking it off.
        if (Options.equip_unequip)
            return (!remove_ring(item_slot));
        else
        {
            mpr("You're already wearing that object!");
            return (false);
        }
    }

    if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are wielding that object.");
        return (false);
    }

    if (item.base_type != OBJ_JEWELLERY)
    {
        mpr("You can only put on jewellery.");
        return (false);
    }

    const bool lring = (you.slot_item(EQ_LEFT_RING, true)  != NULL);
    const bool rring = (you.slot_item(EQ_RIGHT_RING, true) != NULL);
    const bool is_amulet = jewellery_is_amulet(item);

    if (!is_amulet)     // i.e. it's a ring
    {
        const item_def* gloves = you.slot_item(EQ_GLOVES, false);
        // Cursed gloves cannot be removed.
        if (gloves && gloves->cursed())
        {
            mpr("You can't take your gloves off to put on a ring!");
            return (false);
        }

        if (lring && rring)
            return _swap_rings(item_slot);
    }
    else if (item_def* amulet = you.slot_item(EQ_AMULET, true))
    {
        // Remove the previous one.
        if (!check_warning_inscriptions(*amulet, OPER_REMOVE)
            || !remove_ring(you.equip[EQ_AMULET], true))
        {
            return (false);
        }

        // Check that the new amulet won't kill us.
        if (!safe_to_remove_or_wear(item, false))
            return (false);

        // Put on the new amulet.
        start_delay(DELAY_JEWELLERY_ON, 1, item_slot);

        // Assume it's going to succeed.
        return (true);
    }

    // Check that it won't kill us.
    if (!safe_to_remove_or_wear(item, false))
        return (false);

    equipment_type hand_used;

    if (is_amulet)
    {
        hand_used = EQ_AMULET;
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

    if (Hints.hints_left && your_talents(false).size() > old_talents)
        learned_something_new(HINT_NEW_ABILITY_ITEM);

    // Putting on jewellery is as fast as wielding weapons.
    you.time_taken /= 2;
    you.turn_is_over = true;

    return (true);
}

bool puton_ring(int slot)
{
    if (!player_can_handle_equipment())
    {
        mpr("You can't put on anything in your present form.");
        return (false);
    }

    int item_slot;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item( "Put on which piece of jewellery?",
                                        MT_INVLIST, OBJ_JEWELLERY, true, true,
                                        true, 0, -1, NULL, OPER_PUTON );
    }

    if (prompt_failed(item_slot))
        return (false);

    return puton_item(item_slot);
}

bool remove_ring(int slot, bool announce)
{
    if (!player_can_handle_equipment())
    {
        mpr("You can't wear or remove anything in your present form.");
        return (false);
    }

    equipment_type hand_used = EQ_NONE;
    int ring_wear_2;

    const bool left  = player_wearing_slot(EQ_LEFT_RING);
    const bool right = player_wearing_slot(EQ_RIGHT_RING);
    const bool amu   = player_wearing_slot(EQ_AMULET);
    if (!left && !right && !amu)
    {
        mpr("You aren't wearing any rings or amulets.");
        return (false);
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    const item_def* gloves = you.slot_item(EQ_GLOVES);
    const bool gloves_cursed = gloves && gloves->cursed();
    if (gloves_cursed && !amu)
    {
        mpr("You can't take your gloves off to remove any rings!");
        return (false);
    }

    if (left && !right && !amu)
        hand_used = EQ_LEFT_RING;
    else if (!left && right && !amu)
        hand_used = EQ_RIGHT_RING;
    else if (!left && !right && amu)
        hand_used = EQ_AMULET;

    if (hand_used == EQ_NONE)
    {
        const int equipn =
            (slot == -1)? prompt_invent_item("Remove which piece of jewellery?",
                                             MT_INVLIST,
                                             OBJ_JEWELLERY, true, true, true,
                                             0, -1, NULL, OPER_REMOVE)
                        : slot;

        if (prompt_failed(equipn))
            return (false);

        hand_used = item_equip_slot(you.inv[equipn]);
        if (hand_used == EQ_NONE)
        {
            mpr("You aren't wearing that.");
            return (false);
        }
        else if (you.inv[equipn].base_type != OBJ_JEWELLERY)
        {
            mpr("That isn't a piece of jewellery.");
            return (false);
        }
    }

    if (you.equip[hand_used] == -1)
    {
        mpr("I don't think you really meant that.");
        return (false);
    }
    else if (you.melded[hand_used])
    {
        mpr("You can't take that off while it's melded.");
        return (false);
    }
    else if (gloves_cursed
             && (hand_used == EQ_LEFT_RING || hand_used == EQ_RIGHT_RING))
    {
        mpr("You can't take your gloves off to remove any rings!");
        return (false);
    }

    if (!check_warning_inscriptions(you.inv[you.equip[hand_used]],
                                    OPER_REMOVE))
    {
        canned_msg(MSG_OK);
        return (false);
    }

    if (you.inv[you.equip[hand_used]].cursed())
    {
        if (announce)
        {
            mprf("%s is stuck to you!",
                 you.inv[you.equip[hand_used]].name(DESC_CAP_YOUR).c_str());
        }
        else
            mpr("It's stuck to you!");

        set_ident_flags(you.inv[you.equip[hand_used]], ISFLAG_KNOW_CURSE);
        return (false);
    }

    ring_wear_2 = you.equip[hand_used];

    // Remove the ring.
    if (!safe_to_remove_or_wear(you.inv[ring_wear_2], true))
        return (false);

    mprf("You remove %s.", you.inv[ring_wear_2].name(DESC_NOCAP_YOUR).c_str());
    unequip_item(hand_used);

    you.time_taken /= 2;
    you.turn_is_over = true;

    return (true);
}

int _wand_range(zap_type ztype)
{
    // FIXME: Eventually we should have sensible values here.
    return (8);
}

int _max_wand_range()
{
    return (8);
}

static bool _dont_use_invis()
{
    if (!you.backlit())
        return (false);

    if (you.haloed())
    {
        mpr("You can't turn invisible.");
        return (true);
    }
    else if (get_contamination_level() > 0
             && !yesno("Invisibility will do you no good right now; "
                       "use anyway?", false, 'n'))
    {
        return (true);
    }

    return (false);
}

void zap_wand(int slot)
{
    if (!player_can_handle_equipment())
    {
        canned_msg(MSG_PRESENT_FORM);
        return;
    }

    bolt beam;
    dist zap_wand;
    int item_slot;

    // Unless the character knows the type of the wand, the targeting
    // system will default to enemies. -- [ds]
    targ_mode_type targ_mode = TARG_HOSTILE;

    beam.obvious_effect = false;

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

        case WAND_HEALING:
            if (you.religion == GOD_ELYVILON)
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
    }

    int tracer_range = (alreadyknown && wand.sub_type != WAND_RANDOM_EFFECTS) ?
                        _wand_range(type_zapped) : _max_wand_range();
    const std::string zap_title =
        "Zapping: " + get_menu_colour_prefix_tags(wand, DESC_INVENTORY);
    direction_chooser_args args;
    args.mode = targ_mode;
    args.range = tracer_range;
    args.top_prompt = zap_title;
    direction(zap_wand, args);

    if (!zap_wand.isValid)
    {
        if (zap_wand.isCancel)
            canned_msg(MSG_OK);
        return;
    }

    if (alreadyknown && zap_wand.target == you.pos())
    {
        if (wand.sub_type == WAND_TELEPORTATION
            && item_blocks_teleport(false, false))
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
    {
        zap_wand.target = you.pos() + coord_def(random_range(-6, 6),
                                                random_range(-6, 6));
    }

    if (wand.sub_type == WAND_RANDOM_EFFECTS)
        beam.effect_known = false;

    beam.source   = you.pos();
    beam.attitude = ATT_FRIENDLY;
    beam.set_target(zap_wand);

    const bool aimed_at_self = (beam.target == you.pos());

    // Check whether we may hit friends, use "safe" values for random effects
    // and unknown wands (highest possible range, and unresistable beam
    // flavour). Don't use the tracer if firing at self.
    if (!aimed_at_self)
    {
        beam.range = tracer_range;
        if (!player_tracer(beam.effect_known ? type_zapped
                                             : ZAP_DEBUGGING_RAY,
                           2 * (you.skills[SK_EVOCATIONS] - 1),
                           beam, beam.effect_known ? 0 : 17))
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
        xom_is_stimulated(255);
    }

    // Reset range.
    beam.range = _wand_range(type_zapped);

#ifdef WIZARD
    if (you.wizard)
    {
        std::string str = wand.inscription;
        int wiz_range = strip_number_tag(str, "range:");
        if (wiz_range != TAG_UNFOUND)
            beam.range = wiz_range;
    }
#endif

    // zapping() updates beam.
    zapping( type_zapped, 30 + roll_dice(2, you.skills[SK_EVOCATIONS]), beam );

    // Take off a charge.
    wand.plus--;

    // Zap counts count from the last recharge.
    if (wand.plus2 == ZAPCOUNT_MAX_CHARGED || wand.plus2 == ZAPCOUNT_RECHARGED)
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

        mpr(wand.name(DESC_INVENTORY_EQUIP).c_str());
    }
    else
        set_ident_type(wand, ID_TRIED_TYPE);

    if (item_type_known(wand)
        && (item_ident(wand, ISFLAG_KNOW_PLUSES)
            || you.skills[SK_EVOCATIONS] > 5 + random2(15)))
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

    exercise(SK_EVOCATIONS, 1);
    alert_nearby_monsters();

    if (!alreadyknown && !alreadytried && risky)
    {
        // Xom loves it when you use an unknown wand and there is a
        // dangerous monster nearby...
        xom_is_stimulated(255);
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
    if (you.is_undead == US_UNDEAD)
    {
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

    if (!player_can_handle_equipment())
    {
       canned_msg(MSG_PRESENT_FORM);
       _vampire_corpse_help();
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
        && !berserk_check_wielded_weapon())
    {
        return;
    }

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous = (player_in_a_dangerous_place()
                            && you.experience_level > 1);

    // Identify item and type.
    if (potion_effect(static_cast<potion_type>(potion.sub_type),
                      40, true, alreadyknown))
    {
        set_ident_flags(potion, ISFLAG_IDENT_MASK);
        set_ident_type(potion, ID_KNOWN_TYPE);
    }
    else
    {
        set_ident_type(potion, ID_TRIED_TYPE);
    }

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you drink an unknown potion and there is
        // a dangerous monster nearby...
        xom_is_stimulated(255);
    }

    if (is_blood_potion(potion))
    {
        // Always drink oldest potion.
        remove_oldest_blood_potion(potion);
    }

    dec_inv_item_quantity(slot, 1);
    you.turn_is_over = true;

    if (you.species != SP_VAMPIRE)
        lessen_hunger(40, true);
}

bool _drink_fountain()
{
    const dungeon_feature_type feat = grd(you.pos());

    if (feat < DNGN_FOUNTAIN_BLUE || feat > DNGN_FOUNTAIN_BLOOD)
        return (false);

    if (you.flight_mode() == FL_LEVITATE)
    {
        mpr("You're floating high above the fountain.");
        return (false);
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return (true);
    }

    potion_type fountain_effect = POT_WATER;
    if (feat == DNGN_FOUNTAIN_BLUE)
    {
        if (!yesno("Drink from the fountain?", true, 'n'))
            return (false);

        mpr("You drink the pure, clear water.");
    }
    else if (feat == DNGN_FOUNTAIN_BLOOD)
    {
        if (!yesno("Drink from the fountain of blood?", true, 'n'))
            return (false);

        mpr("You drink the blood.");
        fountain_effect = POT_BLOOD;
    }
    else
    {
        if (!yesno("Drink from the sparkling fountain?", true, 'n'))
            return (false);

        mpr("You drink the sparkling water.");

        fountain_effect = static_cast<potion_type>(
            random_choose_weighted(467, POT_WATER,
                                   48,  POT_DECAY,
                                   40,  POT_MUTATION,
                                   40,  POT_HEALING,
                                   40,  POT_HEAL_WOUNDS,
                                   40,  POT_SPEED,
                                   40,  POT_MIGHT,
                                   40,  POT_AGILITY,
                                   40,  POT_BRILLIANCE,
                                   32,  POT_DEGENERATION,
                                   27,  POT_LEVITATION,
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
                                   4,   POT_GAIN_STRENGTH,
                                   4,   POT_GAIN_INTELLIGENCE,
                                   4,   POT_GAIN_DEXTERITY,
                                   0));
    }

    if (fountain_effect != POT_WATER && fountain_effect != POT_BLOOD)
        xom_is_stimulated(64);

    // Good gods do not punish for bad random effects. However, they do
    // punish drinking from a fountain of blood.
    potion_effect(fountain_effect, 100, true, feat != DNGN_FOUNTAIN_SPARKLING);

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
    return (true);
}

// Returns true if a message has already been printed (which will identify
// the scroll.)
static bool _vorpalise_weapon()
{
    if (!you.weapon())
        return (false);

    // Check if you're wielding a brandable weapon.
    item_def& wpn = *you.weapon();
    if (wpn.base_type != OBJ_WEAPONS || wpn.sub_type == WPN_BLOWGUN
        || is_artefact(wpn))
    {
        return (false);
    }

    you.wield_change = true;

    // If there's no brand, make it vorpal.
    if (get_weapon_brand(wpn) == SPWPN_NORMAL)
    {
        alert_nearby_monsters();
        mprf("%s emits a brilliant flash of light!",
             wpn.name(DESC_CAP_YOUR).c_str());
        set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_VORPAL);
        return (true);
    }

    // If there's a permanent brand, fail.
    if (you.duration[DUR_WEAPON_BRAND] == 0)
        return (false);

    // There's a temporary brand, attempt to make it permanent.
    const std::string itname = wpn.name(DESC_CAP_YOUR);
    bool success = true;
    bool msg = true;

    switch (get_weapon_brand(wpn))
    {
    case SPWPN_VORPAL:
        if (get_vorpal_type(wpn) != DVORP_CRUSHING)
            mprf("%s's sharpness seems more permanent.", itname.c_str());
        else
            mprf("%s's heaviness feels very stable.", itname.c_str());
        break;

    case SPWPN_FLAME:
    case SPWPN_FLAMING:
        mprf("%s is engulfed in an explosion of flames!", itname.c_str());
        immolation(10, IMMOLATION_SPELL, you.pos(), true, &you);
        break;

    case SPWPN_FROST:
    case SPWPN_FREEZING:
        if (get_weapon_brand(wpn) == SPWPN_FROST)
            mprf("%s is covered with a thick layer of frost!", itname.c_str());
        else
            mprf("%s glows brilliantly blue for a moment.", itname.c_str());
        cast_refrigeration(60);
        break;

    case SPWPN_DRAINING:
        mprf("%s thirsts for the lives of mortals!", itname.c_str());
        drain_exp();
        break;

    case SPWPN_VENOM:
        mprf("%s seems more permanently poisoned.", itname.c_str());
        cast_toxic_radiance();
        break;

    case SPWPN_PAIN:
        // Can't fix pain brand (balance)...you just get tormented.
        mprf("%s shrieks out in agony!", itname.c_str());

        torment_monsters(you.pos(), 0, TORMENT_GENERIC);
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

    default:
        success = false;
        msg = false;
        break;
    }

    if (success)
        you.duration[DUR_WEAPON_BRAND] = 0;

    return (msg);
}

bool enchant_weapon(enchant_stat_type which_stat, bool quiet, item_def &wpn)
{
    bool to_hit = (which_stat == ENCHANT_TO_HIT);

    // Cannot be enchanted nor uncursed.
    if (!is_enchantable_weapon(wpn, true, to_hit))
    {
        if (!quiet)
            canned_msg( MSG_NOTHING_HAPPENS );

        return (false);
    }

    const bool is_cursed = wpn.cursed();

    // Missiles only have one stat.
    if (wpn.base_type == OBJ_MISSILES)
    {
        which_stat = ENCHANT_TO_HIT;
        to_hit     = true;
    }

    int enchant_level = (to_hit ? wpn.plus
                                : wpn.plus2);

    // Even if not affected, it may be uncursed.
    if (!is_enchantable_weapon(wpn, false, to_hit)
        || enchant_level >= 4 && x_chance_in_y(enchant_level, MAX_WPN_ENCHANT))
    {
        if (is_cursed)
        {
            if (!quiet)
            {
                mprf("%s glows silver for a moment.",
                     wpn.name(DESC_CAP_YOUR).c_str());
            }

            do_uncurse_item(wpn);
            return (true);
        }
        else
        {
            if (!quiet)
                canned_msg(MSG_NOTHING_HAPPENS);

            // Xom thinks it's funny if enchantment is possible but fails.
            if (is_enchantable_weapon(wpn, false, to_hit))
                xom_is_stimulated(32);

            return (false);
        }
    }

    // Get item name now before changing enchantment.
    std::string iname = wpn.name(DESC_CAP_YOUR);

    if (wpn.base_type == OBJ_WEAPONS)
    {
        if (to_hit)
        {
            if (!quiet)
                mprf("%s glows green for a moment.", iname.c_str());

            wpn.plus++;
        }
        else
        {
            if (!quiet)
                mprf("%s glows red for a moment.", iname.c_str());

            wpn.plus2++;
        }
    }
    else if (wpn.base_type == OBJ_MISSILES)
    {
        if (!quiet)
        {
            mprf("%s glow%s red for a moment.", iname.c_str(),
                 wpn.quantity > 1 ? "" : "s");
        }

        wpn.plus++;
    }

    if (is_cursed)
        do_uncurse_item(wpn);

    return (true);
}

static bool _handle_enchant_weapon(enchant_stat_type which_stat,
                                   bool quiet, int item_slot)
{
    if (item_slot == -1)
        item_slot = you.equip[ EQ_WEAPON ];

    if (item_slot == -1)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (false);
    }

    item_def& wpn(you.inv[item_slot]);

    bool result = enchant_weapon(which_stat, quiet, wpn);

    you.wield_change = true;

    return result;
}

bool enchant_armour(int &ac_change, bool quiet, item_def &arm)
{
    ac_change = 0;

    // Cannot be enchanted nor uncursed.
    if (!is_enchantable_armour(arm, true))
    {
        if (!quiet)
            canned_msg( MSG_NOTHING_HAPPENS );

        return (false);
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
                 arm.name(DESC_CAP_YOUR).c_str());
        }

        ac_change = property(arm, PARM_AC);
        hide2armour(arm);
        ac_change = property(arm, PARM_AC) - ac_change;

        if (is_cursed)
            do_uncurse_item(arm);

        // No additional enchantment.
        return (true);
    }

    // Even if not affected, it may be uncursed.
    if (!is_enchantable_armour(arm, false)
        || arm.plus > MAX_SEC_ENCHANT
           && x_chance_in_y(arm.plus, MAX_ARM_ENCHANT))
    {
        if (is_cursed)
        {
            if (!quiet)
            {
                mprf("%s glows silver for a moment.",
                     arm.name(DESC_CAP_YOUR).c_str());
            }

            do_uncurse_item(arm);
            return (true);
        }
        else
        {
            if (!quiet)
                canned_msg(MSG_NOTHING_HAPPENS);

            // Xom thinks it's funny if enchantment is possible but fails.
            if (is_enchantable_armour(arm, false))
                xom_is_stimulated(32);

            return (false);
        }
    }

    // Output message before changing enchantment and curse status.
    if (!quiet)
    {
        mprf("%s glows green for a moment.",
             arm.name(DESC_CAP_YOUR).c_str());
    }

    arm.plus++;
    ac_change++;

    if (is_cursed)
        do_uncurse_item(arm);

    return (true);
}

static bool _handle_enchant_armour(int item_slot)
{
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Enchant which item?", MT_INVLIST,
                                           OSEL_ENCH_ARM, true, true, false);
        }
        if (prompt_failed(item_slot))
            return (false);

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
        int ac_change;
        bool result = enchant_armour(ac_change, false, arm);

        if (ac_change)
            you.redraw_armour_class = true;

        return (result);
    }
    while (true);

    return (false);
}

static void handle_read_book(int item_slot)
{
    item_def& book(you.inv[item_slot]);

    if (book.sub_type == BOOK_DESTRUCTION)
    {
        if (silenced(you.pos()))
            mpr("This book does not work if you cannot read it aloud!");
        else
            tome_of_power(item_slot);
        return;
    }
    else if (book.sub_type == BOOK_MANUAL)
    {
        skill_manual(item_slot);
        return;
    }

    while (true)
    {
        // Spellbook
        const int ltr = read_book( book, RBOOK_READ_SPELL );

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

    // Get the slot of the scroll just read.
    int item_slot = scroll.slot;

    // Get the slot of the item the scroll is to be used on.
    // Ban the scroll's own slot from the prompt to avoid the stupid situation
    // where you use identify on itself.
    item_slot = prompt_invent_item("Use on which item? (\\ to view known items)",
                                   MT_INVLIST, OSEL_ANY, true, true, false, 0,
                                   item_slot, NULL, OPER_ANY, true);

    if (prompt_failed(item_slot))
        return (false);

    item_def &item = you.inv[item_slot];

    switch (scroll.sub_type)
    {
    case SCR_IDENTIFY:
        if (!fully_identified(item))
        {
            mpr("This is a scroll of identify!");
            identify(-1, item_slot);
            return (true);
        }
        break;
    case SCR_RECHARGING:
        if (item_is_rechargeable(item, false, true))
        {
            // Might still fail on highly enchanted weapons of electrocution.
            // (If so, already prints the "Nothing happens" message.)
            if (recharge_wand(item_slot))
                return (true);
            return (false);
        }
        break;
    case SCR_ENCHANT_ARMOUR:
        if (is_enchantable_armour(item, true))
        {
            // Might still fail because of already high enchantment.
            // (If so, already prints the "Nothing happens" message.)
            if (_handle_enchant_armour(item_slot))
                return (true);
            return (false);
        }
        break;
    default:
        mprf("Buggy scroll %d can't modify item!", scroll.sub_type);
        break;
    }

    // Oops, wrong item...
    canned_msg(MSG_NOTHING_HAPPENS);
    return (false);
}

static void _vulnerability_scroll()
{
    // First cast antimagic on yourself.
    antimagic();

    // List of magical enchantments which will be dispelled.
    const enchant_type lost_enchantments[] = {
        ENCH_SLOW,
        ENCH_HASTE,
        ENCH_SWIFT,
        ENCH_MIGHT,
        ENCH_FEAR,
        ENCH_CONFUSION,
        ENCH_INVIS,
        ENCH_CORONA,
        ENCH_CHARM,
        ENCH_PARALYSIS,
        ENCH_PETRIFYING,
        ENCH_PETRIFIED
    };

    mon_enchant lowered_mr(ENCH_LOWERED_MR, 1, KC_YOU, 40);

    // Go over all creatures in LOS.
    for (radius_iterator ri(you.pos(), LOS_RADIUS); ri; ++ri)
    {
        if (monsters* mon = monster_at(*ri))
        {
            // Dispel all magical enchantments.
            for (unsigned int i = 0; i < ARRAYSZ(lost_enchantments); ++i)
                mon->del_ench(lost_enchantments[i], true, true);

            // If relevant, monsters have their MR halved.
            if (!mons_immune_magic(mon))
                mon->add_ench(lowered_mr);

            // Annoying but not enough to turn friendlies against you.
            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, MHITYOU);
        }
    }

    you.set_duration(DUR_LOWERED_MR, 40, 0,
                     "Magic dampens, then quickly surges around you.");
}

void read_scroll(int slot)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (!player_can_handle_equipment())
    {
        canned_msg(MSG_PRESENT_FORM);
        return;
    }

    if (you.stat_zero[STAT_INT])
    {
        mpr("Reading requires mental cohesion, which you lack.");
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    int item_slot = (slot != -1) ? slot
                                 : prompt_invent_item( "Read which item?",
                                                       MT_INVLIST,
                                                       OBJ_SCROLLS,
                                                       true, true, true, 0, -1,
                                                       NULL, OPER_READ );

    if (prompt_failed(item_slot))
        return;

    item_def& scroll = you.inv[item_slot];

    if (scroll.base_type != OBJ_BOOKS && scroll.base_type != OBJ_SCROLLS)
    {
        mpr("You can't read that!");
        crawl_state.zero_turns_taken();
        return;
    }

    // Here we try to read a book {dlb}:
    if (scroll.base_type == OBJ_BOOKS)
    {
        handle_read_book( item_slot );
        return;
    }

    if (silenced(you.pos()))
    {
        mpr("Magic scrolls do not work when you're silenced!");
        crawl_state.zero_turns_taken();
        return;
    }

    // Ok - now we FINALLY get to read a scroll !!! {dlb}
    you.turn_is_over = true;

    // Imperfect vision prevents players from reading actual content {dlb}:
    if (player_mutation_level(MUT_BLURRY_VISION)
        && x_chance_in_y(player_mutation_level(MUT_BLURRY_VISION), 5))
    {
        mpr((player_mutation_level(MUT_BLURRY_VISION) == 3 && one_chance_in(3))
                        ? "This scroll appears to be blank."
                        : "The writing blurs in front of your eyes.");
        return;
    }

    // Decrement and handle inventory if any scroll other than paper {dlb}:
    const scroll_type which_scroll = static_cast<scroll_type>(scroll.sub_type);
    const bool alreadyknown = item_type_known(scroll);

    if (alreadyknown
        && (which_scroll == SCR_BLINKING || which_scroll == SCR_TELEPORTATION)
        && item_blocks_teleport(false, false))
    {
        mpr("You cannot teleport right now.");
        return;
    }

    if (which_scroll != SCR_PAPER
        && (which_scroll != SCR_IMMOLATION || you.confused()))
    {
        mpr("As you read the scroll, it crumbles to dust.");
        // Actual removal of scroll done afterwards. -- bwr
    }

    const bool dangerous = player_in_a_dangerous_place();

    // Scrolls of paper are also exempted from this handling {dlb}:
    if (which_scroll != SCR_PAPER)
    {
        if (you.confused())
        {
            random_uselessness(item_slot);
            dec_inv_item_quantity(item_slot, 1);
            return;
        }

        if (!you.skills[SK_SPELLCASTING])
            exercise(SK_SPELLCASTING, (coinflip()? 2 : 1));
    }

    // It is the exception, not the rule, that the scroll will not
    // be identified. {dlb}
    bool id_the_scroll = true;  // to prevent unnecessary repetition
    bool tried_on_item = false; // used to modify item (?EA, ?RC, ?ID)

    bool bad_effect = false; // for Xom: result is bad (or at least dangerous)
    switch (which_scroll)
    {
    case SCR_PAPER:
        // Remember, paper scrolls handled as special case above, too.
        mpr("This scroll appears to be blank.");
        if (player_mutation_level(MUT_BLURRY_VISION) == 3)
            id_the_scroll = false;
        break;

    case SCR_RANDOM_USELESSNESS:
        random_uselessness(item_slot);
        break;

    case SCR_BLINKING:
        blink(1000, false);
        break;

    case SCR_TELEPORTATION:
        you_teleport();
        break;

    case SCR_REMOVE_CURSE:
        if (!remove_curse(false))
            id_the_scroll = false;
        break;

    case SCR_DETECT_CURSE:
        if (!detect_curse(item_slot, false))
            id_the_scroll = false;
        break;

    case SCR_ACQUIREMENT:
        mpr("This is a scroll of acquirement!");
        more();
        acquirement(OBJ_RANDOM, AQ_SCROLL);
        break;

    case SCR_FEAR:
    {
        int fear_influenced = 0;
        mass_enchantment(ENCH_FEAR, 1000, MHITYOU, NULL, &fear_influenced);
        id_the_scroll = fear_influenced;
        break;
    }

    case SCR_NOISE:
        noisy(25, you.pos(), "You hear a loud clanging noise!");
        break;

    case SCR_SUMMONING:
    {
        const int monster = create_monster(
                                mgen_data(MONS_ABOMINATION_SMALL, BEH_FRIENDLY,
                                          &you, 0, 0, you.pos(), MHITYOU,
                                          MG_FORCE_BEH));
        if (monster != -1)
        {
            mpr("A horrible Thing appears!");
            player_angers_monster(&menv[monster]);
        }
        else
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        break;
    }

    case SCR_FOG:
        mpr("The scroll dissolves into smoke.");
        big_cloud(random_smoke_type(), KC_YOU, you.pos(), 50, 8 + random2(8));
        break;

    case SCR_MAGIC_MAPPING:
        magic_mapping(50, 90 + random2(11), false);
        break;

    case SCR_TORMENT:
        torment(TORMENT_SCROLL, you.pos());

        // This is only naughty if you know you're doing it.
        did_god_conduct(DID_NECROMANCY, 10, item_type_known(scroll));
        bad_effect = true;
        break;

    case SCR_IMMOLATION:
        mpr("The scroll explodes in your hands!");

        // Doesn't destroy scrolls anymore, so no special check needed. (jpeg)
        immolation(10, IMMOLATION_SCROLL, you.pos(), alreadyknown, &you);
        bad_effect = true;
        more();
        break;

    case SCR_CURSE_WEAPON:
        if (!you.weapon()
            || you.weapon()->base_type != OBJ_WEAPONS
            || you.weapon()->cursed())
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        else
        {
            // Also sets wield_change.
            do_curse_item( *you.weapon(), false );
            learned_something_new(HINT_YOU_CURSED);
            bad_effect = true;
        }
        break;

    // Everything [in the switch] below this line is a nightmare {dlb}:
    case SCR_ENCHANT_WEAPON_I:
        id_the_scroll = _handle_enchant_weapon(ENCHANT_TO_HIT);
        break;

    case SCR_ENCHANT_WEAPON_II:
        id_the_scroll = _handle_enchant_weapon(ENCHANT_TO_DAM);
        break;

    case SCR_ENCHANT_WEAPON_III:
        if (you.weapon())
        {
            item_def& wpn = *you.weapon();

            const bool is_cursed = wpn.cursed();

            if (wpn.base_type != OBJ_WEAPONS && wpn.base_type != OBJ_MISSILES
                || !is_cursed
                   && !is_enchantable_weapon(wpn, true, true)
                   && !is_enchantable_weapon(wpn, true, false))
            {
                canned_msg(MSG_NOTHING_HAPPENS);
                id_the_scroll = false;
                break;
            }
            // It's a weapon or stack of missiles that is not an artefact
            // and not fully enchanted, or at least needs to be uncursed.

            // Get item name now before changing enchantment.
            std::string iname = wpn.name(DESC_CAP_YOUR);

            // Uncursing is always possible.
            bool success = is_cursed;
            if (_handle_enchant_weapon(ENCHANT_TO_HIT, true))
                success = true;

            if (is_enchantable_weapon(wpn, true, true) && coinflip()
                && _handle_enchant_weapon(ENCHANT_TO_HIT, true))
            {
                success = true;
            }

            // Only weapons use the second stat.
            if (wpn.base_type == OBJ_WEAPONS)
            {
                if (_handle_enchant_weapon(ENCHANT_TO_DAM, true))
                    success = true;

                if (is_enchantable_weapon(wpn, true, false) && coinflip()
                    && _handle_enchant_weapon(ENCHANT_TO_DAM, true))
                {
                    success = true;
                }
            }

            if (is_cursed)
                do_uncurse_item(wpn);

            if (success)
            {
                mprf("%s glow%s bright yellow for a while.", iname.c_str(),
                     wpn.quantity > 1 ? "" : "s");
            }
            else
            {
                canned_msg(MSG_NOTHING_HAPPENS);
                id_the_scroll = false;
            }
        }
        else
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        break;

    case SCR_VORPALISE_WEAPON:
        id_the_scroll = _vorpalise_weapon();
        if (!id_the_scroll)
            canned_msg(MSG_NOTHING_HAPPENS);
        break;

    case SCR_IDENTIFY:
        if (!item_type_known(scroll))
        {
            id_the_scroll = _scroll_modify_item(scroll);
            if (!id_the_scroll)
                tried_on_item = true;
        }
        else
            identify(-1);
        break;

    case SCR_RECHARGING:
        if (!item_type_known(scroll))
        {
            id_the_scroll = _scroll_modify_item(scroll);
            if (!id_the_scroll)
                tried_on_item = true;
        }
        else
            recharge_wand(-1);
        break;

    case SCR_ENCHANT_ARMOUR:
        if (!item_type_known(scroll))
        {
            id_the_scroll = _scroll_modify_item(scroll);
            if (!id_the_scroll)
                tried_on_item = true;
        }
        else
            _handle_enchant_armour(-1);
        break;

    case SCR_CURSE_ARMOUR:
    {
        // make sure there's something to curse first
        int count = 0;
        int affected = EQ_WEAPON;
        for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
        {
            if (you.equip[i] != -1 && !you.inv[you.equip[i]].cursed())
            {
                count++;
                if (one_chance_in(count))
                    affected = i;
            }
        }

        if (affected == EQ_WEAPON)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
            break;
        }

        // Make the name before we curse it.
        do_curse_item( you.inv[you.equip[affected]], false );
        learned_something_new(HINT_YOU_CURSED);
        bad_effect = true;
        break;
    }

    case SCR_HOLY_WORD:
    {
        int pow = 100;

        if (is_good_god(you.religion))
        {
            pow += (you.religion == GOD_SHINING_ONE) ? you.piety :
                                                       you.piety / 2;
        }

        const bool success = holy_word(pow, HOLY_WORD_SCROLL, you.pos(),
                                       !item_type_known(scroll), &you);

        if (!success)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }

        // This is only naughty if you know you're doing it, or if it's
        // succeeded, in which case you'll know for next time.
        if (item_type_known(scroll) || success)
            did_god_conduct(DID_HOLY, 10, item_type_known(scroll));
        break;
    }

    case SCR_SILENCE:
        cast_silence(25);
        break;

    case SCR_VULNERABILITY:
        _vulnerability_scroll();
        break;

    default:
        mpr("Read a buggy scroll, please report this.");
        break;
    }

    set_ident_type(scroll, id_the_scroll ? ID_KNOWN_TYPE :
                           tried_on_item ? ID_TRIED_ITEM_TYPE
                                         : ID_TRIED_TYPE);

    // Finally, destroy and identify the scroll.
    if (which_scroll != SCR_PAPER)
    {
        if (id_the_scroll)
            set_ident_flags(scroll, ISFLAG_KNOW_TYPE); // for notes

        dec_inv_item_quantity( item_slot, 1 );
    }

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you read an unknown scroll and there is a
        // dangerous monster nearby... (though not as much as potions
        // since there are no *really* bad scrolls, merely useless ones).
        xom_is_stimulated(bad_effect ? 128 : 64);
    }
}

void examine_object(void)
{
    int item_slot = prompt_invent_item( "Examine which item?",
                                        MT_INVLIST, -1,
                                        true, true, true, 0, -1, NULL,
                                        OPER_EXAMINE );
    if (prompt_failed(item_slot))
        return;

    describe_item( you.inv[item_slot], true );
    redraw_screen();
    mesclr();
}                               // end original_name()

bool wearing_slot(int inv_slot)
{
    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; ++i)
        if (inv_slot == you.equip[i])
            return (true);

    return (false);
}

bool item_blocks_teleport(bool calc_unid, bool permit_id)
{
    return (scan_artefacts(ARTP_PREVENT_TELEPORTATION, calc_unid)
            || stasis_blocks_effect(calc_unid, permit_id, NULL));
}

bool stasis_blocks_effect(bool calc_unid,
                          bool identify,
                          const char *msg, int noise,
                          const char *silenced_msg)
{
    if (wearing_amulet(AMU_STASIS, calc_unid))
    {
        item_def *amulet = you.slot_item(EQ_AMULET, false);

        if (msg)
        {
            const std::string name(amulet? amulet->name(DESC_CAP_YOUR) :
                                   "Something");
            const std::string message =
                make_stringf(msg, name.c_str());

            if (noise)
            {
                if (!noisy(noise, you.pos(), message.c_str())
                    && silenced_msg)
                {
                    mprf(silenced_msg, name.c_str());
                }
            }
            else
            {
                mpr(message.c_str());
            }
        }

        // In all cases, the amulet auto-ids if requested.
        if (amulet && identify)
            set_ident_type(*amulet, ID_KNOWN_TYPE);
        return (true);
    }
    return (false);
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

void tile_item_pickup(int idx)
{
    pickup_single_item(idx, mitm[idx].quantity);
}

void tile_item_drop(int idx)
{
    drop_item(idx, you.inv[idx].quantity);
}

static bool _prompt_eat_bad_food(const item_def food)
{
    if (food.base_type != OBJ_CORPSES
        && (food.base_type != OBJ_FOOD || food.sub_type != FOOD_CHUNK))
    {
        return (true);
    }

    if (!is_bad_food(food))
        return (true);

    const std::string food_colour = menu_colour_item_prefix(food);
    std::string colour            = "";
    std::string colour_off        = "";

    const int col = menu_colour(food.name(DESC_NOCAP_A), food_colour, "pickup");
    if (col != -1)
        colour = colour_to_str( col );

    if (!colour.empty())
    {
        // Order is important here.
        colour_off  = "</" + colour + ">";
        colour      = "<" + colour + ">";
    }

    const std::string qualifier = colour
                                  + (is_poisonous(food)      ? "poisonous" :
                                     is_mutagenic(food)      ? "mutagenic" :
                                     causes_rot(food)        ? "rot-inducing" :
                                     is_forbidden_food(food) ? "forbidden" : "")
                                  + colour_off;

    std::string prompt  = "Really ";
                prompt += (you.species == SP_VAMPIRE ? "drink from" : "eat");
                prompt += " this " + qualifier;
                prompt += (food.base_type == OBJ_CORPSES ? " corpse"
                                                         : " chunk of meat");
                prompt += "?";

    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return (false);
    }
    return (true);
}

void tile_item_eat_floor(int idx)
{
    if (mitm[idx].base_type == OBJ_CORPSES
            && you.species == SP_VAMPIRE
        || mitm[idx].base_type == OBJ_FOOD
            && you.is_undead != US_UNDEAD && you.species != SP_VAMPIRE)
    {
        if (can_ingest(mitm[idx].base_type, mitm[idx].sub_type, false)
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
    {
        wield_weapon(true, PROMPT_GOT_SPECIAL); // unwield
    }
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
        wield_weapon(true, PROMPT_GOT_SPECIAL);
        return;
    }

    const int type = item.base_type;

    // Use it
    switch (type)
    {
        case OBJ_WEAPONS:
        case OBJ_STAVES:
        case OBJ_MISCELLANY:
        case OBJ_WANDS:
            // Wield any unwielded item of these types.
            if (!equipped
                && (type == OBJ_WEAPONS || type == OBJ_STAVES || is_deck(item)
                    || type == OBJ_MISCELLANY
                       && item.sub_type == MISC_LANTERN_OF_SHADOWS))
            {
                wield_weapon(true, idx);
                return;
            }
            // Evoke misc. items, rods, or wands.
            if (item_is_evokable(item))
            {
                evoke_item(idx);
                return;
            }
            // Unwield wielded items.
            if (equipped)
                wield_weapon(true, PROMPT_GOT_SPECIAL); // unwield
            return;

        case OBJ_MISSILES:
            if (check_warning_inscriptions(item, OPER_FIRE))
                fire_thing(idx);
            return;

        case OBJ_ARMOUR:
            if (!player_can_handle_equipment())
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
            if (item.sub_type == BOOK_MANUAL
                || item.sub_type == BOOK_DESTRUCTION
                || you.skills[SK_SPELLCASTING] == 0)
            {
                if (check_warning_inscriptions(item, OPER_READ))
                    handle_read_book(idx);
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
