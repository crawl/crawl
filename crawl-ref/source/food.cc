/*
 *  File:       food.cc
 *  Summary:    Functions for eating and butchering.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "food.h"

#include <sstream>

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "cio.h"
#include "clua.h"
#include "command.h"
#include "debug.h"
#include "delay.h"
#include "initfile.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "it_use2.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "mutation.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills2.h"
#include "spells2.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "tutorial.h"
#include "xom.h"

static corpse_effect_type _determine_chunk_effect(corpse_effect_type chunktype,
                                                  bool rotten_chunk);
static void _eat_chunk(corpse_effect_type chunk_effect, bool cannibal,
                       int mon_intel = 0);
static void _eating(unsigned char item_class, int item_type);
static void _describe_food_change(int hunger_increment);
static bool _vampire_consume_corpse(int slot, bool invent);
static void _heal_from_food(int hp_amt, int mp_amt = 0, bool unrot = false,
                            bool restore_str = false);

/*
 *  BEGIN PUBLIC FUNCTIONS
 */

void make_hungry(int hunger_amount, bool suppress_msg,
                 bool allow_reducing)
{
    if (you.is_undead == US_UNDEAD)
        return;

    if (allow_reducing)
        hunger_amount = calc_hunger(hunger_amount);

    if (hunger_amount == 0 && !suppress_msg)
        return;

#if DEBUG_DIAGNOSTICS
    set_redraw_status( REDRAW_HUNGER );
#endif

    you.hunger -= hunger_amount;

    if (you.hunger < 0)
        you.hunger = 0;

    // So we don't get two messages, ever.
    bool state_message = food_change();

    if (!suppress_msg && !state_message)
        _describe_food_change( -hunger_amount );
}

void lessen_hunger(int satiated_amount, bool suppress_msg)
{
    if (you.is_undead == US_UNDEAD)
        return;

    you.hunger += satiated_amount;

    if (you.hunger > 12000)
        you.hunger = 12000;

    // So we don't get two messages, ever.
    bool state_message = food_change();

    if (!suppress_msg && !state_message)
        _describe_food_change(satiated_amount);
}

void set_hunger(int new_hunger_level, bool suppress_msg)
{
    if (you.is_undead == US_UNDEAD)
        return;

    int hunger_difference = (new_hunger_level - you.hunger);

    if (hunger_difference < 0)
        make_hungry(-hunger_difference, suppress_msg);
    else if (hunger_difference > 0)
        lessen_hunger(hunger_difference, suppress_msg);
}

// More of a "weapon_switch back from butchering" function, switching
// to a weapon is done using the wield_weapon code.
// special cases like staves of power or other special weps are taken
// care of by calling wield_effects().    {gdl}
void weapon_switch(int targ)
{
    if (you.equip[EQ_WEAPON] != -1
        && !check_old_item_warning(you.inv[you.equip[EQ_WEAPON]],
                                   OPER_WIELD))
    {
        return;
    }

    if (targ == -1) // Unarmed Combat.
    {
        // Already unarmed?
        if (!you.weapon())
            return;

        mpr( "You switch back to your bare hands." );
    }
    else
    {
        // Possibly not valid anymore (dropped, etc.).
        if (!is_valid_item(you.inv[targ]))
            return;

        // Already wielding this weapon?
        if (you.equip[EQ_WEAPON] == you.inv[targ].link)
            return;

        mprf("Switching back to %s.",
             you.inv[targ].name(DESC_INVENTORY).c_str());
    }

    // Unwield the old weapon and wield the new.
    // XXX This is a pretty dangerous hack;  I don't like it.--GDL
    //
    // Well yeah, but that's because interacting with the wielding
    // code is a mess... this whole function's purpose was to
    // isolate this hack until there's a proper way to do things. -- bwr
    if (you.weapon())
        unwield_item(false);

    you.equip[EQ_WEAPON] = targ;

    // Special checks: staves of power, etc.
    if (targ != -1)
        wield_effects(targ, false);

    if (Options.chunks_autopickup || you.species == SP_VAMPIRE)
        autopickup();

    // Same amount of time as normal wielding.
    // FIXME: this duplicated code is begging for a bug.
    if (you.weapon())
        you.time_taken /= 2;
    else                        // Swapping to empty hands is faster.
    {
        you.time_taken *= 3;
        you.time_taken /= 10;
    }

    you.wield_change = true;
    you.turn_is_over = true;
}

// Look for a butchering implement. If fallback is true,
// prompt the user if no obvious options exist.
// Returns whether a weapon was switched.
static bool _find_butchering_implement(int &butcher_tool)
{
    // When berserk, you can't change weapons.  Sanity check!
    if (!can_wield(NULL, true))
        return (false);

    // If wielding a distortion weapon, never attempt to switch away
    // automatically.
    if (const item_def *wpn = you.weapon())
    {
        // Otherwise we wouldn't be here.
        ASSERT(!can_cut_meat( *wpn ));

        if (wpn->base_type == OBJ_WEAPONS
            && item_type_known(*wpn)
            && get_weapon_brand(*wpn) == SPWPN_DISTORTION)
        {
            mprf(MSGCH_WARN,
                 "You're wielding a weapon of distortion, will not autoswap "
                 "for butchering.");

            return (false);
        }
    }

    bool potential_candidate = false;

    // Look for a butchering implement in your pack.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def& tool(you.inv[i]);
        if (is_valid_item( tool )
            && tool.base_type == OBJ_WEAPONS
            && can_cut_meat( tool )
            && can_wield( &tool )
            // Don't even suggest autocursing items.
            // Note that unknown autocursing is OK.
            && (!is_random_artefact(tool)
                || (randart_known_wpn_property(tool, RAP_CURSED) <= 0)))
        {
            if (Options.easy_butcher
                && item_known_uncursed(tool)
                && item_type_known(tool)
                && get_weapon_brand(tool) != SPWPN_DISTORTION
                // Don't even ask!
                && !has_warning_inscription(tool, OPER_WIELD))
            {
                butcher_tool = i;
                return (true);
            }
            else
                potential_candidate = true;
        }
    }

    if (!potential_candidate)
    {
        mpr("You don't carry any weapon that could be used for butchering.");
        return (false);
    }

    const int item_slot = prompt_invent_item(
                              "What would you like to use? (- for none)?",
                              MT_INVLIST, OSEL_BUTCHERY,
                              true, true, true, '-', -1, NULL, OPER_WIELD);

    if (prompt_failed(item_slot))
    {
        canned_msg(MSG_OK);
        return (false);
    }
    else if (item_slot == PROMPT_GOT_SPECIAL)
    {

        if (you.has_claws()
            || transform_can_butcher_barehanded(
                    static_cast<transformation_type>(
                        you.attribute[ATTR_TRANSFORMATION])))
        {
            butcher_tool = -1;
            return (true);
        }
        else
        {
            mpr("You can't butcher without a weapon!");
            return (false);
        }
    }
    else if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are already wielding that!");
        return (false);
    }

    if (is_valid_item(you.inv[item_slot])
        && you.inv[item_slot].base_type == OBJ_WEAPONS
        && can_cut_meat(you.inv[item_slot])
        && can_wield(&you.inv[item_slot]))
    {
        butcher_tool = item_slot;
        return (true);
    }

    mpr("That item isn't sharp enough!");
    return (false);
}

static bool _prepare_butchery(bool can_butcher, bool removed_gloves,
                              bool wpn_switch, int butchering_tool)
{
    // No preparation necessary.
    if (can_butcher)
        return (true);

    // At least one of these has to be true, else what are we doing here?
    ASSERT(removed_gloves || wpn_switch);

    // If you can butcher by taking off your gloves, don't prompt.
    if (removed_gloves)
    {
        // Actually take off the gloves; this creates a
        // delay. We assume later on that gloves have a 1-turn
        // takeoff delay!
        if (!takeoff_armour(you.equip[EQ_GLOVES]))
            return (false);
    }

    if (wpn_switch)
    {
        mprf("Switching to %s.",
             butchering_tool == -1 ? "unarmed" : "a butchering implement");

        if (!wield_weapon(true, butchering_tool, false, true))
            return (false);
    }

    // Switched to a good butchering tool.
    return (true);
}

static bool _butcher_corpse(int corpse_id, bool first_corpse = true,
                            bool force_butcher = false)
{
    ASSERT(corpse_id != -1);

    const bool rotten  = food_is_rotten(mitm[corpse_id]);
    const bool can_sac = you.duration[DUR_PRAYER]
                         && god_likes_butchery(you.religion);

    if (can_sac && !rotten)
        start_delay(DELAY_OFFER_CORPSE, 0, corpse_id);
    else
    {
        if (can_sac && rotten)
        {
            simple_god_message(coinflip() ? " refuses to accept that"
                                            " mouldy sacrifice!"
                                          : " demands fresh blood!",
                               you.religion);
        }

        // Start work on the first corpse we butcher.
        if (first_corpse)
            mitm[corpse_id].plus2++;

        int work_req = 4 - mitm[corpse_id].plus2;
        if (work_req < 0)
            work_req = 0;

        delay_type dtype = DELAY_BUTCHER;
        if (!force_butcher && !rotten
            && can_bottle_blood_from_corpse(mitm[corpse_id].plus))
        {
            dtype = DELAY_BOTTLE_BLOOD;
        }

        start_delay(dtype, work_req, corpse_id, mitm[corpse_id].special);
    }

    you.turn_is_over = true;
    return (true);
}

