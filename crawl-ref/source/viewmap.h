#ifndef VIEWMAP_H
#define VIEWMAP_H

struct level_pos;
bool is_feature(ucs_t feature, const coord_def& where);
bool show_map(level_pos &spec_place,
              bool travel_mode, bool allow_esc, bool allow_offlevel);

bool emphasise(const coord_def& where);
unsigned get_map_col(const coord_def& c, bool travel = true);

#endif
