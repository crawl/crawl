#include "AppHdr.h"

#include "externs.h"
#include "items.h"
#include "maps.h"
#include "mon-util.h"
#include "mpr.h"
#include "ng-setup.h"
#include "player.h"
#include "random.h"

void sprint_give_items()
{
    newgame_give_item(OBJ_POTIONS, POT_HEALING);
    newgame_give_item(OBJ_POTIONS, POT_HEAL_WOUNDS);
    newgame_give_item(OBJ_POTIONS, POT_SPEED);
    newgame_give_item(OBJ_POTIONS, POT_MAGIC, 2);
    newgame_give_item(OBJ_POTIONS, POT_BERSERK_RAGE);
    newgame_give_item(OBJ_SCROLLS, SCR_BLINKING);
}

int sprint_modify_exp(int exp)
{
    return exp * 9;
}

int sprint_modify_exp_inverse(int exp)
{
    return div_rand_round(exp, 9);
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
    return random2(20) > mons_class_hit_dice(type);
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
