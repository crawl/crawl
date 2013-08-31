#ifndef TELEPORT_H
#define TELEPORT_H

class actor;
class monster;

void blink_other_close(actor* victim, const coord_def& target);
bool blink_away(monster* mon, bool self_cast = false);
bool blink_away(monster* mon, actor* caster, bool from_seen = true, bool self_cast = false);
void blink_range(monster* mon);
void blink_close(monster* mon);

bool random_near_space(const coord_def& origin, coord_def& target,
                       bool allow_adjacent = false, bool restrict_LOS = true,
                       bool forbid_dangerous = false,
                       bool forbid_sanctuary = false);

#endif
