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
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mstuff2.h"
#include "mon-util.h"
#include "ouch.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spells3.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "stuff.h"
#include "transfor.h"
#include "view.h"

bool drink_fountain(void);
static bool enchant_armour( void );

// Rather messy - we've gathered all the can't-wield logic from wield_weapon()
// here.
bool can_wield(const item_def *weapon, bool say_reason)
{
#define SAY(x) if (say_reason) { x; } else 

    if (you.berserker)
    {
        SAY(canned_msg(MSG_TOO_BERSERK));
        return false;
    }
    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE 
            && !can_equip( EQ_WEAPON ))
    {
        SAY(mpr("You can't wield anything in your present form."));
        return false;
    }

    if (you.equip[EQ_WEAPON] != -1
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

    if (is_shield_incompatible(*weapon))
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
                || weapon->sub_type == WPN_LOCHABER_AXE
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
                    || weap_brand == SPWPN_DISRUPTION
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
    int item_slot = 0;
    char str_pass[ ITEMNAME_SIZE ];

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    // Any general reasons why we can't wield a new object?
    if (!can_wield(NULL, true))
        return (false);

    if (you.sure_blade)
    {
        mpr("The bond with your blade fades away.");
        you.sure_blade = 0;
    }

    if (auto_wield)
    {
        if (you.equip[EQ_WEAPON] == 0)  // ie. weapon is currently 'a'
            item_slot = 1;
        else
            item_slot = 0;
        if (slot != -1) item_slot = slot;
    }

    bool force_unwield = 
                you.inv[item_slot].base_type != OBJ_WEAPONS
                   && you.inv[item_slot].base_type != OBJ_MISSILES
                   && you.inv[item_slot].base_type != OBJ_STAVES;

    // Prompt if not using the auto swap command, 
    // or if the swap slot is empty.
    if (!auto_wield || !is_valid_item(you.inv[item_slot]) || force_unwield)
    {
        if (!auto_wield)
            item_slot = prompt_invent_item(
                            "Wield which item (- for none)?",
                            MT_INVSELECT, OSEL_WIELD, 
                            true, true, true, '-', NULL, OPER_WIELD);
        else
            item_slot = PROMPT_GOT_SPECIAL;

        if (item_slot == PROMPT_ABORT)
        {
            canned_msg( MSG_OK );
            return (false);
        }
        else if (item_slot == PROMPT_GOT_SPECIAL)  // '-' or bare hands
        {
            if (you.equip[EQ_WEAPON] != -1)
            {
                unwield_item(you.equip[EQ_WEAPON]);
                you.turn_is_over = true;

                you.equip[EQ_WEAPON] = -1;
                canned_msg( MSG_EMPTY_HANDED );
                you.time_taken *= 3;
                you.time_taken /= 10;
            }
            else
            {
                mpr( "You are already empty-handed." );
            }
            return (true);
        }
    }

    if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are already wielding that!");
        return (true);
    }

    if (!can_wield(&you.inv[item_slot], true))
        return (false);

    // Go ahead and wield the weapon.
    if (you.equip[EQ_WEAPON] != -1)
        unwield_item(you.equip[EQ_WEAPON]);

    you.equip[EQ_WEAPON] = item_slot;

    // any oddness on wielding taken care of here
    wield_effects(item_slot, show_weff_messages);

    in_name( item_slot, DESC_INVENTORY_EQUIP, str_pass );
    mpr( str_pass );

    // warn player about low str/dex or throwing skill
    wield_warning();

    // time calculations
    you.time_taken *= 5;
    you.time_taken /= 10;

    you.wield_change = true;
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

    // Warnings are limited to rods and bows at the moment.
    const item_def *weapon = player_weapon();
    if (!weapon)
        return;

    if (item_is_rod(*weapon))
        warn_rod_shield_interference(*weapon);
    else if (is_range_weapon(*weapon))
        warn_launcher_shield_slowdown(*weapon);
}

// provide a function for handling initial wielding of 'special'
// weapons, or those whose function is annoying to reproduce in
// other places *cough* auto-butchering *cough*    {gdl}

