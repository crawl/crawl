#ifdef USE_TILE
#ifndef TILECELL_H
#define TILECELL_H

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
    bool is_haloed;
    bool is_moldy;
    bool is_sanctuary;
    bool is_liquefied;
    bool swamp_tree_water;
    char blood_rotation;

    bool operator ==(const packed_cell &other) const;
    bool operator !=(const packed_cell &other) const { return !(*this == other); }

    packed_cell() : num_dngn_overlay(0), is_bloody(false), is_silenced(false),
                    is_haloed(false), is_moldy(false), is_sanctuary(false),
                    is_liquefied(false), swamp_tree_water (false),
                    blood_rotation(0) {}

    packed_cell(const packed_cell* c) : num_dngn_overlay(c->num_dngn_overlay),
                                        fg(c->fg), bg(c->bg), flv(c->flv),
                                        is_bloody(c->is_bloody),
                                        is_silenced(c->is_silenced),
                                        is_haloed(c->is_haloed),
                                        is_moldy(c->is_moldy),
                                        is_sanctuary(c->is_sanctuary),
                                        is_liquefied(c->is_liquefied),
                                        swamp_tree_water(c->swamp_tree_water),
                                        blood_rotation(c->blood_rotation) {}

    void clear();
};

// For a given location, pack any waves/ink/wall shadow tiles
// that require knowledge of the surrounding env cells.
void pack_cell_overlays(const coord_def &gc, packed_cell *cell);

#endif // TILECELL_H
#endif // USE_TILE
