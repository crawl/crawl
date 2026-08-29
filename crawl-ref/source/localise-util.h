/**
 * @file localise-util.h
 * @brief String localisation (translation) utility functions
 **/

#pragma once

#include <string>
#include <vector>

using std::string;
using std::vector;

string add_context(const string &context, const string &s);
string strip_context(const string &s);

vector<string> extract_params(const string& s);

// escape any characters that have a special meaning in regex
string escape_regex_specials(const string& s);

// length of string excluding any @foo@ parameters
size_t length_excl_params(const string &s);

// is this a string representation of an integer?
// (1 or more digits with an optional sign in front)
bool is_integer_string(const string &s);

// is this a string representation of a floating point number?
bool is_float_string(const string &s);

// Is string prefixed with a determiner (e.g. the, a, your, etc.)?
bool has_determiner(const string& s);

// Strip determiner from string
string strip_determiner(const string& s);

bool is_adverb(const string& s);
bool is_determiner(const string& s);

bool starts_with_uppercase(const string& s);

// make first letter lowercase if string starts with a detrminer
string maybe_lowercase_first(const string& s);

void separate_end_punctuation(const string& s, string& punct, string& rest);
string get_end_punctuation(const string& s);

// separate menu letter prefix (e.g. "a - ", "a) ")
void separate_menu_letter_prefix(const string& s, string& annotation, string& rest);

void separate_prefix_annotation(const string& s, string& annotation, string& rest);
void separate_postfix_annotation(const string& s, string& annotation, string& rest);

// separate string into 3 types of tokens
// - plain text
// - context specifiers (e.g. "{poss}")
// - parameters (e.g. "@the_monster@")
vector<string> tokenise_parameterised_string(const string& s);

// separate list into tokens. separators are included.
// Example: "a goblin, 2 orcs and a kobold"
//  -> ["a goblin", ", ", "2 orcs", " and ", "a kobold"]
vector<string> tokenise_comma_separated_list(const string& s);

// because POSIX regex can't do non-greedy matching
void fixup_greedy_matching(vector<string>& params, vector<string>& values);

// Apply a regex rule to a string.
//
// Rules can have two forms:
// 1. /<pattern>/<replacement>/ - replace all instances of <pattern> in the
//    string with <replacement>. You can use perl-style backreferences
//    ($1, $2, etc.) in the replacement string.
// 2. /<filter-pattern>/<pattern>/<replacement>/ - this is the same as #1,
//    except the rule is only applied to the first substring that matches
//    <filter-pattern>.
string apply_regex_rule(const string& rule, const string& s);

// Apply a set of regex rules to a string. The rules must be separated by a
// newline. They are applied cumulatively in order. See apply_regex_rule for an
// explanation of how individual rules work.
string apply_regex_rules(const string& rules_str, string s);
