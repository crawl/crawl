/*
 *  File:       item_use.cc
 *  Summary:    Functions for making use of inventory items.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <8>     28July2000  GDL    Revised player throwing
 *   <7>     11/23/99    LRH    Horned characters can wear hats/caps
 *   <6>     7/13/99     BWR    Lowered learning rates for
 *                              throwing skills, and other
 *                              balance tweaks
 *   <5>     5/28/99     JDJ    Changed wear_armour to allow Spriggans to
 *                              wear bucklers.
 *   <4>     5/26/99     JDJ    body armour can be removed and worn if an
 *                              uncursed cloak is being worn.
 *                              Removed lots of unnessary mpr string copying.
 *                              Added missing ponderous message.
 *   <3>     5/20/99     BWR    Fixed staff of air bug, output of trial
 *                              identified items, a few you.wield_changes so
 *                              that the weapon gets updated.
 *   <2>     5/08/99     JDJ    Added armour_prompt.
 *   <1>     -/--/--     LRH    Created
 */

#include "AppHdr.h"
#include "item_use.h"

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
#include "direct.h"
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
#include "ouch.h"
#include "player.h"
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
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tiles.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

static bool drink_fountain();
static bool enchant_armour();

// Rather messy - we've gathered all the can't-wield logic from wield_weapon()
// here.
bool can_wield(const item_def *weapon, bool say_reason,
               bool ignore_temporary_disability)
{
#define SAY(x) if (say_reason) { x; } else 

    if (!ignore_temporary_disability && you.duration[DUR_BERSERKER])
    {
        SAY(canned_msg(MSG_TOO_BERSERK));
        return false;
    }
    if (!can_equip( EQ_WEAPON, ignore_temporary_disability ))
    {
        SAY(mpr("You can't wield anything in your present form."));
        return false;
    }

    if (!ignore_temporary_disability
        && you.equip[EQ_WEAPON] != -1
        && you.inv[you.equip[EQ_WEAPON]].base_type == OBJ_WEAPONS
        && item_cursed( you.inv[you.equip[EQ_WEAPON]] ))
    {
        SAY(mpr("You can't unwield your weapon to draw a new one!"));
        return false;
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

    if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
    {
        SAY(mpr("You can't wield that with a shield."));
        return (false);
    }

    // We don't have to check explicitly for staves - all staves are wieldable
    // by everyone.
    if (weapon->base_type != OBJ_WEAPONS)
        return (true);

    if ((you.species < SP_OGRE || you.species > SP_OGRE_MAGE)
            && item_mass( *weapon ) >= 300)
    {
        SAY(mpr("That's too large and heavy for you to wield."));
        return false;
    }

    if ((you.species == SP_HALFLING || you.species == SP_GNOME
            || you.species == SP_KOBOLD || you.species == SP_SPRIGGAN)
            && (weapon->sub_type == WPN_GREAT_SWORD
                || weapon->sub_type == WPN_TRIPLE_SWORD
                || weapon->sub_type == WPN_GREAT_MACE
                || weapon->sub_type == WPN_DIRE_FLAIL
                || weapon->sub_type == WPN_BATTLEAXE
                || weapon->sub_type == WPN_EXECUTIONERS_AXE
                || weapon->sub_type == WPN_BARDICHE
                || weapon->sub_type == WPN_HALBERD
                || weapon->sub_type == WPN_GLAIVE
                || weapon->sub_type == WPN_GIANT_CLUB
                || weapon->sub_type == WPN_GIANT_SPIKED_CLUB
                || weapon->sub_type == WPN_LONGBOW
                || weapon->sub_type == WPN_SCYTHE))
    {
        SAY(mpr("That's too large for you to wield."));
        return false;
    }

    int weap_brand = get_weapon_brand( *weapon );
    if ((you.is_undead || you.species == SP_DEMONSPAWN)
            && (!is_fixed_artefact( *weapon )
                && (weap_brand == SPWPN_HOLY_WRATH 
                    || (weapon->base_type == OBJ_WEAPONS
                         && weapon->sub_type == WPN_BLESSED_BLADE))))
    {
        SAY(mpr("This weapon will not allow you to wield it."));
        return false;
    }

    // We can wield this weapon. Phew!
    return true;

#undef SAY
}

bool wield_weapon(bool auto_wield, int slot, bool show_weff_messages)
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

    if (you.duration[DUR_SURE_BLADE])
    {
        mpr("The bond with your blade fades away.");
        you.duration[DUR_SURE_BLADE] = 0;
    }

    int item_slot = 0;          // default is 'a'

    if (auto_wield)
    {
        if ( item_slot == you.equip[EQ_WEAPON] )
            item_slot = 1;      // backup is 'b'

        if (slot != -1)         // allow external override
            item_slot = slot;
    }

    // If the swap slot has a bad (but valid) item in it,
    // the swap will be to bare hands.
    const bool good_swap = (item_slot == PROMPT_GOT_SPECIAL)
        || you.inv[item_slot].base_type == OBJ_WEAPONS
        || you.inv[item_slot].base_type == OBJ_STAVES
        || (you.inv[item_slot].base_type == OBJ_MISCELLANY
            && you.inv[item_slot].sub_type != MISC_RUNE_OF_ZOT);

    // Reset the warning counter.
    you.received_weapon_warning = false;

    // Prompt if not using the auto swap command, or if the swap slot
    // is empty.
    if (item_slot != PROMPT_GOT_SPECIAL &&
        (!auto_wield || !is_valid_item(you.inv[item_slot]) || !good_swap))
    {
        if (!auto_wield)
            item_slot = prompt_invent_item(
                            "Wield which item (- for none, * to show all)?",
                            MT_INVLIST, OSEL_WIELD, 
                            true, true, true, '-', NULL, OPER_WIELD);
        else
            item_slot = PROMPT_GOT_SPECIAL;
    }

    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return (false);
    }
    else if (item_slot == PROMPT_GOT_SPECIAL)  // '-' or bare hands
    {
        if (you.equip[EQ_WEAPON] != -1)
        {
            if (!unwield_item(show_weff_messages))
                return (false);
                    
            canned_msg( MSG_EMPTY_HANDED );

            you.turn_is_over = true;
            you.time_taken *= 3;
            you.time_taken /= 10;
        }
        else
            mpr( "You are already empty-handed." );

        return (true);
    }

    if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are already wielding that!");
        return (true);
    }

    if (!can_wield(&you.inv[item_slot], true))
        return (false);

    if (!safe_to_remove_or_wear(you.inv[item_slot], false))
        return (false);

    // Go ahead and wield the weapon.
    if (you.equip[EQ_WEAPON] != -1 && !unwield_item(show_weff_messages))
        return (false);

    you.equip[EQ_WEAPON] = item_slot;

    // any oddness on wielding taken care of here
    wield_effects(item_slot, show_weff_messages);

    mpr(you.inv[item_slot].name(DESC_INVENTORY_EQUIP).c_str());

    // warn player about low str/dex or throwing skill
    if (show_weff_messages)
        wield_warning();

    // time calculations
    you.time_taken /= 2;

    you.wield_change = true;
    you.quiver_change = true;
    you.turn_is_over = true;

    return (true);
}

static const char *shield_base_name(const item_def *shield)
{
    return (shield->sub_type == ARM_BUCKLER? "buckler"
                                           : "shield");
}

static const char *shield_impact_degree(int impact)
{
    return (impact > 160? "severely "       :
            impact > 130? "significantly "  :
            impact > 110? ""                :
                          NULL);
}

static void warn_rod_shield_interference(const item_def &)
{
    const int leakage = rod_shield_leakage();
    const char *leak_degree = shield_impact_degree(leakage);

    // Any way to avoid the double entendre? :-)
    if (leak_degree)
        mprf(MSGCH_WARN, 
                "Your %s %sreduces the effectiveness of your rod.",
                shield_base_name(player_shield()),
                leak_degree);
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
            mprf(MSGCH_WARN, 
                    "Your %s %sslows your rate of fire.", 
                    shield_base_name(player_shield()),
                    slow_degree);
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
             && weapon->sub_type == WPN_QUARTERSTAFF)
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

    case SPWPN_GLAIVE_OF_PRUNE:
        return (SPWLD_NONE);

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

// provide a function for handling initial wielding of 'special'
// weapons, or those whose function is annoying to reproduce in
// other places *cough* auto-butchering *cough*    {gdl}

void wield_effects(int item_wield_2, bool showMsgs)
{
    unsigned char i_dam = 0;

    const bool known_cursed = item_known_cursed(you.inv[item_wield_2]);

    // and here we finally get to the special effects of wielding {dlb}
    if (you.inv[item_wield_2].base_type == OBJ_MISCELLANY)
    {
        if (you.inv[item_wield_2].sub_type == MISC_LANTERN_OF_SHADOWS)
        {
            if (showMsgs)
                mpr("The area is filled with flickering shadows.");
            
            you.current_vision -= 2;
            setLOSRadius(you.current_vision);
            you.special_wield = SPWLD_SHADOW;
        }
    }

    if (you.inv[item_wield_2].base_type == OBJ_STAVES)
    {
        if (you.inv[item_wield_2].sub_type == STAFF_POWER)
        {
            // inc_max_mp(13);
            calc_mp();
            set_ident_flags( you.inv[item_wield_2], ISFLAG_EQ_WEAPON_MASK );
            mpr("You feel your mana capacity increase.");
        }
        else
        {
            // Most staves only give curse status when wielded and
            // right now that's always "uncursed". -- bwr
            set_ident_flags( you.inv[item_wield_2], ISFLAG_KNOW_CURSE );
        }
    }

    if (you.inv[item_wield_2].base_type == OBJ_WEAPONS)
    {
        if (is_demonic(you.inv[item_wield_2])
            && (you.religion == GOD_ZIN || you.religion == GOD_SHINING_ONE
                || you.religion == GOD_ELYVILON))
        {
            if (showMsgs)
                mpr("You really shouldn't be using a nasty item like this.");
        }

        // only used for Singing Sword introduction
        const bool was_known = item_type_known(you.inv[item_wield_2]);
        const char *old_desc = you.inv[item_wield_2].name(DESC_CAP_THE).c_str();
        
        set_ident_flags( you.inv[item_wield_2], ISFLAG_EQ_WEAPON_MASK );

        if (is_random_artefact( you.inv[item_wield_2] ))
        {
            i_dam = randart_wpn_property(you.inv[item_wield_2], RAP_BRAND);
            use_randart(item_wield_2);
        }
        else 
        {
            i_dam = you.inv[item_wield_2].special;
        }

        if (i_dam != SPWPN_NORMAL)
        {
            // message first
            if (showMsgs)
            {
                switch (i_dam)
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
                    if (!silenced(you.x_pos, you.y_pos))
                        mpr("You hear the crackle of electricity.",
                            MSGCH_SOUND);
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
                    mpr("It glows red for a moment.");
                    break;

                case SPWPN_FROST:
                    mpr("It is covered in frost.");
                    break;

                case SPWPN_VAMPIRICISM:
                    if (!you.is_undead)
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

                case SPWPN_SINGING_SWORD:
                    if (!was_known)
                        mprf(MSGCH_TALK, "%s says, \"Hi! I'm the Singing Sword!\"", old_desc);
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
                    // mummies cannot smell, and do not hunger {dlb}
                    if (!you.is_undead)
                        mpr("You feel a strange hunger, and smell blood on the air...");
                    else
                        mpr("You feel strangely empty.");
                    break;

                default:
                    break;
                }
            }

            // effect second
            switch (i_dam)
            {
            case SPWPN_PROTECTION:
                you.redraw_armour_class = true;
                break;

            case SPWPN_DISTORTION:
                mpr("Space warps around you for a moment!");
                
                if (!was_known)
                    xom_is_stimulated(32);
                break;

            case SPWPN_SINGING_SWORD:
                you.special_wield = SPWLD_SING;
                break;

            case SPWPN_WRATH_OF_TROG:
                you.special_wield = SPWLD_TROG;
                break;

            case SPWPN_SCYTHE_OF_CURSES:
                you.special_wield = SPWLD_CURSE;
                if (one_chance_in(5))
                    do_curse_item( you.inv[item_wield_2] );
                break;

            case SPWPN_MACE_OF_VARIABILITY:
                you.special_wield = SPWLD_VARIABLE;
                break;

            case SPWPN_GLAIVE_OF_PRUNE:
                you.special_wield = SPWLD_NONE;
                break;

            case SPWPN_SCEPTRE_OF_TORMENT:
                you.special_wield = SPWLD_TORMENT;
                break;

            case SPWPN_SWORD_OF_ZONGULDROK:
                you.special_wield = SPWLD_ZONGULDROK;
                break;

            case SPWPN_SWORD_OF_POWER:
                you.special_wield = SPWLD_POWER;
                break;

            case SPWPN_STAFF_OF_OLGREB:
                // josh declares mummies cannot smell {dlb}
                you.special_wield = SPWLD_OLGREB;
                break;

            case SPWPN_STAFF_OF_WUCAD_MU:
                miscast_effect( SPTYP_DIVINATION, 9, 90, 100, "the Staff of Wucad Mu" );
                you.special_wield = SPWLD_WUCAD_MU;
                break;
            }
        }

        if (item_cursed( you.inv[item_wield_2] ))
        {
            mpr("It sticks to your hand!");
            if (known_cursed)
                xom_is_stimulated(32);
            else
                xom_is_stimulated(64);
        }
    }

    if (showMsgs)
        warn_shield_penalties();
}                               // end wield_weapon()

