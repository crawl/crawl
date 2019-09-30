#pragma once

struct newgame_def;
class formatted_string;

formatted_string opening_screen();
formatted_string options_read_status();
bool validate_player_name(const string &name);
bool is_good_name(const string &name, bool blankOK);
void enter_player_name(newgame_def& ng);
