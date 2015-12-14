#ifndef SPL_WPNENCH_H
#define SPL_WPNENCH_H

#include "itemprop-enum.h"
#include "spl-cast.h"

#define ORIGINAL_BRAND_KEY "orig brand"

class dist;

void end_weapon_brand(item_def &weapon, bool verbose = false);

spret_type brand_weapon(brand_type which_brand, int power, bool fail = false);
spret_type cast_confusing_touch(int power, bool fail);

#endif
