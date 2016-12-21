/**
 * @file
 * @brief Player quiver functionality
 *
 * - Only change last_used when actually using
 * - Not changing Qv; nobody knows about internals
 * - Track last_used of each type so each weapon can do the right thing
**/

#include "AppHdr.h"

#include "quiver.h"

#include <algorithm>

#include "env.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "options.h"
#include "player.h"
#include "prompt.h"
#include "stringutil.h"
#include "tags.h"
#include "throw.h"

static int _get_pack_slot(const item_def&);
static ammo_t _get_weapon_ammo_type(const item_def*);
static bool _item_matches(const item_def &item, fire_type types,
                          const item_def* launcher, bool manual);
static bool _items_similar(const item_def& a, const item_def& b,
                           bool force = true);

// ----------------------------------------------------------------------
// player_quiver
// ----------------------------------------------------------------------

player_quiver::player_quiver()
    : m_last_used_type(AMMO_THROW)
{
    COMPILE_CHECK(ARRAYSZ(m_last_used_of_type) == NUM_AMMO);
}

// Return:
//   *slot_out filled in with the inv slot of the item we would like
//   to fire by default. If -1, the inv doesn't contain our desired
//   item.
//
//   *item_out filled in with item we would like to fire by default.
//   This can be returned even if the item is not in inv (although if
//   it is in inv, a reference to the inv item, with accurate count,
//   is returned)
//
// This is the item that will be displayed in Qv:
//
void player_quiver::get_desired_item(const item_def** item_out, int* slot_out) const
{
    const int slot = _get_pack_slot(m_last_used_of_type[m_last_used_type]);
    if (slot == -1)
    {
        // Not in inv, but caller can at least get the type of the item.
        if (item_out)
            *item_out = &m_last_used_of_type[m_last_used_type];
    }
    else
    {
        // Return the item in inv, since it will have an accurate count.
        if (item_out)
            *item_out = &you.inv[slot];
    }

    if (slot_out)
        *slot_out = slot;
}

// Return inv slot of item that should be fired by default.
// This differs from get_desired_item; that method can return
// an item that is not in inventory, while this one cannot.
// If no item can be found, return the reason why.
int player_quiver::get_fire_item(string* no_item_reason) const
{
    // Felids have no use for the quiver.
    if (you.species == SP_FELID)
    {
        if (no_item_reason != nullptr)
            *no_item_reason = "You can't grasp things well enough to throw them.";
        return -1;
    }
    int slot;
    const item_def* desired_item;

    get_desired_item(&desired_item, &slot);

    // If not in inv, try the head of the fire order.
    if (slot == -1)
    {
        vector<int> order;
        _get_fire_order(order, false, you.weapon(), false);
        if (!order.empty())
            slot = order[0];
    }

    // If we can't find anything, tell caller why.
    if (slot == -1)
    {
        vector<int> full_fire_order;
        _get_fire_order(full_fire_order, true, you.weapon(), false);
        if (no_item_reason == nullptr)
        {
            // nothing
        }
        else if (full_fire_order.empty())
            *no_item_reason = "No suitable missiles.";
        else
        {
            const int skipped_item = full_fire_order[0];
            if (skipped_item < Options.fire_items_start)
            {
                *no_item_reason = make_stringf(
                    "Nothing suitable (fire_items_start = '%c').",
                    index_to_letter(Options.fire_items_start));
            }
            else
            {
                *no_item_reason = make_stringf(
                    "Nothing suitable (ignored '=f'-inscribed item on '%c').",
                    index_to_letter(skipped_item));
            }
        }
    }

    return slot;
}

void player_quiver::set_quiver(const item_def &item, ammo_t ammo_type)
{
    m_last_used_of_type[ammo_type] = item;
    m_last_used_of_type[ammo_type].quantity = 1;
    m_last_used_type  = ammo_type;
    you.redraw_quiver = true;
}

void player_quiver::empty_quiver(ammo_t ammo_type)
{
    m_last_used_of_type[ammo_type] = item_def();
    m_last_used_of_type[ammo_type].quantity = 0;
    m_last_used_type  = ammo_type;
    you.redraw_quiver = true;
}

void quiver_item(int slot)
{
    const item_def item = you.inv[slot];
    ASSERT(item.defined());

    ammo_t t = AMMO_THROW;
    const item_def *weapon = you.weapon();
    if (weapon && item.launched_by(*weapon))
        t = _get_weapon_ammo_type(weapon);

    you.m_quiver.set_quiver(you.inv[slot], t);
    mprf("Quivering %s for %s.", you.inv[slot].name(DESC_INVENTORY).c_str(),
         t == AMMO_THROW    ? "throwing" :
         t == AMMO_BLOWGUN  ? "blowguns" :
         t == AMMO_SLING    ? "slings" :
         t == AMMO_BOW      ? "bows" :
                              "crossbows");
}

