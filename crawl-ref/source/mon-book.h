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

struct mon_spellbook
{
    mon_spellbook_type type;
    spell_type spells[NUM_MONSTER_SPELL_SLOTS];
};

extern const mon_spellbook mspell_list[];

typedef vector<vector<spell_type> > unique_books;

vector<mon_spellbook_type> get_spellbooks(const monster_info &mon);
unique_books get_unique_spells(const monster_info &mon);
#endif
