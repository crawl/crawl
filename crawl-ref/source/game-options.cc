/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#include "AppHdr.h"

#include "game-options.h"
#include "options.h"
#include "misc.h"
#include "tiles-build-specific.h"

static unsigned _curses_attribute(const string &field, string &error)
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

        error = make_stringf("Bad highlight string -- %s",
                             field.c_str());
    }
    else if (field != "none")
        error = make_stringf("Bad colour -- %s", field.c_str());
    return CHATTR_NORMAL;
}

/**
 * Read a maybe bool field. Accepts anything for the third value.
 */
maybe_bool read_maybe_bool(const string &field)
{
    // TODO: check for "maybe" explicitly or something?
    if (field == "true" || field == "1" || field == "yes")
        return true;

    if (field == "false" || field == "0" || field == "no")
        return false;

    return maybe_bool::maybe;
}

bool read_bool(const string &field, bool def_value)
{
    const maybe_bool result = read_maybe_bool(field);
    if (result.is_bool())
        return bool(result);

    Options.report_error("Bad boolean: %s (should be true or false)", field.c_str());
    return def_value;
}

string GameOption::loadFromString(const std::string &, rc_line_type)
{
    loaded = true;
    if (on_set)
        on_set();
    return "";
}

string GameOption::loadFromParseState(const opt_parse_state &state)
{
    // if an override doesn't call loadFromString here, make sure to call
    // on_set() directly
    return loadFromString(
        case_sensitive ? state.raw_field : state.field,
        state.line_type);
}

string DisabledGameOption::loadFromParseState(const opt_parse_state &state)
{
    // use the parse state key: this lets this class group together
    // multiple unrelated options for convenience
    if (on_set)
        on_set();
    return make_stringf("Option '%s' is disabled in this build.",
            state.key.c_str());
}

string BoolGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    string error;
    const maybe_bool result = read_maybe_bool(field);
    if (!result.is_bool())
    {
        return make_stringf("Bad %s value: %s (should be true or false)",
                            name().c_str(), field.c_str());
    }

    value = bool(result);
    return GameOption::loadFromString(field, ltyp);
}

string ColourGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    const int col = str_to_colour(field, -1, true, elemental);
    if (col == -1)
        return make_stringf("Bad %s -- %s\n", name().c_str(), field.c_str());

    value = col;
    return GameOption::loadFromString(field, ltyp);
}

string CursesGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    string error;
    const unsigned result = _curses_attribute(field, error);
    if (!error.empty())
        return make_stringf("%s (for %s)", error.c_str(), name().c_str());

    value = result;
    return GameOption::loadFromString(field, ltyp);
}

#ifdef USE_TILE
TileColGameOption::TileColGameOption(VColour &val, std::set<std::string> _names,
                    string _default)
        : GameOption(_names), value(val),
          default_value(str_to_tile_colour(_default)) { }

string TileColGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    value = str_to_tile_colour(field);
    return GameOption::loadFromString(field, ltyp);
}
#endif

string IntGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    int val = default_value;
    if (!parse_int(field.c_str(), val))
        return make_stringf("Couldn't parse integer option %s: \"%s\"", name().c_str(), field.c_str());
    if (val < min_value)
        return make_stringf("Bad %s: %d should be >= %d", name().c_str(), val, min_value);
    if (val > max_value)
        return make_stringf("Bad %s: %d should be <= %d", name().c_str(), val, max_value);
    value = val;
    return GameOption::loadFromString(field, ltyp);
}

string FixedpGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    // is this still the best way to handle this with error checking in 2023??
    // strtod doesn't give good error checking, maybe std::stod?
    float tmp_float;
    if (!sscanf(field.c_str(), "%f", &tmp_float))
        return make_stringf("Couldn't parse decimal option %s: \"%s\"", name().c_str(), field.c_str());
    fixedp<> val = tmp_float; // implicit cast
    if (val < min_value)
        return make_stringf("Bad %s: %g should be >= %g", name().c_str(), (float) val, (float) min_value);
    if (val > max_value)
        return make_stringf("Bad %s: %g should be <= %g", name().c_str(), (float) val, (float) max_value);

    value = val;
    return GameOption::loadFromString(field, ltyp);
}

string StringGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    value = field;
    return GameOption::loadFromString(field, ltyp);
}

string ColourThresholdOption::loadFromString(const string &field,
                                             rc_line_type ltyp)
{
    string error;
    const colour_thresholds result = parse_colour_thresholds(field, &error);
    if (!error.empty())
        return error;

    switch (ltyp)
    {
        case RCFILE_LINE_EQUALS:
            value = result;
            break;
        case RCFILE_LINE_PLUS:
        case RCFILE_LINE_CARET:
            value.insert(value.end(), result.begin(), result.end());
            stable_sort(value.begin(), value.end(), ordering_function);
            break;
        case RCFILE_LINE_MINUS:
            for (pair<int, int> entry : result)
                remove_matching(value, entry);
            break;
        default:
            // XX should this really be a die?
            die("Unknown rc line type for %s: %d!", name().c_str(), ltyp);
    }
    return GameOption::loadFromString(field, ltyp);
}

colour_thresholds
    ColourThresholdOption::parse_colour_thresholds(const string &field,
                                                   string* error) const
{
    colour_thresholds result;
    for (string pair_str : split_string(",", field))
    {
        const vector<string> insplit = split_string(":", pair_str);

        if (insplit.size() != 2)
        {
            const string failure = make_stringf("Bad %s pair: '%s'",
                                                name().c_str(),
                                                pair_str.c_str());
            // XX should this really be a `die`? I *think* it can only be
            // triggered by someone setting a bad default in this file...
            if (!error)
                die("%s", failure.c_str());
            *error = failure;
            break;
        }

        const int threshold = atoi(insplit[0].c_str());

        const string colstr = insplit[1];
        const int scolour = str_to_colour(colstr, -1, true, false);
        if (scolour <= 0)
        {
            const string failure = make_stringf("Bad %s: '%s'",
                                                name().c_str(),
                                                colstr.c_str());
            // see note above
            if (!error)
                die("%s", failure.c_str());
            *error = failure;
            break;
        }

        result.push_back({threshold, scolour});
    }
    stable_sort(result.begin(), result.end(), ordering_function);
    return result;
}
