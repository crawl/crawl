/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#pragma once

#include <functional>
#include <string>
#include <set>
#include <vector>

#include "colour.h"
#include "stringutil.h"
#include "maybe-bool.h"

using std::vector;
struct game_options;

enum rc_line_type
{
    RCFILE_LINE_EQUALS, ///< foo = bar
    RCFILE_LINE_PLUS,   ///< foo += bar
    RCFILE_LINE_MINUS,  ///< foo -= bar
    RCFILE_LINE_CARET,  ///< foo ^= bar
    NUM_RCFILE_LINE_TYPES,
};

template<class A, class B> void merge_lists(A &dest, const B &src, bool prepend)
{
    dest.insert(prepend ? dest.begin() : dest.end(), src.begin(), src.end());
}

template <class L, class E>
L& remove_matching(L& lis, const E& entry)
{
    lis.erase(remove(lis.begin(), lis.end(), entry), lis.end());
    return lis;
}

class GameOption
{
public:
    GameOption(std::set<std::string> _names)
        : names(_names), loaded(false) { }
    virtual ~GameOption() {};

    // XX reset, set_from, and some other stuff could be templated for most
    // subclasses, but this is hard to reconcile with the polymorphism involved
    virtual void reset() { loaded = false; }
    virtual void set_from(const GameOption *other) = 0;
    virtual string loadFromString(const std::string &, rc_line_type)
    {
        loaded = true;
        return "";
    }


    const std::set<std::string> &getNames() const { return names; }
    const std::string name() const { return *names.begin(); }

    bool was_loaded() const { return loaded; }

protected:
    std::set<std::string> names;
    bool loaded; // tracks whether the option has changed via loadFromString.
                 // will miss whether it was changed directly in c++ code. (TODO)

    friend struct game_options;
};

class BoolGameOption : public GameOption
{
public:
    BoolGameOption(bool &val, std::set<std::string> _names,
                   bool _default)
        : GameOption(_names), value(val), default_value(_default) { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const BoolGameOption *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }

    string loadFromString(const std::string &field, rc_line_type) override;

private:
    bool &value;
    bool default_value;
};

class ColourGameOption : public GameOption
{
public:
    ColourGameOption(unsigned &val, std::set<std::string> _names,
                     unsigned _default, bool _elemental = false)
        : GameOption(_names), value(val), default_value(_default),
          elemental(_elemental) { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const ColourGameOption *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }

    string loadFromString(const std::string &field, rc_line_type) override;

private:
    unsigned &value;
    unsigned default_value;
    bool elemental;
};

class CursesGameOption : public GameOption
{
public:
    CursesGameOption(unsigned &val, std::set<std::string> _names,
                     unsigned _default)
        : GameOption(_names), value(val), default_value(_default) { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const CursesGameOption *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }


    string loadFromString(const std::string &field, rc_line_type) override;

private:
    unsigned &value;
    unsigned default_value;
};

class IntGameOption : public GameOption
{
public:
    IntGameOption(int &val, std::set<std::string> _names, int _default,
                  int min_val = INT_MIN, int max_val = INT_MAX)
        : GameOption(_names), value(val), default_value(_default),
          min_value(min_val), max_value(max_val) { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const IntGameOption *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }


    string loadFromString(const std::string &field, rc_line_type) override;

private:
    int &value;
    int default_value, min_value, max_value;
};

class StringGameOption : public GameOption
{
public:
    StringGameOption(string &val, std::set<std::string> _names,
                     string _default)
        : GameOption(_names), value(val), default_value(_default) { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const StringGameOption *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }

    string loadFromString(const std::string &field, rc_line_type) override;

private:
    string &value;
    string default_value;
};

#ifdef USE_TILE
class TileColGameOption : public GameOption
{
public:
    TileColGameOption(VColour &val, std::set<std::string> _names,
                      string _default);

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const TileColGameOption *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }

    string loadFromString(const std::string &field, rc_line_type) override;

private:
    VColour &value;
    VColour default_value;
};
#endif

typedef pair<int, int> colour_threshold;
typedef vector<colour_threshold> colour_thresholds;
typedef function<bool(const colour_threshold &l, const colour_threshold &r)>
        colour_ordering;

class ColourThresholdOption : public GameOption
{
public:
    ColourThresholdOption(colour_thresholds &val, std::set<std::string> _names,
                          string _default, colour_ordering ordering_func)
        : GameOption(_names), value(val), ordering_function(ordering_func),
          default_value(parse_colour_thresholds(_default)) { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const ColourThresholdOption *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }


    string loadFromString(const string &field, rc_line_type ltyp) override;

private:
    colour_thresholds parse_colour_thresholds(const string &field,
                                              string* error = nullptr) const;

    colour_thresholds &value;
    colour_ordering ordering_function;
    colour_thresholds default_value;
};

// T must be convertible to a string.
template<typename T>
class ListGameOption : public GameOption
{
public:
    ListGameOption(vector<T> &list, std::set<std::string> _names,
                   vector<T> _default = {})
        : GameOption(_names), value(list), default_value(_default) { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const ListGameOption<T> *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }

    string loadFromString(const std::string &field, rc_line_type ltyp) override
    {
        if (ltyp == RCFILE_LINE_EQUALS)
            value.clear();

        vector<T> new_entries;
        for (const auto &part : split_string(",", field))
        {
            if (part.empty())
                continue;

            if (ltyp == RCFILE_LINE_MINUS)
                remove_matching(value, T(part));
            else
                new_entries.emplace_back(part);
        }
        merge_lists(value, new_entries, ltyp == RCFILE_LINE_CARET);
        return GameOption::loadFromString(field, ltyp);
    }

private:
    vector<T> &value;
    vector<T> default_value;
};

// A template for an option which can take one of a fixed list of values.
// Trying to set it to a value which isn't listed in _choices gives an error
// message, and does not alter _val.
template<typename T>
class MultipleChoiceGameOption : public GameOption
{
public:
    MultipleChoiceGameOption(T &_val, std::set<std::string> _names, T _default,
                             map<string, T> _choices,
                             bool _normalize_bools=false)
        : GameOption(_names), value(_val), default_value(_default),
          choices(_choices), normalize_bools(_normalize_bools)
    { }

    void reset() override
    {
        value = default_value;
        GameOption::reset();
    }

    void set_from(const GameOption *other) override
    {
        const auto other_casted = dynamic_cast<const MultipleChoiceGameOption<T> *>(other);
        // ugly: I can't currently find any better way to enforce types
        ASSERT(other_casted);
        value = other_casted->value;
    }

    string loadFromString(const std::string &field, rc_line_type ltyp) override
    {
        string normalized = field;
        if (normalize_bools)
        {
            if (field == "1" || field == "yes")
                normalized = "true";
            else if (field == "0" || field == "no")
                normalized = "false";
        }

        const T *choice = map_find(choices, normalized);
        if (choice == 0)
        {
            string all_choices = comma_separated_fn(choices.begin(),
                choices.end(), [] (const pair<string, T> &p) {return p.first;},
                " or ");
            return make_stringf("Bad %s value: %s (should be %s)",
                                name().c_str(), field.c_str(),
                                all_choices.c_str());
        }
        else
        {
            value = *choice;
            return GameOption::loadFromString(normalized, ltyp);
        }
    }

private:
    T &value, default_value;
    map<string, T> choices;
    bool normalize_bools;
};

bool read_bool(const std::string &field, bool def_value);
maybe_bool read_maybe_bool(const std::string &field);
