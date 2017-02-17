/**
 * @file
 * @brief Functions and data structures dealing with the syntax,
 *        morphology, and orthography of the English language.
**/

#ifndef ENGLISH_H
#define ENGLISH_H

#include <string>

#include "enum.h"
#include "gender-type.h"
#include "pronoun-type.h"

extern const char * const standard_plural_qualifiers[];

bool is_vowel(const char32_t chr);

string pluralise(const string &name,
                 const char * const stock_plural_quals[]
                     = standard_plural_qualifiers,
                 const char * const no_of[] = nullptr);
string pluralise_monster(const string &name);
string apostrophise(const string &name);
string conjugate_verb(const string &verb, bool plural);
const char *decline_pronoun(gender_type gender, pronoun_type variant);

string number_in_words(unsigned number);

string article_a(const string &name, bool lowercase = true);

// Applies a description type to a name, but does not pluralise! You
// must pluralise the name if needed. The quantity is used to prefix the
// name with a quantity if appropriate.
string apply_description(description_level_type desc, const string &name,
                         int quantity = 1, bool num_in_words = false);

string thing_do_grammar(description_level_type dtype, bool add_stop,
                        bool force_article, string desc);

string get_desc_quantity(const int quant, const int total,
                         string whose = "your");
#endif
