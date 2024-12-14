/**
 * @file  regex-rules.h
 * @brief Functions for applying regular expression rules
 **/

#pragma once

#include <string>
using std::string;

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
