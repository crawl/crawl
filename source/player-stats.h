#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

enum stat_desc_type
{
    SD_NAME,
    SD_LOSS,
    SD_DECREASE,
    SD_INCREASE,
    NUM_STAT_DESCS
};

const char* stat_desc(stat_type stat, stat_desc_type desc);

bool attribute_increase();

void modify_stat(stat_type which_stat, int amount, bool suppress_msg,
                 bool see_source = true);

void notify_stat_change(stat_type which_stat, int amount,
                        bool suppress_msg, bool see_source = true);
void notify_stat_change();

void jiyva_stat_action();

bool lose_stat(stat_type which_stat, int stat_loss,
               bool force = false, const string cause = "",
               bool see_source = true);
bool lose_stat(stat_type which_stat, int stat_loss,
               bool force = false, const char* cause = nullptr,
               bool see_source = true);
bool lose_stat(stat_type which_stat, int stat_loss,
               const monster* cause, bool force = false);

bool restore_stat(stat_type which_stat, int stat_gain,
                  bool suppress_msg, bool recovery = false);

void update_stat_zero();

#endif
