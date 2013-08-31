/**
 * @file
 * @brief Monster death functionality (kraken, uniques, and so-on).
**/

#ifndef MONDEATH_H
#define MONDEATH_H

bool mons_is_pikel(monster* mons);
void pikel_band_neutralise();

bool mons_is_duvessa(const monster* mons);
bool mons_is_dowan(const monster* mons);
bool mons_is_elven_twin(const monster* mons);
void elven_twin_died(monster* twin, bool in_transit, killer_type killer, int killer_index);
void elven_twins_pacify(monster* twin);
void elven_twins_unpacify(monster* twin);

bool mons_is_kirke(monster* mons);
void hogs_to_humans();

bool mons_is_phoenix(const monster* mons);
void phoenix_died(monster* mons);
void timeout_phoenix_markers(int duration);

monster* get_shedu_pair(const monster* mons);
bool shedu_pair_alive(const monster* mons);
bool mons_is_shedu(const monster* mons);
void shedu_do_resurrection(const monster *mons);
void shedu_do_actual_resurrection(monster* mons);

#endif
