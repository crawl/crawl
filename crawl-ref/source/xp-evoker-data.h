#pragma once

#include <unordered_map>
#include <string>

#include "item-prop-enum.h"

// Used in spl-damage.cc for lightning rod damage calculations
const int LIGHTNING_CHARGE_MULT = 100;
const int LIGHTNING_MAX_CHARGE = 4;

struct recharge_messages
{
    string noisy;
    string silent;
};

struct evoker_data
{
    const char * key;
    int charge_xp_debt;
    int max_charges;
    recharge_messages recharge_msg;
};

static const unordered_map<misc_item_type, evoker_data, std::hash<int>> xp_evoker_data = {
    { MISC_PHIAL_OF_FLOODS, { "phial_debt", 10, 1 ,
        { "You hear a faint sloshing from %s as it returns to readiness.",
          "Water glimmers in %s, now refilled and ready to use.", },
    }},
    { MISC_HORN_OF_GERYON, { "horn_debt", 10, 1 } },
    { MISC_LIGHTNING_ROD,  { "rod_debt", 3, LIGHTNING_MAX_CHARGE } },
    { MISC_TIN_OF_TREMORSTONES, { "tin_debt", 10, 2 } },
    { MISC_PHANTOM_MIRROR, { "mirror_debt", 10, 1 } },
    { MISC_BOX_OF_BEASTS, { "box_debt", 10, 1 } },
    { MISC_SACK_OF_SPIDERS, { "sack_debt", 10, 1,
        { "You hear chittering from %s. It's ready.",
          "%s twitches, refilled and ready to use.", },
    }},
    { MISC_CONDENSER_VANE, { "condenser_debt", 10, 1 } },
};
