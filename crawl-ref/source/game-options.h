/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#ifndef GAME_OPTIONS_H
#define GAME_OPTIONS_H

#include <string>
#include <set>

class GameOption
{
    public:
    GameOption(std::set<std::string> _names)
        : names(_names) { }
    virtual ~GameOption() {};

    virtual void reset() const = 0;
    virtual string loadFromString(std::string field) const = 0;

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
    string loadFromString(std::string field) const override;

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
    string loadFromString(std::string field) const override;

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
    string loadFromString(std::string field) const override;

private:
    unsigned &value;
    unsigned default_value;
};

class IntGameOption : public GameOption
{
public:
    IntGameOption(int &val, std::set<std::string> _names, int _default,
                  int min_val, int max_val)
        : GameOption(_names), value(val), default_value(_default),
          min_value(min_val), max_value(max_val) { }
    void reset() const override;
    string loadFromString(std::string field) const override;

private:
    int &value;
    int default_value, min_value, max_value;
};

bool read_bool(const std::string &field, bool def_value);

#endif // GAME_OPTION_H
