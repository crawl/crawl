/**
 * @file
 * @brief Monster spellbook functions, types, and globals.
**/

#ifndef MONBOOK_H
#define MONBOOK_H

#include <vector>
#include "defines.h"
#include "enum.h"
#include "mon-mst.h"

enum mon_spell_slot_flags
{
    MON_SPELL_NO_FLAGS  = 0,
    MON_SPELL_EMERGENCY = 1 << 0, // only use this spell slot in emergencies
    MON_SPELL_INNATE    = 1 << 1,
    MON_SPELL_WIZARD    = 1 << 2,
    MON_SPELL_PRIEST    = 1 << 3,
    MON_SPELL_BREATH    = 1 << 4, // sets a breath timer, requires it to be 0
    MON_SPELL_NO_SILENT = 1 << 5, // can't be used while silenced/mute/etc.
    MON_SPELL_INSTANT   = 1 << 6, // allows another action on the same turn
    MON_SPELL_NOISY     = 1 << 7, // makes noise despite being innate
};

struct mon_spellbook
{
    mon_spellbook_type type;
    mon_spell_slot spells[256]; // because it needs to be stored in a byte
};

#define END_OF_MONS_BOOK { SPELL_NO_SPELL, 0, MON_SPELL_NO_FLAGS }

typedef vector<vector<spell_type> > unique_books;

vector<mon_spellbook_type> get_spellbooks(const monster_info &mon);
unique_books get_unique_spells(const monster_info &mon,
                               mon_spell_slot_flags flags = MON_SPELL_NO_FLAGS);
#endif
