#ifndef FEATURE_H
#define FEATURE_H

#include "show.h"

enum feature_flag_type
{
    FFT_NONE          = 0,
    FFT_NOTABLE       = 1<< 0,           // should be noted for dungeon overview
    FFT_EXAMINE_HINT  = 1<< 1,           // could get an "examine-this" hint.
    FFT_OPAQUE        = 1<< 2,           // Does this feature block LOS?
    FFT_WALL          = 1<< 3,           // Is this a "wall"?
    FFT_SOLID         = 1<< 4,           // Does this feature block beams / normal movement?
    FFT_TRAP          = 1<< 5,           // Is this a "trap"?
};

struct feature_def
{
    dungeon_feature_type feat;
    const char*          name;
    const char*          vaultname;        // used for KFEAT and &(
    dungeon_char_type    dchar;            // the symbol
    dungeon_char_type    magic_dchar;      // the symbol shown when magic mapped
    colour_t             dcolour;          // normal in LoS colour
    colour_t             map_dcolour;      // colour when out of LoS on display
    colour_t             seen_dcolour;     // map_colour when env.map_knowledge().seen()
    colour_t             em_dcolour;       // Emphasised colour when in LoS.
    colour_t             seen_em_dcolour;  // Emphasised colour when out of LoS
    unsigned             flags;
    map_feature          minimap;          // mini-map categorization

    feature_def(
        dungeon_feature_type feat_ = DNGN_UNSEEN,
        const char *name_ = "", const char *vaultname_ = "",
        dungeon_char_type dchar_ = NUM_DCHAR_TYPES,
        dungeon_char_type magic_dchar_ = NUM_DCHAR_TYPES,
        colour_t dcolour_ = BLACK, colour_t map_dcolour_ = DARKGREY,
        colour_t seen_dcolour_ = BLACK, colour_t em_dcolour_ = BLACK,
        colour_t seen_em_dcolour_ = BLACK, unsigned flags_ = FFT_NONE,
        map_feature minimap_ = MF_UNSEEN) :
        feat{feat_}, name{name_}, vaultname{vaultname_}, dchar{dchar_},
        magic_dchar{magic_dchar_}, dcolour{dcolour_}, map_dcolour{map_dcolour_},
        seen_dcolour{seen_dcolour_}, em_dcolour{em_dcolour_},
        seen_em_dcolour{seen_em_dcolour_}, flags{flags_}, minimap{minimap_}
    {}

    bool is_notable() const { return flags & FFT_NOTABLE; }
    char32_t symbol() const;
    char32_t magic_symbol() const;
    colour_t colour() const;
    colour_t map_colour() const;
    colour_t seen_colour() const;
    colour_t em_colour() const;
    colour_t seen_em_colour() const;
};

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