//---------------------------------------------------------------
//
// armour_prompt
//
// Prompt the user for some armour. Returns true if the user picked
// something legit.
//
//---------------------------------------------------------------
bool armour_prompt( const std::string & mesg, int *index, operation_types oper)
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
                                   true, true, true, 0, NULL,
                                   oper );

        if (slot != PROMPT_ABORT)
        {
            *index = slot;
            succeeded = true;
        }
    }

    return (succeeded);
}                               // end armour_prompt()

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
void wear_armour( int slot )
{
    int armour_wear_2 = 0;

    if (slot != -1)
        armour_wear_2 = slot;
    else if (!armour_prompt("Wear which item?", &armour_wear_2, OPER_WEAR))
        return;

    if (safe_to_remove_or_wear(you.inv[armour_wear_2], false))
        do_wear_armour( armour_wear_2, false );
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

    if (sub_type == ARM_CENTAUR_BARDING)
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
        if (you.mutation[MUT_HOOVES])
        {
            if (verbose)
                mpr("You can't wear boots with hooves!");
            return (false);
        }

        if (you.mutation[MUT_TALONS])
        {
            if (verbose)
                mpr("Boots don't fit your talons!");
           return (false);
        }

        if (you.mutation[MUT_PAWS])
        {
            if (verbose)
                mpr("Boots don't fit your paws!");
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
        // it fits
        return (true);
    }
    else if (you.species == SP_CENTAUR
             && sub_type == ARM_CENTAUR_BARDING
             && (ignore_temporary || !player_is_shapechanged()))
    {
        // it fits
        return (true);
    }
    else if (slot == EQ_HELMET)
    {
        // soft helmets (caps and wizard hats) always fit
        if (!is_hard_helmet( item ))
            return (true);

        if (you.mutation[MUT_HORNS])
        {
            if (verbose)
                mpr("You can't wear that with your horns!");

            return (false);
        }

        if (you.species == SP_KENKU
            && (ignore_temporary || !player_is_shapechanged()))
        {
            if (verbose)
               mpr("That helmet does not fit your head!");

            return (false);
        }
    }
    else if (!can_equip( slot, ignore_temporary ))
    {
        if (verbose)
           mpr("You can't wear that in your present form.");

        return (false);
    }

    // Cannot swim in heavy armour
    if (!ignore_temporary
        && player_is_swimming()
        && slot == EQ_BODY_ARMOUR
        && !is_light_armour( item ))
    {
        if (verbose)
           mpr("You can't swim in that!");

        return (false);
    }

    // Giant races
    if ((you.species >= SP_OGRE && you.species <= SP_OGRE_MAGE)
        || player_genus(GENPC_DRACONIAN))
    {
        if ((sub_type >= ARM_LEATHER_ARMOUR
                && sub_type <= ARM_PLATE_MAIL)
            || (sub_type >= ARM_GLOVES
                && sub_type <= ARM_BUCKLER)
            || sub_type == ARM_CRYSTAL_PLATE_MAIL
            || is_hard_helmet(item))
        {
            if (verbose)
               mpr("This armour doesn't fit on your body.");

            return (false);
        }
    }

    // Tiny races
    if (you.species == SP_SPRIGGAN)
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

bool do_wear_armour( int item, bool quiet )
{
    if (!is_valid_item( you.inv[item] ))
    {
        if (!quiet)
           mpr("You don't have any such object.");

        return (false);
    }

    if (!can_wear_armour(you.inv[item], !quiet, false))
        return (false);
    
    const item_def &invitem = you.inv[item];
    const equipment_type slot = get_armour_slot(invitem);

    if (item == you.equip[EQ_WEAPON])
    {
        if (!quiet)
           mpr("You are wielding that object!");

        return (false);
    }

    if ( wearing_slot(item) )
    {
        if (!quiet)
            mpr("You are already wearing that!");

        return (false);
    }

    // if you're wielding something,
    if (you.equip[EQ_WEAPON] != -1
        // attempting to wear a shield,
        && is_shield(you.inv[item])
        && is_shield_incompatible(
                    you.inv[you.equip[EQ_WEAPON]],
                    &you.inv[item]))
    {
        if (!quiet)
           mpr("You'd need three hands to do that!");

        return (false);
    }
    
    bool removedCloak = false;
    int  cloak = -1;

    if (slot == EQ_BODY_ARMOUR
        && you.equip[EQ_CLOAK] != -1 && !cloak_is_being_removed())
    {
        if (you.equip[EQ_BODY_ARMOUR] != -1 &&
            item_cursed(you.inv[you.equip[EQ_BODY_ARMOUR]]) )
        {
            if ( !quiet )
                mprf("%s is stuck to your body!",
                     you.inv[you.equip[EQ_BODY_ARMOUR]].name(DESC_CAP_YOUR)
                                                       .c_str());
            return (false);
        }
        if (!item_cursed( you.inv[you.equip[EQ_CLOAK]] ))
        {
            cloak = you.equip[ EQ_CLOAK ];
            if (!takeoff_armour(you.equip[EQ_CLOAK]))
                return (false);

            removedCloak = true;
        }
        else
        {
            if (!quiet)
               mpr("Your cloak prevents you from wearing the armour.");

            return (false);
        }
    }

    if (slot == EQ_CLOAK && you.equip[EQ_CLOAK] != -1)
    {
        if (!takeoff_armour(you.equip[EQ_CLOAK]))
            return (false);
    }

    if (slot == EQ_HELMET && you.equip[EQ_HELMET] != -1)
    {
        if (!takeoff_armour(you.equip[EQ_HELMET]))
            return (false);
    }

    if (slot == EQ_GLOVES && you.equip[EQ_GLOVES] != -1)
    {
        if (!takeoff_armour(you.equip[EQ_GLOVES]))
            return (false);
    }

    if (slot == EQ_BOOTS && you.equip[EQ_BOOTS] != -1)
    {
        if (!takeoff_armour(you.equip[EQ_BOOTS]))
            return (false);
    }

    if (slot == EQ_SHIELD && you.equip[EQ_SHIELD] != -1)
    {
        if (!takeoff_armour(you.equip[EQ_SHIELD]))
            return (false);
    }

    if (slot == EQ_BODY_ARMOUR && you.equip[EQ_BODY_ARMOUR] != -1)
    {
        if (!takeoff_armour(you.equip[EQ_BODY_ARMOUR]))
            return (false);
    }

    if (!safe_to_remove_or_wear(you.inv[item], false))
        return (false);

    you.turn_is_over = true;

    int delay = armour_equip_delay( you.inv[item] );
    if (delay)
        start_delay( DELAY_ARMOUR_ON, delay, item );

    if (removedCloak)
        start_delay( DELAY_ARMOUR_ON, 1, cloak );

    return (true);
}                               // do_end wear_armour()

bool takeoff_armour(int item)
{
    if (you.inv[item].base_type != OBJ_ARMOUR)
    {
        mpr("You aren't wearing that!");
        return false;
    }

    if (item_cursed( you.inv[item] ))
    {
        for (int loopy = EQ_CLOAK; loopy <= EQ_BODY_ARMOUR; loopy++)
        {
            if (item == you.equip[loopy])
            {
                mprf("%s is stuck to your body!",
                     you.inv[item].name(DESC_CAP_YOUR).c_str());
                return false;
            }
        }
    }

    if (!safe_to_remove_or_wear(you.inv[item], true))
        return (false);

    bool removedCloak = false;
    int cloak = -1;
    const equipment_type slot = get_armour_slot(you.inv[item]);

    if (slot == EQ_BODY_ARMOUR)
    {
        if (you.equip[EQ_CLOAK] != -1 && !cloak_is_being_removed())
        {
            if (!item_cursed( you.inv[you.equip[EQ_CLOAK]] ))
            {
                cloak = you.equip[ EQ_CLOAK ];
                if (!takeoff_armour(you.equip[EQ_CLOAK]))
                    return (false);

                removedCloak = true;
            }
            else
            {
                mpr("Your cloak prevents you from removing the armour.");
                return false;
            }
        }

        if (item != you.equip[EQ_BODY_ARMOUR])
        {
            mpr("You aren't wearing that!");
            return false;
        }

        // you.equip[EQ_BODY_ARMOUR] = -1;
    }
    else
    {
        switch (slot)
        {
        case EQ_SHIELD:
            if (item != you.equip[EQ_SHIELD])
            {
                mpr("You aren't wearing that!");
                return false;
            }
            break;

        case EQ_CLOAK:
            if (item != you.equip[EQ_CLOAK])
            {
                mpr("You aren't wearing that!");
                return false;
            }
            break;

        case EQ_HELMET:
            if (item != you.equip[EQ_HELMET])
            {
                mpr("You aren't wearing that!");
                return false;
            }
            break;

        case EQ_GLOVES:
            if (item != you.equip[EQ_GLOVES])
            {
                mpr("You aren't wearing that!");
                return false;
            }
            break;

        case EQ_BOOTS:
            if (item != you.equip[EQ_BOOTS])
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

    int delay = armour_equip_delay( you.inv[item] );
    start_delay( DELAY_ARMOUR_OFF, delay, item );

    if (removedCloak)
        start_delay( DELAY_ARMOUR_ON, 1, cloak );

    return true;
}                               // end takeoff_armour()

void throw_anything( int slot )
{
    struct bolt beam;
    int throw_slot;

    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }
    else if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (slot != -1)
        throw_slot = slot;
    else
        throw_slot = prompt_invent_item( "Throw which item? (* to show all)",
                                     MT_INVLIST,
                                     OBJ_MISSILES, true, true, true, 0, NULL,
                                     OPER_THROW );
    if (throw_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (throw_slot == you.equip[EQ_WEAPON]
             && (item_cursed( you.inv[you.equip[EQ_WEAPON]] )))
    {
        mpr("That thing is stuck to your hand!");
        return;
    }
    else
    {
        if ( wearing_slot(throw_slot) )
        {
            mpr("You are wearing that object!");
            return;
        }
    }

    throw_it(beam, throw_slot);
}                               // end throw_anything()

static bool fire_item_matches(const item_def &item, unsigned fire_type)
{
    if (!is_valid_item(item))
        return (false);

    if (you.attribute[ATTR_HELD])
    {
        if (item.base_type == OBJ_MISSILES)
        {
            const item_def *weapon = you.weapon();
            if (weapon && weapon->sub_type == WPN_BLOWGUN
                && item.launched_by(*weapon))
            {
                return (true);
            }
        }
        return (false);
    }

    if (item.base_type == OBJ_MISSILES)
    {
        if ((fire_type & FIRE_DART) && item.sub_type == MI_DART)
            return (true);
        if ((fire_type & FIRE_STONE) && item.sub_type == MI_STONE)
            return (true);
        if ((fire_type & FIRE_JAVELIN) && item.sub_type == MI_JAVELIN)
            return (true);
        if ((fire_type & FIRE_ROCK) && item.sub_type == MI_LARGE_ROCK)
            return (true);
        if ((fire_type & FIRE_NET) && item.sub_type == MI_THROWING_NET)
            return (true);

        if (fire_type & FIRE_LAUNCHER)
        {
            const item_def *weapon = you.weapon();
            if (weapon && item.launched_by(*weapon))
                return (true);
        }
    }
    else if (item.base_type == OBJ_WEAPONS
             && is_throwable(item, you.body_size()))
    {
        if ((fire_type & FIRE_RETURNING) && item.special == SPWPN_RETURNING
             && item_ident(item, ISFLAG_KNOW_TYPE))
        {
            return (true);
        }
        if ((fire_type & FIRE_DAGGER) && item.sub_type == WPN_DAGGER)
            return (true);
        if ((fire_type & FIRE_SPEAR) && item.sub_type == WPN_SPEAR)
            return (true);
        if ((fire_type & FIRE_HAND_AXE) && item.sub_type == WPN_HAND_AXE)
            return (true);
        if ((fire_type & FIRE_CLUB) && item.sub_type == WPN_CLUB)
            return (true);
    }
    return (false);
}

static bool fire_item_okay(const item_def &item, unsigned flags)
{
    return (fire_item_matches(item, flags)
            && you.equip[EQ_WEAPON] != item.link);
}

quiver_type get_quiver_type()
{
    const int wielded = you.equip[EQ_WEAPON];
    if (wielded == -1)
        return (QUIVER_THROW);

    item_def &weapon = you.inv[wielded];

    if (weapon.base_type != OBJ_WEAPONS)
        return (QUIVER_THROW);

    switch (weapon.sub_type)
    {
        case WPN_BLOWGUN:
            return (QUIVER_BLOWGUN);
        case WPN_SLING:
            return (QUIVER_SLING);
        case WPN_BOW:
        case WPN_LONGBOW:
            return (QUIVER_BOW);
        case WPN_CROSSBOW:
            return (QUIVER_CROSSBOW);
        case WPN_HAND_CROSSBOW:
            return (QUIVER_HAND_CROSSBOW);
        default:
            return (QUIVER_THROW);
    }

}

static int find_fire_item_matching(unsigned fire_type, int start,
                                   bool forward, bool check_quiver)
{
    if (check_quiver)
    {
        const int q = you.quiver[get_quiver_type()];
        if (q >= 0 && q < ENDOFPACK && fire_item_okay(you.inv[q], fire_type))
            return (q);
    }

    const int dir = forward? 1 : -1;
    int end = forward? ENDOFPACK : -1;

    for (int i = start; i != end; )
    {
        if (i >= Options.fire_items_start
            && fire_item_okay(you.inv[i], fire_type))
        {
            return (i);
        }

        i += dir;
        if (i == -1)
        {
            i   += ENDOFPACK;
            end = start;
        }
        else if (i == ENDOFPACK)
        {
            i   = 0;
            end = start;
        }
    }
    return (ENDOFPACK);
}

// Note: This is a simple implementation, not an efficient one. -- bwr
//
// Returns item index or ENDOFPACK if no item found for auto-firing
int get_fire_item_index(int start_from, bool forward, bool check_quiver)
{
    int item = ENDOFPACK;

    for (unsigned i = 0; i < Options.fire_order.size(); i++)
    {
        const unsigned fire_flags = Options.fire_order[i];
        if ((item =
             find_fire_item_matching(fire_flags,
                                     start_from,
                                     forward,
                                     check_quiver)) != ENDOFPACK)
            break;
    }

    // either item was found or is still ENDOFPACK for no item
    return (item);
}

static void announce_ammo(int item)
{
    mesclr();
    mprf("Firing%s: %s",
         get_fire_item_index((item + 1) % ENDOFPACK, true, false) != item?
         " (^N/^P - change)" : "",         
         you.inv[item].name(DESC_INVENTORY_EQUIP).c_str());
}

class fire_target_behaviour : public targeting_behaviour
{
public:
    fire_target_behaviour(int it) : item(it), need_prompt(false) { }
    command_type get_command(int key = -1);
    bool should_redraw();
    void announce_new_ammo(bool redraw = true);

public:
    int item;
    bool need_prompt;

private:
    void find_next_ammo();
    void find_prev_ammo();
};

bool fire_target_behaviour::should_redraw()
{
    if (need_prompt)
    {
        need_prompt = false;
        return (true);
    }
    return (false);
}

command_type fire_target_behaviour::get_command(int key)
{
    if (key == -1)
        key = get_key();

    switch (key)
    {
    case CONTROL('N'):
        find_next_ammo();
        break;
    case CONTROL('P'):
        find_prev_ammo();
        break;
    case '?':
        show_targeting_help();
        redraw_screen();
        announce_new_ammo(item);
        need_prompt = true;
        return (CMD_NO_CMD);
    }
    
    return targeting_behaviour::get_command(key);
}

void fire_target_behaviour::announce_new_ammo(bool redraw)
{
    announce_ammo(item);
    need_prompt = redraw;
}

void fire_target_behaviour::find_next_ammo()
{
    const int start = item == ENDOFPACK - 1? 0 : item + 1;
    const int next = get_fire_item_index(start, true, false);

    // We should never get back ENDOFPACK.
    if (next != item)
    {
        item = next;
        announce_new_ammo();
    }
}

void fire_target_behaviour::find_prev_ammo()
{
    const int start = item == 0? ENDOFPACK - 1 : item - 1;
    const int next = get_fire_item_index(start, false, false);

    // We should never get back ENDOFPACK.
    if (next != item)
    {
        item = next;
        announce_new_ammo();
    }
}

static bool choose_fire_target(dist &target, int &item)
{
    announce_ammo(item);
    message_current_target();

    fire_target_behaviour beh(item);
    direction( target, DIR_NONE, TARG_ENEMY, false, true, NULL, &beh );
    
    if (!target.isValid)
    {
        if (target.isCancel)
            canned_msg(MSG_OK);
        return (false);
    }

    // Okay, valid target; if the user chose different ammo, quiver it.
    if (beh.item != item)
    {
        item = beh.item;
        if (you.inv[beh.item].quantity > 1)
        {
            you.quiver[get_quiver_type()] = beh.item;
            you.quiver_change = true;
        }
    }

    return (true);
}

void shoot_thing(void)
{
    if (you.duration[DUR_BERSERKER])
    {
        canned_msg(MSG_TOO_BERSERK);
        flush_input_buffer( FLUSH_ON_FAILURE );        
        return;
    }

    int item = get_fire_item_index();

    if (item == ENDOFPACK)
    {
        unwind_var<int> festart(Options.fire_items_start, 0);
        if ((item = get_fire_item_index()) == ENDOFPACK)
            mpr("No suitable missiles.");
        else
            mprf("No suitable missiles (fire_items_start = '%c', "
                 "ignoring item on '%c').",
                 index_to_letter(festart.original_value()),
                 index_to_letter(item));
        flush_input_buffer( FLUSH_ON_FAILURE );
        return;
    }

    dist target;
    bolt beam;
    if (choose_fire_target(target, item)
        && check_warning_inscriptions(you.inv[item], OPER_FIRE))
    {
        throw_it( beam, item, false, 0, &target );
    }
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
    
    // do the same when trying to shoot while held in a net
    if (you.attribute[ATTR_HELD]) // only for blowguns
    {
        int speed_adjust = 105; // analogous to buckler and one-handed weapon
        speed_adjust -= ((speed_adjust - 100) * 5 / 10)
                            * you.skills[SK_THROWING] / 27;

        // also reduce the speed cap.
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
bool elemental_missile_beam(int launcher_brand, int ammo_brand)
{
    int element = (launcher_brand == SPWPN_FROST)
                + (ammo_brand == SPMSL_ICE)
                - (launcher_brand == SPWPN_FLAME)
                - (ammo_brand == SPMSL_FLAME);
    return (element);
}

// XXX This is a bit too generous, as it lets the player determine
// that the bolt of fire he just shot from a flaming bow is actually
// a poison arrow. Hopefully this isn't too abusable.
static bool determines_ammo_brand(int bow_brand, int ammo_brand)   
{
    if (bow_brand == SPWPN_FLAME && ammo_brand == SPMSL_FLAME)
        return false;
    if (bow_brand == SPWPN_FROST && ammo_brand == SPMSL_ICE)
        return false;
    if (bow_brand == SPWPN_VENOM && ammo_brand == SPMSL_POISONED)
        return false;

    return true;
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
    int item = NON_ITEM;
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            item = igrd[x][y];
            while (item != NON_ITEM)
            {
                item_def &i(mitm[item]);
                item = i.link;

                if ((i.flags & ISFLAG_THROWN) && items_stack(i, mitem))
                    i.flags |= idflags;
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

    // launcher weapon sub-type
    weapon_type lnchType;

    int baseHit = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;     // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;     // special add from launcher
    int exHitBonus = 0, exDamBonus = 0; // 'extra' bonus from skill/dex/str
    int effSkill = 0;           // effective launcher skill
    int dice_mult = 100;
    bool returning = false;     // item can return to pack
    bool did_return = false;    // returning item actually does return to pack
    int slayDam = 0;

    if (!teleport)
    {
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
    }

    // Must unwield before fire_beam() makes a copy in order to remove things
    // like temporary branding. -- bwr
    if (throw_2 == you.equip[EQ_WEAPON] && you.inv[throw_2].quantity == 1)
    {
        unwield_item();
        canned_msg( MSG_EMPTY_HANDED );
    }

    // Making a copy of the item: changed only for venom launchers
    item_def item = you.inv[throw_2];
    item.quantity = 1;
    item.slot     = index_to_letter(item.link);
    origin_set_unknown(item);

    if (you.duration[DUR_CONF])
    {
        thr.isTarget = true;
        thr.tx = you.x_pos + random2(13) - 6;
        thr.ty = you.y_pos + random2(13) - 6;
    }

    // even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    if ( !teleport )
        pbolt.set_target(thr);

    pbolt.flavour = BEAM_MISSILE;
    // pbolt.range is set below

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

    pbolt.type = dchar_glyph(zapsym);

    pbolt.source_x = you.x_pos;
    pbolt.source_y = you.y_pos;
    pbolt.colour = item.colour;

    pbolt.name = item.name(DESC_PLAIN, false, false, false);

    pbolt.thrower = KILL_YOU_MISSILE;
    pbolt.aux_source.clear();

    // get the ammo/weapon type.  Convenience.
    const object_class_type wepClass = item.base_type;
    const int wepType = item.sub_type;

    // get the launcher class,type.  Convenience.
    if (you.equip[EQ_WEAPON] < 0)
        lnchType = NUM_WEAPONS;
    else
        lnchType =
            static_cast<weapon_type>( you.inv[you.equip[EQ_WEAPON]].sub_type );

    // baseHit and damage for generic objects
    baseHit = you.strength - item_mass(item) / 10;
    if (baseHit > 0)
        baseHit = 0;

    baseDam = item_mass(item) / 100;

    // special: might be throwing generic weapon;
    // use base wep. damage, w/ penalty
    if (wepClass == OBJ_WEAPONS)
    {
        baseDam = property( item, PWPN_DAMAGE ) - 4;
        if (baseDam < 0)
            baseDam = 0;
    }

    // figure out if we're thrown or launched
    const launch_retval projected = is_launched(&you, you.weapon(), item);
    
    // extract launcher bonuses due to magic
    if (projected == LRET_LAUNCHED)
    {
        lnchHitBonus = you.inv[you.equip[EQ_WEAPON]].plus;
        lnchDamBonus = you.inv[you.equip[EQ_WEAPON]].plus2;
    }

    // extract weapon/ammo bonuses due to magic
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    // CALCULATIONS FOR LAUNCHED WEAPONS
    if (projected == LRET_LAUNCHED)
    {
        const item_def &launcher = you.inv[you.equip[EQ_WEAPON]];
        const int bow_brand = get_weapon_brand( launcher );
        const int ammo_brand = get_ammo_brand( item );
        bool poisoned = (ammo_brand == SPMSL_POISONED);

        const int item_base_dam = property( item, PWPN_DAMAGE );
        const int lnch_base_dam = property( launcher, PWPN_DAMAGE );

        const skill_type launcher_skill = range_skill( launcher );

        baseHit = property( launcher, PWPN_HIT );
        baseDam = lnch_base_dam + random2(1 + item_base_dam);

        // Slings are terribly weakened otherwise
        if (lnch_base_dam == 0)
            baseDam = item_base_dam;

        // If we've a zero base damage + an elemental brand, up the damage
        // slightly so the brand has something to work with. This should
        // only apply to needles.
        if (!baseDam && elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to consider
        // this beam "needle-like".
        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
            pbolt.ench_power = AUTOMATIC_HIT;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
                "Base hit == %d; Base damage == %d "
                "(item %d + launcher %d)", 
                        baseHit, baseDam,
                        item_base_dam, lnch_base_dam);
#endif

        // fix ammo damage bonus, since missiles only use inv_plus
        ammoDamBonus = ammoHitBonus;

        // check for matches; dwarven, elven, orcish
        if (!get_equip_race(you.inv[you.equip[EQ_WEAPON]]) == 0)
        {
            if (get_equip_race( you.inv[you.equip[EQ_WEAPON]] ) 
                        == get_equip_race( item ))
            {
                baseHit += 1;
                baseDam += 1;

                // elves with elven bows
                if (get_equip_race(you.inv[you.equip[EQ_WEAPON]]) 
                        == ISFLAG_ELVEN
                    && player_genus(GENPC_ELVEN))
                {
                    baseHit += 1;
                }
            }
        }
        
        // lower accuracy if held in a net (needs testing)
        if (you.attribute[ATTR_HELD])
        {
            baseHit--;
        }

        // for all launched weapons, maximum effective specific skill
        // is twice throwing skill.  This models the fact that no matter
        // how 'good' you are with a bow, if you know nothing about
        // trajectories you're going to be a damn poor bowman.  Ditto
        // for crossbows and slings.

        // [dshaligram] Throwing now two parts launcher skill, one part
        // ranged combat. Removed the old model which is... silly.
        
        // [jpeg] Throwing now only affects actual throwing weapons,
        // i.e. not launched ones. (Sep 10, 2007)
        
        shoot_skill = you.skills[launcher_skill];
        effSkill = shoot_skill;

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
        exDamBonus = exDamBonus > 0? random2(exDamBonus + 1)
                                   : -random2(-exDamBonus + 1);
        exHitBonus = lnchHitBonus > 0? random2(lnchHitBonus + 1)
                                     : -random2(-lnchHitBonus + 1);

        // Identify ammo type if the information is there. Note
        // that the bow is always type-identified because it's
        // wielded.
        if (determines_ammo_brand(bow_brand, ammo_brand))
        {
            set_ident_flags(item, ISFLAG_KNOW_TYPE);
            if (ammo_brand != SPMSL_NORMAL)
                set_ident_flags(you.inv[throw_2], ISFLAG_KNOW_TYPE);
        }

        // removed 2 random2(2)s from each of the learning curves, but
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

            // strength is good if you're using a nice sling.
            int strbonus = (10 * (you.strength - 10)) / 9;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // cap
            if (strbonus > lnchDamBonus + 1)
                strbonus = lnchDamBonus + 1;

            exDamBonus += strbonus;
            // add skill for slings.. helps to find those vulnerable spots
            dice_mult = dice_mult * (14 + random2(1 + effSkill)) / 14;

            // now kill the launcher damage bonus
            if (lnchDamBonus > 0)
                lnchDamBonus = 0;
            break;
        }
            // blowguns take a _very_ steady hand;  a lot of the bonus
            // comes from dexterity.  (Dex bonus here as well as below)
        case SK_DARTS:
            baseHit -= 2;
            exercise(SK_DARTS, (coinflip()? 2 : 1));
            exHitBonus += (effSkill * 3) / 2 + you.dex / 2;

            // no extra damage for blowguns
            // exDamBonus = 0;

            // now kill the launcher damage and ammo bonuses
            if (lnchDamBonus > 0)
                lnchDamBonus = 0;
            if (ammoDamBonus > 0)
                ammoDamBonus = 0;
            break;

        case SK_BOWS:
        {
            baseHit -= 3;
            exercise(SK_BOWS, (coinflip()? 2 : 1));
            exHitBonus += (effSkill * 2);

            // strength is good if you're using a nice bow
            int strbonus = (10 * (you.strength - 10)) / 4;
            strbonus = (strbonus * (2 * baseDam + ammoDamBonus)) / 20;

            // cap; reduced this cap, because we don't want to allow
            // the extremely-strong to quadruple the enchantment bonus.
            if (strbonus > lnchDamBonus + 1)
                strbonus = lnchDamBonus + 1;

            exDamBonus += strbonus;

            // add in skill for bows.. help you to find those vulnerable spots.
            // exDamBonus += effSkill;
            
            dice_mult = dice_mult * (17 + random2(1 + effSkill)) / 17;

            // now kill the launcher damage bonus
            if (lnchDamBonus > 0)
                lnchDamBonus = 0;
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

        // Slings and Darts train Throwing a bit
        if (launcher_skill == SK_SLINGS || launcher_skill == SK_DARTS)
        {
            if (coinflip())
                exercise(SK_THROWING, 1);

            // they also get a minor tohit boost from throwing skill.
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

        // special cases for flame, frost, poison, etc.
        // check for venom brand (usually only available for blowguns)
        if (bow_brand == SPWPN_VENOM && ammo_brand == SPMSL_NORMAL)
        {
            // poison brand the ammo
            set_item_ego_type( item, OBJ_MISSILES, SPMSL_POISONED );
            pbolt.name = item.name(DESC_PLAIN);
        }

        // Note that bow_brand is known since the bow is equipped.
        if ((bow_brand == SPWPN_FLAME || ammo_brand == SPMSL_FLAME)
            && ammo_brand != SPMSL_ICE && bow_brand != SPWPN_FROST)
        {
            // [dshaligram] Branded arrows are much stronger.
            dice_mult = (dice_mult * 150) / 100;

            pbolt.flavour = BEAM_FIRE;
            pbolt.name = "bolt of ";

            if (poisoned)
                pbolt.name += "poison ";

            pbolt.name += "flame";
            pbolt.colour = RED;
            pbolt.type = dchar_glyph(DCHAR_FIRED_BOLT);
            pbolt.thrower = KILL_YOU_MISSILE;
            pbolt.aux_source.clear();

        }

        if ((bow_brand == SPWPN_FROST || ammo_brand == SPMSL_ICE)
            && ammo_brand != SPMSL_FLAME && bow_brand != SPWPN_FLAME)
        {
            // [dshaligram] Branded arrows are much stronger.
            dice_mult = (dice_mult * 150) / 100;

            pbolt.flavour = BEAM_COLD;
            pbolt.name = "bolt of ";

            if (poisoned)
                pbolt.name += "poison ";

            pbolt.name += "frost";
            pbolt.colour = WHITE;
            pbolt.type = dchar_glyph(DCHAR_FIRED_BOLT);
            pbolt.thrower = KILL_YOU_MISSILE;
            pbolt.aux_source.clear();
        }

        /* the chief advantage here is the extra damage this does
         * against susceptible creatures */

        /* Note: weapons & ammo of eg fire are not cumulative
         * ammo of fire and weapons of frost don't work together,
         * and vice versa */

        // ID check. Can't ID off teleported projectiles, uh, because
        // it's too weird. Also it messes up the messages.
        if (item_ident(you.inv[you.equip[EQ_WEAPON]], ISFLAG_KNOW_PLUSES))
        {
            if ( !teleport &&
                 !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES) &&
                 random2(100) < shoot_skill )
            {
                set_ident_flags( item, ISFLAG_KNOW_PLUSES );
                set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_PLUSES );
                identify_floor_missiles_matching(item, ISFLAG_KNOW_PLUSES);
                mprf("You are firing %s.",
                     you.inv[throw_2].name(DESC_NOCAP_A).c_str());
            }
        }
        else if (!teleport && random2(100) < shoot_skill)
        {
            item_def& weapon = you.inv[you.equip[EQ_WEAPON]];
            set_ident_flags(weapon, ISFLAG_KNOW_PLUSES);

            mprf("You are wielding %s.", weapon.name(DESC_NOCAP_A).c_str());

            more();
            you.wield_change = true;
        }
    }

    // CALCULATIONS FOR THROWN WEAPONS
    if (projected == LRET_THROWN)
    {
        returning = ((get_weapon_brand(item) == SPWPN_RETURNING ||
                      get_ammo_brand(item) == SPMSL_RETURNING) && !teleport);
                      
        if (returning && !one_chance_in(1 + skill_bump(SK_THROWING)))
            did_return = true;
            
        baseHit = 0;

        // missiles only use inv_plus
        if (wepClass == OBJ_MISSILES)
            ammoDamBonus = ammoHitBonus;

        // all weapons that use 'throwing' go here..
        if (wepClass == OBJ_WEAPONS
            || (wepClass == OBJ_MISSILES
                && (wepType == MI_STONE || wepType == MI_LARGE_ROCK
                    || wepType == MI_JAVELIN)))
        {
            // elves with elven weapons
            if (get_equip_race(item) == ISFLAG_ELVEN
                && player_genus(GENPC_ELVEN))
                baseHit += 1;

            // give an appropriate 'tohit' -
            // hand axes and clubs are -5
            // daggers are +1
            // spears are -1
            // rocks are 0
            if (wepClass == OBJ_WEAPONS)
            {
                switch (wepType)
                {
                    case WPN_DAGGER:
                        baseHit += 1;
                        break;
                    case WPN_SPEAR:
                        baseHit -= 1;
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
                    case MI_JAVELIN:
                        baseHit += 1;
                        break;
                    default:
                        break;
                }
            }

            exHitBonus = you.skills[SK_THROWING] * 2;

            baseDam = property( item, PWPN_DAMAGE );

            // dwarves/orcs with dwarven/orcish weapons
            if ( (get_equip_race(item) == ISFLAG_DWARVEN
                  && player_genus(GENPC_DWARVEN)) ||
                 (get_equip_race(item) == ISFLAG_ORCISH
                  && you.species == SP_HILL_ORC))
            {
                baseDam += 1;
            }

            exDamBonus =
                (10 * (you.skills[SK_THROWING] / 2 + you.strength - 10)) / 12;

            // now, exDamBonus is a multiplier.  The full multiplier
            // is applied to base damage, but only a third is applied
            // to the magical modifier.
            exDamBonus = (exDamBonus * (3 * baseDam + ammoDamBonus)) / 30;
        }

        if (wepClass == OBJ_MISSILES)
        {
            switch (wepType)
            {
            case MI_LARGE_ROCK:
                if (you.can_throw_rocks())
                    baseHit = 1;
                break;
            case MI_DART:
                // give an appropriate 'tohit' & damage
                baseHit = 2;
                baseDam = property( item, PWPN_DAMAGE );

                exHitBonus = you.skills[SK_DARTS] * 2;
                exHitBonus += (you.skills[SK_THROWING] * 2) / 3;
                exDamBonus = you.skills[SK_DARTS] / 3;
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
                // Nets use throwing skill
                // They don't do any damage!
                baseDam = 0;
                exDamBonus = 0;
                ammoDamBonus = 0;

                // but accuracy is important for this one
                baseHit = 1;
                exHitBonus += (skill_bump(SK_THROWING) * 7 / 2);
                // Adjust for strength and dex.
                exHitBonus = dex_adjust_thrown_tohit(exHitBonus);

                // Nets train throwing
                exercise(SK_THROWING, 1);
                break;
            }
        }

        // [dshaligram] The defined base damage applies only when used
        // for launchers. Hand-thrown stones and darts do only half
        // base damage. Yet another evil 4.0ism.
        if (wepClass == OBJ_MISSILES
                && (wepType == MI_DART || wepType == MI_STONE))
            baseDam = div_rand_round(baseDam, 2);
        
        // exercise skill
        if (coinflip())
            exercise(SK_THROWING, 1);
        
        // ID check
        if ( !teleport &&
             !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES) &&
             random2(100) < you.skills[SK_THROWING] )
        {                
            set_ident_flags( item, ISFLAG_KNOW_PLUSES );
            set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_PLUSES );
            identify_floor_missiles_matching(item, ISFLAG_KNOW_PLUSES);
            mprf("You are throwing %s.",
                 you.inv[throw_2].name(DESC_NOCAP_A).c_str());
        }
    }

    // range, dexterity bonus, possible skill increase for silly throwing
    if (projected)
    {
        if (wepType == MI_LARGE_ROCK)
        {
            pbolt.range = 1 + random2( you.strength / 5 );
            if (you.can_throw_rocks())
                pbolt.range += random_range(4, 7);
            if (pbolt.range > 12)
                pbolt.range = 12;

            pbolt.rangeMax = pbolt.range;
        }
        else if (wepType == MI_THROWING_NET)
        {
            pbolt.rangeMax = pbolt.range = 2 + player_size(PSIZE_BODY);
        }
        else
        {
            pbolt.range = 12;
            pbolt.rangeMax = 12;

            exHitBonus += you.dex / 2;

            // slaying bonuses
            if (projected != LRET_LAUNCHED || wepType != MI_NEEDLE)
            {
                slayDam = slaying_bonus(PWPN_DAMAGE);
                slayDam = slayDam < 0? -random2(1 - slayDam)
                                     : random2(1 + slayDam);
            }

            exHitBonus += slaying_bonus(PWPN_HIT);
        }
    }
    else
    {
        // range based on mass & strength, between 1 and 9
        pbolt.range = you.strength - item_mass(item) / 10 + 3;
        if (pbolt.range < 1)
            pbolt.range = 1;

        if (pbolt.range > 9)
            pbolt.range = 9;

        // set max range equal to range for this
        pbolt.rangeMax = pbolt.range;

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
        pbolt.damage = dice_def( 1, baseDam + random2(exDamBonus + 1) );
    else
        pbolt.damage = dice_def( 1, baseDam - random2(0 - (exDamBonus - 1)) );

    pbolt.damage.size  = dice_mult * pbolt.damage.size / 100;
    pbolt.damage.size += slayDam;

    // only add bonuses if we're throwing something sensible
    if (projected || wepClass == OBJ_WEAPONS)
    {
        pbolt.hit += ammoHitBonus + lnchHitBonus;
        pbolt.damage.size += ammoDamBonus + lnchDamBonus;
    }

    // Add in bonus (only from Portal Projectile for now)
    if (acc_bonus != DEBUG_COOKIE)
        pbolt.hit += acc_bonus;

    scale_dice( pbolt.damage );

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS,
            "H:%d+%d;a%dl%d.  D:%d+%d;a%dl%d -> %d,%dd%d",
              baseHit, exHitBonus, ammoHitBonus, lnchHitBonus,
              baseDam, exDamBonus, ammoDamBonus, lnchDamBonus,
              pbolt.hit, pbolt.damage.num, pbolt.damage.size );
#endif

    // create message
    mprf( "You %s%s %s.",
          projected? "" : "awkwardly ",
          projected == LRET_LAUNCHED ? "shoot" : "throw",
          item.name(DESC_NOCAP_A).c_str() );

    // ensure we're firing a 'missile'-type beam
    pbolt.is_beam = false;
    pbolt.is_tracer = false;

    // mark this item as thrown if it's a missile, so that we'll pick it up
    // when we walk over it.
    if (wepClass == OBJ_MISSILES || wepClass == OBJ_WEAPONS)
        item.flags |= ISFLAG_THROWN;

    bool hit = false;
    if (teleport)
    {
        // Violating encapsulation somewhat...oh well.
        hit = (affect( pbolt, pbolt.target_x, pbolt.target_y ) != 0);
        if (acc_bonus != DEBUG_COOKIE)
            beam_drop_object( pbolt, &item, pbolt.target_x, pbolt.target_y );
    }
    else
    {
        // Dropping item copy, since the launched item might be different.
        fire_beam(pbolt, &item, !did_return);

        // The item can be destroyed before returning.
        if (did_return && thrown_object_destroyed(&item, pbolt.target_x,
                                                  pbolt.target_y, true))
        {
            did_return = false;
        }
    }

    if ( did_return )
    {
        // Fire beam in reverse
        pbolt.setup_retrace();
        viewwindow(true, false);
        fire_beam(pbolt, &item, false);

        msg::stream << item.name(DESC_CAP_THE) << " returns to your pack!"
                    << std::endl;

        // Player saw the item return
        if (!is_artefact(you.inv[throw_2]))
            set_ident_flags(you.inv[throw_2], ISFLAG_KNOW_TYPE);
    }
    else
    {
        if (returning) // should have returned but didn't
            msg::stream << item.name(DESC_CAP_THE)
                        << " fails to return to your pack!" << std::endl;

        dec_inv_item_quantity( throw_2, 1 );
    }

    // throwing and blowguns are silent
    if (projected == LRET_LAUNCHED && lnchType != WPN_BLOWGUN)
        noisy( 6, you.x_pos, you.y_pos );

    // but any monster nearby can see that something has been thrown:
    alert_nearby_monsters();

    you.turn_is_over = true;

    return (hit);
}                               // end throw_it()

bool thrown_object_destroyed( item_def *item, int x, int y, bool returning )
{
    ASSERT( item != NULL );

    int chance = 0;
    bool destroyed = false;
    bool hostile_grid = false;

    if (item->base_type == OBJ_MISSILES)
    {
        // [dshaligram] Removed influence of Throwing on ammo preservation.
        // The effect is nigh impossible to perceive.
        switch (item->sub_type)
        {
        case MI_NEEDLE:
            chance = (get_ammo_brand(*item) == SPMSL_CURARE ? 3 : 6);
            break;
        case MI_SLING_BULLET:
        case MI_STONE:  chance = 4; break;
        case MI_DART:   chance = 3; break;
        case MI_ARROW:  chance = 4; break;
        case MI_BOLT:   chance = 4; break;
        case MI_JAVELIN: chance = 10; break;
        case MI_THROWING_NET: break; // doesn't get destroyed by throwing

        case MI_LARGE_ROCK:
        default:
            chance = 25;
            break;
        }
    }

    destroyed = (chance == 0) ? false : one_chance_in(chance);
    hostile_grid = grid_destroys_items(grd[x][y]);

    // Non-returning items thrown into item-destroying grids are always
    // destroyed.  Returning items are only destroyed if they would have
    // been randomly destroyed anyway.
    if (returning && !destroyed)
        hostile_grid = false;

    if (hostile_grid)
    {
        if (player_can_hear(x, y))
            mprf(MSGCH_SOUND, grid_item_destruction_message(grd[x][y]));

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

    // Randart jewellery shouldn't auto-ID just because the 
    // base type is known. Somehow the player should still 
    // be told, preferably by message. (jpeg)
    const bool artefact     = is_random_artefact( item );
    const bool known_pluses = item_ident( item, ISFLAG_KNOW_PLUSES );
    const bool known_cursed = item_known_cursed( item );
    const bool known_bad    = item_type_known( item )
        && (item_value( item ) <= 2);

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
                mprf("You feel %s.", item.plus > 0?
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
        you.redraw_evasion = 1;
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
        modify_stat(STAT_STRENGTH, item.plus, false, item);
        if (item.plus != 0 && !artefact)
            ident = ID_KNOWN_TYPE;
        learn_pluses = true;
        break;

    case RING_DEXTERITY:
        modify_stat(STAT_DEXTERITY, item.plus, false, item);
        if (item.plus != 0 && !artefact)
            ident = ID_KNOWN_TYPE;
        learn_pluses = true;
        break;

    case RING_INTELLIGENCE:
        modify_stat(STAT_INTELLIGENCE, item.plus, false, item);
        if (item.plus != 0 && !artefact)
            ident = ID_KNOWN_TYPE;
        learn_pluses = true;
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
        if (!scan_randarts( RAP_LEVITATE ))
        {
            mpr("You feel buoyant.");
            if (artefact)
                fake_rap = RAP_LEVITATE;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;

    case RING_TELEPORTATION:
        if (!scan_randarts( RAP_CAN_TELEPORT ))
        {
            mpr("You feel slightly jumpy.");
            if (artefact)
                fake_rap = RAP_CAUSE_TELEPORTATION;
            else
                ident = ID_KNOWN_TYPE;
        }
        break;
        
    case AMU_RAGE:
        if (!scan_randarts( RAP_BERSERK ))
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
    }

    // Artefacts have completely different appearance than base types
    // so we don't allow them to make the base types known
    if (artefact)
    {
        use_randart(item);

        if (learn_pluses && (item.plus != 0 || item.plus2 != 0))
            set_ident_flags( item, ISFLAG_KNOW_PLUSES );

        if (fake_rap != RAP_NUM_PROPERTIES)
            randart_wpn_learn_prop( item, fake_rap );
    }
    else
    {
        set_ident_type( item.base_type, item.sub_type, ident );

        if (ident == ID_KNOWN_TYPE)
            set_ident_flags( item, ISFLAG_EQ_JEWELLERY_MASK );
    }

    if (item_cursed( item ))
    {
        mprf("Oops, that %s feels deathly cold.", 
                jewellery_is_amulet(item)? "amulet" : "ring");
        learned_something_new(TUT_YOU_CURSED);

        if (known_cursed || known_bad)
            xom_is_stimulated(64);
        else
            xom_is_stimulated(128);
    }

    // cursed or not, we know that since we've put the ring on
    set_ident_flags( item, ISFLAG_KNOW_CURSE );

    mpr( item.name(DESC_INVENTORY_EQUIP).c_str() );
}

static int prompt_ring_to_remove(int new_ring)
{
    const item_def &left  = you.inv[ you.equip[EQ_LEFT_RING] ];
    const item_def &right = you.inv[ you.equip[EQ_RIGHT_RING] ];

    if (item_cursed(left) && item_cursed(right))
    {
        mprf("You're already wearing two cursed rings!");
        return (-1);
    }

    mesclr();
    mprf("Wearing %s.", you.inv[new_ring].name(DESC_NOCAP_A).c_str());

    char lslot = index_to_letter(left.link),
         rslot = index_to_letter(right.link);

    mprf(MSGCH_PROMPT,
            "You're wearing two rings. Remove which one? (%c/%c/Esc)",
            lslot, rslot);
    mprf(" %s", left.name(DESC_INVENTORY).c_str() );
    mprf(" %s", right.name(DESC_INVENTORY).c_str() );

    int c;
    do
        c = getch();
    while (c != lslot && c != rslot && c != ESCAPE && c != ' ');

    mesclr();

    if (c == ESCAPE || c == ' ')
        return (-1);

    const int eqslot = c == lslot? EQ_LEFT_RING : EQ_RIGHT_RING;

    if (!check_warning_inscriptions(you.inv[you.equip[eqslot]], OPER_REMOVE))
        return -1;

    return (you.equip[eqslot]);
}

// Checks whether a to-be-worn or to-be-removed item affects
// character stats and whether wearing/removing it could be fatal.
// If so, warns the player.
bool safe_to_remove_or_wear(const item_def &item, bool remove)
{
    int prop_str = 0;
    int prop_dex = 0;
    int prop_int = 0;

    // don't warn when putting on an unknown item
    if (item.base_type == OBJ_JEWELLERY
        && item_ident( item, ISFLAG_KNOW_PLUSES ))
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

    if (is_random_artefact( item ))
    {
        prop_str += randart_known_wpn_property(item, RAP_STRENGTH);
        prop_int += randart_known_wpn_property(item, RAP_INTELLIGENCE);
        prop_dex += randart_known_wpn_property(item, RAP_DEXTERITY);
    }

    if (remove)
    {
        std::string prompt =
            item.base_type == OBJ_WEAPONS ? "Unwield" : "Remov";
        prompt += "ing this item could be fatal. ";
        prompt += item.base_type == OBJ_WEAPONS ? "Unwield" : "Remove";
        prompt += " anyway? ";

        if ((prop_str >= you.strength || prop_int >= you.intel ||
             prop_dex >= you.dex)
            && !yesno(prompt.c_str(), false, 'n'))
        {
            return (false);
        }
    }
    else // put on
    {
        std::string prompt =
            item.base_type == OBJ_WEAPONS ? "Wield" : "Wear";
        prompt += "ing this item could be fatal. ";
        prompt += item.base_type == OBJ_WEAPONS ? "Wield" : "Put on";
        prompt += " anyway? ";
                    
        if ((-prop_str >= you.strength || -prop_int >= you.intel ||
             -prop_dex >= you.dex)
            && !yesno(prompt.c_str(), false, 'n'))
        {
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
static bool swap_rings(int ring_slot)
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
        mpr("You've already put that on!");
        return (true);
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
            return swap_rings(item_slot);
    }
    else if (you.equip[EQ_AMULET] != -1)
    {
        if (!check_warning_inscriptions(you.inv[you.equip[EQ_AMULET]],
                                        OPER_REMOVE) ||
            !remove_ring( you.equip[EQ_AMULET], true ))
            return false;

        if (!safe_to_remove_or_wear(you.inv[item_slot], false))
            return (false);
            
        start_delay(DELAY_JEWELLERY_ON, 1, item_slot);

        // Assume it's going to succeed.
        return (true);
    }

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
    you.time_taken = you.time_taken * 5 / 10;
    you.turn_is_over = true;

    return (true);
}

bool puton_ring(int slot, bool prompt_finger)
{
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
        item_slot = prompt_invent_item( "Put on which piece of jewellery?",
                        MT_INVLIST, OBJ_JEWELLERY, true, true, true, 0, NULL,
                        OPER_PUTON );

    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return (false);
    }

    return puton_item(item_slot, prompt_finger);
}                               // end puton_ring()

void jewellery_remove_effects(item_def &item)
{
    // The ring/amulet must already be removed from you.equip at this point.
    
    // Turn off show_uncursed before getting the item name, because this item
    // was just removed, and the player knows it's uncursed.
    const bool old_showuncursed = Options.show_uncursed;
    Options.show_uncursed = false;

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
        you.redraw_armour_class = 1;
        break;

    case RING_EVASION:
        you.redraw_evasion = 1;
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

    case RING_INVISIBILITY:
        // removing this ring effectively cancels all invisibility {dlb}
        if (you.duration[DUR_INVIS])
            you.duration[DUR_INVIS] = 1;
        break;

    case RING_LEVITATION:
        // removing this ring effectively cancels all levitation {dlb}
        if (you.duration[DUR_LEVITATION])
            you.duration[DUR_LEVITATION] = 1;
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

    // must occur after ring is removed -- bwr
    calc_mp();
}

bool remove_ring(int slot, bool announce)
{
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
            slot == -1? prompt_invent_item( "Remove which piece of jewellery?",
                                            MT_INVLIST,
                                            OBJ_JEWELLERY, true, true, true,
                                            0, NULL, OPER_REMOVE)
                      : slot;

        if (equipn == PROMPT_ABORT)
        {
            canned_msg( MSG_OK );
            return (false);
        }

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
    else if (!check_warning_inscriptions(you.inv[you.equip[hand_used]],
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
            mprf("%s is stuck to you!",
                 you.inv[you.equip[hand_used]].name(DESC_CAP_YOUR).c_str());
        else
            mpr("It's stuck to you!");

        set_ident_flags( you.inv[you.equip[hand_used]], ISFLAG_KNOW_CURSE );
        return (false);
    }
    
    ring_wear_2 = you.equip[hand_used];

    if (!safe_to_remove_or_wear(you.inv[ring_wear_2], true))
        return (false);

    you.equip[hand_used] = -1;

    jewellery_remove_effects(you.inv[ring_wear_2]);

    you.time_taken /= 2;
    you.turn_is_over = true;

    return (true);
}                               // end remove_ring()

void zap_wand( int slot )
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
        item_slot = prompt_invent_item( "Zap which item?",
                                    MT_INVLIST,
                                    OBJ_WANDS,
                                    true, true, true, 0, NULL,
                                    OPER_ZAP );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    item_def& wand = you.inv[item_slot];
    if (wand.base_type != OBJ_WANDS)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // if you happen to be wielding the wand, its display might change
    if (you.equip[EQ_WEAPON] == item_slot)
        you.wield_change = true;

    if ( wand.plus < 1 )
    {
        // it's an empty wand, inscribe it that way
        canned_msg(MSG_NOTHING_HAPPENS);
        wand.plus2 = ZAPCOUNT_EMPTY;
        you.turn_is_over = true;
        return;
    }

    const bool alreadyknown = item_type_known(wand);
    const bool alreadytried = item_type_tried(wand);
    if (!alreadyknown)
        beam.effect_known = false;

    const bool dangerous = player_in_a_dangerous_place();
    if (alreadyknown)
    {
        switch ( wand.sub_type )
        {
        case WAND_TELEPORTATION:
            targ_mode = TARG_ANY;
            break;

        case WAND_HASTING:
        case WAND_HEALING:
        case WAND_INVISIBILITY:
            targ_mode = TARG_FRIEND;
            break;
            
        default:
            targ_mode = TARG_ENEMY;
            break;
        }
    }

    message_current_target();
    direction( zap_wand, DIR_NONE, targ_mode );

    if (!zap_wand.isValid)
    {
        if (zap_wand.isCancel)
            canned_msg(MSG_OK);
        return;
    }

    if (you.duration[DUR_CONF])
    {
        zap_wand.tx = you.x_pos + random2(13) - 6;
        zap_wand.ty = you.y_pos + random2(13) - 6;
    }

    const zap_type type_zapped = static_cast<zap_type>(wand.zap());
    if (wand.sub_type == WAND_RANDOM_EFFECTS)
    {
        beam.effect_known = false;
        if (dangerous)
        {
            // Xom loves it when you use a Wand of Random Effects and
            // there is a dangerous monster nearby...
            xom_is_stimulated(255);
        }
    }

    beam.source_x = you.x_pos;
    beam.source_y = you.y_pos;
    beam.set_target(zap_wand);

    // zapping() updates beam
    zapping( static_cast<zap_type>(type_zapped),
             30 + roll_dice(2, you.skills[SK_EVOCATIONS]), beam );

    // take off a charge
    wand.plus--;

    // increment zap count
    if ( wand.plus2 >= 0 )
        wand.plus2++;

    // identify if necessary
    if ((beam.obvious_effect || type_zapped == ZAP_FIREBALL) &&
        !alreadyknown)
    {
        set_ident_type( wand.base_type, wand.sub_type, ID_KNOWN_TYPE );
        mpr(wand.name(DESC_INVENTORY_EQUIP).c_str());
    }
    else
    {
        set_ident_type( wand.base_type, wand.sub_type, ID_TRIED_TYPE );
    }

    if (item_type_known(wand)
        && (item_ident( wand, ISFLAG_KNOW_PLUSES )
            || you.skills[SK_EVOCATIONS] > 5 + random2(15)))
    {
        if (!item_ident( wand, ISFLAG_KNOW_PLUSES ))
        {
            mpr("Your skill with magical items lets you calculate "
                "the power of this device...");
        }

        mprf("This wand has %d charge%s left.",
             wand.plus, (wand.plus == 1) ? "" : "s" );
        set_ident_flags( wand, ISFLAG_KNOW_PLUSES );
    }

    exercise( SK_EVOCATIONS, 1 );
    alert_nearby_monsters();

    if (!alreadyknown && !alreadytried && dangerous)
    {
        // Xom loves it when you use an unknown wand and there is a
        // dangerous monster nearby...
        xom_is_stimulated(255);
    }

    you.turn_is_over = true;
}                               // end zap_wand()

void inscribe_item()
{
    int item_slot;
    char buf[79];
    if (inv_count() < 1)
    {
        mpr("You don't have anything to inscribe.");
        return;
    }
    item_slot = prompt_invent_item(
                    "Inscribe which item? ", 
                    MT_INVLIST, 
                    OSEL_ANY );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }
    mpr( you.inv[item_slot].name(DESC_INVENTORY).c_str(), MSGCH_EQUIPMENT );
    mpr( "Inscribe with what? ", MSGCH_PROMPT );
    if (!cancelable_get_line(buf, sizeof buf))
    {
        // strip spaces from the end
        for (int i = strlen(buf) - 1; i >= 0; i--)
        {
            if (isspace( buf[i] ))
                buf[i] = 0;
            else
                break;
        }
            
        you.inv[item_slot].inscription = std::string(buf);
        you.wield_change = true;
    }
    else
    {
        canned_msg(MSG_OK);
    }
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
        if (grd[you.x_pos][you.y_pos] == DNGN_BLUE_FOUNTAIN
            || grd[you.x_pos][you.y_pos] == DNGN_SPARKLING_FOUNTAIN)
        {
            if (drink_fountain())
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
        item_slot = prompt_invent_item( "Drink which item?",
                                    MT_INVLIST, OBJ_POTIONS,
                                    true, true, true, 0, NULL,
                                    OPER_QUAFF );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (you.inv[item_slot].base_type != OBJ_POTIONS)
    {
        mpr("You can't drink that!");
        return;
    }

    const bool alreadyknown = item_type_known(you.inv[item_slot]);

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous =
        player_in_a_dangerous_place() && (you.experience_level > 1);
    
    if (potion_effect(static_cast<potion_type>(you.inv[item_slot].sub_type),
                      40))
    {
        set_ident_flags( you.inv[item_slot], ISFLAG_IDENT_MASK );

        set_ident_type( you.inv[item_slot].base_type, 
                        you.inv[item_slot].sub_type, ID_KNOWN_TYPE );
    }
    else
    {
        set_ident_type( you.inv[item_slot].base_type, 
                        you.inv[item_slot].sub_type, ID_TRIED_TYPE );
    }
    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you drink an unknown potion and there is
        // a dangerous monster nearby...
        xom_is_stimulated(255);
    }

    dec_inv_item_quantity( item_slot, 1 );
    you.turn_is_over = true;

    lessen_hunger(40, true);
}                               // end drink()

bool drink_fountain()
{
    const dungeon_feature_type feat = grd[you.x_pos][you.y_pos];

    if ( feat != DNGN_BLUE_FOUNTAIN && feat != DNGN_SPARKLING_FOUNTAIN )
        return false;

    if (you.flight_mode() == FL_LEVITATE)
    {
        mpr("You're floating high above the fountain.");
        return false;
    }

    potion_type fountain_effect = POT_WATER;
    if ( feat == DNGN_BLUE_FOUNTAIN )
    {
        if (!yesno("Drink from the fountain?"))
            return false;
        else
            mpr("You drink the pure, clear water.");
    }
    else                        // sparkling fountain
    {
        if (!yesno("Drink from the sparkling fountain?"))
            return false;
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

        ASSERT( ARRAYSIZE(weights) == ARRAYSIZE(effects) );
        fountain_effect =
            effects[choose_random_weighted(weights,
                                           weights + ARRAYSIZE(weights))];
    }

    if (fountain_effect != POT_WATER)
        xom_is_stimulated(64);

    potion_effect(fountain_effect, 100);

    bool gone_dry = false;
    if ( feat == DNGN_BLUE_FOUNTAIN )
    {
        if ( one_chance_in(20) )
            gone_dry = true;
    }
    else                        // sparkling fountain
    {
        if (one_chance_in(10))
            gone_dry = true;
        else if ( random2(50) > 40 ) // no message!
        {
            grd[you.x_pos][you.y_pos] = DNGN_BLUE_FOUNTAIN;
            set_terrain_changed(you.x_pos, you.y_pos);
        }
    }

    if (gone_dry)
    {
        mpr("The fountain dries up!");
        if (feat == DNGN_BLUE_FOUNTAIN)
            grd[you.x_pos][you.y_pos] = DNGN_DRY_FOUNTAIN_I;
        else
            grd[you.x_pos][you.y_pos] = DNGN_DRY_FOUNTAIN_II;

        set_terrain_changed(you.x_pos, you.y_pos);

        crawl_state.cancel_cmd_repeat();
    }

    you.turn_is_over = true;
    return true;
}                               // end drink_fountain()

static bool affix_weapon_enchantment()
{
    const int wpn = you.equip[ EQ_WEAPON ];
    bool success = true;

    struct bolt beam;

    if (wpn == -1 || !you.duration[ DUR_WEAPON_BRAND ])
        return (false);

    std::string itname = you.inv[wpn].name(DESC_CAP_YOUR);

    switch (get_weapon_brand( you.inv[wpn] ))
    {
    case SPWPN_VORPAL:
        if (get_vorpal_type( you.inv[wpn] ) != DVORP_CRUSHING)
            mprf("%s's sharpness seems more permanent.", itname.c_str());
        else
            mprf("%s's heaviness feels very stable.", itname.c_str());
        break;

    case SPWPN_FLAMING:
        mprf("%s is engulfed in an explosion of flames!", itname.c_str());

        beam.type = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage = dice_def( 3, 10 );
        beam.flavour = BEAM_FIRE;
        beam.target_x = you.x_pos;
        beam.target_y = you.y_pos;
        beam.name = "fiery explosion";
        beam.colour = RED;
        beam.thrower = KILL_YOU;
        beam.aux_source = "a fiery explosion";
        beam.ex_size = 2;
        beam.is_tracer = false;
        beam.is_explosion = true;

        explosion(beam);
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

        torment_monsters(you.x_pos, you.y_pos, 0, TORMENT_GENERIC);
        success = false;

        // is only naughty if you know you're doing it
        did_god_conduct(DID_UNHOLY, 10,
            get_ident_type(OBJ_SCROLLS, SCR_ENCHANT_WEAPON_III)==ID_KNOWN_TYPE);

        break;

    case SPWPN_DISTORTION:
        // [dshaligram] Attempting to fix a distortion brand gets you a free
        // distortion effect, and no permabranding. Sorry, them's the breaks.
        mprf("%s twongs alarmingly.", itname.c_str());

        // from unwield_item
        miscast_effect( SPTYP_TRANSLOCATION, 9, 90, 100, 
                        "distortion affixation" );
        success = false;
        break;

    default:
        success = false;
        break;
    }

    if (success)
        you.duration[DUR_WEAPON_BRAND] = 0;

    return (success);
}

bool enchant_weapon( enchant_stat_type which_stat, bool quiet )
{
    const int wpn = you.equip[ EQ_WEAPON ];
    bool affected = true;
    int enchant_level;

    if (wpn == -1 
        || (you.inv[ wpn ].base_type != OBJ_WEAPONS
            && you.inv[ wpn ].base_type != OBJ_MISSILES))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_HAPPENS);

        return (false);
    }

    item_def& item = you.inv[wpn];

    you.wield_change = true;

    // missiles only have one stat
    if (item.base_type == OBJ_MISSILES)
        which_stat = ENCHANT_TO_HIT;

    if (which_stat == ENCHANT_TO_HIT)
        enchant_level = item.plus;
    else
        enchant_level = item.plus2;

    // artefacts can't be enchanted, but scrolls still remove curses
    if (item.base_type == OBJ_WEAPONS
        && (is_fixed_artefact( you.inv[wpn] )
            || is_random_artefact( you.inv[wpn] )))
    {
        affected = false;
    }

    if (enchant_level >= 4 && random2(9) < enchant_level)
    {
        affected = false;
    }

    // if it isn't affected by the enchantment, it will still
    // be uncursed:
    if (!affected)
    {
        if (item_cursed(item))
        {
            if (!quiet)
            {
                mprf("%s glows silver for a moment.",
                     item.name(DESC_CAP_YOUR).c_str());
            }

            do_uncurse_item( you.inv[you.equip[EQ_WEAPON]] );

            return (true);
        }
        else
        {
            if (!quiet)
                canned_msg(MSG_NOTHING_HAPPENS);

            return (false);
        }
    }

    // Get the item name now before the enchantment changes it.
    std::string iname = item.name(DESC_CAP_YOUR);

    do_uncurse_item( item );

    if (item.base_type == OBJ_WEAPONS)
    {
        if (which_stat == ENCHANT_TO_DAM)
        {
            item.plus2++;

            if (!quiet)
                mprf("%s glows red for a moment.", iname.c_str());
        }
        else if (which_stat == ENCHANT_TO_HIT)
        {
            item.plus++;

            if (!quiet)
                mprf("%s glows green for a moment.", iname.c_str());
        }
    }
    else if (item.base_type == OBJ_MISSILES)
    {
        mprf("%s %s red for a moment.", iname.c_str(),
             item.quantity > 1 ? "glow" : "glows");

        item.plus++;
    }

    xom_is_stimulated(16);
    return (true);
}

static bool enchant_armour( void )
{
    // NOTE: It is assumed that armour which changes in this way does
    // not change into a form of armour with a different evasion modifier.
    int nthing = you.equip[EQ_BODY_ARMOUR];

    if (nthing != -1
        && (you.inv[nthing].sub_type == ARM_DRAGON_HIDE
            || you.inv[nthing].sub_type == ARM_ICE_DRAGON_HIDE
            || you.inv[nthing].sub_type == ARM_STEAM_DRAGON_HIDE
            || you.inv[nthing].sub_type == ARM_MOTTLED_DRAGON_HIDE
            || you.inv[nthing].sub_type == ARM_STORM_DRAGON_HIDE
            || you.inv[nthing].sub_type == ARM_GOLD_DRAGON_HIDE
            || you.inv[nthing].sub_type == ARM_SWAMP_DRAGON_HIDE
            || you.inv[nthing].sub_type == ARM_TROLL_HIDE))
    {
        mprf("%s glows purple and changes!",
             you.inv[you.equip[EQ_BODY_ARMOUR]].name(DESC_CAP_YOUR).c_str());

        you.redraw_armour_class = 1;

        hide2armour(you.inv[nthing]);
        return (true);
    }

    // pick random piece of armour
    int count = 0;
    int affected_slot = EQ_WEAPON;

    for (int i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++) 
    {
        if (you.equip[i] != -1)
        {
            count++;
            if (one_chance_in( count ))
                affected_slot = i;
        }
    } 

    // no armour == no enchantment
    if (affected_slot == EQ_WEAPON)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (false);
    }

    bool affected = true;
    item_def &item = you.inv[you.equip[ affected_slot ]];

    if (is_random_artefact( item )
        || ((item.sub_type >= ARM_CLOAK && item.sub_type <= ARM_BOOTS)
            && item.plus >= 2)
        || ((item.sub_type == ARM_SHIELD 
                || item.sub_type == ARM_BUCKLER
                || item.sub_type == ARM_LARGE_SHIELD)
            && item.plus >= 2)
        || (item.plus >= 3 && random2(8) < item.plus))
    {
        affected = false;
    }

    // even if not affected, it may be uncursed.
    if (!affected)
    {
        if (item_cursed( item ))
        {
            mprf("%s glows silver for a moment.",
                 item.name(DESC_CAP_YOUR).c_str());

            do_uncurse_item( item );
            return (true);
        }
        else
        {
            canned_msg( MSG_NOTHING_HAPPENS );
            return (false);
        }
    }

    // vVvVv    This is *here* for a reason!
    mprf("%s glows green for a moment.",
         item.name(DESC_CAP_YOUR).c_str());

    item.plus++;

    do_uncurse_item( item );
    you.redraw_armour_class = 1;
    xom_is_stimulated(16);
    return (true);
}

static void handle_read_book( int item_slot )
{
    item_def& book(you.inv[item_slot]);

    if (book.sub_type == BOOK_DESTRUCTION)
    {
        if (silenced(you.x_pos, you.y_pos))
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
            mesclr( true );
            return;
        }

        const spell_type spell = which_spell_in_book(book.sub_type,
                                                     letter_to_index(ltr));
        if (spell == SPELL_NO_SPELL)
        {
            mesclr( true );
            return;
        }

        describe_spell( spell );
    }
}

