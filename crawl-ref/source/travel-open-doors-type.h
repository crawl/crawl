#pragma once

// This list contains the possible values for Options.travel_open_doors.
// This used to be a boolean, so "true" and "false" are mapped to their
// equivalents.
// Where there is more than one name for a value, the one the game should give
// for it (in l-option.cc, or anywhere else) should come first.
// This uses a macro so that l-options.cc doesn't need a copy of the list.
#define TRAVEL_OPEN_DOORS_LIST \
    X(avoid, 1) \
    X(approach, 2) \
    X(open, 3) \
    X(false, _approach) \
    X(true, _open)

#define X(s, i) _ ## s = i,
enum class travel_open_doors_type
{
    TRAVEL_OPEN_DOORS_LIST
};
#undef X
