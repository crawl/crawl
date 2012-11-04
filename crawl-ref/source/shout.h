#ifndef SHOUT_H
#define SHOUT_H

bool noisy(int loudness, const coord_def& where, int who,
           bool mermaid = false, bool message_if_unseen = false);
bool noisy(int loudness, const coord_def& where, const char *msg = NULL,
           int who = -1, bool mermaid = false, bool message_if_unseen = false,
           bool fake_noise = false);
bool fake_noisy(int loudness, const coord_def& where);

void item_noise(const item_def& item, string msg, int loudness = 25);
void noisy_equipment();

void check_player_sense(sense_type sense, int range, const coord_def& where);
void check_monsters_sense(sense_type sense, int range, const coord_def& where);

void blood_smell(int strength, const coord_def& where);
void handle_monster_shouts(monster* mons, bool force = false);
bool check_awaken(monster* mons);

void apply_noises();

#endif
