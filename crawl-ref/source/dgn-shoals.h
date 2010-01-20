#ifndef DGN_SHOALS_H
#define DGN_SHOALS_H

void dgn_build_shoals_level(int level_number);
void shoals_postprocess_level();
void shoals_apply_tides(int turns_elapsed, bool force = false);
void shoals_release_tide(monsters *caller);

#ifdef WIZARD
void wizard_mod_tide();
#endif

#endif
