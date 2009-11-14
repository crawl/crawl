#ifndef TELEPORT_H
#define TELEPORT_H

class actor;
class monsters;

void blink_other_close(actor* victim, const coord_def& target);
void blink_away(monsters* mon);
void blink_range(monsters* mon);

bool random_near_space(const coord_def& origin, coord_def& target,
                       bool allow_adjacent = false, bool restrict_LOS = true,
                       bool forbid_dangerous = true,
                       bool forbid_sanctuary = false);

#endif

