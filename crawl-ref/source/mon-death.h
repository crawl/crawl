/*
 *  File:       mon-death.h
 *  Summary:    Monster death functionality (kraken, uniques, and so-on).
 */

bool mons_is_pikel (monsters* mons);
void pikel_band_neutralise(bool check_tagged = false);

bool mons_is_duvessa (monsters *mons);
bool mons_is_dowan (monsters *mons);
bool mons_is_elven_twin (monsters *mons);
void elven_twin_died(monsters* twin, bool in_transit, killer_type killer, int killer_index);
void elven_twins_pacify (monsters* twin);
void elven_twins_unpacify (monsters* twin);

bool mons_is_kirke (monsters* mons);
void hogs_to_humans();
