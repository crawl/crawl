#include "AppHdr.h"

#include "player-equip.h"

#include "delay.h"
#include "item_use.h"
#include "itemprop.h"
#include "it_use2.h"
#include "player.h"

static void _equip_effect(equipment_type slot, int item_slot, bool unmeld, bool msg);
static void _unequip_effect(equipment_type slot, int item_slot, bool msg);

// Fill an empty equipment slot.
void equip_item(equipment_type slot, int item_slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(you.equip[slot] == -1);
    ASSERT(!you.melded[slot]);

    you.equip[slot] = item_slot;

    _equip_effect(slot, item_slot, false, msg);
}

// Clear an equipment slot (possibly melded).
bool unequip_item(equipment_type slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    const int item_slot = you.equip[slot];
    if (item_slot == -1)
        return (false);
    else
    {
        you.equip[slot] = -1;
        if (!you.melded[slot])
            _unequip_effect(slot, item_slot, msg);
        else
            you.melded[slot] = false;
        return (true);
    }
}

// Meld a slot (if equipped).
bool meld_slot(equipment_type slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && !you.melded[slot])
    {
        you.melded[slot] = true;
        _unequip_effect(slot, you.equip[slot], msg);
        return (true);
    }
    return (false);
}

bool unmeld_slot(equipment_type slot, bool msg)
{
    ASSERT(slot > EQ_NONE && slot < NUM_EQUIP);
    ASSERT(!you.melded[slot] || you.equip[slot] != -1);

    if (you.equip[slot] != -1 && you.melded[slot])
    {
        you.melded[slot] = false;
        _equip_effect(slot, you.equip[slot], true, msg);
        return (true);
    }
    return (false);
}

static void _equip_effect(equipment_type slot, int item_slot, bool unmeld, bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    ASSERT(slot == eq
           || eq == EQ_RINGS
              && (slot == EQ_LEFT_RING || slot == EQ_RIGHT_RING));

    if (slot == EQ_WEAPON)
        equip_weapon_effect(item, msg);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        equip_armour_effect(item, unmeld);
    else if (slot >= EQ_LEFT_RING && slot <= EQ_AMULET)
        equip_jewellery_effect(item);
}

static void _unequip_effect(equipment_type slot, int item_slot, bool msg)
{
    item_def& item = you.inv[item_slot];
    equipment_type eq = get_item_slot(item);

    if (slot == EQ_WEAPON && eq != EQ_WEAPON)
        return;

    ASSERT(slot == eq
           || eq == EQ_RINGS
              && (slot == EQ_LEFT_RING || slot == EQ_RIGHT_RING));

    if (slot == EQ_WEAPON)
        unequip_weapon_effect(item, msg);
    else if (slot >= EQ_CLOAK && slot <= EQ_BODY_ARMOUR)
        unequip_armour_effect(item);
    else if (slot >= EQ_LEFT_RING && slot <= EQ_AMULET)
        unequip_jewellery_effect(item, msg);
}
