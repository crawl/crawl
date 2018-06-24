#ifdef USE_TILE
#pragma once

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

    bool is_highlighted;
    bool is_bloody;
    bool is_silenced;
    char halo;
    bool is_moldy;
    bool glowing_mold;
    bool is_sanctuary;
    bool is_liquefied;
    bool mangrove_water;
    bool awakened_forest;
    uint8_t orb_glow;
    char blood_rotation;
    bool old_blood;
    uint8_t travel_trail;
    bool quad_glow;
    uint8_t disjunct;
#if TAG_MAJOR_VERSION == 34
    uint8_t heat_aura;
#endif

    bool operator ==(const packed_cell &other) const;
    bool operator !=(const packed_cell &other) const { return !(*this == other); }

    packed_cell() : num_dngn_overlay(0), fg(0), bg(0), cloud(0), is_bloody(false),
                    is_silenced(false), halo(HALO_NONE), is_moldy(false),
                    glowing_mold(false), is_sanctuary(false), is_liquefied(false),
                    mangrove_water(false), awakened_forest(false), orb_glow(0),
                    blood_rotation(0), old_blood(false), travel_trail(0),
                    quad_glow(false), disjunct(false)
#if TAG_MAJOR_VERSION == 34
                    , heat_aura(false)
#endif
                    {}

    packed_cell(const packed_cell* c) : num_dngn_overlay(c->num_dngn_overlay),
                                        fg(c->fg), bg(c->bg), flv(c->flv),
                                        cloud(c->cloud),
                                        is_bloody(c->is_bloody),
                                        is_silenced(c->is_silenced),
                                        halo(c->halo),
                                        is_moldy(c->is_moldy),
                                        glowing_mold(c->glowing_mold),
                                        is_sanctuary(c->is_sanctuary),
                                        is_liquefied(c->is_liquefied),
                                        mangrove_water(c->mangrove_water),
                                        awakened_forest(c->awakened_forest),
                                        orb_glow(c->orb_glow),
                                        blood_rotation(c->blood_rotation),
                                        old_blood(c->old_blood),
                                        travel_trail(c->travel_trail),
                                        quad_glow(c->quad_glow),
                                        disjunct(c->disjunct)
#if TAG_MAJOR_VERSION == 34
                                        , heat_aura(c->heat_aura)
#endif
                                        {}

    void clear();
};

// For a given location, pack any waves/ink/wall shadow tiles
// that require knowledge of the surrounding env cells.
void pack_cell_overlays(const coord_def &gc, packed_cell *cell);

#endif // TILECELL_H
