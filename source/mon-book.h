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
    MON_SPELL_NATURAL   = 1 << 1, // physiological, not really a spell
    MON_SPELL_MAGICAL   = 1 << 2, // a generic magical ability
    MON_SPELL_DEMONIC   = 1 << 3, // demonic
    MON_SPELL_WIZARD    = 1 << 4, // a real spell, affected by AM and silence
    MON_SPELL_PRIEST    = 1 << 5,

    MON_SPELL_FIRST_CATEGORY = MON_SPELL_NATURAL,
    MON_SPELL_LAST_CATEGORY  = MON_SPELL_PRIEST,

    MON_SPELL_TYPE_MASK = MON_SPELL_NATURAL | MON_SPELL_MAGICAL
                             | MON_SPELL_DEMONIC | MON_SPELL_WIZARD
                             | MON_SPELL_PRIEST,
    MON_SPELL_INNATE_MASK = MON_SPELL_NATURAL | MON_SPELL_MAGICAL
                             | MON_SPELL_DEMONIC,
    MON_SPELL_ANTIMAGIC_MASK = MON_SPELL_MAGICAL | MON_SPELL_DEMONIC
                             | MON_SPELL_WIZARD,

    MON_SPELL_BREATH    = 1 << 6, // sets a breath timer, requires it to be 0
    MON_SPELL_NO_SILENT = 1 << 7, // can't be used while silenced/mute/etc.

    MON_SPELL_SILENCE_MASK = MON_SPELL_WIZARD | MON_SPELL_PRIEST
                             | MON_SPELL_NO_SILENT,

    MON_SPELL_INSTANT   = 1 << 8, // allows another action on the same turn
    MON_SPELL_NOISY     = 1 << 9, // makes noise despite being innate

    MON_SPELL_LAST_EXPONENT = 9,
};

struct mon_spellbook
{
    mon_spellbook_type type;
    vector<mon_spell_slot> spells;
};

typedef vector<vector<spell_type> > unique_books;

vector<mon_spellbook_type> get_spellbooks(const monster_info &mon);
unique_books get_unique_spells(const monster_info &mon,
                               mon_spell_slot_flags flags = MON_SPELL_NO_FLAGS);
#endif
