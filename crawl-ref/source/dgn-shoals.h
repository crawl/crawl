#pragma once

void dgn_build_shoals_level();
void dgn_shoals_generate_flora();
bool dgn_shoals_connect_point(const coord_def &point,
                              bool (*overwriteable)(dungeon_feature_type));
void shoals_postprocess_level();
void shoals_apply_tides(int turns_elapsed, bool force,
                        bool incremental_tide);
void shoals_release_tide(monster* caller);

#ifdef WIZARD
void wizard_mod_tide();
#endif