void wield_effects(int item_wield_2, bool showMsgs)
{
    unsigned char i_dam = 0;

    // and here we finally get to the special effects of wielding {dlb}
    if (you.inv[item_wield_2].base_type == OBJ_MISCELLANY)
    {
        if (you.inv[item_wield_2].sub_type == MISC_LANTERN_OF_SHADOWS)
        {
            if (showMsgs)
                mpr("The area is filled with flickering shadows.");

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
                    mpr("You hear the crackle of electricity.");
                    break;

                case SPWPN_ORC_SLAYING:
                    mpr((you.species == SP_HILL_ORC)
                            ? "You feel a sudden desire to commit suicide."
                            : "You feel a sudden desire to kill orcs!");
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

                case SPWPN_DISRUPTION:
                    mpr("You sense a holy aura.");
                    break;

                case SPWPN_PAIN:
                    mpr("A searing pain shoots up your arm!");
                    break;

                case SPWPN_SINGING_SWORD:
                    mpr("The Singing Sword hums in delight!");
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
                    // mummies cannot smell
                    if (you.species != SP_MUMMY)
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
                you.redraw_armour_class = 1;
                break;

            case SPWPN_DISTORTION:
                miscast_effect( SPTYP_TRANSLOCATION, 9, 90, 100, "a distortion effect" );
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
            mpr("It sticks to your hand!");
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
    else if (you.berserker)
        canned_msg(MSG_TOO_BERSERK);
    else
    {
        slot = prompt_invent_item( mesg.c_str(), MT_INVSELECT, OBJ_ARMOUR,
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
void wear_armour(void)
{
    int armour_wear_2;

    if (!armour_prompt("Wear which item?", &armour_wear_2, OPER_WEAR))
        return;

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

bool do_wear_armour( int item, bool quiet )
{
    if (!is_valid_item( you.inv[item] ))
    {
        if (!quiet)
           mpr("You don't have any such object.");

        return (false);
    }

    const int base_type = you.inv[item].base_type;
    if (base_type != OBJ_ARMOUR)
    {
        if (!quiet)
           mpr("You can't wear that.");

        return (false);
    }

    const int sub_type  = you.inv[item].sub_type;
    const item_def &invitem = you.inv[item];
    const equipment_type slot = get_armour_slot(invitem);

    if (item == you.equip[EQ_WEAPON])
    {
        if (!quiet)
           mpr("You are wielding that object!");

        return (false);
    }

    for (int loopy = EQ_CLOAK; loopy <= EQ_BODY_ARMOUR; loopy++)
    {
        if (item == you.equip[loopy])
        {
            if (!quiet)
               mpr("You are already wearing that!");

            return (false);
        }
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

    bool can_wear = true;
    if (sub_type == ARM_NAGA_BARDING)
        can_wear = (you.species == SP_NAGA);

    if (sub_type == ARM_CENTAUR_BARDING)
        can_wear = (you.species == SP_CENTAUR);

    if (!can_wear)
    {
        if (!quiet)
            mpr("You can't wear that!");
        return (false);
    }

    if (you.inv[item].sub_type == ARM_BOOTS)
    {
        if (you.species == SP_NAGA || you.species == SP_CENTAUR)
        {
            if (!quiet)
                mpr("You can't wear that!");
            return (false);
        }

        if (player_is_swimming() && you.species == SP_MERFOLK)
        {
            if (!quiet)
               mpr("You don't currently have feet!");

            return (false);
        }
    }

    if (you.species == SP_NAGA && sub_type == ARM_NAGA_BARDING
        && !player_is_shapechanged())
    {
        // it fits
    }
    else if (you.species == SP_CENTAUR
             && sub_type == ARM_CENTAUR_BARDING
             && !player_is_shapechanged())
    {
        // it fits
    }
    else if (sub_type == ARM_HELMET
             && (get_helmet_type(invitem) == THELM_CAP
                 || get_helmet_type(invitem) == THELM_WIZARD_HAT))
    {
        // caps & wiz hats always fit, unless your head's too big (ogres &c)
    }
    else if (!can_equip( slot ))
    {
        if (!quiet)
           mpr("You can't wear that in your present form.");

        return (false);
    }

    // Cannot swim in heavy armour
    if (player_is_swimming()
        && slot == EQ_BODY_ARMOUR
        && !is_light_armour( invitem ))
    {
        if (!quiet)
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
            || (sub_type == ARM_HELMET
                && (get_helmet_type(invitem) == THELM_HELM
                    || get_helmet_type(invitem) == THELM_HELMET)))
        {
            if (!quiet)
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
            || (sub_type == ARM_HELMET
                && (get_helmet_type(invitem) == THELM_HELM
                    || get_helmet_type(invitem) == THELM_HELMET)))
        {
            if (!quiet)
               mpr("This armour doesn't fit on your body.");

            return (false);
        }
    }

    bool removedCloak = false;
    int  cloak = -1;

    if (slot == EQ_BODY_ARMOUR
        && you.equip[EQ_CLOAK] != -1 && !cloak_is_being_removed())
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
                in_name(item, DESC_CAP_YOUR, info);
                strcat(info, " is stuck to your body!");
                mpr(info);
                return false;
            }
        }
    }

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

void throw_anything(void)
{
    struct bolt beam;
    int throw_slot;

    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }
    else if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    throw_slot = prompt_invent_item( "Throw which item?", MT_INVSELECT,
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
        for (int loopy = EQ_CLOAK; loopy <= EQ_AMULET; loopy++)
        {
            if (throw_slot == you.equip[loopy])
            {
                mpr("You are wearing that object!");
                return;
            }
        }
    }

    throw_it(beam, throw_slot);
}                               // end throw_anything()

// Return index of first valid balanced throwing weapon or ENDOFPACK
static int try_finding_throwing_weapon( int sub_type )
{
    int i;

    for (i = Options.fire_items_start; i < ENDOFPACK; i++)
    {
        // skip invalid objects, wielded object
        if (!is_valid_item( you.inv[i] ) || you.equip[EQ_WEAPON] == i)
            continue;

        // consider melee weapons that can also be thrown
        if (you.inv[i].base_type == OBJ_WEAPONS
            && you.inv[i].sub_type == sub_type)
        {
            break;
        }
    }

    return (i);
}

// Return index of first missile of sub_type or ENDOFPACK
static int try_finding_missile( int sub_type )
{
    int i;

    for (i = Options.fire_items_start; i < ENDOFPACK; i++)
    {
        // skip invalid objects
        if (!is_valid_item( you.inv[i] ))
            continue;

        // consider melee weapons that can also be thrown
        if (you.inv[i].base_type == OBJ_MISSILES
            && you.inv[i].sub_type == sub_type)
        {
            break;
        }
    }

    return (i);
}

// Note: This is a simple implementation, not an efficient one. -- bwr
//
// Returns item index or ENDOFPACK if no item found for auto-firing
int get_fire_item_index( void )
{
    int item = ENDOFPACK;
    const int weapon = you.equip[ EQ_WEAPON ];

    for (int i = 0; i < NUM_FIRE_TYPES; i++)
    {
        // look for next type on list... if found item is set != ENDOFPACK
        switch (Options.fire_order[i])
        {
        case FIRE_LAUNCHER:
            // check if we have ammo for a wielded launcher:
            if (weapon != -1 
                && you.inv[ weapon ].base_type == OBJ_WEAPONS
                && is_range_weapon( you.inv[ weapon ] ))
            {
                int type_wanted = fires_ammo_type( you.inv[ weapon ] );
                item = try_finding_missile( type_wanted );
            }
            break;

        case FIRE_DART:
            item = try_finding_missile( MI_DART );
            break;

        case FIRE_STONE:
            item = try_finding_missile( MI_STONE );
            break;

        case FIRE_DAGGER:
            item = try_finding_throwing_weapon( WPN_DAGGER );
            break;

        case FIRE_SPEAR:
            item = try_finding_throwing_weapon( WPN_SPEAR );
            break;

        case FIRE_HAND_AXE:
            item = try_finding_throwing_weapon( WPN_HAND_AXE );
            break;

        case FIRE_CLUB:
            item = try_finding_throwing_weapon( WPN_CLUB );
            break;

        case FIRE_NONE:
        default:
            break;
        }

        // if successful break
        if (item != ENDOFPACK)
            break;
    }

    // either item was found or is still ENDOFPACK for no item
    return (item);
}

void shoot_thing(void)
{
    struct bolt beam;  // passed in by reference, but never used here
    char str_pass[ ITEMNAME_SIZE ];

    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    const int item = get_fire_item_index();

    if (item == ENDOFPACK)
    {
        mpr("No suitable missiles.");
        return;
    }

    in_name( item, DESC_INVENTORY_EQUIP, str_pass );
    snprintf( info, INFO_SIZE, "Firing: %s", str_pass );
    mpr( info );

    throw_it( beam, item );
}                               // end shoot_thing()

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

// throw_it - currently handles player throwing only.  Monster
// throwing is handled in mstuff2:mons_throw()
// Note: If dummy_target is non-NULL, throw_it fakes a bolt and calls
// affect() on the monster's square.
//
// Return value is only relevant if dummy_target is non-NULL, and returns
// true if dummy_target is hit.
bool throw_it(struct bolt &pbolt, int throw_2, monsters *dummy_target)
{
    struct dist thr;
    char shoot_skill = 0;

    char wepClass, wepType;     // ammo class and type
    char lnchClass, lnchType;   // launcher class and type

    int baseHit = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;     // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;     // special add from launcher
    int exHitBonus = 0, exDamBonus = 0; // 'extra' bonus from skill/dex/str
    int effSkill = 0;           // effective launcher skill
    int dice_mult = 100;
    bool launched = false;      // item is launched
    bool thrown = false;        // item is sensible thrown item
    int slayDam = 0;

    if (dummy_target)
    {
        thr.isValid = true;
        thr.isCancel = false;
        thr.tx = dummy_target->x;
        thr.ty = dummy_target->y;
    }
    else
    {
        mpr( STD_DIRECTION_PROMPT, MSGCH_PROMPT );
        message_current_target();
        direction( thr, DIR_NONE, TARG_ENEMY );
    }

    if (!thr.isValid)
    {
        if (thr.isCancel)
            canned_msg(MSG_OK);

        return (false);
    }

    // Must unwield before fire_beam() makes a copy in order to remove things
    // like temporary branding. -- bwr
    if (throw_2 == you.equip[EQ_WEAPON] && you.inv[throw_2].quantity == 1)
    {
        unwield_item( throw_2 );
        you.equip[EQ_WEAPON] = -1;
        canned_msg( MSG_EMPTY_HANDED );
    }

    // Making a copy of the item: changed only for venom launchers
    item_def item = you.inv[throw_2];  
    item.quantity = 1;
    item.slot     = index_to_letter(item.link);
    origin_set_unknown(item);

    char str_pass[ ITEMNAME_SIZE ];

    if (you.conf)
    {
        thr.isTarget = true;
        thr.tx = you.x_pos + random2(13) - 6;
        thr.ty = you.y_pos + random2(13) - 6;
    }

    // even though direction is allowed, we're throwing so we
    // want to use tx, ty to make the missile fly to map edge.
    pbolt.set_target(thr);

    pbolt.flavour = BEAM_MISSILE;
    // pbolt.range is set below

    switch (item.base_type)
    {
    case OBJ_WEAPONS:    pbolt.type = SYM_WEAPON;  break;
    case OBJ_MISSILES:   pbolt.type = SYM_MISSILE; break;
    case OBJ_ARMOUR:     pbolt.type = SYM_ARMOUR;  break;
    case OBJ_WANDS:      pbolt.type = SYM_STICK;   break;
    case OBJ_FOOD:       pbolt.type = SYM_CHUNK;   break;
    case OBJ_UNKNOWN_I:  pbolt.type = SYM_BURST;   break;
    case OBJ_SCROLLS:    pbolt.type = SYM_SCROLL;  break;
    case OBJ_JEWELLERY:  pbolt.type = SYM_TRINKET; break;
    case OBJ_POTIONS:    pbolt.type = SYM_FLASK;   break;
    case OBJ_UNKNOWN_II: pbolt.type = SYM_ZAP;     break;
    case OBJ_BOOKS:      pbolt.type = SYM_OBJECT;  break;
        // this does not seem right, but value was 11 {dlb}
        // notice how the .type does not match the class -- hmmm... {dlb}
    case OBJ_STAVES:      pbolt.type = SYM_CHUNK;  break;
    }

    pbolt.source_x = you.x_pos;
    pbolt.source_y = you.y_pos;
    pbolt.colour = item.colour;

    item_name( item, DESC_PLAIN, str_pass );
    pbolt.name = str_pass;

    pbolt.thrower = KILL_YOU_MISSILE;
    pbolt.aux_source.clear();

    // get the ammo/weapon type.  Convenience.
    wepClass = item.base_type;
    wepType = item.sub_type;

    // get the launcher class,type.  Convenience.
    if (you.equip[EQ_WEAPON] < 0)
    {
        lnchClass = -1;
        // set lnchType to 0 so the 'figure out if launched'
        // code doesn't break
        lnchType = 0;
    }
    else
    {
        lnchClass = you.inv[you.equip[EQ_WEAPON]].base_type;
        lnchType = you.inv[you.equip[EQ_WEAPON]].sub_type;
    }

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
    throw_type(lnchClass, lnchType, wepClass, wepType, launched, thrown);

    // extract launcher bonuses due to magic
    if (launched)
    {
        lnchHitBonus = you.inv[you.equip[EQ_WEAPON]].plus;
        lnchDamBonus = you.inv[you.equip[EQ_WEAPON]].plus2;
    }

    // extract weapon/ammo bonuses due to magic
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    // CALCULATIONS FOR LAUNCHED WEAPONS
    if (launched)
    {
        const item_def &launcher = you.inv[you.equip[EQ_WEAPON]];        
        const int bow_brand = get_weapon_brand( launcher );
        const int ammo_brand = get_ammo_brand( item );
        bool poisoned = (ammo_brand == SPMSL_POISONED
                            || ammo_brand == SPMSL_POISONED_II);
        const int rc_skill = you.skills[SK_RANGED_COMBAT];

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

        // check for matches;  dwarven,elven,orcish
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

        // for all launched weapons, maximum effective specific skill
        // is twice throwing skill.  This models the fact that no matter
        // how 'good' you are with a bow, if you know nothing about
        // trajectories you're going to be a damn poor bowman.  Ditto
        // for crossbows and slings.

        // [dshaligram] Throwing now two parts launcher skill, one part
        // ranged combat. Removed the old model which is... silly.
        
        shoot_skill = you.skills[launcher_skill];
        effSkill = (shoot_skill * 2 + rc_skill) / 3;

        const int speed = launcher_final_speed(launcher, player_shield());
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Final launcher speed: %d", speed);
#endif
        you.time_taken = speed * you.time_taken / 100;

        // effSkill = you.skills[SK_RANGED_COMBAT] * 2 + 1;
        // effSkill = (shoot_skill > effSkill) ? effSkill : shoot_skill;

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

        // removed 2 random2(2)s from each of the learning curves, but
        // left slings because they're hard enough to develop without
        // a good source of shot in the dungeon.
        switch (launcher_skill)
        {
        case SK_SLINGS:
        {
            // Slings are really easy to learn because they're not
            // really all that good, and its harder to get ammo anyways.
            exercise(SK_SLINGS, 1 + random2avg(3, 2));
            baseHit += 0;
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

        // all launched weapons have a slight chance of improving
        // throwing skill
        if (coinflip())
            exercise(SK_RANGED_COMBAT, 1);

        // all launched weapons get a minor tohit boost from throwing skill.
        exHitBonus += you.skills[SK_RANGED_COMBAT] / 5;

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
            item_name( item, DESC_PLAIN, str_pass );
            pbolt.name = str_pass;
        }
        
        // Note that bow_brand is known since the bow is equiped.
        if ((bow_brand == SPWPN_FLAME || ammo_brand == SPMSL_FLAME)
            && ammo_brand != SPMSL_ICE && bow_brand != SPWPN_FROST)
        {
            // [dshaligram] Branded arrows are much stronger.
            dice_mult = dice_mult * 150 / 100;

            pbolt.flavour = BEAM_FIRE;
            pbolt.name = "bolt of ";

            if (poisoned)
                pbolt.name += "poison ";

            pbolt.name += "flame";
            pbolt.colour = RED;
            pbolt.type = SYM_BOLT;
            pbolt.thrower = KILL_YOU_MISSILE;
            pbolt.aux_source.clear();

            // ammo known if we can't attribute it to the bow
            if (bow_brand != SPWPN_FLAME)
            {
                set_ident_flags( item, ISFLAG_KNOW_TYPE );
                set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_TYPE );
            }
        }

        if ((bow_brand == SPWPN_FROST || ammo_brand == SPMSL_ICE)
            && ammo_brand != SPMSL_FLAME && bow_brand != SPWPN_FLAME)
        {
            // [dshaligram] Branded arrows are much stronger.
            dice_mult = dice_mult * 150 / 100;

            pbolt.flavour = BEAM_COLD;
            pbolt.name = "bolt of ";

            if (poisoned)
                pbolt.name += "poison ";

            pbolt.name += "frost";
            pbolt.colour = WHITE;
            pbolt.type = SYM_BOLT;
            pbolt.thrower = KILL_YOU_MISSILE;
            pbolt.aux_source.clear();

            // ammo known if we can't attribute it to the bow
            if (bow_brand != SPWPN_FROST)
            {
                set_ident_flags( item, ISFLAG_KNOW_TYPE );
                set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_TYPE );
            }
        }

        // ammo known if it cancels the effect of the bow
        if ((bow_brand == SPWPN_FLAME && ammo_brand == SPMSL_ICE)
            || (bow_brand == SPWPN_FROST && ammo_brand == SPMSL_FLAME))
        {
            set_ident_flags( item, ISFLAG_KNOW_TYPE );
            set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_TYPE );
        }

        /* the chief advantage here is the extra damage this does
         * against susceptible creatures */

        /* Note: weapons & ammo of eg fire are not cumulative
         * ammo of fire and weapons of frost don't work together,
         * and vice versa */

        // ID check
        if (item_ident(you.inv[you.equip[EQ_WEAPON]], ISFLAG_KNOW_PLUSES))
        {
            if ( !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES) &&
                 random2(100) < rc_skill )
            { 
                set_ident_flags( item, ISFLAG_KNOW_PLUSES );
                set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_PLUSES );
                in_name( throw_2, DESC_NOCAP_A, str_pass);
                snprintf(info, INFO_SIZE, "You are firing %s.", str_pass);
                mpr(info);
            }
        }
        else if (random2(100) < shoot_skill)
        {
            set_ident_flags(you.inv[you.equip[EQ_WEAPON]], ISFLAG_KNOW_PLUSES);

            strcpy(info, "You are wielding ");
            in_name(you.equip[EQ_WEAPON], DESC_NOCAP_A, str_pass);
            strcat(info, str_pass);
            strcat(info, ".");
            mpr(info);

            more();
            you.wield_change = true;
        }

    }

    // CALCULATIONS FOR THROWN WEAPONS
    if (thrown)
    {
        baseHit = 0;

        // since darts/rocks are missiles, they only use inv_plus
        if (wepClass == OBJ_MISSILES)
            ammoDamBonus = ammoHitBonus;

        // all weapons that use 'throwing' go here..
        if (wepClass == OBJ_WEAPONS
            || (wepClass == OBJ_MISSILES && wepType == MI_STONE))
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

            exHitBonus = you.skills[SK_RANGED_COMBAT] * 2;

            baseDam = property( item, PWPN_DAMAGE );
            exDamBonus =
                (10 * (you.skills[SK_RANGED_COMBAT] / 2 + you.strength - 10)) / 12;

            // now, exDamBonus is a multiplier.  The full multiplier
            // is applied to base damage, but only a third is applied
            // to the magical modifier.
            exDamBonus = (exDamBonus * (3 * baseDam + ammoDamBonus)) / 30;
        }

        if (wepClass == OBJ_MISSILES && wepType == MI_DART)
        {
            // give an appropriate 'tohit' & damage
            baseHit = 2;
            baseDam = property( item, PWPN_DAMAGE );

            exHitBonus = you.skills[SK_DARTS] * 2;
            exHitBonus += (you.skills[SK_RANGED_COMBAT] * 2) / 3;
            exDamBonus = you.skills[SK_DARTS] / 3;
            exDamBonus += you.skills[SK_RANGED_COMBAT] / 5;

            // exercise skills
            exercise(SK_DARTS, 1 + random2avg(3, 2));
        }

        // [dshaligram] The defined base damage applies only when used
        // for launchers. Hand-thrown stones and darts do only half
        // base damage. Yet another evil 4.0ism.
        if (wepClass == OBJ_MISSILES
                && (wepType == MI_DART || wepType == MI_STONE))
            baseDam = div_rand_round(baseDam, 2);
        
        // exercise skill
        if (coinflip())
            exercise(SK_RANGED_COMBAT, 1);
        
        // ID check
        if ( !item_ident(you.inv[throw_2], ISFLAG_KNOW_PLUSES) &&
             random2(100) < you.skills[SK_RANGED_COMBAT] )
        { 
            set_ident_flags( item, ISFLAG_KNOW_PLUSES );
            set_ident_flags( you.inv[throw_2], ISFLAG_KNOW_PLUSES );
            in_name( throw_2, DESC_NOCAP_A, str_pass);
            snprintf(info, INFO_SIZE, "You are throwing %s.", str_pass);
            mpr(info);
        }
    }

    // range, dexterity bonus, possible skill increase for silly throwing
    if (thrown || launched)
    {
        if (wepType == MI_LARGE_ROCK)
        {
            pbolt.range = 1 + random2( you.strength / 5 );
            if (pbolt.range > 9)
                pbolt.range = 9;

            pbolt.rangeMax = pbolt.range;
        }
        else
        {
            pbolt.range = 9;
            pbolt.rangeMax = 9;

            exHitBonus += you.dex / 2;

            // slaying bonuses
            if (!(launched && wepType == MI_NEEDLE))
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
            exercise(SK_RANGED_COMBAT, 1);

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

    scale_dice( pbolt.damage );

    // only add bonuses if we're throwing something sensible
    if (thrown || launched || wepClass == OBJ_WEAPONS)
    {
        pbolt.hit += ammoHitBonus + lnchHitBonus;
        pbolt.damage.size += ammoDamBonus + lnchDamBonus;
    }

#if DEBUG_DIAGNOSTICS
    mprf( MSGCH_DIAGNOSTICS,
            "H:%d+%d;a%dl%d.  D:%d+%d;a%dl%d -> %d,%dd%d",
              baseHit, exHitBonus, ammoHitBonus, lnchHitBonus,
              baseDam, exDamBonus, ammoDamBonus, lnchDamBonus,
              pbolt.hit, pbolt.damage.num, pbolt.damage.size );
#endif

    // create message
    if (launched)
        strcpy(info, "You shoot ");
    else
        strcpy(info, "You throw ");

    item_name( item, DESC_NOCAP_A, str_pass );

    strcat(info, str_pass);
    strcat(info, ".");
    mpr(info);

    // ensure we're firing a 'missile'-type beam
    pbolt.is_beam = false;
    pbolt.is_tracer = false;

    // mark this item as thrown if it's a missile, so that we'll pick it up
    // when we walk over it.
    if (wepClass == OBJ_MISSILES || wepClass == OBJ_WEAPONS)
        item.flags |= ISFLAG_THROWN;

    bool hit = false;
    // using copy, since the launched item might be differect (venom blowgun)
    if (dummy_target)
        hit = (affect( pbolt, dummy_target->x, dummy_target->y ) != 0);
    else
    {
        fire_beam( pbolt, &item );
        dec_inv_item_quantity( throw_2, 1 );
    }

    // throwing and blowguns are silent
    if (launched && lnchType != WPN_BLOWGUN)
        noisy( 6, you.x_pos, you.y_pos );

    // but any monster nearby can see that something has been thrown:
    alert_nearby_monsters();

    you.turn_is_over = true;

    return (hit);
}                               // end throw_it()

void jewellery_wear_effects(item_def &item)
{
    int ident = ID_TRIED_TYPE;

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
    case RING_TELEPORTATION:
    case RING_WIZARDRY:
    case RING_REGENERATION:
    case RING_TELEPORT_CONTROL:
        break;

    case RING_PROTECTION:
        you.redraw_armour_class = 1;
        if (item.plus != 0)
            ident = ID_KNOWN_TYPE;
        break;

    case RING_INVISIBILITY:
        if (!you.invis)
        {
            mpr("You become transparent for a moment.");
            ident = ID_KNOWN_TYPE;
        }
        break;

    case RING_EVASION:
        you.redraw_evasion = 1;
        if (item.plus != 0)
            ident = ID_KNOWN_TYPE;
        break;

    case RING_STRENGTH:
        modify_stat(STAT_STRENGTH, item.plus, true);
        if (item.plus != 0)
            ident = ID_KNOWN_TYPE;
        break;

    case RING_DEXTERITY:
        modify_stat(STAT_DEXTERITY, item.plus, true);
        if (item.plus != 0)
            ident = ID_KNOWN_TYPE;
        break;

    case RING_INTELLIGENCE:
        modify_stat(STAT_INTELLIGENCE, item.plus, true);
        if (item.plus != 0)
            ident = ID_KNOWN_TYPE;
        break;

    case RING_MAGICAL_POWER:
        calc_mp();
        ident = ID_KNOWN_TYPE;
        break;

    case RING_LEVITATION:
        mpr("You feel buoyant.");
        ident = ID_KNOWN_TYPE;
        break;

    case AMU_RAGE:
        mpr("You feel a brief urge to hack something to bits.");
        ident = ID_KNOWN_TYPE;
        break;

    case AMU_THE_GOURMAND:
        you.duration[DUR_GOURMAND] = 0;
        break;
    }

    // Artefacts have completely different appearance than base types
    // so we don't allow them to make the base types known
    if (is_random_artefact( item ))
        use_randart(item);
    else
        set_ident_type( item.base_type, item.sub_type, ident );

    if (ident == ID_KNOWN_TYPE)
        set_ident_flags( item, ISFLAG_EQ_JEWELLERY_MASK );

    if (item_cursed( item ))
        mprf("Oops, that %s feels deathly cold.", 
                jewellery_is_amulet(item)? "amulet" : "ring");

    // cursed or not, we know that since we've put the ring on
    set_ident_flags( item, ISFLAG_KNOW_CURSE );

    mpr( item_name( item, DESC_INVENTORY_EQUIP ) );
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
    mprf("Wearing %s.", in_name(new_ring, DESC_NOCAP_A));

    char lslot = index_to_letter(left.link),
         rslot = index_to_letter(right.link);

    mprf(MSGCH_PROMPT,
            "You're wearing two rings. Remove which one? (%c/%c/Esc)",
            lslot, rslot);
    mprf(" %s", item_name( left, DESC_INVENTORY ));
    mprf(" %s", item_name( right, DESC_INVENTORY ));

    int c;
    do
        c = getch();
    while (c != lslot && c != rslot && c != ESCAPE && c != ' ');

    mesclr();

    if (c == ESCAPE || c == ' ')
        return (-1);

    int eqslot = c == lslot? EQ_LEFT_RING : EQ_RIGHT_RING;
    return (you.equip[eqslot]);
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
        return (false);

    if (!remove_ring(unwanted, false))
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

    bool is_amulet = jewellery_is_amulet( you.inv[item_slot] );

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
        if (!remove_ring( you.equip[EQ_AMULET], true ))
            return (false);

        start_delay(DELAY_JEWELLERY_ON, 1, item_slot);

        // Assume it's going to succeed.
        return (true);
    }

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

    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (slot != -1)
        item_slot = slot;
    else
        item_slot = prompt_invent_item( "Put on which piece of jewellery?",
                        MT_INVSELECT, OBJ_JEWELLERY );

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
    bool old_showuncursed = Options.show_uncursed;
    Options.show_uncursed = false;

    mprf("You remove %s.", item_name(item, DESC_NOCAP_YOUR));

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
        modify_stat(STAT_STRENGTH, -item.plus, true);
        break;

    case RING_DEXTERITY:
        modify_stat(STAT_DEXTERITY, -item.plus, true);
        break;

    case RING_INTELLIGENCE:
        modify_stat(STAT_INTELLIGENCE, -item.plus, true);
        break;

    case RING_INVISIBILITY:
        // removing this ring effectively cancels all invisibility {dlb}
        if (you.invis)
            you.invis = 1;
        break;

    case RING_LEVITATION:
        // removing this ring effectively cancels all levitation {dlb}
        if (you.levitation)
            you.levitation = 1;
        break;

    case RING_MAGICAL_POWER:
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
    int hand_used = 10;
    int ring_wear_2;

    if (you.equip[EQ_LEFT_RING] == -1 && you.equip[EQ_RIGHT_RING] == -1
        && you.equip[EQ_AMULET] == -1)
    {
        mpr("You aren't wearing any rings or amulets.");
        return (false);
    }

    if (you.berserker)
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
        hand_used = 0;
    }

    if (you.equip[EQ_LEFT_RING] == -1 && you.equip[EQ_RIGHT_RING] != -1
        && you.equip[EQ_AMULET] == -1)
    {
        hand_used = 1;
    }

    if (you.equip[EQ_LEFT_RING] == -1 && you.equip[EQ_RIGHT_RING] == -1
        && you.equip[EQ_AMULET] != -1)
    {
        hand_used = 2;
    }

    if (hand_used == 10)
    {
        int equipn = 
            slot == -1? prompt_invent_item( "Remove which piece of jewellery?",
                                            MT_INVSELECT,
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
            hand_used = 0;
        else if (you.equip[EQ_RIGHT_RING] == equipn)
            hand_used = 1;
        else if (you.equip[EQ_AMULET] == equipn)
            hand_used = 2;
        else
        {
            mpr("You aren't wearing that.");
            return (false);
        }
    }

    if (you.equip[EQ_GLOVES] != -1 
        && item_cursed( you.inv[you.equip[EQ_GLOVES]] )
        && (hand_used == 0 || hand_used == 1))
    {
        mpr("You can't take your gloves off to remove any rings!");
        return (false);
    }

    if (you.equip[hand_used + EQ_LEFT_RING] == -1)
    {
        mpr("I don't think you really meant that.");
        return (false);
    }

    if (item_cursed( you.inv[you.equip[hand_used + EQ_LEFT_RING]] ))
    {
        if (announce)
            mprf("%s is stuck to you!",
                in_name(you.equip[hand_used + EQ_LEFT_RING], 
                        DESC_CAP_YOUR));
        else
            mpr("It's stuck to you!");

        set_ident_flags( you.inv[you.equip[hand_used + EQ_LEFT_RING]], 
                         ISFLAG_KNOW_CURSE );
        return (false);
    }

    ring_wear_2 = you.equip[hand_used + EQ_LEFT_RING];
    you.equip[hand_used + EQ_LEFT_RING] = -1;

    jewellery_remove_effects(you.inv[ring_wear_2]);

    you.time_taken = you.time_taken * 5 / 10;
    you.turn_is_over = true;

    return (true);
}                               // end remove_ring()

