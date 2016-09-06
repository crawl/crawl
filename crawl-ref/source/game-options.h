/*
 *  @file
 *  @brief Global game options controlled by the rcfile.
 */

#ifndef GAME_OPTIONS_H
#define GAME_OPTIONS_H

#include <string>

class GameOption
{
    public:
    virtual ~GameOption() {};

    virtual void reset() const = 0;
    virtual void loadFromString(std::string field) const = 0;
};

class BoolGameOption : public GameOption
{
    public:
    BoolGameOption(bool &val, bool _default)
        : value(val), default_value(_default) { reset(); }
    void reset() const override;
    void loadFromString(std::string field) const override;

    private:
    bool &value;
    bool default_value;
};

bool read_bool(const std::string &field, bool def_value);

#endif // GAME_OPTION_H
