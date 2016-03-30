/**
 * @file
 * @brief Functions for generating random spellbooks.
**/

#ifndef RANDBOOK_H
#define RANDBOOK_H

#include "spl-util.h"

void init_book_theme_randart(item_def &book, vector<spell_type> spells);
void name_book_theme_randart(item_def &book, spschool_flag_type discipline_1,
                             spschool_flag_type discipline_2,
                             string owner = "", string subject = "");
bool make_book_level_randart(item_def &book, int level = -1);
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
void make_book_roxanne_special(item_def *book);
void make_book_kiku_gift(item_def &book, bool first);
void acquire_themed_randbook(item_def &book, int agent);

#endif
