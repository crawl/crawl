#ifndef FEATURE_H
#define FEATURE_H

#include "show.h"

struct feature_def
{
    dungeon_char_type   dchar;
    ucs_t               symbol;          // symbol used for seen terrain
    ucs_t               magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short      colour;          // normal in LoS colour
    unsigned short      map_colour;      // colour when out of LoS on display
    unsigned short      seen_colour;     // map_colour when env.map_knowledge().seen()
    unsigned short      em_colour;       // Emphasised colour when in LoS.
    unsigned short      seen_em_colour;  // Emphasised colour when out of LoS
    unsigned            flags;
    map_feature         minimap;         // mini-map categorization

    feature_def() :
        dchar(NUM_DCHAR_TYPES),
        symbol(0),
        magic_symbol(0),
        colour(BLACK),
        map_colour(DARKGREY),
        seen_colour(BLACK),
        em_colour(BLACK),
        seen_em_colour(BLACK),
        flags(FFT_NONE),
        minimap(MF_UNSEEN)
    {}

    bool is_notable() const { return (flags & FFT_NOTABLE); }
};

const feature_def &get_feature_def(show_type object);
const feature_def &get_feature_def(dungeon_feature_type feat);

static inline bool is_notable_terrain(dungeon_feature_type ftype)
{
    return (get_feature_def(ftype).is_notable());
}

void init_show_table();

dungeon_feature_type magic_map_base_feat(dungeon_feature_type feat);

#endif
