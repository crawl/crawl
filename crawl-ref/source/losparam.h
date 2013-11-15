/**
 * @file
 * @brief Parameters for the LOS algorithm
**/

#ifndef LOSPARAM_H
#define LOSPARAM_H

// Note: find_ray relies on the fact that 2*OPC_HALF == OPC_OPAQUE.
// On the other hand, losight tracks this explicitly.
enum opacity_type
{
    OPC_CLEAR  = 0,
    OPC_HALF   = 1,    // for opaque clouds; two or more block
    OPC_OPAQUE = 2,

    NUM_OPACITIES
};

class opacity_func
{
public:
    virtual opacity_type operator()(const coord_def& p) const = 0;
    virtual ~opacity_func() {}
    virtual opacity_func* clone() const = 0;
};

#define CLONE(typename) \
    typename* clone() const \
    { \
        return new typename(*this); \
    }

// Default LOS rules.
class opacity_default : public opacity_func
{
public:
    CLONE(opacity_default)

    opacity_type operator()(const coord_def& p) const;
};
extern const opacity_default opc_default;

// Default LOS rules, but only consider fully opaque features blocking.
// In particular, clouds don't affect the result.
class opacity_fullyopaque : public opacity_func
{
public:
    CLONE(opacity_fullyopaque)

    opacity_type operator()(const coord_def& p) const;
};
extern const opacity_fullyopaque opc_fullyopaque;

// Make transparent features block in addition to normal LOS.
// * Translocations opacity: blink, apportation, portal projectile.
// * Various "I feel safe"-related stuff.
class opacity_no_trans : public opacity_func
{
public:
    CLONE(opacity_no_trans)

    opacity_type operator()(const coord_def& p) const;
};
extern const opacity_no_trans opc_no_trans;

// Make immobile monsters block in addition to no_trans.
// This is used for monster movement.
class opacity_immob : public opacity_func
{
public:
    CLONE(opacity_immob)

    opacity_type operator()(const coord_def& p) const;
};
extern const opacity_immob opc_immob;

// Line of effect.
class opacity_solid : public opacity_func
{
public:
    CLONE(opacity_solid)

    opacity_type operator()(const coord_def& p) const;
};
extern const opacity_solid opc_solid;

// Both line of sight and line of effect.
class opacity_solid_see : public opacity_func
{
public:
    CLONE(opacity_solid_see)

    opacity_type operator()(const coord_def& p) const;
};
extern const opacity_solid_see opc_solid_see;

// Opacity for monster movement, based on the late monster_los.
class opacity_monmove : public opacity_func
{
public:
    opacity_monmove(const monster& m)
        : mon(m)
    {
    }

    CLONE(opacity_monmove)

    opacity_type operator()(const coord_def& p) const;
private:
    const monster& mon;
};

// Make any actor (as well as solid features) block.
// Note that the blocking actors are still "visible".
class opacity_no_actor : public opacity_func
{
public:
    CLONE(opacity_no_actor)

    opacity_type operator()(const coord_def& p) const;
};
extern const opacity_no_actor opc_no_actor;

// Subclasses of this are passed to losight() to modify the
// LOS calculation. Implementations will have to translate between
// relative coordinates (-8,-8)..(8,8) and real coordinates,
// specify how opaque the cells are and determine what values
// are written to the visible cells.
class los_param
{
public:
    virtual ~los_param() {}

    // Whether the translated coordinate lies within the map
    // (including boundary) and within the LOS area
    virtual bool los_bounds(const coord_def& p) const = 0;

    virtual opacity_type opacity(const coord_def& p) const = 0;
};

#endif
