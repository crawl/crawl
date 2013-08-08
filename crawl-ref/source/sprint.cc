#include "AppHdr.h"

#include "sprint.h"

#include "externs.h"
#include "monster.h"
#include "mpr.h"
#include "player.h"
#include "religion.h"
#include "random.h"

int sprint_modify_exp(int exp)
{
    return div_rand_round(exp * you.exp_mul, you.exp_div);
}

int sprint_modify_exp_inverse(int exp)
{
    return div_rand_round(exp * you.exp_div, you.exp_mul);
}

int sprint_modify_piety(int piety)
{
    if (you_worship(GOD_OKAWARU))
        return piety;
    return div_rand_round(piety * you.piety_mul, you.piety_div);
}
