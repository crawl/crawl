/**
 * @file
 * @brief Monster spellbook functions, types, and globals.
**/

#pragma once

#include <vector>

#include "defines.h"
#include "enum.h"
#include "externs.h"
#include "mon-mst.h"

struct mon_spellbook
{
    mon_spellbook_type type;
    vector<mon_spell_slot> spells;
};

mon_spellbook_type get_spellbook(const monster_info &mon);
vector<mon_spell_slot> get_unique_spells(const monster_info &mon,
                               mon_spell_slot_flags flags = MON_SPELL_NO_FLAGS);
