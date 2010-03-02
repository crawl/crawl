#include "externs.h"
#include "items.h"
#include "player.h"

void sprint_give_items()
{
    item_def item;

    item.quantity = 1;
    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_HEALING;
    const int slot2 = find_free_slot(item);
    you.inv[slot2] = item;

    item.quantity = 1;
    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_HEAL_WOUNDS;
    const int slot3 = find_free_slot(item);
    you.inv[slot3] = item;

    item.quantity = 1;
    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_SPEED;
    const int slot4 = find_free_slot(item);
    you.inv[slot4] = item;

    item.quantity = 1;
    item.base_type = OBJ_SCROLLS;
    item.sub_type  = SCR_BLINKING;
    const int slot5 = find_free_slot(item);
    you.inv[slot5] = item;

    item.quantity = 2;
    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_MAGIC;
    const int slot6 = find_free_slot(item);
    you.inv[slot6] = item;

    item.quantity = 1;
    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BERSERK_RAGE;
    const int slot7 = find_free_slot(item);
    you.inv[slot7] = item;
}

int sprint_modify_exp(int exp)
{
    return exp * 9;
}

int sprint_modify_skills(int skill_gain)
{
    return skill_gain * 27;
}

int sprint_modify_piety(int piety)
{
    return piety * 9;
}
