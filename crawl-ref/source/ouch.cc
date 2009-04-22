/*
 *  File:       ouch.cc
 *  Summary:    Functions used when Bad Things happen to the player.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <string.h>
#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#ifdef DOS
#include <conio.h>
#include <file.h>
#endif

#ifdef UNIX
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "ouch.h"

#ifdef __MINGW32__
#include <io.h>
#endif

#include "externs.h"

#include "chardump.h"
#include "delay.h"
#include "files.h"
#include "fight.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "monstuff.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spells4.h"
#include "state.h"
#include "stuff.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"


static void end_game( scorefile_entry &se );
static void _item_corrode(int slot);


// NOTE: DOES NOT check for hellfire!!!
int check_your_resists(int hurted, beam_type flavour)
{
    int resist;
    int original = hurted;

#if DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "checking resistance: flavour=%d", flavour );
#endif

    if (flavour == BEAM_FIRE || flavour == BEAM_LAVA
        || flavour == BEAM_HELLFIRE || flavour == BEAM_FRAG)
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
            remove_condensation_shield();
    }

    switch (flavour)
    {
    case BEAM_STEAM:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_steam(), hurted, true);
        if (hurted < original)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original)
        {
            mpr("It scalds you terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_FIRE:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_fire(), hurted, true);
        if (hurted < original)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original)
        {
            mpr("It burns terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_COLD:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_cold(), hurted, true);
        if (hurted < original)
            canned_msg(MSG_YOU_RESIST);
        else if (hurted > original)
        {
            mpr("You feel a terrible chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_ELECTRICITY:
        hurted = resist_adjust_damage(&you, flavour,
                                      player_res_electricity(),
                                      hurted, true);

        if (hurted < original)
            canned_msg(MSG_YOU_RESIST);
        break;

    case BEAM_POISON:
        resist = player_res_poison();

        if (resist <= 0)
            poison_player( coinflip() ? 2 : 1 );

        hurted = resist_adjust_damage(&you, flavour, resist,
                                      hurted, true);
        if (resist > 0)
            canned_msg(MSG_YOU_RESIST);
        break;

    case BEAM_POISON_ARROW:
        // [dshaligram] NOT importing uber-poison arrow from 4.1. Giving no
        // bonus to poison resistant players seems strange and unnecessarily
        // arbitrary.
        resist = player_res_poison();

        if (!resist)
            poison_player( 4 + random2(3), true );
        else if (!you.is_undead)
            poison_player( 2 + random2(3), true );

        hurted = resist_adjust_damage(&you, flavour, resist, hurted);
        if (hurted < original)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        break;

    case BEAM_NEG:
        resist = player_prot_life();

        // TSO's protection.
        if (you.religion == GOD_SHINING_ONE && you.piety > resist * 50)
        {
            int unhurted = std::min(hurted, (you.piety * hurted) / 150);

            if (unhurted > 0)
                hurted -= unhurted;
        }
        else if (resist > 0)
            hurted -= (resist * hurted) / 3;

        drain_exp();
        break;

    case BEAM_ICE:
        hurted = resist_adjust_damage(&you, flavour, player_res_cold(),
                                      hurted, true);

        if (hurted < original)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original)
        {
            mpr("You feel a painful chill!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_LAVA:
        hurted = resist_adjust_damage(&you, flavour, player_res_fire(),
                                      hurted, true);

        if (hurted < original)
            canned_msg(MSG_YOU_PARTIALLY_RESIST);
        else if (hurted > original)
        {
            mpr("It burns terribly!");
            xom_is_stimulated(200);
        }
        break;

    case BEAM_ACID:
        if (player_res_acid())
        {
            canned_msg(MSG_YOU_RESIST);
            hurted = hurted * player_acid_resist_factor() / 100;
        }
        break;

    case BEAM_MIASMA:
        if (x_chance_in_y(player_prot_life(), 3))
        {
            canned_msg(MSG_YOU_RESIST);
            hurted = 0;
        }
        break;

    case BEAM_HOLY:
    {
        // Cleansing flame.
        const int rhe = you.res_holy_energy(NULL);
        if (rhe > 0)
            hurted = 0;
        else if (rhe == 0)
            hurted /= 2;
        else if (rhe < -1)
            hurted = (hurted * 3) / 2;

        if (hurted == 0)
            canned_msg(MSG_YOU_RESIST);
        break;
    }

    default:
        break;
    }                           // end switch

    return (hurted);
}

void splash_with_acid(int acid_strength, bool corrode_items)
{
    int dam = 0;
    const bool wearing_cloak = player_wearing_slot(EQ_CLOAK);

    for (int slot = EQ_CLOAK; slot <= EQ_BODY_ARMOUR; slot++)
    {
        if (!player_wearing_slot(slot))
        {
            if (!wearing_cloak || coinflip())
                dam += roll_dice(1, acid_strength);
        }
        else if (corrode_items && x_chance_in_y(acid_strength + 1, 20))
            _item_corrode(you.equip[slot]);
    }

    if (dam > 0)
    {
        const int post_res_dam = dam * player_acid_resist_factor() / 100;

        if (post_res_dam > 0)
        {
            mpr("The acid burns!");

            if (post_res_dam < dam)
                canned_msg(MSG_YOU_RESIST);

            ouch(post_res_dam, NON_MONSTER, KILLED_BY_ACID);
        }
    }
}

void weapon_acid(int acid_strength)
{
    int hand_thing = you.equip[EQ_WEAPON];

    if (hand_thing == -1 && you_tran_can_wear(EQ_GLOVES, true))
        hand_thing = you.equip[EQ_GLOVES];

    if (hand_thing == -1)
    {
        msg::stream << "Your " << your_hand(true) << " burn!" << std::endl;
        ouch(roll_dice(1, acid_strength), NON_MONSTER, KILLED_BY_ACID);
    }
    else if (x_chance_in_y(acid_strength + 1, 20))
        _item_corrode(hand_thing);
}

void _item_corrode(int slot)
{
    bool it_resists = false;
    bool suppress_msg = false;
    item_def& item = you.inv[slot];

    // Artefacts don't corrode.
    if (is_artefact(item))
        return;

    // Anti-corrosion items protect against 90% of corrosion.
    if (wearing_amulet(AMU_RESIST_CORROSION) && !one_chance_in(10))
    {
#if DEBUG_DIAGNOSTICS
        mpr( "Amulet protects.", MSGCH_DIAGNOSTICS );
#endif
        return;
    }

    int how_rusty = ((item.base_type == OBJ_WEAPONS) ? item.plus2 : item.plus);
    // Already very rusty.
    if (how_rusty < -5)
        return;

    // determine possibility of resistance by object type {dlb}:
    switch (item.base_type)
    {
    case OBJ_ARMOUR:
        if ((item.sub_type == ARM_CRYSTAL_PLATE_MAIL
             || get_equip_race(item) == ISFLAG_DWARVEN)
            && !one_chance_in(5))
        {
            it_resists = true;
            suppress_msg = false;
        }
        break;

    case OBJ_WEAPONS:
    case OBJ_MISSILES:
        if (get_equip_race(item) == ISFLAG_DWARVEN && !one_chance_in(5))
        {
            it_resists = true;
            suppress_msg = false;
        }
        break;

    default:
        // Other items can't corrode.
        return;
    }

    // determine chance of corrosion {dlb}:
    if (!it_resists)
    {
        const int chance = abs(how_rusty);

        // The embedded equation may look funny, but it actually works well
        // to generate a pretty probability ramp {6%, 18%, 34%, 58%, 98%}
        // for values [0,4] which closely matches the original, ugly switch.
        // {dlb}
        if (chance >= 0 && chance <= 4)
            it_resists = x_chance_in_y(2 + (4 << chance) + chance * 8, 100);
        else
            it_resists = true;

        // If the checks get this far, you should hear about it. {dlb}
        suppress_msg = false;
    }

    // handle message output and item damage {dlb}:
    if (!suppress_msg)
    {
        if (it_resists)
            mprf("%s resists.", item.name(DESC_CAP_YOUR).c_str());
        else
            mprf("The acid corrodes %s!", item.name(DESC_NOCAP_YOUR).c_str());
    }

    if (!it_resists)
    {
        how_rusty--;
        xom_is_stimulated(64);

        if (item.base_type == OBJ_WEAPONS)
            item.plus2 = how_rusty;
        else
            item.plus  = how_rusty;

        if (item.base_type == OBJ_ARMOUR)
            you.redraw_armour_class = true;

        if (you.equip[EQ_WEAPON] == slot)
            you.wield_change = true;
    }
}

// Helper function for the expose functions below.
// This currently works because elements only target a single type each.
static int _get_target_class(beam_type flavour)
{
    int target_class = OBJ_UNASSIGNED;

    switch (flavour)
    {
    case BEAM_FIRE:
    case BEAM_LAVA:
    case BEAM_NAPALM:
    case BEAM_HELLFIRE:
        target_class = OBJ_SCROLLS;
        break;

    case BEAM_COLD:
    case BEAM_FRAG:
        target_class = OBJ_POTIONS;
        break;

    case BEAM_SPORE:
    case BEAM_STEAL_FOOD:
        target_class = OBJ_FOOD;
        break;

    default:
        break;
    }

    return (target_class);
}

// XXX: These expose functions could use being reworked into a real system...
// the usage and implementation is currently very hacky.
// Handles the destruction of inventory items from the elements.
static bool _expose_invent_to_element(beam_type flavour, int strength)
{
    int num_dest = 0;

    const int target_class = _get_target_class( flavour );
    if (target_class == OBJ_UNASSIGNED)
        return (false);

    // Currently we test against each stack (and item in the stack)
    // independently at strength%... perhaps we don't want that either
    // because it makes the system very fair and removes the protection
    // factor of junk (which might be more desirable for game play).
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        if (you.inv[i].base_type == target_class
            || target_class == OBJ_FOOD
               && you.inv[i].base_type == OBJ_CORPSES)
        {
            // Conservation doesn't help against harpies stealing food.
            if (flavour != BEAM_STEAL_FOOD
                && player_item_conserve() && !one_chance_in(10))
            {
                continue;
            }

            // Loop through all items in the stack.
            for (int j = 0; j < you.inv[i].quantity; ++j)
            {
                if (x_chance_in_y(strength, 100))
                {
                    num_dest++;

                    if (i == you.equip[EQ_WEAPON])
                        you.wield_change = true;

                    if (dec_inv_item_quantity(i, 1))
                        break;
                    else if (is_blood_potion(you.inv[i]))
                        remove_oldest_blood_potion(you.inv[i]);
                }
            }
        }
    }

    if (!num_dest)
        return (false);

    // Message handled elsewhere.
    if (flavour == BEAM_STEAL_FOOD)
        return (true);

    switch (target_class)
    {
    case OBJ_SCROLLS:
        mprf("%s you are carrying %s fire!",
             (num_dest > 1) ? "Some of the scrolls" : "A scroll",
             (num_dest > 1) ? "catch" : "catches" );
        break;

    case OBJ_POTIONS:
        mprf("%s you are carrying %s and %s!",
             (num_dest > 1) ? "Some of the potions" : "A potion",
             (num_dest > 1) ? "freeze" : "freezes",
             (num_dest > 1) ? "shatter" : "shatters" );
        break;

    case OBJ_FOOD:
        mpr("Some of your food is covered with spores!");
        break;

    default:
        mprf("%s you are carrying %s destroyed!",
             (num_dest > 1) ? "Some items" : "An item",
             (num_dest > 1) ? "were" : "was" );
        break;
    }

    xom_is_stimulated((num_dest > 1) ? 32 : 16);

    return (true);
}

bool expose_items_to_element(beam_type flavour, const coord_def& where,
                             int strength)
{
    int num_dest = 0;

    const int target_class = _get_target_class(flavour);
    if (target_class == OBJ_UNASSIGNED)
        return (false);

    for (stack_iterator si(where); si; ++si)
    {
        if (!is_valid_item(*si))
            continue;

        if (si->base_type == target_class
            || target_class == OBJ_FOOD && si->base_type == OBJ_CORPSES)
        {
            if (x_chance_in_y(strength, 100))
            {
                num_dest++;
                if (!dec_mitm_item_quantity(si->index(), 1)
                    && is_blood_potion(*si))
                {
                    remove_oldest_blood_potion(*si);
                }
            }
        }
    }

    if (!num_dest)
        return (false);

    if (flavour == BEAM_STEAL_FOOD)
        return (true);

    if (see_grid(where))
    {
        switch (target_class)
        {
        case OBJ_SCROLLS:
            mprf("You see %s of smoke.",
                 (num_dest > 1) ? "some puffs" : "a puff");
            break;

        case OBJ_POTIONS:
            mprf("You see %s shatter.",
                 (num_dest > 1) ? "some glass" : "glass");
            break;

        case OBJ_FOOD:
            mprf("You see %s of spores.",
                 (num_dest > 1) ? "some clouds" : "a cloud");
            break;

        default:
            mprf("%s on the floor %s destroyed!",
                 (num_dest > 1) ? "Some items" : "An item",
                 (num_dest > 1) ? "were" : "was" );
            break;
        }
    }

    xom_is_stimulated((num_dest > 1) ? 32 : 16);

    return (true);
}

// Handle side-effects for exposure to element other than damage.  This
// function exists because some code calculates its own damage instead
// of using check_your_resists() and we want to isolate all the special
// code they keep having to do... namely condensation shield checks,
// you really can't expect this function to even be called for much
// else.
//
// This function now calls _expose_invent_to_element() if strength > 0.
//
// XXX: This function is far from perfect and a work in progress.
bool expose_player_to_element(beam_type flavour, int strength)
{
    // Note that BEAM_TELEPORT is sent here when the player
    // blinks or teleports.
    if (flavour == BEAM_FIRE || flavour == BEAM_LAVA
        || flavour == BEAM_HELLFIRE || flavour == BEAM_FRAG
        || flavour == BEAM_TELEPORT || flavour == BEAM_NAPALM
        || flavour == BEAM_STEAM)
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
            remove_condensation_shield();
    }

    if (strength <= 0)
        return (false);

    return (_expose_invent_to_element( flavour, strength ));
}

void lose_level()
{
    // Because you.experience is unsigned long, if it's going to be
    // negative, must die straightaway.
    if (you.experience_level == 1)
    {
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_DRAINING);
        // Return in case death was canceled via wizard mode
        return;
    }

    you.experience = exp_needed( you.experience_level + 1 ) - 1;
    you.experience_level--;

    mprf(MSGCH_WARN,
         "You are now level %d!", you.experience_level);

    // Constant value to avoid grape jelly trick... see level_change() for
    // where these HPs and MPs are given back.  -- bwr
    ouch(4, NON_MONSTER, KILLED_BY_DRAINING);
    dec_max_hp(4);

    dec_mp(1);
    dec_max_mp(1);

    calc_hp();
    calc_mp();

    char buf[200];
    sprintf(buf, "HP: %d/%d MP: %d/%d",
            you.hp, you.hp_max, you.magic_points, you.max_magic_points);
    take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, buf));

    redraw_skill(you.your_name, player_title());
    you.redraw_experience = true;

    xom_is_stimulated(255);
}

bool drain_exp(bool announce_full)
{
    const int protection = player_prot_life();

    if (protection == 3)
    {
        if (announce_full)
            canned_msg(MSG_YOU_RESIST);

        return (false);
    }

    if (you.experience == 0)
    {
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_DRAINING);

        // Return in case death was escaped via wizard mode.
        return (true);
    }

    if (you.experience_level == 1)
    {
        you.experience = 0;

        return (true);
    }

    unsigned long total_exp = exp_needed(you.experience_level + 2)
                                  - exp_needed(you.experience_level + 1);
    unsigned long exp_drained = (total_exp * (10 + random2(11))) / 100;
    unsigned long pool_drained = std::min(exp_drained,
                                     (unsigned long)you.exp_available);

    // TSO's protection.
    if (you.religion == GOD_SHINING_ONE && you.piety > protection * 50)
    {
        unsigned long undrained = std::min(exp_drained,
                                      (you.piety * exp_drained) / 150);
        unsigned long pool_undrained = std::min(pool_drained,
                                           (you.piety * pool_drained) / 150);

        if (undrained > 0 || pool_undrained > 0)
        {
            simple_god_message(" protects your life force!");
            if (undrained > 0)
                exp_drained -= undrained;
            if (pool_undrained > 0)
                pool_drained -= pool_undrained;
        }
    }
    else if (protection > 0)
    {
        canned_msg(MSG_YOU_PARTIALLY_RESIST);
        exp_drained -= (protection * exp_drained) / 3;
        pool_drained -= (protection * pool_drained) / 3;
    }

    if (exp_drained > 0)
    {
        mpr("You feel drained.");
        xom_is_stimulated(20);
        you.experience -= exp_drained;
        you.exp_available -= pool_drained;

        you.exp_available = std::max(0, you.exp_available);

#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "You lose %ld experience points, %ld from pool.",
             exp_drained, pool_drained);
#endif

        you.redraw_experience = true;

        if (you.experience < exp_needed(you.experience_level + 1))
            lose_level();

        return (true);
    }

    return (false);
}

static void _xom_checks_damage(kill_method_type death_type,
                               int dam, int death_source)
{
    if (you.religion == GOD_XOM)
    {
        if (death_type == KILLED_BY_TARGETTING
            || death_type == KILLED_BY_BOUNCE
            || death_type == KILLED_BY_REFLECTION
            || death_type == KILLED_BY_SELF_AIMED
               && player_in_a_dangerous_place())
        {
            // Xom thinks the player accidentally hurting him/herself is funny.
            // Deliberate damage is only amusing if it's dangerous.
            int amusement = 255 * dam / (dam + you.hp);
            if (death_type == KILLED_BY_SELF_AIMED)
                amusement /= 5;
            xom_is_stimulated(amusement);
            return;
        }
        else if (death_type == KILLED_BY_FALLING_DOWN_STAIRS)
        {
            // Xom thinks falling down the stairs is hilarious.
            xom_is_stimulated(255);
            return;
        }
        else if (death_type != KILLED_BY_MONSTER
                    && death_type != KILLED_BY_BEAM
                 || invalid_monster_index(death_source))
        {
            return;
        }

        int amusementvalue = 1;
        const monsters *monster = &menv[death_source];

        if (!monster->alive())
            return;

        if (mons_wont_attack(monster))
        {
            // Xom thinks collateral damage is funny.
            xom_is_stimulated(255 * dam / (dam + you.hp));
            return;
        }

        int leveldif = monster->hit_dice - you.experience_level;
        if (leveldif == 0)
            leveldif = 1;

        // Note that Xom is amused when you are significantly hurt by a
        // creature of higher level than yourself, as well as by a
        // creature of lower level than yourself.
        amusementvalue += leveldif * leveldif * dam;

        if (!player_monster_visible(monster))
            amusementvalue += 10;

        if (monster->speed < (int)player_movement_speed())
            amusementvalue += 8;

        if (death_type != KILLED_BY_BEAM
            && you.skills[SK_THROWING] <= (you.experience_level / 4))
        {
            amusementvalue += 2;
        }
        else if (you.skills[SK_FIGHTING] <= (you.experience_level / 4))
            amusementvalue += 2;

        if (player_in_a_dangerous_place())
            amusementvalue += 2;

        amusementvalue /= (you.hp > 0) ? you.hp : 1;

        xom_is_stimulated(amusementvalue);
    }
}

static void _yred_mirrors_injury(int dam, int death_source)
{
    if (yred_injury_mirror())
    {
        if (dam <= 0 || invalid_monster_index(death_source))
            return;

        monsters *mon = &menv[death_source];

        if (!mon->alive())
            return;

        simple_god_message(" mirrors your injury!");

#ifndef USE_TILE
        flash_monster_colour(mon, RED, 200);
#endif

        mon->hurt(&you, dam);

        if (mon->alive())
            print_wounds(mon);

        lose_piety(integer_sqrt(dam));
    }
}

static void _wizard_restore_life()
{
    if (you.hp <= 0)
        set_hp(you.hp_max, false);
    if (you.strength <= 0)
    {
        you.strength        = you.max_strength;
        you.redraw_strength = true;
    }
    if (you.dex <= 0)
    {
        you.dex              = you.max_dex;
        you.redraw_dexterity = true;
        you.redraw_evasion   = true;
    }
    if (you.intel <= 0)
    {
        you.intel               = you.max_intel;
        you.redraw_intelligence = true;
    }
}

// death_source should be set to NON_MONSTER for non-monsters. {dlb}
void ouch(int dam, int death_source, kill_method_type death_type,
          const char *aux, bool see_source)
{
    ASSERT(!crawl_state.arena);

    if (dam != INSTANT_DEATH && you.species == SP_DEEP_DWARF)
    {
        // Deep Dwarves get to shave _any_ hp loss.
        int shave = 1 + random2(2 + random2(1 + you.experience_level / 3));
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "HP shaved: %d.", shave);
#endif
        dam -= shave;
        if (dam <= 0)
            return;
    }

    ait_hp_loss hpl(dam, death_type);
    interrupt_activity(AI_HP_LOSS, &hpl);

    if (dam > 0)
        you.check_awaken(500);

    if (you.duration[DUR_DEATHS_DOOR] && death_type != KILLED_BY_LAVA
        && death_type != KILLED_BY_WATER)
    {
        return;
    }

    if (dam != INSTANT_DEATH)
    {
        if (dam >= you.hp)
        {
            if (harm_protection_type hpt = god_protects_from_harm(you.religion))
            {
                simple_god_message(" protects you from harm!");

                if (you.duration[DUR_PRAYER]
                    && hpt == HPT_RELIABLE_PRAYING_PLUS_ANYTIME)
                {
                    lose_piety(21 + random2(20));
                }
                return;
            }
        }

        dec_hp(dam, true);

        // Even if we have low HP messages off, we'll still give a
        // big hit warning (in this case, a hit for half our HPs) -- bwr
        if (dam > 0 && you.hp_max <= dam * 2)
            mpr( "Ouch! That really hurt!", MSGCH_DANGER );

        if (you.hp > 0)
        {
            if (Options.hp_warning
                && you.hp <= (you.hp_max * Options.hp_warning) / 100)
            {
                mpr( "* * * LOW HITPOINT WARNING * * *", MSGCH_DANGER );
            }

            _xom_checks_damage(death_type, dam, death_source);

            // for note taking
            std::string damage_desc;
            if (!see_source)
            {
                snprintf(info, INFO_SIZE, "something (%d)", dam);
                damage_desc = info;
            }
            else
            {
                damage_desc = scorefile_entry(dam, death_source,
                                              death_type, aux, true)
                    .death_description(scorefile_entry::DDV_TERSE);
            }

            take_note(
                      Note(NOTE_HP_CHANGE, you.hp, you.hp_max, damage_desc.c_str()) );

            _yred_mirrors_injury(dam, death_source);

            return;
        } // else hp <= 0
    }

    // Is the player being killed by a direct act of Xom?
    if (crawl_state.is_god_acting()
        && crawl_state.which_god_acting() == GOD_XOM
        && crawl_state.other_gods_acting().size() == 0)
    {
        you.escaped_death_cause = death_type;
        you.escaped_death_aux   = aux == NULL ? "" : aux;

        // Xom should only kill his worshippers if they're under penance
        // or Xom is bored.
        if (you.religion == GOD_XOM && !you.penance[GOD_XOM]
            && you.gift_timeout > 0)
        {
            return;
        }

        // Also don't kill wizards testing Xom acts.
        if ((crawl_state.repeat_cmd == CMD_WIZARD
                || crawl_state.prev_cmd == CMD_WIZARD)
            && you.religion != GOD_XOM)
        {
            return;
        }

        // Okay, you *didn't* escape death.
        you.reset_escaped_death();

        // Ensure some minimal information about Xom's involvment.
        if (aux == NULL || strlen(aux) == 0)
        {
            if (death_type != KILLED_BY_XOM)
                aux = "Xom";
        }
        else if (strstr(aux, "Xom") == NULL)
            death_type = KILLED_BY_XOM;
    }

    // Construct scorefile entry.
    scorefile_entry se(dam, death_source, death_type, aux);

#ifdef WIZARD
    if (death_type != KILLED_BY_QUITTING
        && death_type != KILLED_BY_WINNING
        && death_type != KILLED_BY_LEAVING)
    {
        if (you.wizard)
        {
            const std::string death_desc
                = se.death_description(scorefile_entry::DDV_VERBOSE);
#ifdef USE_OPTIONAL_WIZARD_DEATH

#if DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS, "Damage: %d; Hit points: %d", dam, you.hp);
#endif

            if (!yesno("Die?", false, 'n'))
            {
                take_note(Note( NOTE_DEATH, you.hp, you.hp_max,
                                death_desc.c_str()), true);
                _wizard_restore_life();
                return;
            }
#else  // !def USE_OPTIONAL_WIZARD_DEATH
            mpr("Since you're a debugger, I'll let you live.");
            mpr("Be more careful next time, okay?");

            take_note(Note( NOTE_DEATH, you.hp, you.hp_max,
                            death_desc.c_str()), true);
            _wizard_restore_life();
            return;
#endif  // USE_OPTIONAL_WIZARD_DEATH
        }
    }
#endif  // WIZARD

    // Okay, so you're dead.
    crawl_state.need_save       = false;
    crawl_state.updating_scores = true;

    take_note(Note( NOTE_DEATH, you.hp, you.hp_max,
                    se.death_description(scorefile_entry::DDV_NORMAL).c_str()),
              true);

    // Prevent bogus notes.
    activate_notes(false);

#ifdef SCORE_WIZARD_CHARACTERS
    // Add this highscore to the score file.
    hiscores_new_entry(se);
    logfile_new_entry(se);
#else

    // Only add non-wizards to the score file.
    // Never generate bones files of wizard characters -- bwr
    if (!you.wizard)
    {
        hiscores_new_entry(se);
        logfile_new_entry(se);

        if (death_type != KILLED_BY_LEAVING
            && death_type != KILLED_BY_WINNING
            && death_type != KILLED_BY_QUITTING)
        {
            save_ghost();
        }
    }
#endif

    end_game(se);
}

static std::string morgue_name(time_t when_crawl_got_even)
{
#ifdef SHORT_FILE_NAMES
    return "morgue";
#else  // !SHORT_FILE_NAMES
    std::string name = "morgue-" + std::string(you.your_name);

    if (tm *loc = TIME_FN(&when_crawl_got_even))
    {
        char buf[25];
        snprintf(buf, sizeof buf, "-%04d%02d%02d-%02d%02d%02d",
                 loc->tm_year + 1900,
                 loc->tm_mon + 1,
                 loc->tm_mday,
                 loc->tm_hour,
                 loc->tm_min,
                 loc->tm_sec);
        name += buf;
    }
    return (name);
#endif // SHORT_FILE_NAMES
}

void end_game( scorefile_entry &se )
{
    bool dead = true;

    for (int i = 0; i < ENDOFPACK; i++)
        set_ident_flags( you.inv[i], ISFLAG_IDENT_MASK );

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].base_type != 0)
            set_ident_type( you.inv[i], ID_KNOWN_TYPE );
    }

    if (!dump_char( morgue_name(se.death_time), !dead, true, &se ))
    {
        mpr("Char dump unsuccessful! Sorry about that.");
        if (!crawl_state.seen_hups)
            more();
        clrscr();
    }

    if (se.death_type == KILLED_BY_LEAVING
        || se.death_type == KILLED_BY_QUITTING
        || se.death_type == KILLED_BY_WINNING)
    {
        dead = false;
    }

    // clean all levels that we think we have ever visited
    for (level_id_set::const_iterator i = Generated_Levels.begin();
         i != Generated_Levels.end(); ++i)
    {
        const level_id &place(*i);
        unlink(
            make_filename( you.your_name,
                           place.absdepth(),
                           place.branch,
                           place.level_type,
                           false ).c_str() );
    }

    // temp levels, if any
    unlink( make_filename( you.your_name, 0, BRANCH_MAIN_DUNGEON,
                           LEVEL_ABYSS, false ).c_str() );
    unlink( make_filename( you.your_name, 0, BRANCH_MAIN_DUNGEON,
                           LEVEL_PANDEMONIUM, false ).c_str() );
    unlink( make_filename( you.your_name, 0, BRANCH_MAIN_DUNGEON,
                           LEVEL_LABYRINTH, false ).c_str() );
    unlink( make_filename( you.your_name, 0, BRANCH_MAIN_DUNGEON,
                           LEVEL_PORTAL_VAULT, false ).c_str() );

    // create base file name
    std::string basename = get_savedir_filename( you.your_name, "", "" );

    const char* suffixes[] = {
#ifdef CLUA_BINDINGS
        ".lua",
#endif
#ifdef PACKAGE_SUFFIX
        PACKAGE_SUFFIX ,
#endif
        ".st", ".kil", ".tc", ".nts", ".tut", ".sav", ".msg"
    };

    const int num_suffixes = sizeof(suffixes) / sizeof(const char*);

    for (int i = 0; i < num_suffixes; ++i)
    {
        std::string tmpname = basename + suffixes[i];
        unlink( tmpname.c_str() );
    }

    // death message
    if (dead)
    {
        mpr("You die...");      // insert player name here? {dlb}
        xom_death_message((kill_method_type) se.death_type);
        flush_prev_message();
        viewwindow(true, false); // don't do for leaving/winning characters

        if (Options.tutorial_left)
            tutorial_death_screen();
    }

#ifdef DGL_WHEREIS
    whereis_record( se.death_type == KILLED_BY_QUITTING? "quit" :
                    se.death_type == KILLED_BY_WINNING ? "won"  :
                    se.death_type == KILLED_BY_LEAVING ? "bailed out"
                                                       : "dead" );
#endif

    if (!crawl_state.seen_hups)
        more();

    browse_inventory(true);
    textcolor( LIGHTGREY );
    clrscr();

    clrscr();
    cprintf( "Goodbye, %s.", you.your_name );
    cprintf( EOL EOL "    " ); // Space padding where # would go in list format

    std::string hiscore = hiscores_format_single_long( se, true );

    const int lines = count_occurrences(hiscore, EOL) + 1;

    cprintf( "%s", hiscore.c_str() );

    cprintf( EOL "Best Crawlers -" EOL );

    // "- 5" gives us an extra line in case the description wraps on a line.
    hiscores_print_list( get_number_of_lines() - lines - 5 );

    // just to pause, actual value returned does not matter {dlb}
    if (!crawl_state.seen_hups)
        get_ch();
    end(0);
}

int actor_to_death_source(const actor* agent)
{
    if (agent->atype() == ACT_PLAYER)
        return (NON_MONSTER);
    else if (agent->atype() == ACT_MONSTER)
        return (dynamic_cast<const monsters*>(agent)->mindex());
    else
        return (NON_MONSTER);
}
