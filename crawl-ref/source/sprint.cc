#include "AppHdr.h"

#include "externs.h"
#include "maps.h"
#include "mon-util.h"
#include "mpr.h"
#include "ng-setup.h"
#include "player.h"
#include "random.h"

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
