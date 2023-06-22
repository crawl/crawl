/**
 * @file
 * @brief Functions and data structures dealing with the syntax,
 *        morphology, and orthography of the English language.
**/

#pragma once

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

// get singular from plural
string singularise(const string& plural);

string apostrophise(const string &name);
string conjugate_verb(const string &verb, bool plural);
const char *decline_pronoun(gender_type gender, pronoun_type variant);
string walk_verb_to_present(string verb);

string number_in_words(unsigned number);

string article_the(const string &name, bool lowercase = true);
string article_a(const string &name, bool lowercase = true);

// Applies a description type to a name, but does not pluralise! You
// must pluralise the name if needed. The quantity is used to prefix the
// name with a quantity if appropriate.
string apply_description(description_level_type desc, const string &name,
                         int quantity = 1);

string thing_do_grammar(description_level_type dtype, string desc,
                        bool ignore_case = false);

string get_desc_quantity(const int quant, const int total,
                         const string &whose = "your");