static void _terminate_butchery(bool wpn_switch, bool removed_gloves,
                                int old_weapon, int old_gloves)
{
    // Switch weapon back.
    if (wpn_switch && you.equip[EQ_WEAPON] != old_weapon
        && (!you.weapon() || !item_cursed(*you.weapon())))
    {
        start_delay(DELAY_WEAPON_SWAP, 1, old_weapon);
    }

    // Put on the removed gloves.
    if (removed_gloves && you.equip[EQ_GLOVES] != old_gloves)
        start_delay(DELAY_ARMOUR_ON, 1, old_gloves);
}

static bool _have_corpses_in_pack(bool remind)
{
    const bool sacrifice  = (you.duration[DUR_PRAYER]
                             && god_likes_butchery(you.religion));

    const bool can_bottle = (!sacrifice && you.species == SP_VAMPIRE
                             && you.experience_level > 5);

    int num        = 0;
    int num_bottle = 0;

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        item_def &obj(you.inv[i]);

        if (!is_valid_item(obj))
            continue;

        // Only actually count corpses, not skeletons.
        if (obj.base_type != OBJ_CORPSES || obj.sub_type != CORPSE_BODY)
            continue;

        // Only saprovorous characters care about rotten food.
        // Also, rotten corpses can't be sacrificed.
        if (food_is_rotten(obj) && (sacrifice
                                    || !player_mutation_level(MUT_SAPROVOROUS)))
        {
            continue;
        }

        num++;
        if (can_bottle && mons_has_blood(obj.plus))
            num_bottle++;
    }

    if (num == 0)
        return (false);

    std::string verb = (num_bottle ? (num == num_bottle ? "bottle"
                                                        : "bottle or butcher")
                                   : "butcher");

    std::string noun, pronoun;
    if (num == 1)
    {
        noun    = "corpse";
        pronoun = "it";
    }
    else
    {
        noun    = "corpses";
        pronoun = "them";
    }

    if (remind)
    {
        mprf("You might want to also %s the %s in your pack.", verb.c_str(),
             noun.c_str());
    }
    else
    {
        mprf("If you dropped the %s in your pack you could %s %s.",
             noun.c_str(), verb.c_str(), pronoun.c_str());
    }

    return (true);
}

bool butchery(int which_corpse)
{
    if (igrd(you.pos()) == NON_ITEM)
    {
        if (!_have_corpses_in_pack(false))
            mpr("There isn't anything here!");
        return (false);
    }

    if (you.flight_mode() == FL_LEVITATE)
    {
        mpr("You can't reach the floor from up here.");
        return (false);
    }

    const transformation_type transform =
        static_cast<transformation_type>(you.attribute[ATTR_TRANSFORMATION]);

    // Vampires' fangs are optimised for biting, not for tearing flesh.
    // (Not that they really need to.) Other species with this mutation
    // might still benefit from it.
    bool teeth_butcher    = (player_mutation_level(MUT_FANGS) == 3
                             && you.species != SP_VAMPIRE);

    bool barehand_butcher = (transform_can_butcher_barehanded(transform)
                                 || you.has_claws())
                             && !player_wearing_slot(EQ_GLOVES);

    bool gloved_butcher   = (you.has_claws() && player_wearing_slot(EQ_GLOVES)
                             && !item_cursed(you.inv[you.equip[EQ_GLOVES]]));

    bool can_butcher      = (teeth_butcher || barehand_butcher
                             || you.weapon() && can_cut_meat(*you.weapon()));

    if (!Options.easy_butcher && !can_butcher)
    {
        mpr("Maybe you should try using a sharper implement.");
        return (false);
    }

    // It makes more sense that you first find out if there's anything
    // to butcher, *then* decide to actually butcher it.
    // The old code did it the other way.
    if (!can_butcher && you.duration[DUR_BERSERKER])
    {
        mpr("You are too berserk to search for a butchering tool!");
        return (false);
    }

    // First determine how many things there are to butcher.
    int num_corpses = 0;
    int corpse_id   = -1;
    bool prechosen  = (which_corpse != -1);
    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY)
        {
            corpse_id = si->index();
            num_corpses++;

            // Return pre-chosen corpse if it exists.
            if (prechosen && corpse_id == which_corpse)
                break;
        }
    }

    if (num_corpses == 0)
    {
        if (!_have_corpses_in_pack(false))
        {
            mprf("There isn't anything to %sbutcher here.",
                 (you.species == SP_VAMPIRE
                     && you.experience_level > 5) ? "bottle or " : "");
        }
        return (false);
    }

    int old_weapon      = you.equip[EQ_WEAPON];
    int old_gloves      = you.equip[EQ_GLOVES];

    bool wpn_switch     = false;
    bool removed_gloves = false;
    int butcher_tool    = -1;

    if (!can_butcher)
    {
        // Try to find a butchering implement.
        if (!_find_butchering_implement(butcher_tool))
            return (false);

        if (butcher_tool == -1 && gloved_butcher)
            removed_gloves = true;

        if (you.equip[EQ_WEAPON] != butcher_tool)
            wpn_switch = true;
    }

    // Butcher pre-chosen corpse, if found, or if there is only one corpse.
    bool success = false;
    if (prechosen && corpse_id == which_corpse
        || num_corpses == 1 && !Options.always_confirm_butcher)
    {
        if (!_prepare_butchery(can_butcher, removed_gloves, wpn_switch,
                               butcher_tool))
        {
            return (false);
        }
        success = _butcher_corpse(corpse_id, true);
        _terminate_butchery(wpn_switch, removed_gloves, old_weapon, old_gloves);

        // Remind player of corpses in pack that could be butchered or
        // bottled.
        _have_corpses_in_pack(true);
        return (success);
    }

    // Now pick what you want to butcher. This is only a problem
    // if there are several corpses on the square.
    bool butcher_all   = false;
    bool bottle_all    = false; // for Vampires
    bool force_butcher = false;
    bool repeat_prompt = false;
    bool did_weap_swap = false;
    bool first_corpse  = true;
    int keyin;
    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
            continue;

        if (bottle_all && !can_bottle_blood_from_corpse(si->plus))
            continue;

        if (butcher_all || bottle_all)
            corpse_id = si->index();
        else
        {
            corpse_id = -1;

            std::string corpse_name = si->name(DESC_NOCAP_A);

            const bool sacrifice = (you.duration[DUR_PRAYER]
                                    && god_likes_butchery(you.religion));

            // We don't need to check for undead because
            // * Mummies can't eat.
            // * Ghouls relish the bad things.
            // * Vampires won't bottle bad corpses.
            // Also, don't bother colouring if it's only for sacrificing.
            if (!sacrifice && !you.is_undead)
            {
                corpse_name = get_message_colour_tags(*si, DESC_NOCAP_A,
                                                      MSGCH_PROMPT);
            }

            // Shall we butcher this corpse?
            do
            {
                mprf(MSGCH_PROMPT, "%s %s? (yc/n/a/q/?)",
                     (sacrifice || !can_bottle_blood_from_corpse(si->plus)) ?
                         "Butcher" : "Bottle",
                     corpse_name.c_str());
                repeat_prompt = false;

                keyin = tolower(getchm(KC_CONFIRM));
                switch (keyin)
                {
                case 'b':
                    force_butcher = true;
                    // intentional fall-through
                case 'y':
                case 'c':
                case 'd':
                case 'a':
                    if (!did_weap_swap)
                    {
                        if (_prepare_butchery(can_butcher, removed_gloves,
                                              wpn_switch, butcher_tool))
                        {
                            did_weap_swap = true;
                        }
                        else
                            return (false);
                    }
                    corpse_id = si->index();

                    if (keyin == 'a')
                    {
                        if (!force_butcher
                            && can_bottle_blood_from_corpse(si->plus)
                            && (!you.duration[DUR_PRAYER]
                                || !god_likes_butchery(you.religion)))
                        {
                            bottle_all = true;
                        }
                        else
                            butcher_all = true;
                    }
                    break;

                case ESCAPE:
                case 'q':
                    canned_msg(MSG_OK);
                    _terminate_butchery(wpn_switch, removed_gloves, old_weapon,
                                        old_gloves);
                    return (false);

                case '?':
                    show_butchering_help();
                    mesclr();
                    redraw_screen();
                    repeat_prompt = true;
                    break;

                default:
                    break;
                }
            }
            while (repeat_prompt);
        }

        if (corpse_id != -1)
        {
            if (_butcher_corpse(corpse_id, first_corpse,
                force_butcher || butcher_all))
            {
                success = true;
                first_corpse = false;
            }

//            if (!butcher_all && !bottle_all)
//                break;
        }
    }

    if (!butcher_all && !bottle_all && corpse_id == -1)
    {
        mprf("There isn't anything else to %s here.",
             you.species == SP_VAMPIRE && you.experience_level >= 6 ?
             "bottle" : "butcher");
    }
    _terminate_butchery(wpn_switch, removed_gloves, old_weapon, old_gloves);

    if (success)
    {
        // Remind player of corpses in pack that could be butchered or
        // bottled.
        _have_corpses_in_pack(true);
    }

    return (success);
}

void lua_push_floor_items(lua_State *ls)
{
    lua_push_items(ls, igrd(you.pos()));
}

void lua_push_inv_items(lua_State *ls = NULL)
{
    if (!ls)
        ls = clua.state();
    lua_newtable(ls);
    int index = 0;
    for (unsigned slot = 0; slot < ENDOFPACK; ++slot)
    {
        if (is_valid_item(you.inv[slot]))
        {
            lua_pushlightuserdata(ls, &you.inv[slot]);
            lua_rawseti(ls, -2, ++index);
        }
    }
}

