/*
 *  File:       item_use.cc
 *  Summary:    Functions for making use of inventory items.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "item_use.h"

#include <sstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "externs.h"

#include "beam.h"
#include "cio.h"
#include "cloud.h"
#include "command.h"
#include "debug.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "effects.h"
#include "fight.h"
#include "food.h"
#include "invent.h"
#include "it_use2.h"
#include "it_use3.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mstuff2.h"
#include "mon-util.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tiles.h"
#include "transfor.h"
#include "traps.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

static bool _drink_fountain();
static bool _handle_enchant_weapon( enchant_stat_type which_stat,
                                    bool quiet = false, int item_slot = -1 );
static bool _handle_enchant_armour( int item_slot = -1 );

static int  _fire_prompt_for_item(std::string& err);
static bool _fire_validate_item(int selected, std::string& err);

// Rather messy - we've gathered all the can't-wield logic from wield_weapon()
// here.
bool can_wield(item_def *weapon, bool say_reason,
               bool ignore_temporary_disability)
{
#define SAY(x) if (say_reason) { x; } else

    if (!ignore_temporary_disability && you.duration[DUR_BERSERKER])
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
        && item_cursed(*you.weapon()))
    {
        SAY(mpr("You can't unwield your weapon to draw a new one!"));
        return (false);
    }

    // If we don't have an actual weapon to check, return now.
    if (!weapon)
        return (true);

    for (int i = EQ_CLOAK; i <= EQ_AMULET; i++)
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

    if (player_size(PSIZE_TORSO) < SIZE_LARGE && item_mass(*weapon) >= 300)
    {
        SAY(mpr("That's too large and heavy for you to wield."));
        return (false);
    }

    // Small species wielding large weapons...
    if (player_size(PSIZE_BODY) < SIZE_MEDIUM
        && weapon_skill(*weapon) != SK_STAVES
        && weapon->sub_type != WPN_BOW
        && !check_weapon_wieldable_size(*weapon, player_size(PSIZE_BODY)))
    {
        SAY(mpr("That's too large for you to wield."));
        return (false);
    }

    if (player_is_unholy() && is_holy_item(*weapon))
    {
        if (say_reason)
        {
            mpr("This weapon is holy and will not allow you to wield it.");
            // If it's a standard weapon, you know its ego now.
            if (!is_artefact(*weapon) && !is_blessed_blade(*weapon)
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

    // Misc. items need to be wielded to be evoked.
    if (item.base_type == OBJ_MISCELLANY && item.sub_type != MISC_RUNE_OF_ZOT)
        return (true);

    // Some missiles need to be wielded for spells.
    if (item.base_type == OBJ_MISSILES)
    {
        if (item.sub_type == MI_STONE)
            return (player_knows_spell(SPELL_SANDBLAST));

        if (item.sub_type == MI_ARROW)
            return (player_knows_spell(SPELL_STICKS_TO_SNAKES));

        return (false);
    }

    // Boneshards.
    if (item.base_type == OBJ_CORPSES)
    {
        return (item.sub_type == CORPSE_SKELETON
                && player_knows_spell(SPELL_BONE_SHARDS));
    }

    // Sublimation of Blood.
    if (!player_knows_spell(SPELL_SUBLIMATION_OF_BLOOD))
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
                  bool force)
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
        && (!auto_wield || !is_valid_item(you.inv[item_slot]) || !good_swap))
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
        if (you.equip[EQ_WEAPON] != -1)
        {
            item_def& wpn = *you.weapon();
            // Can we safely unwield this item?
            if (has_warning_inscription(wpn, OPER_WIELD))
            {
                std::string prompt = "Really unwield ";
                prompt += wpn.name(DESC_INVENTORY);
                prompt += '?';
                if (!yesno(prompt.c_str(), false, 'n'))
                    return (false);
            }

            if (!unwield_item(show_weff_messages))
                return (false);

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

    // Wield the weapon.
    if (!safe_to_remove_or_wear(new_wpn, false))
        return (false);

    // Go ahead and wield the weapon.
    if (you.equip[EQ_WEAPON] != -1 && !unwield_item(show_weff_messages))
        return (false);

    you.equip[EQ_WEAPON] = item_slot;

    // any oddness on wielding taken care of here
    wield_effects(item_slot, show_weff_messages);

    mpr(new_wpn.name(DESC_INVENTORY_EQUIP).c_str());

    // Warn player about low str/dex or throwing skill.
    if (show_weff_messages)
        wield_warning();

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

static void warn_rod_shield_interference(const item_def &)
{
    const int leakage = rod_shield_leakage();
    const char *leak_degree = shield_impact_degree(leakage);

    // Any way to avoid the double entendre? :-)
    if (leak_degree)
    {
        mprf(MSGCH_WARN,
                "Your %s %sreduces the effectiveness of your rod.",
                shield_base_name(player_shield()),
                leak_degree);
    }
}

static void warn_launcher_shield_slowdown(const item_def &launcher)
{
    const int slowspeed =
        launcher_final_speed(launcher, player_shield()) * player_speed() / 100;
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
                    shield_base_name(player_shield()),
                    slow_degree);
        }
    }
}

// Warn if your shield is greatly impacting the effectiveness of your weapon?
void warn_shield_penalties()
{
    if (!player_shield())
        return;

    // Warnings are limited to rods, bows, and quarterstaves at the moment.
    const item_def *weapon = player_weapon();
    if (!weapon)
        return;

    if (item_is_rod(*weapon))
        warn_rod_shield_interference(*weapon);
    else if (is_range_weapon(*weapon))
        warn_launcher_shield_slowdown(*weapon);
    else if (weapon->base_type == OBJ_WEAPONS
             && weapon_skill(*weapon) == SK_STAVES)
    {
        mprf(MSGCH_WARN, "Your %s severely limits your weapon's effectiveness.",
             shield_base_name(player_shield()));
    }
}

int item_special_wield_effect(const item_def &item)
{
    if (item.base_type != OBJ_WEAPONS || !is_artefact(item))
        return (SPWLD_NONE);

    int i_eff = SPWPN_NORMAL;
    if (is_random_artefact( item ))
        i_eff = randart_wpn_property(item, RAP_BRAND);
    else
        i_eff = item.special;

    switch (i_eff)
    {
    case SPWPN_SINGING_SWORD:
        return (SPWLD_SING);

    case SPWPN_WRATH_OF_TROG:
        return (SPWLD_TROG);

    case SPWPN_SCYTHE_OF_CURSES:
        return (SPWLD_CURSE);

    case SPWPN_MACE_OF_VARIABILITY:
        return (SPWLD_VARIABLE);

    case SPWPN_SCEPTRE_OF_TORMENT:
        return (SPWLD_TORMENT);

    case SPWPN_SWORD_OF_ZONGULDROK:
        return (SPWLD_ZONGULDROK);

    case SPWPN_SWORD_OF_POWER:
        return (SPWLD_POWER);

    case SPWPN_STAFF_OF_OLGREB:
        return (SPWLD_OLGREB);

    case SPWPN_STAFF_OF_WUCAD_MU:
        return (SPWLD_WUCAD_MU);

    default:
        return (SPWLD_NONE);
    }
}

// Provide a function for handling initial wielding of 'special'
// weapons, or those whose function is annoying to reproduce in
// other places *cough* auto-butchering *cough*.    {gdl}
void wield_effects(int item_wield_2, bool showMsgs)
{
    unsigned char special = 0;

    item_def &item = you.inv[item_wield_2];
    const bool artefact     = is_random_artefact(item);
    const bool known_cursed = item_known_cursed(item);

    // And here we finally get to the special effects of wielding. {dlb}
    switch (item.base_type)
    {
    case OBJ_MISCELLANY:
    {
        if (item.sub_type == MISC_LANTERN_OF_SHADOWS)
        {
            if (showMsgs)
                mpr("The area is filled with flickering shadows.");

            you.current_vision -= 2;
            setLOSRadius(you.current_vision);
            you.special_wield = SPWLD_SHADOW;
        }
        else if (item.sub_type == MISC_HORN_OF_GERYON)
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        break;
    }

    case OBJ_STAVES:
    {
        if (item.sub_type == STAFF_POWER)
        {
            calc_mp();
            set_ident_type(item, ID_KNOWN_TYPE);
            set_ident_flags(item, ISFLAG_EQ_WEAPON_MASK);
            mpr("You feel your mana capacity increase.");
        }
        else if (!maybe_identify_staff(item))
        {
            // Give curse status when wielded.
            // Right now that's always "uncursed". -- bwr
            set_ident_flags(item, ISFLAG_KNOW_CURSE);
        }
        break;
    }

    case OBJ_WEAPONS:
    {
        if (showMsgs)
        {
            if (is_holy_item(item) && you.religion == GOD_YREDELEMNUL)
                mpr("You really shouldn't be using a holy item like this.");
            else if (is_evil_item(item) && is_good_god(you.religion))
                mpr("You really shouldn't be using an evil item like this.");
            else if (is_chaotic_item(item) && you.religion == GOD_ZIN)
                mpr("You really shouldn't be using a chaotic item like this.");
        }

        const bool was_known = item_type_known(item);

        // Only used for Singing Sword introducing itself
        // (could be extended to other talking weapons...)
        const std::string old_desc = item.name(DESC_CAP_THE);

        set_ident_flags(item, ISFLAG_EQ_WEAPON_MASK);

        if (artefact)
        {
            special = randart_wpn_property(item, RAP_BRAND);
            use_randart(item_wield_2);
            if (!was_known)
            {
                item.flags |= ISFLAG_NOTED_ID;

                if (Options.autoinscribe_randarts)
                    add_autoinscription(item, randart_auto_inscription(item));

                // Make a note of it.
                take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_NOCAP_A).c_str(),
                               origin_desc(item).c_str()));
            }
        }
        else
            special = item.special;

        if (special != SPWPN_NORMAL)
        {
            // message first
            if (showMsgs)
            {
                switch (special)
                {
                case SPWPN_SWORD_OF_CEREBOV:
                case SPWPN_FLAMING:
                    mpr("It bursts into flame!");
                    break;

                case SPWPN_FREEZING:
                    mpr("It glows with a cold blue light!");
                    break;

                case SPWPN_HOLY_WRATH:
                    mpr("It softly glows with a divine radiance!");
                    break;

                case SPWPN_ELECTROCUTION:
                    if (!silenced(you.pos()))
                    {
                        mpr("You hear the crackle of electricity.",
                            MSGCH_SOUND);
                    }
                    else
                        mpr("You see sparks fly.");
                    break;

                case SPWPN_ORC_SLAYING:
                    mpr((you.species == SP_HILL_ORC)
                            ? "You feel a sudden desire to commit suicide."
                            : "You feel a sudden desire to kill orcs!");
                    break;

                case SPWPN_DRAGON_SLAYING:
                    mpr(player_genus(GENPC_DRACONIAN)
                        || you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON
                            ? "You feel a sudden desire to commit suicide."
                            : "You feel a sudden desire to slay dragons!");
                    break;

                case SPWPN_VENOM:
                    mpr("It begins to drip with poison!");
                    break;

                case SPWPN_PROTECTION:
                    mpr("You feel protected!");
                    break;

                case SPWPN_DRAINING:
                    mpr("You sense an unholy aura.");
                    break;

                case SPWPN_SPEED:
                    mpr("Your hands tingle!");
                    break;

                case SPWPN_FLAME:
                    mpr("It bursts into flame!");
                    break;

                case SPWPN_FROST:
                    mpr("It is covered in frost.");
                    break;

                case SPWPN_VAMPIRICISM:
                    if (you.is_undead != US_UNDEAD)
                        mpr("You feel a strange hunger.");
                    else
                        mpr("You feel strangely empty.");
                    break;

                case SPWPN_RETURNING:
                    mpr("It wiggles slightly.");
                    break;

                case SPWPN_PAIN:
                    mpr("A searing pain shoots up your arm!");
                    break;

                case SPWPN_CHAOS:
                    mpr("It is briefly surrounded by a scintillating aura "
                        "of random colours.");
                    break;

                case SPWPN_PENETRATION:
                    mprf("Your %s briefly pass through it before you manage "
                         "to get a firm grip on it.",
                         you.hand_name(true).c_str());
                    break;

                case SPWPN_SHADOW:
                    mpr("It is briefly surrounded by shifting shadows.");
                    break;

                case SPWPN_SINGING_SWORD:
                    if (!was_known)
                    {
                        mprf(MSGCH_TALK, "%s says, "
                             "\"Hi! I'm the Singing Sword!\"",
                             old_desc.c_str());
                    }
                    else
                        mpr("The Singing Sword hums in delight!", MSGCH_TALK);
                    break;

                case SPWPN_WRATH_OF_TROG:
                    mpr("You feel bloodthirsty!");
                    break;

                case SPWPN_SCYTHE_OF_CURSES:
                    mpr("A shiver runs down your spine.");
                    break;

                case SPWPN_GLAIVE_OF_PRUNE:
                    mpr("You feel pruney.");
                    break;

                case SPWPN_SCEPTRE_OF_TORMENT:
                    mpr("A terribly searing pain shoots up your arm!");
                    break;

                case SPWPN_SWORD_OF_ZONGULDROK:
                    mpr("You sense an extremely unholy aura.");
                    break;

                case SPWPN_SWORD_OF_POWER:
                    mpr("You sense an aura of extreme power.");
                    break;

                case SPWPN_STAFF_OF_OLGREB:
                    if (player_can_smell())
                        mpr("You smell chlorine.");
                    else
                        mpr("The staff glows slightly green.");
                    break;

                case SPWPN_VAMPIRES_TOOTH:
                    if (you.is_undead != US_UNDEAD)
                    {
                        mpr("You feel a strange hunger, and smell blood in "
                            "the air...");
                    }
                    else
                        mpr("You feel strangely empty.");
                    break;

                default:
                    break;
                }
            }

            // effect second
            switch (special)
            {
            case SPWPN_PROTECTION:
                you.redraw_armour_class = true;
                break;

            case SPWPN_DISTORTION:
                mpr("Space warps around you for a moment!");

                if (!was_known)
                    xom_is_stimulated(32);
                break;

            case SPWPN_SCYTHE_OF_CURSES:
                if (!item_cursed(item) && one_chance_in(3))
                    do_curse_item(item, false);
                break;

            case SPWPN_STAFF_OF_WUCAD_MU:
                MiscastEffect(&you, WIELD_MISCAST, SPTYP_DIVINATION, 9, 90,
                              "the Staff of Wucad Mu" );
                break;

            default:
                break;
            }
            if (is_fixed_artefact(item))
                you.special_wield = item_special_wield_effect(item);
        }

        if (item_cursed(item))
        {
            mpr("It sticks to your hand!");
            if (known_cursed || was_known && special == SPWPN_SCYTHE_OF_CURSES)
                xom_is_stimulated(32);
            else
                xom_is_stimulated(64);
        }

        break;
    }
    default:
        break;
    }

    if (showMsgs)
        warn_shield_penalties();

    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
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
    else if (you.duration[DUR_BERSERKER])
        canned_msg(MSG_TOO_BERSERK);
    else
    {
        slot = prompt_invent_item( mesg.c_str(), MT_INVLIST, OBJ_ARMOUR,
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
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
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
        if (player_mutation_level(MUT_HOOVES))
        {
            if (verbose)
                mpr("You can't wear boots with hooves!");
            return (false);
        }

        if (player_mutation_level(MUT_TALONS))
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

        if (!ignore_temporary && player_is_swimming()
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
    }

    if (!can_equip( slot, ignore_temporary ))
    {
        if (verbose)
            mpr("You can't wear that in your present form.");

        return (false);
    }

    // Cannot swim in heavy armour.
    if (!ignore_temporary
        && player_is_swimming()
        && slot == EQ_BODY_ARMOUR
        && !is_light_armour( item ))
    {
        if (verbose)
           mpr("You can't swim in that!");

        return (false);
    }

    // Giant races and draconians.
    if (player_size(PSIZE_TORSO) >= SIZE_LARGE || player_genus(GENPC_DRACONIAN))
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
    if (player_size(PSIZE_TORSO) <= SIZE_LITTLE)
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
    if (!is_valid_item(invitem))
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
        return (!takeoff_armour(item));

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
        if (you.equip[EQ_BODY_ARMOUR] != -1 &&
            item_cursed(you.inv[you.equip[EQ_BODY_ARMOUR]]))
        {
            if (!quiet)
            {
                mprf("%s is stuck to your body!",
                     you.inv[you.equip[EQ_BODY_ARMOUR]].name(DESC_CAP_YOUR)
                                                       .c_str());
            }
            return (false);
        }
        if (!item_cursed(you.inv[you.equip[EQ_CLOAK]]))
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

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (item_cursed(invitem))
    {
        for (int i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
        {
            if (item == you.equip[i])
            {
                mprf("%s is stuck to your body!",
                     invitem.name(DESC_CAP_YOUR).c_str());
                return (false);
            }
        }
    }

    const equipment_type slot = get_armour_slot(invitem);
    if (!you_tran_can_wear(invitem) && invitem.link == you.equip[slot])
    {
        mprf("%s is melded into your body!",
             invitem.name(DESC_CAP_YOUR).c_str());
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
            if (!item_cursed(you.inv[you.equip[EQ_CLOAK]]))
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

        if (item != you.equip[EQ_BODY_ARMOUR])
        {
            mpr("You aren't wearing that!");
            return (false);
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
            unsigned next =
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
        : m_slot(-1), selected_from_inventory(false), need_prompt(false),
          chosen_ammo(false)
    {
        m_slot = you.m_quiver->get_fire_item(&m_noitem_reason);
    }

    // targeting_behaviour API
    virtual command_type get_command(int key = -1);
    virtual bool should_redraw();
    virtual void mark_ammo_nonchosen();

    void message_ammo_prompt(const std::string* pre_text = 0);

public:
    int m_slot;
    std::string m_noitem_reason;
    bool selected_from_inventory;
    bool need_prompt;
    bool chosen_ammo;
};

void fire_target_behaviour::message_ammo_prompt(const std::string* pre_text)
{
    const int next_item = get_next_fire_item(m_slot, +1);
    bool no_other_items = (next_item == -1 || next_item == m_slot);

    mesclr();

    if (pre_text)
        mpr(pre_text->c_str());

    std::ostringstream msg;
    if (m_slot == -1)
        msg << "Firing ";
    else
    {
        const item_def& item_def = you.inv[m_slot];
        const launch_retval projected = is_launched(&you, you.weapon(),
                                                    item_def);

        if (projected == LRET_FUMBLED)
            msg << "Awkwardly throwing ";
        else if (projected == LRET_LAUNCHED)
            msg << "Firing ";
        else if (projected == LRET_THROWN)
            msg << "Throwing ";
        else
            msg << "Buggy ";
    }

    msg << (no_other_items ? "(i - inventory)"
                           : "(i - inventory. (,) - cycle)")
        << ": ";

    if (m_slot == -1)
    {
        msg << "<red>" << m_noitem_reason << "</red>";
    }
    else
    {
        const char* colour = (selected_from_inventory ? "lightgrey" : "w");
        msg << "<" << colour << ">"
            << you.inv[m_slot].name(DESC_INVENTORY_EQUIP)
            << "</" << colour << ">";
    }

    formatted_message_history(tagged_string_substr(msg.str(),
                                                   0, crawl_view.msgsz.x),
                              MSGCH_PROMPT);
}

bool fire_target_behaviour::should_redraw()
{
    if (need_prompt)
    {
        need_prompt = false;
        return (true);
    }
    return (false);
}

void fire_target_behaviour::mark_ammo_nonchosen()
{
    chosen_ammo = false;
}

command_type fire_target_behaviour::get_command(int key)
{
    if (key == -1)
        key = get_key();

    switch (key)
    {
    case '(':
    case CONTROL('N'):
    case ')':
    case CONTROL('P'):
    {
        const int direction = (key == CONTROL('P') || key == ')') ? -1 : +1;
        const int next = get_next_fire_item(m_slot, direction);
        if (next != m_slot && next != -1)
        {
            m_slot = next;
            selected_from_inventory = false;
            chosen_ammo = true;
        }
        // Do this stuff unconditionally to make the prompt redraw.
        message_ammo_prompt();
        need_prompt = true;
        return (CMD_NO_CMD);
    }
    case 'i':
    {
        std::string err;
        const int selected = _fire_prompt_for_item(err);
        if (selected >= 0 && _fire_validate_item(selected, err))
        {
            m_slot = selected;
            selected_from_inventory = true;
            chosen_ammo = true;
        }
        message_ammo_prompt( err.length() ? &err : NULL );
        need_prompt = true;
        return (CMD_NO_CMD);
    }
    case '?':
        show_targeting_help();
        redraw_screen();
        message_ammo_prompt();
        need_prompt = true;
        return (CMD_NO_CMD);
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

    beh.message_ammo_prompt();

    // XXX: This stuff should be done by direction()!
    message_current_target();
    direction( target, DIR_NONE, TARG_ENEMY, -1, false, !teleport, true, false,
               NULL, &beh );

    if (beh.m_slot == -1)
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

    you.m_quiver->on_item_fired(you.inv[beh.m_slot], beh.chosen_ammo);
    you.redraw_quiver = true;
    slot = beh.m_slot;

    return (true);
}

// Bring up an inventory screen and have user choose an item.
// Returns an item slot, or -1 on abort/failure
// On failure, returns error text, if any.
static int _fire_prompt_for_item(std::string& err)
{
    if (inv_count() < 1)
    {
        // canned_msg(MSG_NOTHING_CARRIED);         // Hmmm...
        err = "You aren't carrying anything.";
        return -1;
    }

    int slot = prompt_invent_item( "Fire/throw which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_THROWABLE, true, true, true, 0, -1,
                                   NULL, OPER_FIRE );

    if (slot == PROMPT_ABORT || slot == PROMPT_NOTHING)
    {
        err = "Nothing selected.";
        return -1;
    }
    return slot;
}

// Returns false and err text if this item can't be fired.
static bool _fire_validate_item(int slot, std::string &err)
{
    if (slot == you.equip[EQ_WEAPON]
        && you.inv[slot].base_type == OBJ_WEAPONS
        && item_cursed(you.inv[slot]))
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
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
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
    if (you.duration[DUR_BERSERKER])
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
    int slot = _fire_prompt_for_item(warn);

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
    hands_reqd_type hands = hands_reqd(launcher, player_size());

    switch (hands)
    {
    default:
    case HANDS_ONE:
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
    int speed_stat = str_weight * you.strength + dex_weight * you.dex;

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

static int _item_to_skill_level(const item_def *item)
{
    skill_type type = range_skill(*item);

    if (type == SK_DARTS || type == SK_SLINGS)
        return (you.skills[type] + you.skills[SK_THROWING]);

    return (2 * you.skills[type]);
}

static bool _poison_hit_victim(bolt& beam, actor* victim, int dmg, int corpse)
{
    if (!victim->alive() || victim->res_poison() > 0)
        return (false);

    if (beam.is_tracer)
        return (true);

    int levels = 0;

    actor* agent = beam.agent();

    if (agent->atype() == ACT_MONSTER)
    {
        if (dmg > 0 || beam.ench_power == AUTOMATIC_HIT
                       && x_chance_in_y(90 - 3 * victim->armour_class(), 100))
        {
            levels = 1 + random2(3);
        }
    }
    else
    {
        if (beam.ench_power == AUTOMATIC_HIT
            && x_chance_in_y(90 - 3 * victim->armour_class(), 100))
        {
            levels = 2;
        }
        else if (random2(dmg) > random2(victim->armour_class()))
            levels = 1;

        int num_success = 0;
        if (YOU_KILL(beam.thrower))
        {
            const int skill_level = _item_to_skill_level(beam.item);
            if (x_chance_in_y(skill_level + 25, 50))
                num_success++;
            if (x_chance_in_y(skill_level, 50))
                num_success++;
        }
        else
            num_success = 1;

        if (num_success == 0)
            return (false);
        else
        {
            if (num_success == 2)
                levels++;
        }
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

    if (!beam.is_tracer && you.can_see(victim))
        mprf("The %s passes through %s!", beam.name.c_str(),
             victim->name(DESC_NOCAP_THE).c_str());

    return (true);
}

static bool _silver_damages_victim(bolt &beam, actor* victim, int &dmg,
                                   std::string &dmg_msg)
{
    bool shifter;

    if (victim->atype() == ACT_MONSTER)
    {
        monsters* mon = dynamic_cast<monsters*>(victim);
        shifter = mons_is_shapeshifter(mon);
    }
    else
        shifter = transform_changed_physiology();

    mon_holy_type holiness = victim->holiness();

    if (shifter || holiness == MH_UNDEAD || holiness == MH_DEMONIC)
    {
        dmg *= 2;

        if (!beam.is_tracer && you.can_see(victim))
           dmg_msg = "The silver sears " + victim->name(DESC_NOCAP_THE) + "!";
    }

    return (false);
}

static bool _shadow_hit_victim(bolt& beam, actor* victim, int dmg, int corpse)
{
    if (beam.is_tracer || victim->alive() || corpse == -1
        || corpse == NON_ITEM)
    {
        return (false);
    }

    actor*   agent = beam.agent();
    beh_type beh;
    unsigned short hitting;

    if (agent->atype() == ACT_PLAYER)
    {
        hitting = you.pet_target;
        beh     = BEH_FRIENDLY;
    }
    else
    {
        monsters *mon = dynamic_cast<monsters*>(agent);

        beh = SAME_ATTITUDE(mon);

        // Get a new foe for the zombie to target.
        behaviour_event(mon, ME_EVAL);
        hitting = mon->foe;
    }

    int midx = NON_MONSTER;
    if (animate_remains(victim->pos(), CORPSE_BODY, beh, hitting, GOD_NO_GOD,
                        true, true, &midx) <= 0)
    {
        return (false);
    }

    monsters *zomb = &menv[midx];

    if (you.can_see(victim))
        mprf("%s turns into a zombie!", victim->name(DESC_CAP_THE).c_str());
    else if (you.can_see(zomb))
        mprf("%s appears out of thin air!", zomb->name(DESC_CAP_THE).c_str());

    return (true);
}

static bool _dispersal_hit_victim(bolt& beam, actor* victim, int dmg,
                                  int corpse)
{
    const actor* agent = beam.agent();

    if (!victim->alive() || victim == agent)
        return (false);

    if (beam.is_tracer)
        return (true);

    const bool was_seen = you.can_see(victim);
    const bool no_sanct = victim->kill_alignment() == KC_OTHER;

    coord_def pos, pos2;

    int tries = 0;
    do
    {
        if (!random_near_space(victim->pos(), pos, false, true, no_sanct))
            return (false);
    } while (!victim->is_habitable(pos) && tries++ < 100);

    if (!victim->is_habitable(pos))
        return (false);

    tries = 0;
    do
    {
        random_near_space(victim->pos(), pos2, false, true, no_sanct);
    } while (!victim->is_habitable(pos2) && tries++ < 100);

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
        clear_trapping_net();
        victim->moveto(pos);
        mpr("You blink!");
    }
    else
    {
        monsters *mon = dynamic_cast<monsters*>(victim);

        if (!(mon->flags & MF_WAS_IN_VIEW))
            mon->seen_context = "thin air";

        mons_clear_trapping_net(mon);
        mon->move_to_pos(pos);
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

void setup_missile_beam(const actor *agent, bolt &beam, item_def &item,
                        std::string &ammo_name, bool &returning)
{
    dungeon_char_type zapsym = DCHAR_SPACE;
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

    beam.type = dchar_glyph(zapsym);

    returning = get_weapon_brand(item) == SPWPN_RETURNING
                || get_ammo_brand(item) == SPMSL_RETURNING;

    if (agent->atype() == ACT_PLAYER)
    {
        beam.attitude      = ATT_FRIENDLY;
        beam.beam_source   = NON_MONSTER;
        beam.smart_monster = true;
        beam.thrower       = KILL_YOU_MISSILE;
    }
    else
    {
        const monsters *mon = dynamic_cast<const monsters*>(agent);

        beam.attitude      = mons_attitude(mon);
        beam.beam_source   = mon->mindex();
        beam.smart_monster = (mons_intel(mon) >= I_NORMAL);
        beam.thrower       = KILL_MON_MISSILE;
    }

    beam.item     = &item;
    beam.source   = agent->pos();
    beam.colour   = item.colour;
    beam.flavour  = BEAM_MISSILE;
    beam.is_beam  = false;
    beam.aux_source.clear();

    beam.can_see_invis = agent->can_see_invisible();

    const item_def *launcher  = const_cast<actor*>(agent)->weapon(0);
    if (launcher && !item.launched_by(*launcher))
        launcher = NULL;

    int bow_brand = SPWPN_NORMAL;
    if (launcher != NULL)
        bow_brand = get_weapon_brand(*launcher);

    int  ammo_brand = get_ammo_brand(item);
    bool poisoned   = ammo_brand == SPMSL_POISONED;

    if (bow_brand == SPWPN_VENOM && ammo_brand != SPMSL_CURARE)
    {
        if (ammo_brand == SPMSL_NORMAL)
            item.special = SPMSL_POISONED;

        poisoned = true;
    }

    const bool exploding = ammo_brand == SPMSL_EXPLODING;
    const bool penetrating = !exploding
        && (bow_brand  == SPWPN_PENETRATION
            || ammo_brand == SPMSL_PENETRATION);
    const bool silver = ammo_brand == SPMSL_SILVER;
    const bool disperses = ammo_brand == SPMSL_DISPERSAL;
    const bool shadow = bow_brand  == SPWPN_SHADOW
                        || ammo_brand == SPMSL_SHADOW;

    ASSERT(!exploding || !is_artefact(item));

    beam.name = item.name(DESC_PLAIN, false, false, false);

    // Print type of item as influenced by launcher.
    item_def ammo = item;

    // The chief advantage here is the extra damage this does
    // against susceptible creatures.

    // Note: weapons & ammo of eg fire are not cumulative
    // ammo of fire and weapons of frost don't work together,
    // and vice versa.

    // Note that bow_brand is known since the bow is equipped.

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
        beam.name    = "chaos";
        beam.colour  = EC_RANDOM;

        ammo.special = SPMSL_CHAOS;
    }
    else if ((bow_brand == SPWPN_FLAME || ammo_brand == SPMSL_FLAME)
             && ammo_brand != SPMSL_FROST && bow_brand != SPWPN_FROST)
    {
        beam.flavour = BEAM_FIRE;
        beam.name    = "flame";
        beam.colour  = RED;

        ammo.special = SPMSL_FLAME;
    }
    else if ((bow_brand == SPWPN_FROST || ammo_brand == SPMSL_FROST)
             && ammo_brand != SPMSL_FLAME && bow_brand != SPWPN_FLAME)
    {
        beam.flavour = BEAM_COLD;
        beam.name    = "frost";
        beam.colour  = WHITE;

        ammo.special = SPMSL_FROST;
    }

    ASSERT(beam.flavour == BEAM_MISSILE || !is_artefact(item));

    ammo_name = ammo.name(DESC_PLAIN);

    if (silver)
        beam.damage_funcs.push_back(_silver_damages_victim);
    if (poisoned)
        beam.hit_funcs.push_back(_poison_hit_victim);
    if (penetrating)
        beam.range_funcs.push_back(_item_penetrates_victim);
    if (shadow)
        beam.hit_funcs.push_back(_shadow_hit_victim);
    if (disperses)
        beam.hit_funcs.push_back(_dispersal_hit_victim);

    if (shadow && ammo.special != SPMSL_SHADOW)
    {
        beam.name = "shadowy " + beam.name;
        ammo_name = "shadowy " + ammo_name;
    }

    if (disperses && ammo.special != SPMSL_DISPERSAL)
    {
        beam.name = "dispersing " + beam.name;
        ammo_name = "dispersing " + ammo_name;
    }

    if (poisoned && ammo.special != SPMSL_POISONED)
    {
        beam.name = "poison "   + beam.name;
        ammo_name = "poisoned " + ammo_name;
    }

    if (penetrating && ammo.special != SPMSL_PENETRATION)
    {
        beam.name = "penetrating " + beam.name;
        ammo_name = "penetrating " + ammo_name;
    }

    if (silver && ammo.special != SPMSL_SILVER)
    {
        beam.name = "silvery " + beam.name;
        ammo_name = "silvery " + ammo_name;
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
                 ammo.name(DESC_PLAIN, false, false, false, false,
                           ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK
                           | ISFLAG_RACIAL_MASK);

             expl->name = replace_all(expl->name, ammo.name(DESC_PLAIN),
                                      short_name);
         }
         expl->name = "explosion of " + expl->name;

         beam.special_explosion = expl;
    }

    if (exploding && ammo.special != SPMSL_EXPLODING)
    {
        beam.name = "exploding " + beam.name;
        ammo_name = "exploding " + ammo_name;
    }

    if (beam.flavour != BEAM_MISSILE)
    {
        returning = false;

        beam.type = dchar_glyph(DCHAR_FIRED_BOLT);
        beam.name = "bolt of " + beam.name;
    }

    if (!is_artefact(item))
        ammo_name = article_a(ammo_name, true);
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
    if (bow_brand == SPWPN_SHADOW && ammo_brand == SPMSL_SHADOW)
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
    return stat_adjust(dam, you.strength, 15, 160, 90);
}

static int dex_adjust_thrown_tohit(int hit)
{
    return stat_adjust(hit, you.dex, 13, 160, 90);
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
    if (!is_valid_item(you.inv[slot]))
        return;

    bool done_anything = false;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (i == slot || !is_valid_item(you.inv[i]))
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

// throw_it - currently handles player throwing only.  Monster
// throwing is handled in mstuff2:mons_throw()
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

    if (target)
        thr = *target;
    else
    {
        message_current_target();
        direction( thr, DIR_NONE, TARG_ENEMY );

        if (!thr.isValid)
        {
            if (thr.isCancel)
                canned_msg(MSG_OK);

            return (false);
        }
    }
    pbolt.set_target(thr);

    item_def& thrown = you.inv[throw_2];
    ASSERT(is_valid_item(thrown));

    // Making a copy of the item: changed only for venom launchers.
    item_def item = thrown;
    item.quantity = 1;
    item.slot     = index_to_letter(item.link);

    std::string ammo_name;
    setup_missile_beam(&you, pbolt, item, ammo_name, returning);

    // Did we know the ammo's brand before throwing it?
    const bool ammo_brand_known = item_type_known(thrown);

    // Get the ammo/weapon type.  Convenience.
    const object_class_type wepClass = thrown.base_type;
    const int               wepType  = thrown.sub_type;

    // Figure out if we're thrown or launched.
    const launch_retval projected = is_launched(&you, you.weapon(), thrown);

    // Determine range.
    int max_range = 0;
    int range = 0;

    if (projected)
    {
        if (wepType == MI_LARGE_ROCK)
        {
            range     = 1 + random2( you.strength / 5 );
            max_range = you.strength / 5;
            if (you.can_throw_large_rocks())
            {
                range     += random_range(4, 7);
                max_range += 7;
            }
        }
        else if (wepType == MI_THROWING_NET)
        {
            max_range = range = 2 + player_size(PSIZE_BODY);
        }
        else
        {
            max_range = range = LOS_RADIUS;
        }
    }
    else
    {
        // Range based on mass & strength, between 1 and 9.
        max_range = range = std::max(you.strength-item_mass(thrown)/10 + 3, 1);
    }

    range = std::min(range, LOS_RADIUS);
    max_range = std::min(max_range, LOS_RADIUS);

    // For the tracer, use max_range. For the actual shot, use range.
    pbolt.range = max_range;

    // Don't do the tracing when using Portaled Projectile, or when confused.
    if (!teleport && !you.confused())
    {
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
    }
    pbolt.is_tracer = false;

    // Use real range for firing.
    pbolt.range = range;

    // Must unwield before making a copy in order to remove things
    // like temporary branding.
    if (throw_2 == you.equip[EQ_WEAPON] && thrown.quantity == 1)
    {
        unwield_item();
        canned_msg(MSG_EMPTY_HANDED);
    }

    // Now start real firing!
    origin_set_unknown(item);

    if (is_blood_potion(item) && thrown.quantity > 1)
    {
        // Initialize thrown potion with oldest potion in stack.
        long val = remove_oldest_blood_potion(thrown);
        val -= you.num_turns;
        item.props.clear();
        init_stack_blood_potions(item, val);
    }

    if (you.confused())
    {
        thr.isTarget = true;
        thr.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
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
    baseHit = std::min(0, you.strength - item_mass(item) / 10);
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

    const int ammo_brand = get_ammo_brand( item );

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

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
                "Base hit == %d; Base damage == %d "
                "(item %d + launcher %d)",
                        baseHit, baseDam,
                        item_base_dam, lnch_base_dam);
#endif

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

        const int speed = launcher_final_speed(launcher, player_shield());
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Final launcher speed: %d", speed);
#endif
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
            // really all that good, and it's harder to get ammo anyways.
            exercise(SK_SLINGS, 1 + random2avg(3, 2));

            // Sling bullets are designed for slinging and easier to aim.
            if (wepType == MI_SLING_BULLET)
                baseHit += 4;

            exHitBonus += (effSkill * 3) / 2;

            // Strength is good if you're using a nice sling.
            int strbonus = (10 * (you.strength - 10)) / 9;
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
        // Blowguns take a _very_ steady hand;  a lot of the bonus
        // comes from dexterity.  (Dex bonus here as well as below).
        case SK_DARTS:
            baseHit -= 2;
            exercise(SK_DARTS, (coinflip()? 2 : 1));
            exHitBonus += (effSkill * 3) / 2 + you.dex / 2;

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
            int strbonus = (10 * (you.strength - 10)) / 4;
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

            if (lnchType == WPN_HAND_CROSSBOW)
            {
                exHitBonus -= 2;
                dice_mult = dice_mult * 26 / 30;
            }
            break;

        default:
            break;
        }

        // Slings and Darts train Throwing a bit.
        if (launcher_skill == SK_SLINGS || launcher_skill == SK_DARTS)
        {
            if (coinflip())
                exercise(SK_THROWING, 1);

            // They also get a minor tohit boost from throwing skill.
            exHitBonus += you.skills[SK_THROWING] / 5;
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

            // Dwarves/orcs with dwarven/orcish weapons.
            if (get_equip_race(item) == ISFLAG_DWARVEN
                   && player_genus(GENPC_DWARVEN)
                || get_equip_race(item) == ISFLAG_ORCISH
                   && you.species == SP_HILL_ORC)
            {
                baseDam++;
            }

            exDamBonus =
                (10 * (you.skills[SK_THROWING] / 2 + you.strength - 10)) / 12;

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
                exHitBonus  = you.skills[SK_DARTS] * 2;
                exHitBonus += (you.skills[SK_THROWING] * 2) / 3;
                exDamBonus  = you.skills[SK_DARTS] / 3;
                exDamBonus += you.skills[SK_THROWING] / 5;

                // exercise skills
                exercise(SK_DARTS, 1 + random2avg(3, 2));
                break;
            case MI_JAVELIN:
                // Javelins use throwing skill.
                exHitBonus += skill_bump(SK_THROWING);
                exDamBonus += you.skills[SK_THROWING] * 3 / 5;

                // Adjust for strength and dex.
                exDamBonus = str_adjust_thrown_damage(exDamBonus);
                exHitBonus = dex_adjust_thrown_tohit(exHitBonus);

                // High dex helps damage a bit, too (aim for weak spots).
                exDamBonus = stat_adjust(exDamBonus, you.dex, 20, 150, 100);

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

        // [dshaligram] The defined base damage applies only when used
        // for launchers. Hand-thrown stones and darts do only half
        // base damage. Yet another evil 4.0ism.
        if (wepClass == OBJ_MISSILES
            && (wepType == MI_DART || wepType == MI_STONE))
        {
            baseDam = div_rand_round(baseDam, 2);
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
            exHitBonus += you.dex / 2;

            // slaying bonuses
            if (projected != LRET_LAUNCHED || wepType != MI_NEEDLE)
            {
                slayDam = slaying_bonus(PWPN_DAMAGE);
                slayDam = (slayDam < 0 ? -random2(1 - slayDam)
                                       :  random2(1 + slayDam));
            }

            exHitBonus += slaying_bonus(PWPN_HIT);
        }
    }
    else // LRET_FUMBLED
    {
        if (one_chance_in(20))
            exercise(SK_THROWING, 1);

        exHitBonus = you.dex / 4;

        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
        {
            // Throwing needles is now seriously frowned upon; it's difficult
            // to grip a fiddly little needle, and not penalising it cheapens
            // blowguns.
            exHitBonus -= (30 - you.skills[SK_DARTS]) / 3;
            baseHit    -= (30 - you.skills[SK_DARTS]) / 3;
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Needle base hit = %d, exHitBonus = %d",
                    baseHit, exHitBonus);
#endif
        }
    }

    // FINALIZE tohit and damage
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

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS,
            "H:%d+%d;a%dl%d.  D:%d+%d;a%dl%d -> %d,%dd%d",
              baseHit, exHitBonus, ammoHitBonus, lnchHitBonus,
              baseDam, exDamBonus, ammoDamBonus, lnchDamBonus,
              pbolt.hit, pbolt.damage.num, pbolt.damage.size );
#endif

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
        if (acc_bonus != DEBUG_COOKIE)
            pbolt.drop_object();
    }
    else
    {
        if (Options.tutorial_left)
            Options.tut_throw_counter++;

        // Dropping item copy, since the launched item might be different.
        pbolt.drop_item = !did_return;
        pbolt.fire();

        // The item can be destroyed before returning.
        if (did_return && thrown_object_destroyed(&item, pbolt.target, true))
            did_return = false;
    }

    if (bow_brand == SPWPN_CHAOS || ammo_brand == SPMSL_CHAOS)
    {
        did_god_conduct(DID_CHAOS, 2 + random2(3),
                        bow_brand == SPWPN_CHAOS || ammo_brand_known);
    }

    if (ammo_brand ==  SPMSL_SHADOW || bow_brand == SPWPN_SHADOW)
    {
        did_god_conduct(DID_NECROMANCY, 2,
                        bow_brand == SPWPN_SHADOW || ammo_brand_known);
    }

    if (did_return)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow(true, false);
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
    }

    // Throwing and blowguns are silent...
    if (projected == LRET_LAUNCHED && lnchType != WPN_BLOWGUN)
        noisy(6, you.pos());

    // ...but any monster nearby can see that something has been thrown.
    alert_nearby_monsters();

    if (ammo_ided)
        _merge_ammo_in_inventory(throw_2);

    you.turn_is_over = true;

    if (pbolt.special_explosion != NULL)
        delete pbolt.special_explosion;

    return (hit);
}

bool thrown_object_destroyed(item_def *item, const coord_def& where,
                             bool returning)
{
    ASSERT(item != NULL);

    int chance = 0;

    std::string name = item->name(DESC_PLAIN, false, true, false);

    // Exploding missiles are always destroyed.
    if (name.find("explod") != std::string::npos)
        return (true);

    if (item->base_type == OBJ_MISSILES)
    {
        int brand = get_ammo_brand(*item);
        // [dshaligram] Removed influence of Throwing on ammo preservation.
        // The effect is nigh impossible to perceive.
        switch (item->sub_type)
        {
        case MI_NEEDLE:
            chance = (brand == SPMSL_CURARE ? 3 : 6);
            break;
        case MI_SLING_BULLET:
        case MI_STONE:   chance =  4; break;
        case MI_DART:    chance =  3; break;
        case MI_ARROW:   chance =  4; break;
        case MI_BOLT:    chance =  4; break;
        case MI_JAVELIN: chance = 10; break;
        case MI_THROWING_NET: break; // Doesn't get destroyed by throwing.

        case MI_LARGE_ROCK:
        default:
            chance = 25;
            break;
        }
        if (brand == SPMSL_STEEL)
            chance *= 10;
    }

    // Enchanted projectiles get an extra shot at avoiding
    // destruction: plus / (1 + plus) chance of survival.
    bool destroyed = (chance == 0) ? false :
        (one_chance_in(chance) && one_chance_in(item->plus));
    bool hostile_grid = grid_destroys_items(grd(where));

    // Non-returning items thrown into item-destroying grids are always
    // destroyed.  Returning items are only destroyed if they would have
    // been randomly destroyed anyway.
    if (returning && !destroyed)
        hostile_grid = false;

    if (hostile_grid)
    {
        if (player_can_hear(where))
            mprf(MSGCH_SOUND, grid_item_destruction_message(grd(where)));

        item_was_destroyed(*item, NON_MONSTER);
        destroyed = true;
    }

    return destroyed;
}

void jewellery_wear_effects(item_def &item)
{
    item_type_id_state_type ident        = ID_TRIED_TYPE;
    randart_prop_type       fake_rap     = RAP_NUM_PROPERTIES;
    bool                    learn_pluses = false;

    // Randart jewellery shouldn't auto-ID just because the base type
    // is known. Somehow the player should still be told, preferably
    // by message. (jpeg)
    const bool artefact     = is_random_artefact(item);
    const bool known_pluses = item_ident(item, ISFLAG_KNOW_PLUSES);
    const bool known_cursed = item_known_cursed(item);
    const bool known_bad    = (item_type_known(item)
                               && item_value(item) <= 2);

    switch (item.sub_type)
    {
    case RING_FIRE:
    case RING_HUNGER:
    case RING_ICE:
    case RING_LIFE_PROTECTION:
    case RING_POISON_RESISTANCE:
    case RING_PROTECTION_FROM_COLD:
    case RING_PROTECTION_FROM_FIRE:
    case RING_PROTECTION_FROM_MAGIC:
    case RING_SUSTAIN_ABILITIES:
    case RING_SUSTENANCE:
    case RING_SLAYING:
    case RING_SEE_INVISIBLE:
    case RING_WIZARDRY:
    case RING_REGENERATION:
    case RING_TELEPORT_CONTROL:
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        if (item.plus != 0)
        {
            if (!artefact)
                ident = ID_KNOWN_TYPE;
            else if (!known_pluses)
            {
                mprf("You feel %s.", item.plus > 0 ?
                     "well-protected" : "more vulnerable");
            }
            learn_pluses = true;
        }
        break;

    case RING_INVISIBILITY:
        if (!you.duration[DUR_INVIS])
        {
            mpr("You become transparent for a moment.");
            if (artefact)
                fake_rap = RAP_INVISIBLE;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        if (item.plus != 0)
        {
            if (!artefact)
                ident = ID_KNOWN_TYPE;
            else if (!known_pluses)
                mprf("You feel %s.", item.plus > 0? "nimbler" : "more awkward");
            learn_pluses = true;
        }
        break;

    case RING_STRENGTH:
        if (item.plus)
        {
            modify_stat(STAT_STRENGTH, item.plus, false, item);

            if (artefact)
                fake_rap = RAP_STRENGTH;
            else
                ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_DEXTERITY:
        if (item.plus)
        {
            modify_stat(STAT_DEXTERITY, item.plus, false, item);

            if (artefact)
                fake_rap = RAP_DEXTERITY;
            else
                ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_INTELLIGENCE:
        if (item.plus)
        {
            modify_stat(STAT_INTELLIGENCE, item.plus, false, item);

            if (artefact)
                fake_rap = RAP_INTELLIGENCE;
            else
                ident = ID_KNOWN_TYPE;

           learn_pluses = true;
        }
        break;

    case RING_MAGICAL_POWER:
        mpr("You feel your mana capacity increase.");
        calc_mp();
        if (artefact)
            fake_rap = RAP_MAGICAL_POWER;
        else
            ident = ID_KNOWN_TYPE;
        break;

    case RING_LEVITATION:
        if (!scan_randarts(RAP_LEVITATE))
        {
            if (player_is_airborne())
                mpr("You feel vaguely more buoyant than before.");
            else
                mpr("You feel buoyant.");
            if (artefact)
                fake_rap = RAP_LEVITATE;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case RING_TELEPORTATION:
        if (!scan_randarts(RAP_CAN_TELEPORT))
        {
            mpr("You feel slightly jumpy.");
            if (artefact)
                fake_rap = RAP_CAUSE_TELEPORTATION;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case AMU_RAGE:
        if (!scan_randarts(RAP_BERSERK))
        {
            mpr("You feel a brief urge to hack something to bits.");
            if (artefact)
                fake_rap = RAP_BERSERK;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case AMU_THE_GOURMAND:
        you.duration[DUR_GOURMAND] = 0;
        break;

    case AMU_CONTROLLED_FLIGHT:
        if (you.duration[DUR_LEVITATION]
            && !extrinsic_amulet_effect(AMU_CONTROLLED_FLIGHT))
        {
            ident = ID_KNOWN_TYPE;
        }
        break;
    }

    // Artefacts have completely different appearance than base types
    // so we don't allow them to make the base types known.
    if (artefact)
    {
        use_randart(item);

        if (learn_pluses && (item.plus != 0 || item.plus2 != 0))
            set_ident_flags(item, ISFLAG_KNOW_PLUSES);

        if (fake_rap != RAP_NUM_PROPERTIES)
            randart_wpn_learn_prop(item, fake_rap);

        if (!item.props.exists("jewellery_tried")
            || !item.props["jewellery_tried"].get_bool())
        {
            item.props["jewellery_tried"].get_bool() = true;
        }
    }
    else
    {
        set_ident_type(item, ident);

        if (ident == ID_KNOWN_TYPE)
            set_ident_flags(item, ISFLAG_EQ_JEWELLERY_MASK);
    }

    if (item_cursed(item))
    {
        mprf("Oops, that %s feels deathly cold.",
             jewellery_is_amulet(item)? "amulet" : "ring");
        learned_something_new(TUT_YOU_CURSED);

        if (known_cursed || known_bad)
            xom_is_stimulated(64);
        else
            xom_is_stimulated(128);
    }

    // Cursed or not, we know that since we've put the ring on.
    set_ident_flags(item, ISFLAG_KNOW_CURSE);

    mpr(item.name(DESC_INVENTORY_EQUIP).c_str());
}

static int prompt_ring_to_remove(int new_ring)
{
    const item_def &left  = you.inv[you.equip[EQ_LEFT_RING]];
    const item_def &right = you.inv[you.equip[EQ_RIGHT_RING]];

    if (item_cursed(left) && item_cursed(right))
    {
        mprf("You're already wearing two cursed rings!");
        return (-1);
    }

    mesclr();
    mprf("Wearing %s.", you.inv[new_ring].name(DESC_NOCAP_A).c_str());

    char lslot = index_to_letter(left.link);
    char rslot = index_to_letter(right.link);

    mprf(MSGCH_PROMPT,
         "You're wearing two rings. Remove which one? (%c/%c/Esc)",
         lslot, rslot);

    mprf(" %s", left.name(DESC_INVENTORY).c_str());
    mprf(" %s", right.name(DESC_INVENTORY).c_str());

    int c;
    do
        c = getch();
    while (c != lslot && c != rslot && c != ESCAPE && c != ' ');

    mesclr();

    if (c == ESCAPE || c == ' ')
        return (-1);

    const int eqslot = (c == lslot) ? EQ_LEFT_RING : EQ_RIGHT_RING;

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
    int prop_str = 0;
    int prop_dex = 0;
    int prop_int = 0;

    // Don't warn when putting on an unknown item.
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

    if (is_random_artefact(item))
    {
        prop_str += randart_known_wpn_property(item, RAP_STRENGTH);
        prop_int += randart_known_wpn_property(item, RAP_INTELLIGENCE);
        prop_dex += randart_known_wpn_property(item, RAP_DEXTERITY);
    }

    if (remove)
    {
        if (prop_str >= you.strength || prop_int >= you.intel
            || prop_dex >= you.dex)
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
    else // put on
    {
        if (-prop_str >= you.strength || -prop_int >= you.intel
            || -prop_dex >= you.dex)
        {
            if (!quiet)
            {
                mprf(MSGCH_WARN, "%s this item would be fatal, so you refuse "
                                 "to do that.",
                                 (item.base_type == OBJ_WEAPONS ? "Wielding"
                                                                : "Wearing"));
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
    // Ask the player which existing ring is persona non grata.
    int unwanted = prompt_ring_to_remove(ring_slot);
    if (unwanted == -1)
    {
        canned_msg(MSG_OK);
        return (false);
    }

    if (!remove_ring(unwanted, false))
        return (false);

    // Put on the new ring.
    if (!safe_to_remove_or_wear(you.inv[ring_slot], false))
        return (false);

    start_delay(DELAY_JEWELLERY_ON, 1, ring_slot);

    return (true);
}

bool puton_item(int item_slot, bool prompt_finger)
{
    if (item_slot == you.equip[EQ_LEFT_RING]
        || item_slot == you.equip[EQ_RIGHT_RING]
        || item_slot == you.equip[EQ_AMULET])
    {
        return (!remove_ring(item_slot));
    }

    if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are wielding that object.");
        return (false);
    }

    if (you.inv[item_slot].base_type != OBJ_JEWELLERY)
    {
        mpr("You can only put on jewellery.");
        return (false);
    }

    const bool is_amulet = jewellery_is_amulet( you.inv[item_slot] );

    if (!is_amulet)     // ie it's a ring
    {
        if (you.equip[EQ_GLOVES] != -1
            && item_cursed( you.inv[you.equip[EQ_GLOVES]] ))
        {
            mpr("You can't take your gloves off to put on a ring!");
            return (false);
        }

        if (you.equip[EQ_LEFT_RING] != -1
            && you.equip[EQ_RIGHT_RING] != -1)
        {
            return _swap_rings(item_slot);
        }
    }
    else if (you.equip[EQ_AMULET] != -1)
    {
        if (!check_warning_inscriptions(you.inv[you.equip[EQ_AMULET]],
                                        OPER_REMOVE)
            || !remove_ring( you.equip[EQ_AMULET], true ))
        {
            return (false);
        }

        // Put on the new amulet.
        if (!safe_to_remove_or_wear(you.inv[item_slot], false))
            return (false);

        start_delay(DELAY_JEWELLERY_ON, 1, item_slot);

        // Assume it's going to succeed.
        return (true);
    }

    // Put on the amulet.
    if (!safe_to_remove_or_wear(you.inv[item_slot], false))
        return (false);

    // First ring goes on left hand if we're choosing automatically.
    int hand_used = 0;

    if (you.equip[EQ_LEFT_RING] != -1)
        hand_used = 1;

    if (you.equip[EQ_RIGHT_RING] != -1)
        hand_used = 0;

    if (is_amulet)
        hand_used = 2;
    else if (prompt_finger
                && you.equip[EQ_LEFT_RING] == -1
                && you.equip[EQ_RIGHT_RING] == -1)
    {
        mpr("Put on which hand (l or r)?", MSGCH_PROMPT);

        int keyin = get_ch();

        if (keyin == 'l')
            hand_used = 0;
        else if (keyin == 'r')
            hand_used = 1;
        else if (keyin == ESCAPE)
            return (false);
        else
        {
            mpr("You don't have such a hand!");
            return (false);
        }
    }

    you.equip[ EQ_LEFT_RING + hand_used ] = item_slot;

    jewellery_wear_effects( you.inv[item_slot] );

    // Putting on jewellery is as fast as wielding weapons.
    you.time_taken /= 2;
    you.turn_is_over = true;

    return (true);
}

bool puton_ring(int slot, bool prompt_finger)
{
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
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

    if (you.duration[DUR_BERSERKER])
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

    return puton_item(item_slot, prompt_finger);
}

void jewellery_remove_effects(item_def &item, bool mesg)
{
    // The ring/amulet must already be removed from you.equip at this point.

    // Turn off show_uncursed before getting the item name, because this item
    // was just removed, and the player knows it's uncursed.
    const bool old_showuncursed = Options.show_uncursed;
    Options.show_uncursed = false;

    if (mesg)
        mprf("You remove %s.", item.name(DESC_NOCAP_YOUR).c_str() );

    Options.show_uncursed = old_showuncursed;

    switch (item.sub_type)
    {
    case RING_FIRE:
    case RING_HUNGER:
    case RING_ICE:
    case RING_LIFE_PROTECTION:
    case RING_POISON_RESISTANCE:
    case RING_PROTECTION_FROM_COLD:
    case RING_PROTECTION_FROM_FIRE:
    case RING_PROTECTION_FROM_MAGIC:
    case RING_REGENERATION:
    case RING_SEE_INVISIBLE:
    case RING_SLAYING:
    case RING_SUSTAIN_ABILITIES:
    case RING_SUSTENANCE:
    case RING_TELEPORTATION:
    case RING_WIZARDRY:
    case RING_TELEPORT_CONTROL:
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = true;
        break;

    case RING_EVASION:
        you.redraw_evasion = true;
        break;

    case RING_STRENGTH:
        modify_stat(STAT_STRENGTH, -item.plus, false, item, true);
        break;

    case RING_DEXTERITY:
        modify_stat(STAT_DEXTERITY, -item.plus, false, item, true);
        break;

    case RING_INTELLIGENCE:
        modify_stat(STAT_INTELLIGENCE, -item.plus, false, item, true);
        break;

    case RING_MAGICAL_POWER:
        mpr("You feel your mana capacity decrease.");
        // dec_max_mp(9);
        break;

    case AMU_THE_GOURMAND:
        you.duration[DUR_GOURMAND] = 0;
        break;
    }

    if (is_random_artefact(item))
        unuse_randart(item);

    // Must occur after ring is removed. -- bwr
    calc_mp();
}

bool remove_ring(int slot, bool announce)
{
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
    {
        mpr("You can't wear or remove anything in your present form.");
        return (false);
    }

    equipment_type hand_used = EQ_NONE;
    int ring_wear_2;

    if (you.equip[EQ_LEFT_RING] == -1 && you.equip[EQ_RIGHT_RING] == -1
        && you.equip[EQ_AMULET] == -1)
    {
        mpr("You aren't wearing any rings or amulets.");
        return (false);
    }

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (you.equip[EQ_GLOVES] != -1
        && item_cursed( you.inv[you.equip[EQ_GLOVES]] )
        && you.equip[EQ_AMULET] == -1)
    {
        mpr("You can't take your gloves off to remove any rings!");
        return (false);
    }

    if (you.equip[EQ_LEFT_RING] != -1 && you.equip[EQ_RIGHT_RING] == -1
        && you.equip[EQ_AMULET] == -1)
    {
        hand_used = EQ_LEFT_RING;
    }

    if (you.equip[EQ_LEFT_RING] == -1 && you.equip[EQ_RIGHT_RING] != -1
        && you.equip[EQ_AMULET] == -1)
    {
        hand_used = EQ_RIGHT_RING;
    }

    if (you.equip[EQ_LEFT_RING] == -1 && you.equip[EQ_RIGHT_RING] == -1
        && you.equip[EQ_AMULET] != -1)
    {
        hand_used = EQ_AMULET;
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
            return (false);

        if (you.inv[equipn].base_type != OBJ_JEWELLERY)
        {
            mpr("That isn't a piece of jewellery.");
            return (false);
        }

        if (you.equip[EQ_LEFT_RING] == equipn)
            hand_used = EQ_LEFT_RING;
        else if (you.equip[EQ_RIGHT_RING] == equipn)
            hand_used = EQ_RIGHT_RING;
        else if (you.equip[EQ_AMULET] == equipn)
            hand_used = EQ_AMULET;
        else
        {
            mpr("You aren't wearing that.");
            return (false);
        }
    }

    if (!check_warning_inscriptions(you.inv[you.equip[hand_used]],
                                         OPER_REMOVE))
    {
        canned_msg(MSG_OK);
        return (false);
    }

    if (you.equip[EQ_GLOVES] != -1
        && item_cursed( you.inv[you.equip[EQ_GLOVES]] )
        && (hand_used == EQ_LEFT_RING || hand_used == EQ_RIGHT_RING))
    {
        mpr("You can't take your gloves off to remove any rings!");
        return (false);
    }

    if (you.equip[hand_used] == -1)
    {
        mpr("I don't think you really meant that.");
        return (false);
    }

    if (item_cursed( you.inv[you.equip[hand_used]] ))
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

    you.equip[hand_used] = -1;

    jewellery_remove_effects(you.inv[ring_wear_2]);

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

void zap_wand(int slot)
{
    bolt beam;
    dist zap_wand;
    int item_slot;

    // Unless the character knows the type of the wand, the targeting
    // system will default to enemies. -- [ds]
    targ_mode_type targ_mode = TARG_ENEMY;

    beam.obvious_effect = false;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.duration[DUR_BERSERKER])
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

    // if you happen to be wielding the wand, its display might change.
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

        case WAND_HASTING:
        case WAND_HEALING:
        case WAND_INVISIBILITY:
            targ_mode = (wand.sub_type == WAND_HEALING
                            && you.religion == GOD_ELYVILON) ?
                                TARG_ANY : TARG_FRIEND;
            break;

        default:
            targ_mode = TARG_ENEMY;
            break;
        }
    }

    int tracer_range = (alreadyknown && wand.sub_type != WAND_RANDOM_EFFECTS) ?
        _wand_range(type_zapped) : _max_wand_range();
    message_current_target();
    direction(zap_wand, DIR_NONE, targ_mode, tracer_range);

    if (!zap_wand.isValid)
    {
        if (zap_wand.isCancel)
            canned_msg(MSG_OK);
        return;
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
        zap_wand.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);
    }

    if (wand.sub_type == WAND_RANDOM_EFFECTS)
        beam.effect_known = false;

    beam.source = you.pos();
    beam.set_target(zap_wand);

    bool aimed_at_self = (beam.target == you.pos());

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

void drink( int slot )
{
    int item_slot;

    if (you.is_undead == US_UNDEAD)
    {
        mpr("You can't drink.");
        return;
    }

    if (slot == -1)
    {
        if (grd(you.pos()) >= DNGN_FOUNTAIN_BLUE
            && grd(you.pos()) <= DNGN_FOUNTAIN_BLOOD)
        {
            if (_drink_fountain())
                return;
        }
    }

    if (inv_count() == 0)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
    {
       canned_msg(MSG_PRESENT_FORM);
       return;
    }

    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Drink which item?",
                                       MT_INVLIST, OBJ_POTIONS,
                                       true, true, true, 0, -1, NULL,
                                       OPER_QUAFF);
    }

    if (prompt_failed(item_slot))
        return;

    if (you.inv[item_slot].base_type != OBJ_POTIONS)
    {
        mpr("You can't drink that!");
        return;
    }

    const bool alreadyknown = item_type_known(you.inv[item_slot]);

    if (alreadyknown && you.hunger_state == HS_ENGORGED
        && (is_blood_potion(you.inv[item_slot])
            || you.inv[item_slot].sub_type == POT_PORRIDGE))
    {
        mpr("You are much too full right now.");
        return;
    }

    if (alreadyknown && you.inv[item_slot].sub_type == POT_BERSERK_RAGE
        && !berserk_check_wielded_weapon())
    {
        return;
    }

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous = (player_in_a_dangerous_place()
                            && you.experience_level > 1);

    if (potion_effect(static_cast<potion_type>(you.inv[item_slot].sub_type),
                      40, alreadyknown))
    {
        set_ident_flags(you.inv[item_slot], ISFLAG_IDENT_MASK);

        set_ident_type( you.inv[item_slot], ID_KNOWN_TYPE );
    }
    else
    {
        set_ident_type( you.inv[item_slot], ID_TRIED_TYPE );
    }
    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you drink an unknown potion and there is
        // a dangerous monster nearby...
        xom_is_stimulated(255);
    }

    if (is_blood_potion(you.inv[item_slot]))
    {
        // Always drink oldest potion.
        remove_oldest_blood_potion(you.inv[item_slot]);
    }
    dec_inv_item_quantity( item_slot, 1 );
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

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return (true);
    }

    potion_type fountain_effect = POT_WATER;
    if (feat == DNGN_FOUNTAIN_BLUE)
    {
        if (!yesno("Drink from the fountain?"))
            return (false);

        mpr("You drink the pure, clear water.");
    }
    else if (feat == DNGN_FOUNTAIN_BLOOD)
    {
        if (!yesno("Drink from the fountain of blood?"))
            return (false);

        mpr("You drink the blood.");
        fountain_effect = POT_BLOOD;
    }
    else
    {
        if (!yesno("Drink from the sparkling fountain?"))
            return (false);

        mpr("You drink the sparkling water.");

        const potion_type effects[] =
            { POT_WATER, POT_DECAY,
              POT_MUTATION, POT_HEALING, POT_HEAL_WOUNDS, POT_SPEED, POT_MIGHT,
              POT_DEGENERATION,
              POT_LEVITATION, POT_POISON, POT_SLOWING,
              POT_PARALYSIS, POT_CONFUSION, POT_INVISIBILITY,
              POT_MAGIC, POT_RESTORE_ABILITIES, POT_RESISTANCE,
              POT_STRONG_POISON, POT_BERSERK_RAGE,
              POT_GAIN_STRENGTH, POT_GAIN_INTELLIGENCE, POT_GAIN_DEXTERITY };

        const int weights[] = { 467, 48,
                                40, 40, 40, 40, 40,
                                32,
                                27, 27, 27,
                                27, 27, 27,
                                20, 20, 20,
                                20, 20,
                                4, 4, 4 };

        COMPILE_CHECK(ARRAYSZ(weights) == ARRAYSZ(effects), c1);
        fountain_effect =
            effects[choose_random_weighted(weights,
                                           weights + ARRAYSZ(weights))];
    }

    if (fountain_effect != POT_WATER && fountain_effect != POT_BLOOD)
        xom_is_stimulated(64);

    // Good gods do not punish for bad random effects. However, they do
    // punish drinking from a fountain of blood.
    potion_effect(fountain_effect, 100, feat != DNGN_FOUNTAIN_SPARKLING);

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
        return false;

    // Check if you're wielding a brandable weapon.
    item_def& wpn = *you.weapon();
    if (wpn.base_type != OBJ_WEAPONS || wpn.sub_type == WPN_BLOWGUN
        || is_artefact(wpn))
    {
        return false;
    }

    you.wield_change = true;

    // If there's no brand, make it vorpal.
    if (get_weapon_brand(wpn) == SPWPN_NORMAL)
    {
        alert_nearby_monsters();
        mprf("%s emits a brilliant flash of light!",
             wpn.name(DESC_CAP_YOUR).c_str());
        set_item_ego_type(wpn, OBJ_WEAPONS, SPWPN_VORPAL);
        return true;
    }

    // If there's a permanent brand, fail.
    if (you.duration[DUR_WEAPON_BRAND] == 0)
        return false;

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

    case SPWPN_FLAMING:
        mprf("%s is engulfed in an explosion of flames!", itname.c_str());
        immolation(10, IMMOLATION_SPELL, you.pos(), true, &you);
        break;

    case SPWPN_FREEZING:
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
        did_god_conduct(DID_UNHOLY, 10,
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
    // Cannot be enchanted nor uncursed.
    if (!is_enchantable_weapon(wpn, true))
    {
        if (!quiet)
            canned_msg( MSG_NOTHING_HAPPENS );

        return (false);
    }

    const bool is_cursed = item_cursed(wpn);

    // Missiles only have one stat.
    if (wpn.base_type == OBJ_MISSILES)
        which_stat = ENCHANT_TO_HIT;

    int enchant_level = (which_stat == ENCHANT_TO_HIT) ? wpn.plus
                                                       : wpn.plus2;

    // Even if not affected, it may be uncursed.
    if (!is_enchantable_weapon(wpn, false)
        || enchant_level >= 4 && x_chance_in_y(enchant_level, 9))
    {
        if (is_cursed)
        {
            if (!quiet)
            {
                mprf("%s glows silver for a moment.",
                     wpn.name(DESC_CAP_YOUR).c_str());
            }

            do_uncurse_item( wpn );
            return (true);
        }
        else
        {
            if (!quiet)
                canned_msg(MSG_NOTHING_HAPPENS);

            return (false);
        }
    }

    // Get item name now before changing enchantment.
    std::string iname = wpn.name(DESC_CAP_YOUR);

    if (wpn.base_type == OBJ_WEAPONS)
    {
        if (which_stat == ENCHANT_TO_HIT)
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
        do_uncurse_item( wpn );

    xom_is_stimulated(16);
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

    const bool is_cursed = item_cursed(arm);

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
            do_uncurse_item( arm );

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
            do_uncurse_item( arm );
            return (true);
        }
        else
        {
            if (!quiet)
                canned_msg( MSG_NOTHING_HAPPENS );

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
        do_uncurse_item( arm );

    xom_is_stimulated(16);
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
            mesclr(true);
            return;
        }

        const spell_type spell = which_spell_in_book(book,
                                                     letter_to_index(ltr));
        if (spell == SPELL_NO_SPELL)
        {
            mesclr(true);
            return;
        }

        describe_spell(spell);
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
    item_slot = prompt_invent_item("Use on which item?", MT_INVLIST,
                                   OSEL_ANY, true, true, false, 0, item_slot);

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
        if (item_is_rechargeable(item, true))
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
        mpr("Buggy scroll can't modify item!");
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
        ENCH_FEAR,
        ENCH_CONFUSION,
        ENCH_INVIS,
        ENCH_BACKLIGHT,
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
            if (!mons_wont_attack(mon))
                behaviour_event(mon, ME_ANNOY, MHITYOU);
        }
    }

    you.duration[DUR_LOWERED_MR] = 40;
    mpr("Magic dampens around you!");
}

void read_scroll(int slot)
{
    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
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
    if (which_scroll != SCR_PAPER
        && (which_scroll != SCR_IMMOLATION || you.confused()))
    {
        mpr("As you read the scroll, it crumbles to dust.");
        // Actual removal of scroll done afterwards. -- bwr
    }

    const bool alreadyknown = item_type_known(scroll);
    const bool dangerous    = player_in_a_dangerous_place();

    // Scrolls of paper are also exempted from this handling {dlb}:
    if (which_scroll != SCR_PAPER)
    {
        if (you.confused())
        {
            random_uselessness(item_slot);
            dec_inv_item_quantity( item_slot, 1 );
            return;
        }

        if (!you.skills[SK_SPELLCASTING])
            exercise(SK_SPELLCASTING, (coinflip()? 2 : 1));
    }

    // It is the exception, not the rule, that the scroll will not
    // be identified. {dlb}
    bool id_the_scroll = true;  // to prevent unnecessary repetition
    bool tried_on_item = false; // used to modify item (?EA, ?RC, ?ID)

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
        if (!detect_curse(false))
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
                                          0, 0, you.pos(), you.pet_target,
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
        if (you.level_type == LEVEL_PANDEMONIUM)
        {
            if (!item_type_known(scroll))
            {
                mpr("You feel momentarily disoriented.");
                id_the_scroll = false;
            }
            else
                mpr("Your Earth magic cannot map Pandemonium.");
        }
        else
            id_the_scroll = magic_mapping(50, 90 + random2(11), false);
        break;

    case SCR_TORMENT:
        torment(TORMENT_SCROLL, you.pos());

        // This is only naughty if you know you're doing it.
        did_god_conduct(DID_UNHOLY, 10, item_type_known(scroll));
        break;

    case SCR_IMMOLATION:
    {
        mpr("The scroll explodes in your hands!");
        // We do this here to prevent it from blowing itself up.
        set_ident_type(scroll, ID_KNOWN_TYPE);
        dec_inv_item_quantity(item_slot, 1);

        immolation(10, IMMOLATION_SCROLL, you.pos(), alreadyknown, &you);
        break;
    }

    case SCR_CURSE_WEAPON:
        if (!you.weapon()
            || you.weapon()->base_type != OBJ_WEAPONS
            || item_cursed(*you.weapon()))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        else
        {
            // Also sets wield_change.
            do_curse_item( *you.weapon(), false );
            learned_something_new(TUT_YOU_CURSED);
        }
        break;

    // Everything [in the switch] below this line is a nightmare {dlb}:
    case SCR_ENCHANT_WEAPON_I:
        id_the_scroll = _handle_enchant_weapon( ENCHANT_TO_HIT );
        break;

    case SCR_ENCHANT_WEAPON_II:
        id_the_scroll = _handle_enchant_weapon( ENCHANT_TO_DAM );
        break;

    case SCR_ENCHANT_WEAPON_III:
        if (you.weapon())
        {
            item_def& wpn = *you.weapon();
            mprf("%s glows bright yellow for a while.",
                 wpn.name(DESC_CAP_YOUR).c_str() );

            do_uncurse_item(wpn);
            _handle_enchant_weapon( ENCHANT_TO_HIT, true );

            if (coinflip())
                _handle_enchant_weapon( ENCHANT_TO_HIT, true );

            _handle_enchant_weapon( ENCHANT_TO_DAM, true );

            if (coinflip())
                _handle_enchant_weapon( ENCHANT_TO_DAM, true );
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
        for (int i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
        {
            if (you.equip[i] != -1 && !item_cursed(you.inv[you.equip[i]]))
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
        learned_something_new(TUT_YOU_CURSED);
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
    // Scrolls of immolation were already destroyed earlier.
    if (which_scroll != SCR_PAPER && which_scroll != SCR_IMMOLATION)
    {
        if (id_the_scroll)
            set_ident_flags(scroll, ISFLAG_KNOW_TYPE); // for notes

        dec_inv_item_quantity( item_slot, 1 );
    }

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you read an unknown scroll and there is a
        // dangerous monster nearby...
        xom_is_stimulated(255);
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
    mesclr(true);
}                               // end original_name()

void use_randart(unsigned char item_wield_2)
{
    use_randart( you.inv[ item_wield_2 ] );
}

void use_randart(item_def &item, bool unmeld)
{
#define unknown_proprt(prop) (proprt[(prop)] && !known[(prop)])

    ASSERT( is_random_artefact( item ) );

    const bool alreadyknown = item_type_known(item);
    const bool dangerous    = player_in_a_dangerous_place();

    randart_properties_t  proprt;
    randart_known_props_t known;
    randart_wpn_properties( item, proprt, known );

    // Only give property messages for previously unknown properties.
    if (proprt[RAP_AC])
    {
        you.redraw_armour_class = true;
        if (!known[RAP_AC])
        {
            mprf("You feel %s.", proprt[RAP_AC] > 0?
                 "well-protected" : "more vulnerable");
            randart_wpn_learn_prop(item, RAP_AC);
        }
    }

    if (proprt[RAP_EVASION])
    {
        you.redraw_evasion = true;
        if (!known[RAP_EVASION])
        {
            mprf("You feel somewhat %s.", proprt[RAP_EVASION] > 0?
                 "nimbler" : "more awkward");
            randart_wpn_learn_prop(item, RAP_EVASION);
        }
    }

    if (proprt[RAP_MAGICAL_POWER])
    {
        you.redraw_magic_points = true;
        if (!known[RAP_MAGICAL_POWER])
        {
            mprf("You feel your mana capacity %s.",
                 proprt[RAP_MAGICAL_POWER] > 0? "increase" : "decrease");
            randart_wpn_learn_prop(item, RAP_MAGICAL_POWER);
        }
    }

    // Modify ability scores.
    // Output result even when identified (because of potential fatality).
    modify_stat( STAT_STRENGTH,     proprt[RAP_STRENGTH],     false, item );
    modify_stat( STAT_INTELLIGENCE, proprt[RAP_INTELLIGENCE], false, item );
    modify_stat( STAT_DEXTERITY,    proprt[RAP_DEXTERITY],    false, item );

    const randart_prop_type stat_props[3] =
        {RAP_STRENGTH, RAP_INTELLIGENCE, RAP_DEXTERITY};

    for (int i = 0; i < 3; i++)
        if (unknown_proprt(stat_props[i]))
            randart_wpn_learn_prop(item, stat_props[i]);

    // For evokable stuff, check whether other equipped items yield
    // the same ability.  If not, and if the ability granted hasn't
    // already been discovered, give a message.
    if (unknown_proprt(RAP_LEVITATE)
        && !items_give_ability(item.link, RAP_LEVITATE))
    {
        if (player_is_airborne())
            mpr("You feel vaguely more buoyant than before.");
        else
            mpr("You feel buoyant.");
        randart_wpn_learn_prop(item, RAP_LEVITATE);
    }

    if (unknown_proprt(RAP_INVISIBLE) && !you.duration[DUR_INVIS])
    {
        mpr("You become transparent for a moment.");
        randart_wpn_learn_prop(item, RAP_INVISIBLE);
    }

    if (unknown_proprt(RAP_CAN_TELEPORT)
        && !items_give_ability(item.link, RAP_CAN_TELEPORT))
    {
        mpr("You feel slightly jumpy.");
        randart_wpn_learn_prop(item, RAP_CAN_TELEPORT);
    }

    if (unknown_proprt(RAP_BERSERK)
        && !items_give_ability(item.link, RAP_BERSERK))
    {
        mpr("You feel a brief urge to hack something to bits.");
        randart_wpn_learn_prop(item, RAP_BERSERK);
    }

    if (!unmeld && !item_cursed(item) && proprt[RAP_CURSED] > 0
         && one_chance_in(proprt[RAP_CURSED]))
    {
        do_curse_item( item, false );
        randart_wpn_learn_prop(item, RAP_CURSED);
    }

    if (proprt[RAP_NOISES])
        you.special_wield = SPWLD_NOISE;

    if (!alreadyknown && Options.autoinscribe_randarts)
        add_autoinscription(item, randart_auto_inscription(item));

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you use an unknown random artefact and
        // there is a dangerous monster nearby...
        xom_is_stimulated(255);
    }
#undef unknown_proprt
}

bool wearing_slot(int inv_slot)
{
    for (int i = EQ_CLOAK; i <= EQ_AMULET; ++i)
        if (inv_slot == you.equip[i])
            return (true);

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

void tile_item_eat_floor(int idx)
{
    if (mitm[idx].base_type == OBJ_CORPSES
            && you.species == SP_VAMPIRE
        || mitm[idx].base_type == OBJ_FOOD
            && you.is_undead != US_UNDEAD && you.species != SP_VAMPIRE)
    {
        if (can_ingest(mitm[idx].base_type, mitm[idx].sub_type, false))
            eat_floor_item(idx);
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
    else if (item.base_type == OBJ_MISCELLANY
             || item.base_type == OBJ_STAVES
                && item_is_rod(item)) // unwield rods/misc. items
    {
        if (you.equip[EQ_WEAPON] == idx
            && check_warning_inscriptions(item, OPER_WIELD))
        {
            wield_weapon(true, PROMPT_GOT_SPECIAL); // unwield
        }
    }
    else if (you.equip[EQ_WEAPON] == idx
             && check_warning_inscriptions(item, OPER_WIELD))
    {
        wield_weapon(true, PROMPT_GOT_SPECIAL); // unwield
    }
    else if (_valid_weapon_swap(item)
             && check_warning_inscriptions(item, OPER_WIELD))
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
        if (!check_warning_inscriptions(item, OPER_WIELD))
            return;

        wield_weapon(true, PROMPT_GOT_SPECIAL);
        return;
    }

    // Use it
    switch (item.base_type)
    {
        case OBJ_WEAPONS:
        case OBJ_STAVES:
        case OBJ_MISCELLANY:
            // Wield any unwielded item of these types.
            if (!equipped)
            {
                if (check_warning_inscriptions(item, OPER_WIELD))
                    wield_weapon(true, idx);
                return;
            }
            // Evoke misc. items and rods.
            if (item.base_type == OBJ_MISCELLANY || item_is_rod(item))
            {
                if (check_warning_inscriptions(item, OPER_EVOKE))
                    evoke_wielded();
                return;
            }
            // Unwield staves or weapons.
            if (check_warning_inscriptions(item, OPER_WIELD))
                wield_weapon(true, PROMPT_GOT_SPECIAL); // unwield
            return;

        case OBJ_MISSILES:
            if (check_warning_inscriptions(item, OPER_FIRE))
                fire_thing(idx);
            return;

        case OBJ_ARMOUR:
            if (equipped && !equipped_weapon)
            {
                if (check_warning_inscriptions(item, OPER_TAKEOFF))
                    takeoff_armour(idx);
            }
            else if (check_warning_inscriptions(item, OPER_WEAR))
                wear_armour(idx);
            return;

        case OBJ_WANDS:
            if (check_warning_inscriptions(item, OPER_ZAP))
                zap_wand(idx);
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
            if (check_warning_inscriptions(item, OPER_EAT))
                eat_food(idx);
            return;

        case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL
                || item.sub_type == BOOK_DESTRUCTION)
            {
                if (check_warning_inscriptions(item, OPER_READ))
                    handle_read_book(idx);
            } // else it's a spellbook
            else if (check_warning_inscriptions(item, OPER_MEMORISE))
                learn_spell(idx);
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
                puton_ring(idx, false);
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
