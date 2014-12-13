/**
 * @file
 * @brief Spellbook/rod contents array and management functions
**/

#ifndef SPL_BOOK_H
#define SPL_BOOK_H

#define RANDBOOK_SIZE 8
#include "itemprop-enum.h"
#include "spl-util.h" // spschool_flag_type

class formatted_string;

int  book_rarity(book_type which_book);
int  spell_rarity(spell_type which_spell);
bool is_rare_book(book_type type);
void init_spell_rarities();
bool is_player_spell(spell_type which_spell);

void mark_had_book(const item_def &book);
void mark_had_book(book_type booktype);
void inscribe_book_highlevel(item_def &book);

bool maybe_id_book(item_def &book, bool silent = false);
void read_book(item_def &item);

bool player_can_memorise(const item_def &book);
bool can_learn_spell(bool silent = false);
bool learn_spell();
bool learn_spell(spell_type spell);
bool forget_spell_from_book(spell_type spell, const item_def* book);

string desc_cannot_memorise_reason(spell_type spell);
bool player_can_memorise_from_spellbook(const item_def &book);

spell_type spell_in_rod(rod_type rod);
vector<spell_type> spellbook_template(book_type book);
vector<spell_type> spells_in_book(const item_def &book);

bool you_can_memorise(spell_type spell) PURE;
bool has_spells_to_memorise(bool silent = true,
                            spell_type current_spell = SPELL_NO_SPELL);
vector<spell_type> get_mem_spell_list(vector<int> &books);

bool make_book_level_randart(item_def &book, int level = -1,
                             string owner = "");
bool make_book_theme_randart(item_def &book,
                             spschool_flag_type disc1 = SPTYP_NONE,
                             spschool_flag_type disc2 = SPTYP_NONE,
                             int num_spells = -1, int max_levels = -1,
                             spell_type incl_spell = SPELL_NO_SPELL,
                             string owner = "", string title = "",
                             bool exact_level = false);
bool make_book_theme_randart(item_def &book,
                             vector<spell_type> incl_spells,
                             spschool_flag_type disc1 = SPTYP_NONE,
                             spschool_flag_type disc2 = SPTYP_NONE,
                             int num_spells = -1, int max_levels = -1,
                             string owner = "", string title = "",
                             bool exact_level = false);
void make_book_Roxanne_special(item_def *book);
void make_book_Kiku_gift(item_def &book, bool first);

bool book_has_title(const item_def &book);

void destroy_spellbook(const item_def &book);
#endif
