#ifndef FPROP_H
#define FPROP_H

struct coord_def;

enum feature_property_type
{
    FPROP_NONE          = 0,
    FPROP_SANCTUARY_1   = (1 << 0),
    FPROP_SANCTUARY_2   = (1 << 2),
    FPROP_BLOODY        = (1 << 3),
    FPROP_VAULT         = (1 << 4),
    FPROP_HIGHLIGHT     = (1 << 5),  // Highlight on the X map for debugging.
    // NOTE: Bloody floor and sanctuary are exclusive.
    FPROP_UNUSED        = (1 << 6),  // used to be force_exclude
    FPROP_NO_CLOUD_GEN  = (1 << 7),
    FPROP_NO_RTELE_INTO = (1 << 8),
    FPROP_NO_CTELE_INTO = (1 << 9),
    FPROP_NO_TELE_INTO  = FPROP_NO_RTELE_INTO | FPROP_NO_CTELE_INTO,

    // Squares that the tide should not affect.
    FPROP_NO_TIDE       = (1 << 10),
    FPROP_NO_SUBMERGE   = (1 << 11),
    FPROP_MOLD          = (1 << 12),
    FPROP_GLOW_MOLD     = (1 << 13)
};


bool is_sanctuary(const coord_def& p);
bool is_bloodcovered(const coord_def& p);
bool is_tide_immune(const coord_def &p);
bool is_moldy(const coord_def & p);
bool glowing_mold(const coord_def & p);
void remove_mold(const coord_def & p);
feature_property_type str_to_fprop(const std::string &str);

#endif
