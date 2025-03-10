/**
 * @file
 * @brief Functions for letting the player adjust letter assignments.
 **/

#include "AppHdr.h"

#include "adjust.h"

#include "ability.h"
#include "libutil.h"
#include "invent.h"
#include "items.h"
#include "macro.h"
#include "message.h"
#include "prompt.h"
#include "spl-util.h"
#include "ui.h"

static void _adjust_spell();
static void _adjust_ability();

void adjust()
{
    mprf(MSGCH_PROMPT, "Adjust (i)tems, (s)pells, or (a)bilities? ");

    const int keyin = toalower(get_ch());

    if (keyin == 'i')
        adjust_item();
    else if (keyin == 's')
        _adjust_spell();
    else if (keyin == 'a')
        _adjust_ability();
    else if (key_is_escape(keyin))
        canned_msg(MSG_OK);
    else
        canned_msg(MSG_HUH);
}

void adjust_item(int from_slot)
{
#ifdef USE_TILE_WEB
    ui::cutoff_point ui_cutoff_point;
#endif
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (from_slot == -1)
    {
        from_slot = prompt_invent_item("Adjust which item?",
                                       menu_type::invlist, OSEL_ANY);
        if (prompt_failed(from_slot))
            return;

        mprf_nocap("%s", you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());
    }

    const int to_slot = prompt_invent_item("Adjust to which letter? ",
                                           menu_type::invlist,
                                           OSEL_ANY, OPER_ANY,
                                           invprompt_flag::unthings_ok
                                            | invprompt_flag::manual_list);
    if (to_slot == PROMPT_ABORT
        || from_slot == to_slot)
    {
        canned_msg(MSG_OK);
        return;
    }

    swap_inv_slots(from_slot, to_slot, true);
    you.wield_change = true;
    quiver::set_needs_redraw();
}

static void _adjust_spell()
{
    if (!you.spell_no)
    {
        canned_msg(MSG_NO_SPELLS);
        return;
    }

    // Select starting slot
    mprf(MSGCH_PROMPT, "Adjust which spell? ");
    int keyin = list_spells(false, false, false, "adjust");

    if (!isaalpha(keyin))
    {
        canned_msg(MSG_OK);
        return;
    }

    const int input_1 = keyin;
    const int index_1 = letter_to_index(input_1);
    spell_type spell  = get_spell_by_letter(input_1);

    if (spell == SPELL_NO_SPELL)
    {
        mpr("You don't know that spell.");
        return;
    }

    // Print targeted spell.
    mprf_nocap("%c - %s", keyin, spell_title(spell));

    // Select target slot.
    keyin = 0;
    while (!isaalpha(keyin))
    {
        mprf(MSGCH_PROMPT, "Adjust to which letter? ");
        keyin = get_ch();
        if (key_is_escape(keyin))
        {
            canned_msg(MSG_OK);
            return;
        }
        // FIXME: It would be nice if the user really could select letters
        // without spells from this menu.
        // XX this does not really work well with new menu code
        if (keyin == '?' || keyin == '*')
        {
            keyin = list_spells(true, false, false, "adjust it to");
            if (keyin < 'a' || keyin > 'Z')
                continue;
        }
    }

    const int input_2 = keyin;
    const int index_2 = letter_to_index(keyin);

    if (index_1 == index_2)
    {
        canned_msg(MSG_OK);
        return;
    }

    // swap references in the letter table:
    const int tmp = you.spell_letter_table[index_2];
    you.spell_letter_table[index_2] = you.spell_letter_table[index_1];
    you.spell_letter_table[index_1] = tmp;

    // print out spell in new slot
    mprf_nocap("%c - %s", input_2, spell_title(get_spell_by_letter(input_2)));

    // print out other spell if one was involved (now at input_1)
    spell = get_spell_by_letter(input_1);

    if (spell != SPELL_NO_SPELL)
        mprf_nocap("%c - %s", input_1, spell_title(spell));
}

static void _adjust_ability()
{
    const vector<talent> talents = your_talents(false);

    if (talents.empty())
    {
        mpr("You don't currently have any abilities.");
        return;
    }

    mprf(MSGCH_PROMPT, "Adjust which ability? ");
    int selected = choose_ability_menu(talents);

    // If we couldn't find anything, cancel out.
    if (selected == -1)
    {
        mpr("No such ability.");
        return;
    }

    char old_key = static_cast<char>(talents[selected].hotkey);

    mprf_nocap("%c - %s", old_key, ability_name(talents[selected].which).c_str());

    const int index1 = letter_to_index(old_key);

    mprf(MSGCH_PROMPT, "Adjust to which letter?");

    const int keyin = get_ch();

    if (!isaalpha(keyin))
    {
        canned_msg(MSG_HUH);
        return;
    }

    swap_ability_slots(index1, letter_to_index(keyin));
}

void swap_inv_slots(int from_slot, int to_slot, bool verbose)
{
    int new_quiver = -1;
    if (you.quiver_action.item_is_quivered(from_slot))
        new_quiver = to_slot;
    else if (you.quiver_action.item_is_quivered(to_slot))
        new_quiver = from_slot;

    // Swap items.
    item_def tmp = you.inv[to_slot];
    you.inv[to_slot]   = you.inv[from_slot];
    you.inv[from_slot] = tmp;

    // Slot switching.
    tmp.slot = you.inv[to_slot].slot;
    you.inv[to_slot].slot  = index_to_letter(to_slot);//you.inv[from_slot].slot is 0 when 'from_slot' contains no item.
    you.inv[from_slot].slot = tmp.slot;

    you.inv[from_slot].link = from_slot;
    you.inv[to_slot].link  = to_slot;

    bool wield_change = false;
    for (player_equip_entry& entry : you.equipment.items)
    {
        if (entry.item == from_slot)
        {
            entry.item = to_slot;
            if (you.inv[from_slot].base_type == OBJ_WEAPONS)
                wield_change = true;
        }
        else if (entry.item == to_slot)
        {
            entry.item = from_slot;
            if (you.inv[to_slot].base_type == OBJ_WEAPONS)
                wield_change = true;
        }
    }

    if (new_quiver >= 0)
        you.quiver_action.replace(quiver::slot_to_action(new_quiver, true));
    you.m_quiver_history.maybe_swap(from_slot, to_slot);

    if (verbose)
    {
        mprf_nocap("%s", you.inv[to_slot].name(DESC_INVENTORY_EQUIP).c_str());

        if (you.inv[from_slot].defined())
            mprf_nocap("%s", you.inv[from_slot].name(DESC_INVENTORY_EQUIP).c_str());
    }

    if (wield_change)
    {
        you.wield_change = true;
        quiver::on_weapon_changed();
    }
    else // just to make sure
        quiver::set_needs_redraw();

    // Swap the moved items in last_pickup if they're there.
    if (!you.last_pickup.empty())
    {
        auto &last_pickup = you.last_pickup;
        int to_count = lookup(last_pickup, to_slot, 0);
        int from_count = lookup(last_pickup, from_slot, 0);
        last_pickup.erase(to_slot);
        last_pickup.erase(from_slot);
        if (from_count > 0)
            last_pickup[to_slot] = from_count;
        if (to_count > 0)
            last_pickup[from_slot] = to_count;
    }
    if (you.last_unequip == from_slot)
        you.last_unequip = to_slot;
}
