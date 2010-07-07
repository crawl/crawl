#include "AppHdr.h"

#include "externs.h"
#include "items.h"
#include "maps.h"
#include "mon-util.h"
#include "mpr.h"
#include "player.h"
#include "random.h"

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

int sprint_modify_abyss_exit_chance(int exit_chance)
{
    return exit_chance / 3;
}

/* XXX: ugh, can't think of a better thing to do here */
bool sprint_veto_random_abyss_monster(monster_type type)
{
    return random2(20) > (int)get_monster_data(type)->hpdice[0];
}

mapref_vector get_sprint_maps()
{
    return find_maps_for_tag("sprint");
}

// We could save this in crawl_state instead.
// Or choose_game() could save *ng to crawl_state
// entirely, though that'd be redundant with
// you.your_name, you.species, crawl_state.type, ...

static std::string _sprint_map;

std::string get_sprint_map()
{
    return _sprint_map;
}

void set_sprint_map(const std::string& map)
{
    _sprint_map = map;
}
