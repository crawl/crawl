/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#include "AppHdr.h"

#include "colour.h"
#include "game-options.h"
#include "stringutil.h"

static unsigned _curses_attribute(const string &field)
{
    if (field == "standout")               // probably reverses
        return CHATTR_STANDOUT;
    if (field == "bold")              // probably brightens fg
        return CHATTR_BOLD;
    if (field == "blink")             // probably brightens bg
        return CHATTR_BLINK;
    if (field == "underline")
        return CHATTR_UNDERLINE;
    if (field == "reverse")
        return CHATTR_REVERSE;
    if (field == "dim")
        return CHATTR_DIM;
    if (starts_with(field, "hi:")
        || starts_with(field, "hilite:")
        || starts_with(field, "highlight:"))
    {
        const int col = field.find(":");
        const int colour = str_to_colour(field.substr(col + 1));
        if (colour != -1)
            return CHATTR_HILITE | (colour << 8);

        Options.report_error("Bad highlight string -- %s\n",
                             field.c_str());
    }
    else if (field != "none")
        Options.report_error("Bad colour -- %s\n", field.c_str());
    return CHATTR_NORMAL;
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

void BoolGameOption::reset() const { value = default_value; }

string BoolGameOption::loadFromString(string field) const
{
    value = read_bool(field, default_value);
    return "";
}

void ColourGameOption::reset() const { value = default_value; }

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


void CursesGameOption::reset() const { value = default_value; }

string CursesGameOption::loadFromString(string field) const
{
    value = _curses_attribute(field);
    return "";
}
