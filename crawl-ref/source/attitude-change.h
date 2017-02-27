#pragma once

void mons_att_changed(monster* mons);

void fedhas_neutralise(monster* mons);
void beogh_follower_convert(monster* mons, bool orc_hit = false);
void slime_convert(monster* mons);
bool yred_slaves_abandon_you();
bool beogh_followers_abandon_you();
void make_god_gifts_disappear();
enum class conv_t
{
    SIGHT,
    DEATHBED,
    DEATHBED_FOLLOWER,
    RESURRECTION,
};
void beogh_convert_orc(monster* orc, conv_t conv);
void gozag_set_bribe(monster* traitor);
void gozag_check_bribe(monster* traitor);
void gozag_break_bribe(monster* victim);
