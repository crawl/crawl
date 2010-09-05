#ifndef ATTITUDE_CHANGE_H
#define ATTITUDE_CHANGE_H

void good_god_follower_attitude_change(monsters* mons);
void fedhas_neutralise(monsters* mons);
bool fedhas_plants_hostile();
void beogh_follower_convert(monsters* mons, bool orc_hit = false);
void slime_convert(monsters* mons);
bool yred_slaves_abandon_you();
bool beogh_followers_abandon_you();
bool make_god_gifts_disappear();
void good_god_holy_attitude_change(monsters *holy);
void good_god_holy_fail_attitude_change(monsters *holy);
void beogh_convert_orc(monsters *orc, bool emergency,
                       bool converted_by_follower = false);

#endif
