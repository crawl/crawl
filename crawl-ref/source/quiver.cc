/*
 *  File:       quiver.cc
 *  Summary:    Player quiver functionality
 *
 *  Last modified by $Author: $ on $Date: $
 *  
 *  - Only change last_used when actually using
 *  - Not changing Qv; nobody knows about internals
 *  - Track last_used of each type so each weapon can do the right thing
 */

#include "AppHdr.h"
#include "quiver.h"

#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "stuff.h"

// checks base_type for OBJ_UNASSIGNED, and quantity
// bool is_valid_item( const item_def &item )

static int _get_pack_slot(const item_def&);
static ammo_t _get_weapon_ammo_type();
static bool _item_matches(const item_def &item, fire_type types);

// ----------------------------------------------------------------------
// player_quiver
// ----------------------------------------------------------------------

player_quiver::player_quiver()
    : m_last_used_type(AMMO_INVALID)
{
    COMPILE_CHECK(ARRAYSIZE(m_last_used_of_type) == NUM_AMMO, a);
}

// Return item that we would like to fire by default, and its inv slot
// (if there are any in inv).  If the item is in inv, its count will
// be correct.
// This is the item that will be displayed in Qv:
void player_quiver::get_desired_item(const item_def** item_out, int* slot_out) const
{
    if (m_last_used_type == AMMO_INVALID)
    {
        *item_out = NULL;
        *slot_out = -1;
    }
    else
    {
        *slot_out = _get_pack_slot(m_last_used_of_type[m_last_used_type]);
        if (*slot_out == -1)
        {
            // Not in inv, but caller can at least get the type of the item.
            *item_out = &m_last_used_of_type[m_last_used_type];
        }
        else
        {
            // Return the item in inv, since it will have an accurate count
            *item_out = &you.inv[*slot_out];
        }
    }
}

// Return inv slot of item that should be fired by default.
// This is the first item displayed in the fire interface.
int player_quiver::get_default_slot(std::string& no_item_reason) const
{
    int slot;
    const item_def* desired_item;

    get_desired_item(&desired_item, &slot);

    // If not in inv, try the head of the fire order
    if (slot == -1)
    {
        std::vector<int> order;
        _get_fire_order(order, false);
        if (order.size())
            slot = order[0];
    }

    // If we can't find anything, tell caller why
    if (slot == -1)
    {
        std::vector<int> full_fire_order;
        _get_fire_order(full_fire_order, true);
        if (full_fire_order.size() == 0)
        {
            no_item_reason = "No suitable missiles.";
        }            
        else
        {
            const int skipped_item = full_fire_order[0];
            if (skipped_item < Options.fire_items_start)
            {
                no_item_reason = make_stringf(
                    "Nothing suitable (fire_items_start = '%c').",
                    index_to_letter(Options.fire_items_start));
            }
            else
            {
                no_item_reason = make_stringf(
                    "Nothing suitable (ignored '=f'-inscribed item on '%c').",
                    index_to_letter(skipped_item));
            }
        }
    }

    return slot;
}

// Notification that ltem was fired with 'f'
void player_quiver::on_item_fired(const item_def& item)
{
    // If item matches the launcher, put it in that launcher's last-used item.
    // Otherwise, it goes into last hand-thrown item.

    const item_def *weapon = you.weapon();
    
    if (weapon && item.launched_by(*weapon))
    {
        const ammo_t t = _get_weapon_ammo_type();
        m_last_used_of_type[t] = item;
        m_last_used_of_type[t].quantity = 1;    // ensure it's valid
    }
    else
    {
        m_last_used_of_type[AMMO_THROW] = item;
        m_last_used_of_type[AMMO_THROW].quantity = 1;
    }
}

// Notification that ltem was fired with 'f' 't'
void player_quiver::on_item_thrown(const item_def& item)
{
    m_last_used_of_type[AMMO_THROW] = item;
    m_last_used_of_type[AMMO_THROW].quantity = 1;
}

void player_quiver::on_weapon_changed()
{
    m_last_used_type = _get_weapon_ammo_type();
}

