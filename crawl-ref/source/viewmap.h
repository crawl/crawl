#pragma once

struct level_pos;
bool travel_colour_override(const coord_def& p);
bool is_feature(char32_t feature, const coord_def& where);
bool show_map(level_pos &spec_place,
              bool travel_mode, bool allow_esc, bool allow_offlevel);

bool emphasise(const coord_def& where);