void zap_wand(void)
{
    struct bolt beam;
    struct dist zap_wand;
    int item_slot;
    char str_pass[ ITEMNAME_SIZE ];

    // Unless the character knows the type of the wand, the targeting
    // system will default to cycling through all monsters. -- bwr
    int targ_mode = TARG_ANY;

    beam.obvious_effect = false;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    item_slot = prompt_invent_item( "Zap which item?",
                                    MT_INVSELECT,
                                    OBJ_WANDS,
				    true, true, true, 0, NULL,
				    OPER_ZAP );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (you.inv[item_slot].base_type != OBJ_WANDS)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        you.turn_is_over = true;
        return;
    }

    if ( you.inv[item_slot].plus < 1 ) {
	// it's an empty wand, inscribe it that way
        canned_msg(MSG_NOTHING_HAPPENS);
        you.turn_is_over = true;
	if ( !item_ident(you.inv[item_slot], ISFLAG_KNOW_PLUSES) ) {

	    if ( you.inv[item_slot].inscription.find("empty") ==
		 std::string::npos ) {

		if ( !you.inv[item_slot].inscription.empty() )
		    you.inv[item_slot].inscription += ' ';
		you.inv[item_slot].inscription += "[empty]";
		
	    }
	}
	return;
    }

    if (item_ident( you.inv[item_slot], ISFLAG_KNOW_TYPE ))
    {
        if (you.inv[item_slot].sub_type == WAND_HASTING
            || you.inv[item_slot].sub_type == WAND_HEALING
            || you.inv[item_slot].sub_type == WAND_INVISIBILITY)
        {
            targ_mode = TARG_FRIEND;
        }
        else 
        {
            targ_mode = TARG_ENEMY;
        }
    }

    mpr( STD_DIRECTION_PROMPT, MSGCH_PROMPT );
    message_current_target();

    direction( zap_wand, DIR_NONE, targ_mode );

    if (!zap_wand.isValid)
    {
        if (zap_wand.isCancel)
            canned_msg(MSG_OK);
        return;
    }

    if (you.conf)
    {
        zap_wand.tx = you.x_pos + random2(13) - 6;
        zap_wand.ty = you.y_pos + random2(13) - 6;
    }

    // blargh! blech! this is just begging to be a problem ...
    // not to mention work-around after work-around as wands are
    // added, removed, or altered {dlb}:
    char type_zapped = you.inv[item_slot].sub_type;

    if (type_zapped == WAND_ENSLAVEMENT)
        type_zapped = ZAP_ENSLAVEMENT;

    if (type_zapped == WAND_DRAINING)
        type_zapped = ZAP_NEGATIVE_ENERGY;

    if (type_zapped == WAND_DISINTEGRATION)
        type_zapped = ZAP_DISINTEGRATION;

    if (type_zapped == WAND_RANDOM_EFFECTS)
    {
        type_zapped = random2(16);
        if (one_chance_in(20))
            type_zapped = ZAP_NEGATIVE_ENERGY;
        if (one_chance_in(17))
            type_zapped = ZAP_ENSLAVEMENT;
    }

    beam.source_x = you.x_pos;
    beam.source_y = you.y_pos;
    beam.set_target(zap_wand);

    zapping( type_zapped, 30 + roll_dice(2, you.skills[SK_EVOCATIONS]), beam );

    if (beam.obvious_effect == 1 || you.inv[item_slot].sub_type == WAND_FIREBALL)
    {
        if (get_ident_type( you.inv[item_slot].base_type, 
                            you.inv[item_slot].sub_type ) != ID_KNOWN_TYPE)
        {
            set_ident_type( you.inv[item_slot].base_type, 
                            you.inv[item_slot].sub_type, ID_KNOWN_TYPE );

            in_name(item_slot, DESC_INVENTORY_EQUIP, str_pass);
            mpr( str_pass );

            // update if wielding
            if (you.equip[EQ_WEAPON] == item_slot)
                you.wield_change = true;
        }
    }
    else
    {
        set_ident_type( you.inv[item_slot].base_type, 
                        you.inv[item_slot].sub_type, ID_TRIED_TYPE );
    }

    you.inv[item_slot].plus--;

    if (get_ident_type( you.inv[item_slot].base_type, 
                        you.inv[item_slot].sub_type ) == ID_KNOWN_TYPE  
        && (item_ident( you.inv[item_slot], ISFLAG_KNOW_PLUSES )
            || you.skills[SK_EVOCATIONS] > 5 + random2(15)))
    {
        if (!item_ident( you.inv[item_slot], ISFLAG_KNOW_PLUSES ))
        {
            mpr("Your skill with magical items lets you calculate the power of this device...");
        }

        snprintf( info, INFO_SIZE, "This wand has %d charge%s left.",
                 you.inv[item_slot].plus, 
                 (you.inv[item_slot].plus == 1) ? "" : "s" );

        mpr(info);
        set_ident_flags( you.inv[item_slot], ISFLAG_KNOW_PLUSES );
    }

    exercise( SK_EVOCATIONS, 1 );
    alert_nearby_monsters();

    you.turn_is_over = true;
}                               // end zap_wand()

