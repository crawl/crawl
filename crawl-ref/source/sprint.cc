#include "AppHdr.h"

#include "sprint.h"

#include "religion.h"

int sprint_modify_exp(int exp)
{
    return exp * SPRINT_MULTIPLIER;
}

int sprint_modify_exp_inverse(int exp)
{
    return div_rand_round(exp, SPRINT_MULTIPLIER);
}

int sprint_modify_piety(int piety)
{
    if (you_worship(GOD_OKAWARU))
        return piety;
    return piety * SPRINT_MULTIPLIER;
}