bool prompt_eat_inventory_item(int slot)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return (false);
    }

    int which_inventory_slot;

    if (you.species != SP_VAMPIRE)
    {
        which_inventory_slot = (slot != -1) ? slot :
                prompt_invent_item("Eat which item?",
                                   MT_INVLIST,
                                   OBJ_FOOD,
                                   true, true, true, 0, -1, NULL,
                                   OPER_EAT);
    }
    else
    {
        which_inventory_slot = (slot != -1) ? slot :
                prompt_invent_item("Drain what?",
                                   MT_INVLIST,
                                   OSEL_VAMP_EAT,
                                   true, true, true, 0, -1, NULL,
                                   OPER_EAT);
    }

    if (prompt_failed(which_inventory_slot))
        return (false);

    // This conditional can later be merged into food::can_ingest() when
    // expanded to handle more than just OBJ_FOOD 16mar200 {dlb}
    if (you.species != SP_VAMPIRE)
    {
        if (you.inv[which_inventory_slot].base_type != OBJ_FOOD)
        {
            mpr("You can't eat that!");
            return (false);
        }
    }
    else
    {
        if (you.inv[which_inventory_slot].base_type != OBJ_CORPSES
            || you.inv[which_inventory_slot].sub_type != CORPSE_BODY)
        {
            mpr("You crave blood!");
            return (false);
        }
    }

    if (!can_ingest( you.inv[which_inventory_slot].base_type,
                     you.inv[which_inventory_slot].sub_type, false ))
    {
        return (false);
    }

    eat_inventory_item(which_inventory_slot);

    burden_change();
    you.turn_is_over = true;

    return (true);
}

// [ds] Returns true if something was eaten.
bool eat_food(int slot)
{
    if (you.is_undead == US_UNDEAD)
    {
        mpr("You can't eat.");
        crawl_state.zero_turns_taken();
        return (false);
    }

    if (you.hunger >= 11000)
    {
        mprf("You're too full to %s anything.",
             you.species == SP_VAMPIRE ? "drain" : "eat");
        crawl_state.zero_turns_taken();
        return (false);
    }

    int result;
    // Skip the prompts if we already know what we're eating.
    if (slot == -1)
    {
        result = prompt_eat_chunks();
        if (result == 1 || result == -1)
            return (result > 0);

        if (result != -2) // else skip ahead to inventory
        {
            if (igrd(you.pos()) != NON_ITEM)
            {
                result = eat_from_floor(true);
                if (result == 1)
                    return (true);
                if (result == -1)
                    return (false);
            }
        }
    }

    return (prompt_eat_inventory_item(slot));
}

/*
 *     END PUBLIC FUNCTIONS
 */

static bool _player_has_enough_food()
{
    int food_value = 0;
    item_def item;
    for (unsigned slot = 0; slot < ENDOFPACK; ++slot)
    {
        item = you.inv[slot];
        if (!is_valid_item(item))
            continue;

        if (!can_ingest(item.base_type, item.sub_type, true, true, false))
            continue;

        if (food_is_rotten(item) && !player_mutation_level(MUT_SAPROVOROUS))
            continue;

        if (is_poisonous(item))
            continue;

        if (is_mutagenic(item))
            continue;

        if (causes_rot(item))
            continue;

        // Vampires can only drain corpses.
        if (you.species == SP_VAMPIRE)
            food_value += 3;
        else
        {
            if (item.base_type != OBJ_FOOD)
                continue;

            switch (item.sub_type)
            {
            case FOOD_CHUNK:
                if (!player_mutation_level(MUT_HERBIVOROUS))
                    food_value += 2 * item.quantity;
                break;
            case FOOD_MEAT_RATION:
                if (!player_mutation_level(MUT_HERBIVOROUS))
                    food_value += 3 * item.quantity;
                break;
            case FOOD_BREAD_RATION:
                if (!player_mutation_level(MUT_CARNIVOROUS))
                    food_value += 3 * item.quantity;
                break;
            default:
                // Only count snacks if we really like them
                if (is_preferred_food(item))
                    food_value += item.quantity;
                break;
            }
        }
    }

    // You have "enough" food if you have, e.g.
    //  1 meat ration + 1 chunk, or 2 chunks for carnivores, or
    //  5 items of fruit, or 1 bread ration and 2 fruit items as a herbivore.
    return (food_value > 5);
}

static std::string _how_hungry()
{
    if (you.hunger_state > HS_SATIATED)
        return ("full");
    else if (you.species == SP_VAMPIRE)
        return ("thirsty");
    return ("hungry");
}

bool food_change(bool suppress_message)
{
    char newstate = HS_ENGORGED;
    bool state_changed = false;
    bool less_hungry   = false;

    you.hunger = std::max(you_min_hunger(), you.hunger);
    you.hunger = std::min(you_max_hunger(), you.hunger);

    // Get new hunger state.
    if (you.hunger <= 1000)
        newstate = HS_STARVING;
    else if (you.hunger <= 1533)
        newstate = HS_NEAR_STARVING;
    else if (you.hunger <= 2066)
        newstate = HS_VERY_HUNGRY;
    else if (you.hunger <= 2600)
        newstate = HS_HUNGRY;
    else if (you.hunger < 7000)
        newstate = HS_SATIATED;
    else if (you.hunger < 9000)
        newstate = HS_FULL;
    else if (you.hunger < 11000)
        newstate = HS_VERY_FULL;

    if (newstate != you.hunger_state)
    {
        state_changed = true;
        if (newstate > you.hunger_state)
            less_hungry = true;

        you.hunger_state = newstate;
        set_redraw_status( REDRAW_HUNGER );

        if (newstate < HS_SATIATED)
            interrupt_activity( AI_HUNGRY );

        if (you.species == SP_VAMPIRE)
        {
            if (newstate <= HS_SATIATED)
            {
                if (you.duration[DUR_BERSERKER] > 1)
                {
                    mpr("Your blood-deprived body can't sustain your rage any "
                        "longer.", MSGCH_DURATION);
                    you.duration[DUR_BERSERKER] = 1;
                }
                int transform = you.attribute[ATTR_TRANSFORMATION];
                if (transform != TRAN_NONE && transform != TRAN_BAT
                    && you.duration[DUR_TRANSFORMATION] > 2)
                {
                    mpr("Your blood-deprived body can't sustain your "
                        "transformation much longer.", MSGCH_DURATION);
                    you.duration[DUR_TRANSFORMATION] = 2;
                }
            }
            else if (you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT
                     && you.duration[DUR_TRANSFORMATION] > 5)
            {
                print_stats();
                mpr("Your blood-filled body can't sustain your transformation "
                    "much longer.", MSGCH_WARN);

                // Give more time because suddenly stopping flying can be fatal.
                you.duration[DUR_TRANSFORMATION] = 5;
            }
            else if (newstate == HS_ENGORGED && is_vampire_feeding()) // Alive
            {
                print_stats();
                mpr("You can't stomach any more blood right now.");
            }
        }

        if (!suppress_message)
        {
            std::string msg = "You ";
            switch (you.hunger_state)
            {
            case HS_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += "feel devoid of blood!";
                else
                    msg += "are starving!";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                // Xom thinks this is funny if you're in a labyrinth
                // and are low on food.
                if (you.level_type == LEVEL_LABYRINTH
                    && !_player_has_enough_food())
                {
                    xom_is_stimulated(64);
                }

                learned_something_new(TUT_YOU_STARVING);
                you.check_awaken(500);
                break;

            case HS_NEAR_STARVING:
                if (you.species == SP_VAMPIRE)
                    msg += "feel almost devoid of blood!";
                else
                    msg += "are near starving!";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(TUT_YOU_HUNGRY);
                break;

            case HS_VERY_HUNGRY:
            case HS_HUNGRY:
                msg += "are feeling ";
                if (you.hunger_state == HS_VERY_HUNGRY)
                    msg += "very ";
                msg += _how_hungry();
                msg += ".";

                mprf(MSGCH_FOOD, less_hungry, "%s", msg.c_str());

                learned_something_new(TUT_YOU_HUNGRY);
                break;

            default:
                return (state_changed);
            }
        }
    }

    return (state_changed);
}

// food_increment is positive for eating, negative for hungering
static void _describe_food_change(int food_increment)
{
    int magnitude = (food_increment > 0)?food_increment:(-food_increment);
    std::string msg;

    if (magnitude == 0)
        return;

    msg = "You feel ";

    if (magnitude <= 100)
        msg += "slightly ";
    else if (magnitude <= 350)
        msg += "somewhat ";
    else if (magnitude <= 800)
        msg += "quite a bit ";
    else
        msg += "a lot ";

    if ((you.hunger_state > HS_SATIATED) ^ (food_increment < 0))
        msg += "more ";
    else
        msg += "less ";

    msg += _how_hungry().c_str();
    msg += ".";
    mpr(msg.c_str());
}

static bool _player_can_eat_rotten_meat(bool need_msg = false)
{
    if (player_mutation_level(MUT_SAPROVOROUS))
        return (true);

    if (need_msg)
        mpr("You refuse to eat that rotten meat.");

    return (false);
}

// Should really be merged into function below. -- FIXME
void eat_inventory_item(int which_inventory_slot)
{
    item_def& food(you.inv[which_inventory_slot]);
    if (food.base_type == OBJ_CORPSES && food.sub_type == CORPSE_BODY)
    {
        _vampire_consume_corpse(which_inventory_slot, true);
        you.turn_is_over = true;
        return;
    }

    if (food.sub_type == FOOD_CHUNK)
    {
        const int mons_type  = food.plus;
        const bool cannibal  = is_player_same_species(mons_type);
        const int intel      = mons_class_intel(mons_type) - I_ANIMAL;
        const bool rotten    = food_is_rotten(food);
        const corpse_effect_type chunk_type = mons_corpse_effect(mons_type);

        if (rotten && !_player_can_eat_rotten_meat(true))
            return;

        _eat_chunk(_determine_chunk_effect(chunk_type, rotten), cannibal,
                   intel);
    }
    else
        _eating( food.base_type, food.sub_type );

    you.turn_is_over = true;
    dec_inv_item_quantity( which_inventory_slot, 1 );
}

