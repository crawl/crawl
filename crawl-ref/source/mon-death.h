/*
 *  File:       mon-death.h
 *  Summary:    Monster death functionality (kraken, uniques, and so-on).
 */

bool mons_is_pikel (monster* mons);
void pikel_band_neutralise(bool check_tagged = false);

bool mons_is_duvessa (monster* mons);
bool mons_is_dowan (monster* mons);
bool mons_is_elven_twin (monster* mons);
void elven_twin_died(monster* twin, bool in_transit, killer_type killer, int killer_index);
void elven_twins_pacify (monster* twin);
void elven_twins_unpacify (monster* twin);

bool mons_is_kirke (monster* mons);
void hogs_to_humans();
