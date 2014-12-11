/**
 * @file
 * @brief Divine retribution.
**/

#ifndef GODWRATH_H
#define GODWRATH_H

bool divine_retribution(god_type god, bool no_bonus = false, bool force = false);
void ash_reduce_penance(int amount);

void beogh_idol_revenge();

void gozag_incite(monster *mon);
int gozag_goldify(const item_def &it, int quant_got, bool quiet = false);
#endif
