/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#include "AppHdr.h"

#include "colour.h"
#include "game-options.h"
#include "stringutil.h"

void BoolGameOption::reset() const
{
    value = default_value;
}

string BoolGameOption::loadFromString(string field) const
{
    value = read_bool(field, default_value);
    return "";
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


void ColourGameOption::reset() const
{
    value = default_value;
}

string ColourGameOption::loadFromString(string field) const
{
    const int col = str_to_colour(field, -1, true, elemental);
    if (col == -1)
    {
        return make_stringf("Bad %s -- %s\n",
                            names.begin()->c_str(), field.c_str());
    }

    value = col;
    return "";
}
