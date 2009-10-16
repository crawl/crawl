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

struct opacity_func
{
    virtual ~opacity_func() {}
    virtual opacity_type operator()(const coord_def& p) const = 0;
};

struct bounds_func
{
    virtual ~bounds_func() {}
    virtual bool operator()(const coord_def& p) const = 0;
};

// Default LOS rules.
struct opacity_default : opacity_func
{
    opacity_type operator()(const coord_def& p) const;
};
static opacity_default opc_default = opacity_default();

// Make anything solid block in addition to normal LOS.
// XXX: Are trees, bushes solid?
struct opacity_solid : opacity_func
{
    opacity_type operator()(const coord_def& p) const;
};
static opacity_solid opc_solid = opacity_solid();

// LOS bounded by fixed presquared radius.
struct bounds_radius_sq : bounds_func
{
    int radius_sq;
    bounds_radius_sq(int r_sq)
        : radius_sq(r_sq) {}
    bool operator()(const coord_def& p) const;
};

// LOS bounded by current global LOS radius.
struct bounds_los_radius : bounds_func
{
    bool operator()(const coord_def& p) const;
};
static bounds_los_radius bds_default = bounds_los_radius();

// Subclasses of this are passed to losight() to modify the
// LOS calculation. Implementations will have to translate between
// relative coordinates (-8,-8)..(8,8) and real coordinates,
// specify how opaque the cells are and determine what values
// are written to the visible cells.
struct los_param
{
    virtual ~los_param() {}

    // Whether the translated coordinate lies within the map
    // (including boundary) and within the LOS area
    virtual bool los_bounds(const coord_def& p) const = 0;

    // appearance(p) will be copied to show(sh_o+p) for visible p.
    virtual unsigned appearance(const coord_def& p) const = 0;

    virtual opacity_type opacity(const coord_def& p) const = 0;
};

#endif
