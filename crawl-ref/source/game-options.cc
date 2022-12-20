/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#include "AppHdr.h"

#include "game-options.h"
#include "lookup-help.h"
#include "options.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "prompt.h"
#include "tiles-build-specific.h"

// Return a list of the valid field values for _curses_attribute(), excluding
// hi: and hilite: values, which are all duplicates.
// This is called after initialisation to ensure that colour_to_str() works.
static map<unsigned, string>& _curses_attribute_map()
{
    static map<unsigned, string> list;
    if (list.empty())
    {
        // This is the same list as in _curses_attribute.
        list =
        {
            {CHATTR_NORMAL, "none"}, {CHATTR_STANDOUT, "standout"},
            {CHATTR_BOLD, "bold"}, {CHATTR_BLINK, "blink"},
            {CHATTR_UNDERLINE, "underline"}, {CHATTR_REVERSE, "reverse"},
            {CHATTR_DIM, "dim"}
        };
        for (colour_t i = COLOUR_UNDEF; i < NUM_TERM_COLOURS; ++i)
        {
            int idx = CHATTR_HILITE | i << 8;
            list[idx] = string("highlight:")+colour_to_str(i);
        }
    }
    return list;
}

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
        return MB_TRUE;

    if (field == "false" || field == "0" || field == "no")
        return MB_FALSE;

    return MB_MAYBE;
}

bool read_bool(const string &field, bool def_value)
{
    const maybe_bool result = read_maybe_bool(field);
    if (result != MB_MAYBE)
        return tobool(result, false);

    Options.report_error("Bad boolean: %s (should be true or false)", field.c_str());
    return def_value;
}

/// Ask the user to choose between a set of options.
///
/// @param[in] prompt Text to put above the options. Typically a question.
/// @param[in] options List of options (as strings) to choose between.
/// @param[in] def_value Value of default option (as string).
/// @returns The selected option if something was selected, or "" if the prompt
///          was cancelled.
static string _choose_one_from_list(const string prompt,
                                    const vector<string> options,
                                    const string def_value)
{
    // The caller should remove any user-provided formatting.
    Menu menu(MF_SINGLESELECT | MF_ARROWS_SELECT
              | MF_ALLOW_FORMATTING | MF_INIT_HOVER);

    menu.set_title(new MenuEntry(prompt, MEL_TITLE));

    for (unsigned i = 0, size = options.size(); i < size; ++i)
    {
        const char letter = index_to_letter(i % 52);
        MenuEntry* me = new MenuEntry(options[i], MEL_ITEM, 1, letter);
        menu.add_entry(me);
    }

    auto it = find(options.begin(), options.end(), def_value);
    ASSERT(options.end() != it);
    menu.set_hovered(distance(options.begin(), it));

    vector<MenuEntry*> sel = menu.show();
    if (sel.empty())
        return "";
    else
        return sel[0]->text;
}

/// Ask the user to choose between a set of options.
///
/// @param[in,out] caller  Option to edit. Reads the name and current value.
///                        Writes the new value.
/// @param[in]     choices Options to choose between.
/// @returns       True if something is chosen, false otherwise.
bool choose_option_from_UI(GameOption *caller, vector<string> choices)
{
    string prompt = string("Select a value for ")+caller->name()+":";
    string selected = _choose_one_from_list(prompt, choices, caller->str());
    if (!selected.empty())
        caller->loadFromString(selected, RCFILE_LINE_EQUALS);
    return !selected.empty();
}

/// Ask the user to edit a game option using a text box.
///
/// @param[in,out] caller Option to edit. Reads the name and current value.
///                       Writes the new value.
/// @returns       True if something is chosen, false otherwise.
bool load_string_from_UI(GameOption *caller)
{
    string prompt = string("Enter a value for ")+caller->name()+":";
    /// XXX - shouldn't use a fixed length string here.
    char select[1024] = "";
    string old = caller->str();
    while (1)
    {
        if (msgwin_get_line(prompt, select, sizeof(select), nullptr, old))
        {
            caller->loadFromString(caller->str(), RCFILE_LINE_EQUALS);
            return false;
        }
        string error = caller->loadFromString(select, RCFILE_LINE_EQUALS);
        if (error.empty())
            return true;
        show_type_response(error);
        old = select;
    }
}

string BoolGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    string error;
    const maybe_bool result = read_maybe_bool(field);
    if (result == MB_MAYBE)
    {
        return make_stringf("Bad %s value: %s (should be true or false)",
                            name().c_str(), field.c_str());
    }

    value = tobool(result, false);
    return GameOption::loadFromString(field, ltyp);
}

bool BoolGameOption::load_from_UI()
{
    vector<string> choices = {"false", "true"};
    return choose_option_from_UI(this, choices);
}

string ColourGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    const int col = str_to_colour(field, -1, true, elemental);
    if (col == -1)
        return make_stringf("Bad %s -- %s\n", name().c_str(), field.c_str());

    value = col;
    return GameOption::loadFromString(field, ltyp);
}

const string ColourGameOption::str() const
{
    ASSERT(!elemental); // XXX - not handled, as no option uses it.
    return colour_to_str(value);
}

bool ColourGameOption::load_from_UI()
{
    vector<string> choices;
    for (colour_t i = COLOUR_UNDEF; i < NUM_TERM_COLOURS; ++i)
        choices.emplace_back(colour_to_str(i));
    return choose_option_from_UI(this, choices);
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

const string CursesGameOption::str() const
{
    const auto x = _curses_attribute_map().find(value);
    if (x == _curses_attribute_map().end())
        die("Invalid value for %s: %d", name().c_str(), value);
    return x->second;
}

bool CursesGameOption::load_from_UI()
{
    vector<string> choices;
    for (auto &x : _curses_attribute_map()) // Add the names in numerical order.
        choices.emplace_back(x.second);
    return choose_option_from_UI(this, choices);
}

#ifdef USE_TILE
TileColGameOption::TileColGameOption(VColour &val, vector<string> _names,
                                     string _default)
        : GameOption(_names), value(val),
          default_value(str_to_tile_colour(_default)) { }

string TileColGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    value = str_to_tile_colour(field);
    return GameOption::loadFromString(field, ltyp);
}

const string TileColGameOption::str() const
{
    return make_stringf("#%02x%02x%02x", value.r, value.g, value.b);
}
#endif

string IntGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    int val = default_value;
    if (!parse_int(field.c_str(), val))
        return make_stringf("Bad %s: \"%s\"", name().c_str(), field.c_str());
    if (val < min_value)
        return make_stringf("Bad %s: %d should be >= %d", name().c_str(), val, min_value);
    if (val > max_value)
        return make_stringf("Bad %s: %d should be <<= %d", name().c_str(), val, max_value);
    value = val;
    return GameOption::loadFromString(field, ltyp);
}

const string IntGameOption::str() const
{
    return to_string(value);
}

string StringGameOption::loadFromString(const string &field, rc_line_type ltyp)
{
    value = field;
    return GameOption::loadFromString(field, ltyp);
}

const string StringGameOption::str() const
{
    return value;
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

const string ColourThresholdOption::str() const
{
    auto f = [] (const colour_threshold &s)
    {
        return make_stringf("%d:%s", s.first, colour_to_str(s.second).c_str());
    };
    return comma_separated_fn(value.begin(), value.end(), f, ", ");
}