void read_scroll( int slot )
{
    int affected = 0;
    int i;
    int count;
    int nthing;
    struct bolt beam;

    // added: scroll effects are never tracers.
    beam.is_tracer = false;

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

    int item_slot = (slot != -1) ?
        slot :
        prompt_invent_item(
                        "Read which item?", 
                        MT_INVLIST, 
                        OBJ_SCROLLS, 
                        true, true, true, 0, NULL,
                        OPER_READ );

    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    item_def& scroll = you.inv[item_slot];

    if (scroll.base_type != OBJ_BOOKS && scroll.base_type != OBJ_SCROLLS)
    {
        mpr("You can't read that!");
        crawl_state.zero_turns_taken();
        return;
    }

    // here we try to read a book {dlb}:
    if (scroll.base_type == OBJ_BOOKS)
    {
        handle_read_book( item_slot );
        return;
    }

    if (silenced(you.x_pos, you.y_pos))
    {
        mpr("Magic scrolls do not work when you're silenced!");
        crawl_state.zero_turns_taken();
        return;
    }

    // ok - now we FINALLY get to read a scroll !!! {dlb}
    you.turn_is_over = true;

    // imperfect vision prevents players from reading actual content {dlb}:
    if (you.mutation[MUT_BLURRY_VISION]
        && random2(5) < you.mutation[MUT_BLURRY_VISION])
    {
        mpr((you.mutation[MUT_BLURRY_VISION] == 3 && one_chance_in(3))
                        ? "This scroll appears to be blank."
                        : "The writing blurs in front of your eyes.");
        return;
    }

    // decrement and handle inventory if any scroll other than paper {dlb}:
    const int scroll_type = scroll.sub_type;
    if (scroll_type != SCR_PAPER &&
        (scroll_type != SCR_IMMOLATION || you.duration[DUR_CONF]))
    {
        mpr("As you read the scroll, it crumbles to dust.");
        // Actual removal of scroll done afterwards. -- bwr
    }

    const bool alreadyknown = item_type_known(scroll);
    const bool dangerous = player_in_a_dangerous_place();

    // scrolls of paper are also exempted from this handling {dlb}:
    if (scroll_type != SCR_PAPER)
    {
        if (you.duration[DUR_CONF])
        {
            random_uselessness(random2(9), item_slot);
            dec_inv_item_quantity( item_slot, 1 );
            return;
        }

        if (!you.skills[SK_SPELLCASTING])
            exercise(SK_SPELLCASTING, (coinflip()? 2 : 1));
    }

    bool id_the_scroll = true;  // to prevent unnecessary repetition

    // it is the exception, not the rule, that
    // the scroll will not be identified {dlb}:
    switch (scroll_type)
    {
    case SCR_PAPER:
        // remember paper scrolls handled as special case above, too:
        mpr("This scroll appears to be blank.");
        if (you.mutation[MUT_BLURRY_VISION] == 3)
            id_the_scroll = false;
        break;

    case SCR_RANDOM_USELESSNESS:
        random_uselessness(random2(9), item_slot);
        id_the_scroll = false;
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
        mpr("You hear a loud clanging noise!");
        noisy( 25, you.x_pos, you.y_pos );
        break;

    case SCR_SUMMONING:
        if (create_monster( MONS_ABOMINATION_SMALL, 6, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target,
                            MONS_PROGRAM_BUG ) != -1)
        {
            mpr("A horrible Thing appears!");
        }
        break;

    case SCR_FOG:
        mpr("The scroll dissolves into smoke.");
        big_cloud( one_chance_in(20) ? CLOUD_POISON :
                   (one_chance_in(19) ? CLOUD_STEAM : random_smoke_type()),
                   KC_YOU, you.x_pos, you.y_pos, 50, 8 + random2(8));
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
        torment( TORMENT_SCROLL, you.x_pos, you.y_pos );

        // is only naughty if you know you're doing it
        did_god_conduct(DID_UNHOLY, 10, item_type_known(scroll));
        break;

    case SCR_IMMOLATION:
        mpr("The scroll explodes in your hands!");
        // we do this here to prevent it from blowing itself up
        dec_inv_item_quantity( item_slot, 1 );

        beam.type = dchar_glyph(DCHAR_FIRED_BURST);
        beam.damage = dice_def( 3, 10 );
        // unsure about this    // BEAM_EXPLOSION instead? {dlb}
        beam.flavour = BEAM_FIRE;
        beam.target_x = you.x_pos;
        beam.target_y = you.y_pos;
        beam.name = "fiery explosion";
        beam.colour = RED;
        // your explosion, (not someone else's explosion)
        beam.thrower = KILL_YOU;
        beam.aux_source = "reading a scroll of immolation";
        beam.ex_size = 2;
        beam.is_explosion = true;

        if (!alreadyknown)
            beam.effect_known = false;

        explosion(beam);
        break;

    case SCR_IDENTIFY:
        if ( !item_type_known(scroll) )
        {
            mpr("This is a scroll of identify!");
            more();
        }

        set_ident_flags( you.inv[item_slot], ISFLAG_IDENT_MASK );

        // important {dlb}
        set_ident_type( OBJ_SCROLLS, SCR_IDENTIFY, ID_KNOWN_TYPE );

        identify(-1);
        you.wield_change = true;
        break;

    case SCR_CURSE_WEAPON:
        nthing = you.equip[EQ_WEAPON];

        if (nthing == -1
            || you.inv[nthing].base_type != OBJ_WEAPONS
            || item_cursed( you.inv[nthing] ))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        else
        {
            mprf("%s glows black for a moment.",
                 you.inv[nthing].name(DESC_CAP_YOUR).c_str());

            do_curse_item( you.inv[nthing] );
            you.wield_change = true;
        }
        break;

    // everything [in the switch] below this line is a nightmare {dlb}:
    case SCR_ENCHANT_WEAPON_I:
        id_the_scroll = enchant_weapon( ENCHANT_TO_HIT );
        break;

    case SCR_ENCHANT_WEAPON_II:
        id_the_scroll = enchant_weapon( ENCHANT_TO_DAM );
        break;

    case SCR_ENCHANT_WEAPON_III:
        if (you.equip[ EQ_WEAPON ] != -1) 
        {
            // Successfully affixing the enchantment will print
            // its own message.
            if (!affix_weapon_enchantment())
            {
                const std::string iname =
                    you.inv[you.equip[EQ_WEAPON]].name(DESC_CAP_YOUR);

                mprf("%s glows bright yellow for a while.", iname.c_str() );

                enchant_weapon( ENCHANT_TO_HIT, true );

                if (coinflip())
                    enchant_weapon( ENCHANT_TO_HIT, true );

                enchant_weapon( ENCHANT_TO_DAM, true );

                if (coinflip())
                    enchant_weapon( ENCHANT_TO_DAM, true );

                do_uncurse_item( you.inv[you.equip[EQ_WEAPON]] );
            }
        }
        else
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        break;

    case SCR_VORPALISE_WEAPON:
        nthing = you.equip[EQ_WEAPON];
        if (nthing == -1
            || you.inv[ nthing ].base_type != OBJ_WEAPONS
            || (you.inv[ nthing ].base_type == OBJ_WEAPONS
                && (is_fixed_artefact( you.inv[ nthing ] )
                    || is_random_artefact( you.inv[ nthing ] )
                    || you.inv[nthing].sub_type == WPN_BLOWGUN)))
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            break;
        }

        mprf("%s emits a brilliant flash of light!",
             you.inv[nthing].name(DESC_CAP_YOUR).c_str());

        alert_nearby_monsters();

        if (get_weapon_brand( you.inv[nthing] ) != SPWPN_NORMAL)
        {
            mpr("You feel strangely frustrated.");
            break;
        }

        you.wield_change = true;
        set_item_ego_type( you.inv[nthing], OBJ_WEAPONS, SPWPN_VORPAL );
        break;

    case SCR_RECHARGING:
        nthing = you.equip[EQ_WEAPON];

        if (nthing != -1
            && !is_random_artefact( you.inv[nthing] )
            && !is_fixed_artefact( you.inv[nthing] )
            && get_weapon_brand( you.inv[nthing] ) == SPWPN_ELECTROCUTION)
        {
            id_the_scroll = enchant_weapon( ENCHANT_TO_DAM );

            if (!item_ident(you.inv[nthing], ISFLAG_KNOW_TYPE))
	    {
	        if (item_type_known(scroll))
		    set_ident_flags(you.inv[nthing], ISFLAG_KNOW_TYPE);
		else   
                    id_the_scroll = false;
            }
	    break;
        }

        if (!recharge_wand())
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
        }
        break;

    case SCR_ENCHANT_ARMOUR:
        id_the_scroll = enchant_armour();
        break;

    case SCR_CURSE_ARMOUR:
        // make sure there's something to curse first
        count = 0;
        affected = EQ_WEAPON;
        for (i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
        {
            if (you.equip[i] != -1 && !item_cursed( you.inv[you.equip[i]] ))
            {
                count++;
                if (one_chance_in( count ))
                    affected = i;
            }
        }

        if (affected == EQ_WEAPON)
        {
            canned_msg(MSG_NOTHING_HAPPENS);
            id_the_scroll = false;
            break;
        }
        else
        {
            // make the name before we curse it
            item_def& item = you.inv[you.equip[affected]];
            mprf("%s glows black for a moment.",
                 item.name(DESC_CAP_YOUR).c_str());
            do_curse_item( item );
        }
        break;

    }                           // end switch

    // finally, destroy and identify the scroll
    // scrolls of immolation were already destroyed earlier
    if (scroll_type != SCR_PAPER && scroll_type != SCR_IMMOLATION)
    {
        if ( id_the_scroll )
            set_ident_flags( scroll, ISFLAG_KNOW_TYPE ); // for notes

        dec_inv_item_quantity( item_slot, 1 );
    }

    set_ident_type( OBJ_SCROLLS, scroll_type, 
                    (id_the_scroll) ? ID_KNOWN_TYPE : ID_TRIED_TYPE );

    if (!alreadyknown && dangerous)
    {
         // Xom loves it when you read an unknown scroll and there is
         // a dangerous monster nearby...
        xom_is_stimulated(255);
    }
}                               // end read_scroll()

