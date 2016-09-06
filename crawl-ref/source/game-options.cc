/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#include "AppHdr.h"

#include "game-options.h"

void BoolGameOption::reset() const
{
    value = default_value;
}


void BoolGameOption::loadFromString(string string) const
{
    value = read_bool(string, default_value);
}

bool read_bool(const string &field, bool def_value)
{
    bool ret = def_value;

    if (field == "true" || field == "1" || field == "yes")
        ret = true;

    if (field == "false" || field == "0" || field == "no")
        ret = false;

    return ret;
}
