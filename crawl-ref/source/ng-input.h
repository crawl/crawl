#ifndef NG_INPUT_H
#define NG_INPUT_H

struct newgame_def;

void opening_screen();
bool validate_player_name(const std::string &name, bool verbose);
void enter_player_name(newgame_def &ng, bool blankOK);

#endif

