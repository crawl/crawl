#pragma once

class actor;
class monster;

void blink_other_close(actor* victim, const coord_def& target);
bool blink_player_away(monster* caster);
bool blink_away(monster* mon, bool self_cast = false);
bool blink_away(monster* mon, actor* caster, bool from_seen = true,
                bool self_cast = false, int max_dist = LOS_RADIUS);
void blink_range(monster &mon);
void blink_close(monster &mon);
void mons_relocated(monster* mons);
bool monster_blink(monster* mons, bool ignore_stasis = false, bool quiet = false);
bool monster_space_valid(const monster* mons, coord_def target,
                         bool forbid_sanctuary);
void monster_teleport(monster* mons, bool instan, bool silent = false,
                      bool away_from_player = false, const actor* agent = nullptr);

vector<coord_def> find_blink_targets();
bool valid_blink_destination(const actor &moved, const coord_def& target,
                             bool forbid_sanctuary = false,
                             bool forbid_unhabitable = true,
                             bool incl_unseen = true);
bool random_near_space(const actor* victim,
                       const coord_def& origin, coord_def& target,
                       bool allow_adjacent = false,
                       int max_distance = LOS_RADIUS,
                       bool forbid_sanctuary = false,
                       bool forbid_unhabitable = true);
