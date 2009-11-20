#ifndef ATTITUDE_CHANGE_H
#define ATTITUDE_CHANGE_H

void good_god_follower_attitude_change(monsters *monster);
void fedhas_neutralise(monsters* monster);
bool fedhas_plants_hostile();
void beogh_follower_convert(monsters *monster, bool orc_hit = false);
void slime_convert(monsters *monster);
bool holy_beings_attitude_change();
bool unholy_and_evil_beings_attitude_change();
bool unclean_and_chaotic_beings_attitude_change();
bool spellcasters_attitude_change();
bool yred_slaves_abandon_you();
bool beogh_followers_abandon_you();
bool jiyva_slimes_abandon_you();
bool make_god_gifts_disappear(bool level_only = true);
bool make_holy_god_gifts_good_neutral(bool level_only = true);
bool make_god_gifts_hostile(bool level_only = true);
void good_god_holy_attitude_change(monsters *holy);
void good_god_holy_fail_attitude_change(monsters *holy);
void beogh_convert_orc(monsters *orc, bool emergency,
                       bool converted_by_follower = false);
void jiyva_convert_slime(monsters* slime);
void fedhas_neutralise_plant(monsters *plant);

#endif
