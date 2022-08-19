#pragma once

#include "item-prop-enum.h"
#include "spl-cast.h"

#if TAG_MAJOR_VERSION == 34
#define ORIGINAL_BRAND_KEY "orig brand"
#endif

class dist;

spret cast_confusing_touch(int power, bool fail);
