#ifndef DGN_SHOALS_H
#define DGN_SHOALS_H

void prepare_shoals(int level_number);
void shoals_postprocess_level();
void shoals_apply_tides(int turns_elapsed, bool force = false);
void shoals_release_tide(monsters *caller);

#endif
