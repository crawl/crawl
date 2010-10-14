#ifndef SHOUT_H
#define SHOUT_H

bool noisy(int loudness, const coord_def& where, int who,
           bool mermaid = false);
bool noisy(int loudness, const coord_def& where, const char *msg = NULL,
           int who = -1, bool mermaid = false);
void blood_smell(int strength, const coord_def& where);
void handle_monster_shouts(monster* mons, bool force = false);
void force_monster_shout(monster* mons);
bool check_awaken(monster* mons);

#endif
