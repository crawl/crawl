/*
 *  File:       losparam.h
 *  Summary:    Parameters for the LOS algorithm
 */

#ifndef LOSPARAM_H
#define LOSPARAM_H

enum opacity_type
{
    OPC_CLEAR,
    OPC_HALF,    // for opaque clouds; two or more block
    OPC_OPAQUE,
     
    NUM_OPACITIES
};

// Subclasses of this are passed to losight() to modify the
// LOS calculation. Implementations will have to translate between
// relative coordinates (-8,-8)..(8,8) and real coordinates,
// specify how opaque the cells are and determine what values
// are written to the visible cells.
struct los_param
{
    virtual ~los_param() {}

    // Whether the translated coordinate lies in the map (including boundary)
    virtual bool map_bounds(const coord_def& p) const = 0;

    // appearance(p) will be copied to show(p) for visible p.
    virtual unsigned appearance(const coord_def& p) const = 0;

    virtual opacity_type opacity(const coord_def& p) const = 0;
};

// Provides translation to given center and bounds checking.
struct los_param_trans : los_param
{
    coord_def center;

    los_param_trans(const coord_def& c);

    coord_def trans(const coord_def& p) const;

    bool map_bounds(const coord_def& p) const;
};

// Everything is visible.
struct los_param_permissive : los_param_trans
{
    los_param_permissive(const coord_def& c);

    unsigned appearance(const coord_def& p) const;
    opacity_type opacity(const coord_def& p) const;
};

// A complete base implementation that does standard visibility
// based on env.grid.
struct los_param_base : los_param_trans
{
    los_param_base(const coord_def& c);

    dungeon_feature_type feature(const coord_def& p) const;
    unsigned appearance(const coord_def& p) const;
    unsigned short cloud_idx(const coord_def& p) const;
    opacity_type opacity(const coord_def& p) const;
};

// Like los_param_base, but any solid object blocks.
// This includes clear walls and statues.
struct los_param_solid : los_param_base
{
    los_param_solid(const coord_def& c);

    opacity_type opacity(const coord_def& p) const;
};

// Provides a compatible set of parameters for use with the
// legacy losight() function.
struct los_param_compat : los_param_base
{
    feature_grid grid;
    bool solid_blocks;
    bool ignore_clouds;

    los_param_compat(const feature_grid& gr, const coord_def& c,
                     bool sb, bool ic);

    dungeon_feature_type feature(const coord_def& p) const;
    unsigned appearance(const coord_def& p) const;
    opacity_type opacity(const coord_def& p) const;
};

#endif
