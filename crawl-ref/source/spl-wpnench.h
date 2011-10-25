#ifndef SPL_WPNENCH_H
#define SPL_WPNENCH_H

#include "itemprop-enum.h"
#include "spl-cast.h"

class dist;

spret_type brand_weapon(brand_type which_brand, int power, bool fail = false);
spret_type cast_confusing_touch(int power, bool fail);
spret_type cast_sure_blade(int power, bool fail = false);

#endif
