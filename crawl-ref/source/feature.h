#ifndef FEATURE_H
#define FEATURE_H

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

    bool is_notable() const { return (flags & FFT_NOTABLE); }
};

struct feature_override
{
    dungeon_feature_type    feat;
    feature_def             override;
};

const feature_def &get_feature_def(dungeon_feature_type feat);

void init_feature_table();

#endif
