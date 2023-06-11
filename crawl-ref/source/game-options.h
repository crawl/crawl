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
#include "fixedp.h"
#include "stringutil.h"
#include "maybe-bool.h"
#include "options-to-string.h"
#include "rc-line-type.h"

using std::vector;
struct game_options;
struct opt_parse_state;

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

// standard implementations of GameOption subclasses have a `value` at some
// type. This isn't templated (and we are using a complex mix of templating and
// polymorphism in general), but we still want a superclass `set_from` function
// that both checks type and works with arbitrary GameOption objects. The best
// option I've come up with (without rewriting the whole thing to use a lot more
// templating) is just to do a dynamic_cast in a subclass implementation. This
// ends up being boilerplate for both set_from and reset, so it's consolidated
// here as macros. (Is there a more c++ way?)

#define OPT_SET_FROM(C) bool try_set_from(const GameOption *other) override \
    { \
        const auto other_casted = dynamic_cast<const C *>(other); \
        if (other_casted) \
            value = other_casted->value; \
        return other_casted; \
    }

#define OPT_RESET() void reset() override \
    { \
        value = default_value; \
        GameOption::reset(); \
    }

class GameOption
{
public:
    GameOption(vector<string> _names,
               bool _case_sensitive=false,
               function<void()> _on_set=nullptr)
        : on_set(_on_set),
            canonical_name(_names.size() > 0 ? _names[0] : ""),
            names(set<string>(_names.begin(), _names.end())),
            case_sensitive(_case_sensitive), loaded(false)
    {
        // does on_set ever need to be called from here? for currently
        // relevant subclasses, default values are handled in reset(), not in
        // the constructor
    }
    virtual ~GameOption() {};

    // XX reset, set_from, and some other stuff could be templated for most
    // subclasses, but this is hard to reconcile with the polymorphism involved

    // subclass should override and call this:
    virtual void reset()
    {
        loaded = false;
        if (on_set)
            on_set();
    }
    virtual bool try_set_from(const GameOption *other) = 0; // subclass should implement

    void set_from(const GameOption *other)
    {
        ASSERT(try_set_from(other));
    }

    void set_from(const GameOption &other)
    {
        return set_from(&other);
    }

    virtual string str() const = 0;

    virtual string loadFromString(const std::string &, rc_line_type);
    virtual string loadFromParseState(const opt_parse_state &state);

    const std::set<std::string> &getNames() const { return names; }
    const std::string name() const { return canonical_name; }

    bool was_loaded() const { return loaded; }

    function<void()> on_set;

protected:
    string canonical_name;
    std::set<std::string> names;
    bool case_sensitive;
    bool loaded; // tracks whether the option has changed via loadFromString.
                 // will miss whether it was changed directly in c++ code. (TODO)

    friend struct base_game_options;
    friend struct game_options;
};

// warning: if you use this, and a value needs initialized, be sure to do that
// somehow! This class won't do it for you.
class DisabledGameOption : public GameOption
{
public:
    DisabledGameOption(vector<string> _names)
        : GameOption(_names)
    {
    }

    void reset() override
    {
        GameOption::reset();
    }

    bool try_set_from(const GameOption *other) override
    {
        // pure type check:
        return dynamic_cast<const DisabledGameOption *>(other);
    }

    string str() const override { return ""; } // or ASSERT?
    string loadFromString(const std::string &, rc_line_type) override
    {
        // not actually called in automatic code
        if (on_set)
            on_set();
        return make_stringf("disabled option");
    }

    virtual string loadFromParseState(const opt_parse_state &state) override;
};

class BoolGameOption : public GameOption
{
public:
    BoolGameOption(bool &val, vector<string> _names,
                   bool _default,
                   function<void()> _on_set = nullptr)
        : GameOption(_names, false, _on_set), value(val), default_value(_default) { }

    OPT_RESET()
    OPT_SET_FROM(BoolGameOption)

    string str() const override { return value ? "true" : "false"; }
    string loadFromString(const std::string &field, rc_line_type) override;

private:
    bool &value;
    bool default_value;
};

class ColourGameOption : public GameOption
{
public:
    ColourGameOption(unsigned &val, vector<string> _names,
                     unsigned _default, bool _elemental = false)
        : GameOption(_names), value(val), default_value(_default),
          elemental(_elemental) { }