void choose_item_for_quiver()
{
    if (you.species == SP_FELID)
    {
        mpr("You can't grasp things well enough to throw them.");
        return;
    }

    int slot = prompt_invent_item("Quiver which item? (- for none, * to show all)",
                                  MT_INVLIST, OSEL_THROWABLE, OPER_QUIVER,
                                  INVPROMPT_HIDE_KNOWN, '-');

    if (prompt_failed(slot))
        return;

    if (slot == PROMPT_GOT_SPECIAL)  // '-' or empty quiver
    {
        ammo_t t = _get_weapon_ammo_type(you.weapon());
        you.m_quiver.empty_quiver(t);

        mprf("Reset %s quiver to default.",
             t == AMMO_THROW    ? "throwing" :
             t == AMMO_BLOWGUN  ? "blowgun" :
             t == AMMO_SLING    ? "sling" :
             t == AMMO_BOW      ? "bow" :
                                  "crossbow");
        return;
    }
    else
    {
        for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
        {
            if (you.equip[i] == slot)
            {
                mpr("You can't quiver worn items.");
                return;
            }
        }
    }
    quiver_item(slot);
}

// Notification that item was fired with 'f'.
void player_quiver::on_item_fired(const item_def& item, bool explicitly_chosen)
{
    if (!explicitly_chosen)
    {
        // If the item was not actively chosen, i.e. just automatically
        // passed into the quiver, don't change any of the quiver settings.
        you.redraw_quiver = true;
        return;
    }
    // If item matches the launcher, put it in that launcher's last-used item.
    // Otherwise, it goes into last hand-thrown item.

    const item_def *weapon = you.weapon();

    if (weapon && item.launched_by(*weapon))
    {
        const ammo_t t = _get_weapon_ammo_type(weapon);
        m_last_used_of_type[t] = item;
        m_last_used_of_type[t].quantity = 1;    // 0 makes it invalid :(
        m_last_used_type = t;
    }
    else
    {
        const launch_retval projected = is_launched(&you, you.weapon(),
                                                    item);

        // Don't do anything if this item is not really fit for throwing.
        if (projected == LRET_FUMBLED)
            return;

#ifdef DEBUG_QUIVER
        mprf(MSGCH_DIAGNOSTICS, "item %s is for throwing",
             item.name(DESC_PLAIN).c_str());
#endif
        m_last_used_of_type[AMMO_THROW] = item;
        m_last_used_of_type[AMMO_THROW].quantity = 1;
        m_last_used_type = AMMO_THROW;
    }

    you.redraw_quiver = true;
}

// Notification that item was fired with 'f' 'i'
void player_quiver::on_item_fired_fi(const item_def& item)
{
    // Currently no difference.
    on_item_fired(item);
}

// Called when the player might have switched weapons, or might have
// picked up something interesting.
void player_quiver::on_weapon_changed()
{
    // Only switch m_last_used_type if weapon really changed
    const item_def* weapon = you.weapon();
    if (weapon == nullptr)
    {
        if (m_last_weapon.base_type != OBJ_UNASSIGNED)
        {
            m_last_weapon.base_type = OBJ_UNASSIGNED;
            m_last_used_type = AMMO_THROW;
        }
    }
    else
    {
        if (!_items_similar(*weapon, m_last_weapon))
        {
            // Weapon type changed.
            m_last_weapon = *weapon;
            m_last_used_type = _get_weapon_ammo_type(weapon);
        }
    }

    _maybe_fill_empty_slot();
}

void player_quiver::on_inv_quantity_changed(int slot, int amt)
{
    if (m_last_used_of_type[m_last_used_type].base_type == OBJ_UNASSIGNED)
    {
        // Empty quiver. Maybe we can fill it now?
        _maybe_fill_empty_slot();
        you.redraw_quiver = true;
    }
    else
    {
        // We might need to update the quiver...
        int qv_slot = get_fire_item();
        if (qv_slot == slot)
            you.redraw_quiver = true;
    }
}

