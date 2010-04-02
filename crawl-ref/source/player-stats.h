#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

void attribute_increase();

void modify_stat(stat_type which_stat, char amount, bool suppress_msg,
                 const char* cause, bool see_source = true);
    
void notify_stat_change(stat_type which_stat, char amount, bool suppress_msg,
                        const char* cause, bool see_source = true);
void notify_stat_change(stat_type which_stat, char amount, bool suppress_msg,
                        const item_def &cause, bool removed = false);
void notify_stat_change(const char* cause);

int stat_modifier(stat_type stat);

void jiyva_stat_action();

bool lose_stat(stat_type which_stat, unsigned char stat_loss,
               bool force = false, const std::string cause = "",
               bool see_source = true);
bool lose_stat(stat_type which_stat, unsigned char stat_loss,
               bool force = false, const char* cause = NULL,
               bool see_source = true);
bool lose_stat(stat_type which_stat, unsigned char stat_loss,
               const monsters* cause, bool force = false);
bool lose_stat(stat_type which_stat, unsigned char stat_loss,
               const item_def &cause, bool removed, bool force = false);

bool restore_stat(stat_type which_stat, unsigned char stat_gain,
                  bool suppress_msg, bool recovery = false);

#endif
