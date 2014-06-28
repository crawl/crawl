/**
 * @file
 * @brief Monster message-connected functions.
**/

#ifndef MONMESSAGE_H
#define MONMESSAGE_H

enum mon_dam_level_type
{
    MDAM_OKAY,
    MDAM_LIGHTLY_DAMAGED,
    MDAM_MODERATELY_DAMAGED,
    MDAM_HEAVILY_DAMAGED,
    MDAM_SEVERELY_DAMAGED,
    MDAM_ALMOST_DEAD,
    MDAM_DEAD,
};

bool simple_monster_message(const monster* mons, const char *event,
                            msg_channel_type channel = MSGCH_PLAIN,
                            int param = 0,
                            description_level_type descrip = DESC_THE);

void print_wounds(const monster* mons);
bool wounded_damaged(mon_holy_type holi);

mon_dam_level_type mons_get_damage_level(const monster* mons);

string get_damage_level_string(mon_holy_type holi, mon_dam_level_type mdam);
bool mons_class_can_display_wounds(monster_type mc);
bool mons_can_display_wounds(const monster* mon);
#endif