// If current quiver slot is empty, fill it with something useful.
void player_quiver::_maybe_fill_empty_slot()
{
    // Felids have no use for the quiver.
    if (you.species == SP_FELID)
        return;

    const item_def* weapon = you.weapon();
    const ammo_t slot = _get_weapon_ammo_type(weapon);

#ifdef DEBUG_QUIVER
    mprf(MSGCH_DIAGNOSTICS, "last quiver item: %s; link %d, wpn: %d",
         m_last_used_of_type[slot].name(DESC_PLAIN).c_str(),
         m_last_used_of_type[slot].link, you.equip[EQ_WEAPON]);
#endif

    bool unquiver_weapon = false;
    if (m_last_used_of_type[slot].defined())
    {
        // If we're wielding an item previously quivered, the quiver may need
        // to be cleared. Else, any already quivered item is valid and we
        // don't need to do anything else.
        if (m_last_used_of_type[slot].link == you.equip[EQ_WEAPON]
            && you.equip[EQ_WEAPON] != -1)
        {
            unquiver_weapon = true;
        }
        else
            return;
    }

#ifdef DEBUG_QUIVER
    mprf(MSGCH_DIAGNOSTICS, "Recalculating fire order...");
#endif

    const launch_retval desired_ret =
         (weapon && is_range_weapon(*weapon)) ? LRET_LAUNCHED : LRET_THROWN;

    vector<int> order;
    _get_fire_order(order, false, weapon, false);

    if (unquiver_weapon && order.empty())
    {
        // Setting the quantity to zero will force the quiver to be empty,
        // should nothing else be found. We also set the base type to
        // OBJ_UNASSIGNED so this is not an invalid object with a real type,
        // as that would trigger an assertion on saving.
        m_last_used_of_type[slot].base_type = OBJ_UNASSIGNED;
        m_last_used_of_type[slot].quantity = 0;
    }
    else
    {
        for (int ord : order)
        {
            if (is_launched(&you, weapon, you.inv[ord]) == desired_ret)
            {
                m_last_used_of_type[slot] = you.inv[ord];
                m_last_used_of_type[slot].quantity = 1;
                break;
            }
        }
    }
}

void player_quiver::get_fire_order(vector<int>& v, bool manual) const
{
    _get_fire_order(v, false, you.weapon(), manual);
}

// Get a sorted list of items to show in the fire interface.
//
// If ignore_inscription_etc, ignore =f and Options.fire_items_start.
// This is used for generating informational error messages, when the
// fire order is empty.
//
// launcher determines what items match the 'launcher' fire_order type.
void player_quiver::_get_fire_order(vector<int>& order,
                                     bool ignore_inscription_etc,
                                     const item_def* launcher,
                                     bool manual) const
{
    const int inv_start = (ignore_inscription_etc ? 0
                                                  : Options.fire_items_start);

    // If in a net, cannot throw anything, and can only launch from blowgun.
    if (you.attribute[ATTR_HELD])
    {
        if (launcher && launcher->sub_type == WPN_BLOWGUN)
        {
            for (int i_inv = inv_start; i_inv < ENDOFPACK; i_inv++)
                if (you.inv[i_inv].defined()
                    && you.inv[i_inv].launched_by(*launcher))
                {
                    order.push_back(i_inv);
                }
        }
        return;
    }

    for (int i_inv = inv_start; i_inv < ENDOFPACK; i_inv++)
    {
        const item_def& item = you.inv[i_inv];
        if (!item.defined())
            continue;

        // Don't do anything if this item is not really fit for throwing.
        if (is_launched(&you, you.weapon(), item) == LRET_FUMBLED)
            continue;

        // =f prevents item from being in fire order.
        if (!ignore_inscription_etc
            && strstr(item.inscription.c_str(), manual ? "=F" : "=f"))
        {
            continue;
        }

        for (unsigned int i_flags = 0; i_flags < Options.fire_order.size();
             i_flags++)
        {
            if (_item_matches(item, (fire_type) Options.fire_order[i_flags],
                              launcher, manual))
            {
                order.push_back((i_flags<<16) | (i_inv & 0xffff));
                break;
            }
        }
    }

    sort(order.begin(), order.end());

    for (unsigned int i = 0; i < order.size(); i++)
        order[i] &= 0xffff;
}

// ----------------------------------------------------------------------
// Save/load
// ----------------------------------------------------------------------

static const short QUIVER_COOKIE = short(0xb015);
void player_quiver::save(writer& outf) const
{
    marshallShort(outf, QUIVER_COOKIE);

    marshallItem(outf, m_last_weapon);
    marshallInt(outf, m_last_used_type);
    marshallInt(outf, ARRAYSZ(m_last_used_of_type));

    for (unsigned int i = 0; i < ARRAYSZ(m_last_used_of_type); i++)
        marshallItem(outf, m_last_used_of_type[i]);
}

