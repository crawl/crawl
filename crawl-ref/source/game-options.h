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
    virtual void loadFromString(std::string field) const = 0;

    const std::set<std::string> &getNames() { return names; }

    private:
    std::set<std::string> names;
};

class BoolGameOption : public GameOption
{
    public:
    BoolGameOption(bool &val, std::set<std::string> _names,
                   bool _default)
        : GameOption(_names), value(val), default_value(_default) { }
    void reset() const override;
    void loadFromString(std::string field) const override;

    private:
    bool &value;
    bool default_value;
};

bool read_bool(const std::string &field, bool def_value);

#endif // GAME_OPTION_H
