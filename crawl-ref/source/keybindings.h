// keybindings.h
#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

#include <vector>
#include <string>

struct Keybinding {
    std::string action;
    std::string key;
};

// Example movement keys
extern std::vector<Keybinding> movement_keys;

void change_keybinding(int index);

#endif // KEYBINDINGS_H
