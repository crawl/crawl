#ifndef VIEWMAP_H
#define VIEWMAP_H

struct level_pos;
bool show_map(level_pos &spec_place,
              bool travel_mode, bool allow_esc, bool allow_offlevel);

bool emphasise(const coord_def& where);
unsigned get_map_col(const coord_def& c, bool travel = true);

#endif