void player_quiver::_get_fire_order(std::vector<int>& order, bool ignore_inscription_etc) const
{
    const int inv_start = (ignore_inscription_etc ? 0 : Options.fire_items_start);

    // If in a net, cannot throw anything, and can only launch from blowgun
    if (you.attribute[ATTR_HELD])
    {
        const item_def *weapon = you.weapon();
        if (weapon && weapon->sub_type == WPN_BLOWGUN)
            for (int i_inv=inv_start; i_inv<ENDOFPACK; i_inv++)
                if (is_valid_item(you.inv[i_inv]) && you.inv[i_inv].launched_by(*weapon))
                    order.push_back(i_inv);
        return;
    }

    for (int i_inv=inv_start; i_inv<ENDOFPACK; i_inv++)
    {
        const item_def& item = you.inv[i_inv];
        if (!is_valid_item(item))
            continue;
        if (you.equip[EQ_WEAPON] == i_inv)
            continue;

        // =f prevents item from being in fire order
        if (!ignore_inscription_etc &&
            strstr(item.inscription.c_str(), "=f"))
        {
            continue;
        }

        for (unsigned int i_flags=0;
             i_flags<Options.fire_order.size();
             i_flags++)
        {
            if (_item_matches(item, (fire_type)Options.fire_order[i_flags]))
            {
                order.push_back( (i_flags<<16) | (i_inv & 0xffff) );
                break;
            }
        }
    }

    std::sort(order.begin(), order.end());

    for (unsigned int i=0; i<order.size(); i++)
    {
        order[i] &= 0xffff;
    }
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

// Helper for _get_fire_order
// types may actually contain more than one fire_type
static bool _item_matches(const item_def &item, fire_type types)
{
    ASSERT(!is_valid_item(item));

    if (types & FIRE_INSCRIBED)
        if (item.inscription.find("+f", 0) != std::string::npos)
            return true;

    if (item.base_type == OBJ_MISSILES)
    {
        if ((types & FIRE_DART) && item.sub_type == MI_DART)
            return (true);
        if ((types & FIRE_STONE) && item.sub_type == MI_STONE)
            return (true);
        if ((types & FIRE_JAVELIN) && item.sub_type == MI_JAVELIN)
            return (true);
        if ((types & FIRE_ROCK) && item.sub_type == MI_LARGE_ROCK)
            return (true);
        if ((types & FIRE_NET) && item.sub_type == MI_THROWING_NET)
            return (true);

        if (types & FIRE_LAUNCHER)
        {
            const item_def *weapon = you.weapon();
            if (weapon && item.launched_by(*weapon))
                return (true);
        }
    }
    else if (item.base_type == OBJ_WEAPONS
             && is_throwable(item, you.body_size()))
    {
        if (  (types & FIRE_RETURNING)
           && item.special == SPWPN_RETURNING
           && item_ident(item, ISFLAG_KNOW_TYPE))
        {
            return (true);
        }
        if ((types & FIRE_DAGGER) && item.sub_type == WPN_DAGGER)
            return (true);
        if ((types & FIRE_SPEAR) && item.sub_type == WPN_SPEAR)
            return (true);
        if ((types & FIRE_HAND_AXE) && item.sub_type == WPN_HAND_AXE)
            return (true);
        if ((types & FIRE_CLUB) && item.sub_type == WPN_CLUB)
            return (true);
    }
    return (false);
}

// Return inv slot that contains an item that looks like item,
// or -1 if not in inv.
static int _get_pack_slot(const item_def& item)
{
    if (! is_valid_item(item))
        return -1;

    for (int i=0; i<ENDOFPACK; i++)
    {
        if (items_stack(item, you.inv[i]))
            return i;
    }

    return -1;
}


// Return the type of ammo used by the player's equipped weapon,
// or AMMO_THROW if it's not a launcher.
static ammo_t _get_weapon_ammo_type()
{
    const int wielded = you.equip[EQ_WEAPON];
    if (wielded == -1)
        return (AMMO_THROW);

    item_def &weapon = you.inv[wielded];

    if (weapon.base_type != OBJ_WEAPONS)
        return (AMMO_THROW);

    switch (weapon.sub_type)
    {
        case WPN_BLOWGUN:
            return (AMMO_BLOWGUN);
        case WPN_SLING:
            return (AMMO_SLING);
        case WPN_BOW:
        case WPN_LONGBOW:
            return (AMMO_BOW);
        case WPN_CROSSBOW:
            return (AMMO_CROSSBOW);
        case WPN_HAND_CROSSBOW:
            return (AMMO_HAND_CROSSBOW);
        default:
            return (AMMO_THROW);
    }
}
