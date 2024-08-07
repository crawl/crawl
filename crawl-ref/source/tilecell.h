#ifdef USE_TILE
#pragma once

#include "map-cell.h"
#include "tag-version.h"

enum halo_type
{
    HALO_NONE = 0,
    HALO_RANGE = 1,
    HALO_UMBRA = 2,
};

struct packed_cell
{
    // For anything that requires multiple dungeon tiles (such as waves)
    // These tiles will be placed directly on top of the bg tile.
    enum { MAX_DNGN_OVERLAY = 20 };
    int num_dngn_overlay;
    FixedVector<int, MAX_DNGN_OVERLAY> dngn_overlay;

    tileidx_t fg;
    tileidx_t bg;
    tile_flavour flv;
    tileidx_t cloud;
    set<tileidx_t> icons;

    // This is directly copied from env.map_knowledge by viewwindow()
    map_cell map_knowledge;

    bool is_highlighted_summoner;
    bool is_bloody;
    bool is_silenced;
    char halo;
    bool is_sanctuary;
    bool is_blasphemy;
    bool is_liquefied;
    bool mangrove_water;
    bool awakened_forest;
    uint8_t orb_glow;
    char blood_rotation;
    bool old_blood;
    uint8_t travel_trail;
    bool quad_glow;
    uint8_t disjunct;
    bool has_bfb_corpse;

    bool operator ==(const packed_cell &other) const;
    bool operator !=(const packed_cell &other) const { return !(*this == other); }

    packed_cell() : num_dngn_overlay(0), fg(0), bg(0), cloud(0),
                    is_highlighted_summoner(false), is_bloody(false),
                    is_silenced(false), halo(HALO_NONE), is_sanctuary(false),
                    is_blasphemy(false), is_liquefied(false), mangrove_water(false),
                    awakened_forest(false), orb_glow(0), blood_rotation(0),
                    old_blood(false), travel_trail(0),
                    quad_glow(false), disjunct(false), has_bfb_corpse(false)
                    {}

    packed_cell(const packed_cell* c) : num_dngn_overlay(c->num_dngn_overlay),
                                        fg(c->fg), bg(c->bg), flv(c->flv),
                                        cloud(c->cloud),
                                        map_knowledge(c->map_knowledge),
                                        is_highlighted_summoner(c->is_highlighted_summoner),
                                        is_bloody(c->is_bloody),
                                        is_silenced(c->is_silenced),
                                        halo(c->halo),
                                        is_sanctuary(c->is_sanctuary),
                                        is_blasphemy(c->is_blasphemy),
                                        is_liquefied(c->is_liquefied),
                                        mangrove_water(c->mangrove_water),
                                        awakened_forest(c->awakened_forest),
                                        orb_glow(c->orb_glow),
                                        blood_rotation(c->blood_rotation),
                                        old_blood(c->old_blood),
                                        travel_trail(c->travel_trail),
                                        quad_glow(c->quad_glow),
                                        disjunct(c->disjunct),
                                        has_bfb_corpse(c->has_bfb_corpse)
                                        {}

    void clear();
};

class crawl_view_buffer;

// For a given location, pack any waves/ink/wall shadow tiles
// that require knowledge of the surrounding env cells.
void pack_cell_overlays(const coord_def &gc, crawl_view_buffer &vbuf);

#endif // TILECELL_H
