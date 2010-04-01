#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

void attribute_increase();

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const std::string& cause, bool see_source = true);
void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const char* cause, bool see_source = true);
void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const monsters* cause);
void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const item_def &cause, bool removed = false);
    
int stat_modifier(stat_type stat);

void jiyva_stat_action();

#endif
