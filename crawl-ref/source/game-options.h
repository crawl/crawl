/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#pragma once

#include <functional>
#include <string>
#include <set>

#include "colour.h"
#include "stringutil.h"
#include "maybe-bool.h"

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
        : names(_names) { }
    virtual ~GameOption() {};

    virtual void reset() const = 0;
    virtual string loadFromString(std::string field, rc_line_type) const = 0;

    const std::set<std::string> &getNames() const { return names; }
    const std::string name() const { return *names.begin(); }

    protected:
    std::set<std::string> names;
};

class BoolGameOption : public GameOption
{
    public:
    BoolGameOption(bool &val, std::set<std::string> _names,
                   bool _default)
        : GameOption(_names), value(val), default_value(_default) { }
    void reset() const override;
    string loadFromString(std::string field, rc_line_type) const override;

    private:
    bool &value;
    bool default_value;
};

class ColourGameOption : public GameOption
{
public:
    ColourGameOption(unsigned &val, std::set<std::string> _names,
                     unsigned _default, bool _elemental = true)
        : GameOption(_names), value(val), default_value(_default),
          elemental(_elemental) { }
    void reset() const override;
    string loadFromString(std::string field, rc_line_type) const override;

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
    void reset() const override;
    string loadFromString(std::string field, rc_line_type) const override;

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
    void reset() const override;
    string loadFromString(std::string field, rc_line_type) const override;

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
    void reset() const override;
    string loadFromString(std::string field, rc_line_type) const override;

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
    void reset() const override;
    string loadFromString(std::string field, rc_line_type) const override;

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
    void reset() const override;
    string loadFromString(string field, rc_line_type ltyp) const override;

private:
    colour_thresholds parse_colour_thresholds(string field,
                                              string* error = nullptr) const;

private:
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

    void reset() const override { value = default_value; }
    string loadFromString(std::string field, rc_line_type ltyp) const override
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
        return "";
    }

private:
    vector<T> &value;
    vector<T> default_value;
};

bool read_bool(const std::string &field, bool def_value);
maybe_bool read_maybe_bool(const std::string &field);