/*** HP CHANGE ***/
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
                    MT_INVSELECT, 
                    OSEL_ANY );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }
    mpr( "Inscribe with what? ", MSGCH_PROMPT );
    get_input_line( buf, sizeof(buf) );
    you.inv[item_slot].inscription = std::string(buf);
    you.wield_change = true;
}

void drink(void)
{
    int item_slot;

    if (you.is_undead == US_UNDEAD)
    {
        mpr("You can't drink.");
        return;
    }

    if (grd[you.x_pos][you.y_pos] == DNGN_BLUE_FOUNTAIN
        || grd[you.x_pos][you.y_pos] == DNGN_SPARKLING_FOUNTAIN)
    {
        if (drink_fountain())
            return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    item_slot = prompt_invent_item( "Drink which item?",
                                    MT_INVSELECT, OBJ_POTIONS,
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

    if (potion_effect( you.inv[item_slot].sub_type, 40 ))
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

    dec_inv_item_quantity( item_slot, 1 );
    you.turn_is_over = true;

    lessen_hunger(40, true);
}                               // end drink()

bool drink_fountain(void)
{
    bool gone_dry = false;
    int temp_rand;              // for probability determinations {dlb}
    int fountain_effect = POT_WATER;    // for fountain effects {dlb}

    switch (grd[you.x_pos][you.y_pos])
    {
    case DNGN_BLUE_FOUNTAIN:
        if (!yesno("Drink from the fountain?"))
            return false;

        mpr("You drink the pure, clear water.");
        break;

    case DNGN_SPARKLING_FOUNTAIN:
        if (!yesno("Drink from the sparkling fountain?"))
            return false;

        mpr("You drink the sparkling water.");
        break;
    }

    if (grd[you.x_pos][you.y_pos] == DNGN_SPARKLING_FOUNTAIN)
    {
        temp_rand = random2(4500);

        fountain_effect = ((temp_rand > 2399) ? POT_WATER :     // 46.7%
                           (temp_rand > 2183) ? POT_DECAY :     //  4.8%
                           (temp_rand > 2003) ? POT_MUTATION :  //  4.0%
                           (temp_rand > 1823) ? POT_HEALING :   //  4.0%
                           (temp_rand > 1643) ? POT_HEAL_WOUNDS :// 4.0%
                           (temp_rand > 1463) ? POT_SPEED :     //  4.0%
                           (temp_rand > 1283) ? POT_MIGHT :     //  4.0%
                           (temp_rand > 1139) ? POT_DEGENERATION ://3.2%
                           (temp_rand > 1019) ? POT_LEVITATION ://  2.7%
                           (temp_rand > 899) ? POT_POISON :     //  2.7%
                           (temp_rand > 779) ? POT_SLOWING :    //  2.7%
                           (temp_rand > 659) ? POT_PARALYSIS :  //  2.7%
                           (temp_rand > 539) ? POT_CONFUSION :  //  2.7%
                           (temp_rand > 419) ? POT_INVISIBILITY :// 2.7%
                           (temp_rand > 329) ? POT_MAGIC :      //  2.0%
                           (temp_rand > 239) ? POT_RESTORE_ABILITIES ://  2.0%
                           (temp_rand > 149) ? POT_STRONG_POISON ://2.0%
                           (temp_rand > 59) ? POT_BERSERK_RAGE :  //2.0%
                           (temp_rand > 39) ? POT_GAIN_STRENGTH : //0.4%
                           (temp_rand > 19) ? POT_GAIN_DEXTERITY  //0.4%
                                            : POT_GAIN_INTELLIGENCE);//0.4%
    }

    potion_effect(fountain_effect, 100);

    switch (grd[you.x_pos][you.y_pos])
    {
    case DNGN_BLUE_FOUNTAIN:
        if (one_chance_in(20))
            gone_dry = true;
        break;

    case DNGN_SPARKLING_FOUNTAIN:
        if (one_chance_in(10))
        {
            gone_dry = true;
            break;
        }
        else
        {
            temp_rand = random2(50);

            // you won't know it (yet)
            if (temp_rand > 40) // 18% probability
                grd[you.x_pos][you.y_pos] = DNGN_BLUE_FOUNTAIN;
        }
        break;
    }

    if (gone_dry)
    {
        mpr("The fountain dries up!");
        if (grd[you.x_pos][you.y_pos] == DNGN_BLUE_FOUNTAIN)
            grd[you.x_pos][you.y_pos] = DNGN_DRY_FOUNTAIN_I;
        else if (grd[you.x_pos][you.y_pos] == DNGN_SPARKLING_FOUNTAIN)
            grd[you.x_pos][you.y_pos] = DNGN_DRY_FOUNTAIN_II;
    }

    you.turn_is_over = true;
    return true;
}                               // end drink_fountain()

static bool affix_weapon_enchantment( void )
{
    const int wpn = you.equip[ EQ_WEAPON ];
    bool success = true;

    struct bolt beam;

    if (wpn == -1 || !you.duration[ DUR_WEAPON_BRAND ])
        return (false);

    switch (get_weapon_brand( you.inv[wpn] ))
    {
    case SPWPN_VORPAL:
        if (get_vorpal_type( you.inv[wpn] ) != DVORP_CRUSHING)
        {
            strcat(info, "'s sharpness seems more permanent.");
        }
        else
        {
            strcat(info, "'s heaviness feels very stable.");
        }
        mpr(info);
        break;

    case SPWPN_FLAMING:
        strcat(info," is engulfed in an explosion of flames!");
        mpr(info);

        beam.type = SYM_BURST;
        beam.damage = dice_def( 3, 10 );
        beam.flavour = 2;
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
        strcat(info," glows brilliantly blue for a moment.");
        mpr(info);
        cast_refrigeration(60);
        break;

    case SPWPN_DRAINING:
        strcat(info," thirsts for the lives of mortals!");
        mpr(info);
        drain_exp();
        break;

    case SPWPN_VENOM:
        strcat(info, " seems more permanently poisoned.");
        mpr(info);
        cast_toxic_radiance();
        break;

    case SPWPN_DISTORTION:
        // [dshaligram] Attempting to fix a distortion brand gets you a free
        // distortion effect, and no permabranding. Sorry, them's the breaks.
        strcat(info, " twongs alarmingly.");
        mpr(info);

        // from unwield_item
        miscast_effect( SPTYP_TRANSLOCATION, 9, 90, 100, 
                        "a distortion effect" );
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

bool enchant_weapon( int which_stat, bool quiet )
{
    const int wpn = you.equip[ EQ_WEAPON ];
    bool affected = true;
    int enchant_level;
    char str_pass[ ITEMNAME_SIZE ];

    if (wpn == -1 
        || (you.inv[ wpn ].base_type != OBJ_WEAPONS
            && you.inv[ wpn ].base_type != OBJ_MISSILES))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_HAPPENS);

        return (false);
    }

    you.wield_change = true;

    // missiles only have one stat
    if (you.inv[ wpn ].base_type == OBJ_MISSILES)
        which_stat = ENCHANT_TO_HIT;

    if (which_stat == ENCHANT_TO_HIT)
        enchant_level = you.inv[ wpn ].plus;
    else
        enchant_level = you.inv[ wpn ].plus2;

    // artefacts can't be enchanted, but scrolls still remove curses
    if (you.inv[ wpn ].base_type == OBJ_WEAPONS
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
        if (item_cursed( you.inv[you.equip[EQ_WEAPON]] ))
        {
            if (!quiet)
            {
                in_name(you.equip[EQ_WEAPON], DESC_CAP_YOUR, str_pass);
                strcpy(info, str_pass);
                strcat(info, " glows silver for a moment.");
                mpr(info);
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

    // vVvVv    This is *here* (as opposed to lower down) for a reason!
    in_name( wpn, DESC_CAP_YOUR, str_pass );
    strcpy( info, str_pass );

    do_uncurse_item( you.inv[ wpn ] );

    if (you.inv[ wpn ].base_type == OBJ_WEAPONS)
    {
        if (which_stat == ENCHANT_TO_DAM)
        {
            you.inv[ wpn ].plus2++;

            if (!quiet)
            {
                strcat(info, " glows red for a moment.");
                mpr(info);
            }
        }
        else if (which_stat == ENCHANT_TO_HIT)
        {
            you.inv[ wpn ].plus++;

            if (!quiet)
            {
                strcat(info, " glows green for a moment.");
                mpr(info);
            }
        }
    }
    else if (you.inv[ wpn ].base_type == OBJ_MISSILES)
    {
        strcat( info, (you.inv[ wpn ].quantity > 1) ? " glow"
                                                    : " glows" );

        strcat(info, " red for a moment.");

        you.inv[ wpn ].plus++;
    }

    return (true);
}

static bool enchant_armour( void )
{
    // NOTE: It is assumed that armour which changes in this way does
    // not change into a form of armour with a different evasion modifier.
    char str_pass[ ITEMNAME_SIZE ];
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
        in_name( you.equip[EQ_BODY_ARMOUR], DESC_CAP_YOUR, str_pass );
        strcpy(info, str_pass);
        strcat(info, " glows purple and changes!");
        mpr(info);

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
            item_name(item, DESC_CAP_YOUR, str_pass);
            strcpy(info, str_pass);
            strcat(info, " glows silver for a moment.");
            mpr(info);

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
    item_name(item, DESC_CAP_YOUR, str_pass);
    strcpy(info, str_pass);
    strcat(info, " glows green for a moment.");
    mpr(info);

    item.plus++;

    do_uncurse_item( item );
    you.redraw_armour_class = 1;
    return (true);
}

static void handle_read_book( int item_slot )
{
    int spell, spell_index, nthing;

    if (you.inv[item_slot].sub_type == BOOK_DESTRUCTION)
    {
        if (silenced(you.x_pos, you.y_pos))
        {
            mpr("This book does not work if you cannot read it aloud!");
            return;
        }

        tome_of_power(item_slot);
        return;
    }
    else if (you.inv[item_slot].sub_type == BOOK_MANUAL)
    {
        skill_manual(item_slot);
        return;
    }
    else
    {
        // Spellbook
        spell = read_book( you.inv[item_slot], RBOOK_READ_SPELL );
    }

    if (spell < 'a' || spell > 'h')     //jmf: was 'g', but 8=h
    {
        mesclr( true );
        return;
    }

    spell_index = letter_to_index( spell );

    nthing = which_spell_in_book(you.inv[item_slot].sub_type, spell_index);
    if (nthing == SPELL_NO_SPELL)
    {
        mesclr( true );
        return;
    }

    describe_spell( nthing );
    redraw_screen();

    mesclr( true );
    return;
}

void read_scroll(void)
{
    int affected = 0;
    int i;
    int count;
    int nthing;
    struct bolt beam;
    char str_pass[ ITEMNAME_SIZE ];

    // added: scroll effects are never tracers.
    beam.is_tracer = false;

    if (you.berserker)
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    int item_slot = prompt_invent_item(
                        "Read which item?", 
                        MT_INVSELECT, 
                        OBJ_SCROLLS );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    if (you.inv[item_slot].base_type != OBJ_BOOKS
        && you.inv[item_slot].base_type != OBJ_SCROLLS)
    {
        mpr("You can't read that!");
        return;
    }

    // here we try to read a book {dlb}:
    if (you.inv[item_slot].base_type == OBJ_BOOKS)
    {
        handle_read_book( item_slot );
        return;
    }

    if (silenced(you.x_pos, you.y_pos))
    {
        mpr("Magic scrolls do not work when you're silenced!");
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
    const int scroll_type = you.inv[item_slot].sub_type;
    if (scroll_type != SCR_PAPER)
    {
        mpr("As you read the scroll, it crumbles to dust.");
        // Actual removal of scroll done afterwards. -- bwr
    }

    // scrolls of paper are also exempted from this handling {dlb}:
    if (scroll_type != SCR_PAPER)
    {
        if (you.conf)
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
        blink();
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
        acquirement(OBJ_RANDOM, AQ_SCROLL);
        break;

    case SCR_FEAR:
        if (!mass_enchantment(ENCH_FEAR, 1000, MHITYOU))
            id_the_scroll = false;
        break;

    case SCR_NOISE:
        mpr("You hear a loud clanging noise!");
        noisy( 25, you.x_pos, you.y_pos );
        break;

    case SCR_SUMMONING:
        if (create_monster( MONS_ABOMINATION_SMALL, ENCH_ABJ_VI, BEH_FRIENDLY,
                            you.x_pos, you.y_pos, you.pet_target, 250 ) != -1)
        {
            mpr("A horrible Thing appears!");
        }
        break;

    case SCR_FORGETFULNESS:
        mpr("You feel momentarily disoriented.");
        if (!wearing_amulet(AMU_CLARITY))
            forget_map(50 + random2(50));
        break;

    case SCR_MAGIC_MAPPING:
        if (you.level_type == LEVEL_LABYRINTH
            || you.level_type == LEVEL_ABYSS)
        {
            mpr("You feel momentarily disoriented.");
            id_the_scroll = false;
        }
        else
        {
            mpr("You feel aware of your surroundings.");
            magic_mapping(50, 90 + random2(11));
        }
        break;

    case SCR_TORMENT:
        torment( TORMENT_SCROLL, you.x_pos, you.y_pos );

        // is only naughty if you know you're doing it
        if (get_ident_type( OBJ_SCROLLS, SCR_TORMENT ) == ID_KNOWN_TYPE)
        {
            did_god_conduct(DID_UNHOLY, 10);
        }
        break;

    case SCR_IMMOLATION:
        mpr("The scroll explodes in your hands!");
	// we do this here to prevent it from blowing itself up
        dec_inv_item_quantity( item_slot, 1 );

        beam.type = SYM_BURST;
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

        explosion(beam);
        break;

    case SCR_IDENTIFY:
        if ( get_ident_type( OBJ_SCROLLS, scroll_type ) != ID_KNOWN_TYPE ) {
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
            in_name( nthing, DESC_CAP_YOUR, str_pass );
            strcpy(info, str_pass);
            strcat(info, " glows black for a moment.");
            mpr(info);

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
            if (!affix_weapon_enchantment())
            {
                in_name( you.equip[EQ_WEAPON], DESC_CAP_YOUR, str_pass );
                strcpy( info, str_pass );
                strcat( info, " glows bright yellow for a while." );
                mpr( info );

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

        in_name(nthing, DESC_CAP_YOUR, str_pass);

        strcpy(info, str_pass);
        strcat(info, " emits a brilliant flash of light!");
        mpr(info);

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
            id_the_scroll = !enchant_weapon( ENCHANT_TO_DAM );
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

        // make the name _before_ we curse it
        in_name( you.equip[affected], DESC_CAP_YOUR, str_pass );
        do_curse_item( you.inv[you.equip[affected]] );

        strcpy(info, str_pass);
        strcat(info, " glows black for a moment.");
        mpr(info);
        break;
    }                           // end switch

    // finally, destroy and identify the scroll
    // scrolls of immolation were already destroyed earlier
    if (scroll_type != SCR_PAPER && scroll_type != SCR_IMMOLATION)
    {
        dec_inv_item_quantity( item_slot, 1 );
    }

    set_ident_type( OBJ_SCROLLS, scroll_type, 
                    (id_the_scroll) ? ID_KNOWN_TYPE : ID_TRIED_TYPE );
}                               // end read_scroll()

void examine_object(void)
{
    int item_slot = prompt_invent_item( "Examine which item?", 
                                        MT_INVSELECT, -1,
					true, true, true, 0, NULL,
					OPER_EXAMINE );
    if (item_slot == PROMPT_ABORT)
    {
        canned_msg( MSG_OK );
        return;
    }

    describe_item( you.inv[item_slot] );
    redraw_screen();
    mesclr(true);
}                               // end original_name()

void use_randart(unsigned char item_wield_2)
{
    use_randart( you.inv[ item_wield_2 ] );
}

void use_randart(const item_def &item)
{
    ASSERT( is_random_artefact( item ) );

    FixedVector< char, RA_PROPERTIES >  proprt;
    randart_wpn_properties( item, proprt );

    if (proprt[RAP_AC])
        you.redraw_armour_class = 1;

    if (proprt[RAP_EVASION])
        you.redraw_evasion = 1;

    // modify ability scores
    modify_stat( STAT_STRENGTH,     proprt[RAP_STRENGTH],     true );
    modify_stat( STAT_INTELLIGENCE, proprt[RAP_INTELLIGENCE], true );
    modify_stat( STAT_DEXTERITY,    proprt[RAP_DEXTERITY],    true );

    if (proprt[RAP_NOISES])
        you.special_wield = 50 + proprt[RAP_NOISES];
}
