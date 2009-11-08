#ifndef FPROP_H
#define FPROP_H

struct coord_def;

bool is_sanctuary( const coord_def& p );
bool is_bloodcovered( const coord_def& p );

enum feature_property_type
{
    FPROP_NONE          = 0,
    FPROP_SANCTUARY_1   = (1 << 0),
    FPROP_SANCTUARY_2   = (1 << 2),
    FPROP_BLOODY        = (1 << 3),
    FPROP_VAULT         = (1 << 4),
    FPROP_HIGHLIGHT     = (1 << 5),  // Highlight grids on the X map for debugging.
    // NOTE: Bloody floor and sanctuary are exclusive.
    FPROP_FORCE_EXCLUDE = (1 << 6),
    FPROP_NO_CLOUD_GEN  = (1 << 7),
    FPROP_NO_RTELE_INTO = (1 << 8)
};

#endif
