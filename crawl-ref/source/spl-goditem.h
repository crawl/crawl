#ifndef SPL_GODITEM_H
#define SPL_GODITEM_H

#include "spl-cast.h"

spret_type cast_healing(int pow, int max_pow, bool divine_ability = false,
                        const coord_def& where = coord_def(0, 0),
                        bool not_self = false,
                        targ_mode_type mode = TARG_NUM_MODES);

void debuff_player();
void debuff_monster(monster* mons);

int detect_traps(int pow);
int detect_items(int pow);
int detect_creatures(int pow, bool telepathic = false);
bool remove_curse(bool alreadyknown = true, const string &pre_msg = "");
bool curse_item(bool armour, const string &pre_msg = "");

bool entomb(int pow);
bool cast_imprison(int pow, monster* mons, int source);

bool cast_smiting(int pow, monster* mons);

int is_pacifiable(const monster* mon);
#endif
