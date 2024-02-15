#pragma once

#include "god-type.h"

void mons_att_changed(monster* mons);

void fedhas_neutralise(monster* mons);
void beogh_follower_convert(monster* mons, bool orc_hit = false);
void slime_convert(monster* mons);
void dismiss_god_summons(god_type god);
enum class conv_t
{
    sight,
    deathbed,
    deathbed_follower,
    resurrection,
};
void beogh_convert_orc(monster* orc, conv_t conv);
void gozag_set_bribe(monster* traitor);
void gozag_check_bribe(monster* traitor);
void gozag_break_bribe(monster* victim);
void do_conversions(monster* target);
