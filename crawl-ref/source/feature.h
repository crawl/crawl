#ifndef FEATURE_H
#define FEATURE_H

#include "show.h"

struct feature_def
{
    dungeon_feature_type feat;
    const char*          name;
    dungeon_char_type    dchar;           // used for creating symbol
    ucs_t                symbol;          // symbol used for seen terrain
    ucs_t                magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short       colour;          // normal in LoS colour
    unsigned short       map_colour;      // colour when out of LoS on display
    unsigned short       seen_colour;     // map_colour when env.map_knowledge().seen()
    unsigned short       em_colour;       // Emphasised colour when in LoS.
    unsigned short       seen_em_colour;  // Emphasised colour when out of LoS
    unsigned             flags;
    map_feature          minimap;         // mini-map categorization

    bool is_notable() const { return flags & FFT_NOTABLE; }
};

void init_fd(feature_def& fd);
void init_show_table();

const feature_def &get_feature_def(show_type object);
const feature_def &get_feature_def(dungeon_feature_type feat);
bool is_valid_feature_type(dungeon_feature_type feat);

static inline bool is_notable_terrain(dungeon_feature_type ftype)
{
    return get_feature_def(ftype).is_notable();
}

dungeon_feature_type magic_map_base_feat(dungeon_feature_type feat);

#endif
