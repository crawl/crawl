#ifndef ATTITUDE_CHANGE_H
#define ATTITUDE_CHANGE_H

void mons_att_changed(monster* mons);

void fedhas_neutralise(monster* mons);
void beogh_follower_convert(monster* mons, bool orc_hit = false);
void slime_convert(monster* mons);
bool yred_slaves_abandon_you();
bool beogh_followers_abandon_you();
bool make_god_gifts_disappear();
void beogh_convert_orc(monster* orc, bool emergency,
                       bool converted_by_follower = false);
void gozag_set_bribe(monster* traitor);
void gozag_check_bribe(monster* traitor);
void gozag_break_bribe(monster* victim);
#endif
