#pragma once

// This list contains the possible values for Options.travel_open_doors.
// This used to be a boolean, so "true" and "false" are mapped to their
// equivalents.
// Where there is more than one name for a value, the one the game should give
// for it (in l-option.cc, or anywhere else) should come first.
enum class travel_open_doors_type
{
    avoid,
    approach,
    open,
    _false = approach,
    _true = open,
};