    OPT_RESET()
    OPT_SET_FROM(ColourGameOption)

    string str() const override;
    string loadFromString(const std::string &field, rc_line_type) override;

private:
    unsigned &value;
    unsigned default_value;
    bool elemental; // seems to be unused -- remove?
};

class CursesGameOption : public GameOption
{
public:
    CursesGameOption(unsigned &val, vector<string> _names,
                     unsigned _default)
        : GameOption(_names), value(val), default_value(_default) { }

    OPT_RESET()
    OPT_SET_FROM(CursesGameOption)

    string str() const override;
    string loadFromString(const std::string &field, rc_line_type) override;

private:
    unsigned &value;
    unsigned default_value;
};

class IntGameOption : public GameOption
{
public:
    IntGameOption(int &val, vector<string> _names, int _default,
                  int min_val = INT_MIN, int max_val = INT_MAX)
        : GameOption(_names), value(val), default_value(_default),
          min_value(min_val), max_value(max_val) { }

    OPT_RESET()
    OPT_SET_FROM(IntGameOption)

    string str() const override { return to_string(value); }
    string loadFromString(const std::string &field, rc_line_type) override;

private:
    int &value;
    int default_value, min_value, max_value;
};

// could template this for fixedp settings, but it's probably better not to
class FixedpGameOption : public GameOption
{
public:
    FixedpGameOption(fixedp<> &val, vector<string> _names, fixedp<> _default,
                  fixedp<> min_val = numeric_limits<fixedp<>>::min(),
                  fixedp<> max_val = numeric_limits<fixedp<>>::max())
        : GameOption(_names), value(val), default_value(_default),
          min_value(min_val), max_value(max_val)
    { }

    OPT_RESET()
    OPT_SET_FROM(FixedpGameOption)

    string str() const override { return value.str(); }
    string loadFromString(const std::string &field, rc_line_type) override;

private:
    fixedp<> &value;
    fixedp<> default_value, min_value, max_value;
};

class StringGameOption : public GameOption
{
public:
    StringGameOption(string &val, vector<string> _names,
                     string _default, bool _case_sensitive=false,
                     function<void()> _on_set=nullptr)
        : GameOption(_names, _case_sensitive, _on_set), value(val), default_value(_default) { }

    OPT_RESET()
    OPT_SET_FROM(StringGameOption)

    string str() const override { return value; }
    string loadFromString(const std::string &field, rc_line_type) override;

private:
    string &value;
    string default_value;
};

#ifdef USE_TILE
class TileColGameOption : public GameOption
{
public:
    TileColGameOption(VColour &val, vector<string> _names,
                      string _default);

    OPT_RESET()
    OPT_SET_FROM(TileColGameOption)

    string str() const override;
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
    ColourThresholdOption(colour_thresholds &val, vector<string> _names,
                          string _default, colour_ordering ordering_func)
        : GameOption(_names), value(val), ordering_function(ordering_func),
          default_value(parse_colour_thresholds(_default)) { }

    OPT_RESET()
    OPT_SET_FROM(ColourThresholdOption)

    string str() const override;
    string loadFromString(const string &field, rc_line_type ltyp) override;

private:
    colour_thresholds parse_colour_thresholds(const string &field,
                                              string* error = nullptr) const;

    colour_thresholds &value;
    colour_ordering ordering_function;
    colour_thresholds default_value;
};

// T must be convertible from a string via either a constructor or a
// specialization of options::from_string. It also needs some way of converting
// to a string, either via a ::str() function, or an options::to_string
// specialization or overload.
template<typename T>
class ListGameOption : public GameOption
{
public:
    ListGameOption(vector<T> &list, vector<string> _names,
                   vector<T> _default = {},
                   bool _case_sensitive = false,
                   function<void()> _on_set = nullptr)
        : GameOption(_names, _case_sensitive, _on_set),
          value(list), default_value(_default)
    {
    }

    OPT_RESET()
    OPT_SET_FROM(ListGameOption<T>)

