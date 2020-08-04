#pragma once

#include <unordered_map>

#include "item-prop-enum.h"

// Used in spl-damage.cc for lightning rod damage calculations
const int LIGHTNING_CHARGE_MULT = 100;
const int LIGHTNING_MAX_CHARGE = 4;

struct evoker_data
{
    const char * key;
    int charge_xp_debt;
    int max_charges;
};

static const unordered_map<misc_item_type, evoker_data, std::hash<int>> xp_evoker_data = {
    { MISC_PHIAL_OF_FLOODS, { "phial_debt", 10, 1 } },
    { MISC_HORN_OF_GERYON, { "horn_debt", 10, 1 } },
    { MISC_LIGHTNING_ROD,  { "rod_debt", 3, LIGHTNING_MAX_CHARGE } },
    { MISC_TIN_OF_TREMORSTONES, { "tin_debt", 6, 3 } },
    { MISC_PHANTOM_MIRROR, { "mirror_debt", 10, 1 } },
    { MISC_BOX_OF_BEASTS, { "box_debt", 6, 3 } },
    { MISC_CONDENSER_VANE, { "condenser_debt", 3, 4 } },
};