void eat_floor_item(int item_link)
{
    item_def& food(mitm[item_link]);
    if (food.base_type == OBJ_CORPSES && food.sub_type == CORPSE_BODY)
    {
        if (you.species != SP_VAMPIRE)
            return;

        if (_vampire_consume_corpse(item_link, false))
            you.turn_is_over = true;

        return;
    }
    else if (food.sub_type == FOOD_CHUNK)
    {
        const int intel      = mons_class_intel(food.plus) - I_ANIMAL;
        const bool cannibal  = is_player_same_species(food.plus);
        const bool rotten    = food_is_rotten(food);
        const corpse_effect_type chunk_type = mons_corpse_effect(food.plus);

        if (rotten && !_player_can_eat_rotten_meat(true))
            return;

        _eat_chunk(_determine_chunk_effect(chunk_type, rotten), cannibal,
                   intel);
    }
    else
        _eating( food.base_type, food.sub_type );

    you.turn_is_over = true;

    dec_mitm_item_quantity( item_link, 1 );
}

static bool _is_bad_food(item_def food)
{
    return (is_poisonous(food) || is_mutagenic(food)
            || is_forbidden_food(food) || causes_rot(food));
}

// Returns which of two food items is older (true for first, else false).
struct compare_by_freshness
{
    bool operator()(item_def *food1, item_def *food2)
    {
        ASSERT(food1->base_type == OBJ_CORPSES || food1->base_type == OBJ_FOOD);
        ASSERT(food2->base_type == OBJ_CORPSES || food2->base_type == OBJ_FOOD);
        ASSERT(food1->base_type == food2->base_type);

        if (is_inedible(*food1))
            return (false);

        if (is_inedible(*food2))
            return (true);

        if (food1->base_type == OBJ_FOOD)
        {
            // Prefer chunks to non-chunks. (Herbivores handled above.)
            if (food1->sub_type != FOOD_CHUNK && food2->sub_type == FOOD_CHUNK)
                return (false);
            if (food2->sub_type != FOOD_CHUNK && food1->sub_type == FOOD_CHUNK)
                return (true);
        }

        // Both food types are edible (not rotten, or player is Saprovore).
        if (food1->base_type == OBJ_CORPSES
            || food1->sub_type == FOOD_CHUNK && food2->sub_type == FOOD_CHUNK)
        {
            // Always offer poisonous/mutagenic chunks last.
            if (_is_bad_food(*food1) && !_is_bad_food(*food2))
                return (false);
            if (_is_bad_food(*food2) && !_is_bad_food(*food1))
                return (true);

            if (Options.prefer_safe_chunks)
            {
                // Offer non-contaminated chunks first.
                if (is_contaminated(*food1) && !is_contaminated(*food2))
                    return (false);
                if (is_contaminated(*food2) && !is_contaminated(*food1))
                    return (true);
            }
        }

        return (food1->special < food2->special);
    }
};

// Returns -1 for cancel, 1 for eaten, 0 for not eaten.
int eat_from_floor(bool skip_chunks)
{
    if (you.flight_mode() == FL_LEVITATE)
        return (0);

    // Corpses should have been handled before.
    if (you.species == SP_VAMPIRE && skip_chunks)
        return (0);

    bool need_more = false;
    int unusable_corpse = 0;
    int inedible_food = 0;
    item_def wonteat;
    bool found_valid = false;

    std::vector<item_def *> food_items;
    for (stack_iterator si(you.pos()); si; ++si )
    {
        if (you.species == SP_VAMPIRE)
        {
            if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
                continue;

            if (!mons_has_blood(si->plus))
            {
                unusable_corpse++;
                continue;
            }
        }
        else
        {
            if (si->base_type != OBJ_FOOD)
                continue;

            // Chunks should have been handled before.
            if (skip_chunks && si->sub_type == FOOD_CHUNK)
                continue;
        }

        if (!skip_chunks && food_is_rotten(*si)
            && !_player_can_eat_rotten_meat())
        {
            unusable_corpse++;
            continue;
        }
        else if (!can_ingest(si->base_type, si->sub_type, true))
        {
            if (!inedible_food)
            {
                wonteat = *si;
                inedible_food++;
            }
            else
            {
                // Increase only if we're dealing with different subtypes.
                // FIXME: Use a common check for herbivorous/carnivorous
                //        dislikes, for e.g. "Blech! You need blood!"
                ASSERT(is_valid_item(wonteat));
                if (wonteat.sub_type != si->sub_type)
                    inedible_food++;
            }

            continue;
        }

        found_valid = true;
        food_items.push_back(&(*si));
    }

    if (found_valid)
    {
        std::sort(food_items.begin(), food_items.end(), compare_by_freshness());
        for (unsigned int i = 0; i < food_items.size(); ++i)
        {
            item_def *item = food_items[i];
            std::string item_name = get_message_colour_tags(*item, DESC_NOCAP_A,
                                                            MSGCH_PROMPT);

            mprf(MSGCH_PROMPT, "%s %s%s? (ye/n/q/i?)",
                 (you.species == SP_VAMPIRE ? "Drink blood from" : "Eat"),
                 ((item->quantity > 1) ? "one of " : ""),
                 item_name.c_str());

            int keyin = tolower(getchm(KC_CONFIRM));
            switch (keyin)
            {
            case ESCAPE:
            case 'q':
                canned_msg(MSG_OK);
                return -1;
            case 'e':
            case 'y':
                if (can_ingest(item->base_type, item->sub_type, false))
                {
                    int ilink = item_on_floor(*item, you.pos());

                    if (ilink != NON_ITEM)
                    {
                        eat_floor_item(ilink);
                        return (true);
                    }
                    return (false);
                }
                need_more = true;
                break;
            case 'i':
            case '?':
                // Directly skip ahead to inventory.
                return (0);
            default:
                // Else no: try next one.
                break;
            }
        }
    }
    else
    {
        // Give a message about why these food items can not actually be eaten.
        if (unusable_corpse)
        {
            if (you.species == SP_VAMPIRE)
            {
                mprf("%s devoid of blood.",
                     unusable_corpse == 1 ? "This corpse is"
                                          : "These corpses are");
            }
            else
                _player_can_eat_rotten_meat(true);
            need_more = true;
        }
        else if (inedible_food)
        {
            if (inedible_food == 1)
            {
                ASSERT(is_valid_item(wonteat));
                // Use the normal cannot ingest message.
                if (can_ingest(wonteat.base_type, wonteat.sub_type, false))
                {
                    mprf(MSGCH_DIAGNOSTICS, "Error: Can eat %s after all?",
                         wonteat.name(DESC_PLAIN).c_str() );
                }
            }
            else // Several different food items.
                mpr("You refuse to eat these food items.");
            need_more = true;
        }
    }

    if (need_more && Options.auto_list)
        more();

    return (0);
}

bool eat_from_inventory()
{
    // Corpses should have been handled before.
    if (you.species == SP_VAMPIRE)
        return 0;

    int unusable_corpse = 0;
    int inedible_food = 0;
    item_def *wonteat = NULL;
    bool found_valid = false;

    std::vector<item_def *> food_items;
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        item_def *item = &you.inv[i];
        if (you.species == SP_VAMPIRE)
        {
            if (item->base_type != OBJ_CORPSES || item->sub_type != CORPSE_BODY)
                continue;

            if (!mons_has_blood(item->plus))
            {
                unusable_corpse++;
                continue;
            }
        }
        else
        {
            // Chunks should have been handled before.
            if (item->base_type != OBJ_FOOD || item->sub_type == FOOD_CHUNK)
                continue;
        }

        if (food_is_rotten(*item) && !_player_can_eat_rotten_meat())
        {
            unusable_corpse++;
            continue;
        }
        else if (!can_ingest(item->base_type, item->sub_type, true))
        {
            if (!inedible_food)
            {
                wonteat = item;
                inedible_food++;
            }
            else
            {
                // Increase only if we're dealing with different subtypes.
                // FIXME: Use a common check for herbivorous/carnivorous
                //        dislikes, for e.g. "Blech! You need blood!"
                ASSERT(is_valid_item(*wonteat));
                if (wonteat->sub_type != item->sub_type)
                    inedible_food++;
            }
            continue;
        }

        found_valid = true;
        food_items.push_back(item);
    }

    if (found_valid)
    {
        std::sort(food_items.begin(), food_items.end(), compare_by_freshness());
        for (unsigned int i = 0; i < food_items.size(); ++i)
        {
            item_def *item = food_items[i];
            std::string item_name = get_message_colour_tags(*item, DESC_NOCAP_A,
                                                            MSGCH_PROMPT);

            mprf(MSGCH_PROMPT, "%s %s%s? (ye/n/q)",
                 (you.species == SP_VAMPIRE ? "Drink blood from" : "Eat"),
                 ((item->quantity > 1) ? "one of " : ""),
                 item_name.c_str());

            int keyin = tolower(getchm(KC_CONFIRM));
            switch (keyin)
            {
            case ESCAPE:
            case 'q':
                canned_msg(MSG_OK);
                return (false);
            case 'e':
            case 'y':
                if (can_ingest(item->base_type, item->sub_type, false))
                {
                    eat_inventory_item(item->link);
                    return (true);
                }
                break;
            default:
                // Else no: try next one.
                break;
            }
        }
    }
    else
    {
        // Give a message about why these food items can not actually be eaten.
        if (unusable_corpse)
        {
            if (you.species == SP_VAMPIRE)
            {
                mprf("%s devoid of blood.",
                     unusable_corpse == 1 ? "The corpse you are carrying is"
                                          : "The corpses you are carrying are");
            }
            else
                _player_can_eat_rotten_meat(true);
        }
        else if (inedible_food)
        {
            if (inedible_food == 1)
            {
                ASSERT(is_valid_item(*wonteat));
                // Use the normal cannot ingest message.
                if (can_ingest(wonteat->base_type, wonteat->sub_type, false))
                {
                    mprf(MSGCH_DIAGNOSTICS, "Error: Can eat %s after all?",
                         wonteat->name(DESC_PLAIN).c_str() );
                }
            }
            else // Several different food items.
                mpr("You refuse to eat these food items.");
        }
    }

    return (false);
}

