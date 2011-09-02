#ifdef USE_TILE
#ifndef TILECELL_H
#define TILECELL_H

enum halo_type
{
    HALO_NONE = 0,
    HALO_RANGE = 1,
    HALO_MONSTER = 2
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

    bool is_bloody;
    bool is_silenced;
    char halo;
    bool is_moldy;
    bool glowing_mold;
    bool is_sanctuary;
    bool is_liquefied;
    bool swamp_tree_water;
    uint8_t orb_glow;
    char blood_rotation;

    bool operator ==(const packed_cell &other) const;
    bool operator !=(const packed_cell &other) const { return !(*this == other); }

    packed_cell() : num_dngn_overlay(0), fg(0), bg(0), is_bloody(false),
                    is_silenced(false), halo(HALO_NONE), is_moldy(false),
                    glowing_mold(false), is_sanctuary(false), is_liquefied(false),
                    swamp_tree_water(false), orb_glow(0), blood_rotation(0) {}

    packed_cell(const packed_cell* c) : num_dngn_overlay(c->num_dngn_overlay),
                                        fg(c->fg), bg(c->bg), flv(c->flv),
                                        is_bloody(c->is_bloody),
                                        is_silenced(c->is_silenced),
                                        halo(c->halo),
                                        is_moldy(c->is_moldy),
                                        glowing_mold(c->glowing_mold),
                                        is_sanctuary(c->is_sanctuary),
                                        is_liquefied(c->is_liquefied),
                                        swamp_tree_water(c->swamp_tree_water),
                                        orb_glow(c->orb_glow),
                                        blood_rotation(c->blood_rotation) {}

    void clear();
};

// For a given location, pack any waves/ink/wall shadow tiles
// that require knowledge of the surrounding env cells.
void pack_cell_overlays(const coord_def &gc, packed_cell *cell);

#endif // TILECELL_H
#endif // USE_TILE