void player_quiver::load(reader& inf)
{
    const short cooky = unmarshallShort(inf);
    ASSERT(cooky == QUIVER_COOKIE); (void)cooky;

    unmarshallItem(inf, m_last_weapon);
    m_last_used_type = (ammo_t)unmarshallInt(inf);
    ASSERT_RANGE(m_last_used_type, AMMO_THROW, NUM_AMMO);

    const unsigned int count = unmarshallInt(inf);
    ASSERT(count <= ARRAYSZ(m_last_used_of_type));

    for (unsigned int i = 0; i < count; i++)
        unmarshallItem(inf, m_last_used_of_type[i]);
}

// ----------------------------------------------------------------------
// Identify helper
// ----------------------------------------------------------------------

preserve_quiver_slots::preserve_quiver_slots()
{
    COMPILE_CHECK(ARRAYSZ(m_last_used_of_type) ==
                  ARRAYSZ(you.m_quiver.m_last_used_of_type));

    for (unsigned int i = 0; i < ARRAYSZ(m_last_used_of_type); i++)
    {
        m_last_used_of_type[i] =
            _get_pack_slot(you.m_quiver.m_last_used_of_type[i]);
    }
}

preserve_quiver_slots::~preserve_quiver_slots()
{
    for (unsigned int i = 0; i < ARRAYSZ(m_last_used_of_type); i++)
    {
        const int slot = m_last_used_of_type[i];
        if (slot != -1)
        {
            you.m_quiver.m_last_used_of_type[i] = you.inv[slot];
            you.m_quiver.m_last_used_of_type[i].quantity = 1;
        }
    }
    you.redraw_quiver = true;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

// Helper for _get_fire_order.
// Types may actually contain more than one fire_type.
static bool _item_matches(const item_def &item, fire_type types,
                          const item_def* launcher, bool manual)
{
    ASSERT(item.defined());

    if (types & FIRE_INSCRIBED)
        if (item.inscription.find(manual ? "+F" : "+f", 0) != string::npos)
            return true;

    if (item.base_type != OBJ_MISSILES)
        return false;

    if ((types & FIRE_STONE) && item.sub_type == MI_STONE)
        return true;
    if ((types & FIRE_JAVELIN) && item.sub_type == MI_JAVELIN)
        return true;
    if ((types & FIRE_ROCK) && item.sub_type == MI_LARGE_ROCK)
        return true;
    if ((types & FIRE_NET) && item.sub_type == MI_THROWING_NET)
        return true;
    if ((types & FIRE_TOMAHAWK) && item.sub_type == MI_TOMAHAWK)
        return true;

    if (types & FIRE_LAUNCHER)
    {
        if (launcher && item.launched_by(*launcher))
            return true;
    }

    return false;
}

// Returns inv slot that contains an item that looks like item,
// or -1 if not in inv.
static int _get_pack_slot(const item_def& item)
{
    if (!item.defined())
        return -1;

    if (in_inventory(item) && _items_similar(item, you.inv[item.link], false))
        return item.link;

    // First try to find the exact same item.
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def &inv_item = you.inv[i];
        if (inv_item.quantity && _items_similar(item, inv_item, false))
            return i;
    }

    // If that fails, try to find an item sufficiently similar.
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def &inv_item = you.inv[i];
        if (inv_item.quantity && _items_similar(item, inv_item, true))
        {
            // =f prevents item from being in fire order.
            if (strstr(inv_item.inscription.c_str(), "=f"))
                return -1;

            return i;
        }
    }

    return -1;
}

// Returns the type of ammo used by the player's equipped weapon,
// or AMMO_THROW if it's not a launcher.
static ammo_t _get_weapon_ammo_type(const item_def* weapon)
{
    if (weapon == nullptr)
        return AMMO_THROW;
    if (weapon->base_type != OBJ_WEAPONS)
        return AMMO_THROW;

    switch (weapon->sub_type)
    {
        case WPN_BLOWGUN:
            return AMMO_BLOWGUN;
        case WPN_HUNTING_SLING:
        case WPN_FUSTIBALUS:
            return AMMO_SLING;
        case WPN_SHORTBOW:
        case WPN_LONGBOW:
            return AMMO_BOW;
        case WPN_HAND_CROSSBOW:
        case WPN_ARBALEST:
        case WPN_TRIPLE_CROSSBOW:
            return AMMO_CROSSBOW;
        default:
            return AMMO_THROW;
    }
}

static bool _items_similar(const item_def& a, const item_def& b, bool force)
{
    return items_similar(a, b) && (force || a.slot == b.slot);
}