// Returns -1 for cancel, 1 for eaten, 0 for not eaten,
//         -2 for skip to inventory.
int prompt_eat_chunks()
{
    // Herbivores cannot eat chunks.
    if (player_mutation_level(MUT_HERBIVOROUS) == 3)
        return (0);

    bool found_valid = false;
    std::vector<item_def *> chunks;

    // First search the stash on the floor, unless levitating.
    if (you.flight_mode() != FL_LEVITATE)
    {
        for (stack_iterator si(you.pos()); si; ++si)
        {
            if (you.species == SP_VAMPIRE)
            {
                if (si->base_type != OBJ_CORPSES || si->sub_type != CORPSE_BODY)
                    continue;

                if (!mons_has_blood(si->plus))
                    continue;
            }
            else if (si->base_type != OBJ_FOOD || si->sub_type != FOOD_CHUNK)
                continue;

            if (food_is_rotten(*si) && !_player_can_eat_rotten_meat())
                continue;

            found_valid = true;
            chunks.push_back(&(*si));
        }
    }

    // Then search through the inventory.
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (!is_valid_item(you.inv[i]))
            continue;

        item_def *item = &you.inv[i];
        if (you.species == SP_VAMPIRE)
        {
            if (item->base_type != OBJ_CORPSES || item->sub_type != CORPSE_BODY)
                continue;

            if (!mons_has_blood(item->plus))
                continue;
        }
        else if (item->base_type != OBJ_FOOD || item->sub_type != FOOD_CHUNK)
            continue;

        if (food_is_rotten(*item) && !_player_can_eat_rotten_meat())
            continue;

        found_valid = true;
        chunks.push_back(item);
    }

    const bool easy_eat = Options.easy_eat_chunks && !you.is_undead;
    const bool easy_contam = easy_eat
        && (Options.easy_eat_gourmand && wearing_amulet(AMU_THE_GOURMAND)
            || Options.easy_eat_contaminated);

    if (found_valid)
    {
        std::sort(chunks.begin(), chunks.end(), compare_by_freshness());
        for (unsigned int i = 0; i < chunks.size(); ++i)
        {
            bool autoeat = false;
            item_def *item = chunks[i];
            std::string item_name = get_message_colour_tags(*item,
                                                            DESC_NOCAP_A,
                                                            MSGCH_PROMPT);

            const bool contam = is_contaminated(*item);
            const bool bad    = _is_bad_food(*item);
            // Excempt undead from auto-eating since:
            //  * Mummies don't eat.
            //  * Vampire feeding takes a lot more time than eating a chunk
            //    and may have unintended consequences.
            //  * Ghouls may want to wait until chunks become rotten.
            if (easy_eat && !bad && !contam)
                // If this chunk is safe to eat, just do so without prompting.
                autoeat = true;
            else if (easy_contam && contam && !bad)
                autoeat = true;
            else
            {
                mprf(MSGCH_PROMPT, "%s %s%s? (ye/n/q/i?)",
                     (you.species == SP_VAMPIRE ? "Drink blood from" : "Eat"),
                     ((item->quantity > 1) ? "one of " : ""),
                     item_name.c_str());
            }

            int keyin = autoeat ? 'y' : tolower(getchm(KC_CONFIRM));
            switch (keyin)
            {
            case ESCAPE:
            case 'q':
                canned_msg(MSG_OK);
                return (-1);
            case 'i':
            case '?':
                // Skip ahead to the inventory.
                return (-2);
            case 'e':
            case 'y':
                if (can_ingest(item->base_type, item->sub_type, false))
                {
                    if (autoeat)
                    {
                        mprf("%s %s%s.",
                             (you.species == SP_VAMPIRE ? "Drinking blood from"
                                                        : "Eating"),
                             ((item->quantity > 1) ? "one of " : ""),
                             item_name.c_str());
                    }

                    if (in_inventory(*item))
                    {
                        eat_inventory_item(item->link);
                        return (1);
                    }
                    else
                    {
                        int ilink = item_on_floor(*item, you.pos());

                        if (ilink != NON_ITEM)
                        {
                            eat_floor_item(ilink);
                            return (1);
                        }
                        return (0);
                    }
                }
                break;
            default:
                // Else no: try next one.
                break;
            }
        }
    }

    return (0);
}

static const char *_chunk_flavour_phrase(bool likes_chunks)
{
    const char *phrase =
        likes_chunks ? "tastes great." : "tastes terrible.";

    if (!likes_chunks)
    {
        const int gourmand = you.duration[DUR_GOURMAND];
        if (gourmand >= GOURMAND_MAX)
        {
            phrase = one_chance_in(1000) ? "tastes like chicken!"
                                         : "tastes great.";
        }
        else if (gourmand > GOURMAND_MAX * 75 / 100)
            phrase = "tastes very good.";
        else if (gourmand > GOURMAND_MAX * 50 / 100)
            phrase = "tastes good.";
        else if (gourmand > GOURMAND_MAX * 25 / 100)
            phrase = "is not very appetising.";
    }

    return (phrase);
}

void chunk_nutrition_message(int nutrition)
{
    int perc_nutrition = nutrition * 100 / CHUNK_BASE_NUTRITION;
    if (perc_nutrition < 15)
        mpr("That was extremely unsatisfying.");
    else if (perc_nutrition < 35)
        mpr("That was not very filling.");
}

static int _apply_herbivore_chunk_effects(int nutrition)
{
    int how_herbivorous = player_mutation_level(MUT_HERBIVOROUS);

    while (how_herbivorous--)
        nutrition = nutrition * 80 / 100;

    return (nutrition);
}

static int _chunk_nutrition(bool likes_chunks)
{
    int nutrition = CHUNK_BASE_NUTRITION;

    if (likes_chunks || you.hunger_state < HS_SATIATED)
    {
        return (likes_chunks ? nutrition
                             : _apply_herbivore_chunk_effects(nutrition));
    }

    const int gourmand =
        wearing_amulet(AMU_THE_GOURMAND)? you.duration[DUR_GOURMAND] : 0;

    int effective_nutrition =
           nutrition * (gourmand + GOURMAND_NUTRITION_BASE)
                     / (GOURMAND_MAX + GOURMAND_NUTRITION_BASE);

#ifdef DEBUG_DIAGNOSTICS
    const int epercent = effective_nutrition * 100 / nutrition;
    mprf(MSGCH_DIAGNOSTICS,
            "Gourmand factor: %d, chunk base: %d, effective: %d, %%: %d",
                gourmand, nutrition, effective_nutrition, epercent);
#endif

    return (_apply_herbivore_chunk_effects(effective_nutrition));
}

static void _say_chunk_flavour(bool likes_chunks)
{
    mprf("This raw flesh %s", _chunk_flavour_phrase(likes_chunks));
}

