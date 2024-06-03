/**
 * @file
 * @brief Functions used to print information about spells, spellbooks, etc.
 **/

#pragma once

#include <map>
#include <vector>

#include "enum.h"
#include "format.h"
#include "menu.h"
#include "mon-info.h"

using std::vector;

enum class spellbook_source
{
    monster_book,
    wand
};

/// What's in a given spellbook?
struct spellbook_contents
{
    /// A label for the book.
    string label;
    /// The spells contained in the book (or 'book').
    vector<spell_type> spells;
    /// Where do these spells come from: book, wand, etc.
    spellbook_source source;
};

typedef vector<spellbook_contents> spellset;

spellset item_spellset(const item_def &item);
spellset monster_spellset(const monster_info &mi);
vector<pair<spell_type,char>> map_chars_to_spells(const spellset &spells);
void describe_spellset(const spellset &spells,
                       const item_def* const source_item,
                       formatted_string &description,
                       const monster_info *mon_owner = nullptr);
void write_spellset(const spellset &spells,
                       const item_def* const source_item,
                       const monster_info *mon_owner = nullptr);
string describe_item_spells(const item_def &item);
string terse_spell_list(const item_def &item);
