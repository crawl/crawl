/**
 * @file
 * @brief Functions for generating random spellbooks.
**/

#pragma once

#include <functional>

#include "spl-util.h"

typedef function<bool(spschool discipline_1,
                      spschool discipline_2,
                      int agent,
                      const vector<spell_type> &prev,
                      spell_type spell)> themed_spell_filter;

/// How many spells should be in a random theme book?
int theme_book_size();

spschool random_book_theme();
spschool matching_book_theme(const vector<spell_type> &forced_spells);
function<spschool()> forced_book_theme(spschool theme);

bool basic_themed_spell_filter(spschool discipline_1,
                               spschool discipline_2,
                               int agent,
                               const vector<spell_type> &prev,
                               spell_type spell);
themed_spell_filter capped_spell_filter(int max_levels,
                                        themed_spell_filter subfilter
                                            = basic_themed_spell_filter);
themed_spell_filter forced_spell_filter(const vector<spell_type> &forced_spells,
                                        themed_spell_filter subfilter
                                            = basic_themed_spell_filter);

void theme_book_spells(spschool discipline_1,
                       spschool discipline_2,
                       themed_spell_filter filter,
                       int agent,
                       int num_spells,
                       vector<spell_type> &spells);

void build_themed_book(item_def &book,
                       themed_spell_filter filter = basic_themed_spell_filter,
                       function<spschool()> get_discipline
                            = random_book_theme,
                       int num_spells = -1,
                       string owner = "", string subject = "");

void fixup_randbook_disciplines(spschool &discipline_1,
                                spschool &discipline_2,
                                const vector<spell_type> &spells);
void init_book_theme_randart(item_def &book, vector<spell_type> spells);
void name_book_theme_randart(item_def &book, spschool discipline_1,
                             spschool discipline_2,
                             string owner = "", string subject = "");

bool make_book_level_randart(item_def &book, int level = -1);
void make_book_roxanne_special(item_def *book);
void make_book_kiku_gift(item_def &book, bool first);
void acquire_themed_randbook(item_def &book, int agent);

/* Public for testing purposes only: do not use elsewhere */
void _set_book_spell_list(item_def &book, vector<spell_type> spells);
