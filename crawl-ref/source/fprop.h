#pragma once

#include "enum.h" // DEF_BITFIELD
#include "tag-version.h"

struct coord_def;

enum feature_property_type
{
    FPROP_NONE          = 0,
    FPROP_SANCTUARY_1   = (1 << 0),
    FPROP_SANCTUARY_2   = (1 << 2),
    FPROP_BLOODY        = (1 << 3),
    FPROP_UNUSED_2      = (1 << 4),  // Used to be 'vault'.
    FPROP_HIGHLIGHT     = (1 << 5),  // Highlight on the X map for debugging.
    // NOTE: Bloody floor and sanctuary are exclusive.
    FPROP_UNUSED        = (1 << 6),  // used to be force_exclude
    FPROP_NO_CLOUD_GEN  = (1 << 7),
    FPROP_NO_TELE_INTO  = (1 << 8),
    FPROP_UNUSED_3      = (1 << 9),  // used to be no_ctele_into

    // Squares that the tide should not affect.
    FPROP_NO_TIDE       = (1 << 10),
#if TAG_MAJOR_VERSION == 34
    FPROP_NO_SUBMERGE   = (1 << 11),
    FPROP_MOLD          = (1 << 12),
    FPROP_GLOW_MOLD     = (1 << 13),
#endif
    // Immune to spawning jellies and off-level eating.
    FPROP_NO_JIYVA      = (1 << 14),
    // Permanent, unlike map memory.
    FPROP_SEEN_OR_NOEXP = (1 << 15),
    FPROP_BLOOD_WEST    = (1 << 16),
    FPROP_BLOOD_NORTH   = (1 << 17),
    FPROP_BLOOD_EAST    = FPROP_BLOOD_WEST | FPROP_BLOOD_NORTH,
    FPROP_OLD_BLOOD     = (1 << 18),
    FPROP_ICY           = (1 << 19),
    FPROP_BLASPHEMY     = (1 << 20),
    FPROP_SEISMOROCK    = (1 << 21),
    // Don't reveal this tiles when fully mapping a branch on entry
    FPROP_NO_AUTOMAP    = (1 << 22),
    FPROP_GASTRONOMY    = (1 << 23),
};
DEF_BITFIELD(terrain_property_t, feature_property_type);

bool is_sanctuary(const coord_def& p);
bool is_bloodcovered(const coord_def& p);
bool is_tide_immune(const coord_def &p);
feature_property_type str_to_fprop(const string &str);
char blood_rotation(const coord_def & p);
bool is_icecovered(const coord_def& p);
bool is_blasphemy(const coord_def& p);
bool is_gastronomic(const coord_def& p);
