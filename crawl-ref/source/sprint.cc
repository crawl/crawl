#include "AppHdr.h"

#include "sprint.h"

#include "externs.h"
#include "maps.h"
#include "mon-util.h"
#include "monster.h"
#include "mpr.h"
#include "player.h"
#include "religion.h"
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
    if (you_worship(GOD_OKAWARU))
        return piety;
    return piety * 9;
}