void examine_object(void)
{
    int item_slot = prompt_invent_item( "Examine which item?", 
                                        MT_INVLIST, -1,
                                        true, true, true, 0, NULL,
                                        OPER_EXAMINE );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    describe_item( you.inv[item_slot], true );
    redraw_screen();
    mesclr(true);
}                               // end original_name()

void use_randart(unsigned char item_wield_2)
{
    use_randart( you.inv[ item_wield_2 ] );
}

void use_randart(item_def &item)
{
#define unknown_proprt(prop) (proprt[(prop)] && !known[(prop)])

    ASSERT( is_random_artefact( item ) );

    const bool alreadyknown = item_type_known(item);
    const bool dangerous = player_in_a_dangerous_place();

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

    // modify ability scores
    // output result even when identified (because of potential fatality)
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

    if (proprt[RAP_NOISES])
        you.special_wield = SPWLD_NOISE;

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
        if ( inv_slot == you.equip[i] )
            return true;
    return false;
}

#ifdef USE_TILE
// Interactive menu for item drop/use
void tile_use_item(int idx, InvAction act)
{
    if (act == INV_PICKUP)
    {
         pickup_single_item(idx, mitm[idx].quantity);
         return;
    }
    else if (act == INV_DROP)
    {
        TileMoveInvCursor(-1);
        drop_item(idx, you.inv[idx].quantity);
        return;
    }
    else if (act == INV_USE_FLOOR)
    {
        if (mitm[idx].base_type == OBJ_CORPSES
            && mitm[idx].sub_type != CORPSE_SKELETON
            && !food_is_rotten(mitm[idx]))
        {
            butchery(idx);
        }
        return;
    }
    else if (act == INV_EAT_FLOOR)
    {
        if (mitm[idx].base_type == OBJ_CORPSES
                && you.species == SP_VAMPIRE
            || mitm[idx].base_type == OBJ_FOOD
                && you.species != SP_MUMMY && you.species != SP_VAMPIRE)
        {
            eat_floor_item(idx);
        }
        return;
    }
    else if (act != INV_USE)
    {
        return;
    }

    // Equipped?
    bool equipped = false;
    bool equipped_weapon = false;
    for (unsigned int i=0; i< NUM_EQUIP;i++)
    {
        if (you.equip[i] == idx)
        {
            equipped = true;
            if (i == EQ_WEAPON)
                equipped_weapon = true;
            break;
        }
    }

    TileMoveInvCursor(-1);

    // Special case for folks who are wielding something
    // that they shouldn't be wielding.
    // Note that this is only a problem for equipables
    // (otherwise it would only waste a turn)
    if (you.equip[EQ_WEAPON] == idx
        && (you.inv[idx].base_type == OBJ_ARMOUR
            || you.inv[idx].base_type == OBJ_JEWELLERY))
    {
        if (!check_warning_inscriptions(you.inv[idx], OPER_WIELD))
            return;

        wield_weapon(true, PROMPT_GOT_SPECIAL);
        return;
    }

    // Use it
    const int type = you.inv[idx].base_type;
    switch (type)
    {
        case OBJ_WEAPONS:
        case OBJ_STAVES:
        case OBJ_MISCELLANY:
            // wield any unwielded item of these types
            if (!equipped)
            {
                if (check_warning_inscriptions(you.inv[idx], OPER_WIELD))
                    wield_weapon(true, idx);
                return;
            }
            // evoke misc. items and known rods
            if (type == OBJ_MISCELLANY
                ||item_is_rod(you.inv[idx]) && item_type_known(you.inv[idx]))
            {
                if (check_warning_inscriptions(you.inv[idx], OPER_EVOKE))
                    evoke_wielded();
                return;
            }
            // unwield unknown rods, other staves or weapons
            if (check_warning_inscriptions(you.inv[idx], OPER_WIELD))
                wield_weapon(true, PROMPT_GOT_SPECIAL); // unwield
            return;
            
        case OBJ_MISSILES:
            if (check_warning_inscriptions(you.inv[idx], OPER_THROW))
                throw_anything(idx);
            return;

        case OBJ_ARMOUR:
            if (equipped && !equipped_weapon)
            {
                if (check_warning_inscriptions(you.inv[idx], OPER_TAKEOFF))
                    takeoff_armour(idx);
            }
            else if (check_warning_inscriptions(you.inv[idx], OPER_WEAR))
                wear_armour(idx);
            return;

        case OBJ_WANDS:
            if (check_warning_inscriptions(you.inv[idx], OPER_ZAP))
                zap_wand(idx);
            return;

        case OBJ_CORPSES:
            if (you.species != SP_VAMPIRE
                || you.inv[idx].sub_type == CORPSE_SKELETON
                || food_is_rotten(you.inv[idx]))
            {
                break;
            }
            // intentional fall-through for Vampires
        case OBJ_FOOD:
            if (check_warning_inscriptions(you.inv[idx], OPER_EAT))
                eat_food(false, idx);
            return;

        case OBJ_BOOKS:
            if (you.inv[idx].sub_type == BOOK_MANUAL
                || you.inv[idx].sub_type == BOOK_DESTRUCTION)
            {
                if (check_warning_inscriptions(you.inv[idx], OPER_READ))
                    handle_read_book(idx);
            } // else it's a spellbook
            else if (check_warning_inscriptions(you.inv[idx], OPER_MEMORISE))
                learn_spell(idx);
            return;
            
        case OBJ_SCROLLS:
            if (check_warning_inscriptions(you.inv[idx], OPER_READ))
                read_scroll(idx);
            return;

        case OBJ_JEWELLERY:
            if (equipped && !equipped_weapon)
            {
                if (check_warning_inscriptions(you.inv[idx], OPER_REMOVE))
                    remove_ring(idx);
            }
            else if (check_warning_inscriptions(you.inv[idx], OPER_PUTON))
                puton_ring(idx, false);
            return;

        case OBJ_POTIONS:
            if (check_warning_inscriptions(you.inv[idx], OPER_QUAFF))
                drink(idx);
            return;

        default:
            return;
    }
}
#endif