// Never called directly - chunk_effect values must pass
// through food::_determine_chunk_effect() first. {dlb}:
static void _eat_chunk(corpse_effect_type chunk_effect, bool cannibal,
                       int mon_intel)
{
    bool likes_chunks = (you.omnivorous()
                         || player_mutation_level(MUT_CARNIVOROUS));
    int nutrition     = _chunk_nutrition(likes_chunks);
    int hp_amt        = 0;
    bool suppress_msg = false; // do we display the chunk nutrition message?
    bool do_eat       = false;

    if (you.species == SP_GHOUL)
    {
        nutrition    = CHUNK_BASE_NUTRITION;
        hp_amt       = 1 + random2(5) + random2(1 + you.experience_level);
        suppress_msg = true;
    }

    switch (chunk_effect)
    {
    case CE_MUTAGEN_RANDOM:
        mpr("This meat tastes really weird.");
        mutate(RANDOM_MUTATION);
        did_god_conduct( DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(100);
        break;

    case CE_MUTAGEN_BAD:
        mpr("This meat tastes *really* weird.");
        give_bad_mutation();
        did_god_conduct( DID_DELIBERATE_MUTATING, 10);
        xom_is_stimulated(random2(200));
        break;

    case CE_HCL:
        rot_player( 10 + random2(10) );
        if (disease_player( 50 + random2(100) ))
            xom_is_stimulated(random2(100));
        break;

    case CE_POISONOUS:
        mpr("Yeeuch - this meat is poisonous!");
        if (poison_player(3 + random2(4)))
            xom_is_stimulated(random2(128));
        break;

    case CE_ROTTEN:
    case CE_CONTAMINATED:
        if (player_mutation_level(MUT_SAPROVOROUS) == 3)
        {
            mprf("This %sflesh tastes delicious!",
                chunk_effect == CE_ROTTEN ? "rotting " : "");

            if (you.species == SP_GHOUL)
                _heal_from_food(hp_amt, 0, !one_chance_in(4), one_chance_in(5));

            do_eat = true;
        }
        else
        {
            mpr("There is something wrong with this meat.");
            if (disease_player(50 + random2(100)))
                xom_is_stimulated(random2(100));
        }
        break;

    case CE_CLEAN:
        if (player_mutation_level(MUT_SAPROVOROUS) == 3)
        {
            mpr("This raw flesh tastes good.");

            if (you.species == SP_GHOUL)
            {
                _heal_from_food((!one_chance_in(5) ? hp_amt : 0), 0,
                                !one_chance_in(3));
            }
        }
        else
            _say_chunk_flavour(likes_chunks);

        do_eat = true;
        break;

    case CE_MUTAGEN_GOOD:
    case CE_NOCORPSE:
    case CE_RANDOM:
        mprf(MSGCH_ERROR, "This flesh (%d) tastes buggy!", chunk_effect);
        break;
    }

    if (cannibal)
        did_god_conduct(DID_CANNIBALISM, 10);
    else if (mon_intel > 0)
        did_god_conduct(DID_EAT_SOULED_BEING, mon_intel);

    if (do_eat)
    {
        start_delay(DELAY_EAT, 2, (suppress_msg) ? 0 : nutrition, -1);
        lessen_hunger(nutrition, true);
    }
}

static void _eating(unsigned char item_class, int item_type)
{
    int food_value = 0;
    int how_herbivorous = player_mutation_level(MUT_HERBIVOROUS);
    int how_carnivorous = player_mutation_level(MUT_CARNIVOROUS);
    int carnivore_modifier = 0;
    int herbivore_modifier = 0;

    switch (item_class)
    {
    case OBJ_FOOD:
        // apply base sustenance {dlb}:
        switch (item_type)
        {
        case FOOD_MEAT_RATION:
        case FOOD_ROYAL_JELLY:
            food_value = 5000;
            break;
        case FOOD_BREAD_RATION:
            food_value = 4400;
            break;
        case FOOD_HONEYCOMB:
            food_value = 2000;
            break;
        case FOOD_SNOZZCUMBER:  // Maybe a nasty side-effect from RD's book?
                                // I'd like that, but I don't dare. (jpeg)
        case FOOD_PIZZA:
        case FOOD_BEEF_JERKY:
            food_value = 1500;
            break;
        case FOOD_CHEESE:
        case FOOD_SAUSAGE:
            food_value = 1200;
            break;
        case FOOD_ORANGE:
        case FOOD_BANANA:
        case FOOD_LEMON:
            food_value = 1000;
            break;
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
            food_value = 700;
            break;
        case FOOD_CHOKO:
        case FOOD_RAMBUTAN:
        case FOOD_LYCHEE:
            food_value = 600;
            break;
        case FOOD_STRAWBERRY:
            food_value = 200;
            break;
        case FOOD_GRAPE:
            food_value = 100;
            break;
        case FOOD_SULTANA:
            food_value = 70;     // Will not save you from starvation.
            break;
        default:
            break;
        }

        // Next, sustenance modifier for carnivores/herbivores {dlb}:
        switch (item_type)
        {
        case FOOD_MEAT_RATION:
            carnivore_modifier = 500;
            herbivore_modifier = -1500;
            break;
        case FOOD_BEEF_JERKY:
        case FOOD_SAUSAGE:
            carnivore_modifier = 200;
            herbivore_modifier = -200;
            break;
        case FOOD_BREAD_RATION:
            carnivore_modifier = -1000;
            herbivore_modifier = 500;
            break;
        case FOOD_BANANA:
        case FOOD_ORANGE:
        case FOOD_LEMON:
            carnivore_modifier = -300;
            herbivore_modifier = 300;
            break;
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
        case FOOD_CHOKO:
        case FOOD_SNOZZCUMBER:
        case FOOD_RAMBUTAN:
        case FOOD_LYCHEE:
            carnivore_modifier = -200;
            herbivore_modifier = 200;
            break;
        case FOOD_STRAWBERRY:
            carnivore_modifier = -50;
            herbivore_modifier = 50;
            break;
        case FOOD_GRAPE:
        case FOOD_SULTANA:
            carnivore_modifier = -20;
            herbivore_modifier = 20;
            break;
        default:
            carnivore_modifier = 0;
            herbivore_modifier = 0;
            break;
        }

        // Finally, modify player's hunger level {dlb}:
        if (carnivore_modifier && how_carnivorous > 0)
            food_value += (carnivore_modifier * how_carnivorous);

        if (herbivore_modifier && how_herbivorous > 0)
            food_value += (herbivore_modifier * how_herbivorous);

        if (food_value > 0)
        {
            int duration = 1;
            if (item_type == FOOD_MEAT_RATION || item_type == FOOD_BREAD_RATION)
                duration = 3;

            start_delay( DELAY_EAT, duration, 0, item_type );
            lessen_hunger( food_value, true );
        }
        break;

    default:
        break;
    }
}

// Handle messaging at the end of eating.
// Some food types may not get a message.
void finished_eating_message(int food_type)
{
    bool herbivorous = player_mutation_level(MUT_HERBIVOROUS) > 0;
    bool carnivorous = player_mutation_level(MUT_CARNIVOROUS) > 0;

    if (herbivorous)
    {
        switch (food_type)
        {
        case FOOD_MEAT_RATION:
        case FOOD_BEEF_JERKY:
        case FOOD_SAUSAGE:
            mpr("Blech - you need greens!");
            return;
        default:
            break;
        }
    }
    else
    {
        switch (food_type)
        {
        case FOOD_MEAT_RATION:
            mpr("That meat ration really hit the spot!");
            return;
        case FOOD_BEEF_JERKY:
            mprf("That beef jerky was %s!",
                 one_chance_in(4) ? "jerk-a-riffic"
                                  : "delicious");
            return;
        case FOOD_SAUSAGE:
            mpr("That sausage was delicious!");
            return;
        default:
            break;
        }
    }

    if (carnivorous)
    {
        switch (food_type)
        {
        case FOOD_BREAD_RATION:
        case FOOD_BANANA:
        case FOOD_ORANGE:
        case FOOD_LEMON:
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
        case FOOD_CHOKO:
        case FOOD_SNOZZCUMBER:
        case FOOD_RAMBUTAN:
        case FOOD_LYCHEE:
        case FOOD_STRAWBERRY:
        case FOOD_GRAPE:
        case FOOD_SULTANA:
            mpr("Blech - you need meat!");
            return;
        default:
            break;
        }
    }
    else
    {
        switch (food_type)
        {
        case FOOD_BREAD_RATION:
            mpr("That bread ration really hit the spot!");
            return;
        case FOOD_PEAR:
        case FOOD_APPLE:
        case FOOD_APRICOT:
            mprf("Mmmm... Yummy %s.",
                (food_type == FOOD_APPLE)   ? "apple." :
                (food_type == FOOD_PEAR)    ? "pear." :
                (food_type == FOOD_APRICOT) ? "apricot."
                                            : "fruit.");
            return;
        case FOOD_CHOKO:
            mpr("That choko was very bland.");
            return;
        case FOOD_SNOZZCUMBER:
            mpr("That snozzcumber tasted truly putrid!");
            return;
        case FOOD_ORANGE:
            mprf("That orange was delicious!%s",
                 one_chance_in(8) ? " Even the peel tasted good!" : "");
            return;
        case FOOD_BANANA:
            mprf("That banana was delicious!%s",
                 one_chance_in(8) ? " Even the peel tasted good!" : "");
            return;
        case FOOD_STRAWBERRY:
            mpr("That strawberry was delicious!");
            return;
        case FOOD_RAMBUTAN:
            mpr("That rambutan was delicious!");
            return;
        case FOOD_LEMON:
            mpr("That lemon was rather sour... but delicious nonetheless!");
            return;
        case FOOD_GRAPE:
            mpr("That grape was delicious!");
            return;
        case FOOD_SULTANA:
            mpr("That sultana was delicious! (but very small)");
            return;
        case FOOD_LYCHEE:
            mpr("That lychee was delicious!");
            return;
        default:
            break;
        }
    }

    switch (food_type)
    {
    case FOOD_HONEYCOMB:
        mpr("That honeycomb was delicious.");
        break;
    case FOOD_ROYAL_JELLY:
        mpr("That royal jelly was delicious!");
        restore_stat(STAT_ALL, 0, false);
        break;
    case FOOD_PIZZA:
        if (!SysEnv.crawl_pizza.empty() && !one_chance_in(3))
            mprf("Mmm... %s.", SysEnv.crawl_pizza.c_str());
        else
        {
            int temp_rand;
            if (carnivorous) // non-vegetable
                temp_rand = 5 + random2(4);
            else if (herbivorous) // non-meaty
                temp_rand = random2(6) + 2;
            else
                temp_rand = random2(9);

            mprf("Mmm... %s",
                (temp_rand == 0) ? "Ham and pineapple." :
                (temp_rand == 2) ? "Vegetable." :
                (temp_rand == 3) ? "Pepperoni." :
                (temp_rand == 4) ? "Yeuchh - Anchovies!" :
                (temp_rand == 5) ? "Cheesy." :
                (temp_rand == 6) ? "Supreme." :
                (temp_rand == 7) ? "Super Supreme!"
                                 : "Chicken.");
        }
        break;
    case FOOD_CHEESE:
    {
        int temp_rand = random2(9);
        mprf("Mmm...%s.",
             (temp_rand == 0) ? "Cheddar" :
             (temp_rand == 1) ? "Edam" :
             (temp_rand == 2) ? "Wensleydale" :
             (temp_rand == 3) ? "Camembert" :
             (temp_rand == 4) ? "Goat cheese" :
             (temp_rand == 5) ? "Fruit cheese" :
             (temp_rand == 6) ? "Mozzarella" :
             (temp_rand == 7) ? "Sheep cheese"
                              : "Yak cheese");
        break;
    }
    default:
        break;
    }
}

// Divide full nutrition by duration, so that each turn you get the same
// amount of nutrition. Also, experimentally regenerate 1 hp per feeding turn
// - this is likely too strong.
// feeding is -1 at start, 1 when finishing, and 0 else

// Here are some values for nutrition (quantity * 1000) and duration:
//    max_chunks      quantity    duration
//     1               1           1
//     2               1           1
//     3               1           2
//     4               1           2
//     5               1           2
//     6               2           3
//     7               2           3
//     8               2           3
//     9               2           4
//    10               2           4
//    12               3           5
//    15               3           5
//    20               4           6
//    25               4           6
//    30               5           7

void vampire_nutrition_per_turn(const item_def &corpse, int feeding)
{
    const int mons_type  = corpse.plus;
    const int chunk_type = _determine_chunk_effect(
                                mons_corpse_effect(mons_type), false);

    // Duration depends on corpse weight.
    const int max_chunks = mons_weight(mons_type)/150;
    int chunk_amount     = 1 + max_chunks/3;
        chunk_amount     = stepdown_value( chunk_amount, 6, 6, 12, 12 );

    // Add 1 for the artificial extra call at the start of draining.
    const int duration   = 1 + chunk_amount;

    // Use number of potions per corpse to calculate total nutrition, which
    // then gets distributed over the entire duration.
    int food_value = CHUNK_BASE_NUTRITION
                     * num_blood_potions_from_corpse(mons_type, chunk_type);

    bool start_feeding   = false;
    bool during_feeding  = false;
    bool end_feeding     = false;

    if (feeding < 0)
        start_feeding = true;
    else if (feeding > 0)
        end_feeding = true;
    else
        during_feeding = true;

    switch (mons_type)
    {
        case MONS_HUMAN:
            food_value += random2avg((you.experience_level * 10)/duration, 2);

            // Human may give a bit of healing during feeding as long as
            // the corpse is still fairly fresh.
            if (corpse.special > 150)
            {
                int hp_amt = 1 + you.experience_level/3;

                if (!end_feeding)
                {
                    if (start_feeding)
                        mpr("This warm blood tastes really delicious!");

                    if (hp_amt >= duration)
                        hp_amt /= duration;
                    else if (x_chance_in_y(hp_amt, duration))
                        hp_amt = 1;

                    _heal_from_food(hp_amt);
                }
                else
                {
                    // Give the remainder of healing at the end.
                    if (hp_amt > duration)
                        _heal_from_food(hp_amt % duration);
                }
            }
            break;

        case MONS_ELF:
            food_value += random2avg((you.experience_level * 10)/duration, 2);

            // Elven blood gives a bit of mana at the end of feeding, but
            // only from fairly fresh corpses.
            if (corpse.special > 150)
            {
                if (end_feeding)
                {
                    const int mp_amt = 1 + random2(3);
                    _heal_from_food(1, mp_amt);
                }
                else if (start_feeding)
                    mpr("This warm blood tastes magically delicious!");
            }
            break;

        default:
            switch (chunk_type)
            {
                case CE_CLEAN:
                    if (start_feeding)
                        mpr("This warm blood tastes delicious!");
                    else if (end_feeding && corpse.special > 150)
                        _heal_from_food(1);
                    break;

                case CE_CONTAMINATED:
                    food_value /= 2;

                    if (start_feeding)
                        mpr("Somehow this blood was not very filling!");
                    else if (end_feeding && corpse.special > 150)
                        _heal_from_food(1);
                    break;

                case CE_POISONOUS:
                    make_hungry(food_value/2, false);
                    // Always print this message - maybe you lost poison res.
                    // due to feeding.
                    mpr("Blech - this blood tastes nasty!");
                    if (poison_player( 1 + random2(3) ))
                        xom_is_stimulated(random2(128));
                    stop_delay();
                    return;

                case CE_MUTAGEN_RANDOM:
                    food_value /= 2;
                    if (start_feeding)
                        mpr("This blood tastes really weird!");
                    mutate(RANDOM_MUTATION);
                    did_god_conduct( DID_DELIBERATE_MUTATING, 10);
                    xom_is_stimulated(100);
                    // Sometimes heal by one hp.
                    if (end_feeding && corpse.special > 150 && coinflip())
                        _heal_from_food(1);
                    break;

                case CE_MUTAGEN_BAD:
                    food_value /= 2;
                    if (start_feeding)
                        mpr("This blood tastes *really* weird.");
                    give_bad_mutation();
                    did_god_conduct( DID_DELIBERATE_MUTATING, 10);
                    xom_is_stimulated(random2(200));
                    // No healing from bad mutagenic blood.
                    break;

                case CE_HCL:
                    rot_player( 5 + random2(5) );
                    if (disease_player( 50 + random2(100) ))
                        xom_is_stimulated(random2(100));
                    stop_delay();
                    break;
            }
    }

    if (!end_feeding)
        lessen_hunger(food_value / duration, !start_feeding);
}

// Returns true if a food item (also corpses) is poisonous AND the player
// is not (known to be) poison resistant.
bool is_poisonous(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return (false);

    if (player_res_poison(false))
        return (false);

    return (mons_corpse_effect(food.plus) == CE_POISONOUS);
}

// Returns true if a food item (also corpses) is mutagenic.
bool is_mutagenic(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return (false);

    // Has no effect on ghouls.
    if (you.species == SP_GHOUL)
        return (false);

    return (mons_corpse_effect(food.plus) == CE_MUTAGEN_RANDOM);
}

// Returns true if a food item (also corpses) may cause sickness.
bool is_contaminated(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return (false);

    // Has no (bad) effect on ghouls.
    if (you.species == SP_GHOUL)
        return (false);

    return (mons_corpse_effect(food.plus) == CE_CONTAMINATED);
}

// Returns true if a food item (also corpses) will cause rotting.
bool causes_rot(const item_def &food)
{
    if (food.base_type != OBJ_FOOD && food.base_type != OBJ_CORPSES)
        return (false);

    // Has no effect on ghouls.
    if (you.species == SP_GHOUL)
        return (false);

    return (mons_corpse_effect(food.plus) == CE_HCL);
}

// Returns 1 for herbivores, -1 for carnivores and 0 for either.
static int _player_likes_food_type(int type)
{
    switch (static_cast<food_type>(type))
    {
    case FOOD_BREAD_RATION:
    case FOOD_PEAR:
    case FOOD_APPLE:
    case FOOD_CHOKO:
    case FOOD_SNOZZCUMBER:
    case FOOD_PIZZA:
    case FOOD_APRICOT:
    case FOOD_ORANGE:
    case FOOD_BANANA:
    case FOOD_STRAWBERRY:
    case FOOD_RAMBUTAN:
    case FOOD_LEMON:
    case FOOD_GRAPE:
    case FOOD_SULTANA:
    case FOOD_LYCHEE:
    case FOOD_CHEESE:
        return 1;

    case FOOD_CHUNK:
    case FOOD_MEAT_RATION:
    case FOOD_SAUSAGE:
    case FOOD_BEEF_JERKY:
        return -1;

    case FOOD_HONEYCOMB:
    case FOOD_ROYAL_JELLY:
        return 0;

    case NUM_FOODS:
        mpr("Bad food type", MSGCH_ERROR);
        return 0;
    }

    mprf(MSGCH_ERROR, "Couldn't handle food type: %d", type);
    return 0;
}

// Returns true if an item of basetype FOOD or CORPSES cannot currently
// be eaten (respecting species and mutations set).
bool is_inedible(const item_def &item)
{
    if (food_is_rotten(item)
        && !player_mutation_level(MUT_SAPROVOROUS))
    {
        return (true);
    }

    if (item.base_type == OBJ_FOOD
        && !can_ingest(item.base_type, item.sub_type, true, true, false))
    {
        return (true);
    }
    if (item.base_type == OBJ_CORPSES
        && (item.sub_type == CORPSE_SKELETON
            || you.species == SP_VAMPIRE && !mons_has_blood(item.plus)))
    {
        return (true);
    }
    return (false);
}
// As we want to avoid autocolouring the entire food selection, this should
// be restricted to the absolute highlights, even though other stuff may
// still be edible or even delicious.
bool is_preferred_food(const item_def &food)
{
    // Vampires don't really have a preferred food type, but they really
    // like blood potions.
    if (you.species == SP_VAMPIRE)
        return (is_blood_potion(food));

    if (food.base_type == OBJ_POTIONS && food.sub_type == POT_PORRIDGE
        && item_type_known(food))
    {
        return (!player_mutation_level(MUT_CARNIVOROUS));
    }

    if (food.base_type != OBJ_FOOD)
        return (false);

    if (is_poisonous(food))
        return (false);

    // Honeycombs are tasty for everyone.
    if (food.sub_type == FOOD_HONEYCOMB || food.sub_type == FOOD_ROYAL_JELLY)
        return (true);

    // Ghouls specifically like rotten food.
    if (you.species == SP_GHOUL)
        return (food_is_rotten(food));

    if (player_mutation_level(MUT_CARNIVOROUS) == 3)
        return (_player_likes_food_type(food.sub_type) < 0);

    if (player_mutation_level(MUT_HERBIVOROUS) == 3)
        return (_player_likes_food_type(food.sub_type) > 0);

    // No food preference.
    return (false);
}

bool is_forbidden_food(const item_def &food)
{
    if (food.base_type != OBJ_CORPSES
        && (food.base_type != OBJ_FOOD || food.sub_type != FOOD_CHUNK))
    {
        return (false);
    }

    // Some gods frown upon cannibalistic behaviour.
    if (god_hates_cannibalism(you.religion)
        && is_player_same_species(food.plus))
    {
        return (true);
    }

    // Zin doesn't like it if you eat beings with a soul.
    if (you.religion == GOD_ZIN && mons_class_intel(food.plus) >= I_NORMAL)
        return (true);

    // Everything else is allowed.
    return (false);
}

bool can_ingest(int what_isit, int kindof_thing, bool suppress_msg, bool reqid,
                bool check_hunger)
{
    bool survey_says = false;

    // [ds] These redundant checks are now necessary - Lua might be calling us.
    if (you.is_undead == US_UNDEAD)
    {
        if (!suppress_msg)
            mpr("You can't eat.");
        return (false);
    }

    if (check_hunger && you.hunger >= 11000)
    {
        if (!suppress_msg)
            mpr("You're too full to eat anything.");
        return (false);
    }

    if (you.species == SP_VAMPIRE)
    {
        if (what_isit == OBJ_CORPSES && kindof_thing == CORPSE_BODY)
            return (true);

        if (what_isit == OBJ_POTIONS && (kindof_thing == POT_BLOOD
            || kindof_thing == POT_BLOOD_COAGULATED))
        {
            return (true);
        }

        if (!suppress_msg)
            mpr("Blech - you need blood!");

        return (false);
    }

    bool ur_carnivorous = (player_mutation_level(MUT_CARNIVOROUS) == 3);

    bool ur_herbivorous = (player_mutation_level(MUT_HERBIVOROUS) == 3);

    // ur_chunkslover not defined in terms of ur_carnivorous because
    // a player could be one and not the other IMHO - 13mar2000 {dlb}
    bool ur_chunkslover = (
                (check_hunger ? you.hunger_state < HS_SATIATED : true)
                           || you.omnivorous()
                           || player_mutation_level(MUT_CARNIVOROUS));

    switch (what_isit)
    {
    case OBJ_FOOD:
    {
        if (you.species == SP_VAMPIRE)
        {
            if (!suppress_msg)
                mpr("Blech - you need blood!");
             return (false);
        }

        int vorous = _player_likes_food_type(kindof_thing);
        if (vorous > 0) // Herbivorous food.
        {
            if (ur_carnivorous)
            {
                if (!suppress_msg)
                    mpr("Sorry, you're a carnivore.");
                return (false);
            }
            else
                return (true);
        }
        else if (vorous < 0) // Carnivorous food.
        {
            if (ur_herbivorous)
            {
                if (!suppress_msg)
                    mpr("Blech - you need greens!");
                return (false);
            }
            else if (kindof_thing == FOOD_CHUNK)
            {
                if (ur_chunkslover)
                    return (true);

                // Else, we're not hungry enough.
                if (wearing_amulet(AMU_THE_GOURMAND, !reqid))
                {
                    const int amulet = you.equip[EQ_AMULET];

                    ASSERT(amulet != -1);

                    if (!item_type_known(you.inv[amulet]))
                    {
                        // For artefact amulets, this will tell you its name
                        // and subtype. Other properties may still be hidden.
                        set_ident_flags( you.inv[ amulet], ISFLAG_KNOW_TYPE);
                        set_ident_type( OBJ_JEWELLERY, AMU_THE_GOURMAND,
                                        ID_KNOWN_TYPE );
                        mpr(you.inv[amulet].name(DESC_INVENTORY, false).c_str());
                    }
                    return (true);
                }
                if (!suppress_msg)
                    mpr("You aren't quite hungry enough to eat that!");
                return (false);
            }
        }
        // Any food types not specifically handled until here (e.g. meat
        // rations for non-herbivores) are okay.
        return (true);
    }
    case OBJ_CORPSES:
        if (you.species == SP_VAMPIRE)
        {
            if (kindof_thing == CORPSE_BODY)
                return (true);
            else
            {
                if (!suppress_msg)
                    mpr("Blech - you need blood!");
                return (false);
            }
        }
        return (false);

    case OBJ_POTIONS: // called by lua
        if (get_ident_type(OBJ_POTIONS, kindof_thing) != ID_KNOWN_TYPE)
            return (true);

        switch (kindof_thing)
        {
            case POT_BLOOD:
            case POT_BLOOD_COAGULATED:
                if (ur_herbivorous)
                {
                    if (!suppress_msg)
                        mpr("Urks, you're a herbivore!");
                    return (false);
                }
                return (true);
            case POT_WATER:
                if (you.species == SP_VAMPIRE)
                {
                    if (!suppress_msg)
                        mpr("Blech - you need blood!");
                    return (false);
                }
                return (true);
             case POT_PORRIDGE:
                if (you.species == SP_VAMPIRE)
                {
                    if (!suppress_msg)
                        mpr("Blech - you need blood!");
                    return (false);
                }
                else if (ur_carnivorous)
                {
                    if (!suppress_msg)
                        mpr("Sorry, you're a carnivore.");
                    return (false);
                }
             default:
                return (true);
        }

    // Other object types are set to return false for now until
    // someone wants to recode the eating code to permit consumption
    // of things other than just food.
    default:
        return (false);
    }

    return (survey_says);
}

// See if you can follow along here -- except for the amulet of the gourmand
// addition (long missing and requested), what follows is an expansion of how
// chunks were handled in the codebase up to this date ... {dlb}
static corpse_effect_type _determine_chunk_effect(corpse_effect_type chunktype,
                                                  bool rotten_chunk)
{
    // Determine the initial effect of eating a particular chunk. {dlb}
    switch (chunktype)
    {
    case CE_HCL:
    case CE_MUTAGEN_RANDOM:
        if (you.species == SP_GHOUL)
            chunktype = CE_CLEAN;
        break;

    case CE_POISONOUS:
        if (player_res_poison() > 0)
            chunktype = CE_CLEAN;
        break;

    case CE_CONTAMINATED:
        switch (player_mutation_level(MUT_SAPROVOROUS))
        {
        case 1:
            if (!one_chance_in(15))
                chunktype = CE_CLEAN;
            break;

        case 2:
            if (!one_chance_in(45))
                chunktype = CE_CLEAN;
            break;

        default:
            if (!one_chance_in(3))
                chunktype = CE_CLEAN;
            break;
        }
        break;

    default:
        break;
    }

    // Determine effects of rotting on base chunk effect {dlb}:
    if (rotten_chunk)
    {
        switch (chunktype)
        {
        case CE_CLEAN:
        case CE_CONTAMINATED:
            chunktype = CE_ROTTEN;
            break;
        case CE_MUTAGEN_RANDOM:
            chunktype = CE_MUTAGEN_BAD;
            break;
        default:
            break;
        }
    }

    // One last chance for some species to safely eat rotten food. {dlb}
    if (chunktype == CE_ROTTEN)
    {
        switch (player_mutation_level(MUT_SAPROVOROUS))
        {
        case 1:
            if (!one_chance_in(5))
                chunktype = CE_CLEAN;
            break;

        case 2:
            if (!one_chance_in(15))
                chunktype = CE_CLEAN;
            break;

        default:
            break;
        }
    }

    // The amulet of the gourmand will permit consumption of
    // contaminated meat as though it were "clean" meat - level 3
    // saprovores get rotting meat effect from clean chunks, since they
    // love rotting meat.
    if (wearing_amulet(AMU_THE_GOURMAND)
        && x_chance_in_y(you.duration[DUR_GOURMAND], GOURMAND_MAX))
    {
        if (player_mutation_level(MUT_SAPROVOROUS) == 3)
        {
            // [dshaligram] Level 3 saprovores relish contaminated meat.
            if (chunktype == CE_CLEAN)
                chunktype = CE_CONTAMINATED;
        }
        else
        {
            // [dshaligram] New AotG behaviour - contaminated chunks become
            // clean, but rotten chunks remain rotten.
            if (chunktype == CE_CONTAMINATED)
                chunktype = CE_CLEAN;
        }
    }

    return (chunktype);
}

static bool _vampire_consume_corpse(int slot, bool invent)
{
    ASSERT(you.species == SP_VAMPIRE);

    item_def &corpse = (invent ? you.inv[slot]
                               : mitm[slot]);

    ASSERT(corpse.base_type == OBJ_CORPSES);
    ASSERT(corpse.sub_type == CORPSE_BODY);

    if (!mons_has_blood(corpse.plus))
    {
        mpr( "There is no blood in this body!" );
        return (false);
    }

    if (food_is_rotten(corpse))
    {
        mpr("It's not fresh enough.");
        return (false);
    }

    // The delay for eating a chunk (mass 1000) is 2
    // Here the base nutrition value equals that of chunks,
    // but the delay should be smaller.
    const int max_chunks = mons_weight(corpse.plus)/150;
    int duration = 1 + max_chunks/3;
        duration = stepdown_value( duration, 6, 6, 12, 12 );

    // Get some nutrition right away, in case we're interrupted.
    // (-1 for the starting message.)
    vampire_nutrition_per_turn(corpse, -1);
    // The draining delay doesn't have a start action, and we only need
    // the continue/finish messages if it takes longer than 1 turn.
    start_delay(DELAY_FEED_VAMPIRE, duration, (int)invent, slot);

    return (true);
}

static void _heal_from_food(int hp_amt, int mp_amt, bool unrot,
                            bool restore_str)
{
    if (hp_amt > 0)
        inc_hp(hp_amt, false);

    if (mp_amt > 0)
        inc_mp(mp_amt, false);

    if (unrot && player_rotted())
    {
        mpr("You feel more resilient.");
        unrot_hp(1);
    }

    if (restore_str && you.strength < you.max_strength)
        restore_stat(STAT_STRENGTH, 1, false);

    calc_hp();
    calc_mp();
}

int you_max_hunger()
{
    // This case shouldn't actually happen.
    if (you.is_undead == US_UNDEAD)
        return (6000);

    // Take care of ghouls - they can never be 'full'.
    if (you.species == SP_GHOUL)
        return (6999);

    return (40000);
}

int you_min_hunger()
{
    // This case shouldn't actually happen.
    if (you.is_undead == US_UNDEAD)
        return (6000);

    // Vampires can never starve to death.
    if (you.species == SP_VAMPIRE)
        return (701);

    return (0);
}