    // template nonsense follows. We choose between these calls using SFINAE
    // based on whether V is constructible from a string, or not. If not, try
    // to call an options::from_string variant.
    // other notes:
    // * the `typename V = T` is present because SFINAE resolution will not
    //   trigger unless there is some kind of inference.
    // * the `bool` in the second one is in order to keep the two from having
    //   the same type signature, and believe it or not, is the right way to
    //   do this, at least in c++11.
    template <typename V = T,
        typename = typename std::enable_if<std::is_constructible<V, std::string>::value>::type>
    T element_from_string(const string &s)
    {
        return V(s);
    }

    template <typename V = T,
        typename std::enable_if<!std::is_constructible<V, std::string>::value, bool>::type = true>
    T element_from_string(const string &s)
    {
        return options::from_string<V>(s);
    }

    string str() const override
    {
        // note: if insane template errors show up here, debugging may be
        // easier if you convert this to procedural code and use join_strings
        // instead
        return comma_separated_fn(value.begin(), value.end(),
            options::to_string<T>, ", ", ", ");
    }

    string loadFromString(const std::string &field, rc_line_type ltyp) override
    {
        if (ltyp == RCFILE_LINE_EQUALS)
            value.clear();

        vector<T> new_entries;
        // note: this does *not* handle `\\` escapes right now (because they
        // could also be relevant for subsplits). If your use case is likely
        // to have this, you should handle it yourself somehow. (In some cases,
        // e.g. where this is used for regex parsing, they should be left as-is
        // in any case.)
        for (const auto &part : split_string(",", field, true, false, -1, true))
        {
            if (part.empty())
                continue;

            T element = element_from_string(part);

            if (ltyp == RCFILE_LINE_MINUS)
                remove_matching(value, element);
            else
                new_entries.push_back(move(element));
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
    MultipleChoiceGameOption(T &_val, vector<string> _names, T _default,
                             vector<pair<string, T>> _choices,
                             bool _normalize_bools=false)
        : GameOption(_names), value(_val), default_value(_default),
          choices(map<string, T>(_choices.begin(), _choices.end())),
          normalize_bools(_normalize_bools)
    {
        for (auto c = _choices.begin(); c != _choices.end(); ++c)
            if (rchoices.count(c->second) == 0) // treat first instance as canonical
                rchoices[c->second] = c->first;
    }

    OPT_RESET()
    OPT_SET_FROM(MultipleChoiceGameOption<T>)

    string str() const override
    {
        const auto choice = rchoices.find(value);
        ASSERT(choice != rchoices.end());
        return choice->second;
    }

    string loadFromString(const std::string &field, rc_line_type ltyp) override
    {
        string normalized = field;
        if (!choices.size())
        {
            return make_stringf("Option '%s' is disabled in this build.",
                name().c_str());
        }
        if (normalize_bools)
        {
            if (field == "1" || field == "yes")
                normalized = "true";
            else if (field == "0" || field == "no")
                normalized = "false";
        }

        const T *choice = map_find(choices, normalized);
        if (choice == nullptr)
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

protected:
    T &value, default_value;
    map<string, T> choices;
    map<T, string> rchoices;
    bool normalize_bools;
};

// Convenience specialization of the multiple choice case for maybe_bool.
// use this if want you want is a straightforward mapping of "true"->true,
// "maybe"->maybe_bool::maybe, and "false"->false. (Also handles 0,1,yes,no).
// You can add extra words for "maybe". Otherwise, use a more general
// MultipleChoiceGameOption invocation.
class MaybeBoolGameOption : public MultipleChoiceGameOption<maybe_bool>
{
public:
    MaybeBoolGameOption(maybe_bool &_val, vector<string> _names,
                maybe_bool _default, vector<string> extra_maybe = {})
        : MultipleChoiceGameOption(_val, _names, _default,
            {{"true", maybe_bool::t},
             {"false", maybe_bool::f},
             {"maybe", maybe_bool::maybe}},
            true)
    {
        for (const auto &s : extra_maybe)
            choices[s] = maybe_bool::maybe;
    }

    OPT_SET_FROM(MaybeBoolGameOption)
};

#undef OPT_SET_FROM
#undef OPT_RESET

bool read_bool(const std::string &field, bool def_value);
maybe_bool read_maybe_bool(const std::string &field);
