#include "AppHdr.h"

#include "externs.h"
#include "maps.h"
#include "mon-util.h"
#include "mpr.h"
#include "ng-setup.h"
#include "player.h"
#include "random.h"

void sprint_give_items()
{
    newgame_give_item(OBJ_POTIONS, POT_CURING);
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
