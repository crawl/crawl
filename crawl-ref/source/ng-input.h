#pragma once

struct newgame_def;

string opening_screen();
bool validate_player_name(const string &name, bool verbose);
bool is_good_name(const string &name, bool blankOK);
void enter_player_name(newgame_def& ng);
