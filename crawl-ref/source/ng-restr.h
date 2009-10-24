#ifndef NG_RESTR_H
#define NG_RESTR_H

#include "newgame.h"

enum char_choice_restriction
{
    CC_BANNED = 0,
    CC_RESTRICTED,
    CC_UNRESTRICTED
};

char_choice_restriction class_allowed(species_type speci,
                                      job_type char_class);
bool is_good_combination(species_type spc, job_type cls,
                         bool good = false);
char_choice_restriction book_restriction(startup_book_type booktype,
                                         const newgame_def &ng);
char_choice_restriction weapon_restriction(weapon_type wpn,
                                           const newgame_def &ng);
char_choice_restriction religion_restriction(god_type god,
                                             const newgame_def &ng);

#endif

