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
    const char * plus;
    int charge_xp_debt;
    int max_charges;
    recharge_messages recharge_msg;
};

static const unordered_map<misc_item_type, evoker_data, std::hash<int>> xp_evoker_data = {
    { MISC_PHIAL_OF_FLOODS, { "phial_debt", "phial_plus", 10, 1,
        { "You hear a faint sloshing from %s as it returns to readiness.",
          "Water glimmers in %s, now refilled and ready to use.", },
    }},
    { MISC_HORN_OF_GERYON, { "horn_debt", "horn_plus", 10, 1 } },
    { MISC_LIGHTNING_ROD,  { "rod_debt", "rod_plus", 3, LIGHTNING_MAX_CHARGE } },
    { MISC_TIN_OF_TREMORSTONES, { "tin_debt", "tin_plus", 10, 2 } },
    { MISC_PHANTOM_MIRROR, { "mirror_debt", "mirror_plus", 10, 1 } },
    { MISC_BOX_OF_BEASTS, { "box_debt", "box_plus", 10, 1 } },
    { MISC_SACK_OF_SPIDERS, { "sack_debt", "sack_plus", 10, 1,
        { "You hear chittering from %s. It's ready.",
          "%s twitches, refilled and ready to use.", },
    }},
    { MISC_CONDENSER_VANE, { "condenser_debt", "condenser_plus", 10, 1 } },
    { MISC_GRAVITAMBOURINE, { "tambourine_debt", "tambourine_plus", 10, 2,
        { "%s jingles faintly as it regains its power.",
          "%s shakes itself petulantly as it silently regains its power." },
    }},
};
