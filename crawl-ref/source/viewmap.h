#ifndef VIEWMAP_H
#define VIEWMAP_H

screen_buffer_t colour_code_map(const coord_def& p, bool item_colour = false,
                                bool travel_colour = false, bool on_level = true);

bool emphasise(const coord_def& where);

#endif

