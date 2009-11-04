#ifndef FEATURE_H
#define FEATURE_H

#include "show.h"

struct feature_def
{
    dungeon_char_type   dchar;
    unsigned            symbol;          // symbol used for seen terrain
    unsigned            magic_symbol;    // symbol used for magic-mapped terrain
    unsigned short      colour;          // normal in LoS colour
    unsigned short      map_colour;      // colour when out of LoS on display
    unsigned short      seen_colour;     // map_colour when is_terrain_seen()
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

struct feature_override
{
    show_type    object;
    feature_def  override;
};

const feature_def &get_feature_def(show_type object);
const feature_def &get_feature_def(dungeon_feature_type feat);

void init_show_table();

#endif
